/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2022 Zynaptic Limited
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
 * a WIZnet W5500 TCP/IP offload device in UDP mode.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-scheduler.h"
#include "gmos-streams.h"
#include "gmos-network.h"
#include "gmos-driver-tcpip.h"
#include "gmos-tcpip-stack.h"
#include "wiznet-driver-tcpip.h"
#include "wiznet-driver-core.h"
#include "wiznet-spi-adaptor.h"

/*
 * From the UDP ready state, initiate either an interrupt driven packet
 * receive operation or a queued packet transmit operation.
 */
static inline gmosTaskStatus_t gmosNalTcpipSocketProcessUdp (
    gmosNalTcpipSocket_t* socket, uint8_t* nextState)
{
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    gmosNalTcpipState_t* nalData = socket->common.tcpipDriver->nalData;
    wiznetSpiAdaptorCmd_t bufStatusCommand;
    gmosStream_t* rxStream = &(socket->common.rxStream);
    gmosStream_t* txStream = &(socket->common.txStream);
    uint8_t socketId = socket->socketId;
    uint8_t intFlags = socket->interruptFlags;

    // Check for the socket close request flag.
    if ((intFlags & WIZNET_SPI_ADAPTOR_SOCKET_FLAG_CLOSE_REQ) != 0) {
        *nextState = WIZNET_SOCKET_UDP_STATE_CLOSE;
    }

    // Check for UDP receive notifications, which are indicated by the
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
            *nextState = WIZNET_SOCKET_UDP_STATE_RX_BUFFER_CHECK;
            taskStatus = GMOS_TASK_SUSPEND;
        }
    }

    // Check for outbound UDP transfers. If an outbound transfer is
    // queued, the two octet WIZnet transmit buffer read pointer will
    // be requested from address 0x0022.
    else if (gmosStreamGetReadCapacity (txStream) > 0) {
        bufStatusCommand.address = 0x0022;
        bufStatusCommand.control =
            WIZNET_SPI_ADAPTOR_CTRL_SOCKET_REGS (socketId) |
            WIZNET_SPI_ADAPTOR_CTRL_READ_ENABLE;
        bufStatusCommand.size = 2;

        // Attempt to send the buffer status read request.
        if (wiznetSpiAdaptorStream_write (
            &nalData->spiCommandStream, &bufStatusCommand)) {
            *nextState = WIZNET_SOCKET_UDP_STATE_TX_BUFFER_CHECK;
            taskStatus = GMOS_TASK_SUSPEND;
        }
    }

    // Socket processing can be suspended if no UDP transfer is ready.
    else {
        taskStatus = GMOS_TASK_SUSPEND;
    }
    return taskStatus;
}

/*
 * Requests the size field from the header of the next UDP packet in
 * the receive buffer.
 */
static inline bool gmosNalTcpipSocketUdpRxDataSizeRead (
    gmosNalTcpipSocket_t* socket)
{
    gmosNalTcpipState_t* nalData = socket->common.tcpipDriver->nalData;
    wiznetSpiAdaptorCmd_t getSizeCommand;
    uint8_t socketId = socket->socketId;

    // Set up the command to read from the socket receive buffer at the
    // read pointer offset. The packet size field is located in bytes
    // 6 and 7 of the header.
    getSizeCommand.address = socket->data.active.dataPtr + 6;
    getSizeCommand.control =
        WIZNET_SPI_ADAPTOR_CTRL_SOCKET_RX_BUF (socketId) |
        WIZNET_SPI_ADAPTOR_CTRL_READ_ENABLE;
    getSizeCommand.size = 2;

    // Issue the UDP packet size read request.
    return wiznetSpiAdaptorStream_write (
        &nalData->spiCommandStream, &getSizeCommand);
}

/*
 * Checks the size of the next UDP packet. The receive state machine
 * will only proceed if there is sufficient data in the buffer to
 * support the full packet transfer.
 */
static inline bool gmosNalTcpipSocketUdpRxDataSizeCheck (
    gmosNalTcpipSocket_t* socket, wiznetSpiAdaptorCmd_t* response,
    bool* sequenceError)
{
    uint8_t socketId = socket->socketId;
    uint8_t expectedControl =
        WIZNET_SPI_ADAPTOR_CTRL_SOCKET_RX_BUF (socketId) |
        WIZNET_SPI_ADAPTOR_CTRL_READ_ENABLE;
    uint16_t bufRxSize;
    uint16_t dataRxSize;

    // A response sequence error is generated if this is not a valid
    // response message.
    if ((response->address != socket->data.active.dataPtr + 6) ||
        (response->control != expectedControl) ||
        (response->size != 2)) {
        *sequenceError = true;
        return false;
    }

    // Extract the receive data size.
    *sequenceError = false;
    bufRxSize = socket->data.active.limitPtr - socket->data.active.dataPtr;
    dataRxSize = ((uint16_t) response->data.bytes [1]) +
        (((uint16_t) response->data.bytes [0]) << 8);
    GMOS_LOG_FMT (LOG_VERBOSE,
        "WIZnet TCP/IP : Socket %d UDP receive message size %d/%d octets.",
        socketId, dataRxSize, bufRxSize);

    // Modify the end of data pointer so that it references the end of
    // the UDP packet, rather than the end of the received data block.
    if (dataRxSize <= bufRxSize - 8) {
        socket->data.active.limitPtr =
            socket->data.active.dataPtr + dataRxSize + 8;
        return true;
    } else {
        return false;
    }
}

/*
 * Initiate a read data transfer to copy the UDP header and payload
 * to a local buffer.
 */
static inline bool gmosNalTcpipSocketUdpRxDataBufRead (
    gmosNalTcpipSocket_t* socket)
{
    gmosNalTcpipState_t* nalData = socket->common.tcpipDriver->nalData;
    wiznetSpiAdaptorCmd_t readDataCommand;
    gmosBuffer_t* readDataBuffer = &(readDataCommand.data.buffer);
    uint8_t socketId = socket->socketId;
    uint16_t bufferSize;

    // Attempt to allocate data storage for the read data buffer.
    bufferSize = socket->data.active.limitPtr - socket->data.active.dataPtr;
    gmosBufferInit (readDataBuffer);
    if (!gmosBufferResize (readDataBuffer, bufferSize)) {
        return false;
    }

    // Set up the command to read the UDP data from the WIZnet buffer.
    readDataCommand.address = socket->data.active.dataPtr;
    readDataCommand.control =
        WIZNET_SPI_ADAPTOR_CTRL_SOCKET_RX_BUF (socketId) |
        WIZNET_SPI_ADAPTOR_CTRL_READ_ENABLE;
    readDataCommand.size = 0;

    // Issue the UDP packet read data request. Revert the buffer
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
 * Checks the status of the UDP transmit buffer. Only one UDP packet
 * can be 'in flight' at any given time, so the read pointer is used as
 * starting point for data transfer.
 */
static inline bool gmosNalTcpipSocketUdpTxBufferCheck (
    gmosNalTcpipSocket_t* socket, wiznetSpiAdaptorCmd_t* response,
    bool* sequenceError)
{
    gmosStream_t* txStream = &(socket->common.txStream);
    uint8_t socketId = socket->socketId;
    uint8_t expectedControl =
        WIZNET_SPI_ADAPTOR_CTRL_SOCKET_REGS (socketId) |
        WIZNET_SPI_ADAPTOR_CTRL_READ_ENABLE;
    uint16_t bufReadPtr;

    // A response sequence error is generated if this is not a valid
    // response message.
    if ((response->address != 0x0022) ||
        (response->control != expectedControl) ||
        (response->size != 2)) {
        *sequenceError = true;
        return false;
    }

    // If ready, transfer the next UDP datagram to a local buffer for
    // further processing.
    *sequenceError = false;
    bufReadPtr = ((uint16_t) response->data.bytes [1]) +
        (((uint16_t) response->data.bytes [0]) << 8);
    if (gmosStreamAcceptBuffer (txStream, &(socket->payloadData))) {
        socket->data.active.dataPtr = bufReadPtr;
        return true;
    } else {
        return false;
    }
}

/*
 * Checks the interrupt status flags on completion of a UDP transmit
 * operation.
 */
static inline gmosTaskStatus_t gmosNalTcpipSocketUdpTxInterruptCheck (
    gmosNalTcpipSocket_t* socket, uint8_t* nextState)
{
    uint8_t intFlags = socket->interruptFlags;
    bool interruptHandled = false;

    // If an ARP timeout occurred, the outgoing UDP message remains
    // in the socket transmit buffer. It will be overwritten by the
    // next UDP transmit request. Notify ARP failure condition to next
    // higher layer.
    if ((intFlags & WIZNET_SPI_ADAPTOR_SOCKET_INT_TIMEOUT) != 0) {
        if (socket->common.notifyHandler != NULL) {
            socket->common.notifyHandler (socket->common.notifyData,
                GMOS_TCPIP_STACK_NOTIFY_UDP_ARP_TIMEOUT);
        }
        *nextState = WIZNET_SOCKET_UDP_STATE_READY;
        interruptHandled = true;
    }

    // After transmitting a UDP packet, start polling for new UDP
    // transmit or receive packets. Notify UDP datagram sent to next
    // higher layer.
    else if ((intFlags & WIZNET_SPI_ADAPTOR_SOCKET_INT_SENDOK) != 0) {
        if (socket->common.notifyHandler != NULL) {
            socket->common.notifyHandler (socket->common.notifyData,
                GMOS_TCPIP_STACK_NOTIFY_UDP_MESSAGE_SENT);
        }
        *nextState = WIZNET_SOCKET_UDP_STATE_READY;
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
 * Sends a UDP datagram to the specified remote IP address using an
 * opened UDP socket.
 */
gmosNetworkStatus_t gmosDriverTcpipUdpSendTo (
    gmosNalTcpipSocket_t* udpSocket, uint8_t* remoteAddr,
    uint16_t remotePort, gmosBuffer_t* payload)
{
    gmosStream_t* txStream = &(udpSocket->common.txStream);
    uint8_t socketPhase = udpSocket->socketState & WIZNET_SOCKET_PHASE_MASK;
    uint16_t payloadLength = gmosBufferGetSize (payload);
    uint8_t remotePortBytes [2];

    // Check that the specified socket has been opened for UDP data
    // transfer.
    if (socketPhase != WIZNET_SOCKET_PHASE_UDP) {
        return GMOS_NETWORK_STATUS_NOT_OPEN;
    }

    // Check that the payload length does not exceed the available
    // buffer memory on the WIZnet device.
    // TODO: Also check for length exceeding a single Ethernet frame,
    // since fragmentation is not supported.
    if (payloadLength > gmosNalTcpipSocketGetBufferSize (udpSocket)) {
        return GMOS_NETWORK_STATUS_OVERSIZED;
    }

    // Append the remote IPv4 address and port to the payload buffer
    // in network byte order and queue the UDP packet for transmission.
    remotePortBytes [0] = (uint8_t) (remotePort >> 8);
    remotePortBytes [1] = (uint8_t) remotePort;
    if ((gmosBufferAppend (payload, remoteAddr, 4)) &&
        (gmosBufferAppend (payload, remotePortBytes, 2)) &&
        (gmosStreamSendBuffer (txStream, payload))) {
        return GMOS_NETWORK_STATUS_SUCCESS;
    } else {
        gmosBufferResize (payload, payloadLength);
        return GMOS_NETWORK_STATUS_RETRY;
    }
}

/*
 * Receives a UDP datagram from a remote IP address using an opened UDP
 * socket.
 */
gmosNetworkStatus_t gmosDriverTcpipUdpReceiveFrom (
    gmosNalTcpipSocket_t* udpSocket, uint8_t* remoteAddr,
    uint16_t* remotePort, gmosBuffer_t* payload)
{
    gmosStream_t* rxStream = &(udpSocket->common.rxStream);
    uint8_t socketPhase = udpSocket->socketState & WIZNET_SOCKET_PHASE_MASK;
    uint16_t payloadLength;
    uint8_t remotePortBytes [2];

    // Check that the specified socket has been opened for UDP data
    // transfer.
    if (socketPhase != WIZNET_SOCKET_PHASE_UDP) {
        return GMOS_NETWORK_STATUS_NOT_OPEN;
    }

    // Attempt to read the next entry from the receive data stream.
    if (!gmosStreamAcceptBuffer (rxStream, payload)) {
        return GMOS_NETWORK_STATUS_RETRY;
    }

    // Extract the address and port number from the WIZnet UDP header.
    gmosBufferRead (payload, 0, remoteAddr, 4);
    gmosBufferRead (payload, 4, remotePortBytes, 2);
    *remotePort = ((uint16_t) remotePortBytes [1]) +
        (((uint16_t) remotePortBytes [0]) << 8);

    // Rebase the payload buffer to strip the WIZnet UDP header.
    payloadLength = gmosBufferGetSize (payload);
    gmosBufferRebase (payload, payloadLength - 8);
    return GMOS_NETWORK_STATUS_SUCCESS;
}

/*
 * Closes the specified UDP socket, releasing all allocated resources.
 */
gmosNetworkStatus_t gmosDriverTcpipUdpClose (
    gmosNalTcpipSocket_t* udpSocket)
{
    gmosNalTcpipState_t* nalData = udpSocket->common.tcpipDriver->nalData;
    uint8_t socketPhase = udpSocket->socketState & WIZNET_SOCKET_PHASE_MASK;

    // Check that the specified socket has been opened for UDP data
    // transfer.
    if (socketPhase != WIZNET_SOCKET_PHASE_UDP) {
        return GMOS_NETWORK_STATUS_NOT_OPEN;
    }

    // Set the close request flag to initiate a clean shutdown.
    udpSocket->interruptFlags |= WIZNET_SPI_ADAPTOR_SOCKET_FLAG_CLOSE_REQ;
    gmosSchedulerTaskResume (&(nalData->coreWorkerTask));
    return GMOS_NETWORK_STATUS_SUCCESS;
}

/*
 * Implements a socket processing tick cycle when in the UDP open phase.
 */
gmosTaskStatus_t gmosNalTcpipSocketProcessTickUdp (
    gmosNalTcpipSocket_t* socket)
{
    uint8_t nextState = socket->socketState & ~WIZNET_SOCKET_PHASE_MASK;
    uint8_t nextPhase = WIZNET_SOCKET_PHASE_UDP;
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;

    // Implement the UDP socket processing state machine.
    switch (nextState) {

        // Issue notification callback on opening the socket.
        case WIZNET_SOCKET_UDP_STATE_OPEN :
            if (socket->common.notifyHandler != NULL) {
                socket->common.notifyHandler (socket->common.notifyData,
                    GMOS_TCPIP_STACK_NOTIFY_UDP_SOCKET_OPENED);
            }
            nextState = WIZNET_SOCKET_UDP_STATE_READY;
            break;

        // Carry out processing for an open UDP socket.
        case WIZNET_SOCKET_UDP_STATE_READY :
            taskStatus = gmosNalTcpipSocketProcessUdp (socket, &nextState);
            break;

        // Issue a UDP socket close request and start the common socket
        // cleanup process.
        case WIZNET_SOCKET_UDP_STATE_CLOSE :
            if (gmosNalTcpipSocketIssueCommand (socket,
                WIZNET_SPI_ADAPTOR_SOCKET_COMMAND_CLOSE)) {
                if (socket->common.notifyHandler != NULL) {
                    socket->common.notifyHandler (socket->common.notifyData,
                        GMOS_TCPIP_STACK_NOTIFY_UDP_SOCKET_CLOSED);
                }
                nextPhase = WIZNET_SOCKET_PHASE_CLOSED;
                nextState = WIZNET_SOCKET_STATE_CLOSING_STATUS_READ;
            }
            break;

        // Wait for the UDP receive buffer check to complete via the
        // socket processing response callback.
        case WIZNET_SOCKET_UDP_STATE_RX_BUFFER_CHECK :
            taskStatus = GMOS_TASK_SUSPEND;
            break;

        // Request the UDP packet size from the buffer header.
        case WIZNET_SOCKET_UDP_STATE_RX_DATA_SIZE_READ :
            if (gmosNalTcpipSocketUdpRxDataSizeRead (socket)) {
                nextState = WIZNET_SOCKET_UDP_STATE_RX_DATA_SIZE_CHECK;
            }
            break;

        // Wait for the UDP data size check to complete via the socket
        // processing response callback.
        case WIZNET_SOCKET_UDP_STATE_RX_DATA_SIZE_CHECK :
            taskStatus = GMOS_TASK_SUSPEND;
            break;

        // Request the UDP packet data from the WIZnet socket buffer.
        case WIZNET_SOCKET_UDP_STATE_RX_DATA_BLOCK_READ :
            if (gmosNalTcpipSocketUdpRxDataBufRead (socket)) {
                nextState = WIZNET_SOCKET_UDP_STATE_RX_DATA_BLOCK_CHECK;
            }
            break;

        // Wait for the UDP buffer data transfer to complete via the
        // socket processing response callback.
        case WIZNET_SOCKET_UDP_STATE_RX_DATA_BLOCK_CHECK :
            taskStatus = GMOS_TASK_SUSPEND;
            break;

        // Write the updated read data pointer.
        case WIZNET_SOCKET_UDP_STATE_RX_POINTER_WRITE :
            if (gmosNalTcpipSocketRxPointerWrite (socket)) {
                nextState = WIZNET_SOCKET_UDP_STATE_RX_READ_CONFIRM;
            }
            break;

        // Confirm completion of the transaction by issuing the
        // received data command.
        case WIZNET_SOCKET_UDP_STATE_RX_READ_CONFIRM :
            if (gmosNalTcpipSocketIssueCommand (socket,
                WIZNET_SPI_ADAPTOR_SOCKET_COMMAND_RECV)) {
                nextState = WIZNET_SOCKET_UDP_STATE_RX_PACKET_QUEUE;
            }
            break;

        // Add the received UDP packet to the socket received data
        // queue.
        case WIZNET_SOCKET_UDP_STATE_RX_PACKET_QUEUE :
            if (gmosStreamSendBuffer (
                &(socket->common.rxStream), &(socket->payloadData))) {
                nextState = WIZNET_SOCKET_UDP_STATE_READY;
            }
            break;

        // Wait for the UDP transmit buffer check to complete via the
        // socket processing response callback.
        case WIZNET_SOCKET_UDP_STATE_TX_BUFFER_CHECK :
            taskStatus = GMOS_TASK_SUSPEND;
            break;

        // Set the remote address and port number for the UDP transfer.
        case WIZNET_SOCKET_UDP_STATE_TX_SET_REMOTE_ADDR :
            if (gmosNalTcpipSocketSetRemoteAddr (socket)) {
                nextState = WIZNET_SOCKET_UDP_STATE_TX_PAYLOAD_WRITE;
            }
            break;

        // Copy transmit data to the socket data buffer.
        case WIZNET_SOCKET_UDP_STATE_TX_PAYLOAD_WRITE :
            if (gmosNalTcpipSocketTxDataWrite (socket)) {
                nextState = WIZNET_SOCKET_UDP_STATE_TX_POINTER_WRITE;
            }
            break;

        // Update the transmit data pointer to the end of the valid
        // transmit data .
        case WIZNET_SOCKET_UDP_STATE_TX_POINTER_WRITE :
            if (gmosNalTcpipSocketTxPointerWrite (socket)) {
                nextState = WIZNET_SOCKET_UDP_STATE_TX_DATA_SEND;
            }
            break;

        // Send the UDP transmit data and then wait for completion.
        case WIZNET_SOCKET_UDP_STATE_TX_DATA_SEND :
            if (gmosNalTcpipSocketIssueCommand (socket,
                WIZNET_SPI_ADAPTOR_SOCKET_COMMAND_SEND)) {
                nextState = WIZNET_SOCKET_UDP_STATE_TX_INTERRUPT_CHECK;
                taskStatus = GMOS_TASK_SUSPEND;
            }
            break;

        // Wait for UDP transmit to complete via interrupt callback.
        case WIZNET_SOCKET_UDP_STATE_TX_INTERRUPT_CHECK :
            taskStatus = gmosNalTcpipSocketUdpTxInterruptCheck (
                socket, &nextState);
            break;
    }

    // Update the socket state and the task scheduling status.
    socket->socketState = nextPhase | nextState;
    return taskStatus;
}

/*
 * Implements a socket processing response callback when in the UDP open
 * phase.
 */
void gmosNalTcpipSocketProcessResponseUdp (
    gmosNalTcpipSocket_t* socket, wiznetSpiAdaptorCmd_t* response)
{
    gmosNalTcpipState_t* nalData = socket->common.tcpipDriver->nalData;
    bool sequenceError;
    uint8_t nextState = socket->socketState & ~WIZNET_SOCKET_PHASE_MASK;
    uint8_t nextPhase = WIZNET_SOCKET_PHASE_UDP;
    bool resumeProcessing = false;

    // Process SPI response messages according to the current state.
    switch (nextState) {

        // Implement UDP receive buffer status check. At least 8 bytes
        // must be available for processing the WIZnet UDP packet
        // information header. On success, prepare to read the UDP
        // packet length field.
        case WIZNET_SOCKET_UDP_STATE_RX_BUFFER_CHECK :
            if (gmosNalTcpipSocketRxBufferCheck (
                socket, response, 8, &sequenceError)) {
                nextState = WIZNET_SOCKET_UDP_STATE_RX_DATA_SIZE_READ;
            } else if (!sequenceError) {
                nextState = WIZNET_SOCKET_UDP_STATE_READY;
            } else {
                nextState = WIZNET_SOCKET_UDP_STATE_ERROR;
            }
            resumeProcessing = true;
            break;

        // Implement UDP packet data size check. On success, prepare to
        // read back the payload data.
        case WIZNET_SOCKET_UDP_STATE_RX_DATA_SIZE_CHECK :
            if (gmosNalTcpipSocketUdpRxDataSizeCheck (
                socket, response, &sequenceError)) {
                nextState = WIZNET_SOCKET_UDP_STATE_RX_DATA_BLOCK_READ;
            } else if (!sequenceError) {
                nextState = WIZNET_SOCKET_UDP_STATE_READY;
            } else {
                nextState = WIZNET_SOCKET_UDP_STATE_ERROR;
            }
            resumeProcessing = true;
            break;

        // Implement UDP packet data read check. On success, prepare to
        // update the buffer read pointer.
        case WIZNET_SOCKET_UDP_STATE_RX_DATA_BLOCK_CHECK :
            if (gmosNalTcpipSocketRxDataBlockCheck (
                socket, response, &sequenceError)) {
                nextState = WIZNET_SOCKET_UDP_STATE_RX_POINTER_WRITE;
            } else if (!sequenceError) {
                nextState = WIZNET_SOCKET_UDP_STATE_READY;
            } else {
                nextState = WIZNET_SOCKET_UDP_STATE_ERROR;
            }
            resumeProcessing = true;
            break;

        // Implement UDP transmit buffer status check. On success, copy
        // the UDP payload to the local processing buffer and prepare
        // the WIZnet socket for data transfer.
        case WIZNET_SOCKET_UDP_STATE_TX_BUFFER_CHECK :
            if (gmosNalTcpipSocketUdpTxBufferCheck (
                socket, response, &sequenceError)) {
                nextState = WIZNET_SOCKET_UDP_STATE_TX_SET_REMOTE_ADDR;
            } else if (!sequenceError) {
                nextState = WIZNET_SOCKET_UDP_STATE_READY;
            } else {
                nextState = WIZNET_SOCKET_UDP_STATE_ERROR;
            }
            resumeProcessing = true;
            break;
    }

    // Update the socket state and resume the worker task on a change.
    socket->socketState = nextPhase | nextState;
    if (resumeProcessing) {
        gmosSchedulerTaskResume (&(nalData->coreWorkerTask));
    }
}
