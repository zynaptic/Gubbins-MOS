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
 * This header provides device specific GPIO definitions and functions
 * for Silicon Labs EFR32xG2x family devices.
 */

#ifndef EFR32_DRIVER_GPIO_H
#define EFR32_DRIVER_GPIO_H

// These constants define the EFR32 GPIO bank encoding used when
// configuring GPIO pins.
#define EFR32_GPIO_BANK_A 0x0000
#define EFR32_GPIO_BANK_B 0x0100
#define EFR32_GPIO_BANK_C 0x0200
#define EFR32_GPIO_BANK_D 0x0300

// These constants define the EFR32 output driver slew rate speed
// options to be used when configuring GPIO pins.
#define EFR32_GPIO_DRIVER_SLEW_LIMITED 0
#define EFR32_GPIO_DRIVER_SLEW_SLOW    1
#define EFR32_GPIO_DRIVER_SLEW_MEDIUM  2
#define EFR32_GPIO_DRIVER_SLEW_FAST    4
#define EFR32_GPIO_DRIVER_SLEW_MAXIMUM 6
#define EFR32_GPIO_DRIVER_SLEW_MASK    7

// The EFR32 supports input signal filtering. This option can be
// selected as part of the slew rate setting by performing a bitwise OR
// between the required slew rate and this signal filter flag.
#define EFR32_GPIO_DRIVER_SLEW_FILTER  8

/**
 * Initialises the GPIO platform abstraction layer on startup.
 */
void gmosPalGpioInit (void);

#endif // EFR32_DRIVER_GPIO_H
