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
 * This file defines the core driver functionality for accessing a
 * WIZnet W5500 TCP/IP offload device.
 */

#ifndef WIZNET_DRIVER_CORE_H
#define WIZNET_DRIVER_CORE_H

#include <stdint.h>
#include <stdbool.h>
#include "gmos-config.h"
#include "gmos-scheduler.h"
#include "gmos-driver-tcpip.h"
#include "wiznet-driver-tcpip.h"
#include "wiznet-spi-adaptor.h"

/**
 * Specify the available core processing states.
 */
typedef enum {
    WIZNET_CORE_STATE_COMMON_VER_READ,
    WIZNET_CORE_STATE_COMMON_VER_CHECK,
    WIZNET_CORE_STATE_COMMON_CFG_SET,
    WIZNET_CORE_STATE_COMMON_CFG_READ,
    WIZNET_CORE_STATE_COMMON_CFG_CHECK,
    WIZNET_CORE_STATE_COMMON_CFG_INT_ENABLE,
    WIZNET_CORE_STATE_COMMON_CFG_INT_READ,
    WIZNET_CORE_STATE_COMMON_CFG_INT_CHECK,
    WIZNET_CORE_STATE_SOCKET_CFG_SET,
    WIZNET_CORE_STATE_SOCKET_CFG_READ,
    WIZNET_CORE_STATE_SOCKET_CFG_CHECK,
    WIZNET_CORE_STATE_STARTUP_PHY_READ,
    WIZNET_CORE_STATE_STARTUP_PHY_CHECK,
    WIZNET_CORE_STATE_RUNNING_INT_IDLE,
    WIZNET_CORE_STATE_RUNNING_INT_ACTIVE,
    WIZNET_CORE_STATE_ERROR
} wiznetCoreState_t;

/**
 * Specify the available socket operating phases.
 */
typedef enum {
    WIZNET_SOCKET_PHASE_CLOSED = 0x00,
    WIZNET_SOCKET_PHASE_UDP    = 0x40,
    WIZNET_SOCKET_PHASE_TCP    = 0x80,
    WIZNET_SOCKET_PHASE_MASK   = 0xC0
} wiznetSocketPhase_t;

/**
 * Specify the common socket processing states, which are used while
 * the socket is in the 'closed' phase.
 */
typedef enum {
    WIZNET_SOCKET_STATE_FREE,
    WIZNET_SOCKET_STATE_ERROR,
    WIZNET_SOCKET_STATE_UDP_SET_PORT,
    WIZNET_SOCKET_STATE_UDP_SET_OPEN,
    WIZNET_SOCKET_STATE_UDP_OPEN_STATUS_READ,
    WIZNET_SOCKET_STATE_UDP_OPEN_STATUS_CHECK,
    WIZNET_SOCKET_STATE_UDP_INTERRUPT_ENABLE,
    WIZNET_SOCKET_STATE_TCP_SET_PORT,
    WIZNET_SOCKET_STATE_TCP_SET_OPEN,
    WIZNET_SOCKET_STATE_TCP_OPEN_STATUS_READ,
    WIZNET_SOCKET_STATE_TCP_OPEN_STATUS_CHECK,
    WIZNET_SOCKET_STATE_TCP_INTERRUPT_ENABLE,
    WIZNET_SOCKET_STATE_CLOSING_STATUS_READ,
    WIZNET_SOCKET_STATE_CLOSING_STATUS_CHECK,
    WIZNET_SOCKET_STATE_CLOSING_INTERRUPT_DISABLE,
    WIZNET_SOCKET_STATE_CLOSING_CLEANUP
} wiznetSocketState_t;

/**
 * Specify the UDP specific socket processing states, which are used
 * while the socket is in the 'UDP open' phase.
 */
typedef enum {
    WIZNET_SOCKET_UDP_STATE_OPEN,
    WIZNET_SOCKET_UDP_STATE_READY,
    WIZNET_SOCKET_UDP_STATE_ERROR,
    WIZNET_SOCKET_UDP_STATE_CLOSE,
    WIZNET_SOCKET_UDP_STATE_RX_BUFFER_CHECK,
    WIZNET_SOCKET_UDP_STATE_RX_DATA_SIZE_READ,
    WIZNET_SOCKET_UDP_STATE_RX_DATA_SIZE_CHECK,
    WIZNET_SOCKET_UDP_STATE_RX_DATA_BLOCK_READ,
    WIZNET_SOCKET_UDP_STATE_RX_DATA_BLOCK_CHECK,
    WIZNET_SOCKET_UDP_STATE_RX_POINTER_WRITE,
    WIZNET_SOCKET_UDP_STATE_RX_READ_CONFIRM,
    WIZNET_SOCKET_UDP_STATE_RX_PACKET_QUEUE,
    WIZNET_SOCKET_UDP_STATE_TX_BUFFER_CHECK,
    WIZNET_SOCKET_UDP_STATE_TX_SET_REMOTE_ADDR,
    WIZNET_SOCKET_UDP_STATE_TX_PAYLOAD_WRITE,
    WIZNET_SOCKET_UDP_STATE_TX_POINTER_WRITE,
    WIZNET_SOCKET_UDP_STATE_TX_DATA_SEND,
    WIZNET_SOCKET_UDP_STATE_TX_INTERRUPT_CHECK
} wiznetSocketUdpState_t;

/**
 * Specify the TCP specific socket processing states, which are used
 * while the socket is in the 'TCP open' phase.
 */
typedef enum {
    WIZNET_SOCKET_TCP_STATE_OPEN,
    WIZNET_SOCKET_TCP_STATE_READY,
    WIZNET_SOCKET_TCP_STATE_ERROR,
    WIZNET_SOCKET_TCP_STATE_CLOSE,
    WIZNET_SOCKET_TCP_STATE_DISCONNECT,
    WIZNET_SOCKET_TCP_STATE_SET_REMOTE_ADDR,
    WIZNET_SOCKET_TCP_STATE_CONNECT_REQUEST,
    WIZNET_SOCKET_TCP_STATE_CONNECT_WAIT,
    WIZNET_SOCKET_TCP_STATE_ACTIVE,
    WIZNET_SOCKET_TCP_STATE_SLEEPING,
    WIZNET_SOCKET_TCP_STATE_RX_BUFFER_CHECK,
    WIZNET_SOCKET_TCP_STATE_RX_DATA_BLOCK_READ,
    WIZNET_SOCKET_TCP_STATE_RX_DATA_BLOCK_CHECK,
    WIZNET_SOCKET_TCP_STATE_RX_POINTER_WRITE,
    WIZNET_SOCKET_TCP_STATE_RX_READ_CONFIRM,
    WIZNET_SOCKET_TCP_STATE_RX_DATA_BLOCK_QUEUE,
    WIZNET_SOCKET_TCP_STATE_TX_BUFFER_CHECK,
    WIZNET_SOCKET_TCP_STATE_TX_PAYLOAD_WRITE,
    WIZNET_SOCKET_TCP_STATE_TX_PAYLOAD_APPEND,
    WIZNET_SOCKET_TCP_STATE_TX_POINTER_WRITE,
    WIZNET_SOCKET_TCP_STATE_TX_DATA_SEND,
    WIZNET_SOCKET_TCP_STATE_TX_INTERRUPT_CHECK
} wiznetSocketTcpState_t;

/**
 * Performs socket specific initialisation on startup.
 * @param tcpipStack This is the TCP/IP stack for which the socket is
 *     being initialised.
 * @param socket This is the socket instance that is to be initialised.
 */
void gmosNalTcpipSocketInit (
    gmosDriverTcpip_t* tcpipStack, gmosTcpipStackSocket_t* socket);

/**
 * Implements a socket processing tick cycle. This updates the local
 * socket state as required and then returns a task status value
 * indicating the next required execution time.
 * @param socket This is the socket instance that is to be processed.
 * @return Returns the GubbinsMOS task status which indicates when the
 *     socket processing state machine next needs to be updated.
 */
gmosTaskStatus_t gmosNalTcpipSocketProcessTick (
    gmosTcpipStackSocket_t* socket);

/**
 * Implements a socket processing tick cycle when in the UDP open phase.
 * This updates the local socket state as required and then returns a
 * task status value indicating the next required execution time.
 * @param socket This is the socket instance that is to be processed.
 * @return Returns the GubbinsMOS task status which indicates when the
 *     socket processing state machine next needs to be updated.
 */
gmosTaskStatus_t gmosNalTcpipSocketProcessTickUdp (
    gmosTcpipStackSocket_t* socket);

/**
 * Implements a socket processing tick cycle when in the TCP open phase.
 * This updates the local socket state as required and then returns a
 * task status value indicating the next required execution time.
 * @param socket This is the socket instance that is to be processed.
 * @return Returns the GubbinsMOS task status which indicates when the
 *     socket processing state machine next needs to be updated.
 */
gmosTaskStatus_t gmosNalTcpipSocketProcessTickTcp (
    gmosTcpipStackSocket_t* socket);

/**
 * Implements a socket processing response callback. All SPI response
 * messages which correspond to the socket are sent via this callback.
 * @param socket This is the socket instance that is to process the SPI
 *     response message.
 * @param response This is the SPI response message that is to be
 *     processed.
 */
void gmosNalTcpipSocketProcessResponse (
    gmosTcpipStackSocket_t* socket, wiznetSpiAdaptorCmd_t* response);

/**
 * Implements a socket processing response callback when in the UDP open
 * phase. All SPI response messages which correspond to the socket are
 * sent via this callback.
 * @param socket This is the socket instance that is to process the SPI
 *     response message.
 * @param response This is the SPI response message that is to be
 *     processed.
 */
void gmosNalTcpipSocketProcessResponseUdp (
    gmosTcpipStackSocket_t* socket, wiznetSpiAdaptorCmd_t* response);

/**
 * Implements a socket processing response callback when in the TCP open
 * phase. All SPI response messages which correspond to the socket are
 * sent via this callback.
 * @param socket This is the socket instance that is to process the SPI
 *     response message.
 * @param response This is the SPI response message that is to be
 *     processed.
 */
void gmosNalTcpipSocketProcessResponseTcp (
    gmosTcpipStackSocket_t* socket, wiznetSpiAdaptorCmd_t* response);

/**
 * Gets the W5500 transmit and receive buffer size associated with a
 * given socket.
 * @param socket This is the socket for which the buffer size is being
 *     requested.
 * @return Returns the transmit and receive buffer size for the
 *     specified socket.
 */
uint16_t gmosNalTcpipSocketGetBufferSize (
    gmosTcpipStackSocket_t* socket);

/**
 * Issues a command for the WIZnet socket controller. This writes the
 * specified command value to the command register for the specified
 * socket.
 * @param socket This is the socket for which the command is being
 *     issued.
 * @param command This is the socket command that is to be issued.
 * @return Returns a boolean value which will be set to 'true' if the
 *     socket command was issued and 'false' if the request can not be
 *     serviced at this time.
 */
bool gmosNalTcpipSocketIssueCommand (gmosTcpipStackSocket_t* socket,
    wiznetSpiAdaptorSocketCommands_t command);

/**
 * Sets the remote IP address and port for an outgoing TCP connection
 * or UDP datagram. The remote IP address and port information should
 * be stored in network byte order at the end of the current local data
 * buffer.
 * @param socket This is the socket for which the remote address
 *     information is being set.
 * @return Returns a boolean value which will be set to 'true' if the
 *     request was issued and 'false' if the request can not be serviced
 *     at this time.
 */
bool gmosNalTcpipSocketSetRemoteAddr (gmosTcpipStackSocket_t* socket);

/**
 * Checks the status of the socket receive buffer. The receive state
 * machine will only proceed if the buffer status fields are consistent.
 * @param socket This is the socket for which the receive buffer status
 *     is being checked.
 * @param response This is the SPI adaptor response to the previous
 *     receive buffer status register read request.
 * @param rxThreshold This specifies the minimum number of octets that
 *     must be received before further processing can proceed.
 * @param sequenceError This is a pointer to a boolean value which will
 *     be set to 'true' if the SPI adaptor response is a valid receive
 *     buffer status register read response and 'false' otherwise.
 * @return Returns a boolean value which will be set to 'true' if the
 *     buffer check succeeded and further processing can proceed and
 *     'false' otherwise.
 */
bool gmosNalTcpipSocketRxBufferCheck (
    gmosTcpipStackSocket_t* socket, wiznetSpiAdaptorCmd_t* response,
    uint16_t rxThreshold, bool* sequenceError);

/**
 * Checks the result of a data block read operation.
 * @param socket This is the socket for which the receive buffer block
 *     read operation is being checked.
 * @param response This is the SPI adaptor response to the previous
 *     receive buffer block read request.
 * @param sequenceError This is a pointer to a boolean value which will
 *     be set to 'true' if the SPI adaptor response is a valid receive
 *     buffer block read response and 'false' otherwise.
 * @return Returns a boolean value which will be set to 'true' if the
 *     block read succeeded and further processing can proceed and
 *     'false' otherwise.
 */
bool gmosNalTcpipSocketRxDataBlockCheck (
    gmosTcpipStackSocket_t* socket, wiznetSpiAdaptorCmd_t* response,
    bool* sequenceError);

/**
 * Writes the new read data pointer value after reading an inbound TCP
 * data block or UDP packet.
 * @param socket This is the socket for which the WIZnet socket buffer
 *     read data pointer is being written.
 * @return Returns a boolean value which will be set to 'true' if the
 *     request was issued and 'false' if the request can not be serviced
 *     at this time.
 */
bool gmosNalTcpipSocketRxPointerWrite (gmosTcpipStackSocket_t* socket);

/**
 * Writes the contents of the local buffer to the WIZnet socket memory,
 * starting from the current socket memory address pointer.
 * @param socket This is the socket for which the local buffer is being
 *     written to WIZnet socket memory.
 * @return Returns a boolean value which will be set to 'true' if the
 *     request was issued and 'false' if the request can not be serviced
 *     at this time.
 */
bool gmosNalTcpipSocketTxDataWrite (gmosTcpipStackSocket_t* socket);

/**
 * Updates the new write data pointer value after transferring a new
 * block of data.
 * @param socket This is the socket for which the WIZnet socket memory
 *     transmit write pointer is being updated.
 * @return Returns a boolean value which will be set to 'true' if the
 *     request was issued and 'false' if the request can not be serviced
 *     at this time.
 */
bool gmosNalTcpipSocketTxPointerWrite (gmosTcpipStackSocket_t* socket);

#endif // WIZNET_DRIVER_CORE_H
