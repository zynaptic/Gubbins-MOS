/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2020-2021 Zynaptic Limited
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
 * Implements SPI bus controller functionality for devices using the
 * Microchip Harmony vendor framework.
 */

#include <stdint.h>
#include <stdbool.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-events.h"
#include "gmos-driver-spi.h"
#include "gmos-driver-gpio.h"
#include "harmony-driver-spi.h"
#include "harmony-driver-gpio.h"

/*
 * Harmony ISR callback handler on attempting to start a SPI buffer
 * transfer.
 */
static void harmonyTransferStartHandler (DRV_SPI_BUFFER_EVENT event,
    DRV_SPI_BUFFER_HANDLE bufferHandle, void* context)
{
    // No action required.
}

/*
 * Harmony ISR callback handler on finishing a SPI buffer transfer.
 */
static void harmonyTransferEndHandler (DRV_SPI_BUFFER_EVENT event,
    DRV_SPI_BUFFER_HANDLE bufferHandle, void* context)
{
    // No action required.
}

/*
 * Harmony ISR callback handler on completing a SPI buffer transfer.
 */
static void harmonyTransferCompleteHandler (DRV_SPI_BUFFER_EVENT status,
    DRV_SPI_BUFFER_HANDLE bufferHandle, void* context)
{
    gmosDriverSpiBus_t* spiInterface = (gmosDriverSpiBus_t*) context;
    gmosDriverSpiDevice_t* spiDevice = spiInterface->device;
    uint32_t eventFlags = 0;

    // Indicate successful completion.
    if (status == DRV_SPI_BUFFER_EVENT_COMPLETE) {
        eventFlags = spiInterface->transferSize;
        eventFlags <<= GMOS_DRIVER_SPI_EVENT_SIZE_OFFSET;
        eventFlags |= GMOS_DRIVER_SPI_EVENT_COMPLETION_FLAG |
            GMOS_DRIVER_SPI_STATUS_SUCCESS;
    }

    // Indicate failure. Pending or active transfers are ignored.
    else if (status == DRV_SPI_BUFFER_EVENT_ERROR) {
        eventFlags = GMOS_DRIVER_SPI_EVENT_COMPLETION_FLAG |
            GMOS_DRIVER_SPI_STATUS_DRIVER_ERROR;
    }

    // Trigger the GMOS event if required.
    if (eventFlags != 0) {
        gmosEventAssignBits (&spiDevice->completionEvent, eventFlags);
    }
}

/*
 * Implement common transaction request function.
 */
static DRV_SPI_BUFFER_HANDLE harmonyTransferRequest (
    gmosDriverSpiBus_t* spiInterface, bool addCallbacks)
{
    gmosPalSpiBusState_t* spiState = spiInterface->palData;
    uint8_t* rxDataBuffer = spiInterface->readData;
    uint8_t* txDataBuffer = spiInterface->writeData;
    uint16_t transferSize = spiInterface->transferSize;
    DRV_SPI_BUFFER_HANDLE drvBuffer = DRV_SPI_BUFFER_HANDLE_INVALID;
    DRV_SPI_BUFFER_EVENT_HANDLER callbackHandler;
    void* callbackData;

    // Determine whether callbacks are required.
    if (addCallbacks) {
        callbackHandler = harmonyTransferCompleteHandler;
        callbackData = spiInterface;
    } else {
        callbackHandler = NULL;
        callbackData = NULL;
    }

    // Initiate a combined read and write transaction.
    if ((rxDataBuffer != NULL) && (txDataBuffer != NULL)) {
        drvBuffer = DRV_SPI_BufferAddWriteRead (spiState->harmonyDriver,
            txDataBuffer, transferSize, rxDataBuffer, transferSize,
            callbackHandler, callbackData);
    }

    // Initiate a read only transaction.
    else if (rxDataBuffer != NULL) {
        drvBuffer = DRV_SPI_BufferAddRead (spiState->harmonyDriver,
            rxDataBuffer, transferSize, callbackHandler, callbackData);
    }

    // Initiate a write only transaction.
    else if (txDataBuffer != NULL) {
        drvBuffer = DRV_SPI_BufferAddWrite (spiState->harmonyDriver,
            txDataBuffer, transferSize, callbackHandler, callbackData);
    }
    return drvBuffer;
}

/*
 * Initialises the SPI driver platform abstraction layer for the
 * Microchip Harmony vendor framework.
 */
bool gmosDriverSpiPalInit (gmosDriverSpiBus_t* spiInterface)
{
    const gmosPalSpiBusConfig_t* spiConfig = spiInterface->palConfig;
    gmosPalSpiBusState_t* spiState = spiInterface->palData;
    DRV_HANDLE drvHandle;

    // Open the Harmony SPI driver ready for use.
    drvHandle = DRV_SPI_Open (
        spiConfig->harmonyDeviceIndex,
        DRV_IO_INTENT_EXCLUSIVE | DRV_IO_INTENT_READWRITE);

    // Cache the new driver handle if it is valid.
    if (drvHandle != DRV_HANDLE_INVALID) {
        spiState->harmonyDriver = drvHandle;
        return true;
    } else {
        return false;
    }
}

/*
 * Sets up the platform abstraction layer for one or more SPI
 * transactions that share the same SPI clock configuration.
 */
void gmosDriverSpiPalClockSetup (gmosDriverSpiBus_t* spiInterface)
{
    gmosPalSpiBusState_t* spiState = spiInterface->palData;
    gmosDriverSpiDevice_t* spiDevice = spiInterface->device;
    DRV_SPI_CLIENT_DATA drvClient;
    int32_t drvStatus;

    // Populate the SPI driver client data structure. This specifies
    // the required baud rate and the callback handlers.
    drvClient.baudRate = 1000 * spiDevice->spiClockRate;
    drvClient.operationStarting = harmonyTransferStartHandler;
    drvClient.operationEnded = harmonyTransferEndHandler;

    // Assign the new clock configuration.
    drvStatus = DRV_SPI_ClientConfigure (
        spiState->harmonyDriver, &drvClient);
    GMOS_ASSERT (ASSERT_ERROR, (drvStatus >= 0),
       "Failed to set Harmony SPI driver client options.");
}

/*
 * Performs a platform specific SPI transaction using the given SPI
 * interface settings.
 */
void gmosDriverSpiPalTransaction (gmosDriverSpiBus_t* spiInterface)
{
    gmosPalSpiBusState_t* spiState = spiInterface->palData;
    gmosDriverSpiDevice_t* spiDevice = spiInterface->device;
    DRV_SPI_BUFFER_HANDLE drvBuffer;

    // Initiate a Harmony framework transfer request.
    drvBuffer = harmonyTransferRequest (spiInterface, true);

    // On success, update the SPI interface state and wait for the
    // completion callback.
    if (drvBuffer != DRV_SPI_BUFFER_HANDLE_INVALID) {
        spiState->harmonyBuffer = drvBuffer;
    }

    // On failure, notify status via the GMOS event.
    else {
        gmosEventAssignBits (&spiDevice->completionEvent,
            GMOS_DRIVER_SPI_EVENT_COMPLETION_FLAG |
            GMOS_DRIVER_SPI_STATUS_DRIVER_ERROR);
    }
}

/*
 * Performs a platform specific SPI inline transaction using the given
 * SPI interface.
 */
gmosDriverSpiStatus_t gmosDriverSpiPalInlineTransaction
    (gmosDriverSpiBus_t* spiInterface)
{
    DRV_SPI_BUFFER_HANDLE drvBuffer;

    // Initiate a Harmony framework transfer request.
    drvBuffer = harmonyTransferRequest (spiInterface, false);

    // Return on immediate failure.
    if (drvBuffer == DRV_SPI_BUFFER_HANDLE_INVALID) {
        return GMOS_DRIVER_SPI_STATUS_DRIVER_ERROR;
    }

    // Implement busy waiting for completion.
    while (true) {
        DRV_SPI_BUFFER_EVENT status = DRV_SPI_BufferStatus (drvBuffer);
        if (status == DRV_SPI_BUFFER_EVENT_COMPLETE) {
            return GMOS_DRIVER_SPI_STATUS_SUCCESS;
        } else if (status == DRV_SPI_BUFFER_EVENT_ERROR) {
            return GMOS_DRIVER_SPI_STATUS_DRIVER_ERROR;
        }
    }
}
