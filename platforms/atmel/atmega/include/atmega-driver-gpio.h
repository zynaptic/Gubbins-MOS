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
 * for the Microchip/Atmel ATMEGA range of devices.
 */

#ifndef ATMEGA_DRIVER_GPIO_H
#define ATMEGA_DRIVER_GPIO_H

#include "atmega-device.h"

// These constants define the ATMEGA GPIO bank encoding used when
// configuring GPIO pins. Note that only the first eight ports are
// currently supported.
#define ATMEGA_GPIO_BANK_A 0x0000
#define ATMEGA_GPIO_BANK_B 0x0100
#define ATMEGA_GPIO_BANK_C 0x0200
#define ATMEGA_GPIO_BANK_D 0x0300
#define ATMEGA_GPIO_BANK_E 0x0400
#define ATMEGA_GPIO_BANK_F 0x0500
#define ATMEGA_GPIO_BANK_G 0x0600
#define ATMEGA_GPIO_BANK_H 0x0700

// These constants define the ATMEGA output driver slew rate speed
// options to be used when configuring GPIO pins. This is not a
// configurable option on ATMEGA devices.
#define ATMEGA_GPIO_DRIVER_SLEW_FIXED 0

// Specify the pins and registers used for dedicated interrupt inputs.
// This depends on the selected target device.
#if ((TARGET_DEVICE == atmega32) || (TARGET_DEVICE == atmega32a))
#define ATMEGA_GPIO_EXTINT_CFG_REG MCUCR
#define ATMEGA_GPIO_EXTINT_MSK_REG GICR
#define ATMEGA_GPIO_EXTINT_NUM 3
#define ATMEGA_GPIO_EXTINT_PINS {(ATMEGA_GPIO_BANK_D | 2), \
    (ATMEGA_GPIO_BANK_D | 3), (ATMEGA_GPIO_BANK_B | 2)}

#elif (TARGET_DEVICE == atmega328p)
#define ATMEGA_GPIO_EXTINT_CFG_REG EICRA
#define ATMEGA_GPIO_EXTINT_MSK_REG EIMSK
#define ATMEGA_GPIO_EXTINT_NUM 2
#define ATMEGA_GPIO_EXTINT_PINS {(ATMEGA_GPIO_BANK_D | 2), \
    (ATMEGA_GPIO_BANK_D | 3)}

// Device not currently supported.
#else
#error ("ATMEGA Target Device Not Supported By GPIO Driver");
#endif

#endif // ATMEGA_DRIVER_GPIO_H
