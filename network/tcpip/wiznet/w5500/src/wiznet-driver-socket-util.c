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
 * This file implements common utility functions for use when accessing
 * a WIZnet W5500 TCP/IP offload device.
 */

#include <stdint.h>
#include <stdbool.h>

#include "gmos-buffers.h"
#include "gmos-streams.h"
#include "gmos-driver-tcpip.h"
#include "gmos-tcpip-stack.h"
#include "wiznet-driver-tcpip.h"
#include "wiznet-driver-core.h"
#include "wiznet-spi-adaptor.h"

/*
 * Issues a command for the WIZnet socket controller.
 */
bool gmosNalTcpipSocketIssueCommand (gmosNalTcpipSocket_t* socket,
    wiznetSpiAdaptorSocketCommands_t command)
{
    gmosNalTcpipState_t* nalData = socket->common.tcpipDriver->nalData;
    wiznetSpiAdaptorCmd_t socketCommand;
    uint8_t socketId = socket->socketId;

    // Set up the command to write to the command register at offset
    // address 0x0001.
    socketCommand.address = 0x0001;
    socketCommand.control =
        WIZNET_SPI_ADAPTOR_CTRL_SOCKET_REGS (socketId) |
        WIZNET_SPI_ADAPTOR_CTRL_WRITE_ENABLE |
        WIZNET_SPI_ADAPTOR_CTRL_DISCARD_RESPONSE;
    socketCommand.size = 1;
    socketCommand.data.bytes [0] = (uint8_t) command;

    // Issue the socket configuration request.
    return wiznetSpiAdaptorStream_write (
        &nalData->spiCommandStream, &socketCommand);
}

/*
 * Sets the remote IP address and port for an outgoing TCP connection
 * or UDP datagram.
 */
bool gmosNalTcpipSocketSetRemoteAddr (gmosNalTcpipSocket_t* socket)
{
    gmosNalTcpipState_t* nalData = socket->common.tcpipDriver->nalData;
    uint8_t socketId = socket->socketId;
    uint16_t remoteAddrOffset;
    gmosBuffer_t* payloadData = &(socket->payloadData);
    wiznetSpiAdaptorCmd_t remoteAddrCommand;

    // The settings for the remote address and port registers are
    // stored in the correct order at the end of the current data
    // buffer.
    remoteAddrOffset = gmosBufferGetSize (payloadData) - 6;
    gmosBufferRead (payloadData, remoteAddrOffset,
        remoteAddrCommand.data.bytes, 6);

    // Format the request to set the remote IP address and port
    // registers, starting from register address 0x000C.
    remoteAddrCommand.address = 0x000C;
    remoteAddrCommand.control =
        WIZNET_SPI_ADAPTOR_CTRL_SOCKET_REGS (socketId) |
        WIZNET_SPI_ADAPTOR_CTRL_WRITE_ENABLE |
        WIZNET_SPI_ADAPTOR_CTRL_DISCARD_RESPONSE;
    remoteAddrCommand.size = 6;

    // On issuing the command, trim the address and port information
    // from the end of the payload data buffer.
    if (wiznetSpiAdaptorStream_write (
        &nalData->spiCommandStream, &remoteAddrCommand)) {
        gmosBufferResize (payloadData, remoteAddrOffset);
        return true;
    } else {
        return false;
    }
}

/*
 * Checks the status of the socket receive buffer. The receive state
 * machine will only proceed if the buffer status fields are consistent.
 */
bool gmosNalTcpipSocketRxBufferCheck (
    gmosNalTcpipSocket_t* socket, wiznetSpiAdaptorCmd_t* response,
    uint16_t rxThreshold, bool* sequenceError)
{
    uint8_t socketId = socket->socketId;
    uint8_t expectedControl =
        WIZNET_SPI_ADAPTOR_CTRL_SOCKET_REGS (socketId) |
        WIZNET_SPI_ADAPTOR_CTRL_READ_ENABLE;
    uint16_t bufRxSize;
    uint16_t bufReadPtr;
    uint16_t bufWritePtr;

    // A response sequence error is generated if this is not a valid
    // response message.
    if ((response->address != 0x0026) ||
        (response->control != expectedControl) ||
        (response->size != 6)) {
        *sequenceError = true;
        return false;
    }

    // Extract the receive buffer pointer state.
    *sequenceError = false;
    bufRxSize = ((uint16_t) response->data.bytes [1]) +
        (((uint16_t) response->data.bytes [0]) << 8);
    bufReadPtr = ((uint16_t) response->data.bytes [3]) +
        (((uint16_t) response->data.bytes [2]) << 8);
    bufWritePtr = ((uint16_t) response->data.bytes [5]) +
        (((uint16_t) response->data.bytes [4]) << 8);

    // Check for receive buffer consistency. This also checks that
    // the amount of received data exceeds the specified receive
    // threshold.
    if ((bufRxSize >= rxThreshold) &&
        (bufWritePtr - bufReadPtr == bufRxSize)) {
        socket->data.active.dataPtr = bufReadPtr;
        socket->data.active.limitPtr = bufWritePtr;
        return true;
    }

    // Cancel the receive data interrupt here if there is no longer any
    // data to be transferred.
    if (bufRxSize == 0) {
        socket->interruptClear |= WIZNET_SPI_ADAPTOR_SOCKET_INT_RECV;
    }
    return false;
}

/*
 * Writes the new read data pointer value after reading an inbound TCP
 * data block or UDP packet.
 */
bool gmosNalTcpipSocketRxPointerWrite (gmosNalTcpipSocket_t* socket)
{
    gmosNalTcpipState_t* nalData = socket->common.tcpipDriver->nalData;
    uint8_t socketId = socket->socketId;
    uint16_t endOfDataPtr = socket->data.active.limitPtr;
    wiznetSpiAdaptorCmd_t rxPtrCommand;

    // Format the request to set the receive data read pointer at
    // address 0x0028.
    rxPtrCommand.address = 0x0028;
    rxPtrCommand.control =
        WIZNET_SPI_ADAPTOR_CTRL_SOCKET_REGS (socketId) |
        WIZNET_SPI_ADAPTOR_CTRL_WRITE_ENABLE |
        WIZNET_SPI_ADAPTOR_CTRL_DISCARD_RESPONSE;
    rxPtrCommand.size = 2;

    // Set the updated write pointer value.
    rxPtrCommand.data.bytes [0] = (uint8_t) (endOfDataPtr >> 8);
    rxPtrCommand.data.bytes [1] = (uint8_t) (endOfDataPtr);

    // Issue the write pointer update request.
    return wiznetSpiAdaptorStream_write (
        &nalData->spiCommandStream, &rxPtrCommand);
}

/*
 * Checks the result of a data buffer read operation.
 */
bool gmosNalTcpipSocketRxDataBlockCheck (
    gmosNalTcpipSocket_t* socket, wiznetSpiAdaptorCmd_t* response,
    bool* sequenceError)
{
    uint8_t socketId = socket->socketId;
    uint8_t expectedControl =
        WIZNET_SPI_ADAPTOR_CTRL_SOCKET_RX_BUF (socketId) |
        WIZNET_SPI_ADAPTOR_CTRL_READ_ENABLE;
    gmosBuffer_t* rxDataBuffer = &(response->data.buffer);
    gmosBuffer_t* payloadData = &(socket->payloadData);

    // A response sequence error is generated if this is not a valid
    // response message.
    if ((response->address != socket->data.active.dataPtr) ||
        (response->control != expectedControl) ||
        (response->size != 0)) {
        *sequenceError = true;
        return false;
    }

    // Copy the read buffer contents to the local payload buffer.
    *sequenceError = false;
    gmosBufferMove (rxDataBuffer, payloadData);
    return true;
}

/*
 * Writes the contents of the local buffer to the WIZnet socket memory,
 * starting from the current socket memory address pointer.
 */
bool gmosNalTcpipSocketTxDataWrite (gmosNalTcpipSocket_t* socket)
{
    gmosNalTcpipState_t* nalData = socket->common.tcpipDriver->nalData;
    uint8_t socketId = socket->socketId;
    wiznetSpiAdaptorCmd_t txDataCommand;
    gmosBuffer_t* txDataBuffer = &txDataCommand.data.buffer;
    gmosBuffer_t* payloadData = &(socket->payloadData);
    uint16_t payloadSize = gmosBufferGetSize (payloadData);

    // Format the request to send the transmit data, starting from the
    // current address pointer.
    txDataCommand.address = socket->data.active.dataPtr;
    txDataCommand.control =
        WIZNET_SPI_ADAPTOR_CTRL_SOCKET_TX_BUF (socketId) |
        WIZNET_SPI_ADAPTOR_CTRL_WRITE_ENABLE |
        WIZNET_SPI_ADAPTOR_CTRL_DISCARD_RESPONSE;
    txDataCommand.size = 0;

    // Move the payload data to the transmit data buffer.
    gmosBufferInit (txDataBuffer);
    gmosBufferMove (payloadData, txDataBuffer);

    // On issuing the command, update the local data pointer to
    // correspond to the end of the written data.
    if (wiznetSpiAdaptorStream_write (
        &nalData->spiCommandStream, &txDataCommand)) {
        socket->data.active.dataPtr += payloadSize;
        return true;
    }

    // Revert the payload data to the intermediate buffer on failure.
    else {
        gmosBufferMove (txDataBuffer, payloadData);
        return false;
    }
}

/*
 * Updates the new write data pointer value after transferring a new
 * block of data.
 */
bool gmosNalTcpipSocketTxPointerWrite (gmosNalTcpipSocket_t* socket)
{
    gmosNalTcpipState_t* nalData = socket->common.tcpipDriver->nalData;
    uint8_t socketId = socket->socketId;
    uint16_t endOfDataPtr = socket->data.active.dataPtr;
    wiznetSpiAdaptorCmd_t txPtrCommand;

    // Format the request to set the transmit data write pointer at
    // address 0x0024.
    txPtrCommand.address = 0x0024;
    txPtrCommand.control =
        WIZNET_SPI_ADAPTOR_CTRL_SOCKET_REGS (socketId) |
        WIZNET_SPI_ADAPTOR_CTRL_WRITE_ENABLE |
        WIZNET_SPI_ADAPTOR_CTRL_DISCARD_RESPONSE;
    txPtrCommand.size = 2;

    // Set the updated write pointer value.
    txPtrCommand.data.bytes [0] = (uint8_t) (endOfDataPtr >> 8);
    txPtrCommand.data.bytes [1] = (uint8_t) (endOfDataPtr);

    // Issue the write pointer update request.
    return wiznetSpiAdaptorStream_write (
        &nalData->spiCommandStream, &txPtrCommand);
}
