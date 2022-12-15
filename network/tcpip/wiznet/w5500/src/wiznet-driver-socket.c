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
 * a WIZnet W5500 TCP/IP offload device.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-scheduler.h"
#include "gmos-streams.h"
#include "gmos-driver-tcpip.h"
#include "gmos-tcpip-stack.h"
#include "wiznet-driver-tcpip.h"
#include "wiznet-driver-core.h"
#include "wiznet-spi-adaptor.h"

/*
 * Sets the local port number when opening a new socket.
 */
static inline bool gmosNalTcpipSocketSetPort (
    gmosNalTcpipSocket_t* socket)
{
    gmosNalTcpipState_t* nalData = socket->common.tcpipDriver->nalData;
    wiznetSpiAdaptorCmd_t cfgCommand;
    uint8_t socketId = socket->socketId;
    uint16_t localPort = socket->data.setup.localPort;

    // Set up the command to write to the local source port registers
    // at offset 0x0004 in network byte order.
    cfgCommand.address = 0x0004;
    cfgCommand.control =
        WIZNET_SPI_ADAPTOR_CTRL_SOCKET_REGS (socketId) |
        WIZNET_SPI_ADAPTOR_CTRL_WRITE_ENABLE |
        WIZNET_SPI_ADAPTOR_CTRL_DISCARD_RESPONSE;
    cfgCommand.size = 2;
    cfgCommand.data.bytes [0] = (uint8_t) (localPort >> 8);
    cfgCommand.data.bytes [1] = (uint8_t) (localPort);

    // Issue the socket configuration request.
    return wiznetSpiAdaptorStream_write (
        &nalData->spiCommandStream, &cfgCommand);
}

/*
 * Sets the socket type and then issues the open request.
 */
static inline bool gmosNalTcpipSocketSetOpen (
    gmosNalTcpipSocket_t* socket, bool isTcpSocket)
{
    gmosNalTcpipState_t* nalData = socket->common.tcpipDriver->nalData;
    wiznetSpiAdaptorCmd_t cfgCommand;
    uint8_t socketId = socket->socketId;

    // Set up the command to write to the socket mode and command
    // registers at 0x0000.
    cfgCommand.address = 0x0000;
    cfgCommand.control =
        WIZNET_SPI_ADAPTOR_CTRL_SOCKET_REGS (socketId) |
        WIZNET_SPI_ADAPTOR_CTRL_WRITE_ENABLE |
        WIZNET_SPI_ADAPTOR_CTRL_DISCARD_RESPONSE;
    cfgCommand.size = 2;
    cfgCommand.data.bytes [0] = isTcpSocket ? 0x01 : 0x02;
    cfgCommand.data.bytes [1] = WIZNET_SPI_ADAPTOR_SOCKET_COMMAND_OPEN;

    // Issue the socket configuration request.
    return wiznetSpiAdaptorStream_write (
        &nalData->spiCommandStream, &cfgCommand);
}

/*
 * Issues a read request for the socket status register.
 */
static bool gmosNalTcpipSocketStatusRead (
    gmosNalTcpipSocket_t* socket)
{
    gmosNalTcpipState_t* nalData = socket->common.tcpipDriver->nalData;
    wiznetSpiAdaptorCmd_t readCommand;
    uint8_t socketId = socket->socketId;

    // Set up the command to read from the socket status register at
    // offset 0x0003.
    readCommand.address = 0x0003;
    readCommand.control =
        WIZNET_SPI_ADAPTOR_CTRL_SOCKET_REGS (socketId) |
        WIZNET_SPI_ADAPTOR_CTRL_READ_ENABLE;
    readCommand.size = 1;

    // Issue the socket status read request.
    return wiznetSpiAdaptorStream_write (
        &nalData->spiCommandStream, &readCommand);
}

/*
 * Checks the expected contents of the socket status register.
 */
static bool gmosNalTcpipSocketStatusCheck (
    gmosNalTcpipSocket_t* socket, wiznetSpiAdaptorCmd_t* response,
    uint8_t expectedStatus, bool* sequenceError)
{
    uint8_t socketId = socket->socketId;
    uint8_t expectedControl =
        WIZNET_SPI_ADAPTOR_CTRL_SOCKET_REGS (socketId) |
        WIZNET_SPI_ADAPTOR_CTRL_READ_ENABLE;

    // A response sequence error is generated if this is not a valid
    // response message.
    if ((response->address != 0x0003) ||
        (response->control != expectedControl) ||
        (response->size != 1)) {
        *sequenceError = true;
        return false;
    }

    // Check for expected status.
    *sequenceError = false;
    if (response->data.bytes [0] == expectedStatus) {
        return true;
    } else {
        return false;
    }
}

/*
 * Perform socket cleanup after closing.
 */
static inline void gmosNalTcpipSocketCleanup (
    gmosNalTcpipSocket_t* socket)
{
    gmosBuffer_t* payloadData = &(socket->payloadData);
    gmosStream_t* txStream = &(socket->common.txStream);
    gmosStream_t* rxStream = &(socket->common.rxStream);

    // Release any locally allocated payload data.
    gmosBufferReset (payloadData, 0);

    // Drain the socket transmit queue.
    while (gmosStreamAcceptBuffer (txStream, payloadData)) {
        gmosBufferReset (payloadData, 0);
    }

    // Drain the socket receive queue.
    while (gmosStreamAcceptBuffer (rxStream, payloadData)) {
        gmosBufferReset (payloadData, 0);
    }

    // Disable socket status notification callbacks.
    socket->common.notifyHandler = NULL;
    socket->common.notifyData = NULL;
}

/*
 * Sets the interrupt enable flags for the specified TCP/IP socket.
 */
static bool gmosNalTcpipSocketInterruptEnable (
    gmosNalTcpipSocket_t* socket, bool isTcpSocket, bool isEnabled)
{
    gmosNalTcpipState_t* nalData = socket->common.tcpipDriver->nalData;
    wiznetSpiAdaptorCmd_t intEnableCommand;
    uint8_t socketId = socket->socketId;
    uint8_t intEnables;

    // Disable all interrupts if requested.
    if (!isEnabled) {
        intEnables = 0;
        socket->interruptClear = 0xFF;
    }

    // For TCP sockets, include all interrupt sources.
    else if (isTcpSocket) {
        intEnables =
            WIZNET_SPI_ADAPTOR_SOCKET_INT_CON |
            WIZNET_SPI_ADAPTOR_SOCKET_INT_DISCON |
            WIZNET_SPI_ADAPTOR_SOCKET_INT_RECV |
            WIZNET_SPI_ADAPTOR_SOCKET_INT_TIMEOUT |
            WIZNET_SPI_ADAPTOR_SOCKET_INT_SENDOK;
        socket->interruptFlags = 0;
        socket->interruptClear = 0;
    }

    // For UDP sockets, the connection handling interrupts are not
    // required.
    else {
        intEnables =
            WIZNET_SPI_ADAPTOR_SOCKET_INT_RECV |
            WIZNET_SPI_ADAPTOR_SOCKET_INT_TIMEOUT |
            WIZNET_SPI_ADAPTOR_SOCKET_INT_SENDOK;
        socket->interruptFlags = 0;
        socket->interruptClear = 0;
    }

    // Set up the command to write to the interrupt mask register at
    // 0x002C.
    intEnableCommand.address = 0x002C;
    intEnableCommand.control =
        WIZNET_SPI_ADAPTOR_CTRL_SOCKET_REGS (socketId) |
        WIZNET_SPI_ADAPTOR_CTRL_WRITE_ENABLE |
        WIZNET_SPI_ADAPTOR_CTRL_DISCARD_RESPONSE;
    intEnableCommand.size = 1;
    intEnableCommand.data.bytes [0] = intEnables;

    // Issue the socket interrupt enable request.
    return wiznetSpiAdaptorStream_write (
        &nalData->spiCommandStream, &intEnableCommand);
}

/*
 * Issues a command to clear the selected socket interrupts.
 */
static inline void gmosNalTcpipSocketInterruptClear (
    gmosNalTcpipSocket_t* socket)
{
    gmosNalTcpipState_t* nalData = socket->common.tcpipDriver->nalData;
    wiznetSpiAdaptorCmd_t socketCommand;
    uint8_t socketId = socket->socketId;

    // Clear hardware interrupts if required.
    if ((socket->interruptClear & 0x1F) != 0) {

        // Set up the command to write to the interrupt clear register
        // at offset address 0x0002.
        socketCommand.address = 0x0002;
        socketCommand.control =
            WIZNET_SPI_ADAPTOR_CTRL_SOCKET_REGS (socketId) |
            WIZNET_SPI_ADAPTOR_CTRL_WRITE_ENABLE |
            WIZNET_SPI_ADAPTOR_CTRL_DISCARD_RESPONSE;
        socketCommand.size = 1;
        socketCommand.data.bytes [0] = socket->interruptClear & 0x1F;

        // Issue the socket interrupt status clear request.
        if (wiznetSpiAdaptorStream_write (
            &nalData->spiCommandStream, &socketCommand)) {
            socket->interruptFlags &= ~socket->interruptClear;
            socket->interruptClear = 0;
        }
    }

    // Clear software only flags.
    else {
        socket->interruptFlags &= ~socket->interruptClear;
        socket->interruptClear = 0;
    }
}

/*
 * Implement common socket processing phase, which performs socket setup
 * on opening.
 */
static inline gmosTaskStatus_t gmosNalTcpipSocketProcessTickCommon (
    gmosNalTcpipSocket_t* socket)
{
    uint8_t nextState = socket->socketState & ~WIZNET_SOCKET_PHASE_MASK;
    uint8_t nextPhase = WIZNET_SOCKET_PHASE_CLOSED;
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    bool isTcpSocket = false;

    // Implement the common socket processing state machine.
    switch (nextState) {

        // Suspend further processing in the idle state.
        case WIZNET_SOCKET_STATE_FREE :
            taskStatus = GMOS_TASK_SUSPEND;
            break;

        // Set the local source port for all sockets.
        case WIZNET_SOCKET_STATE_TCP_SET_PORT :
            isTcpSocket = true;
        case WIZNET_SOCKET_STATE_UDP_SET_PORT :
            if (gmosNalTcpipSocketSetPort (socket)) {
                nextState = isTcpSocket ?
                    WIZNET_SOCKET_STATE_TCP_SET_OPEN :
                    WIZNET_SOCKET_STATE_UDP_SET_OPEN;
            }
            break;

        // Send the command to open the socket on the W5500.
        case WIZNET_SOCKET_STATE_TCP_SET_OPEN :
            isTcpSocket = true;
        case WIZNET_SOCKET_STATE_UDP_SET_OPEN :
            if (gmosNalTcpipSocketSetOpen (socket, isTcpSocket)) {
                nextState = isTcpSocket ?
                    WIZNET_SOCKET_STATE_TCP_OPEN_STATUS_READ :
                    WIZNET_SOCKET_STATE_UDP_OPEN_STATUS_READ;
            }
            break;

        // Issue a read request for the socket status register.
        case WIZNET_SOCKET_STATE_TCP_OPEN_STATUS_READ :
            isTcpSocket = true;
        case WIZNET_SOCKET_STATE_UDP_OPEN_STATUS_READ :
            if (gmosNalTcpipSocketStatusRead (socket)) {
                nextState = isTcpSocket ?
                    WIZNET_SOCKET_STATE_TCP_OPEN_STATUS_CHECK :
                    WIZNET_SOCKET_STATE_UDP_OPEN_STATUS_CHECK;
                taskStatus = GMOS_TASK_SUSPEND;
            }
            break;

        // Wait for the socket status register read to complete via the
        // socket processing response callback.
        case WIZNET_SOCKET_STATE_TCP_OPEN_STATUS_CHECK :
        case WIZNET_SOCKET_STATE_UDP_OPEN_STATUS_CHECK :
            taskStatus = GMOS_TASK_SUSPEND;
            break;

        // Set the required interrupt enable flags.
        case WIZNET_SOCKET_STATE_TCP_INTERRUPT_ENABLE :
            isTcpSocket = true;
        case WIZNET_SOCKET_STATE_UDP_INTERRUPT_ENABLE :
            if (gmosNalTcpipSocketInterruptEnable (
                socket, isTcpSocket, true)) {
                if (isTcpSocket) {
                    nextPhase = WIZNET_SOCKET_PHASE_TCP;
                    nextState = WIZNET_SOCKET_TCP_STATE_OPEN;
                } else {
                    nextPhase = WIZNET_SOCKET_PHASE_UDP;
                    nextState = WIZNET_SOCKET_UDP_STATE_OPEN;
                }
            }
            GMOS_LOG_FMT (LOG_DEBUG,
                "WIZnet TCP/IP : Socket %d opened for %s.",
                socket->socketId, isTcpSocket ? "TCP" : "UDP");
            break;

        // Request the socket status while processing a close request.
        case WIZNET_SOCKET_STATE_CLOSING_STATUS_READ :
            if (gmosNalTcpipSocketStatusRead (socket)) {
                nextState = WIZNET_SOCKET_STATE_CLOSING_STATUS_CHECK;
                taskStatus = GMOS_TASK_SUSPEND;
            }
            break;

        // Wait for the socket status register read to complete via the
        // socket processing response callback.
        case WIZNET_SOCKET_STATE_CLOSING_STATUS_CHECK :
            taskStatus = GMOS_TASK_SUSPEND;
            break;

        // Disable further interrupts for this socket.
        case WIZNET_SOCKET_STATE_CLOSING_INTERRUPT_DISABLE :
            if (gmosNalTcpipSocketInterruptEnable (
                socket, isTcpSocket, false)) {
                nextState = WIZNET_SOCKET_STATE_CLOSING_CLEANUP;
            }
            break;

        // Perform socket cleanup, releasing any allocated resources.
        case WIZNET_SOCKET_STATE_CLOSING_CLEANUP :
            gmosNalTcpipSocketCleanup (socket);
            nextState = WIZNET_SOCKET_STATE_FREE;
            GMOS_LOG_FMT (LOG_DEBUG,
                "WIZnet TCP/IP : Socket %d closed.", socket->socketId);
            break;

        // Generate an assertion condition in failure mode.
        default :
            GMOS_ASSERT_FAIL ("Unrecoverable error in WIZnet core.");
            taskStatus = GMOS_TASK_SUSPEND;
            break;
    }

    // Update the socket state and the task scheduling status.
    socket->socketState = nextPhase | nextState;
    return taskStatus;
}

/*
 * Implement common socket processing phase response handling.
 */
static inline void gmosNalTcpipSocketProcessResponseCommon (
    gmosNalTcpipSocket_t* socket, wiznetSpiAdaptorCmd_t* response)
{
    gmosNalTcpipState_t* nalData = socket->common.tcpipDriver->nalData;
    bool sequenceError;
    uint8_t expectedStatus;
    uint8_t nextState = socket->socketState & ~WIZNET_SOCKET_PHASE_MASK;
    uint8_t nextPhase = WIZNET_SOCKET_PHASE_CLOSED;
    bool isTcpSocket = false;
    bool resumeProcessing = false;

    // Process SPI response messages according to the current state.
    switch (nextState) {

        // Implement status register check after opening a new socket.
        // Retry the status request if required.
        case WIZNET_SOCKET_STATE_TCP_OPEN_STATUS_CHECK :
            isTcpSocket = true;
        case WIZNET_SOCKET_STATE_UDP_OPEN_STATUS_CHECK :
            expectedStatus = isTcpSocket ?
                WIZNET_SPI_ADAPTOR_SOCKET_STATUS_INIT_TCP :
                WIZNET_SPI_ADAPTOR_SOCKET_STATUS_UDP;
            if (gmosNalTcpipSocketStatusCheck (socket, response,
                expectedStatus, &sequenceError)) {
                nextState = isTcpSocket ?
                    WIZNET_SOCKET_STATE_TCP_INTERRUPT_ENABLE :
                    WIZNET_SOCKET_STATE_UDP_INTERRUPT_ENABLE;
            } else if (!sequenceError) {
                nextState = isTcpSocket ?
                    WIZNET_SOCKET_STATE_TCP_OPEN_STATUS_READ :
                    WIZNET_SOCKET_STATE_UDP_OPEN_STATUS_READ;
            } else {
                nextState = WIZNET_SOCKET_STATE_ERROR;
            }
            resumeProcessing = true;
            break;

        // Implement status register check when closing a socket.
        // Retry the status request if required.
        case WIZNET_SOCKET_STATE_CLOSING_STATUS_CHECK :
            expectedStatus = WIZNET_SPI_ADAPTOR_SOCKET_STATUS_CLOSED;
            if (gmosNalTcpipSocketStatusCheck (socket, response,
                expectedStatus, &sequenceError)) {
                nextState = WIZNET_SOCKET_STATE_CLOSING_INTERRUPT_DISABLE;
            } else if (!sequenceError) {
                nextState = WIZNET_SOCKET_STATE_CLOSING_STATUS_READ;
            } else {
                nextState = WIZNET_SOCKET_STATE_ERROR;
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

/*
 * Performs socket specific initialisation on startup.
 */
void gmosNalTcpipSocketInit (
    gmosDriverTcpip_t* tcpipDriver, gmosNalTcpipSocket_t* socket)
{
    gmosNalTcpipState_t* nalData = tcpipDriver->nalData;

    // The socket transmit stream is configured to use the driver worker
    // task as the consumer.
    gmosStreamInit (&(socket->common.txStream),
        &(nalData->coreWorkerTask), GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE);

    // The socket receive stream is configured with no consumer task.
    // This will be dynamically assigned when the socket is opened.
    gmosStreamInit (&(socket->common.rxStream),
        NULL, GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE);

    // Initialise the local payload buffer.
    gmosBufferInit (&(socket->payloadData));

    // Set the socket state as being available for use.
    socket->socketState = WIZNET_SOCKET_STATE_FREE;

    // Hold a local reference to the associated TCP/IP driver.
    socket->common.tcpipDriver = tcpipDriver;

    // Clear local interrupt flag state.
    socket->interruptFlags = 0;
    socket->interruptClear = 0;

    // Disable socket status notification callbacks.
    socket->common.notifyHandler = NULL;
    socket->common.notifyData = NULL;
}

/*
 * Attempts to open a new UDP socket for subsequent use.
 */
gmosNalTcpipSocket_t* gmosDriverTcpipUdpOpen (
    gmosDriverTcpip_t* tcpipDriver, bool useIpv6,
    uint16_t localPort, gmosTaskState_t* appTask,
    gmosTcpipStackNotifyCallback_t notifyHandler, void* notifyData)
{
    uint8_t i;
    gmosNalTcpipState_t* nalData = tcpipDriver->nalData;
    gmosNalTcpipSocket_t* socket = NULL;

    // Ensure IPv6 is not selected.
    GMOS_ASSERT (ASSERT_FAILURE, useIpv6 == false,
        "IPv6 not supported by WIZnet W5500.");

    // Sockets can not be opened until a physical layer link has been
    // established.
    if ((nalData->wiznetCoreFlags &
        WIZNET_SPI_ADAPTOR_CORE_FLAG_PHY_UP) == 0) {
        return NULL;
    }

    // UDP sockets are allocated from the 'end' of the socket list, so
    // that sockets with the smaller buffers are used for UDP.
    for (i = 0; i < GMOS_CONFIG_TCPIP_MAX_SOCKETS; i++) {
        gmosNalTcpipSocket_t* nextSocket =
            &(nalData->socketData [GMOS_CONFIG_TCPIP_MAX_SOCKETS - 1 - i]);
        if (nextSocket->socketState == WIZNET_SOCKET_STATE_FREE) {
            socket = nextSocket;
            break;
        }
    }

    // Start the UDP socket setup process, storing the local port number
    // for future reference.
    if (socket != NULL) {
        socket->data.setup.localPort = localPort;
        socket->socketState = WIZNET_SOCKET_STATE_UDP_SET_PORT;
        socket->common.notifyHandler = notifyHandler;
        socket->common.notifyData = notifyData;
        gmosStreamSetConsumerTask (&(socket->common.rxStream), appTask);
        gmosSchedulerTaskResume (&(nalData->coreWorkerTask));
    }
    return socket;
}

/*
 * Attempts to open a new TCP socket for subsequent use.
 */
gmosNalTcpipSocket_t* gmosDriverTcpipTcpOpen (
    gmosDriverTcpip_t* tcpipDriver, bool useIpv6,
    uint16_t localPort, gmosTaskState_t* appTask,
    gmosTcpipStackNotifyCallback_t notifyHandler, void* notifyData)
{
    uint8_t i;
    gmosNalTcpipState_t* nalData = tcpipDriver->nalData;
    gmosNalTcpipSocket_t* socket = NULL;

    // Ensure IPv6 is not selected.
    GMOS_ASSERT (ASSERT_FAILURE, useIpv6 == false,
        "IPv6 not supported by WIZnet W5500.");

    // Sockets can not be opened until a physical layer link has been
    // established.
    if ((nalData->wiznetCoreFlags &
        WIZNET_SPI_ADAPTOR_CORE_FLAG_PHY_UP) == 0) {
        return NULL;
    }

    // TCP sockets are allocated from the 'start' of the socket list, so
    // that sockets with the larger buffers are used for TCP.
    for (i = 0; i < GMOS_CONFIG_TCPIP_MAX_SOCKETS; i++) {
        gmosNalTcpipSocket_t* nextSocket = &(nalData->socketData [i]);
        if (nextSocket->socketState == WIZNET_SOCKET_STATE_FREE) {
            socket = nextSocket;
            break;
        }
    }

    // Start the TCP socket setup process, storing the local port number
    // for future reference.
    if (socket != NULL) {
        socket->data.setup.localPort = localPort;
        socket->socketState = WIZNET_SOCKET_STATE_TCP_SET_PORT;
        socket->common.notifyHandler = notifyHandler;
        socket->common.notifyData = notifyData;
        gmosStreamSetConsumerTask (&(socket->common.rxStream), appTask);
        gmosSchedulerTaskResume (&(nalData->coreWorkerTask));
    }
    return socket;
}

/*
 * Implements a socket processing cycle. This updates the local socket
 * state as required and then returns a task status value indicating the
 * next required execution time.
 */
gmosTaskStatus_t gmosNalTcpipSocketProcessTick (
    gmosNalTcpipSocket_t* socket)
{
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;

    // Clearing interrupts takes priority over all other actions.
    if (socket->interruptClear != 0) {
        gmosNalTcpipSocketInterruptClear (socket);
    }

    // Select the appropriate socket phase state machine.
    else switch (socket->socketState & WIZNET_SOCKET_PHASE_MASK) {

        // Invoke the UDP processing state machine.
        case WIZNET_SOCKET_PHASE_UDP :
            taskStatus = gmosNalTcpipSocketProcessTickUdp (socket);
            break;

        // Invoke the TCP processing state machine.
        case WIZNET_SOCKET_PHASE_TCP :
            taskStatus = gmosNalTcpipSocketProcessTickTcp (socket);
            break;

        // Invoke the common socket processing state machine.
        default :
            taskStatus = gmosNalTcpipSocketProcessTickCommon (socket);
            break;
    }
    return taskStatus;
}

/*
 * Implements a socket processing response callback. All SPI response
 * messages which correspond to the socket are sent via this callback.
 */
void gmosNalTcpipSocketProcessResponse (
    gmosNalTcpipSocket_t* socket, wiznetSpiAdaptorCmd_t* response)
{
    gmosNalTcpipState_t* nalData = socket->common.tcpipDriver->nalData;

    // Interrupt events are detected as asynchronous read responses from
    // the interrupt status register.
    if ((response->address == 0x0002) &&
        (response->size == 2)) {
        GMOS_LOG_FMT (LOG_VERBOSE,
            "WIZnet TCP/IP : Socket %d interrupts 0x%02X, status 0x%02X.",
            socket->socketId, response->data.bytes [0],
            response->data.bytes [1]);
        socket->interruptFlags |= response->data.bytes [0];
        gmosSchedulerTaskResume (&(nalData->coreWorkerTask));
    }

    // Select the appropriate processing phase to handle other
    // responses.
    else switch (socket->socketState & WIZNET_SOCKET_PHASE_MASK) {

        // Invoke the UDP processing state machine.
        case WIZNET_SOCKET_PHASE_UDP :
            gmosNalTcpipSocketProcessResponseUdp (socket, response);
            break;

        // Invoke the TCP processing state machine.
        case WIZNET_SOCKET_PHASE_TCP :
            gmosNalTcpipSocketProcessResponseTcp (socket, response);
            break;

        // Invoke the common socket processing state machine.
        default :
            gmosNalTcpipSocketProcessResponseCommon (socket, response);
            break;
    }
}
