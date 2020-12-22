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
 * This header provides device specific GPIO definitions and functions
 * for the STM32L0XX range of devices.
 */

#ifndef STM32_DRIVER_GPIO_H
#define STM32_DRIVER_GPIO_H

// These constants define the STM32 GPIO bank encoding used when
// configuring GPIO pins.
#define STM32_GPIO_BANK_A 0x0000
#define STM32_GPIO_BANK_B 0x0100
#define STM32_GPIO_BANK_C 0x0200
#define STM32_GPIO_BANK_D 0x0300
#define STM32_GPIO_BANK_E 0x0400
#define STM32_GPIO_BANK_H 0x0700

// These constants define the STM32 output driver slew rate speed
// options to be used when configuring GPIO pins.
#define STM32_GPIO_DRIVER_SLEW_SLOW    0
#define STM32_GPIO_DRIVER_SLEW_MEDIUM  1
#define STM32_GPIO_DRIVER_SLEW_FAST    2
#define STM32_GPIO_DRIVER_SLEW_MAXIMUM 3

/**
 * Sets up one of the STM32 GPIO pins for alternate function use.
 * @param gpioPinId This is the platform specific GPIO pin identifier
 *     that is associated with the GPIO pin being initialised.
 * @param openDrain This is a boolean value which when set to 'true'
 *     specifies that an open drain output should be used instead of the
 *     default push/pull driver operation.
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
 *     It should be in the range from 0 to 15 and will be a device
 *     specific value.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully setting up the GPIO pin and 'false' on failure.
 */
bool gmosDriverGpioAltModeInit (uint16_t gpioPinId, bool openDrain,
    uint8_t driveStrength, int8_t biasResistor, uint8_t altFunction);

#endif // STM32_DRIVER_GPIO_H
