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
 * This file provides the platform specific implementation of the
 * Raspberry Pi RP2040 SPI driver.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-events.h"
#include "gmos-driver-spi.h"
#include "gmos-driver-gpio.h"
#include "pico-device.h"
#include "pico-driver-spi.h"
#include "pico-driver-gpio.h"
#include "hardware/dma.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"

// Provide reverse mapping of SPI interface IDs to bus state data
// structures.
static gmosDriverSpiBus_t* spiBusStateMap [] = { NULL, NULL };

// Fake DMA data source for use in receive only transactions.
static uint8_t dmaFakeSource = 0xFF;

// Fake DMA data target for use in transmit only transactions.
static uint8_t dmaFakeTarget;

/*
 * Common DMA ISR callback on completion of receive transaction.
 */
static uint8_t gmosDriverSpiPalIsrCommon
    (gmosDriverSpiBus_t* spiInterface)
{
    gmosPalSpiBusState_t* palData = spiInterface->palData;
    uint8_t dmaRxChannel = palData->dmaRxChannel;
    gmosEvent_t* completionEvent;
    uint32_t eventFlags;

    // Always indicate successful completion.
    eventFlags = spiInterface->transferSize;
    eventFlags <<= GMOS_DRIVER_SPI_EVENT_SIZE_OFFSET;
    eventFlags |= GMOS_DRIVER_SPI_EVENT_COMPLETION_FLAG |
        GMOS_DRIVER_SPI_STATUS_SUCCESS;

    // Set the GubbinsMOS event flags.
    completionEvent = &(spiInterface->device->completionEvent);
    gmosEventSetBits (completionEvent, eventFlags);

    // Disable the receive channel interrupt.
    gmosPalDmaIsrSetEnabled (dmaRxChannel, false);

    // Clear all interrupts, regardless of status.
    return true;
}

/*
 * SPI0 DMA ISR callback on completion of receive transaction.
 */
static bool gmosDriverSpiPalIsrSpi0 (void)
{
    return gmosDriverSpiPalIsrCommon (spiBusStateMap [0]);
}

/*
 * SPI1 DMA ISR callback on completion of receive transaction.
 */
static bool gmosDriverSpiPalIsrSpi1 (void)
{
    return gmosDriverSpiPalIsrCommon (spiBusStateMap [1]);
}

/*
 * Initialises the platform abstraction layer for a given SPI interface.
 */
bool gmosDriverSpiPalInit (gmosDriverSpiBus_t* spiInterface)
{
    const gmosPalSpiBusConfig_t* palConfig = spiInterface->palConfig;
    gmosPalSpiBusState_t* palData = spiInterface->palData;
    uint8_t spiIndex;
    int32_t dmaTxAlloc;
    int32_t dmaRxAlloc;
    dma_channel_config dmaTxConfig;
    dma_channel_config dmaRxConfig;
    spi_inst_t* spiInst;

    // Check for available SPI interface on target device.
    spiIndex = palConfig->spiInterfaceId;
    if ((spiIndex >= 2) || (spiBusStateMap [spiIndex] != NULL)) {
        return false;
    }

    // Attempt to allocate two DMA channels for SPI interface use.
    dmaTxAlloc = dma_claim_unused_channel (false);
    dmaRxAlloc = dma_claim_unused_channel (false);
    if ((dmaTxAlloc < 0) || (dmaRxAlloc < 0)) {
        return false;
    }
    palData->dmaTxChannel = dmaTxAlloc;
    palData->dmaRxChannel = dmaRxAlloc;

    // Attempt to register the DMA ISR for the received data DMA.
    if (spiIndex == 0) {
        spiInst = spi0;
        if (!gmosPalDmaIsrAttach (dmaRxAlloc, gmosDriverSpiPalIsrSpi0)) {
            return false;
        }
    } else {
        spiInst = spi1;
        if (!gmosPalDmaIsrAttach (dmaRxAlloc, gmosDriverSpiPalIsrSpi1)) {
            return false;
        }
    }

    // Keep a reference to the platform data structures.
    spiBusStateMap [spiIndex] = spiInterface;
    palData->spiInst = spiInst;

    // Configure the SPI I/O as GPIO alternate functions.
    gmosDriverGpioAltModeInit (palConfig->sclkPinId,
        GMOS_CONFIG_SPI_GPIO_DRIVE_STRENGTH,
        GMOS_DRIVER_GPIO_INPUT_PULL_NONE, GPIO_FUNC_SPI);
    gmosDriverGpioAltModeInit (palConfig->mosiPinId,
        GMOS_CONFIG_SPI_GPIO_DRIVE_STRENGTH,
        GMOS_DRIVER_GPIO_INPUT_PULL_NONE, GPIO_FUNC_SPI);
    gmosDriverGpioAltModeInit (palConfig->misoPinId,
        GMOS_DRIVER_GPIO_SLEW_MINIMUM,
        GMOS_DRIVER_GPIO_INPUT_PULL_NONE, GPIO_FUNC_SPI);

    // Enable the SPI interface using the SDK. An arbitrary initial
    // baud rate of 100 kHz is used during configuration.
    spi_init (spiInst, 100 * 1000);

    // Configure DMA transmit channel, setting the standard transfer
    // size as 8-bits, with flow control from the SPI interface.
    dmaTxConfig = dma_channel_get_default_config (dmaTxAlloc);
    channel_config_set_transfer_data_size (&dmaTxConfig, DMA_SIZE_8);
    channel_config_set_read_increment (&dmaTxConfig, true);
    channel_config_set_write_increment (&dmaTxConfig, false);
    channel_config_set_dreq (&dmaTxConfig, spi_get_dreq (spiInst, true));
    dma_channel_set_config (dmaTxAlloc, &dmaTxConfig, false);
    dma_channel_set_write_addr (dmaTxAlloc, &(spi_get_hw (spiInst)->dr), false);

    // Configure DMA receive channel, setting the standard transfer
    // size as 8-bits, with flow control from the SPI interface.
    dmaRxConfig = dma_channel_get_default_config (dmaRxAlloc);
    channel_config_set_transfer_data_size (&dmaRxConfig, DMA_SIZE_8);
    channel_config_set_read_increment (&dmaRxConfig, false);
    channel_config_set_write_increment (&dmaRxConfig, true);
    channel_config_set_dreq (&dmaRxConfig, spi_get_dreq (spiInst, false));
    dma_channel_set_config (dmaRxAlloc, &dmaRxConfig, false);
    dma_channel_set_read_addr (dmaRxAlloc, &(spi_get_hw (spiInst)->dr), false);

    return true;
}

/*
 * Sets up the platform abstraction layer for one or more SPI
 * transactions that share the same SPI clock configuration.
 */
void gmosDriverSpiPalClockSetup (gmosDriverSpiBus_t* spiInterface)
{
    gmosPalSpiBusState_t* palData = spiInterface->palData;
    gmosDriverSpiDevice_t* spiDevice = spiInterface->device;
    spi_inst_t* spiInst = (spi_inst_t*) palData->spiInst;
    uint32_t spiClockRequest;
    uint32_t spiClockFreq;
    spi_cpol_t clkPolarity;
    spi_cpha_t clkPhase;

    // Select the closest SPI clock scaling to the one requested.
    spiClockRequest = 1000 * (uint32_t) (spiDevice->spiClockRate);
    spiClockFreq = spi_set_baudrate (spiInst, spiClockRequest);
    if (spiClockRequest != spiClockFreq) {
        GMOS_LOG_FMT (LOG_VERBOSE,
            "Requested SPI clock %d Hz, using closest option %d Hz",
            spiClockRequest, spiClockFreq);
    }

    // Select the SPI transfer format to use. Only 8-bit transfers are
    // currently supported.
    switch (spiDevice->spiClockMode) {
        case 0 :
            clkPolarity = SPI_CPOL_0;
            clkPhase = SPI_CPHA_0;
            break;
        case 1 :
            clkPolarity = SPI_CPOL_0;
            clkPhase = SPI_CPHA_1;
            break;
        case 2 :
            clkPolarity = SPI_CPOL_1;
            clkPhase = SPI_CPHA_0;
            break;
        default :
            clkPolarity = SPI_CPOL_1;
            clkPhase = SPI_CPHA_1;
            break;
    }
    spi_set_format (spiInst, 8, clkPolarity, clkPhase, SPI_MSB_FIRST);
}

/*
 * Performs a platform specific SPI transaction using the given SPI
 * interface settings.
 */
void gmosDriverSpiPalTransaction (gmosDriverSpiBus_t* spiInterface)
{
    gmosPalSpiBusState_t* palData = spiInterface->palData;
    uint8_t dmaTxChannel = palData->dmaTxChannel;
    uint8_t dmaRxChannel = palData->dmaRxChannel;
    uint16_t transferSize = spiInterface->transferSize;
    uint8_t* dmaTxAddr;
    uint8_t* dmaRxAddr;
    dma_channel_config dmaTxConfig;
    dma_channel_config dmaRxConfig;

    // Configure the DMA transmit channel.
    dmaTxConfig = dma_get_channel_config (dmaTxChannel);
    if (spiInterface->writeData == NULL) {
        dmaTxAddr = &dmaFakeSource;
        channel_config_set_read_increment (&dmaTxConfig, false);
    } else {
        dmaTxAddr = spiInterface->writeData;
        channel_config_set_read_increment (&dmaTxConfig, true);
    }
    dma_channel_set_read_addr (dmaTxChannel, dmaTxAddr, false);
    dma_channel_set_trans_count (dmaTxChannel, transferSize, false);
    dma_channel_set_config (dmaTxChannel, &dmaTxConfig, false);

    // Configure the DMA receive channel.
    dmaRxConfig = dma_get_channel_config (dmaRxChannel);
    if (spiInterface->readData == NULL) {
        dmaRxAddr = &dmaFakeTarget;
        channel_config_set_write_increment (&dmaRxConfig, false);
    } else {
        dmaRxAddr = spiInterface->readData;
        channel_config_set_write_increment (&dmaRxConfig, true);
    }
    dma_channel_set_write_addr (dmaRxChannel, dmaRxAddr, false);
    dma_channel_set_trans_count (dmaRxChannel, transferSize, false);
    dma_channel_set_config (dmaRxChannel, &dmaRxConfig, false);

    // Enable the receive channel interrupt. This should always be the
    // last to complete.
    gmosPalDmaIsrSetEnabled (dmaRxChannel, true);

    // Initiate both DMA transfers at the same time.
    dma_start_channel_mask ((1 << dmaTxChannel) | (1 << dmaRxChannel));
}

/*
 * Performs a platform specific SPI inline transaction using the given
 * SPI interface.
 */
gmosDriverSpiStatus_t gmosDriverSpiPalInlineTransaction
    (gmosDriverSpiBus_t* spiInterface)
{
    gmosPalSpiBusState_t* palData = spiInterface->palData;
    spi_inst_t* spiInst = (spi_inst_t*) palData->spiInst;
    uint32_t transferSize = spiInterface->transferSize;
    uint8_t* txDataAddr = spiInterface->writeData;
    uint8_t* rxDataAddr = spiInterface->readData;
    int32_t transferResult = -1;

    // Perform combined write and read.
    if ((txDataAddr != NULL) && (rxDataAddr != NULL)) {
        transferResult = spi_write_read_blocking (spiInst,
            txDataAddr, rxDataAddr, transferSize);
    }

    // Perform write only.
    else if (txDataAddr != NULL) {
        transferResult = spi_write_blocking (
            spiInst, txDataAddr, transferSize);
    }

    // Perform read only.
    else if (rxDataAddr != NULL) {
        transferResult = spi_read_blocking (
            spiInst, 0xFF, rxDataAddr, transferSize);
    }

    // The number of transferred bytes should always match the requsted
    // value.
    if (transferResult == transferSize) {
        return GMOS_DRIVER_SPI_STATUS_SUCCESS;
    } else {
        return GMOS_DRIVER_SPI_STATUS_DRIVER_ERROR;
    }
}
