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
 * This header provides device specific GPIO definitions and functions
 * for Microchip PIC32 devices that utilise the Harmony V2 vendor
 * framework.
 */

#ifndef HARMONY_DRIVER_GPIO_H
#define HARMONY_DRIVER_GPIO_H

// These constants define the target GPIO bank encoding used when
// configuring GPIO pins on PIC32 devices.
#define HARMONY_GPIO_BANK_A 0x0000
#define HARMONY_GPIO_BANK_B 0x0100
#define HARMONY_GPIO_BANK_C 0x0200
#define HARMONY_GPIO_BANK_D 0x0300
#define HARMONY_GPIO_BANK_E 0x0400
#define HARMONY_GPIO_BANK_F 0x0500
#define HARMONY_GPIO_BANK_G 0x0600
#define HARMONY_GPIO_BANK_H 0x0700
#define HARMONY_GPIO_BANK_J 0x0800
#define HARMONY_GPIO_BANK_K 0x0900

// These constants define the PIC32 output driver slew rate speed
// options to be used when configuring GPIO pins. The PIC32 slew rate
// management is quite convoluted, so only the default setting is
// supported.
#define HARMONY_GPIO_DRIVER_SLEW_DEFAULT 0

// Specify the pins used for dedicated interrupt inputs. This depends on
// the selected target device. The list of supported external interrupt
// pins includes the GPIO pin ID in the lower 16 bits and the interrupt
// number in the upper 8 bits.
#if defined(__PIC32MZ__)
#define HARMONY_GPIO_EXTINT_NUM 5
#define HARMONY_GPIO_EXTINT_PINS {           \
    ((0 << 24) | HARMONY_GPIO_BANK_D | 0),   \
    ((1 << 24) | HARMONY_GPIO_BANK_D | 1),   \
    ((1 << 24) | HARMONY_GPIO_BANK_G | 9),   \
    ((1 << 24) | HARMONY_GPIO_BANK_B | 14),  \
    ((1 << 24) | HARMONY_GPIO_BANK_B | 6),   \
    ((1 << 24) | HARMONY_GPIO_BANK_D | 5),   \
    ((1 << 24) | HARMONY_GPIO_BANK_B | 2),   \
    ((1 << 24) | HARMONY_GPIO_BANK_F | 3),   \
    ((1 << 24) | HARMONY_GPIO_BANK_F | 13),  \
    ((1 << 24) | HARMONY_GPIO_BANK_F | 2),   \
    ((1 << 24) | HARMONY_GPIO_BANK_C | 2),   \
    ((1 << 24) | HARMONY_GPIO_BANK_E | 8),   \
    ((2 << 24) | HARMONY_GPIO_BANK_D | 9),   \
    ((2 << 24) | HARMONY_GPIO_BANK_G | 6),   \
    ((2 << 24) | HARMONY_GPIO_BANK_B | 8),   \
    ((2 << 24) | HARMONY_GPIO_BANK_B | 15),  \
    ((2 << 24) | HARMONY_GPIO_BANK_D | 4),   \
    ((2 << 24) | HARMONY_GPIO_BANK_B | 0),   \
    ((2 << 24) | HARMONY_GPIO_BANK_E | 3),   \
    ((2 << 24) | HARMONY_GPIO_BANK_B | 7),   \
    ((2 << 24) | HARMONY_GPIO_BANK_F | 12),  \
    ((2 << 24) | HARMONY_GPIO_BANK_D | 12),  \
    ((2 << 24) | HARMONY_GPIO_BANK_F | 8),   \
    ((2 << 24) | HARMONY_GPIO_BANK_C | 3),   \
    ((2 << 24) | HARMONY_GPIO_BANK_E | 9),   \
    ((3 << 24) | HARMONY_GPIO_BANK_D | 2),   \
    ((3 << 24) | HARMONY_GPIO_BANK_G | 8),   \
    ((3 << 24) | HARMONY_GPIO_BANK_F | 4),   \
    ((3 << 24) | HARMONY_GPIO_BANK_D | 10),  \
    ((3 << 24) | HARMONY_GPIO_BANK_F | 1),   \
    ((3 << 24) | HARMONY_GPIO_BANK_B | 9),   \
    ((3 << 24) | HARMONY_GPIO_BANK_B | 10),  \
    ((3 << 24) | HARMONY_GPIO_BANK_C | 14),  \
    ((3 << 24) | HARMONY_GPIO_BANK_B | 5),   \
    ((3 << 24) | HARMONY_GPIO_BANK_C | 1),   \
    ((3 << 24) | HARMONY_GPIO_BANK_D | 14),  \
    ((3 << 24) | HARMONY_GPIO_BANK_G | 1),   \
    ((3 << 24) | HARMONY_GPIO_BANK_A | 14),  \
    ((3 << 24) | HARMONY_GPIO_BANK_D | 6),   \
    ((4 << 24) | HARMONY_GPIO_BANK_D | 3),   \
    ((4 << 24) | HARMONY_GPIO_BANK_G | 7),   \
    ((4 << 24) | HARMONY_GPIO_BANK_F | 5),   \
    ((4 << 24) | HARMONY_GPIO_BANK_D | 11),  \
    ((4 << 24) | HARMONY_GPIO_BANK_F | 0),   \
    ((4 << 24) | HARMONY_GPIO_BANK_B | 1),   \
    ((4 << 24) | HARMONY_GPIO_BANK_E | 5),   \
    ((4 << 24) | HARMONY_GPIO_BANK_C | 13),  \
    ((4 << 24) | HARMONY_GPIO_BANK_B | 3),   \
    ((4 << 24) | HARMONY_GPIO_BANK_C | 4),   \
    ((4 << 24) | HARMONY_GPIO_BANK_D | 15),  \
    ((4 << 24) | HARMONY_GPIO_BANK_G | 0),   \
    ((4 << 24) | HARMONY_GPIO_BANK_A | 15),  \
    ((4 << 24) | HARMONY_GPIO_BANK_D | 7)    \
}

// Device not currently supported.
#else
#error ("Microchip Harmony Target Device Not Supported By GPIO Driver");
#endif

#endif // HARMONY_DRIVER_GPIO_H
