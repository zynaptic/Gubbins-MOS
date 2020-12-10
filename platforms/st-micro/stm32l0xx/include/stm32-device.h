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

// These constants define the STM32 GPIO bank encoding used when
// configuring GPIO pins.
#define STM32_GPIO_BANK_A 0
#define STM32_GPIO_BANK_B 1
#define STM32_GPIO_BANK_C 2
#define STM32_GPIO_BANK_D 3
#define STM32_GPIO_BANK_E 4
#define STM32_GPIO_BANK_H 7

// These constants define the STM32 output driver configuration options
// to be used when configuring GPIO pins.
#define STM32_GPIO_DRIVER_PUSH_PULL  0
#define STM32_GPIO_DRIVER_OPEN_DRAIN 1

// These constants define the STM32 output driver slew rate speed
// options to be used when configuring GPIO pins.
#define STM32_GPIO_DRIVER_SLEW_SLOW    0
#define STM32_GPIO_DRIVER_SLEW_MEDIUM  1
#define STM32_GPIO_DRIVER_SLEW_FAST    2
#define STM32_GPIO_DRIVER_SLEW_MAXIMUM 3

// These constants define the STM32 pin pullup or pulldown options to
// be used when configuring GPIO pins.
#define STM32_GPIO_INPUT_PULL_NONE 0
#define STM32_GPIO_INPUT_PULL_UP   1
#define STM32_GPIO_INPUT_PULL_DOWN 2

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

/**
 * Sets up one of the STM32 GPIO pins for alternate function use.
 * @param pinBank This is the GPIO bank to which the selected GPIO pin
 *     belongs. It should be one of the defined GPIO bank encoding
 *     values.
 * @param pinIndex This is the GPIO pin index within the GPIO bank. It
 *     should be in the range from 0 to 15.
 * @param driverType This is the GPIO output driver type to be used,
 *     selecting between push-pull and open drain configurations.
 * @param slewRate This is the GPIO output driver slew rate speed. It
 *     should be one of the defined GPIO slew rate values.
 * @param pullUpOrDown This is the GPIO input pull up or pull down
 *     configuration. It should be one of the defined GPIO pull up or
 *     pull down values.
 * @param altFunction This is the GPIO alternate function selection.
 *     It should be in the range from 0 to 15 and will be a device
 *     specific value.
 */
void gmosPalGpioSetAltFunction (uint32_t pinBank, uint32_t pinIndex,
    uint32_t driverType, uint32_t slewRate, uint32_t pullUpOrDown,
    uint32_t altFunction);

#endif // STM32_DEVICE_H
