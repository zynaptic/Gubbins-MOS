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
 * Specifies the Raspberry Pi RP2040 default configuration options.
 */

#ifndef GMOS_PAL_CONFIG_H
#define GMOS_PAL_CONFIG_H

#include "pico-driver-gpio.h"

/**
 * Specify the maximum number of supported GPIO interrupt service
 * routines. In principle, all RP2040 GPIO pins may be used as interrupt
 * sources, but restricting the available number can reduce resource
 * utilisation.
 */
#ifndef GMOS_CONFIG_PICO_GPIO_MAX_ISRS
#define GMOS_CONFIG_PICO_GPIO_MAX_ISRS 4
#endif

/**
 * Specify whether multicore access to the GPIO logic is supported.
 * Multicore access requires the GPIO routines to claim the main
 * platform lock for the duration of each GPIO access. For the most
 * efficient operation all GPIO access should be restricted to a single
 * processor core, in which case this option may be disabled.
 */
#ifndef GMOS_CONFIG_PICO_GPIO_MULTICORE_ACCESS
#define GMOS_CONFIG_PICO_GPIO_MULTICORE_ACCESS false
#endif

/**
 * Specify the UART to use for the serial debug console.
 */
#ifndef GMOS_CONFIG_PICO_DEBUG_CONSOLE_UART_ID
#define GMOS_CONFIG_PICO_DEBUG_CONSOLE_UART_ID 0
#endif

/**
 * Specify the GPIO pin to use for the serial debug console. This must
 * support alternate function mapping for the 'TX' pin of the selected
 * UART instance.
 */
#ifndef GMOS_CONFIG_PICO_DEBUG_CONSOLE_UART_TX_PIN
#define GMOS_CONFIG_PICO_DEBUG_CONSOLE_UART_TX_PIN (PICO_GPIO_BANK_U | 0)
#endif

/**
 * Specify the baud rate to use for the serial debug console.
 */
#ifndef GMOS_CONFIG_PICO_DEBUG_CONSOLE_BAUD_RATE
#define GMOS_CONFIG_PICO_DEBUG_CONSOLE_BAUD_RATE 38400
#endif

/**
 * Specify the maximum size of the serial debug console transmit buffer.
 * The transmit buffer will be dynamically allocated from the memory
 * pool.
 */
#ifndef GMOS_CONFIG_PICO_DEBUG_CONSOLE_BUFFER_SIZE
#define GMOS_CONFIG_PICO_DEBUG_CONSOLE_BUFFER_SIZE 1024
#endif

/**
 * Specify whether the serial debug console should include the device
 * uptime, as derived from the RP2040 system timer.
 */
#ifndef GMOS_CONFIG_PICO_DEBUG_CONSOLE_INCLUDE_UPTIME
#define GMOS_CONFIG_PICO_DEBUG_CONSOLE_INCLUDE_UPTIME false
#endif

/**
 * Select the GPIO drive strength to use for the SPI interface pins.
 */
#ifndef GMOS_CONFIG_SPI_GPIO_DRIVE_STRENGTH
#define GMOS_CONFIG_SPI_GPIO_DRIVE_STRENGTH PICO_GPIO_DRIVER_SLEW_FAST_4MA
#endif

/*
 * The Raspberry Pi SDK includes fast memcpy implementations that will
 * be used for stream and buffer data transfers.
 */
#define GMOS_CONFIG_STREAMS_USE_MEMCPY 1
#define GMOS_CONFIG_BUFFERS_USE_MEMCPY 1

/*
 * Set the system timer frequency. This is set by dividing the Pico SDK
 * 1MHz system timer value by 1024. This is an integer approximation
 * with rounding, since the corresponding frequency actually works out
 * at 976.5625 Hz.
 */
#define GMOS_CONFIG_SYSTEM_TIMER_FREQUENCY ((1000000 + 512) / 1024)

/*
 * Converts the specified number of milliseconds to the closest number
 * of system timer ticks (rounding down). Defining the macro here will
 * give more accurate results than using the integer approximation of
 * the timer frequency in the standard macro.
 */
#define GMOS_MS_TO_TICKS(_ms_) ((uint32_t) \
    ((((uint64_t) _ms_) * 1000) / 1024))

/*
 * Converts the specified number of system timer ticks to the closest
 * number of milliseconds (rounding down). Defining the macro here will
 * give more accurate results than using the integer approximation of
 * the timer frequency in the standard macro.
 */
#define GMOS_TICKS_TO_MS(_ticks_) ((uint32_t) \
    ((((uint64_t) _ticks_) * 1024) / 1000))

#endif // GMOS_PAL_CONFIG_H
