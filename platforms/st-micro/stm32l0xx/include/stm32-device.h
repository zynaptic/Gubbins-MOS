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
 * This header provides device specific definitions for the STM32L0XX
 * range of devices. Most of these definitions are provided by the
 * vendor device headers, which are selected according to the target
 * device for the build.
 */

#ifndef STM32_DEVICE_H
#define STM32_DEVICE_H

// Enumerate the supported devices.
#define STM32L010RB 1

// Select the appropriate vendor header.
#if (TARGET_DEVICE == STM32L010RB)
#include "stm32l010xb.h"

// No vendor header available.
#else
#error ("STM32L0XX Device Not Currently Supported");
#endif

/**
 * Initialises the STM32 system timer implementation using the 16-bit
 * low power timer.
 */
void gmosPalSystemTimerInit (void);

/**
 * Initialises the STM32 serial debug console using USART2 and
 * optionally DMA channel 4.
 */
void gmosPalSerialConsoleInit (void);

/**
 * Writes the contents of the specified write data buffer to the STM32
 * serial debug console.
 * @param writeData This is a pointer to the write data buffer that is
 *     to be written to the serial debug console.
 * @param writeSize This specifies the number of bytes in the write data
 *     buffer that are to be written to the serial debug console.
 * @return Returns a boolean value which will be set to 'true' if all
 *     the contents of the write data buffer could be queued for
 *     transmission and 'false' if there is currently insufficient space
 *     in the serial console transmit queue.
 */
bool gmosPalSerialConsoleWrite (uint8_t* writeData, uint16_t writeSize);

#endif // STM32_DEVICE_H
