/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2022-2025 Zynaptic Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

/*
 * This file implements the socket specific functionality for accessing
 * a WIZnet W5500 TCP/IP offload device in TCP mode.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-scheduler.h"
#include "gmos-buffers.h"
#include "gmos-streams.h"
#include "gmos-network.h"
#include "gmos-driver-tcpip.h"
#include "gmos-tcpip-stack.h"
#include "wiznet-driver-tcpip.h"
#include "wiznet-driver-core.h"
#include "wiznet-spi-adaptor.h"

/*
 * Issue a TCP socket status notification callback.
 */
static inline void gmosNalTcpipSocketSendNotification (
    gmosNalTcpipSocket_t* socket, gmosTcpipStackNotify_t notification)
{
    if (socket->common.notifyHandler != NULL) {
        socket->common.notifyHandler (
            socket->common.notifyData, notification);
    }
}

/*
 * From the TCP ready state, check for socket close requests.
 */
static inline gmosTaskStatus_t gmosNalTcpipSocketProcessTcpReady (
    gmosNalTcpipSocket_t* socket, uint8_t* nextState)
{
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    uint8_t intFlags = socket->interruptFlags;

    // Check for the socket close request flag.
    if ((intFlags & WIZNET_SPI_ADAPTOR_SOCKET_FLAG_CLOSE_REQ) != 0) {
        *nextState = WIZNET_SOCKET_TCP_STATE_CLOSE;
    }

    // Socket processing can be suspended if no TCP transfer is ready.
    else {
        taskStatus = GMOS_TASK_SUSPEND;
    }
    return taskStatus;
}

/*
 * From the TCP active state, initiate either an interrupt driven data
 * receive operation or a queued data transmit operation.
 */
static inline gmosTaskStatus_t gmosNalTcpipSocketProcessTcpActive (
    gmosNalTcpipSocket_t* socket, uint8_t* nextState)
{
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    gmosNalTcpipState_t* nalData = socket->common.tcpipDriver->nalData;
    wiznetSpiAdaptorCmd_t bufStatusCommand;
    gmosStream_t* rxStream = &(socket->common.rxStream);
    gmosStream_t* txStream = &(socket->common.txStream);
    uint8_t socketId = socket->socketId;
    uint8_t intFlags = socket->interruptFlags;

    // Check for the socket close request flag or the remote disconnect
    // request interrupt. On a remote disconnection make sure that any
    // pending data is processed first.
    if (((intFlags & WIZNET_SPI_ADAPTOR_SOCKET_INT_DISCON) != 0) &&
        ((intFlags & WIZNET_SPI_ADAPTOR_SOCKET_INT_RECV) == 0)) {
        *nextState = WIZNET_SOCKET_TCP_STATE_CLOSE;
    } else if (
        (intFlags & WIZNET_SPI_ADAPTOR_SOCKET_FLAG_CLOSE_REQ) != 0) {
        *nextState = WIZNET_SOCKET_TCP_STATE_DISCONNECT;
    }

    // Check for TCP receive notifications, which are indicated by the
    // socket interrupt flags. If an inbound transfer can be queued,
    // the WIZnet receive buffer information will be requested from
    // address 0x0026.
    else if ((gmosStreamGetWriteCapacity (rxStream) > 0) &&
        ((intFlags & WIZNET_SPI_ADAPTOR_SOCKET_INT_RECV) != 0)) {
        bufStatusCommand.address = 0x0026;
        bufStatusCommand.control =
            WIZNET_SPI_ADAPTOR_CTRL_SOCKET_REGS (socketId) |
            WIZNET_SPI_ADAPTOR_CTRL_READ_ENABLE;
        bufStatusCommand.size = 6;

        // Attempt to send the buffer status read request.
        if (wiznetSpiAdaptorStream_write (
            &nalData->spiCommandStream, &bufStatusCommand)) {
            *nextState = WIZNET_SOCKET_TCP_STATE_RX_BUFFER_CHECK;
            taskStatus = GMOS_TASK_SUSPEND;
        }
    }

    // Check for outbound TCP transfers. If an outbound transfer is
    // queued or there may be residual data in the TCP transmit buffer,
    // the WIZnet transmit buffer information will be requested from
    // address 0x0020.
    else if ((socket->socketState == WIZNET_SOCKET_TCP_STATE_ACTIVE) ||
        (gmosStreamGetReadCapacity (txStream) > 0)) {
        bufStatusCommand.address = 0x0020;
        bufStatusCommand.control =
            WIZNET_SPI_ADAPTOR_CTRL_SOCKET_REGS (socketId) |
            WIZNET_SPI_ADAPTOR_CTRL_READ_ENABLE;
        bufStatusCommand.size = 6;

        // Attempt to send the buffer status read request.
        if (wiznetSpiAdaptorStream_write (
            &nalData->spiCommandStream, &bufStatusCommand)) {
            *nextState = WIZNET_SOCKET_TCP_STATE_TX_BUFFER_CHECK;
            taskStatus = GMOS_TASK_SUSPEND;
        }
    }

    // Socket processing can be suspended if no TCP transfer is ready.
    else {
        taskStatus = GMOS_TASK_SUSPEND;
    }
    return taskStatus;
}

/*
 * Checks the interrupt status flags on completion of a TCP connection
 * request.
 */
static inline gmosTaskStatus_t gmosNalTcpipSocketTcpConnectInterruptCheck (
    gmosNalTcpipSocket_t* socket, uint8_t* nextState)
{
    uint8_t intFlags = socket->interruptFlags;
    bool interruptHandled = false;

    // If an ARP or TCP handshake timeout occurred, the socket reverts
    // to its unconnected state.
    if ((intFlags & WIZNET_SPI_ADAPTOR_SOCKET_INT_TIMEOUT) != 0) {
        gmosNalTcpipSocketSendNotification (socket,
            GMOS_TCPIP_STACK_NOTIFY_TCP_CONNECT_TIMEOUT);
        *nextState = WIZNET_SOCKET_TCP_STATE_READY;
        interruptHandled = true;
    }

    // Check for the socket close request flag which indicates that the
    // connection was closed by the remote end.
    else if ((intFlags & WIZNET_SPI_ADAPTOR_SOCKET_INT_DISCON) != 0) {
        *nextState = WIZNET_SOCKET_TCP_STATE_CLOSE;
        GMOS_LOG_FMT (LOG_DEBUG,
            "WIZnet TCP/IP : Socket %d TCP closed during connection.",
        socket->socketId);
        interruptHandled = true;
    }

    // After completing the TCP handshake, start polling for transmit
    // or receive data.
    else if ((intFlags & WIZNET_SPI_ADAPTOR_SOCKET_INT_CON) != 0) {
        gmosNalTcpipSocketSendNotification (socket,
            GMOS_TCPIP_STACK_NOTIFY_TCP_SOCKET_CONNECTED);
        *nextState = WIZNET_SOCKET_TCP_STATE_ACTIVE;
        GMOS_LOG_FMT (LOG_DEBUG,
            "WIZnet TCP/IP : Socket %d TCP connection established.",
        socket->socketId);
        interruptHandled = true;
    }

    // Clear all handled interrupt conditions after processing.
    if (interruptHandled) {
        socket->interruptClear |=
            WIZNET_SPI_ADAPTOR_SOCKET_INT_TIMEOUT |
            WIZNET_SPI_ADAPTOR_SOCKET_INT_CON |
            WIZNET_SPI_ADAPTOR_SOCKET_INT_DISCON;
        return GMOS_TASK_RUN_IMMEDIATE;
    } else {
        return GMOS_TASK_SUSPEND;
    }
}

/*
 * Initiate a read data transfer to copy the TCP payload to a local
 * buffer.
 */
static inline bool gmosNalTcpipSocketTcpRxDataBufRead (
    gmosNalTcpipSocket_t* socket)
{
    gmosNalTcpipState_t* nalData = socket->common.tcpipDriver->nalData;
    wiznetSpiAdaptorCmd_t readDataCommand;
    gmosBuffer_t* readDataBuffer = &(readDataCommand.data.buffer);
    uint8_t socketId = socket->socketId;
    uint16_t bufferSize;
    uint16_t maxTransferSize;

    // Determine the amount of data storage for the read data buffer.
    bufferSize = socket->data.active.limitPtr - socket->data.active.dataPtr;

    // When using a fixed memory pool, leave at leat 4 memory pool
    // segments available for other processing. Wait for memory pool
    // capacity to be released if this is not possible.
    if (!GMOS_CONFIG_MEMPOOL_USE_HEAP) {
        maxTransferSize = gmosMempoolSegmentsAvailable () - 4;
        maxTransferSize *= GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE;
        if (maxTransferSize < bufferSize) {
            return false;
        }
    }

    // Allocate sufficient buffer memory to receive all the data from
    // the WIZnet buffer.
    gmosBufferInit (readDataBuffer);
    if (!gmosBufferResize (readDataBuffer, bufferSize)) {
        return false;
    }

    // Set up the command to read the TCP data from the WIZnet buffer.
    readDataCommand.address = socket->data.active.dataPtr;
    readDataCommand.control =
        WIZNET_SPI_ADAPTOR_CTRL_SOCKET_RX_BUF (socketId) |
        WIZNET_SPI_ADAPTOR_CTRL_READ_ENABLE;
    readDataCommand.size = 0;

    // Issue the TCP data read data request. Revert the buffer
    // allocation on failure.
    if (wiznetSpiAdaptorStream_write (
        &nalData->spiCommandStream, &readDataCommand)) {
        return true;
    } else {
        gmosBufferReset (readDataBuffer, 0);
        return false;
    }
}

/*
 * Checks the status of the TCP transmit buffer. The transmit state
 * machine will only proceed if the buffer status fields are consistent.
 */
static inline void gmosNalTcpipSocketTcpTxBufferCheck (
    gmosNalTcpipSocket_t* socket, wiznetSpiAdaptorCmd_t* response,
    uint8_t* nextState)
{
    gmosStream_t* txStream = &(socket->common.txStream);
    uint8_t socketId = socket->socketId;
    uint8_t expectedControl =
        WIZNET_SPI_ADAPTOR_CTRL_SOCKET_REGS (socketId) |
        WIZNET_SPI_ADAPTOR_CTRL_READ_ENABLE;
    uint16_t bufSize;
    uint16_t bufTxFree;
    uint16_t bufReadPtr;
    uint16_t bufWritePtr;

    // A response sequence error is generated if this is not a valid
    // response message.
    if ((response->address != 0x0020) ||
        (response->control != expectedControl) ||
        (response->size != 6)) {
        *nextState = WIZNET_SOCKET_TCP_STATE_ERROR;
        return;
    }

    // Extract the transmit buffer pointer state.
    bufTxFree = ((uint16_t) response->data.bytes [1]) +
        (((uint16_t) response->data.bytes [0]) << 8);
    bufReadPtr = ((uint16_t) response->data.bytes [3]) +
        (((uint16_t) response->data.bytes [2]) << 8);
    bufWritePtr = ((uint16_t) response->data.bytes [5]) +
        (((uint16_t) response->data.bytes [4]) << 8);

    // Check for transmit buffer consistency. Attempt to re-read the
    // register values if not consistent.
    bufSize = gmosNalTcpipSocketGetBufferSize (socket);
    if ((bufWritePtr - bufReadPtr != bufSize - bufTxFree)) {
        *nextState = WIZNET_SOCKET_TCP_STATE_ACTIVE;
    }

    // Check for the condition where there is no queued data, and then
    // either flush residual data from the WIZnet socket buffer or
    // suspend further processing.
    else if (gmosStreamGetReadCapacity (txStream) == 0) {
        if (bufWritePtr == bufReadPtr) {
            *nextState = WIZNET_SOCKET_TCP_STATE_SLEEPING;
        } else {
            *nextState = WIZNET_SOCKET_TCP_STATE_TX_DATA_SEND;
        }
    }

    // Set up the data transfer pointer for the buffer write. The start
    // of data pointer is the hardware buffer write pointer and the end
    // of data pointer is the location immediately after the last free
    // entry in the hardware buffer.
    else {
        socket->data.active.dataPtr = bufWritePtr;
        socket->data.active.limitPtr = bufWritePtr + bufTxFree;
        *nextState = WIZNET_SOCKET_TCP_STATE_TX_PAYLOAD_APPEND;
    }
}

/*
 * Determine whether a new payload data buffer can be appended to the
 * TCP hardware transmit buffer.
 */
static inline bool gmosNalTcpipSocketTcpTxDataAppend (
    gmosNalTcpipSocket_t* socket)
{
    gmosStream_t* txStream = &(socket->common.txStream);

    // Calculate the remaining free space available in the hardware
    // buffer.
    uint16_t  bufTxFree =
        socket->data.active.limitPtr - socket->data.active.dataPtr;

    // Transfer the next TCP data buffer to a local buffer for further
    // processing.
    if (!gmosStreamAcceptBuffer (txStream, &(socket->payloadData))) {
        return false;
    }

    // Check that there is sufficient free space to copy the buffer
    // contents to the WIZnet socket buffer. If not, flush residual
    // data from the WIZnet socket buffer.
    else if (gmosBufferGetSize (&(socket->payloadData)) > bufTxFree) {
        gmosStreamPushBackBuffer (txStream, &(socket->payloadData));
        return false;
    }

    // Indicate that the local buffer data may be appended to the
    // hardware buffer.
    return true;
}

/*
 * Checks the interrupt status flags on completion of a TCP transmit
 * operation.
 */
static inline gmosTaskStatus_t gmosNalTcpipSocketTcpTxInterruptCheck (
    gmosNalTcpipSocket_t* socket, uint8_t* nextState)
{
    uint8_t intFlags = socket->interruptFlags;
    bool interruptHandled = false;

    // If an ARP or TCP timeout occurred, the outgoing data remains in
    // the socket transmit buffer.
    // TODO: Notify timeout condition to next higher layer.
    if ((intFlags & WIZNET_SPI_ADAPTOR_SOCKET_INT_TIMEOUT) != 0) {
        *nextState = WIZNET_SOCKET_TCP_STATE_ACTIVE;
        interruptHandled = true;
    }

    // After transmitting a TCP packet, start polling for new TCP
    // transmit or receive data.
    else if ((intFlags & WIZNET_SPI_ADAPTOR_SOCKET_INT_SENDOK) != 0) {
        *nextState = WIZNET_SOCKET_TCP_STATE_ACTIVE;
        interruptHandled = true;
    }

    // Clear both interrupt conditions after processing.
    if (interruptHandled) {
        socket->interruptClear |=
            WIZNET_SPI_ADAPTOR_SOCKET_INT_TIMEOUT |
            WIZNET_SPI_ADAPTOR_SOCKET_INT_SENDOK;
        return GMOS_TASK_RUN_IMMEDIATE;
    } else {
        return GMOS_TASK_SUSPEND;
    }
}

/*
 * Initiates the TCP connection process as a TCP client, using the
 * specified server address and port.
 */
gmosNetworkStatus_t gmosDriverTcpipTcpConnect (
    gmosNalTcpipSocket_t* tcpSocket,
    uint8_t* serverAddr, uint16_t serverPort)
{
    gmosNalTcpipState_t* nalData = tcpSocket->common.tcpipDriver->nalData;
    uint8_t nextPhase = tcpSocket->socketState & WIZNET_SOCKET_PHASE_MASK;
    uint8_t nextState = tcpSocket->socketState & ~WIZNET_SOCKET_PHASE_MASK;
    gmosBuffer_t* remoteAddrBuf = &(tcpSocket->payloadData);
    uint8_t remotePortBytes [2];

    // Check that the specified socket has been opened for TCP data
    // transfer.
    if (nextPhase != WIZNET_SOCKET_PHASE_TCP) {
        return GMOS_NETWORK_STATUS_NOT_OPEN;
    }

    // Check that the specified socket is in a valid state for the
    // connection request.
    if (nextState != WIZNET_SOCKET_TCP_STATE_READY) {
        return GMOS_NETWORK_STATUS_NOT_VALID;
    }

    // Allocate a temporary buffer for storing the server address.
    if (!gmosBufferReset (remoteAddrBuf, 6)) {
        return GMOS_NETWORK_STATUS_RETRY;
    }

    // Log the connection request for debug purposes.
    GMOS_LOG_FMT (LOG_DEBUG,
        "WIZnet TCP/IP : Socket %d TCP connection request to %d.%d.%d.%d:%d.",
        tcpSocket->socketId, serverAddr [0], serverAddr [1],
        serverAddr [2], serverAddr [3], serverPort);

    // Store the address and port in network byte order so that it can
    // be loaded directly into the WIZnet device.
    remotePortBytes [0] = (uint8_t) (serverPort >> 8);
    remotePortBytes [1] = (uint8_t) serverPort;
    gmosBufferWrite (remoteAddrBuf, 0, serverAddr, 4);
    gmosBufferWrite (remoteAddrBuf, 4, remotePortBytes, 2);

    // Initiate the TCP port connection request.
    nextState = WIZNET_SOCKET_TCP_STATE_SET_REMOTE_ADDR;
    tcpSocket->socketState = nextPhase | nextState;
    gmosSchedulerTaskResume (&(nalData->coreWorkerTask));
    return GMOS_NETWORK_STATUS_SUCCESS;
}

/*
 * Sends the contents of a GubbinsMOS buffer over an established TCP
 * connection.
 */
gmosNetworkStatus_t gmosDriverTcpipTcpSend (
    gmosNalTcpipSocket_t* tcpSocket, gmosBuffer_t* payload)
{
    gmosStream_t* txStream = &(tcpSocket->common.txStream);
    uint8_t socketPhase = tcpSocket->socketState & WIZNET_SOCKET_PHASE_MASK;
    uint8_t socketState = tcpSocket->socketState & ~WIZNET_SOCKET_PHASE_MASK;
    uint16_t payloadLength = gmosBufferGetSize (payload);

    // Check that the specified socket has been opened for TCP data
    // transfer.
    if (socketPhase != WIZNET_SOCKET_PHASE_TCP) {
        return GMOS_NETWORK_STATUS_NOT_OPEN;
    }

    // Check that a TCP connection has been established.
    if (socketState < WIZNET_SOCKET_TCP_STATE_ACTIVE) {
        return GMOS_NETWORK_STATUS_NOT_CONNECTED;
    }

    // Check that the payload length does not exceed the available
    // buffer memory on the WIZnet device.
    if (payloadLength > gmosNalTcpipSocketGetBufferSize (tcpSocket)) {
        return GMOS_NETWORK_STATUS_OVERSIZED;
    }

    // Queue the buffer for transmission.
    if (gmosStreamSendBuffer (txStream, payload)) {
        return GMOS_NETWORK_STATUS_SUCCESS;
    } else {
        return GMOS_NETWORK_STATUS_RETRY;
    }
}

/*
 * Receives a block of data over an established TCP connection.
 */
gmosNetworkStatus_t gmosDriverTcpipTcpReceive (
    gmosNalTcpipSocket_t* tcpSocket, gmosBuffer_t* payload)
{
    gmosStream_t* rxStream = &(tcpSocket->common.rxStream);
    uint8_t socketPhase = tcpSocket->socketState & WIZNET_SOCKET_PHASE_MASK;
    uint8_t socketState = tcpSocket->socketState & ~WIZNET_SOCKET_PHASE_MASK;

    // Check that the specified socket has been opened for TCP data
    // transfer.
    if (socketPhase != WIZNET_SOCKET_PHASE_TCP) {
        return GMOS_NETWORK_STATUS_NOT_OPEN;
    }

    // Check that a TCP connection has been established.
    if (socketState < WIZNET_SOCKET_TCP_STATE_ACTIVE) {
        return GMOS_NETWORK_STATUS_NOT_CONNECTED;
    }

    // Receive the next payload buffer, if available.
    if (gmosStreamAcceptBuffer (rxStream, payload)) {
        return GMOS_NETWORK_STATUS_SUCCESS;
    } else {
        return GMOS_NETWORK_STATUS_RETRY;
    }
}

/*
 * Closes the specified TCP socket, terminating any active connection
 * and releasing all allocated resources.
 */
gmosNetworkStatus_t gmosDriverTcpipTcpClose (
    gmosNalTcpipSocket_t* tcpSocket)
{
    gmosNalTcpipState_t* nalData = tcpSocket->common.tcpipDriver->nalData;
    uint8_t socketPhase = tcpSocket->socketState & WIZNET_SOCKET_PHASE_MASK;

    // Check that the specified socket has been opened for TCP data
    // transfer.
    if (socketPhase != WIZNET_SOCKET_PHASE_TCP) {
        return GMOS_NETWORK_STATUS_NOT_OPEN;
    }

    // Set the close request flag to initiate a clean shutdown.
    tcpSocket->interruptFlags |= WIZNET_SPI_ADAPTOR_SOCKET_FLAG_CLOSE_REQ;
    gmosSchedulerTaskResume (&(nalData->coreWorkerTask));
    return GMOS_NETWORK_STATUS_SUCCESS;
}

/*
 * Implements a socket processing tick cycle when in the TCP open phase.
 */
gmosTaskStatus_t gmosNalTcpipSocketProcessTickTcp (
    gmosNalTcpipSocket_t* socket)
{
    uint8_t nextState = socket->socketState & ~WIZNET_SOCKET_PHASE_MASK;
    uint8_t nextPhase = WIZNET_SOCKET_PHASE_TCP;
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    uint8_t closeCommand;
    bool isConnected = true;

    // Implement the TCP socket processing state machine.
    switch (nextState) {

        // Issue notification callback on opening the socket.
        case WIZNET_SOCKET_TCP_STATE_OPEN :
            gmosNalTcpipSocketSendNotification (socket,
                GMOS_TCPIP_STACK_NOTIFY_TCP_SOCKET_OPENED);
            nextState = WIZNET_SOCKET_TCP_STATE_READY;
            break;

        // Wait for a newly opened TCP socket to receive a connect or
        // bind request.
        case WIZNET_SOCKET_TCP_STATE_READY :
            taskStatus = gmosNalTcpipSocketProcessTcpReady (
                socket, &nextState);
            break;

        // Issue the appropriate TCP socket close request and start the
        // common socket cleanup process.
        case WIZNET_SOCKET_TCP_STATE_CLOSE :
            isConnected = false;             // Intentional fallthrough.
        case WIZNET_SOCKET_TCP_STATE_DISCONNECT :
            closeCommand = isConnected ?
                WIZNET_SPI_ADAPTOR_SOCKET_COMMAND_DISCONNECT :
                WIZNET_SPI_ADAPTOR_SOCKET_COMMAND_CLOSE;
            if (gmosNalTcpipSocketIssueCommand (socket, closeCommand)) {
                gmosNalTcpipSocketSendNotification (socket,
                    GMOS_TCPIP_STACK_NOTIFY_TCP_SOCKET_CLOSED);
                nextPhase = WIZNET_SOCKET_PHASE_CLOSED;
                nextState = WIZNET_SOCKET_STATE_CLOSING_STATUS_READ;
            }
            break;

        // Set the remote address for a new TCP client connection.
        case WIZNET_SOCKET_TCP_STATE_SET_REMOTE_ADDR :
            if (gmosNalTcpipSocketSetRemoteAddr (socket)) {
                nextState = WIZNET_SOCKET_TCP_STATE_CONNECT_REQUEST;
            }
            break;

        // Send the connection request command to initiate the TCP
        // client handshake.
        case WIZNET_SOCKET_TCP_STATE_CONNECT_REQUEST :
            if (gmosNalTcpipSocketIssueCommand (socket,
                WIZNET_SPI_ADAPTOR_SOCKET_COMMAND_CONNECT)) {
                nextState = WIZNET_SOCKET_TCP_STATE_CONNECT_WAIT;
                taskStatus = GMOS_TASK_SUSPEND;
            }
            break;

        // Wait for connection to complete via interrupt callback.
        case WIZNET_SOCKET_TCP_STATE_CONNECT_WAIT :
            taskStatus = gmosNalTcpipSocketTcpConnectInterruptCheck (
                socket, &nextState);
            break;

        // Process transmit and receive data for the active connection.
        case WIZNET_SOCKET_TCP_STATE_ACTIVE :
        case WIZNET_SOCKET_TCP_STATE_SLEEPING :
            taskStatus = gmosNalTcpipSocketProcessTcpActive (
                socket, &nextState);
            break;

        // Wait for the TCP receive buffer check to complete via the
        // socket processing response callback.
        case WIZNET_SOCKET_TCP_STATE_RX_BUFFER_CHECK :
            taskStatus = GMOS_TASK_SUSPEND;
            break;

        // Request the TCP packet data from the WIZnet socket buffer.
        case WIZNET_SOCKET_TCP_STATE_RX_DATA_BLOCK_READ :
            if (gmosNalTcpipSocketTcpRxDataBufRead (socket)) {
                nextState = WIZNET_SOCKET_TCP_STATE_RX_DATA_BLOCK_CHECK;
            }
            break;

        // Wait for the TCP buffer data transfer to complete via the
        // socket processing response callback.
        case WIZNET_SOCKET_TCP_STATE_RX_DATA_BLOCK_CHECK :
            taskStatus = GMOS_TASK_SUSPEND;
            break;

        // Write the updated read data pointer.
        case WIZNET_SOCKET_TCP_STATE_RX_POINTER_WRITE :
            if (gmosNalTcpipSocketRxPointerWrite (socket)) {
                nextState = WIZNET_SOCKET_TCP_STATE_RX_READ_CONFIRM;
            }
            break;

        // Confirm completion of the transaction by issuing the
        // received data command.
        case WIZNET_SOCKET_TCP_STATE_RX_READ_CONFIRM :
            if (gmosNalTcpipSocketIssueCommand (socket,
                WIZNET_SPI_ADAPTOR_SOCKET_COMMAND_RECV)) {
                nextState = WIZNET_SOCKET_TCP_STATE_RX_DATA_BLOCK_QUEUE;
            }
            break;

        // Add the received TCP data block to the socket received data
        // queue.
        case WIZNET_SOCKET_TCP_STATE_RX_DATA_BLOCK_QUEUE :
            if (gmosStreamSendBuffer (
                &(socket->common.rxStream), &(socket->payloadData))) {
                nextState = WIZNET_SOCKET_TCP_STATE_ACTIVE;
            }
            break;

        // Wait for the TCP transmit buffer check to complete via the
        // socket processing response callback.
        case WIZNET_SOCKET_TCP_STATE_TX_BUFFER_CHECK :
            taskStatus = GMOS_TASK_SUSPEND;
            break;

        // Attempt to append queued payload buffers to the hardware
        // buffer.
        case WIZNET_SOCKET_TCP_STATE_TX_PAYLOAD_APPEND :
            if (gmosNalTcpipSocketTcpTxDataAppend (socket)) {
                nextState = WIZNET_SOCKET_TCP_STATE_TX_PAYLOAD_WRITE;
            } else {
                nextState = WIZNET_SOCKET_TCP_STATE_TX_POINTER_WRITE;
            }
            break;

        // Copy transmit data to the socket data buffer.
        case WIZNET_SOCKET_TCP_STATE_TX_PAYLOAD_WRITE :
            if (gmosNalTcpipSocketTxDataWrite (socket)) {
                nextState = WIZNET_SOCKET_TCP_STATE_TX_PAYLOAD_APPEND;
            }
            break;

        // Update the transmit data pointer to the end of the valid
        // transmit data. This then returns to the active state in order
        // to copy any other queued data blocks before initiating a
        // TCP data send. This approach gives the most efficient TCP
        // packetisation when transmitting the data.
        case WIZNET_SOCKET_TCP_STATE_TX_POINTER_WRITE :
            if (gmosNalTcpipSocketTxPointerWrite (socket)) {
                nextState = WIZNET_SOCKET_TCP_STATE_TX_DATA_SEND;
            }
            break;

        // Send the TCP transmit data and wait for data sent
        // notification.
        case WIZNET_SOCKET_TCP_STATE_TX_DATA_SEND :
            if (gmosNalTcpipSocketIssueCommand (socket,
                WIZNET_SPI_ADAPTOR_SOCKET_COMMAND_SEND)) {
                nextState = WIZNET_SOCKET_TCP_STATE_TX_INTERRUPT_CHECK;
            }
            break;

        // Wait for TCP transmit to complete via interrupt callback.
        // This ensures that the WIZnet TCP state machine is in a
        // consistent state before issuing any further commands.
        case WIZNET_SOCKET_TCP_STATE_TX_INTERRUPT_CHECK :
            taskStatus = gmosNalTcpipSocketTcpTxInterruptCheck (
                socket, &nextState);
            break;
    }

    // Update the socket state and the task scheduling status.
    socket->socketState = nextPhase | nextState;
    return taskStatus;
}

/*
 * Implements a socket processing response callback when in the TCP open
 * phase.
 */
void gmosNalTcpipSocketProcessResponseTcp (
    gmosNalTcpipSocket_t* socket, wiznetSpiAdaptorCmd_t* response)
{
    gmosNalTcpipState_t* nalData = socket->common.tcpipDriver->nalData;
    bool sequenceError;
    uint8_t nextState = socket->socketState & ~WIZNET_SOCKET_PHASE_MASK;
    uint8_t nextPhase = WIZNET_SOCKET_PHASE_TCP;
    bool resumeProcessing = false;

    // Process SPI response messages according to the current state.
    switch (nextState) {

        // Implement TCP receive buffer status check. At least 1 byte
        // must be available for processing. On success, prepare to read
        // the received TCP data.
        case WIZNET_SOCKET_TCP_STATE_RX_BUFFER_CHECK :
            if (gmosNalTcpipSocketRxBufferCheck (
                socket, response, 1, &sequenceError)) {
                nextState = WIZNET_SOCKET_TCP_STATE_RX_DATA_BLOCK_READ;
            } else if (!sequenceError) {
                nextState = WIZNET_SOCKET_TCP_STATE_ACTIVE;
            } else {
                nextState = WIZNET_SOCKET_TCP_STATE_ERROR;
            }
            resumeProcessing = true;
            break;

        // Implement TCP data block read check. On success, prepare to
        // update the buffer read pointer.
        case WIZNET_SOCKET_TCP_STATE_RX_DATA_BLOCK_CHECK :
            if (gmosNalTcpipSocketRxDataBlockCheck (
                socket, response, &sequenceError)) {
                nextState = WIZNET_SOCKET_TCP_STATE_RX_POINTER_WRITE;
            } else if (!sequenceError) {
                nextState = WIZNET_SOCKET_TCP_STATE_ACTIVE;
            } else {
                nextState = WIZNET_SOCKET_TCP_STATE_ERROR;
            }
            resumeProcessing = true;
            break;

        // Implement TCP transmit buffer status check. On success, copy
        // the TCP payload to the local processing buffer and prepare
        // the WIZnet socket for data transfer.
        case WIZNET_SOCKET_TCP_STATE_TX_BUFFER_CHECK :
            gmosNalTcpipSocketTcpTxBufferCheck (
                socket, response, &nextState);
            resumeProcessing = true;
            break;
    }

    // Update the socket state and resume the worker task on a change.
    socket->socketState = nextPhase | nextState;
    if (resumeProcessing) {
        gmosSchedulerTaskResume (&(nalData->coreWorkerTask));
    }
}
