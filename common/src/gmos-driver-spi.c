/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2020-2023 Zynaptic Limited
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
 * Implements the common GubbinsMOS SPI driver framework.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-scheduler.h"
#include "gmos-driver-spi.h"
#include "gmos-driver-gpio.h"

// Sets the drive strength for the SPI chip select lines if not defined
// in the platform configuration header.
#ifndef GMOS_CONFIG_SPI_GPIO_DRIVE_STRENGTH
#define GMOS_CONFIG_SPI_GPIO_DRIVE_STRENGTH 0
#endif

/*
 * Initialises a SPI bus interface data structure and initiates the
 * platform specific SPI hardware setup process.
 */
bool gmosDriverSpiBusInit (gmosDriverSpiBus_t* spiInterface)
{
    bool initOk = false;

    // Initialise the SPI interface platform specific hardware.
    if (spiInterface->busState == GMOS_DRIVER_SPI_BUS_RESET) {
        if (gmosDriverSpiPalInit (spiInterface)) {
            spiInterface->busState = GMOS_DRIVER_SPI_BUS_IDLE;
            initOk = true;
        } else {
            spiInterface->busState = GMOS_DRIVER_SPI_BUS_ERROR;
        }
    }
    return initOk;
}

/*
 * Initialises a SPI device data structure with the specified SPI
 * protocol parameters.
 */
bool gmosDriverSpiDeviceInit (gmosDriverSpiDevice_t* spiDevice,
    gmosTaskState_t* clientTask, uint16_t spiChipSelectPin,
    gmosDriverSpiChipSelectOption_t spiChipSelectOptions,
    uint16_t spiClockRate, gmosDriverSpiClockMode_t spiClockMode)
{
    bool csIdleState;
    bool csOutputType;

    // Populate the SPI device data structure.
    spiDevice->spiChipSelectPin = spiChipSelectPin;
    spiDevice->spiChipSelectOptions = spiChipSelectOptions;
    spiDevice->spiClockRate = spiClockRate;
    spiDevice->spiClockMode = spiClockMode;

    // Initialise the completion event data structure.
    gmosEventInit (&(spiDevice->completionEvent), clientTask);

    // Derive the chip select pin options.
    csIdleState = ((spiChipSelectOptions &
        GMOS_DRIVER_SPI_CHIP_SELECT_OPTION_ACTIVE_HIGH) != 0) ?
        false : true;
    csOutputType = ((spiChipSelectOptions &
        GMOS_DRIVER_SPI_CHIP_SELECT_OPTION_OPEN_DRAIN) != 0) ?
        GMOS_DRIVER_GPIO_OUTPUT_OPEN_DRAIN :
        GMOS_DRIVER_GPIO_OUTPUT_PUSH_PULL;

    // Initialise the single chip select output.
    if (!gmosDriverGpioPinInit (spiChipSelectPin, csOutputType,
        GMOS_CONFIG_SPI_GPIO_DRIVE_STRENGTH,
        GMOS_DRIVER_GPIO_INPUT_PULL_NONE)) {
        return false;
    }
    if (!gmosDriverGpioSetAsOutput (spiChipSelectPin)) {
        return false;
    }
    gmosDriverGpioSetPinState (spiChipSelectPin, csIdleState);
    return true;
}

/*
 * Selects a SPI device peripheral connected to the SPI bus. This
 * sets the device specific SPI bus frequency and bus mode then asserts
 * the chip select line at the start of a sequence of low level
 * transactions.
 */
bool gmosDriverSpiDeviceSelect (gmosDriverSpiBus_t* spiInterface,
    gmosDriverSpiDevice_t* spiDevice)
{
    bool selectOk = false;
    bool csActiveState = ((spiDevice->spiChipSelectOptions &
        GMOS_DRIVER_SPI_CHIP_SELECT_OPTION_ACTIVE_HIGH) != 0) ?
        true : false;

    if (spiInterface->busState == GMOS_DRIVER_SPI_BUS_IDLE) {
        spiInterface->busState = GMOS_DRIVER_SPI_BUS_SELECTED;

        // Note that SPI bus clock setup is not required for successive
        // accesses to the same device.
        if (spiInterface->device != spiDevice) {
            spiInterface->device = spiDevice;
            gmosDriverSpiPalClockSetup (spiInterface);
        }
        gmosDriverGpioSetPinState (
            spiDevice->spiChipSelectPin, csActiveState);
        gmosSchedulerStayAwake ();
        selectOk = true;
    }
    return selectOk;
}

/*
 * Releases a SPI device peripheral connected to the SPI bus. This
 * deasserts the chip select line at the end of a sequence of low level
 * transactions.
 */
bool gmosDriverSpiDeviceRelease (gmosDriverSpiBus_t* spiInterface,
    gmosDriverSpiDevice_t* spiDevice)
{
    bool releaseOk = false;
    bool csIdleState = ((spiDevice->spiChipSelectOptions &
        GMOS_DRIVER_SPI_CHIP_SELECT_OPTION_ACTIVE_HIGH) != 0) ?
        false : true;

    if ((spiInterface->busState == GMOS_DRIVER_SPI_BUS_SELECTED) &&
        (spiInterface->device == spiDevice)) {
        spiInterface->busState = GMOS_DRIVER_SPI_BUS_IDLE;
        gmosDriverGpioSetPinState (
            spiDevice->spiChipSelectPin, csIdleState);
        gmosSchedulerCanSleep ();
        releaseOk = true;
    }
    return releaseOk;
}

/*
 * Initiates a SPI write request for a device peripheral connected to
 * the SPI interface.
 */
bool gmosDriverSpiIoWrite (gmosDriverSpiBus_t* spiInterface,
    uint8_t* writeData, uint16_t writeSize)
{
    bool writeOk = false;
    if (spiInterface->busState == GMOS_DRIVER_SPI_BUS_SELECTED) {
        spiInterface->busState = GMOS_DRIVER_SPI_BUS_ACTIVE;
        spiInterface->writeData = writeData;
        spiInterface->readData = NULL;
        spiInterface->transferSize = writeSize;
        gmosDriverSpiPalTransaction (spiInterface);
        writeOk = true;
    }
    return writeOk;
}

/*
 * Initiates a SPI read request for a device peripheral connected to
 * the SPI interface.
 */
bool gmosDriverSpiIoRead (gmosDriverSpiBus_t* spiInterface,
    uint8_t* readData, uint16_t readSize)
{
    bool readOk = false;
    if (spiInterface->busState == GMOS_DRIVER_SPI_BUS_SELECTED) {
        spiInterface->busState = GMOS_DRIVER_SPI_BUS_ACTIVE;
        spiInterface->writeData = NULL;
        spiInterface->readData = readData;
        spiInterface->transferSize = readSize;
        gmosDriverSpiPalTransaction (spiInterface);
        readOk = true;
    }
    return readOk;
}

/*
 * Initiates a SPI bidirectional transfer request for a device
 * peripheral connected to the SPI interface.
 */
bool gmosDriverSpiIoTransfer (gmosDriverSpiBus_t* spiInterface,
    uint8_t* writeData, uint8_t* readData, uint16_t transferSize)
{
    bool transferOk = false;
    if (spiInterface->busState == GMOS_DRIVER_SPI_BUS_SELECTED) {
        spiInterface->busState = GMOS_DRIVER_SPI_BUS_ACTIVE;
        spiInterface->writeData = writeData;
        spiInterface->readData = readData;
        spiInterface->transferSize = transferSize;
        gmosDriverSpiPalTransaction (spiInterface);
        transferOk = true;
    }
    return transferOk;
}

/*
 * Completes an asynchronous SPI transaction for a device peripheral
 * connected to the SPI interface.
 */
gmosDriverSpiStatus_t gmosDriverSpiIoComplete
    (gmosDriverSpiBus_t* spiInterface, uint16_t* transferSize)
{
    uint32_t eventBits;
    gmosEvent_t* completionEvent;
    gmosDriverSpiStatus_t spiStatus = GMOS_DRIVER_SPI_STATUS_IDLE;

    // Only poll the completion event if a transaction is active.
    if (spiInterface->busState == GMOS_DRIVER_SPI_BUS_ACTIVE) {
        completionEvent = &(spiInterface->device->completionEvent);
        eventBits = gmosEventResetBits (completionEvent);
        if (eventBits != 0) {
            spiInterface->busState = GMOS_DRIVER_SPI_BUS_SELECTED;
            spiStatus = eventBits & GMOS_DRIVER_SPI_EVENT_STATUS_MASK;

            // Transfer size notifications are optional.
            if (transferSize != NULL) {
                *transferSize =
                    (eventBits & GMOS_DRIVER_SPI_EVENT_SIZE_MASK) >>
                    GMOS_DRIVER_SPI_EVENT_SIZE_OFFSET;
            }
        } else {
            spiStatus = GMOS_DRIVER_SPI_STATUS_ACTIVE;
        }
    }
    return spiStatus;
}

/*
 * Requests an inline SPI write data transfer for short transactions
 * where the overhead of setting up an asynchronous transfer is likely
 * to exceed the cost of carrying out a simple polled transaction.
 */
gmosDriverSpiStatus_t gmosDriverSpiIoInlineWrite
    (gmosDriverSpiBus_t* spiInterface, uint8_t* writeData,
    uint16_t writeSize)
{
    gmosDriverSpiStatus_t spiStatus = GMOS_DRIVER_SPI_STATUS_NOT_READY;
    if (spiInterface->busState == GMOS_DRIVER_SPI_BUS_SELECTED) {
        spiInterface->writeData = writeData;
        spiInterface->readData = NULL;
        spiInterface->transferSize = writeSize;
        spiStatus = gmosDriverSpiPalInlineTransaction (spiInterface);
    }
    return spiStatus;
}

/*
 * Requests an inline SPI read data transfer for short transactions
 * where the overhead of setting up an asynchronous transfer is likely
 * to exceed the cost of carrying out a simple polled transaction.
 */
gmosDriverSpiStatus_t gmosDriverSpiIoInlineRead
    (gmosDriverSpiBus_t* spiInterface, uint8_t* readData,
    uint16_t readSize)
{
    gmosDriverSpiStatus_t spiStatus = GMOS_DRIVER_SPI_STATUS_NOT_READY;
    if (spiInterface->busState == GMOS_DRIVER_SPI_BUS_SELECTED) {
        spiInterface->writeData = NULL;
        spiInterface->readData = readData;
        spiInterface->transferSize = readSize;
        spiStatus = gmosDriverSpiPalInlineTransaction (spiInterface);
    }
    return spiStatus;
}

/*
 * Requests a bidirectional inline SPI data transfer for short
 * transactions where the overhead of setting up an asynchronous
 * transfer is likely to exceed the cost of carrying out a simple polled
 * transaction.
 */
gmosDriverSpiStatus_t gmosDriverSpiIoInlineTransfer
    (gmosDriverSpiBus_t* spiInterface, uint8_t* writeData,
    uint8_t* readData, uint16_t transferSize)
{
    gmosDriverSpiStatus_t spiStatus = GMOS_DRIVER_SPI_STATUS_NOT_READY;
    if (spiInterface->busState == GMOS_DRIVER_SPI_BUS_SELECTED) {
        spiInterface->writeData = writeData;
        spiInterface->readData = readData;
        spiInterface->transferSize = transferSize;
        spiStatus = gmosDriverSpiPalInlineTransaction (spiInterface);
    }
    return spiStatus;
}
