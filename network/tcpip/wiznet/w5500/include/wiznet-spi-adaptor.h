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
 * This header defines the vendor specific data structures and device
 * driver functions for accessing a WIZnet W5500 TCP/IP offload device
 * over the SPI interface.
 */

#ifndef WIZNET_SPI_ADAPTOR_H
#define WIZNET_SPI_ADAPTOR_H

#include <stdint.h>
#include <stdbool.h>
#include "gmos-buffers.h"
#include "gmos-driver-tcpip.h"
#include "wiznet-driver-config.h"

/**
 * This enumeration defines the state space used by the WIZnet SPI
 * adaptor state machine.
 */
typedef enum {
    WIZNET_SPI_ADAPTOR_STATE_INIT,
    WIZNET_SPI_ADAPTOR_STATE_RESET,
    WIZNET_SPI_ADAPTOR_STATE_IDLE,
    WIZNET_SPI_ADAPTOR_STATE_SELECT,
    WIZNET_SPI_ADAPTOR_STATE_SEND_HEADER,
    WIZNET_SPI_ADAPTOR_STATE_TRANSFER_BYTES,
    WIZNET_SPI_ADAPTOR_STATE_TRANSFER_BUFFER,
    WIZNET_SPI_ADAPTOR_STATE_TRANSFER_WAIT,
    WIZNET_SPI_ADAPTOR_STATE_RELEASE,
    WIZNET_SPI_ADAPTOR_STATE_RESPOND,
    WIZNET_SPI_ADAPTOR_STATE_ERROR
} wiznetSpiAdaptorState_t;

/**
 * This enumeration defines the set of socket command values that may
 * be written to the socket command registers.
 */
typedef enum {
    WIZNET_SPI_ADAPTOR_SOCKET_COMMAND_OPEN       = 0x01,
    WIZNET_SPI_ADAPTOR_SOCKET_COMMAND_CONNECT    = 0x04,
    WIZNET_SPI_ADAPTOR_SOCKET_COMMAND_DISCONNECT = 0x08,
    WIZNET_SPI_ADAPTOR_SOCKET_COMMAND_CLOSE      = 0x10,
    WIZNET_SPI_ADAPTOR_SOCKET_COMMAND_SEND       = 0x20,
    WIZNET_SPI_ADAPTOR_SOCKET_COMMAND_RECV       = 0x40
} wiznetSpiAdaptorSocketCommands_t;

/**
 * This enumeration defines the set of socket status values that may
 * be read from the socket status registers.
 */
typedef enum {
    WIZNET_SPI_ADAPTOR_SOCKET_STATUS_CLOSED   = 0x00,
    WIZNET_SPI_ADAPTOR_SOCKET_STATUS_INIT_TCP = 0x13,
    WIZNET_SPI_ADAPTOR_SOCKET_STATUS_UDP      = 0x22
} wiznetSpiAdaptorSocketStatus_t;

/**
 * This enumeration specifies the socket interrupt bit positions and
 * control flags.
 */
typedef enum {
    WIZNET_SPI_ADAPTOR_SOCKET_INT_CON        = 0x01,
    WIZNET_SPI_ADAPTOR_SOCKET_INT_DISCON     = 0x02,
    WIZNET_SPI_ADAPTOR_SOCKET_INT_RECV       = 0x04,
    WIZNET_SPI_ADAPTOR_SOCKET_INT_TIMEOUT    = 0x08,
    WIZNET_SPI_ADAPTOR_SOCKET_INT_SENDOK     = 0x10,
    WIZNET_SPI_ADAPTOR_SOCKET_FLAG_CLOSE_REQ = 0x80
} wiznetSpiAdaptorSocketInts_t;

/**
 * Defines the command and response data structure used for initiating
 * new SPI transactions and returning SPI responses.
 */
typedef struct wiznetSpiAdaptorCmd_t {

    // Specify the address to use for the transfer.
    uint16_t address;

    // Specify the control byte to be used for the transfer.
    uint8_t control;

    // Specify the data transfer size. A non-zero value indicates that
    // data transfer is via the short byte array. A value of zero
    // indicates that data transfer is via the buffer, with the transfer
    // size being inferred from the buffer size.
    uint8_t size;

    // The command data may be a short byte array or a data buffer.
    union {
        uint8_t bytes [8];
        gmosBuffer_t buffer;
    } data;

} wiznetSpiAdaptorCmd_t;

// Specify the expected version number for the WIZnet device.
#define WIZNET_SPI_ADAPTOR_DEVICE_VERSION 0x04

// This is a bit mask that is used to force variable length data mode
// operation.
#define WIZNET_SPI_ADAPTOR_CTRL_DATA_MODE_MASK 0xFC

// This is a locally significant control flag which when set disables
// response generation for the SPI transaction. This will usually be
// used for 'fire and forget' write transactions.
#define WIZNET_SPI_ADAPTOR_CTRL_DISCARD_RESPONSE 0x01

// Provides a macro for selecting SPI read operations.
#define WIZNET_SPI_ADAPTOR_CTRL_READ_ENABLE 0x00

// Provides a macro for selecting SPI write operations.
#define WIZNET_SPI_ADAPTOR_CTRL_WRITE_ENABLE 0x04

// Provides a macro for selecting the common register block.
#define WIZNET_SPI_ADAPTOR_CTRL_COMMON_REGS 0x00

// Provides a macro for selecting the socket register block.
#define WIZNET_SPI_ADAPTOR_CTRL_SOCKET_REGS(_socket_) \
    ((_socket_ << 5) + 0x08)

// Provides a macro for selecting the socket transmit buffer.
#define WIZNET_SPI_ADAPTOR_CTRL_SOCKET_TX_BUF(_socket_) \
    ((_socket_ << 5) + 0x10)

// Provides a macro for selecting the socket receive buffer.
#define WIZNET_SPI_ADAPTOR_CTRL_SOCKET_RX_BUF(_socket_) \
    ((_socket_ << 5) + 0x18)

// Specify the interrupt event flags used by the WIZnet SPI adaptor.
#define WIZNET_INTERRUPT_FLAG_NCP_REQUEST 0x01

// Specify the interrupt interval used by the WIZnet adaptor, expressed
// as an integer number of microseconds.
#define WIZNET_INTERRUPT_LOW_LEVEL_INTERVAL 250

// Specify the PHY link state polling interval. This is the interval
// at which the PHY status register will be read in order to detect
// a physical layer disconnection event, expressed as an integer number
// of milliseconds.
#define WIZNET_PHY_LINK_POLLING_INTERVAL 1000

// Specify the maximum SPI interface clock rate to use. This is an
// integer multiple of 1kHz.
#define WIZNET_SPI_CLOCK_FREQUENCY 32000

// Specify the SPI interface clock mode to use.
#define WIZNET_SPI_CLOCK_MODE 0

// Specify the size of the SPI adaptor streams as an integer number
// of SPI commands.
#define WIZNET_SPI_ADAPTOR_STREAM_SIZE (2 * GMOS_CONFIG_TCPIP_MAX_SOCKETS)

// Define the SPI command streams to use the command data type.
GMOS_STREAM_DEFINITION (wiznetSpiAdaptorStream, wiznetSpiAdaptorCmd_t)

/**
 * Initialise the WIZnet W5500 SPI adaptor task on startup.
 * @param tcpipStack This is the TCP/IP stack data structure that
 *     represents the WIZnet W5500 SPI device being initialised.
 * @return Returns a boolean value which will be set to 'true' if the
 *     WIZnet W5500 SPI interface was successfully initialised and
 *     'false' otherwise.
 */
bool gmosNalTcpipWiznetSpiInit (gmosDriverTcpip_t* tcpipStack);

#endif // WIZNET_SPI_ADAPTOR_H
