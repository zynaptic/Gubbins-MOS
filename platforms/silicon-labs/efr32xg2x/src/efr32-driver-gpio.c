/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2023-2025 Zynaptic Limited
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
 * Implements GPIO driver functionality for the Silicon Labs EFR32xG2x
 * series of microcontrollers by wrapping the standard Simplicity SDK
 * GPIO driver.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-driver-gpio.h"
#include "efr32-driver-gpio.h"
#include "sl_status.h"
#include "sl_gpio.h"
#include "sl_hal_gpio.h"
#include "sl_hal_bus.h"

// This is the set of GPIO open drain configuration flags.
static uint16_t gmosDriverGpioOpenDrainFlags [4];

// This is the set of GPIO filter mode configuration flags.
static uint16_t gmosDriverGpioFilterFlags [4];

// This is the set of GPIO pullup and pulldown configuration flags.
static uint16_t gmosDriverGpioPullupFlags [4];
static uint16_t gmosDriverGpioPulldownFlags [4];

// This is the set of GPIO output enable flags.
static uint16_t gmosDriverGpioOutputEnFlags [4];

// Allocate the specified number of ISR slots, using an array of GPIO
// pin IDs and an array of associated callback functions.
static gmosDriverGpioIsr_t gpioIsrHandlers [GMOS_CONFIG_EFR32_GPIO_MAX_ISRS];
static uint16_t gpioIsrPinIds [GMOS_CONFIG_EFR32_GPIO_MAX_ISRS];
static uint8_t gpioIsrIntIds [GMOS_CONFIG_EFR32_GPIO_MAX_ISRS];
static void* gpioIsrDataItems [GMOS_CONFIG_EFR32_GPIO_MAX_ISRS];

/*
 * Map GubbinsMOS pin identifiers to Simplicity SDK pin identifiers.
 * This relies on both encodings using the same enumeration order for
 * both bank and pin identifiers.
 */
static inline void gmosPalGpioMapPinId (
    uint_fast16_t gmosGpioPinId, sl_gpio_t* slGpioPinId)
{
    // Perform GPIO bank mapping.
    slGpioPinId->port = (uint8_t) (gmosGpioPinId >> 8);

    // Perform GPIO pin mapping.
    slGpioPinId->pin = (uint8_t) (gmosGpioPinId);
}

/*
 * Sets the slew rate for conventional GPIO pins. The slew rate is set
 * for each GPIO bank, so the maximum requested slew rate for all pins
 * on that bank is the one which will be used.
 */
static inline bool gmosDriverGpioSetSlewRate (
    uint_fast8_t gpioPort, uint_fast8_t driveStrength)
{
    sl_status_t slStatus;
    uint8_t currentSlewRate;

    // Check the current slew rate and only update it if required.
    slStatus = sl_gpio_get_slew_rate (gpioPort, &currentSlewRate);
    if ((slStatus == SL_STATUS_OK) && (driveStrength > currentSlewRate)) {
        slStatus = sl_gpio_set_slew_rate (gpioPort, driveStrength);
    }
    return (slStatus == SL_STATUS_OK) ? true : false;
}

/*
 * Initialises a general purpose IO pin for conventional use. For the
 * EFR32xG2x series of devices, the upper byte of the GPIO pin ID is
 * used to select the GPIO bank and the lower byte is used to select
 * the pin number.
 */
bool gmosDriverGpioPinInit (uint16_t gpioPinId, bool openDrain,
    uint8_t driveStrength, int8_t biasResistor)
{
    bool initOk;
    uint_fast8_t gpioPort;
    uint_fast8_t gpioPin;
    uint_fast16_t gpioPinMask;

    // Extract the GPIO port and pin ID.
    gpioPort = (gpioPinId >> 8) & 0x03;
    gpioPin = gpioPinId & 0x0F;
    gpioPinMask = (1 << gpioPin);

    // Set the appropriate open drain configuration.
    if (openDrain) {
        gmosDriverGpioOpenDrainFlags [gpioPort] |= gpioPinMask;
    } else {
        gmosDriverGpioOpenDrainFlags [gpioPort] &= ~gpioPinMask;
    }

    // Set the glitch filter option.
    if ((driveStrength & EFR32_GPIO_DRIVER_SLEW_FILTER) != 0) {
        gmosDriverGpioFilterFlags [gpioPort] |= gpioPinMask;
    } else {
        gmosDriverGpioFilterFlags [gpioPort] &= ~gpioPinMask;
    }

    // Set the appropriate pullup or pulldown configuration.
    if (biasResistor > 0) {
        gmosDriverGpioPullupFlags [gpioPort] |= gpioPinMask;
        gmosDriverGpioPulldownFlags [gpioPort] &= ~gpioPinMask;
    } else if (biasResistor < 0) {
        gmosDriverGpioPullupFlags [gpioPort] &= ~gpioPinMask;
        gmosDriverGpioPulldownFlags [gpioPort] |= gpioPinMask;
    } else {
        gmosDriverGpioPullupFlags [gpioPort] &= ~gpioPinMask;
        gmosDriverGpioPulldownFlags [gpioPort] &= ~gpioPinMask;
    }

    // Set the GPIO drive strength.
    initOk = gmosDriverGpioSetSlewRate (
        gpioPort, driveStrength & EFR32_GPIO_DRIVER_SLEW_MASK);

    // Once all the pin options have been set, it is configured as an
    // input by default.
    return initOk && gmosDriverGpioSetAsInput (gpioPinId);
}

/*
 * Sets a general purpose IO pin as a conventional input, using the
 * configuration previously assigned by the 'gmosDriverGpioPinInit'
 * function.
 */
bool gmosDriverGpioSetAsInput (uint16_t gpioPinId)
{
    uint_fast8_t gpioPort;
    uint_fast8_t gpioPin;
    uint_fast16_t gpioPinMask;
    uint_fast16_t gpioFilterEn;
    uint_fast16_t gpioPullupEn;
    uint_fast16_t gpioPulldownEn;
    sl_gpio_t slGpioPinId;
    sl_gpio_mode_t gpioMode;
    bool gpioOption;
    sl_status_t slStatus;

    // Extract the GPIO port and pin ID.
    gpioPort = (gpioPinId >> 8) & 0x03;
    gpioPin = gpioPinId & 0x0F;
    gpioPinMask = (1 << gpioPin);

    // Extract the GPIO status flags.
    gpioFilterEn = gmosDriverGpioFilterFlags [gpioPort] & gpioPinMask;
    gpioPullupEn = gmosDriverGpioPullupFlags [gpioPort] & gpioPinMask;
    gpioPulldownEn = gmosDriverGpioPulldownFlags [gpioPort] & gpioPinMask;
    gmosDriverGpioOutputEnFlags [gpioPort] &= ~gpioPinMask;

    // Select input without any bias resistors. Setting the option flag
    // enables the input glitch filter.
    if ((gpioPullupEn == 0) && (gpioPulldownEn == 0)) {
        gpioMode = SL_GPIO_MODE_INPUT;
        gpioOption = (gpioFilterEn == 0) ? false : true;
    }

    // Select input with bias resistor and no filter. Setting the option
    // flag enables pullup operation.
    else if (gpioFilterEn == 0) {
        gpioMode = SL_GPIO_MODE_INPUT_PULL;
        gpioOption = (gpioPullupEn == 0) ? false : true;
    }

    // Select input with bias resistor and input glitch filter. Setting
    // the option flag enables pullup operation.
    else {
        gpioMode = SL_GPIO_MODE_INPUT_PULL_FILTER;
        gpioOption = (gpioPullupEn == 0) ? false : true;
    }

    // Set the input pin mode via the SDK.
    gmosPalGpioMapPinId (gpioPinId, &slGpioPinId);
    slStatus = sl_gpio_set_pin_mode (&slGpioPinId, gpioMode, gpioOption);
    return (slStatus == SL_STATUS_OK) ? true : false;
}

/*
 * Sets a general purpose IO pin as a conventional output, using the
 * configuration previously assigned by the 'gmosDriverGpioPinInit'
 * function.
 */
bool gmosDriverGpioSetAsOutput (uint16_t gpioPinId)
{
    uint_fast8_t gpioPort;
    uint_fast8_t gpioPin;
    uint_fast16_t gpioPinMask;
    uint_fast16_t gpioOpenDrainEn;
    uint_fast16_t gpioFilterEn;
    uint_fast16_t gpioPullupEn;
    sl_gpio_t slGpioPinId;
    sl_gpio_mode_t gpioMode;
    bool gpioOutput;
    sl_status_t slStatus;

    // Extract the GPIO port and pin ID.
    gpioPort = (gpioPinId >> 8) & 0x03;
    gpioPin = gpioPinId & 0x0F;
    gpioPinMask = (1 << gpioPin);

    // Extract the GPIO status flags.
    gpioOpenDrainEn = gmosDriverGpioOpenDrainFlags [gpioPort] & gpioPinMask;
    gpioFilterEn = gmosDriverGpioFilterFlags [gpioPort] & gpioPinMask;
    gpioPullupEn = gmosDriverGpioPullupFlags [gpioPort] & gpioPinMask;
    gmosDriverGpioOutputEnFlags [gpioPort] |= gpioPinMask;

    // Select conventional push-pull output driver. The output is set
    // low by default.
    if (gpioOpenDrainEn == 0) {
        gpioMode = SL_GPIO_MODE_PUSH_PULL;
        gpioOutput = false;
    }

    // Select conventional open drain. The output is not driven.
    else if ((gpioFilterEn == 0) && (gpioPullupEn == 0)) {
        gpioMode = SL_GPIO_MODE_WIRED_AND;
        gpioOutput = true;
    }

    // Select open drain with pullup.
    else if ((gpioFilterEn == 0) && (gpioPullupEn != 0)) {
        gpioMode = SL_GPIO_MODE_WIRED_AND_PULLUP;
        gpioOutput = true;
    }

    // Select open drain with input filter.
    else if ((gpioFilterEn != 0) && (gpioPullupEn == 0)) {
        gpioMode = SL_GPIO_MODE_WIRED_AND_FILTER;
        gpioOutput = true;
    }

    // Select open drain with both pullup and input filter.
    else {
        gpioMode = SL_GPIO_MODE_WIRED_AND_PULLUP_FILTER;
        gpioOutput = true;
    }

    // Set the output pin mode via the SDK.
    gmosPalGpioMapPinId (gpioPinId, &slGpioPinId);
    slStatus = sl_gpio_set_pin_mode (&slGpioPinId, gpioMode, gpioOutput);
    return (slStatus == SL_STATUS_OK) ? true : false;
}

/*
 * Sets the GPIO pin state. If the GPIO is configured as an output this
 * will update the output value.
 */
void gmosDriverGpioSetPinState (uint16_t gpioPinId, bool pinState)
{
    uint_fast8_t gpioPort;
    uint_fast8_t gpioPin;
    uint_fast16_t gpioPinMask;
    uint_fast16_t gpioOutputEn;
    sl_gpio_t slGpioPinId;
    sl_status_t slStatus;

    // Extract the GPIO port and pin ID.
    gpioPort = (gpioPinId >> 8) & 0x03;
    gpioPin = gpioPinId & 0x0F;
    gpioPinMask = (1 << gpioPin);

    // Extract the GPIO status flags.
    gpioOutputEn = gmosDriverGpioOutputEnFlags [gpioPort] & gpioPinMask;

    // Only set the pin state if the pin output is enabled.
    if (gpioOutputEn != 0) {
        gmosPalGpioMapPinId (gpioPinId, &slGpioPinId);
        if (pinState) {
            slStatus = sl_gpio_set_pin (&slGpioPinId);
        } else {
            slStatus = sl_gpio_clear_pin (&slGpioPinId);
        }
        if (slStatus != SL_STATUS_OK) {
            GMOS_LOG_FMT (LOG_DEBUG,
                "Error setting GPIO pin 0x%04X (status 0x%04X).",
                gpioPinId, slStatus);
        }
    }
}

/*
 * Gets the GPIO pin state. If the GPIO is configured as an input this
 * will be the sampled value and if configured as an output this will
 * be the current output value, as seen on the pin.
 */
bool gmosDriverGpioGetPinState (uint16_t gpioPinId)
{
    bool pinState = false;
    sl_gpio_t slGpioPinId;
    sl_status_t slStatus;

    // Read the state of the input buffer.
    gmosPalGpioMapPinId (gpioPinId, &slGpioPinId);
    slStatus = sl_gpio_get_pin_input (&slGpioPinId, &pinState);
    if (slStatus != SL_STATUS_OK) {
        GMOS_LOG_FMT (LOG_DEBUG,
            "Error reading GPIO pin 0x%04X (status 0x%04X).",
            gpioPinId, slStatus);
    }
    return pinState;
}

/*
 * Implement intermediate interrupt callback handler.
 */
static void gmosDriverGpioInterruptHandler (
    uint8_t interruptNumber, void *context)
{
    uint_fast8_t slot;
    gmosDriverGpioIsr_t gpioIsrHandler;

    // The interrupt handler slot can be inferred from the context,
    // which is a pointer to the correponding interrupt ID table entry.
    slot = (uint_fast8_t) ((uint8_t*) context - gpioIsrIntIds);

    // Sanity check the context before dispatching the interrupt.
    if ((slot < GMOS_CONFIG_EFR32_GPIO_MAX_ISRS) &&
        (gpioIsrIntIds [slot] == interruptNumber)) {
        gpioIsrHandler = gpioIsrHandlers [slot];
        if (gpioIsrHandler != NULL) {
            gpioIsrHandler (gpioIsrDataItems [slot]);
        }
    }
}

/*
 * Initialises a general purpose IO pin for interrupt generation. This
 * should be called for each interrupt input GPIO pin prior to accessing
 * it via any of the other API functions. The interrupt is not activated
 * at this stage, since neither rising edge or falling edge is selected.
 */
bool gmosDriverGpioInterruptInit (uint16_t gpioPinId,
    gmosDriverGpioIsr_t gpioIsr, void* gpioIsrData,
    int8_t biasResistor)
{
    bool initOk;
    uint_fast8_t slot;
    int32_t intId;
    sl_gpio_t slGpioPinId;
    sl_status_t slStatus;

    // Select the next available ISR callback slot.
    for (slot = 0; slot < GMOS_CONFIG_EFR32_GPIO_MAX_ISRS; slot++) {
        if (gpioIsrHandlers [slot] == NULL) {
            break;
        }
    }
    if (slot >= GMOS_CONFIG_EFR32_GPIO_MAX_ISRS) {
        initOk = false;
        goto out;
    }

    // Configure the GPIO pin and set it as an input.
    if (!gmosDriverGpioPinInit (gpioPinId, false, 0, biasResistor)) {
        initOk = false;
        goto out;
    }

    // Attempt to configure the interrupt source using an interrupt
    // number selected by the SDK driver. This also enables the
    // interrupt, which prevents it being claimed by other Simplicity
    // SDK drivers
    gmosPalGpioMapPinId (gpioPinId, &slGpioPinId);
    intId = SL_GPIO_INTERRUPT_UNAVAILABLE;
    slStatus = sl_gpio_configure_external_interrupt (
        &slGpioPinId, &intId, SL_GPIO_INTERRUPT_NO_EDGE,
        gmosDriverGpioInterruptHandler, &(gpioIsrIntIds [slot]));
    if (slStatus != SL_STATUS_OK) {
        initOk = false;
        goto out;
    }

    // Populate the local ISR slot.
    gpioIsrHandlers [slot] = gpioIsr;
    gpioIsrPinIds [slot] = gpioPinId;
    gpioIsrIntIds [slot] = intId;
    gpioIsrDataItems [slot] = gpioIsrData;
    initOk = true;
out:
    return initOk;
}

/*
 * Enables a GPIO interrupt for rising and/or falling edge detection.
 * This should be called after initialising a general purpose IO pin
 * as an interrupt source in order to receive interrupt notifications.
 */
void gmosDriverGpioInterruptEnable (uint16_t gpioPinId,
    bool risingEdge, bool fallingEdge)
{
    uint_fast8_t i;
    uint_fast8_t intId = 0xFF;

    // Determine the interrupt ID which is associated with the specified
    // GPIO pin.
    for (i = 0; i < GMOS_CONFIG_EFR32_GPIO_MAX_ISRS; i++) {
        if ((gpioIsrHandlers [i] != NULL) &&
            (gpioIsrPinIds [i] == gpioPinId)) {
            intId = gpioIsrIntIds [i];
            break;
        }
    }
    if (intId == 0xFF) {
        return;
    }

    // Set the rising and falling edge options. The interrupt is already
    // enabled as a result of the initialisation process, so this just
    // allows it to be triggered by GPIO input transitions.
    sl_hal_bus_reg_write_bit (&(GPIO->EXTIRISE), intId, risingEdge);
    sl_hal_bus_reg_write_bit (&(GPIO->EXTIFALL), intId, fallingEdge);
}

/*
 * Disables a GPIO interrupt for the specified GPIO pin. This should be
 * called after enabling a general purpose IO pin as an interrupt source
 * in order to stop receiving interrupt notifications.
 */
void gmosDriverGpioInterruptDisable (uint16_t gpioPinId)
{
    uint_fast8_t i;
    uint_fast8_t intId = 0xFF;

    // Determine the interrupt ID which is associated with the specified
    // GPIO pin.
    for (i = 0; i < GMOS_CONFIG_EFR32_GPIO_MAX_ISRS; i++) {
        if ((gpioIsrHandlers [i] != NULL) &&
            (gpioIsrPinIds [i] == gpioPinId)) {
            intId = gpioIsrIntIds [i];
            break;
        }
    }
    if (intId == 0xFF) {
        return;
    }

    // Clear the rising and falling edge options. The interrupt is left
    // enabled in order to prevent the interrupt number being claimed by
    // other Simplicity SDK drivers.
    sl_hal_bus_reg_write_bit (&(GPIO->EXTIRISE), intId, false);
    sl_hal_bus_reg_write_bit (&(GPIO->EXTIFALL), intId, false);
}

/*
 * Initialises the GPIO platform abstraction layer on startup.
 */
bool gmosPalGpioInit (void)
{
    bool initOk = true;
    uint_fast8_t i;
    sl_status_t slStatus;

    // Initialise the SDK driver.
    slStatus = sl_gpio_init ();
    if (slStatus != SL_STATUS_OK) {
        initOk = false;
        goto out;
    }

    // Clear the local configuration registers and set the bank slew
    // rates to their minimum setting.
    for (i = 0; i < 4; i++) {
        gmosDriverGpioOpenDrainFlags [i] = 0;
        gmosDriverGpioFilterFlags [i] = 0;
        gmosDriverGpioPullupFlags [i] = 0;
        gmosDriverGpioPulldownFlags [i] = 0;
        gmosDriverGpioOutputEnFlags [i] = 0;
        slStatus = sl_gpio_set_slew_rate (i, 0);
        if (slStatus != SL_STATUS_OK) {
            initOk = false;
            goto out;
        }
    }

    // Clear the ISR callback slots.
    for (i = 0; i < GMOS_CONFIG_EFR32_GPIO_MAX_ISRS; i++) {
        gpioIsrHandlers [i] = NULL;
    }
out :
    return initOk;
}
