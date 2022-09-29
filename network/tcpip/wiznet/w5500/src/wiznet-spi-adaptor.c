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
 * This file implements the vendor specific functions for accessing a
 * WIZnet W5500 TCP/IP offload device over the SPI interface.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-mempool.h"
#include "gmos-streams.h"
#include "gmos-driver-spi.h"
#include "gmos-driver-tcpip.h"
#include "wiznet-driver-tcpip.h"
#include "wiznet-spi-adaptor.h"

/*
 * Send the SPI command header using the blocking API.
 */
static inline gmosDriverSpiStatus_t wiznetSpiAdaptorSendHeader (
    gmosDriverTcpip_t* tcpipStack)
{
    const gmosNalTcpipConfig_t* nalConfig = tcpipStack->nalConfig;
    gmosNalTcpipState_t* nalData = tcpipStack->nalData;
    wiznetSpiAdaptorCmd_t* spiCommand = &(nalData->spiCommand);
    uint8_t headerBytes [3];

    // Format the header for variable length data mode transfers.
    headerBytes [0] = (uint8_t) (spiCommand->address >> 8);
    headerBytes [1] = (uint8_t) (spiCommand->address);
    headerBytes [2] = spiCommand->control &
        WIZNET_SPI_ADAPTOR_CTRL_DATA_MODE_MASK;

    // Perform a blocking write to the SPI interface.
    return gmosDriverSpiIoInlineWrite (nalConfig->spiInterface,
        headerBytes, sizeof (headerBytes));
}

/*
 * Implement short data transfers using the blocking API.
 */
static inline gmosDriverSpiStatus_t wiznetSpiAdaptorTransferBytes (
    gmosDriverTcpip_t* tcpipStack)
{
    const gmosNalTcpipConfig_t* nalConfig = tcpipStack->nalConfig;
    gmosNalTcpipState_t* nalData = tcpipStack->nalData;
    wiznetSpiAdaptorCmd_t* spiCommand = &(nalData->spiCommand);

    // Ensure that the size field is valid.
    if (spiCommand->size > sizeof (spiCommand->data.bytes)) {
        return GMOS_DRIVER_SPI_STATUS_DRIVER_ERROR;
    }

    // Perform a blocking SPI write if requested.
    if ((spiCommand->control & WIZNET_SPI_ADAPTOR_CTRL_WRITE_ENABLE) != 0) {
        return gmosDriverSpiIoInlineWrite (nalConfig->spiInterface,
            spiCommand->data.bytes, spiCommand->size);
    }

    // Alternatively perform a blocking SPI read.
    else {
        return gmosDriverSpiIoInlineRead (nalConfig->spiInterface,
            spiCommand->data.bytes, spiCommand->size);
    }
}

/*
 * Implement buffer based transfers using the non-blocking API.
 */
static inline gmosDriverSpiStatus_t wiznetSpiAdaptorTransferBuffer (
    gmosDriverTcpip_t* tcpipStack)
{
    const gmosNalTcpipConfig_t* nalConfig = tcpipStack->nalConfig;
    gmosNalTcpipState_t* nalData = tcpipStack->nalData;
    wiznetSpiAdaptorCmd_t* spiCommand = &(nalData->spiCommand);
    gmosBuffer_t* dataBuffer = &(spiCommand->data.buffer);
    gmosMempoolSegment_t* mempoolSegment;
    uint16_t transferOffset;
    uint8_t* transferPtr;
    uint16_t transferSize;

    // Get the memory pool segment to use for the next transfer.
    transferOffset = nalData->spiBufferOffset;
    mempoolSegment = gmosBufferGetSegment (dataBuffer, transferOffset);
    if (mempoolSegment == NULL) {
        return GMOS_DRIVER_SPI_STATUS_DRIVER_ERROR;
    }

    // In most cases, the data transfer pointer will be set to the
    // beginning of the memory pool segment and the transfer request
    // will be sized accordingly.
    transferPtr = mempoolSegment->data.bytes;
    transferSize = dataBuffer->bufferSize - transferOffset;
    if (transferSize > GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE) {
        transferSize = GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE;
    }

    // An adjustment is required if the buffer contents are not aligned
    // to the start of the first memory segment.
    if ((transferOffset == 0) && (dataBuffer->bufferOffset != 0)) {
        transferPtr += dataBuffer->bufferOffset;
        if (transferSize + dataBuffer->bufferOffset >
            GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE) {
            transferSize = GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE -
                dataBuffer->bufferOffset;
        }
    }

    // Initiate a non-blocking SPI write if requested.
    if ((spiCommand->control & WIZNET_SPI_ADAPTOR_CTRL_WRITE_ENABLE) != 0) {
        if (!gmosDriverSpiIoWrite (nalConfig->spiInterface,
            transferPtr, transferSize)) {
            return GMOS_DRIVER_SPI_STATUS_NOT_READY;
        }
    }

    // Alternatively initiate a non-blocking SPI read.
    else {
        if (!gmosDriverSpiIoRead (nalConfig->spiInterface,
            transferPtr, transferSize)) {
            return GMOS_DRIVER_SPI_STATUS_NOT_READY;
        }
    }

    // Update the transfer offset on initiating the transfer.
    nalData->spiBufferOffset += transferSize;
    return GMOS_DRIVER_SPI_STATUS_SUCCESS;
}

/*
 * Format an interrupt status read command for the common register
 * block.
 */
static inline void wiznetSpiAdaptorFormatIntRead (
    wiznetSpiAdaptorCmd_t* intReadCommand) {

    // On an interrupt, attempt to issue a request for the common
    // interrupt register block at address 0x0015.
    intReadCommand->address = 0x0015;
    intReadCommand->control =
        WIZNET_SPI_ADAPTOR_CTRL_COMMON_REGS |
        WIZNET_SPI_ADAPTOR_CTRL_READ_ENABLE;
    intReadCommand->size = 4;
}

/*
 * Implement SPI transaction response generation.
 */
static inline bool wiznetSpiAdaptorSendResponse (
    gmosDriverTcpip_t* tcpipStack)
{
    gmosNalTcpipState_t* nalData = tcpipStack->nalData;
    wiznetSpiAdaptorCmd_t* spiCommand = &(nalData->spiCommand);

    // Discard the response if it is not required.
    if ((spiCommand->control &
        WIZNET_SPI_ADAPTOR_CTRL_DISCARD_RESPONSE) != 0) {
        if (spiCommand->size == 0) {
            gmosBufferReset (&(spiCommand->data.buffer), 0);
        }
        return true;
    }

    // Attempt to queue the response if it is required.
    else {
        return wiznetSpiAdaptorStream_write (
            &nalData->spiResponseStream, spiCommand);
    }
}

/*
 * Implement the main task loop for the WIZnet SPI interface.
 */
static gmosTaskStatus_t wiznetSpiAdaptorWorkerTaskFn (void* taskData)
{
    gmosDriverTcpip_t* tcpipStack = (gmosDriverTcpip_t*) taskData;
    const gmosNalTcpipConfig_t* nalConfig = tcpipStack->nalConfig;
    gmosNalTcpipState_t* nalData = tcpipStack->nalData;
    gmosEvent_t* interruptEvent = &nalData->interruptEvent;
    gmosDriverSpiStatus_t spiStatus;
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    uint8_t nextState = nalData->wiznetAdaptorState;

    // Implement the WIZnet SPI adaptor state machine.
    switch (nalData->wiznetAdaptorState) {

        // From the initialisation state, ensure the WIZnet NCP is held
        // in reset for 250 milliseconds.
        case WIZNET_SPI_ADAPTOR_STATE_INIT :
            gmosDriverGpioSetPinState (nalConfig->ncpResetPin, false);
            taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (250));
            nextState = WIZNET_SPI_ADAPTOR_STATE_RESET;
            break;

        // Enable interrupts and take the WIZnet NCP out of reset. A
        // delay of 250 milliseconds is inserted to allow the device to
        // start up.
        case WIZNET_SPI_ADAPTOR_STATE_RESET :
            gmosDriverGpioInterruptEnable (nalConfig->ncpInterruptPin, false, true);
            gmosDriverGpioSetPinState (nalConfig->ncpResetPin, true);
            taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (250));
            nextState = WIZNET_SPI_ADAPTOR_STATE_IDLE;
            break;

        // In the idle state, wait for an interrupt or a new command to
        // process.
        case WIZNET_SPI_ADAPTOR_STATE_IDLE :
            if (gmosEventResetBits (interruptEvent) != 0) {
                wiznetSpiAdaptorFormatIntRead (&(nalData->spiCommand));
                nextState = WIZNET_SPI_ADAPTOR_STATE_SELECT;
            } else if (wiznetSpiAdaptorStream_read (
                &nalData->spiCommandStream, &(nalData->spiCommand))) {
                nextState = WIZNET_SPI_ADAPTOR_STATE_SELECT;
            } else {
                taskStatus = GMOS_TASK_SUSPEND;
            }
            break;

        // Select the SPI device.
        case WIZNET_SPI_ADAPTOR_STATE_SELECT :
            if (gmosDriverSpiDeviceSelect (
                nalConfig->spiInterface, &nalData->spiDevice)) {
                nextState = WIZNET_SPI_ADAPTOR_STATE_SEND_HEADER;
            }
            break;

        // Send the SPI command header using a blocking SPI transfer.
        case WIZNET_SPI_ADAPTOR_STATE_SEND_HEADER :
            spiStatus = wiznetSpiAdaptorSendHeader (tcpipStack);
            if (spiStatus == GMOS_DRIVER_SPI_STATUS_SUCCESS) {
                if (nalData->spiCommand.size > 0) {
                    nextState = WIZNET_SPI_ADAPTOR_STATE_TRANSFER_BYTES;
                } else {
                    nalData->spiBufferOffset = 0;
                    nextState = WIZNET_SPI_ADAPTOR_STATE_TRANSFER_BUFFER;
                }
            } else if (spiStatus != GMOS_DRIVER_SPI_STATUS_NOT_READY) {
                nextState = WIZNET_SPI_ADAPTOR_STATE_ERROR;
            }
            break;

        // Transfer the command data bytes using a blocking SPI transfer.
        case WIZNET_SPI_ADAPTOR_STATE_TRANSFER_BYTES :
            spiStatus = wiznetSpiAdaptorTransferBytes (tcpipStack);
            if (spiStatus == GMOS_DRIVER_SPI_STATUS_SUCCESS) {
                nextState = WIZNET_SPI_ADAPTOR_STATE_RELEASE;
            } else if (spiStatus != GMOS_DRIVER_SPI_STATUS_NOT_READY) {
                nextState = WIZNET_SPI_ADAPTOR_STATE_ERROR;
            }
            break;

        // Transfer the command data buffer using a sequence of
        // non-blocking operations.
        case WIZNET_SPI_ADAPTOR_STATE_TRANSFER_BUFFER :
            spiStatus = wiznetSpiAdaptorTransferBuffer (tcpipStack);
            if (spiStatus == GMOS_DRIVER_SPI_STATUS_SUCCESS) {
                nextState = WIZNET_SPI_ADAPTOR_STATE_TRANSFER_WAIT;
                taskStatus = GMOS_TASK_SUSPEND;
            } else if (spiStatus != GMOS_DRIVER_SPI_STATUS_NOT_READY) {
                nextState = WIZNET_SPI_ADAPTOR_STATE_ERROR;
            }
            break;

        // Wait for the completion of a non-blocking operation.
        case WIZNET_SPI_ADAPTOR_STATE_TRANSFER_WAIT :
            spiStatus = gmosDriverSpiIoComplete (
                nalConfig->spiInterface, NULL);
            if (spiStatus == GMOS_DRIVER_SPI_STATUS_SUCCESS) {
                if (nalData->spiBufferOffset >=
                    nalData->spiCommand.data.buffer.bufferSize) {
                    nextState = WIZNET_SPI_ADAPTOR_STATE_RELEASE;
                } else {
                    nextState = WIZNET_SPI_ADAPTOR_STATE_TRANSFER_BUFFER;
                }
            } else if (spiStatus == GMOS_DRIVER_SPI_STATUS_ACTIVE) {
                taskStatus = GMOS_TASK_SUSPEND;
            } else {
                nextState = WIZNET_SPI_ADAPTOR_STATE_ERROR;
            }
            break;

        // Release the SPI bus on completion of the current transaction.
        case WIZNET_SPI_ADAPTOR_STATE_RELEASE :
            if (gmosDriverSpiDeviceRelease (
                nalConfig->spiInterface, &nalData->spiDevice)) {
                nextState = WIZNET_SPI_ADAPTOR_STATE_RESPOND;
            }
            break;

        // Respond by forwarding the API command data structure to the
        // command response stream.
        case WIZNET_SPI_ADAPTOR_STATE_RESPOND :
            if (wiznetSpiAdaptorSendResponse (tcpipStack)) {
                nextState = WIZNET_SPI_ADAPTOR_STATE_IDLE;
            }
            break;

        // Generate an assertion condition in failure mode.
        default :
            GMOS_ASSERT_FAIL ("Unrecoverable error in WIZnet SPI adaptor.");
            taskStatus = GMOS_TASK_SUSPEND;
            break;
    }
    nalData->wiznetAdaptorState = nextState;
    return taskStatus;
}

/*
 * Implement the interrupt service routine for detecting the falling
 * edge of the NCP interrupt line and converting it to an ISR event.
 */
static void wiznetSpiAdaptorIsr (void* isrData)
{
    gmosEvent_t* interruptEvent = (gmosEvent_t*) isrData;
    gmosEventSetBits (interruptEvent,
        WIZNET_INTERRUPT_FLAG_NCP_REQUEST);
}

/*
 * Initialise the WIZnet W5500 SPI adaptor task on startup.
 */
bool gmosNalTcpipWiznetSpiInit (gmosDriverTcpip_t* tcpipStack)
{
    const gmosNalTcpipConfig_t* nalConfig = tcpipStack->nalConfig;
    gmosNalTcpipState_t* nalData = tcpipStack->nalData;
    gmosTaskState_t* spiWorkerTask = &nalData->spiWorkerTask;
    gmosEvent_t* interruptEvent = &nalData->interruptEvent;
    gmosStream_t* spiCommandStream = &nalData->spiCommandStream;
    gmosDriverSpiDevice_t* spiDevice = &nalData->spiDevice;

    // Initialise the GPIO reset line and place the NCP in reset.
    if (!gmosDriverGpioPinInit (nalConfig->ncpResetPin,
        GMOS_DRIVER_GPIO_OUTPUT_PUSH_PULL, GMOS_DRIVER_GPIO_SLEW_MINIMUM,
        GMOS_DRIVER_GPIO_INPUT_PULL_NONE)) {
        return false;
    }
    if (!gmosDriverGpioSetAsOutput (nalConfig->ncpResetPin)) {
        return false;
    }
    gmosDriverGpioSetPinState (nalConfig->ncpResetPin, false);

    // Initialise the interrupt request line and attach the interrupt
    // service routine. Interrupts are not enabled at this stage.
    if (!gmosDriverGpioInterruptInit (nalConfig->ncpInterruptPin,
        wiznetSpiAdaptorIsr, interruptEvent,
        GMOS_DRIVER_GPIO_INPUT_PULL_UP)) {
        return false;
    }

    // Initialise the SPI device, with the worker task as the SPI device
    // event consumer.
    if (!gmosDriverSpiDeviceInit (spiDevice, spiWorkerTask,
        nalConfig->spiChipSelectPin, WIZNET_SPI_CLOCK_FREQUENCY,
        WIZNET_SPI_CLOCK_MODE)) {
        return false;
    }

    // Initialise the notification events, with the SPI worker task as
    // the event consumer.
    gmosEventInit (interruptEvent, spiWorkerTask);

    // Initialise the SPI command data stream, with the SPI worker task
    // as the stream consumer.
    wiznetSpiAdaptorStream_init (spiCommandStream, spiWorkerTask,
        WIZNET_SPI_ADAPTOR_STREAM_SIZE);

    // Initialise the worker task state machine.
    nalData->wiznetAdaptorState = WIZNET_SPI_ADAPTOR_STATE_INIT;

    // Initialise the worker task and schedule it for immediate
    // execution.
    spiWorkerTask->taskTickFn = wiznetSpiAdaptorWorkerTaskFn;
    spiWorkerTask->taskData = tcpipStack;
    spiWorkerTask->taskName =
        GMOS_TASK_NAME_WRAPPER ("WIZnet SPI Driver Task");
    gmosSchedulerTaskStart (spiWorkerTask);

    return true;
}
