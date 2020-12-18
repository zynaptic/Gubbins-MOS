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
 * This header defines the common API for accessing microcontroller GPIO
 * pins.
 */

#ifndef GMOS_DRIVER_GPIO_H
#define GMOS_DRIVER_GPIO_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * Initialises a general purpose IO pin for conventional use. This
 * should be called for each conventional GPIO pin prior to accessing it
 * via any of the other API functions. After initialisation the pin will
 * be configured as an input by default.
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
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully setting up the GPIO pin and 'false' on failure.
 */
bool gmosDriverGpioPinInit (uint16_t gpioPinId, bool openDrain,
    uint8_t driveStrength, int8_t biasResistor);

/**
 * Sets a general purpose IO pin as a conventional input, using the
 * configuration previously assigned by the 'gmosDriverGpioPinInit'
 * function.
 * @param gpioPinId This is the platform specific GPIO pin identifier
 *     that is associated with the GPIO pin being updated.
 */
bool gmosDriverGpioSetAsInput (uint16_t gpioPinId);

/**
 * Sets a general purpose IO pin as a conventional output, using the
 * configuration previously assigned by the 'gmosDriverGpioPinInit'
 * function.
 * @param gpioPinId This is the platform specific GPIO pin identifier
 *     that is associated with the GPIO pin being updated.
 */
bool gmosDriverGpioSetAsOutput (uint16_t gpioPinId);

/**
 * Sets the GPIO pin state. If the GPIO is configured as an output this
 * will update the output value.
 * @param gpioPinId This is the platform specific GPIO pin identifier
 *     that is associated with the GPIO pin being updated.
 * @param pinState This is the GPIO output pin state. A value of 'true'
 *     drives the pin high and a value of 'false' drives the pin low.
 */
void gmosDriverGpioSetPinState (uint16_t gpioPinId, bool pinState);

/**
 * Gets the GPIO pin state. If the GPIO is configured as an input this
 * will be the sampled value and if configured as an output this will
 * be the current output value.
 * @param gpioPinId This is the platform specific GPIO pin identifier
 *     that is associated with the GPIO pin being sampled.
 * @return Returns a boolean value which will be set to 'true' if the
 *     pin level is high and a value of 'false' if the pin level is low.
 */
bool gmosDriverGpioGetPinState (uint16_t gpioPinId);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // GMOS_DRIVER_GPIO_H
