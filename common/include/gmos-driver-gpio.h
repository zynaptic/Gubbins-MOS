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

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

// These constants define the standard output driver configuration
// options to be used when configuring GPIO pins.
#define GMOS_DRIVER_GPIO_OUTPUT_PUSH_PULL  false
#define GMOS_DRIVER_GPIO_OUTPUT_OPEN_DRAIN true

// These constants define the standard GPIO input pin pullup or pulldown
// options to be used when configuring GPIO pins.
#define GMOS_DRIVER_GPIO_INPUT_PULL_NONE 0
#define GMOS_DRIVER_GPIO_INPUT_PULL_UP   1
#define GMOS_DRIVER_GPIO_INPUT_PULL_DOWN -1

/**
 * Defines the function prototype to be used for GPIO interrupt service
 * routine callbacks.
 */
typedef void (*gmosDriverGpioIsr_t) (void);

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
 * @param biasResistor This specifies the input bias resistor
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

/**
 * Initialises a general purpose IO pin for interrupt generation. This
 * should be called for each interrupt input GPIO pin prior to accessing
 * it via any of the other API functions. The interrupt is not enabled
 * at this stage.
 * @param gpioPinId This is the platform specific GPIO pin identifier
 *     that is associated with the GPIO pin being initialised.
 * @param gpioIsr This is the GPIO interrupt service routine function
 *     that is being registered with the specified GPIO interrupt pin.
 *     It will be called in the interrupt service context so can only
 *     use the ISR safe GubbinsMOS API calls.
 * @param biasResistor This specifies the input bias resistor
 *     configuration. A value of zero selects no bias resistor, negative
 *     values select a pull down resistor, and positive values select
 *     a pull up resistor. For devices that support multiple bias
 *     resistor values, increasing the magnitude of this value maps to
 *     an increase in the bias resistor value.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully setting up the GPIO pin interrupts and 'false' on
 *     failure.
 */
bool gmosDriverGpioInterruptInit (uint16_t gpioPinId,
    gmosDriverGpioIsr_t gpioIsr, int8_t biasResistor);

/**
 * Enables a GPIO interrupt for rising and/or falling edge detection.
 * This should be called after initialising a general purpose IO pin
 * as an interrupt source in order to receive interrupt notifications.
 * @param gpioPinId This is the platform specific GPIO pin identifier
 *     that is associated with the GPIO pin being used as an interrupt
 *     source.
 * @param risingEdge If set to 'true' this indicates that interrupts
 *     should be generated when the input to the GPIO pin transitions
 *     from low to high.
 * @param fallingEdge If set to 'true' this indicates that interrupts
 *     should be generated when the input to the GPIO pin transitions
 *     from high to low.
 */
void gmosDriverGpioInterruptEnable (uint16_t gpioPinId,
    bool risingEdge, bool fallingEdge);

/**
 * Disables a GPIO interrupt for the specified GPIO pin. This should be
 * called after enabling a general purpose IO pin as an interrupt source
 * in order to stop receiving interrupt notifications.
 * @param gpioPinId This is the platform specific GPIO pin identifier
 *     that is associated with the GPIO pin being used as an interrupt
 *     source.
 */
void gmosDriverGpioInterruptDisable (uint16_t gpioPinId);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // GMOS_DRIVER_GPIO_H
