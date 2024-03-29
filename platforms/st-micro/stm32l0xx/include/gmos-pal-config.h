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
 * Specifies the STM32L0XX default configuration options.
 */

#ifndef GMOS_PAL_CONFIG_H
#define GMOS_PAL_CONFIG_H

#include <stdbool.h>
#include "stm32-driver-gpio.h"

/**
 * Specify the STM32 system clock rate. The only currently supported
 * options use the 16MHz HSI clock and 32 MHz PLL clock.
 */
#ifndef GMOS_CONFIG_STM32_SYSTEM_CLOCK
#define GMOS_CONFIG_STM32_SYSTEM_CLOCK 16000000
#endif

/**
 * Specify whether STM32 low power sleep modes should be used when no
 * tasks are currently scheduled. If this is not enabled the scheduler
 * will use busy waiting instead.
 */
#ifndef GMOS_CONFIG_STM32_SYSTEM_SLEEP_ENABLE
#define GMOS_CONFIG_STM32_SYSTEM_SLEEP_ENABLE true
#endif

/**
 * Specify the STM32 AHB clock divider. The only currently supported
 * option is to derive it directly from the system clock.
 */
#ifndef GMOS_CONFIG_STM32_AHB_CLOCK_DIV
#define GMOS_CONFIG_STM32_AHB_CLOCK_DIV 1
#endif

/**
 * Specify the STM32 APB1 clock divider. The only currently supported
 * option is to derive it directly from the AHB bus clock.
 */
#ifndef GMOS_CONFIG_STM32_APB1_CLOCK_DIV
#define GMOS_CONFIG_STM32_APB1_CLOCK_DIV 1
#endif

/**
 * Specify the STM32 APB2 clock divider. The only currently supported
 * option is to derive it directly from the AHB bus clock.
 */
#ifndef GMOS_CONFIG_STM32_APB2_CLOCK_DIV
#define GMOS_CONFIG_STM32_APB2_CLOCK_DIV 1
#endif

/**
 * Enable the STM32 low speed external oscillator, which provides the
 * 32.768 kHz system timer reference frequency.
 */
#ifndef GMOS_CONFIG_STM32_USE_LSE_OSC
#define GMOS_CONFIG_STM32_USE_LSE_OSC true
#endif

/**
 * Specify the baud rate to use for the serial debug console.
 */
#ifndef GMOS_CONFIG_STM32_DEBUG_CONSOLE_BAUD_RATE
#define GMOS_CONFIG_STM32_DEBUG_CONSOLE_BAUD_RATE 38400
#endif

/**
 * Specify whether the serial debug console should use DMA transfers.
 */
#ifndef GMOS_CONFIG_STM32_DEBUG_CONSOLE_USE_DMA
#define GMOS_CONFIG_STM32_DEBUG_CONSOLE_USE_DMA true
#endif

/**
 * Specify the maximum size of the serial debug console transmit buffer.
 * The transmit buffer will be dynamically allocated from the memory
 * pool.
 */
#ifndef GMOS_CONFIG_STM32_DEBUG_CONSOLE_BUFFER_SIZE
#define GMOS_CONFIG_STM32_DEBUG_CONSOLE_BUFFER_SIZE 1024
#endif

/**
 * Specify the number of system timer ticks that represent the minimum
 * period for which the STM32 device will be placed in power down mode.
 */
#ifndef GMOS_CONFIG_STM32_STAY_AWAKE_THRESHOLD
#define GMOS_CONFIG_STM32_STAY_AWAKE_THRESHOLD 2
#endif

/**
 * Specify the number of system timer ticks that represent the minimum
 * period for which the STM32 device will be placed in deep sleep mode.
 */
#ifndef GMOS_CONFIG_STM32_DEEP_SLEEP_THRESHOLD
#define GMOS_CONFIG_STM32_DEEP_SLEEP_THRESHOLD 128
#endif

/**
 * Select interrupt driven or polled mode driver operation for the STM32
 * I2C interfaces.
 */
#ifndef GMOS_CONFIG_STM32_I2C_USE_INTERRUPTS
#define GMOS_CONFIG_STM32_I2C_USE_INTERRUPTS true
#endif

/**
 * Select interrupt driven or polled mode driver operation for the STM32
 * SPI interfaces.
 */
#ifndef GMOS_CONFIG_STM32_SPI_USE_INTERRUPTS
#define GMOS_CONFIG_STM32_SPI_USE_INTERRUPTS true
#endif

/**
 * Select the GPIO drive strength to use for the SPI interface pins.
 */
#ifndef GMOS_CONFIG_SPI_GPIO_DRIVE_STRENGTH
#define GMOS_CONFIG_SPI_GPIO_DRIVE_STRENGTH STM32_GPIO_DRIVER_SLEW_FAST
#endif

// Configure the system timer frequency based on the selected low speed
// clock source.
#if GMOS_CONFIG_STM32_USE_LSE_OSC
#define GMOS_CONFIG_SYSTEM_TIMER_FREQUENCY 1024
#else
#define GMOS_CONFIG_SYSTEM_TIMER_FREQUENCY 578
#endif

// Derive the AHB bus clock based on the clock divider setting.
#define GMOS_CONFIG_STM32_AHB_CLOCK \
    (GMOS_CONFIG_STM32_SYSTEM_CLOCK / GMOS_CONFIG_STM32_AHB_CLOCK_DIV)

// Derive the APB1 bus clock based on the clock divider setting.
#define GMOS_CONFIG_STM32_APB1_CLOCK \
    (GMOS_CONFIG_STM32_AHB_CLOCK / GMOS_CONFIG_STM32_APB1_CLOCK_DIV)

// Derive the APB2 bus clock based on the clock divider setting.
#define GMOS_CONFIG_STM32_APB2_CLOCK \
    (GMOS_CONFIG_STM32_AHB_CLOCK / GMOS_CONFIG_STM32_APB2_CLOCK_DIV)

#endif
