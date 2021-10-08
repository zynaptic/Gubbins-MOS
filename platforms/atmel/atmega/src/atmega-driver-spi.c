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
 * Implements SPI bus controller functionality for the Microchip/Atmel
 * ATMEGA series of microcontrollers.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-driver-spi.h"
#include "gmos-driver-gpio.h"
#include "atmega-device.h"
#include "atmega-driver-spi.h"
#include "atmega-driver-gpio.h"

// Store a local reference to the SPI interface data for the single
// SPI interface.
static gmosDriverSpiBus_t* spiInterfaceData = NULL;

/*
 * Transfer the next byte in the current SPI transfer.
 */
static bool gmosDriverSpiPalTransfer (gmosDriverSpiBus_t* spiInterface)
{
    gmosPalSpiBusState_t* spiState = spiInterface->palData;
    uint16_t transferCount = spiState->transferCount;
    uint8_t* rxDataBuffer = spiInterface->readData;
    uint8_t* txDataBuffer = spiInterface->writeData;
    uint8_t rxDataByte;
    uint8_t txDataByte;
    bool spiActive = false;

    // If this is not the first transfer, data can be read from the
    // SPI read data register.
    if (transferCount > 0) {
        rxDataByte = SPDR;
        if (rxDataBuffer != NULL) {
            rxDataBuffer [transferCount - 1] = rxDataByte;
        }
    }

    // If this is not the last transfer, data can be written to the
    // SPI data register.
    if (transferCount < spiInterface->transferSize) {
        if (txDataBuffer != NULL) {
            txDataByte = txDataBuffer [transferCount];
        } else {
            txDataByte = 0xFF;
        }
        SPDR = txDataByte;

        // Update the transfer count for the next transfer cycle.
        spiState->transferCount = transferCount + 1;
        spiActive = true;
    }

    // Indicate that an active transfer has been initiated.
    return spiActive;
}

/*
 * On transfer completion, disable the SPI interface and signal the
 * completion event.
 */
static void gmosDriverSpiPalComplete (gmosDriverSpiBus_t* spiInterface)
{
    // Set the GubbinsMOS event flags to indicate successful completion.
    uint32_t eventFlags = spiInterfaceData->transferSize;
    eventFlags <<= GMOS_DRIVER_SPI_EVENT_SIZE_OFFSET;
    eventFlags |= GMOS_DRIVER_SPI_EVENT_COMPLETION_FLAG |
        GMOS_DRIVER_SPI_STATUS_SUCCESS;

    // Disable the SPI interface and send the completion event.
    SPCR &= ~((1 << SPIE) | (1 << SPE));
    gmosEventSetBits (&(spiInterfaceData->device->completionEvent), eventFlags);
}

/*
 * Implement SPI data transfer complete interrupt.
 */
ISR (SPI_STC_vect)
{
    if (!gmosDriverSpiPalTransfer (spiInterfaceData)) {
        gmosDriverSpiPalComplete (spiInterfaceData);
    }
}

/*
 * Initialises the platform abstraction layer for the ATMEGA SPI
 * interface.
 */
bool gmosDriverSpiPalInit (gmosDriverSpiBus_t* spiInterface)
{
    bool initOk = true;

    // Check that the interface is not already configured.
    if (spiInterfaceData != NULL) {
        return false;
    } else {
        spiInterfaceData = spiInterface;
    }

    // Configure the SPI interface pins.
    initOk &= gmosDriverGpioPinInit (
        ATMEGA_SPI_PIN_MOSI, GMOS_DRIVER_GPIO_OUTPUT_PUSH_PULL,
        ATMEGA_GPIO_DRIVER_SLEW_FIXED, GMOS_DRIVER_GPIO_INPUT_PULL_NONE);
    initOk &= gmosDriverGpioSetAsOutput (ATMEGA_SPI_PIN_MOSI);

    initOk &= gmosDriverGpioPinInit (
        ATMEGA_SPI_PIN_MISO, GMOS_DRIVER_GPIO_OUTPUT_PUSH_PULL,
        ATMEGA_GPIO_DRIVER_SLEW_FIXED, GMOS_DRIVER_GPIO_INPUT_PULL_NONE);
    initOk &= gmosDriverGpioSetAsInput (ATMEGA_SPI_PIN_MISO);

    initOk &= gmosDriverGpioPinInit (
        ATMEGA_SPI_PIN_SCLK, GMOS_DRIVER_GPIO_OUTPUT_PUSH_PULL,
        ATMEGA_GPIO_DRIVER_SLEW_FIXED, GMOS_DRIVER_GPIO_INPUT_PULL_NONE);
    initOk &= gmosDriverGpioSetAsOutput (ATMEGA_SPI_PIN_SCLK);

    // Configure the SPI interface with default settings.
    if (initOk) {
        SPCR = (1 << MSTR);
    }
    return initOk;
}

/*
 * Sets up the platform abstraction layer for one or more SPI
 * transactions that share the same SPI clock configuration.
 */
void gmosDriverSpiPalClockSetup (gmosDriverSpiBus_t* spiInterface)
{
    gmosDriverSpiDevice_t* spiDevice = spiInterface->device;
    uint32_t spiClockRequest;
    uint32_t spiClockFreq;
    uint8_t clockDiv;
    uint8_t regValue;
    uint8_t spi2xValue;

    // Select the initial SPI clock rate.
    spiClockFreq = GMOS_CONFIG_ATMEGA_SYSTEM_CLOCK / 2;

    // Select the closest SPI clock scaling to the one requested.
    spiClockRequest = 1000 * (uint32_t) (spiDevice->spiClockRate);
    for (clockDiv = 0; clockDiv < 6; clockDiv++) {
        if (spiClockFreq <= spiClockRequest) {
            break;
        } else {
            spiClockFreq /= 2;
        }
    }

    // Clear all the clock configuration bits.
    regValue = SPCR &
        ~((1 << SPR0) | (1 << SPR1) | (1 << CPHA) | (1 << CPOL));

    // The 128 clock divider is a special case.
    if (clockDiv == 6) {
        regValue |= (1 << SPR0) | (1 << SPR1);
        spi2xValue = 0;
    }

    // Other clock divider settings can be derived directly.
    else {
        regValue |= ((clockDiv >> 1) & 3) << SPR0;
        spi2xValue = (~clockDiv) & 1;
    }

    // The standard clock phase encoding can be loaded directly into the
    // clock phase and polarity bits.
    regValue |= ((spiDevice->spiClockMode) & 3) << CPHA;

    // Update the register clock settings. Note that only bit 0 of SPSR
    // is writeable. All other bits are written as zero.
    SPCR = regValue;
    SPSR = spi2xValue;
}

/*
 * Performs a platform specific SPI transaction using the given SPI
 * interface settings.
 */
void gmosDriverSpiPalTransaction (gmosDriverSpiBus_t* spiInterface)
{
    gmosPalSpiBusState_t* spiState = spiInterface->palData;

    // Initialise the SPI interface state.
    spiState->transferCount = 0;

    // Enable the SPI interface and associated interrupt.
    SPCR |= (1 << SPIE) | (1 << SPE);

    // Initiate transfer. This completes immediately for zero length
    // transfer requests.
    if (!gmosDriverSpiPalTransfer (spiInterface)) {
        gmosDriverSpiPalComplete (spiInterface);
    }
}

/*
 * Performs a platform specific SPI inline transaction using the given
 * SPI interface.
 */
gmosDriverSpiStatus_t gmosDriverSpiPalInlineTransaction
    (gmosDriverSpiBus_t* spiInterface)
{
    gmosPalSpiBusState_t* spiState = spiInterface->palData;
    bool spiActive;

    // Enable the SPI interface without interrupts and initate SPI
    // data transfer.
    SPCR |= (1 << SPE);
    spiState->transferCount = 0;
    spiActive = gmosDriverSpiPalTransfer (spiInterface);

    // Transfer additional bytes while the SPI interface is active.
    // This implements a busy waiting loop on the SPIF bit.
    while (spiActive) {
        while ((SPSR & (1 << SPIF)) == 0);
        spiActive = gmosDriverSpiPalTransfer (spiInterface);
    }

    // Disable the SPI interface and indication completion status.
    SPCR &= ~(1 << SPE);
    return GMOS_DRIVER_SPI_STATUS_SUCCESS;
}
