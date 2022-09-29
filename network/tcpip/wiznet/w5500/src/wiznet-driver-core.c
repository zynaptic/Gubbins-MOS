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
 * This file implements the core driver functionality for accessing a
 * WIZnet W5500 TCP/IP offload device.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-driver-tcpip.h"
#include "gmos-tcpip-stack.h"
#include "wiznet-driver-config.h"
#include "wiznet-driver-tcpip.h"
#include "wiznet-driver-core.h"
#include "wiznet-spi-adaptor.h"

/*
 * Set up the socket buffer sizes, based on the selected socket number.
 */
#if (GMOS_CONFIG_TCPIP_MAX_SOCKETS == 8)
static const uint8_t socketBufSizes [] = { 2, 2, 2, 2, 2, 2, 2, 2 };
#elif (GMOS_CONFIG_TCPIP_MAX_SOCKETS == 7)
static const uint8_t socketBufSizes [] = { 4, 2, 2, 2, 2, 2, 2, 0 };
#elif (GMOS_CONFIG_TCPIP_MAX_SOCKETS == 6)
static const uint8_t socketBufSizes [] = { 4, 4, 2, 2, 2, 2, 0, 0 };
#elif (GMOS_CONFIG_TCPIP_MAX_SOCKETS == 5)
static const uint8_t socketBufSizes [] = { 4, 4, 4, 2, 2, 0, 0, 0 };
#elif (GMOS_CONFIG_TCPIP_MAX_SOCKETS == 4)
static const uint8_t socketBufSizes [] = { 4, 4, 4, 4, 0, 0, 0, 0 };
#elif (GMOS_CONFIG_TCPIP_MAX_SOCKETS == 3)
static const uint8_t socketBufSizes [] = { 8, 4, 4, 0, 0, 0, 0, 0 };
#elif (GMOS_CONFIG_TCPIP_MAX_SOCKETS == 2)
static const uint8_t socketBufSizes [] = { 8, 8, 0, 0, 0, 0, 0, 0 };
#elif (GMOS_CONFIG_TCPIP_MAX_SOCKETS == 1)
static const uint8_t socketBufSizes [] = { 16, 0, 0, 0, 0, 0, 0, 0 };
#else
#error "Invalid setting for TCP/IP maximum sockets."
#endif

/*
 * Read the attached device version number.
 */
static inline bool wiznetCoreCommonVerRead (
    gmosDriverTcpip_t* tcpipStack)
{
    gmosNalTcpipState_t* nalData = tcpipStack->nalData;
    wiznetSpiAdaptorCmd_t verCommand;

    // Set up the command to read from the 8-bit version register at
    // address 0x0039.
    verCommand.address = 0x0039;
    verCommand.control =
        WIZNET_SPI_ADAPTOR_CTRL_COMMON_REGS |
        WIZNET_SPI_ADAPTOR_CTRL_READ_ENABLE;
    verCommand.size = 1;

    // Issue the version readback request.
    return wiznetSpiAdaptorStream_write (
        &nalData->spiCommandStream, &verCommand);
}

/*
 * Check the attached device version number.
 */
static inline bool wiznetCoreCommonVerCheck (
    gmosDriverTcpip_t* tcpipStack, bool* statusOk)
{
    gmosNalTcpipState_t* nalData = tcpipStack->nalData;
    wiznetSpiAdaptorCmd_t verResponse;

    // Attempt to read back the next SPI transaction response.
    *statusOk = false;
    if (!wiznetSpiAdaptorStream_read (
        &nalData->spiResponseStream, &verResponse)) {
        return false;
    }

    // Check the payload for the expected device version.
    if ((verResponse.size == 1) &&
        (verResponse.data.bytes [0] ==
        WIZNET_SPI_ADAPTOR_DEVICE_VERSION)) {
        *statusOk = true;
    }
    GMOS_LOG_FMT (LOG_VERBOSE,
        "WIZnet TCP/IP : Device version check status : %d", *statusOk);
    return true;
}

/*
 * Set the common configuration registers on startup and when assigning
 * new local network parameters.
 */
static bool wiznetCoreCommonCfgSet (
    gmosDriverTcpip_t* tcpipStack)
{
    gmosNalTcpipState_t* nalData = tcpipStack->nalData;
    wiznetSpiAdaptorCmd_t cfgCommand;
    gmosBuffer_t* cfgBuffer = &cfgCommand.data.buffer;

    // Set up the configuration command to write to the common register
    // block starting at address 1.
    cfgCommand.address = 0x0001;
    cfgCommand.control =
        WIZNET_SPI_ADAPTOR_CTRL_COMMON_REGS |
        WIZNET_SPI_ADAPTOR_CTRL_WRITE_ENABLE |
        WIZNET_SPI_ADAPTOR_CTRL_DISCARD_RESPONSE;
    cfgCommand.size = 0;

    // Initialise the command buffer for a block data transfer.
    gmosBufferInit (cfgBuffer);

    // Set the gateway address registers.
    if ((!gmosBufferAppend (cfgBuffer, nalData->gatewayAddr, 4)) ||
        (!gmosBufferAppend (cfgBuffer, nalData->subnetMask, 4)) ||
        (!gmosBufferAppend (cfgBuffer, nalData->ethMacAddr, 6)) ||
        (!gmosBufferAppend (cfgBuffer, nalData->interfaceAddr, 4))) {
        goto fail;
    }

    // Write the configuration options as a single SPI transaction. All
    // remaining options are left as their default values.
    if (!wiznetSpiAdaptorStream_write (
        &nalData->spiCommandStream, &cfgCommand)) {
        goto fail;
    }
    return true;

    // Clean up on failure.
fail:
   gmosBufferReset (cfgBuffer, 0);
   return false;
}

/*
 * Read back the common configuration registers on startup.
 */
static inline bool wiznetCoreCommonCfgRead (
    gmosDriverTcpip_t* tcpipStack)
{
    gmosNalTcpipState_t* nalData = tcpipStack->nalData;
    wiznetSpiAdaptorCmd_t cfgCommand;
    gmosBuffer_t* cfgBuffer = &cfgCommand.data.buffer;

    // Set up the configuration command to read from the common register
    // block starting at address 1.
    cfgCommand.address = 0x0001;
    cfgCommand.control =
        WIZNET_SPI_ADAPTOR_CTRL_COMMON_REGS |
        WIZNET_SPI_ADAPTOR_CTRL_READ_ENABLE;
    cfgCommand.size = 0;

    // Initialise the command buffer for a block data transfer.
    gmosBufferInit (cfgBuffer);
    if (!gmosBufferExtend (cfgBuffer, 18)) {
        goto fail;
    }

    // Issue the configuration readback as a single SPI transaction.
    if (!wiznetSpiAdaptorStream_write (
        &nalData->spiCommandStream, &cfgCommand)) {
        goto fail;
    }
    return true;

    // Clean up on failure.
fail:
   gmosBufferReset (cfgBuffer, 0);
   return false;
}

/*
 * Check the results of the configuration setup process.
 */
static inline bool wiznetCoreCommonCfgCheck (
    gmosDriverTcpip_t* tcpipStack, bool* statusOk)
{
    gmosNalTcpipState_t* nalData = tcpipStack->nalData;
    wiznetSpiAdaptorCmd_t cfgResponse;
    gmosBuffer_t* cfgBuffer = &cfgResponse.data.buffer;
    uint8_t cfgData [18];

    // Attempt to read back the next SPI transaction response.
    *statusOk = false;
    if (!wiznetSpiAdaptorStream_read (
        &nalData->spiResponseStream, &cfgResponse)) {
        return false;
    }

    // Extract the configuration data from the buffer and compare the
    // contents against the expected values.
    if ((cfgResponse.size == 0) &&
        (gmosBufferRead (cfgBuffer, 0, cfgData, sizeof (cfgData))) &&
        (memcmp (cfgData + 0, nalData->gatewayAddr, 4) == 0) &&
        (memcmp (cfgData + 4, nalData->subnetMask, 4) == 0) &&
        (memcmp (cfgData + 8, nalData->ethMacAddr, 6) == 0) &&
        (memcmp (cfgData + 14, nalData->interfaceAddr, 4) == 0)) {
        *statusOk = true;
    }
    gmosBufferReset (cfgBuffer, 0);
    GMOS_LOG_FMT (LOG_VERBOSE,
        "WIZnet TCP/IP : Common configuration status : %d", *statusOk);
    return true;
}

/*
 * Set the socket configuration options. This sets the transmit and
 * receive buffer sizes for each socket in turn.
 */
static inline bool wiznetCoreSocketCfgSet (
    gmosDriverTcpip_t* tcpipStack)
{
    gmosNalTcpipState_t* nalData = tcpipStack->nalData;
    wiznetSpiAdaptorCmd_t cfgCommand;
    uint8_t socketId = nalData->wiznetSocketSelect;
    uint8_t socketBufSize = socketBufSizes [socketId];

    // Set up the command to write to the two 8-bit buffer size
    // registers at addresses 0x001E and 0x001F.
    cfgCommand.address = 0x001E;
    cfgCommand.control =
        WIZNET_SPI_ADAPTOR_CTRL_SOCKET_REGS (socketId) |
        WIZNET_SPI_ADAPTOR_CTRL_WRITE_ENABLE |
        WIZNET_SPI_ADAPTOR_CTRL_DISCARD_RESPONSE;
    cfgCommand.size = 2;
    cfgCommand.data.bytes [0] = socketBufSize;
    cfgCommand.data.bytes [1] = socketBufSize;

    // Issue the socket configuration request.
    return wiznetSpiAdaptorStream_write (
        &nalData->spiCommandStream, &cfgCommand);
}

/*
 * Read back the socket configuration options. This includes the
 * transmit and receive buffer sizes for each socket and the initial
 * free space settings.
 */
static inline bool wiznetCoreSocketCfgRead (
    gmosDriverTcpip_t* tcpipStack)
{
    gmosNalTcpipState_t* nalData = tcpipStack->nalData;
    wiznetSpiAdaptorCmd_t cfgCommand;
    gmosBuffer_t* cfgBuffer = &cfgCommand.data.buffer;
    uint8_t socketId = nalData->wiznetSocketSelect;

    // Set up the command to read the 14 transmit and receive buffer
    // state registers starting from address 0x001E.
    cfgCommand.address = 0x001E;
    cfgCommand.control =
        WIZNET_SPI_ADAPTOR_CTRL_SOCKET_REGS (socketId) |
        WIZNET_SPI_ADAPTOR_CTRL_READ_ENABLE;
    cfgCommand.size = 0;

    // Initialise the command buffer for a block data transfer.
    gmosBufferInit (cfgBuffer);
    if (!gmosBufferExtend (cfgBuffer, 14)) {
        goto fail;
    }

    // Issue the socket configuration request.
    if (!wiznetSpiAdaptorStream_write (
        &nalData->spiCommandStream, &cfgCommand)) {
        goto fail;
    }
    return true;

    // Clean up on failure.
fail:
   gmosBufferReset (cfgBuffer, 0);
   return false;
}

/*
 * Check the initial socket configuration state.
 */
static inline bool wiznetCoreSocketCfgCheck (
    gmosDriverTcpip_t* tcpipStack, bool* statusOk)
{
    gmosNalTcpipState_t* nalData = tcpipStack->nalData;
    wiznetSpiAdaptorCmd_t cfgResponse;
    gmosBuffer_t* cfgBuffer = &cfgResponse.data.buffer;
    uint8_t socketId = nalData->wiznetSocketSelect;
    uint8_t socketBufSize = socketBufSizes [socketId];
    uint16_t socketBufBytes = 1024 * (uint16_t) socketBufSize;
    uint8_t cfgData [14];
    uint8_t cfgMatch [14] = {
        socketBufSize, socketBufSize, 0xFF & (socketBufBytes >> 8),
        0xFF & socketBufBytes, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    // Attempt to read back the next SPI transaction response.
    *statusOk = false;
    if (!wiznetSpiAdaptorStream_read (
        &nalData->spiResponseStream, &cfgResponse)) {
        return false;
    }

    // Extract the configuration data from the buffer and compare the
    // contents against the expected values.
    if ((cfgResponse.size == 0) &&
        (gmosBufferRead (cfgBuffer, 0, cfgData, sizeof (cfgData))) &&
        (memcmp (cfgData, cfgMatch, sizeof (cfgData)) == 0)) {
        *statusOk = true;
    }
    gmosBufferReset (cfgBuffer, 0);
    GMOS_LOG_FMT (LOG_VERBOSE,
        "WIZnet TCP/IP : Socket %d buffer size %dK status : %d",
        socketId, socketBufSize, *statusOk);
    return true;
}

/*
 * Sets the common register block interrupt enable options.
 */
static inline bool wiznetCoreCommonCfgIntEnable (
    gmosDriverTcpip_t* tcpipStack)
{
    gmosNalTcpipState_t* nalData = tcpipStack->nalData;
    wiznetSpiAdaptorCmd_t cfgCommand;
    uint16_t cfgIntTimerReg;
    uint8_t commonIntMask;
    uint8_t socketIntMask;

    // Calculate the interrupt interval low level timer value, which
    // allows the level based W5500 interrupts to be treated as edge
    // triggered GPIO interrupts.
    cfgIntTimerReg = (150 * WIZNET_INTERRUPT_LOW_LEVEL_INTERVAL / 4) - 1;

    // The address conflict and destination unreachable interrupts are
    // not currently used.
    commonIntMask = 0;

    // Generate the socket interrupt mask based on the number of
    // supported sockets.
    socketIntMask = (1 << GMOS_CONFIG_TCPIP_MAX_SOCKETS) - 1;

    // Set up the command to write to the six 8-bit common interrupt
    // registers starting at address 0x0013.
    cfgCommand.address = 0x0013;
    cfgCommand.control =
        WIZNET_SPI_ADAPTOR_CTRL_COMMON_REGS |
        WIZNET_SPI_ADAPTOR_CTRL_WRITE_ENABLE |
        WIZNET_SPI_ADAPTOR_CTRL_DISCARD_RESPONSE;
    cfgCommand.size = 6;
    cfgCommand.data.bytes [0] = (uint8_t) (cfgIntTimerReg >> 8);
    cfgCommand.data.bytes [1] = (uint8_t) cfgIntTimerReg;
    cfgCommand.data.bytes [2] = 0;
    cfgCommand.data.bytes [3] = commonIntMask;
    cfgCommand.data.bytes [4] = 0;
    cfgCommand.data.bytes [5] = socketIntMask;

    // Issue the socket configuration request.
    return wiznetSpiAdaptorStream_write (
        &nalData->spiCommandStream, &cfgCommand);
}

/*
 * Read back the common interrupt configuration settings.
 */
static inline bool wiznetCoreCommonCfgIntRead (
    gmosDriverTcpip_t* tcpipStack)
{
    gmosNalTcpipState_t* nalData = tcpipStack->nalData;
    wiznetSpiAdaptorCmd_t cfgCommand;

    // Set up the command to read from the interrupt register block at
    // address 0x0013.
    cfgCommand.address = 0x0013;
    cfgCommand.control =
        WIZNET_SPI_ADAPTOR_CTRL_COMMON_REGS |
        WIZNET_SPI_ADAPTOR_CTRL_READ_ENABLE;
    cfgCommand.size = 6;

    // Issue the interrupt registers readback request.
    return wiznetSpiAdaptorStream_write (
        &nalData->spiCommandStream, &cfgCommand);
}

/*
 * Check the initial common interrupt state.
 */
static inline bool wiznetCoreCommonCfgIntCheck (
    gmosDriverTcpip_t* tcpipStack, bool* statusOk)
{
    const gmosNalTcpipConfig_t* nalConfig = tcpipStack->nalConfig;
    gmosNalTcpipState_t* nalData = tcpipStack->nalData;
    wiznetSpiAdaptorCmd_t cfgResponse;
    uint16_t cfgIntTimerReg;
    uint8_t commonIntMask;
    uint8_t socketIntMask;

    // Attempt to read back the next SPI transaction response.
    *statusOk = false;
    if (!wiznetSpiAdaptorStream_read (
        &nalData->spiResponseStream, &cfgResponse)) {
        return false;
    }

    // Calculate the interrupt interval low level timer value, which
    // allows the level based W5500 interrupts to be treated as edge
    // triggered GPIO interrupts.
    cfgIntTimerReg = (150 * WIZNET_INTERRUPT_LOW_LEVEL_INTERVAL / 4) - 1;

    // The address conflict and destination unreachable interrupts are
    // not currently used.
    commonIntMask = 0;

    // Generate the socket interrupt mask based on the number of
    // supported sockets.
    socketIntMask = (1 << GMOS_CONFIG_TCPIP_MAX_SOCKETS) - 1;

    // Compare the response against the expected values.
    if ((cfgResponse.size == 6) &&
        (cfgResponse.data.bytes [0] == (uint8_t) (cfgIntTimerReg >> 8)) &&
        (cfgResponse.data.bytes [1] == (uint8_t) cfgIntTimerReg) &&
        (cfgResponse.data.bytes [2] == 0) &&
        (cfgResponse.data.bytes [3] == commonIntMask) &&
        (cfgResponse.data.bytes [4] == 0) &&
        (cfgResponse.data.bytes [5] == socketIntMask)) {
        *statusOk = true;
    }

    // Enable GPIO interrupt input on falling edge.
    if (*statusOk) {
        gmosDriverGpioInterruptEnable (
            nalConfig->ncpInterruptPin, false, true);
    }
    GMOS_LOG_FMT (LOG_VERBOSE,
        "WIZnet TCP/IP : Common interrupt enable status : %d", *statusOk);
    return true;
}

/*
 * Read the Ethernet PHY status register.
 */
static inline bool wiznetCoreStartupPhyRead (
    gmosDriverTcpip_t* tcpipStack)
{
    gmosNalTcpipState_t* nalData = tcpipStack->nalData;
    wiznetSpiAdaptorCmd_t cfgCommand;

    // Set up the command to read from the 8-bit Ethernet PHY status
    // register at address 0x002E.
    cfgCommand.address = 0x002E;
    cfgCommand.control =
        WIZNET_SPI_ADAPTOR_CTRL_COMMON_REGS |
        WIZNET_SPI_ADAPTOR_CTRL_READ_ENABLE;
    cfgCommand.size = 1;

    // Issue the version readback request.
    return wiznetSpiAdaptorStream_write (
        &nalData->spiCommandStream, &cfgCommand);
}

/*
 * Check whether the Ethernet PHY link is connected.
 */
static inline bool wiznetCoreStartupPhyCheck (
    gmosDriverTcpip_t* tcpipStack, bool* statusOk)
{
    gmosNalTcpipState_t* nalData = tcpipStack->nalData;
    wiznetSpiAdaptorCmd_t cfgResponse;
    uint8_t phyStatus;

    // Attempt to read back the next SPI transaction response.
    *statusOk = false;
    if (!wiznetSpiAdaptorStream_read (
        &nalData->spiResponseStream, &cfgResponse)) {
        return false;
    }

    // Check the payload for the PHY link established bit.
    if (cfgResponse.size == 1) {
        phyStatus = cfgResponse.data.bytes [0];
        if ((phyStatus & 0x01) != 0) {
            *statusOk = true;
            GMOS_LOG_FMT (LOG_INFO,
                "WIZnet TCP/IP : PHY link established (%d Mbps, %s Duplex).",
                ((phyStatus & 0x02) == 0) ? 10 : 100,
                ((phyStatus & 0x04) == 0) ? "Half" : "Full");
        }
    }
    return true;
}

/*
 * Read back the common interrupt status registers.
 */
static inline bool wiznetCoreCommonIntRead (
    gmosDriverTcpip_t* tcpipStack)
{
    gmosNalTcpipState_t* nalData = tcpipStack->nalData;
    wiznetSpiAdaptorCmd_t intReadCommand;

    // Set up the command to read from the common interrupt status
    // registers at address 0x0015.
    intReadCommand.address = 0x0015;
    intReadCommand.control =
        WIZNET_SPI_ADAPTOR_CTRL_COMMON_REGS |
        WIZNET_SPI_ADAPTOR_CTRL_READ_ENABLE;
    intReadCommand.size = 4;

    // Issue the interrupt registers readback request.
    return wiznetSpiAdaptorStream_write (
        &nalData->spiCommandStream, &intReadCommand);
}

/*
 * Read back the socket specific interrupt status registers.
 */
static inline bool wiznetCoreSocketIntRead (
    gmosDriverTcpip_t* tcpipStack, uint8_t socketId)
{
    gmosNalTcpipState_t* nalData = tcpipStack->nalData;
    wiznetSpiAdaptorCmd_t intReadCommand;

    // Set up the command to read from the interrupt and status
    // registers at address 0x0002.
    intReadCommand.address = 0x0002;
    intReadCommand.control =
        WIZNET_SPI_ADAPTOR_CTRL_SOCKET_REGS (socketId) |
        WIZNET_SPI_ADAPTOR_CTRL_READ_ENABLE;
    intReadCommand.size = 2;

    // Issue the interrupt registers readback request.
    return wiznetSpiAdaptorStream_write (
        &nalData->spiCommandStream, &intReadCommand);
}

/*
 * Process SPI response messages for the common register block.
 */
static inline void wiznetCoreProcessSpiResponses (
    gmosDriverTcpip_t* tcpipStack, wiznetSpiAdaptorCmd_t* spiResponse)
{
    gmosNalTcpipState_t* nalData = tcpipStack->nalData;
    uint8_t socketSelectMask = (1 << GMOS_CONFIG_TCPIP_MAX_SOCKETS) - 1;

    // Detect common interrupt notifications. These are 4-byte reads
    // from address 0x0015 that are generated automatically by the SPI
    // interface module whenever an interrupt is detected. Only the
    // socket specific interrupts are currently processed. These are
    // stored for subsequent processing.
    if ((spiResponse->address == 0x0015) &&
        (spiResponse->size == 4)) {
        nalData->wiznetSocketSelect |=
            socketSelectMask & spiResponse->data.bytes [2];
    }
}

/*
 * Dispatch SPI reponse messages to the appropriate message handlers.
 */
static inline gmosTaskStatus_t wiznetCoreDispatchSpiResponses (
    gmosDriverTcpip_t* tcpipStack)
{
    gmosNalTcpipState_t* nalData = tcpipStack->nalData;
    wiznetSpiAdaptorCmd_t spiResponse;
    gmosTaskStatus_t taskStatus = GMOS_TASK_SUSPEND;
    uint8_t socketId;

    // Process any outstanding SPI response messages.
    while (wiznetSpiAdaptorStream_read (
        &nalData->spiResponseStream, &spiResponse)) {

        // Process SPI responses for the common register block. This
        // may result in change of core state machine state.
        if ((spiResponse.control & 0xF8) == 0) {
            wiznetCoreProcessSpiResponses (tcpipStack, &spiResponse);
        }

        // Forward remaining responses to the appropriate socket
        // response handler.
        else {
            socketId = spiResponse.control >> 5;
            if (socketId < GMOS_CONFIG_TCPIP_MAX_SOCKETS) {
                gmosNalTcpipSocketProcessResponse (
                    &(nalData->socketData [socketId]), &spiResponse);
            }
        }

        // Release any buffer resources on completion.
        if (spiResponse.size == 0) {
            gmosBufferReset (&(spiResponse.data.buffer), 0);
        }

        // Always schedule the core task for immediate execution after
        // processing a SPI response.
        taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    }
    return taskStatus;
}

/*
 * Implement core processing once the WIZnet device has been set up and
 * is ready for use.
 */
static inline gmosTaskStatus_t wiznetCoreRunning (
    gmosDriverTcpip_t* tcpipStack, uint8_t* nextState)
{
    gmosNalTcpipState_t* nalData = tcpipStack->nalData;
    gmosTaskStatus_t intTaskStatus;
    gmosTaskStatus_t tickTaskStatus;
    gmosTaskStatus_t taskStatus;
    uint8_t socketSelect;
    uint8_t socketSelectMask;
    uint8_t i;

    // Process any outstanding SPI responses.
    taskStatus = wiznetCoreDispatchSpiResponses (tcpipStack);

    // Issue requests for socket interrupt status registers if required.
    socketSelect = nalData->wiznetSocketSelect;
    intTaskStatus = GMOS_TASK_SUSPEND;
    if (socketSelect != 0) {
        *nextState = WIZNET_CORE_STATE_RUNNING_INT_ACTIVE;
        for (i = 0; i < GMOS_CONFIG_TCPIP_MAX_SOCKETS; i++) {
            socketSelectMask = 1 << i;

            // Attempt to send the socket specific interrupt read
            // request. Defer the request if the command stream is full.
            if ((socketSelectMask & socketSelect) != 0) {
                if (wiznetCoreSocketIntRead (tcpipStack, i)) {
                    socketSelect &= ~socketSelectMask;
                }
                break;
            }
        }
        nalData->wiznetSocketSelect = socketSelect;

        // Reschedule immediately if more socket interrupt registers
        // need to be read. Otherwise insert an idle period before
        // polling the main interrupt register again.
        if (socketSelect != 0) {
            intTaskStatus = GMOS_TASK_RUN_IMMEDIATE;
        } else {
            intTaskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (5000));
        }
    }

    // Issue a new request for the main interrupt register while
    // interrupt polling is active.
    else if (nalData->wiznetCoreState ==
        WIZNET_CORE_STATE_RUNNING_INT_ACTIVE) {
        if (wiznetCoreCommonIntRead (tcpipStack)) {
            *nextState = WIZNET_CORE_STATE_RUNNING_INT_IDLE;
        } else {
            intTaskStatus = GMOS_TASK_RUN_IMMEDIATE;
        }
    }
    taskStatus = gmosSchedulerPrioritise (taskStatus, intTaskStatus);

    // Run each socket state machine in turn.
    for (i = 0; i < GMOS_CONFIG_TCPIP_MAX_SOCKETS; i++) {
        tickTaskStatus = gmosNalTcpipSocketProcessTick (
            &(nalData->socketData [i]));
        taskStatus = gmosSchedulerPrioritise (taskStatus, tickTaskStatus);
    }
    return taskStatus;
}

/*
 * Implement the main task loop for the WIZnet core protocol processing.
 */
static gmosTaskStatus_t wiznetCoreWorkerTaskFn (void* taskData)
{
    gmosDriverTcpip_t* tcpipStack = (gmosDriverTcpip_t*) taskData;
    gmosNalTcpipState_t* nalData = tcpipStack->nalData;
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    uint8_t nextState = nalData->wiznetCoreState;
    bool statusOk;

    // Implement the WIZnet core processing state machine.
    switch (nalData->wiznetCoreState) {

        // Initiate the version register readback.
        case WIZNET_CORE_STATE_COMMON_VER_READ :
            if (wiznetCoreCommonVerRead (tcpipStack)) {
                nextState = WIZNET_CORE_STATE_COMMON_VER_CHECK;
            }
            break;

        // Check the results of the version register readback.
        case WIZNET_CORE_STATE_COMMON_VER_CHECK :
            if (wiznetCoreCommonVerCheck (tcpipStack, &statusOk)) {
                if (statusOk) {
                    nextState = WIZNET_CORE_STATE_COMMON_CFG_SET;
                } else {
                    nextState = WIZNET_CORE_STATE_ERROR;
                }
            } else {
                taskStatus = GMOS_TASK_SUSPEND;
            }
            break;

        // Set the common configuration registers.
        case WIZNET_CORE_STATE_COMMON_CFG_SET :
            if (wiznetCoreCommonCfgSet (tcpipStack)) {
                nextState = WIZNET_CORE_STATE_COMMON_CFG_READ;
            }
            break;

        // Read back the common configuration registers.
        case WIZNET_CORE_STATE_COMMON_CFG_READ :
            if (wiznetCoreCommonCfgRead (tcpipStack)) {
                nextState = WIZNET_CORE_STATE_COMMON_CFG_CHECK;
            }
            break;

        // Check the results of the configuration register setup.
        case WIZNET_CORE_STATE_COMMON_CFG_CHECK :
            if (wiznetCoreCommonCfgCheck (tcpipStack, &statusOk)) {
                if (statusOk) {
                    nalData->wiznetSocketSelect = 0;
                    nextState = WIZNET_CORE_STATE_SOCKET_CFG_SET;
                } else {
                    nextState = WIZNET_CORE_STATE_ERROR;
                }
            } else {
                taskStatus = GMOS_TASK_SUSPEND;
            }
            break;

        // Set the socket specific configuration registers.
        case WIZNET_CORE_STATE_SOCKET_CFG_SET :
            if (wiznetCoreSocketCfgSet (tcpipStack)) {
                nextState = WIZNET_CORE_STATE_SOCKET_CFG_READ;
            }
            break;

        // Read back the socket specific configuration registers.
        case WIZNET_CORE_STATE_SOCKET_CFG_READ :
            if (wiznetCoreSocketCfgRead (tcpipStack)) {
                nextState = WIZNET_CORE_STATE_SOCKET_CFG_CHECK;
            }
            break;

        // Check the socket specific buffer configuration.
        case WIZNET_CORE_STATE_SOCKET_CFG_CHECK :
            if (wiznetCoreSocketCfgCheck (tcpipStack, &statusOk)) {
                if (!statusOk) {
                    nextState = WIZNET_CORE_STATE_ERROR;
                } else if (nalData->wiznetSocketSelect < 7) {
                    nalData->wiznetSocketSelect += 1;
                    nextState = WIZNET_CORE_STATE_SOCKET_CFG_SET;
                } else {
                    nextState = WIZNET_CORE_STATE_COMMON_CFG_INT_ENABLE;
                }
            } else {
                taskStatus = GMOS_TASK_SUSPEND;
            }
            break;

        // Enable the required common interrupts.
        case WIZNET_CORE_STATE_COMMON_CFG_INT_ENABLE :
            if (wiznetCoreCommonCfgIntEnable (tcpipStack)) {
                nextState = WIZNET_CORE_STATE_COMMON_CFG_INT_READ;
            }
            break;

        // Read back the common interrupt settings.
        case WIZNET_CORE_STATE_COMMON_CFG_INT_READ :
            if (wiznetCoreCommonCfgIntRead (tcpipStack)) {
                nextState = WIZNET_CORE_STATE_COMMON_CFG_INT_CHECK;
            }
            break;

        // Check the results of the interrupt enable setup.
        case WIZNET_CORE_STATE_COMMON_CFG_INT_CHECK :
            if (wiznetCoreCommonCfgIntCheck (tcpipStack, &statusOk)) {
                if (statusOk) {
                    nextState = WIZNET_CORE_STATE_STARTUP_PHY_READ;
                } else {
                    nextState = WIZNET_CORE_STATE_ERROR;
                }
            } else {
                taskStatus = GMOS_TASK_SUSPEND;
            }
            break;

        // Request the startup status for the Ethernet PHY link.
        case WIZNET_CORE_STATE_STARTUP_PHY_READ :
            if (wiznetCoreStartupPhyRead (tcpipStack)) {
                nextState = WIZNET_CORE_STATE_STARTUP_PHY_CHECK;
            }
            break;

        // Check whether the Ethernet PHY link is up. This repeats at
        // 250ms intervals until a connection is established.
        case WIZNET_CORE_STATE_STARTUP_PHY_CHECK :
            if (wiznetCoreStartupPhyCheck (tcpipStack, &statusOk)) {
                if (statusOk) {
                    nalData->wiznetSocketSelect = 0;
                    nextState = WIZNET_CORE_STATE_RUNNING_INT_ACTIVE;
                } else {
                    nextState = WIZNET_CORE_STATE_STARTUP_PHY_READ;
                    taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (250));
                }
            } else {
                taskStatus = GMOS_TASK_SUSPEND;
            }
            break;

        // Implement running state which provides interrupt detection
        // and the main socket processing loop.
        case WIZNET_CORE_STATE_RUNNING_INT_IDLE :
        case WIZNET_CORE_STATE_RUNNING_INT_ACTIVE :
            taskStatus = wiznetCoreRunning (tcpipStack, &nextState);
            break;

        // Generate an assertion condition in failure mode.
        default :
            GMOS_ASSERT_FAIL ("Unrecoverable error in WIZnet core.");
            taskStatus = GMOS_TASK_SUSPEND;
            break;
    }
    nalData->wiznetCoreState = nextState;
    return taskStatus;
}

/*
 * Initialise the TCP/IP driver network abstraction layer on startup,
 * using the supplied network settings.
 */
bool gmosDriverTcpipInit (
    gmosDriverTcpip_t* tcpipStack, const uint8_t* ethMacAddr)
{
    gmosNalTcpipState_t* nalData = tcpipStack->nalData;
    gmosTaskState_t* coreWorkerTask = &nalData->coreWorkerTask;
    gmosStream_t* spiResponseStream = &nalData->spiResponseStream;
    uint8_t i;

    // Store the Ethernet MAC address in network byte order.
    for (i = 0; i < 6; i++) {
        nalData->ethMacAddr [i] = ethMacAddr [i];
    }

    // Store the default network parameters, which correspond to the
    // initial settings used by DHCP.
    for (i = 0; i < 4; i++) {
        nalData->gatewayAddr [i] = 0xFF;
        nalData->subnetMask [i] = 0xFF;
        nalData->interfaceAddr [i] = 0x00;
    }

    // Initialise the WIZnet SPI interface adaptor.
    if (!gmosNalTcpipWiznetSpiInit (tcpipStack)) {
        return false;
    }

    // Initialise the SPI response data stream, with the core worker
    // task as the stream consumer.
    wiznetSpiAdaptorStream_init (spiResponseStream, coreWorkerTask,
        WIZNET_SPI_ADAPTOR_STREAM_SIZE);

    // Initialise the worker task state machine.
    nalData->wiznetCoreState = WIZNET_CORE_STATE_COMMON_VER_READ;

    // Initialise the socket specific state.
    for (i = 0; i < GMOS_CONFIG_TCPIP_MAX_SOCKETS; i++) {
        (nalData->socketData [i]).socketId = i;
        gmosNalTcpipSocketInit (tcpipStack, &nalData->socketData [i]);
    }

    // Initialise the core worker task and schedule it for immediate
    // execution.
    coreWorkerTask->taskTickFn = wiznetCoreWorkerTaskFn;
    coreWorkerTask->taskData = tcpipStack;
    coreWorkerTask->taskName =
        GMOS_TASK_NAME_WRAPPER ("WIZnet Core Worker Task");
    gmosSchedulerTaskStart (coreWorkerTask);

    return true;
}

/*
 * Update the IPv4 network address and associated network parameters
 * that are to be used by the TCP/IP network abstraction layer.
 */
bool gmosDriverTcpipSetNetworkInfoIpv4 (
    gmosDriverTcpip_t* tcpipStack, const uint8_t* interfaceAddr,
    const uint8_t* gatewayAddr, const uint8_t* subnetMask)
{
    gmosNalTcpipState_t* nalData = tcpipStack->nalData;
    uint8_t oldGatewayAddr [4];
    uint8_t oldSubnetMask [4];
    uint8_t oldInterfaceAddr [4];

    // The network settings can not be configured until initial setup
    // is complete.
    if ((nalData->wiznetCoreState != WIZNET_CORE_STATE_RUNNING_INT_IDLE) &&
        (nalData->wiznetCoreState != WIZNET_CORE_STATE_RUNNING_INT_ACTIVE)) {
        return false;
    }

    // Store the new network parameters in network byte order.
    memcpy (oldGatewayAddr, nalData->gatewayAddr, 4);
    memcpy (oldSubnetMask, nalData->subnetMask, 4);
    memcpy (oldInterfaceAddr, nalData->interfaceAddr, 4);
    memcpy (nalData->gatewayAddr, gatewayAddr, 4);
    memcpy (nalData->subnetMask, subnetMask, 4);
    memcpy (nalData->interfaceAddr, interfaceAddr, 4);

    // Issue the network information configuration command.
    if (wiznetCoreCommonCfgSet (tcpipStack)) {
        return true;
    } else {
        memcpy (nalData->gatewayAddr, oldGatewayAddr, 4);
        memcpy (nalData->subnetMask, oldSubnetMask, 4);
        memcpy (nalData->interfaceAddr, oldInterfaceAddr, 4);
        return false;
    }
}

/*
 * Update the IPv6 network address and associated network parameters
 * that are to be used by the TCP/IP network abstraction layer. IPv6
 * is not supported by the W5500 device.
 */
bool gmosDriverTcpipSetNetworkInfoIpv6 (
    gmosDriverTcpip_t* tcpipStack, const uint8_t* interfaceAddr,
    const uint8_t* gatewayAddr, const uint8_t subnetMask)
{
    GMOS_ASSERT_FAIL ("IPv6 not supported by WIZnet W5500.");
    return false;
}

/*
 * Gets the W5500 transmit and receive buffer size associated with a
 * given socket.
 */
uint16_t gmosNalTcpipSocketGetBufferSize (
    gmosTcpipStackSocket_t* socket)
{
    if ((socket == NULL) ||
        (socket->socketId >= GMOS_CONFIG_TCPIP_MAX_SOCKETS)) {
        return 0;
    } else {
        return 1024 * socketBufSizes [socket->socketId];
    }
}

/*
 * Determines if the underlying physical layer link is ready to
 * transport TCP/IP traffic.
 */
bool gmosDriverTcpipPhyLinkIsUp (gmosDriverTcpip_t* tcpipStack)
{
    uint8_t wiznetCoreState = tcpipStack->nalData->wiznetCoreState;

    if ((wiznetCoreState == WIZNET_CORE_STATE_RUNNING_INT_IDLE) ||
        (wiznetCoreState == WIZNET_CORE_STATE_RUNNING_INT_ACTIVE)) {
        return true;
    } else {
        return false;
    }
}

/*
 * Gets the 48-bit Ethernet MAC address for the TCP/IP driver.
 */
uint8_t* gmosDriverTcpipGetMacAddr (gmosDriverTcpip_t* tcpipStack)
{
    return tcpipStack->nalData->ethMacAddr;
}
