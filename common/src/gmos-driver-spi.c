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
 * Initialises a SPI interface to use as a simple point to point link,
 * with the microcontroller as the SPI bus controller and a single
 * attached SPI device peripheral.
 */
bool gmosDriverSpiLinkInit (gmosDriverSpiIo_t* spiInterface,
    gmosTaskState_t* clientTask, uint16_t spiChipSelectPin,
    uint16_t spiClockRate, uint8_t spiClockMode)
{
    // Populate the SPI interface data structure.
    spiInterface->linkState = GMOS_DRIVER_SPI_LINK_IDLE;
    spiInterface->spiChipSelectPin = spiChipSelectPin;
    spiInterface->spiClockRate = spiClockRate;
    spiInterface->spiClockMode = spiClockMode;

    // Initialise the completion event data structure.
    gmosEventInit (&(spiInterface->completionEvent), clientTask);

    // Initialise the single chip select output.
    if (!gmosDriverGpioPinInit (spiInterface->spiChipSelectPin,
        GMOS_DRIVER_GPIO_OUTPUT_PUSH_PULL,
        GMOS_CONFIG_SPI_GPIO_DRIVE_STRENGTH,
        GMOS_DRIVER_GPIO_INPUT_PULL_NONE)) {
        return false;
    }
    if (!gmosDriverGpioSetAsOutput (spiInterface->spiChipSelectPin)) {
        return false;
    }
    gmosDriverGpioSetPinState (spiInterface->spiChipSelectPin, 1);

    // Run the platform specific initialisation.
    if (!gmosDriverSpiPalInit (spiInterface)) {
        return false;
    }

    // Perform clock setup for the lifetime of the SPI link.
    gmosDriverSpiPalClockSetup (spiInterface);
    return true;
}

/*
 * Selects a SPI device peripheral connected to the SPI interface using
 * a simple point to point link. This asserts the chip select line at
 * the start of a sequence of low level transactions.
 */
bool gmosDriverSpiLinkSelect (gmosDriverSpiIo_t* spiInterface)
{
    bool selectOk = false;
    if (spiInterface->linkState == GMOS_DRIVER_SPI_LINK_IDLE) {
        spiInterface->linkState = GMOS_DRIVER_SPI_LINK_SELECTED;
        gmosDriverGpioSetPinState (spiInterface->spiChipSelectPin, 0);
        gmosSchedulerStayAwake ();
        selectOk = true;
    }
    return selectOk;
}

/*
 * Releases a SPI device peripheral connected to the SPI interface using
 * a simple point to point link. This deasserts the chip select line at
 * the end of a sequence of low level transactions.
 */
bool gmosDriverSpiLinkRelease (gmosDriverSpiIo_t* spiInterface)
{
    bool releaseOk = false;
    if (spiInterface->linkState == GMOS_DRIVER_SPI_LINK_SELECTED) {
        spiInterface->linkState = GMOS_DRIVER_SPI_LINK_IDLE;
        gmosDriverGpioSetPinState (spiInterface->spiChipSelectPin, 1);
        gmosSchedulerCanSleep ();
        releaseOk = true;
    }
    return releaseOk;
}

/*
 * Initiates a SPI write request for a device peripheral connected to
 * the SPI interface.
 */
bool gmosDriverSpiIoWrite (gmosDriverSpiIo_t* spiInterface,
    uint8_t* writeData, uint16_t writeSize)
{
    bool writeOk = false;
    if (spiInterface->linkState == GMOS_DRIVER_SPI_LINK_SELECTED) {
        spiInterface->linkState = GMOS_DRIVER_SPI_LINK_ACTIVE;
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
bool gmosDriverSpiIoRead (gmosDriverSpiIo_t* spiInterface,
    uint8_t* readData, uint16_t readSize)
{
    bool readOk = false;
    if (spiInterface->linkState == GMOS_DRIVER_SPI_LINK_SELECTED) {
        spiInterface->linkState = GMOS_DRIVER_SPI_LINK_ACTIVE;
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
bool gmosDriverSpiIoTransfer (gmosDriverSpiIo_t* spiInterface,
    uint8_t* writeData, uint8_t* readData, uint16_t transferSize)
{
    bool transferOk = false;
    if (spiInterface->linkState == GMOS_DRIVER_SPI_LINK_SELECTED) {
        spiInterface->linkState = GMOS_DRIVER_SPI_LINK_ACTIVE;
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
    (gmosDriverSpiIo_t* spiInterface, uint16_t* transferSize)
{
    uint32_t eventBits;
    gmosDriverSpiStatus_t spiStatus = GMOS_DRIVER_SPI_STATUS_IDLE;

    // Only poll the completion event if a transaction is active.
    if (spiInterface->linkState == GMOS_DRIVER_SPI_LINK_ACTIVE) {
        eventBits = gmosEventGetBits (&spiInterface->completionEvent);
        if ((eventBits & GMOS_DRIVER_SPI_EVENT_COMPLETION_FLAG) != 0) {
            spiInterface->linkState = GMOS_DRIVER_SPI_LINK_SELECTED;
            gmosEventClearBits (&spiInterface->completionEvent, 0xFFFFFFFF);
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
    (gmosDriverSpiIo_t* spiInterface, uint8_t* writeData,
    uint16_t writeSize)
{
    gmosDriverSpiStatus_t spiStatus = GMOS_DRIVER_SPI_STATUS_NOT_READY;
    if (spiInterface->linkState == GMOS_DRIVER_SPI_LINK_SELECTED) {
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
   (gmosDriverSpiIo_t* spiInterface, uint8_t* readData,
   uint16_t readSize)
{
    gmosDriverSpiStatus_t spiStatus = GMOS_DRIVER_SPI_STATUS_NOT_READY;
    if (spiInterface->linkState == GMOS_DRIVER_SPI_LINK_SELECTED) {
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
    (gmosDriverSpiIo_t* spiInterface, uint8_t* writeData,
    uint8_t* readData, uint16_t transferSize)
{
    gmosDriverSpiStatus_t spiStatus = GMOS_DRIVER_SPI_STATUS_NOT_READY;
    if (spiInterface->linkState == GMOS_DRIVER_SPI_LINK_SELECTED) {
        spiInterface->writeData = writeData;
        spiInterface->readData = readData;
        spiInterface->transferSize = transferSize;
        spiStatus = gmosDriverSpiPalInlineTransaction (spiInterface);
    }
    return spiStatus;
}
