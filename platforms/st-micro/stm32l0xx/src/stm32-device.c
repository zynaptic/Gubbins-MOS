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
 * Provides device configuration and setup routines for STM32L0XX family
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
static gmosPalDmaIsr_t attachedDmaIsrs [] =
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL };

/*
 * Configures the STM32 device for standard performance. This sets the
 * system clock to 16 MHz with a single flash memory wait cycle. This
 * is the maximum performance supported with the default 1.5V core
 * voltage setting.
 */
static void gmosPalClockSetup16MHz (void)
{
    // Enable the HSI oscillator and wait for it to stabilise.
    RCC->CR |= RCC_CR_HSION;
    while ((RCC->CR & RCC_CR_HSIRDY) == 0) {};

    // Enable the extra flash memory access wait state. Wait for the
    // latency to be updated before altering the clock source.
    FLASH->ACR |= FLASH_ACR_LATENCY;
    while ((FLASH->ACR & FLASH_ACR_LATENCY) == 0) {};

    // Select the 16MHz HSI oscillator as the system clock source.
    // Also selects this as the clock source to use on waking from
    // deep sleep.
    RCC->CFGR |= (RCC_CFGR_SW_HSI | RCC_CFGR_STOPWUCK);
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI) {};

    // Disable the internal voltage reference in deep sleep mode.
    PWR->CR |= PWR_CR_ULP;
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

    // Enable flash memory prefetch with extra latency. Wait for the
    // latency to be updated before altering the clock source.
    FLASH->ACR |= FLASH_ACR_LATENCY | FLASH_ACR_PRFTEN;
    while ((FLASH->ACR & (FLASH_ACR_LATENCY | FLASH_ACR_PRFTEN)) == 0) {};

    // Select the 32MHz PLL output as the system clock source.
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {};
}

/*
 * Configures the STM32 low power timer to run off the external
 * 32.768kHz oscillator, divided to 1.024kHz.
 */
static void gmosPalTimerSetup (bool useExternalOsc)
{
    // Enable the low power timer clock in standard and sleep modes.
    RCC->APB1ENR |= RCC_APB1ENR_LPTIM1EN;
    RCC->APB1SMENR |= RCC_APB1SMENR_LPTIM1SMEN;

    // Configures the STM32 low power timer to run off the external
    // 32.768kHz oscillator, divided to 1.024kHz. The LSE clock control
    // bits are treated as part of the RTC subsystem, which means they
    // persist over a reset and need to be 'unlocked' prior to any
    // changes by disabling backup protection.
    if (useExternalOsc) {
        if ((RCC->CSR & RCC_CSR_LSERDY) == 0) {
            RCC->APB1ENR |= RCC_APB1ENR_PWREN;
            PWR->CR |= PWR_CR_DBP;
            RCC->CSR |= RCC_CSR_LSEON;
            while ((RCC->CSR & RCC_CSR_LSERDY) == 0) {};

            // Enable RTC clock if an external oscillator is available.
            RCC->CSR |= RCC_CSR_RTCSEL_LSE | RCC_CSR_RTCEN;
        }
        LPTIM1->CFGR = (5 << LPTIM_CFGR_PRESC_Pos);
        RCC->CCIPR |= RCC_CCIPR_LPTIM1SEL_0 | RCC_CCIPR_LPTIM1SEL_1;
    }

    // Configure the STM32 low power timer to run off the internal low
    // speed RC oscillator, divided from a nominal 37kHz to 578Hz. Note
    // that the source frequency can be anything from 26kHz to 56kHz,
    // so this is not intended for use in timing sensitive applications.
    else {
        RCC->CSR |= (RCC_CSR_LSION);
        while ((RCC->CSR & RCC_CSR_LSIRDY) == 0) {};
        LPTIM1->CFGR = (6 << LPTIM_CFGR_PRESC_Pos);
        RCC->CCIPR |= RCC_CCIPR_LPTIM1SEL_0;
    }

    // Enable the low power timer ready for use.
    LPTIM1->CR = LPTIM_CR_ENABLE;
    while ((LPTIM1->CR & LPTIM_CR_ENABLE) == 0) {};
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
    gmosPalTimerSetup (GMOS_CONFIG_STM32_USE_LSE_OSC);
}

/*
 * Attaches a DMA interrupt service routine for the specified DMA
 * channel.
 */
bool gmosPalDmaIsrAttach (uint8_t channel, uint8_t (*isr) (uint8_t))
{
    // Check for invalid or duplicate requests.
    if ((channel <= 0) || (channel > 7) ||
        (attachedDmaIsrs [channel-1] != NULL)) {
        return false;
    }

    // Register the DMA channel ISR.
    attachedDmaIsrs [channel-1] = isr;

    // Enable the appropriate NVIC interrupt line.
    if (channel == 1) {
        NVIC_EnableIRQ (DMA1_Channel1_IRQn);
    } else if (channel <= 3) {
        NVIC_EnableIRQ (DMA1_Channel2_3_IRQn);
    } else {
        NVIC_EnableIRQ (DMA1_Channel4_5_6_7_IRQn);
    }
    return true;
}

/*
 * Implements common ISR handling for the DMA interrupts.
 */
static void gmosPalDmaIsrCommon (uint8_t indexStart, uint8_t indexEnd)
{
    gmosPalDmaIsr_t dmaIsr;
    uint32_t regFlags;
    uint32_t isrFlags;
    uint32_t regClear;
    uint32_t isrClear;
    uint8_t i;

    // Loop over the requested ISR set.
    regFlags = DMA1->ISR;
    regClear = 0;
    for (i = indexStart; i <= indexEnd; i++) {
        dmaIsr = attachedDmaIsrs [i];
        isrFlags = 0x0F & (regFlags >> (4 * i));
        if ((dmaIsr == NULL) || (isrFlags == 0)) {
            isrClear = 0;
        } else {
            isrClear = 0x0F & dmaIsr (isrFlags);
        }
        regClear |= (isrClear << (4 * i));
    }
    DMA1->IFCR = regClear;
}

/*
 * Process DMA interrupts for channel 1.
 */
void gmosPalIsrDMA1A (void) {
    gmosPalDmaIsrCommon (0, 0);
}

/*
 * Process DMA interrupts for channels 2 and 3.
 */
void gmosPalIsrDMA1B (void) {
    gmosPalDmaIsrCommon (1, 2);
}

/*
 * Process DMA interrupts for channels 4, 5, 6 and 7.
 */
void gmosPalIsrDMA1C (void) {
    gmosPalDmaIsrCommon (3, 6);
}
