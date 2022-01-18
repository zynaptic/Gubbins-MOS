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
 * Specifies the STM32L1XX default configuration options.
 */

#ifndef GMOS_PAL_CONFIG_H
#define GMOS_PAL_CONFIG_H

#include <stdbool.h>

/**
 * Specify the STM32 system clock rate. The only currently supported
 * options use the 16MHz HSI clock or the 32MHz PLL output derived from
 * the HSI clock.
 */
#ifndef GMOS_CONFIG_STM32_SYSTEM_CLOCK
#define GMOS_CONFIG_STM32_SYSTEM_CLOCK 32000000
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
 * Specify whether LCD segment remapping is to be used. This is required
 * when there is not a direct mapping from logical LCD segments to
 * LCD driver segments. If this option is enabled, a corresponding
 * segment mapping table must be provided in the LCD driver
 * configuration.
 */
#ifndef GMOS_CONFIG_STM32_LCD_REMAP_SEGMENTS
#define GMOS_CONFIG_STM32_LCD_REMAP_SEGMENTS false
#endif

/**
 * Specify whether LCD pin remapping is to be used. This is required
 * for smaller footprint devices to map the device segments 28 to 31
 * onto available pins which would normally control device segments
 * 40 to 43.
 */
#ifndef GMOS_CONFIG_STM32_LCD_REMAP_DEVICE_PINS
#define GMOS_CONFIG_STM32_LCD_REMAP_DEVICE_PINS true
#endif

/**
 * Specify the approximate LCD frame refresh frequency. This should be
 * in the range from 30 to 100 Hz, and is a compromise between power
 * consumption and the potential for visible flicker.
 */
#ifndef GMOS_CONFIG_STM32_LCD_FRAME_RATE
#define GMOS_CONFIG_STM32_LCD_FRAME_RATE 85
#endif

/**
 * Specify the default LCD drive voltage level. This should be in the
 * range from 0 to 7 (highest voltage), and is a compromise between
 * power consumption and LCD saturation.
 */
#ifndef GMOS_CONFIG_STM32_LCD_DEFAULT_VOLTAGE_LEVEL
#define GMOS_CONFIG_STM32_LCD_DEFAULT_VOLTAGE_LEVEL 7
#endif

/**
 * Specify the LCD duty cycle ratio. This is a compromise between the
 * number of common terminals that may be used by the LCD panel and the
 * potential for visible flicker. Duty cycle ratios of 1, 2, 3, 4 and 8
 * are supported.
 */
#ifndef GMOS_CONFIG_STM32_LCD_DUTY_CYCLE_RATIO
#define GMOS_CONFIG_STM32_LCD_DUTY_CYCLE_RATIO 4
#endif

// Configure the system timer frequency based on the selected low speed
// clock source.
#define GMOS_CONFIG_SYSTEM_TIMER_FREQUENCY 1024

// Derive the AHB bus clock based on the clock divider setting.
#define GMOS_CONFIG_STM32_AHB_CLOCK \
    (GMOS_CONFIG_STM32_SYSTEM_CLOCK / GMOS_CONFIG_STM32_AHB_CLOCK_DIV)

// Derive the APB1 bus clock based on the clock divider setting.
#define GMOS_CONFIG_STM32_APB1_CLOCK \
    (GMOS_CONFIG_STM32_AHB_CLOCK / GMOS_CONFIG_STM32_APB1_CLOCK_DIV)

// Derive the APB2 bus clock based on the clock divider setting.
#define GMOS_CONFIG_STM32_APB2_CLOCK \
    (GMOS_CONFIG_STM32_AHB_CLOCK / GMOS_CONFIG_STM32_APB2_CLOCK_DIV)

#endif // GMOS_PAL_CONFIG_H
