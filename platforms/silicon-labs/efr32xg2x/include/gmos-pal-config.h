/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2023 Zynaptic Limited
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
 * This header defines the platform specific data structures used for
 * the Silicon Labs EFR32xG2x SPI driver implementation.
 */

#ifndef GMOS_PAL_CONFIG_H
#define GMOS_PAL_CONFIG_H

#include <stdbool.h>

/**
 * Set a fixed HFXO tuning capacitor value which is suitable for most
 * standard implementations. A negative value indicates that the default
 * device specific settings should be used.
 */
#ifndef GMOS_CONFIG_EFR32_HFXO_FIXED_CTUNE_VAL
#define GMOS_CONFIG_EFR32_HFXO_FIXED_CTUNE_VAL -1
#endif

/**
 * Set the precision of the 32.768kHz low frequency oscillator in parts
 * per million.
 */
#ifndef GMOS_CONFIG_EFR32_LFXO_PRECISION
#define GMOS_CONFIG_EFR32_LFXO_PRECISION 100
#endif

/**
 * Specify the pin to be used as the debug console UART transmit pin.
 */
#ifndef GMOS_CONFIG_EFR32_DEBUG_CONSOLE_TX_PIN
#define GMOS_CONFIG_EFR32_DEBUG_CONSOLE_TX_PIN (EFR32_GPIO_BANK_A | 8)
#endif

/**
 * Specify the pin to be used as the debug console UART transmit enable,
 * if required. This is primarily used for selecting the virtual COM
 * port on the Silicon Labs Wireless Pro Kit.
 */
#ifndef GMOS_CONFIG_EFR32_DEBUG_CONSOLE_TX_EN
// Leave undefined by default.
#endif

/**
 * Specify the baud rate to use for the serial debug console.
 */
#ifndef GMOS_CONFIG_EFR32_DEBUG_CONSOLE_BAUD_RATE
#define GMOS_CONFIG_EFR32_DEBUG_CONSOLE_BAUD_RATE 115200
#endif

/**
 * Specify whether the serial debug console should use DMA transfers.
 */
#ifndef GMOS_CONFIG_EFR32_DEBUG_CONSOLE_USE_DMA
#define GMOS_CONFIG_EFR32_DEBUG_CONSOLE_USE_DMA false
#endif

/**
 * Specify the maximum size of the serial debug console transmit buffer.
 * The transmit buffer will be dynamically allocated from the memory
 * pool.
 */
#ifndef GMOS_CONFIG_EFR32_DEBUG_CONSOLE_BUFFER_SIZE
#define GMOS_CONFIG_EFR32_DEBUG_CONSOLE_BUFFER_SIZE 1024
#endif

/**
 * Specify the number of DMA channels that will be configured for use.
 * The valid range is from 0 to 8. Increasing the number of configured
 * DMA channels will increase the amount of memory allocated to the DMA
 * driver.
 */
#ifndef GMOS_CONFIG_EFR32_DMA_CHANNEL_COUNT
#define GMOS_CONFIG_EFR32_DMA_CHANNEL_COUNT 4
#endif

/**
 * Specify the interrupt priority level to use for the DMA controller.
 * The valid range is from 0 to 15, with 0 being the highest priority
 * level.
 */
#ifndef GMOS_CONFIG_EFR32_DMA_INTERRUPT_PRIORITY
#define GMOS_CONFIG_EFR32_DMA_INTERRUPT_PRIORITY 7
#endif

/**
 * Specify the set of USART interfaces that are to be used as SPI bus
 * controllers.
 */
#ifndef GMOS_CONFIG_EFR32_SPI_BUS_CONTROLLERS
#define GMOS_CONFIG_EFR32_SPI_BUS_CONTROLLERS \
        { GMOS_PAL_SPI_BUS_ID_EUSART0, GMOS_PAL_SPI_BUS_ID_EUSART1 }
#endif

/**
 * Specify the IIC bus controller transaction timeout delay as an
 * integer number of milliseconds. This is the time between sending the
 * transaction start bit and abandoning the transaction due to an
 * unresponsive bus target.
 */
#ifndef GMOS_CONFIG_EFR32_IIC_BUS_TIMEOUT
#define GMOS_CONFIG_EFR32_IIC_BUS_TIMEOUT 5000
#endif

/*
 * The Gecko SDK includes the 'nano' version of the C standard library,
 * so the optimised memcpy implementations can be used for stream and
 * buffer data transfers.
 */
#define GMOS_CONFIG_STREAMS_USE_MEMCPY 1
#define GMOS_CONFIG_BUFFERS_USE_MEMCPY 1

/*
 * Set the system timer frequency. This is a fixed value that is derived
 * by dividing the 32.768kHz LFXO clock by 32.
 */
#define GMOS_CONFIG_SYSTEM_TIMER_FREQUENCY 1024

#endif // GMOS_PAL_CONFIG_H