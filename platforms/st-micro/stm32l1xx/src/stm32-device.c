/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2020-2022 Zynaptic Limited
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
 * Provides device configuration and setup routines for STM32L1XX family
 * devices.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "stm32-device.h"

/*
 * Store pointers to the attached DMA interrupt service routines.
 */
#ifdef DMA2
static gmosPalDmaIsr_t attachedDmaIsrs [] = {
    NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL };
#else
static gmosPalDmaIsr_t attachedDmaIsrs [] = {
    NULL, NULL, NULL, NULL, NULL, NULL, NULL };
#endif

/*
 * Store lookup table of DMA interrupt numbers.
 */
#ifdef DMA2
static uint8_t lookupDmaIrqs [] = {
    DMA1_Channel1_IRQn, DMA1_Channel2_IRQn, DMA1_Channel3_IRQn,
    DMA1_Channel4_IRQn, DMA1_Channel5_IRQn, DMA1_Channel6_IRQn,
    DMA1_Channel7_IRQn, DMA2_Channel1_IRQn, DMA2_Channel2_IRQn,
    DMA2_Channel3_IRQn, DMA2_Channel4_IRQn, DMA2_Channel5_IRQn };
#else
static uint8_t lookupDmaIrqs [] = {
    DMA1_Channel1_IRQn, DMA1_Channel2_IRQn, DMA1_Channel3_IRQn,
    DMA1_Channel4_IRQn, DMA1_Channel5_IRQn, DMA1_Channel6_IRQn,
    DMA1_Channel7_IRQn };
#endif

/*
 * Configures the STM32 device for standard performance. This sets the
 * system clock to 16 MHz, directly sourced from the 16 MHz internal
 * oscillator. This is the maximum performance supported with the
 * default 1.5V core voltage setting.
 */
static void gmosPalClockSetup16MHz (void)
{
    // Enable the HSI oscillator and wait for it to stabilise.
    RCC->CR |= RCC_CR_HSION;
    while ((RCC->CR & RCC_CR_HSIRDY) == 0) {};

    // Enable 64-bit flash memory access support and ensure that it is
    // set before updating the prefetch and wait state bits.
    FLASH->ACR |= FLASH_ACR_ACC64;
    while ((FLASH->ACR & FLASH_ACR_ACC64) == 0) {};

    // Enable flash memory prefetch with extra latency. Wait for the
    // latency to be updated before altering the clock source.
    FLASH->ACR |= FLASH_ACR_LATENCY | FLASH_ACR_PRFTEN;
    while ((FLASH->ACR & (FLASH_ACR_LATENCY | FLASH_ACR_PRFTEN)) == 0) {};

    // Select the 16MHz HSI oscillator as the system clock source.
    RCC->CFGR |= RCC_CFGR_SW_HSI;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI) {};
}

/*
 * Configures the STM32 device for high performance. This sets the
 * system clock to 32 MHz, derived from the 16 MHz internal oscillator
 * using the PLL. This is the maximum performance supported with the
 * high power 1.8V core voltage setting.
 */
static void gmosPalClockSetup32MHz (void)
{
    uint32_t regValue;

    // Set the core supply voltage to 1.8V.
    regValue = PWR->CR;
    regValue &= PWR_CR_VOS_Msk;
    regValue |= PWR_CR_VOS_0;
    PWR->CR = regValue;

    // Wait for the core supply voltage to stabilise.
    while ((PWR->CSR & PWR_CSR_VOSF) != 0) {};

    // Enable the HSI oscillator and wait for it to stabilise.
    RCC->CR |= RCC_CR_HSION;
    while ((RCC->CR & RCC_CR_HSIRDY) == 0) {};

    // Enable the PLL to multiply the HSI clock by four and divide by
    // two and then wait for it to stabilise.
    RCC->CFGR |= RCC_CFGR_PLLDIV2 | RCC_CFGR_PLLMUL4;
    RCC->CR |= RCC_CR_PLLON;
    while ((RCC->CR & RCC_CR_PLLRDY) == 0) {};

    // Enable 64-bit flash memory access support and ensure that it is
    // set before updating the prefetch and wait state bits.
    FLASH->ACR |= FLASH_ACR_ACC64;
    while ((FLASH->ACR & FLASH_ACR_ACC64) == 0) {};

    // Enable flash memory prefetch with extra latency. Wait for the
    // latency to be updated before altering the clock source.
    FLASH->ACR |= FLASH_ACR_LATENCY | FLASH_ACR_PRFTEN;
    while ((FLASH->ACR & (FLASH_ACR_LATENCY | FLASH_ACR_PRFTEN)) == 0) {};

    // Select the 32MHz PLL output as the system clock source.
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {};
}

/*
 * Configures the STM32 timer 11 to run off the external 32.768kHz
 * oscillator, divided to 1.024kHz. The core timer logic runs off the
 * default internal clock, so requires this to be stable during
 * operation.
 */
static void gmosPalTimerSetup (void)
{
    // Enable the system timer clock in standard and sleep modes.
    RCC->APB2ENR |= RCC_APB2ENR_TIM11EN;
    RCC->APB2LPENR |= RCC_APB2LPENR_TIM11LPEN;

    // Configures the STM32 system timer to run off the external
    // 32.768kHz oscillator, divided to 1.024kHz. The LSE clock control
    // bits are treated as part of the RTC subsystem, which means they
    // persist over a reset and need to be 'unlocked' prior to any
    // changes by disabling backup protection.
    if ((RCC->CSR & RCC_CSR_LSERDY) == 0) {
        RCC->APB1ENR |= RCC_APB1ENR_PWREN;
        PWR->CR |= PWR_CR_DBP;
        RCC->CSR |= RCC_CSR_LSEON;
        while ((RCC->CSR & RCC_CSR_LSERDY) == 0) {};

        // Enable RTC clock if an external oscillator is available. This
        // is also used as the refresh clock for the LCD controller.
        RCC->CSR |= RCC_CSR_RTCSEL_LSE | RCC_CSR_RTCEN;
    }
    TIM11->SMCR |= TIM_SMCR_ECE;
    TIM11->PSC = 31;
}

/*
 * Performs STM32 system setup after reset.
 */
void gmosPalSystemSetup (void)
{
    // Select the 32MHz PLL or 16MHz HSI clock.
    if (GMOS_CONFIG_STM32_SYSTEM_CLOCK == 32000000) {
        gmosPalClockSetup32MHz ();
    } else {
        gmosPalClockSetup16MHz ();
    }

    // Select the required low speed clock source.
    gmosPalTimerSetup ();
}

/*
 * Attaches a DMA interrupt service routine for the specified DMA
 * channel.
 */
bool gmosPalDmaIsrAttach (
    uint8_t dmaUnit, uint8_t dmaChannel, gmosPalDmaIsr_t isr)
{
    uint8_t channelIndex;
    uint8_t dmaIrq;

    // Check for invalid or duplicate requests (DMA unit 1).
    if (dmaUnit == 1) {
        channelIndex = dmaChannel - 1;
        if ((dmaChannel < 1) || (dmaChannel > 7) ||
            (attachedDmaIsrs [channelIndex] != NULL)) {
            return false;
        }
    }

    // Check for invalid or duplicate requests (DMA unit 2).
#ifdef DMA2
    else if (dmaUnit == 2) {
        channelIndex = dmaChannel + 6;
        if ((dmaChannel < 1) || (dmaChannel > 5) ||
            (attachedDmaIsrs [channelIndex] != NULL)) {
            return false;
        }
    }
#endif // DMA2

    // Invalid DMA unit selection.
    else {
        return false;
    }

    // Register the DMA channel ISR.
    attachedDmaIsrs [channelIndex] = isr;
    dmaIrq = lookupDmaIrqs [channelIndex];

    // Enable the appropriate NVIC interrupt line.
    NVIC_EnableIRQ (dmaIrq);
    return true;
}

/*
 * Implements common ISR handling for the DMA1 interrupts.
 */
static void gmosPalDma1IsrCommon (uint8_t dmaChannel)
{
    uint8_t channelIndex = dmaChannel - 1;
    uint8_t regOffset = 4 * (dmaChannel - 1);
    gmosPalDmaIsr_t dmaIsr;
    uint32_t regFlags;
    uint32_t isrFlags;
    uint32_t regClear;

    // Check the interrupt flags at the specified channel index.
    regFlags = DMA1->ISR;
    isrFlags = 0x0F & (regFlags >> regOffset);
    dmaIsr = attachedDmaIsrs [channelIndex];

    // If any interrupt flags are active, forward them to the registered
    // DMA interrupt service routine.
    if ((dmaIsr != NULL) && (isrFlags != 0)) {
        regClear = 0x0F & dmaIsr (isrFlags);
        regClear = regClear << regOffset;
        DMA1->IFCR = regClear;
    }
}

/*
 * Process DMA1 interrupts for channel 1.
 */
void gmosPalIsrDMA1A (void) {
    gmosPalDma1IsrCommon (1);
}

/*
 * Process DMA1 interrupts for channel 2.
 */
void gmosPalIsrDMA1B (void) {
    gmosPalDma1IsrCommon (2);
}

/*
 * Process DMA1 interrupts for channel 3.
 */
void gmosPalIsrDMA1C (void) {
    gmosPalDma1IsrCommon (3);
}

/*
 * Process DMA1 interrupts for channel 4.
 */
void gmosPalIsrDMA1D (void) {
    gmosPalDma1IsrCommon (4);
}

/*
 * Process DMA1 interrupts for channel 5.
 */
void gmosPalIsrDMA1E (void) {
    gmosPalDma1IsrCommon (5);
}

/*
 * Process DMA1 interrupts for channel 6.
 */
void gmosPalIsrDMA1F (void) {
    gmosPalDma1IsrCommon (6);
}

/*
 * Process DMA1 interrupts for channel 7.
 */
void gmosPalIsrDMA1G (void) {
    gmosPalDma1IsrCommon (7);
}

/*
 * Not all devices in the STM32L1XX family support DMA2. Do not include
 * the DMA2 interrupt service routines if they are not requred.
 */
#ifdef DMA2

/*
 * Implements common ISR handling for the DMA2 interrupts.
 */
static void gmosPalDma2IsrCommon (uint8_t dmaChannel)
{
    uint8_t channelIndex = dmaChannel + 6;
    uint8_t regOffset = 4 * (dmaChannel - 1);
    gmosPalDmaIsr_t dmaIsr;
    uint32_t regFlags;
    uint32_t isrFlags;
    uint32_t regClear;

    // Check the interrupt flags at the specified channel index.
    regFlags = DMA2->ISR;
    isrFlags = 0x0F & (regFlags >> regOffset);
    dmaIsr = attachedDmaIsrs [channelIndex];

    // If any interrupt flags are active, forward them to the registered
    // DMA interrupt service routine.
    if ((dmaIsr != NULL) && (isrFlags != 0)) {
        regClear = 0x0F & dmaIsr (isrFlags);
        regClear = regClear << regOffset;
        DMA2->IFCR = regClear;
    }
}

/*
 * Process DMA2 interrupts for channel 1.
 */
void gmosPalIsrDMA2A (void) {
    gmosPalDma2IsrCommon (1);
}

/*
 * Process DMA2 interrupts for channel 2.
 */
void gmosPalIsrDMA2B (void) {
    gmosPalDma2IsrCommon (2);
}

/*
 * Process DMA2 interrupts for channel 3.
 */
void gmosPalIsrDMA2C (void) {
    gmosPalDma2IsrCommon (3);
}

/*
 * Process DMA2 interrupts for channel 4.
 */
void gmosPalIsrDMA2D (void) {
    gmosPalDma2IsrCommon (4);
}

/*
 * Process DMA2 interrupts for channel 5.
 */
void gmosPalIsrDMA2E (void) {
    gmosPalDma2IsrCommon (5);
}

#endif // DMA2
