/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2020 Zynaptic Limited
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
 * Implements SPI bus controller functionality for the STM32L0XX series
 * of microcontrollers.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-events.h"
#include "gmos-driver-spi.h"
#include "gmos-driver-gpio.h"
#include "stm32-device.h"
#include "stm32-driver-spi.h"
#include "stm32-driver-gpio.h"

// Add dummy definitions if SPI interface 2 is not supported.
#ifndef SPI2
#define SPI2 NULL
#define SPI2_IRQn 0
#define RCC_APB1ENR_SPI2EN 0
#endif

// Provide mapping of SPI interface IDs to register sets.
static SPI_TypeDef* spiRegisterMap [] = {SPI1, SPI2};

// Provide mapping of SPI interface IDs to DMA register sets.
static DMA_Channel_TypeDef* dmaTxRegisterMap [] =
    {DMA1_Channel3, DMA1_Channel7};
static DMA_Channel_TypeDef* dmaRxRegisterMap [] =
    {DMA1_Channel2, DMA1_Channel6};

// Provide reverse mapping of SPI interface IDs to bus state data
// structures.
static gmosDriverSpiIo_t* spiIoStateMap [] = {NULL, NULL};

// Fake DMA data source for use in receive only transactions.
static uint8_t dmaFakeSource = 0xFF;

// Fake DMA data target for use in transmit only transactions.
static uint8_t dmaFakeTarget;

/*
 * Common DMA ISR callback on completion of receive transaction. Note
 * that no checking is carried out on the transmit channel since all DMA
 * transfers should be using valid addresses.
 */
static uint8_t gmosDriverSpiPalIsrCommon
    (gmosDriverSpiIo_t* spiInterface, uint8_t isrFlags)
{
    const gmosPalSpiIoConfig_t* palConfig = spiInterface->palConfig;
    uint8_t spiIndex = palConfig->spiInterfaceId - 1;
    SPI_TypeDef* spiRegs = spiRegisterMap [spiIndex];
    DMA_Channel_TypeDef* dmaTxRegs = dmaTxRegisterMap [spiIndex];
    DMA_Channel_TypeDef* dmaRxRegs = dmaRxRegisterMap [spiIndex];
    uint32_t eventFlags = 0;

    // Check for error condition. Note that all flags are shifted into
    // the channel 1 bit positions.
    if ((isrFlags & DMA_ISR_TEIF1) != 0) {
        eventFlags = GMOS_DRIVER_SPI_EVENT_COMPLETION_FLAG |
            GMOS_DRIVER_SPI_STATUS_DMA_ERROR;
    }

    // Check for sucessful completion.
    if ((isrFlags & DMA_ISR_TCIF1) != 0) {
        eventFlags = spiInterface->transferSize;
        eventFlags <<= GMOS_DRIVER_SPI_EVENT_SIZE_OFFSET;
        eventFlags |= GMOS_DRIVER_SPI_EVENT_COMPLETION_FLAG |
            GMOS_DRIVER_SPI_STATUS_SUCCESS;
    }

    // Disable SPI interface and signal completion if required.
    if (eventFlags != 0) {
        dmaTxRegs->CCR &= ~DMA_CCR_EN;
        dmaRxRegs->CCR &= ~DMA_CCR_EN;

        // Busy waiting on TX empty and TX/RX busy should complete
        // immediately in normal operation.
        while ((spiRegs->SR & SPI_SR_TXE) == 0) {
            // Wait for transmit buffer empty.
        }
        while ((spiRegs->SR & SPI_SR_BSY) != 0) {
            // Wait for SPI bus activity to complete.
        }

        // Disable the SPI interface and associated DMA requests.
        spiRegs->CR1 &= ~SPI_CR1_SPE;
        spiRegs->CR2 &= ~(SPI_CR2_TXDMAEN | SPI_CR2_RXDMAEN);

        // Set the GubbinsMOS event flags.
        gmosEventSetBits (&(spiInterface->completionEvent), eventFlags);
    }

    // Clear all interrupts, regardless of status.
    return 0x0F;
}

/*
 * SPI1 DMA ISR callback on completion of receive transaction.
 */
#if GMOS_CONFIG_STM32_SPI_USE_INTERRUPTS
static uint8_t gmosDriverSpiPalIsrSpi1 (uint8_t isrFlags)
{
    return gmosDriverSpiPalIsrCommon (spiIoStateMap [0], isrFlags);
}
#endif

/*
 * SPI2 DMA ISR callback on completion of receive transaction.
 */
#if GMOS_CONFIG_STM32_SPI_USE_INTERRUPTS
static uint8_t gmosDriverSpiPalIsrSpi2 (uint8_t isrFlags)
{
    return gmosDriverSpiPalIsrCommon (spiIoStateMap [1], isrFlags);
}
#endif

/*
 * Implement a task based polling loop if interrupt driven operation is
 * not selected. This will typically be used for debug purposes only.
 */
#if !GMOS_CONFIG_STM32_SPI_USE_INTERRUPTS
static bool pollingLoopRunning = false;
static gmosTaskState_t pollingLoopTaskData;

static gmosTaskStatus_t pollingLoopTaskFn (void* nullData)
{
    uint8_t spiIndex;
    uint8_t isrFlags;
    uint8_t isrClear;
    gmosDriverSpiIo_t* spiInterface;
    // gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_BACKGROUND;
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_AFTER (1000);

    // Loop over the active SPI interfaces.
    for (spiIndex = 0; spiIndex < 2; spiIndex++) {
        spiInterface = spiIoStateMap [spiIndex];
        if (spiInterface != NULL) {
            if (spiInterface->linkState == GMOS_DRIVER_SPI_LINK_ACTIVE) {

                // Get the interrupt status flags for the appropriate
                // receive DMA channel.
                if (spiIndex == 0) {
                    isrFlags = (DMA1->ISR >> ((2 - 1) * 4)) & 0x0F;
                } else {
                    isrFlags = (DMA1->ISR >> ((6 - 1) * 4)) & 0x0F;
                }

                // Run the common ISR if required.
                if (isrFlags != 0) {
                    isrClear = gmosDriverSpiPalIsrCommon (spiInterface, isrFlags);

                    // Clear the requested DMA interrupt status flags.
                    if (spiIndex == 0) {
                        DMA1->IFCR = ((0x0F & isrClear) << ((2 - 1) * 4));
                    } else {
                        DMA1->IFCR = ((0x0F & isrClear) << ((6 - 1) * 4));
                    }
                    taskStatus = GMOS_TASK_RUN_IMMEDIATE;
                }
            }
        }
    }
    return taskStatus;
}

GMOS_TASK_DEFINITION (pollingLoopTask, pollingLoopTaskFn, void);
#endif

/*
 * Initialises the platform abstraction layer for a given SPI interface.
 * Refer to the platform specific SPI implementation for details of the
 * platform data area and the SPI interface configuration options.
 */
bool gmosDriverSpiPalInit (gmosDriverSpiIo_t* spiInterface)
{
    const gmosPalSpiIoConfig_t* palConfig = spiInterface->palConfig;
    uint8_t spiIndex;
    uint32_t regValue;

    // Check for supported SPI interface on target device.
    spiIndex = palConfig->spiInterfaceId - 1;
    if ((spiIndex >= 2) || (spiRegisterMap [spiIndex] == NULL)) {
        return false;
    }

    // Attempt to register the DMA ISR if required.
#if GMOS_CONFIG_STM32_SPI_USE_INTERRUPTS
    if (spiIndex == 0) {
        if (!gmosPalDmaIsrAttach (2, gmosDriverSpiPalIsrSpi1)) {
            return false;
        }
    } else {
        if (!gmosPalDmaIsrAttach (6, gmosDriverSpiPalIsrSpi2)) {
            return false;
        }
    }

    // Start the interrupt polling loop if required.
#else
    if (!pollingLoopRunning) {
        pollingLoopTask_start (&pollingLoopTaskData,
            NULL, "STM32 SPI Driver Polling Loop");
        pollingLoopRunning = true;
    }
#endif

    // Keep a reference to the platform data structures.
    spiIoStateMap [spiIndex] = spiInterface;

    // Configure the SPI I/O as GPIO alternate functions.
    gmosDriverGpioAltModeInit (palConfig->sclkPinId,
        GMOS_DRIVER_GPIO_OUTPUT_PUSH_PULL,
        GMOS_CONFIG_SPI_GPIO_DRIVE_STRENGTH,
        GMOS_DRIVER_GPIO_INPUT_PULL_NONE, palConfig->sclkPinAltFn);
    gmosDriverGpioAltModeInit (palConfig->mosiPinId,
        GMOS_DRIVER_GPIO_OUTPUT_PUSH_PULL,
        GMOS_CONFIG_SPI_GPIO_DRIVE_STRENGTH,
        GMOS_DRIVER_GPIO_INPUT_PULL_NONE, palConfig->mosiPinAltFn);
    gmosDriverGpioAltModeInit (palConfig->misoPinId,
        GMOS_DRIVER_GPIO_OUTPUT_PUSH_PULL,
        STM32_GPIO_DRIVER_SLEW_SLOW, GMOS_DRIVER_GPIO_INPUT_PULL_NONE,
        palConfig->misoPinAltFn);

    // Enable the SPI peripheral clock. Note that this is not enabled in
    // the corresponding sleep mode register, so it will automatically
    // be gated on entering sleep mode.
    if (spiIndex == 0) {
        RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    } else {
        RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;
    }

    // Enable the DMA peripheral clock.
    RCC->AHBENR |= RCC_AHBENR_DMAEN;

    // Map the DMA channels to the SPI peripherals. SPI1 always maps to
    // DMA channels 2 and 3. SPI2 always maps to DMA channels 6 and 7.
    // Also set up the SPI peripheral register addresses for the DMA.
    regValue = DMA1_CSELR->CSELR;
    if (spiIndex == 0) {
        regValue &= ~(DMA_CSELR_C2S_Msk |DMA_CSELR_C3S_Msk);
        regValue |= (1 << DMA_CSELR_C2S_Pos) | (1 << DMA_CSELR_C3S_Pos);
        DMA1_Channel2->CPAR = (uintptr_t) &(SPI1->DR);
        DMA1_Channel3->CPAR = (uintptr_t) &(SPI1->DR);
    } else {
        regValue &= ~(DMA_CSELR_C6S_Msk |DMA_CSELR_C7S_Msk);
        regValue |= (2 << DMA_CSELR_C6S_Pos) | (2 << DMA_CSELR_C7S_Pos);
        DMA1_Channel6->CPAR = (uintptr_t) &(SPI2->DR);
        DMA1_Channel7->CPAR = (uintptr_t) &(SPI2->DR);
    }
    DMA1_CSELR->CSELR = regValue;

    return true;
}

/*
 * Sets up the platform abstraction layer for one or more SPI
 * transactions that share the same SPI clock configuration.
 */
void gmosDriverSpiPalClockSetup (gmosDriverSpiIo_t* spiInterface)
{
    const gmosPalSpiIoConfig_t* palConfig = spiInterface->palConfig;
    uint8_t spiIndex = palConfig->spiInterfaceId - 1;
    SPI_TypeDef* spiRegs = spiRegisterMap [spiIndex];
    uint32_t clockDiv;
    uint32_t spiClockRequest;
    uint32_t spiClockFreq;
    uint32_t regValue;

    // Select the initial SPI clock rate.
    if (spiIndex == 0) {
        spiClockFreq = GMOS_CONFIG_STM32_APB2_CLOCK / 2;
    } else {
        spiClockFreq = GMOS_CONFIG_STM32_APB1_CLOCK / 2;
    }

    // Select the closest SPI clock scaling to the one requested.
    spiClockRequest = 1000 * (uint32_t) (spiInterface->spiClockRate);
    for (clockDiv = 0; clockDiv < 7; clockDiv++) {
        if (spiClockFreq <= spiClockRequest) {
            break;
        } else {
            spiClockFreq /= 2;
        }
    }

    // Set up configuration register 1.
    regValue = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI |
        (clockDiv << SPI_CR1_BR_Pos);
    regValue |= 3 & (spiInterface->spiClockMode);
    spiRegs->CR1 = regValue;
}

/*
 * Performs a platform specific SPI transaction using the given SPI
 * interface settings.
 */
void gmosDriverSpiPalTransaction (gmosDriverSpiIo_t* spiInterface)
{
    const gmosPalSpiIoConfig_t* palConfig = spiInterface->palConfig;
    uint8_t spiIndex = palConfig->spiInterfaceId - 1;
    SPI_TypeDef* spiRegs = spiRegisterMap [spiIndex];
    DMA_Channel_TypeDef* dmaTxRegs = dmaTxRegisterMap [spiIndex];
    DMA_Channel_TypeDef* dmaRxRegs = dmaRxRegisterMap [spiIndex];
    uint32_t ccrValue;

    // Always enable DMA receive for the SPI interface before setting up
    // the DMA controller.
    spiRegs->CR2 |= SPI_CR2_RXDMAEN;

    // Configure the DMA receive channel.
    dmaRxRegs->CNDTR = spiInterface->transferSize;
    ccrValue = DMA_CCR_EN | DMA_CCR_TCIE | DMA_CCR_TEIE;
    if (spiInterface->readData == NULL) {
        dmaRxRegs->CMAR = (uintptr_t) &dmaFakeTarget;
    } else {
        dmaRxRegs->CMAR = (uintptr_t) spiInterface->readData;
        ccrValue |= DMA_CCR_MINC;
    }
    dmaRxRegs->CCR = ccrValue;

    // Configure the DMA transmit channel.
    dmaTxRegs->CNDTR = spiInterface->transferSize;
    ccrValue = DMA_CCR_EN | DMA_CCR_DIR;
    if (spiInterface->writeData == NULL) {
        dmaTxRegs->CMAR = (uintptr_t) &dmaFakeSource;
    } else {
        dmaTxRegs->CMAR = (uintptr_t) spiInterface->writeData;
        ccrValue |= DMA_CCR_MINC;
    }
    dmaTxRegs->CCR = ccrValue;

    // Always enable DMA transmit for the SPI interface after setting up
    // the DMA controller.
    spiRegs->CR2 |= SPI_CR2_TXDMAEN;

    // Enable the SPI interface once DMA has been set up.
    spiRegs->CR1 |= SPI_CR1_SPE;
}
