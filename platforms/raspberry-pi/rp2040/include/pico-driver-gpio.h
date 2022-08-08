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
 * This header provides device specific GPIO definitions and functions
 * for the Raspberry Pi Pico RP2040 range of devices.
 */

#ifndef PICO_DRIVER_GPIO_H
#define PICO_DRIVER_GPIO_H

#include <stdint.h>
#include "gmos-driver-gpio.h"

// These constants define the RP2040 GPIO bank encoding used when
// configuring GPIO pins.
#define PICO_GPIO_BANK_U 0x0000  // User GPIO bank.
#define PICO_GPIO_BANK_P 0x1000  // Program interface GPIO bank.

// These constants define the RP2040 output driver slew rate speed
// and drive strength options to be used when configuring GPIO pins.
#define PICO_GPIO_DRIVER_SLEW_SLOW_2MA    GMOS_DRIVER_GPIO_SLEW_MINIMUM
#define PICO_GPIO_DRIVER_SLEW_SLOW_4MA    0x01
#define PICO_GPIO_DRIVER_SLEW_SLOW_8MA    0x02
#define PICO_GPIO_DRIVER_SLEW_SLOW_12MA   0x03
#define PICO_GPIO_DRIVER_SLEW_FAST_2MA    0x10
#define PICO_GPIO_DRIVER_SLEW_FAST_4MA    0x11
#define PICO_GPIO_DRIVER_SLEW_FAST_8MA    0x12
#define PICO_GPIO_DRIVER_SLEW_FAST_12MA   GMOS_DRIVER_GPIO_SLEW_MAXIMUM

/**
 * Initialises the GPIO platform abstraction layer on startup.
 */
void gmosPalGpioInit (void);

/**
 * Sets up one of the RP2040 GPIO pins for alternate function use.
 * @param gpioPinId This is the platform specific GPIO pin identifier
 *     that is associated with the GPIO pin being initialised.
 * @param driveStrength This specifies the output driver strength. Low
 *     values imply slower, lower power switching and higher values
 *     imply faster, more power hungry switching. The range of supported
 *     values will depend on the underlying hardware.
 * @param biasResistor This specifies the output bias resistor
 *     configuration. A value of zero selects no bias resistor, negative
 *     values select a pull down resistor, and positive values select
 *     a pull up resistor. For devices that support multiple bias
 *     resistor values, increasing the magnitude of this value maps to
 *     an increase in the bias resistor value.
 * @param altFunction This is the GPIO alternate function selection.
 *     It should be in the range from 0 to 31 and will be a device
 *     specific value.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully setting up the GPIO pin and 'false' on failure.
 */
bool gmosDriverGpioAltModeInit (uint16_t gpioPinId,
    uint8_t driveStrength, int8_t biasResistor, uint8_t altFunction);

#endif // PICO_DRIVER_GPIO_H
