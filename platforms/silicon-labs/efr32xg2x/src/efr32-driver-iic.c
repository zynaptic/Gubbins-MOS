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
 * Implements IIC (AKA I2C) driver functionality for the Silicon Labs
 * EFR32xG2x series of microcontrollers.
 *
 * TODO: Transaction processing timeouts for interrupt driven transfers
 * are not currently supported.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-driver-iic.h"
#include "efr32-driver-iic.h"
#include "em_gpio.h"
#include "em_cmu.h"
#include "em_i2c.h"

/*
 * Specify the SDK register base addresses to use.
 */
static I2C_TypeDef* gmosDriverIicPalSdkInstances [] = { I2C0, I2C1 };

/*
 * Specify the NVIC interrupts to use.
 */
static IRQn_Type gmosDriverIicPalNvicInts [] = { I2C0_IRQn, I2C1_IRQn };

/*
 * Allocate memory for the Gecko SDK driver data structures.
 */
static I2C_TransferSeq_TypeDef gmosDriverIicPalSdkTransferData [2];

/*
 * Hold reference pointers for the associated IIC interface data
 * structures.
 */
static gmosDriverIicBus_t* gmosDriverIicPalIicInterfaces [2] = { NULL, NULL };

/*
 * Convert SDK I2C driver status codes to GubbinsMOS IIC status codes.
 */
static gmosDriverIicStatus_t gmosDriverIicPalConvStatus (
    I2C_TransferReturn_TypeDef sdkStatus)
{
    gmosDriverIicStatus_t iicStatus;
    switch (sdkStatus) {
        case i2cTransferInProgress :
            iicStatus = GMOS_DRIVER_IIC_STATUS_ACTIVE;
            break;
        case i2cTransferDone :
            iicStatus = GMOS_DRIVER_IIC_STATUS_SUCCESS;
            break;
        case i2cTransferNack :
            iicStatus = GMOS_DRIVER_IIC_STATUS_NACK;
            break;
        default :
            iicStatus = GMOS_DRIVER_IIC_STATUS_DRIVER_ERROR;
            break;
    }
    if ((sdkStatus != i2cTransferInProgress) &&
        (sdkStatus != i2cTransferDone)) {
        GMOS_LOG_FMT (LOG_VERBOSE,
            "Mapped EFR32 I2C status 0x%08X to GubbinsMOS status %d.",
            sdkStatus, iicStatus);
    }
    return iicStatus;
}

/*
 * Populate an SDK transaction data structure from the GubbinsMOS
 * transfer request.
 */
static bool gmosDriverIicPalSetTransferData (
    gmosDriverIicBus_t* iicInterface)
{
    uint8_t interfaceId = iicInterface->palConfig->iicInterfaceId;
    gmosDriverIicDevice_t* iicDevice = iicInterface->device;
    I2C_TransferSeq_TypeDef* transferData =
        &(gmosDriverIicPalSdkTransferData [interfaceId]);
    bool validTransfer = false;

    // Populate the device address. This needs to be shifted into the
    // upper 7 bits.
    transferData->addr = iicDevice->iicAddr << 1;

    // Populate combined write/read transfer.
    if ((iicInterface->writeData != NULL) &&
        (iicInterface->readData != NULL)) {
        transferData->flags = I2C_FLAG_WRITE_READ;
        transferData->buf[0].data = iicInterface->writeData;
        transferData->buf[0].len = iicInterface->writeSize;
        transferData->buf[1].data = iicInterface->readData;
        transferData->buf[1].len = iicInterface->readSize;
        validTransfer = true;
    }

    // Populate write only transfer.
    else if (iicInterface->writeData != NULL) {
        transferData->flags = I2C_FLAG_WRITE;
        transferData->buf[0].data = iicInterface->writeData;
        transferData->buf[0].len = iicInterface->writeSize;
        validTransfer = true;
    }

    // Populate read only transfer.
    else if (iicInterface->readData != NULL) {
        transferData->flags = I2C_FLAG_READ;
        transferData->buf[0].data = iicInterface->readData;
        transferData->buf[0].len = iicInterface->readSize;
        validTransfer = true;
    }
    return validTransfer;
}

/*
 * Implement transaction completion handler.
 */
void gmosDriverIicPalCompletionHandler (
    gmosDriverIicBus_t* iicInterface, gmosDriverIicStatus_t iicStatus,
    uint32_t transferSize)
{
    gmosEvent_t* completionEvent;
    uint32_t eventFlags;

    // Set event flags to indicate completion.
    eventFlags = transferSize;
    eventFlags <<= GMOS_DRIVER_IIC_EVENT_SIZE_OFFSET;
    eventFlags |= GMOS_DRIVER_IIC_EVENT_COMPLETION_FLAG | iicStatus;

    // Set the GubbinsMOS event flags.
    completionEvent = &(iicInterface->device->completionEvent);
    gmosEventSetBits (completionEvent, eventFlags);
}

/*
 * Implement common interrupt handler.
 */
static void gmosDriverIicPalCommonIsr (uint8_t interfaceId)
{
    gmosDriverIicBus_t* iicInterface;
    I2C_TypeDef* i2cInstance;
    I2C_TransferReturn_TypeDef i2cStatus;
    gmosDriverIicStatus_t iicStatus;
    uint32_t intsPending;
    uint32_t transferSize;

    // Implement an SDK driver processing step.
    i2cInstance = gmosDriverIicPalSdkInstances [interfaceId];
    do {
        i2cStatus = I2C_Transfer (i2cInstance);
        intsPending = i2cInstance->IEN & i2cInstance->IF;
    } while ((i2cStatus == i2cTransferInProgress) && (intsPending != 0));

    // Disable interrupts and indicate completion if required.
    if (i2cStatus != i2cTransferInProgress) {
        NVIC_DisableIRQ (gmosDriverIicPalNvicInts [interfaceId]);
        iicInterface = gmosDriverIicPalIicInterfaces [interfaceId];
        iicStatus = gmosDriverIicPalConvStatus (i2cStatus);
        if (iicStatus == GMOS_DRIVER_IIC_STATUS_SUCCESS) {
            transferSize =
                iicInterface->writeSize + iicInterface->readSize;
        } else {
            transferSize = 0;
        }
        gmosDriverIicPalCompletionHandler (
            iicInterface, iicStatus, transferSize);
    }
}

/*
 * Implement interrupt handler for I2C interface 0.
 */
void I2C0_IRQHandler (void)
{
    gmosDriverIicPalCommonIsr (GMOS_PAL_IIC_BUS_ID_I2C0);
}

/*
 * Implement interrupt handler for I2C interface 1.
 */
void I2C1_IRQHandler (void)
{
    gmosDriverIicPalCommonIsr (GMOS_PAL_IIC_BUS_ID_I2C1);
}

/*
 * Initialises the platform abstraction layer for a given IIC interface.
 */
bool gmosDriverIicPalInit (gmosDriverIicBus_t* iicInterface)
{
    const gmosPalIicBusConfig_t* palConfig = iicInterface->palConfig;
    I2C_TypeDef* i2cInstance;
    I2C_Init_TypeDef i2cInit = I2C_INIT_DEFAULT;
    GPIO_I2CROUTE_TypeDef* i2cRoute;
    uint8_t interfaceId = palConfig->iicInterfaceId;
    uint32_t sclPort;
    uint32_t sclPin;
    uint32_t sdaPort;
    uint32_t sdaPin;

    // Select the IIC bus data structures to use for the specified
    // I2C interface.
    if ((interfaceId > GMOS_PAL_IIC_BUS_ID_I2C1) ||
        (gmosDriverIicPalIicInterfaces [interfaceId] != NULL)) {
        return false;
    }

    // Store a local reference to the IIC interface data structure.
    gmosDriverIicPalIicInterfaces [interfaceId] = iicInterface;

    // Select the peripheral clock for the specified I2C interface.
    if (interfaceId == GMOS_PAL_IIC_BUS_ID_I2C0) {
        CMU_ClockEnable (cmuClock_I2C0, true);
    } else if (interfaceId == GMOS_PAL_IIC_BUS_ID_I2C1) {
        CMU_ClockEnable (cmuClock_I2C1, true);
    }

    // The default GPIO output value must be set to 1 to keep lines
    // high. Set SCL first, to ensure it is high before changing SDA.
    sclPort = (palConfig->sclPinId >> 8) & 0x03;
    sclPin = palConfig->sclPinId & 0x0F;
    sdaPort = (palConfig->sdaPinId >> 8) & 0x03;
    sdaPin = palConfig->sdaPinId & 0x0F;
    GPIO_PinModeSet (sclPort, sclPin, gpioModeWiredAndPullUp, 1);
    GPIO_PinModeSet (sdaPort, sdaPin, gpioModeWiredAndPullUp, 1);

    // Route I2C signals to their assigned GPIO pins.
    i2cRoute = &GPIO->I2CROUTE [interfaceId];
    i2cRoute->ROUTEEN = GPIO_I2C_ROUTEEN_SDAPEN |
        GPIO_I2C_ROUTEEN_SCLPEN;
    i2cRoute->SCLROUTE = (sclPin << _GPIO_I2C_SCLROUTE_PIN_SHIFT) |
        (sclPort << _GPIO_I2C_SCLROUTE_PORT_SHIFT);
    i2cRoute->SDAROUTE = (sdaPin << _GPIO_I2C_SDAROUTE_PIN_SHIFT) |
        (sdaPort << _GPIO_I2C_SDAROUTE_PORT_SHIFT);

    // Initialise the selected SDK I2C instance using the default
    // peripheral clock source and 1:1 mark space ratio on SCL. If
    // a bus frequency is not specified, the default value is used.
    i2cInstance = gmosDriverIicPalSdkInstances [interfaceId];
    if (palConfig->iicBusFreq != 0) {
        i2cInit.freq = ((uint32_t) palConfig->iicBusFreq) * 1000;
    }
    I2C_Init (i2cInstance, &i2cInit);

    // Set up NVIC interrupts for the specified I2C interface. Since
    // I2C is a slow interface with no real time requirements, a low
    // interrupt priority level is used.
    NVIC_SetPriority (
        gmosDriverIicPalNvicInts [interfaceId], 0xF0 + interfaceId);

    return true;
}

/*
 * Performs a platform specific IIC transaction using the given IIC
 * interface.
 */
void gmosDriverIicPalTransaction (gmosDriverIicBus_t* iicInterface)
{
    I2C_TransferReturn_TypeDef i2cStatus;
    I2C_TypeDef* i2cInstance;
    I2C_TransferSeq_TypeDef* i2cTransfer;
    uint8_t interfaceId = iicInterface->palConfig->iicInterfaceId;
    gmosDriverIicStatus_t iicStatus;

    // Set the transfer parameters for the SDK low level driver.
    if (!gmosDriverIicPalSetTransferData (iicInterface)) {
        gmosDriverIicPalCompletionHandler (iicInterface,
            GMOS_DRIVER_IIC_STATUS_DRIVER_ERROR, 0);
        return;
    }

    // Select the SDK transfer data structures.
    i2cInstance = gmosDriverIicPalSdkInstances [interfaceId];
    i2cTransfer = &(gmosDriverIicPalSdkTransferData [interfaceId]);

    // Initiate low level SDK transaction.
    i2cStatus = I2C_TransferInit (i2cInstance, i2cTransfer);

    // Check for successful transfer start.
    if (i2cStatus != i2cTransferInProgress) {
        iicStatus = gmosDriverIicPalConvStatus (i2cStatus);
        gmosDriverIicPalCompletionHandler (iicInterface, iicStatus, 0);
        return;
    }

    // Enable the main NVIC interrupt, allowing the low level SDK driver
    // to run from ISR context.
    NVIC_EnableIRQ (gmosDriverIicPalNvicInts [interfaceId]);
}

/*
 * Performs a platform specific IIC inline transaction using the given
 * IIC interface.
 */
gmosDriverIicStatus_t gmosDriverIicPalInlineTransaction
    (gmosDriverIicBus_t* iicInterface)
{
    I2C_TypeDef* i2cInstance;
    I2C_TransferSeq_TypeDef* i2cTransfer;
    I2C_TransferReturn_TypeDef i2cStatus;
    uint32_t timestamp = gmosPalGetTimer () +
        GMOS_MS_TO_TICKS (GMOS_CONFIG_EFR32_IIC_BUS_TIMEOUT);
    uint8_t interfaceId = iicInterface->palConfig->iicInterfaceId;
    gmosDriverIicStatus_t iicStatus;

    // Set the transfer parameters for the SDK low level driver.
    if (!gmosDriverIicPalSetTransferData (iicInterface)) {
        return GMOS_DRIVER_IIC_STATUS_DRIVER_ERROR;
    }

    // Select the SDK transfer data structures.
    i2cInstance = gmosDriverIicPalSdkInstances [interfaceId];
    i2cTransfer = &(gmosDriverIicPalSdkTransferData [interfaceId]);

    // Initiate low level SDK transaction.
    i2cStatus = I2C_TransferInit (i2cInstance, i2cTransfer);

    // Implement polled operation until completion or timeout.
    while (true) {
        int32_t timeout = (int32_t) (gmosPalGetTimer () - timestamp);

        // Check for orderly completion.
        if (i2cStatus != i2cTransferInProgress) {
            iicStatus = gmosDriverIicPalConvStatus (i2cStatus);
            break;
        }

        // Check for transaction timeout.
        else if (timeout > 0) {
            iicStatus = GMOS_DRIVER_IIC_STATUS_TIMEOUT;
            break;
        }

        // Continue polling.
        else {
            i2cStatus = I2C_Transfer (i2cInstance);
        }
    }
    return iicStatus;
}
