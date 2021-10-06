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
 * Specifies the Microchip/Atmel ATMEGA default configuration options.
 */

#ifndef GMOS_PAL_CONFIG_H
#define GMOS_PAL_CONFIG_H

/**
 * Specify the ATMEGA system clock rate. This will depend on the target
 * board and clock fuse settings and may be set to another value in the
 * application configuration header.
 */
#ifndef GMOS_CONFIG_ATMEGA_SYSTEM_CLOCK
#define GMOS_CONFIG_ATMEGA_SYSTEM_CLOCK 3686400
#endif

/**
 * Enable the ATMEGA low speed external oscillator, which provides the
 * 32.768 kHz system timer reference frequency.
 */
#ifndef GMOS_CONFIG_ATMEGA_USE_LSE_OSC
#define GMOS_CONFIG_ATMEGA_USE_LSE_OSC false
#endif

/**
 * Specify the postscaling amount to be applied to the system timer.
 * This specifies the number of low order bits from the hardware timer
 * that are to be discarded in order to yield a system timer frequency
 * close to 1 ms resolution. This is not required when using the low
 * speed external oscillator.
 */
#ifndef GMOS_CONFIG_ATMEGA_SYSTEM_TIMER_POSTSCALE
#define GMOS_CONFIG_ATMEGA_SYSTEM_TIMER_POSTSCALE 2
#endif

/**
 * This configuration option specifies whether the ATMEGA external
 * interrupt 0 should use active low level or conventional edge
 * triggering.
 */
#ifndef GMOS_CONFIG_ATMEGA_EXTINT0_ACTIVE_LOW
#define GMOS_CONFIG_ATMEGA_EXTINT0_ACTIVE_LOW false
#endif

/**
 * This configuration option specifies whether the ATMEGA external
 * interrupt 1 should use active low level or conventional edge
 * triggering.
 */
#ifndef GMOS_CONFIG_ATMEGA_EXTINT1_ACTIVE_LOW
#define GMOS_CONFIG_ATMEGA_EXTINT1_ACTIVE_LOW false
#endif

/**
 * Specify the baud rate to use for the serial debug console.
 */
#ifndef GMOS_CONFIG_ATMEGA_DEBUG_CONSOLE_BAUD_RATE
#define GMOS_CONFIG_ATMEGA_DEBUG_CONSOLE_BAUD_RATE 38400
#endif

/**
 * Specify the maximum size of the serial debug console transmit buffer.
 * The transmit buffer will be dynamically allocated from the memory
 * pool.
 */
#ifndef GMOS_CONFIG_ATMEGA_DEBUG_CONSOLE_BUFFER_SIZE
#define GMOS_CONFIG_ATMEGA_DEBUG_CONSOLE_BUFFER_SIZE 256
#endif

/**
 * This configuration option specifies the size of individual memory
 * pool segments as an integer number of bytes. This must be an integer
 * multiple of 4.
 */
#ifndef GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE
#define GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE 32
#endif

/**
 * This configuration option specifies the number of memory pool
 * segments to be allocated.  The default memory pool size for ATMEGA
 * devices is set to 512 bytes.
 */
#ifndef GMOS_CONFIG_MEMPOOL_SEGMENT_NUMBER
#define GMOS_CONFIG_MEMPOOL_SEGMENT_NUMBER 16
#endif

// Configure the system timer frequency based on the selected low speed
// clock source. If the low speed oscillator is not being used, the
// maximum prescaling is applied to the system clock.
#if GMOS_CONFIG_ATMEGA_USE_LSE_OSC
#define GMOS_CONFIG_SYSTEM_TIMER_FREQUENCY 1024
#else
#define GMOS_CONFIG_SYSTEM_TIMER_FREQUENCY \
    (GMOS_CONFIG_ATMEGA_SYSTEM_CLOCK / 1024) / \
    (1 << GMOS_CONFIG_ATMEGA_SYSTEM_TIMER_POSTSCALE)
#endif

#endif // GMOS_PAL_CONFIG_H
