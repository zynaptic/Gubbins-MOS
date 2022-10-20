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
 * driver functions for the WIZnet W5500 TCP/IP network coprocessor
 * device.
 */

#ifndef WIZNET_DRIVER_TCPIP_H
#define WIZNET_DRIVER_TCPIP_H

#include <stdint.h>
#include "gmos-config.h"
#include "gmos-buffers.h"
#include "gmos-streams.h"
#include "gmos-driver-spi.h"
#include "gmos-tcpip-stack.h"
#include "wiznet-driver-config.h"
#include "wiznet-spi-adaptor.h"

/**
 * Defines the socket state data structure for a single WIZnet W5500
 * socket instance.
 */
typedef struct gmosTcpipStackSocket_t {

    // Link to the associated TCP/IP stack instance.
    gmosDriverTcpip_t* tcpipStack;

    // Specifies the stack notification handler used for this socket.
    gmosTcpipStackNotifyCallback_t notifyHandler;

    // Specifies the stack notification data item used for this socket.
    void* notifyData;

    // Allocate the socket transmit data stream.
    gmosStream_t txStream;

    // Allocate the socket receive data stream.
    gmosStream_t rxStream;

    // Allocate buffer storage for payload data.
    gmosBuffer_t payloadData;

    // Specify the socket ID value.
    uint8_t socketId;

    // Specify the current socket operating state.
    uint8_t socketState;

    // Specify the current set of active interrupt flags.
    uint8_t interruptFlags;

    // Specify the interrupt flag clear requests.
    uint8_t interruptClear;

    // Store context specific state information.
    union {
        struct {
            uint16_t localPort;
        } setup;
        struct {
            uint16_t dataPtr;
            uint16_t limitPtr;
        } active;
    } data;

} gmosTcpipStackSocket_t;

/**
 * Defines the TCP/IP stack specific I/O state data structure for a
 * single WIZnet W5500 TCP/IP network coprocessor device.
 */
typedef struct gmosNalTcpipState_t {

    // Allocate the stream data structure for WIZnet SPI commands.
    gmosStream_t spiCommandStream;

    // Allocate the stream data structure for WIZnet SPI responses.
    gmosStream_t spiResponseStream;

    // Allocate the event data structure used for interrupt events.
    gmosEvent_t interruptEvent;

    // Allocate the SPI protocol worker task data structure.
    gmosTaskState_t spiWorkerTask;

    // Allocate the core worker task data structure.
    gmosTaskState_t coreWorkerTask;

    // Allocate SPI device data structure.
    gmosDriverSpiDevice_t spiDevice;

    // Allocate memory for the current SPI command data.
    wiznetSpiAdaptorCmd_t spiCommand;

    // Allocate memory for the required number of sockets.
    gmosTcpipStackSocket_t socketData [GMOS_CONFIG_TCPIP_MAX_SOCKETS];

    // Specify the timestamp used for PHY connection state polling.
    uint16_t phyPollTimestamp;

    // Specify the current offset for buffer based transfers.
    uint16_t spiBufferOffset;

    // Specify the current WIZnet interface adaptor state.
    uint8_t wiznetAdaptorState;

    // Specify the current WIZnet core processing state.
    uint8_t wiznetCoreState;

    // Specify the WIZnet core processing interrupt and status flags.
    uint8_t wiznetCoreFlags;

    // Specify the socket selection for the core state machine.
    uint8_t wiznetSocketSelect;

    // Store the Ethernet MAC address in network byte order.
    uint8_t ethMacAddr [6];

    // Store the current gateway address in network byte order.
    uint8_t gatewayAddr [4];

    // Store the current subnet mask in network byte order.
    uint8_t subnetMask [4];

    // Store the local interface address in network byte order.
    uint8_t interfaceAddr [4];

} gmosNalTcpipState_t;

/**
 * Defines the TCP/IP stack specific I/O configuration options for a
 * single WIZnet W5500 TCP/IP network coprocessor device.
 */
typedef struct gmosNalTcpipConfig_t {

    // Specify the SPI bus instance to use for communicating with the
    // WIZnet NCP device. It should have been initialised prior to use.
    gmosDriverSpiBus_t* spiInterface;

    // Specify the GPIO pin used as the SPI chip select line.
    uint16_t spiChipSelectPin;

    // Specify the GPIO pin used as the WIZnet NCP reset line.
    uint16_t ncpResetPin;

    // Specify the GPIO pin used as the WIZnet NCP interrupt input.
    uint16_t ncpInterruptPin;

} gmosNalTcpipConfig_t;

#endif // WIZNET_DRIVER_TCPIP_H
