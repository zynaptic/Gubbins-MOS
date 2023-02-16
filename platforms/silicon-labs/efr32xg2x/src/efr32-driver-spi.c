/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2023 Zynaptic Limited
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
 * Provides a wrapper around the Silicon Labs Gecko SDK SPI driver
 * for EFR32xG2x devices.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-driver-spi.h"
#include "efr32-driver-spi.h"
#include "spidrv.h"

/*
 * Specify the set of USART devices that are allocated for use as SPI
 * bus controllers.
 */
static const uint8_t gmosDriverSpiPalBusControllers [] =
    GMOS_CONFIG_EFR32_SPI_BUS_CONTROLLERS;

/*
 * Allocate memory for the Gecko SDK driver data structures.
 */
static SPIDRV_HandleData_t gmosDriverSpiPalSdkHandleData
    [sizeof (gmosDriverSpiPalBusControllers)];

/*
 * Hold reference pointers for the associated SPI interface data
 * structures.
 */
static gmosDriverSpiBus_t* gmosDriverSpiPalSpiInterfaces
    [sizeof (gmosDriverSpiPalBusControllers)] = { NULL };

/*
 * Convert SDK SPI driver status codes to GubbinsMOS SPI status codes.
 */
static gmosDriverSpiStatus_t gmosDriverSpiPalConvStatus (
    Ecode_t sdkStatus)
{
    gmosDriverSpiStatus_t spiStatus;
    switch (sdkStatus) {
        case ECODE_EMDRV_SPIDRV_OK :
            spiStatus = GMOS_DRIVER_SPI_STATUS_SUCCESS;
            break;
        case ECODE_EMDRV_SPIDRV_BUSY :
            spiStatus = GMOS_DRIVER_SPI_STATUS_ACTIVE;
            break;
        case ECODE_EMDRV_SPIDRV_IDLE :
            spiStatus = GMOS_DRIVER_SPI_STATUS_IDLE;
            break;
        case ECODE_EMDRV_SPIDRV_DMA_ALLOC_ERROR :
            spiStatus = GMOS_DRIVER_SPI_STATUS_DMA_ERROR;
            break;
        default :
            spiStatus = GMOS_DRIVER_SPI_STATUS_DRIVER_ERROR;
            break;
    }
    if (sdkStatus != ECODE_EMDRV_SPIDRV_OK) {
        GMOS_LOG_FMT (LOG_VERBOSE,
            "Mapped EFR32 SPI status 0x%08X to GubbinsMOS status %d.",
            sdkStatus, spiStatus);
    }
    return spiStatus;
}

/*
 * Implement SPI transfer completion callback handler for the SDK
 * driver.
 */
static void gmosDriverSpiPalCompletionHandler (
    struct SPIDRV_HandleData *sdkHandle, Ecode_t sdkStatus,
    int transferCount)
{
    gmosDriverSpiBus_t* spiInterface;
    gmosEvent_t* completionEvent;
    uint32_t spiStatus;
    uint32_t eventFlags;
    uint32_t i;

    // Find a matching SDK device driver handle in order to determine
    // the SPI device index.
    for (i = 0; i < sizeof (gmosDriverSpiPalBusControllers); i++) {
        if (sdkHandle == &(gmosDriverSpiPalSdkHandleData [i])) {
            break;
        }
    }
    if (i >= sizeof (gmosDriverSpiPalBusControllers)) {
        return;
    }

    // Set event flags to indicate completion.
    spiInterface = gmosDriverSpiPalSpiInterfaces [i];
    spiStatus = gmosDriverSpiPalConvStatus (sdkStatus);
    eventFlags = transferCount;
    eventFlags <<= GMOS_DRIVER_SPI_EVENT_SIZE_OFFSET;
    eventFlags |= GMOS_DRIVER_SPI_EVENT_COMPLETION_FLAG | spiStatus;

    // Set the GubbinsMOS event flags.
    completionEvent = &(spiInterface->device->completionEvent);
    gmosEventSetBits (completionEvent, eventFlags);
}

/*
 * Initialises the platform abstraction layer for a given SPI interface.
 */
bool gmosDriverSpiPalInit (gmosDriverSpiBus_t* spiInterface)
{
    const gmosPalSpiBusConfig_t* palConfig = spiInterface->palConfig;
    gmosPalSpiBusState_t* palData = spiInterface->palData;
    SPIDRV_Handle_t sdkHandle;
    SPIDRV_Init_t sdkSpiInit;
    Ecode_t sdkStatus;
    uint8_t interfaceId = palConfig->usartInterfaceId;
    uint32_t i;

    // Select the SPI bus data structures to use for the specified
    // USART interface.
    for (i = 0; i < sizeof (gmosDriverSpiPalBusControllers); i++) {
        if (gmosDriverSpiPalBusControllers [i] == interfaceId) {
            break;
        }
    }
    if ((i >= sizeof (gmosDriverSpiPalBusControllers)) ||
        (gmosDriverSpiPalSpiInterfaces [i] != NULL)) {
        return false;
    }

    // Store a local reference to the SPI interface data structure.
    gmosDriverSpiPalSpiInterfaces [i] = spiInterface;

    // Store the index into the matching SPi bus data structures.
    palData->spiIndex = i;

    // Select the appropriate USART instance.
    switch (palConfig->usartInterfaceId) {
        case GMOS_PAL_SPI_BUS_ID_USART0 :
            sdkSpiInit.port = USART0;
            break;
        case GMOS_PAL_SPI_BUS_ID_EUSART0 :
            sdkSpiInit.port = EUSART0;
            break;
        case GMOS_PAL_SPI_BUS_ID_EUSART1 :
            sdkSpiInit.port = EUSART1;
            break;
        default :
            return false;
    }

    // Select the SPI clock pin.
    sdkSpiInit.portClk = (palConfig->sclkPinId >> 8) & 0x03;
    sdkSpiInit.pinClk = palConfig->sclkPinId & 0x0F;

    // Select the SPI transmit data (MOSI) pin.
    sdkSpiInit.portTx = (palConfig->mosiPinId >> 8) & 0x03;
    sdkSpiInit.pinTx = palConfig->mosiPinId & 0x0F;

    // Select the SPI receive data (MISO) pin.
    sdkSpiInit.portRx = (palConfig->misoPinId >> 8) & 0x03;
    sdkSpiInit.pinRx = palConfig->misoPinId & 0x0F;

    // Always use application controlled chip selects.
    sdkSpiInit.portCs = 0;
    sdkSpiInit.pinCs = 0;
    sdkSpiInit.csControl = spidrvCsControlApplication;

    // Set the default clock rate.
    sdkSpiInit.bitRate = 1000000;

    // Select the SPI clock mode to use for all devices on the bus.
    switch (palConfig->spiClockMode) {
        case GMOS_DRIVER_SPI_CLOCK_MODE_0 :
            sdkSpiInit.clockMode = spidrvClockMode0;
            break;
        case GMOS_DRIVER_SPI_CLOCK_MODE_1 :
            sdkSpiInit.clockMode = spidrvClockMode1;
            break;
        case GMOS_DRIVER_SPI_CLOCK_MODE_2 :
            sdkSpiInit.clockMode = spidrvClockMode2;
            break;
        case GMOS_DRIVER_SPI_CLOCK_MODE_3 :
            sdkSpiInit.clockMode = spidrvClockMode3;
            break;
        default :
            return false;
    }

    // Set the remaining fixed configuration options.
    sdkSpiInit.frameLength = 8;
    sdkSpiInit.dummyTxValue = 0xFFFFFFFF;
    sdkSpiInit.type = spidrvMaster;
    sdkSpiInit.bitOrder = spidrvBitOrderMsbFirst;
    sdkSpiInit.slaveStartMode = spidrvSlaveStartImmediate;

    // Initialise the Gecko SDK SPI driver instance.
    sdkHandle = &(gmosDriverSpiPalSdkHandleData [i]);
    sdkStatus = SPIDRV_Init (sdkHandle, &sdkSpiInit);
    return (sdkStatus == ECODE_EMDRV_SPIDRV_OK) ? true : false;
}

/*
 * Sets up the platform abstraction layer for one or more SPI
 * transactions that share the same SPI clock configuration.
 */
void gmosDriverSpiPalClockSetup (gmosDriverSpiBus_t* spiInterface)
{
    const gmosPalSpiBusConfig_t* palConfig = spiInterface->palConfig;
    gmosPalSpiBusState_t* palData = spiInterface->palData;
    gmosDriverSpiDevice_t* spiDevice = spiInterface->device;
    SPIDRV_Handle_t sdkHandle;
    uint32_t spiClockRequest;
    uint32_t spiClockFreq;
    Ecode_t sdkStatus;

    // Select the closest SPI clock scaling to the one requested.
    sdkHandle = &(gmosDriverSpiPalSdkHandleData [palData->spiIndex]);
    spiClockRequest = 1000 * (uint32_t) (spiDevice->spiClockRate);
    sdkStatus = SPIDRV_SetBitrate (sdkHandle, spiClockRequest);
    if (sdkStatus == ECODE_EMDRV_SPIDRV_OK) {
        sdkStatus = SPIDRV_GetBitrate (sdkHandle, &spiClockFreq);
    }
    GMOS_ASSERT (ASSERT_CONFORMANCE,
        (sdkStatus == ECODE_EMDRV_SPIDRV_OK),
        "Requested SPI clock frequency is not supported.");
    if (spiClockRequest != spiClockFreq) {
        GMOS_LOG_FMT (LOG_VERBOSE,
            "Requested SPI clock %d Hz, using closest option %d Hz",
            spiClockRequest, spiClockFreq);
    }

    // The SDK driver API does not support dynamic SPI mode switching,
    // so all devices on the bus must be configured to use the same
    // clock mode.
    GMOS_ASSERT (ASSERT_CONFORMANCE,
        (palConfig->spiClockMode == spiDevice->spiClockMode),
        "Requested SPI clock mode is not supported.");
}

/*
 * Performs a platform specific SPI transaction using the given SPI
 * interface settings.
 */
void gmosDriverSpiPalTransaction (gmosDriverSpiBus_t* spiInterface)
{
    gmosPalSpiBusState_t* palData = spiInterface->palData;
    const void* txBuffer = spiInterface->writeData;
    void* rxBuffer = spiInterface->readData;
    uint16_t transferSize = spiInterface->transferSize;
    SPIDRV_Handle_t sdkHandle;
    Ecode_t sdkStatus;

    // Initiate a read/write transaction using the SDK driver.
    sdkHandle = &(gmosDriverSpiPalSdkHandleData [palData->spiIndex]);
    if ((txBuffer != NULL) && (rxBuffer != NULL)) {
        sdkStatus = SPIDRV_MTransfer (sdkHandle, txBuffer, rxBuffer,
            transferSize, gmosDriverSpiPalCompletionHandler);
    }

    // Initiate a write transaction using the SDK driver.
    else if (txBuffer != NULL) {
        sdkStatus = SPIDRV_MTransmit (sdkHandle, txBuffer,
            transferSize, gmosDriverSpiPalCompletionHandler);
    }

    // Initiate a read transaction using the SDK driver.
    else if (rxBuffer != NULL) {
        sdkStatus = SPIDRV_MReceive (sdkHandle, rxBuffer,
            transferSize, gmosDriverSpiPalCompletionHandler);
    } else {
        sdkStatus = ECODE_EMDRV_SPIDRV_PARAM_ERROR;
    }

    // Forward error conditions to the callback handler.
    if (sdkStatus != ECODE_EMDRV_SPIDRV_OK) {
        gmosDriverSpiPalCompletionHandler (sdkHandle, sdkStatus, 0);
    }
}

/*
 * Performs a platform specific SPI inline transaction using the given
 * SPI interface.
 */
gmosDriverSpiStatus_t gmosDriverSpiPalInlineTransaction
    (gmosDriverSpiBus_t* spiInterface)
{
    gmosPalSpiBusState_t* palData = spiInterface->palData;
    const void* txBuffer = spiInterface->writeData;
    void* rxBuffer = spiInterface->readData;
    uint16_t transferSize = spiInterface->transferSize;
    SPIDRV_Handle_t sdkHandle;
    Ecode_t sdkStatus;

    // Initiate a read/write transaction using the SDK driver.
    sdkHandle = &(gmosDriverSpiPalSdkHandleData [palData->spiIndex]);
    if ((txBuffer != NULL) && (rxBuffer != NULL)) {
        sdkStatus = SPIDRV_MTransferB (sdkHandle,
            txBuffer, rxBuffer, transferSize);
    }

    // Initiate a write transaction using the SDK driver.
    else if (txBuffer != NULL) {
        sdkStatus = SPIDRV_MTransmitB (sdkHandle,
            txBuffer, transferSize);
    }

    // Initiate a read transaction using the SDK driver.
    else if (rxBuffer != NULL) {
        sdkStatus = SPIDRV_MReceiveB (sdkHandle,
            rxBuffer, transferSize);
    } else {
        sdkStatus = ECODE_EMDRV_SPIDRV_PARAM_ERROR;
    }

    // Convert SDK status codes to GubbinsMOS SPI status codes.
    return gmosDriverSpiPalConvStatus (sdkStatus);
}
