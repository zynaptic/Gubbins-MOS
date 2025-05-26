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
 * series of microcontrollers.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-driver-gpio.h"
#include "efr32-driver-gpio.h"
#include "em_core.h"
#include "em_cmu.h"
#include "em_gpio.h"
#include "sl_interrupt_manager.h"

// This is the set of GPIO open drain configuration flags.
static uint16_t gmosDriverGpioOpenDrainFlags [4];

// This is the set of GPIO filter mode configuration flags.
static uint16_t gmosDriverGpioFilterFlags [4];

// This is the set of GPIO pullup and pulldown configuration flags.
static uint16_t gmosDriverGpioPullupFlags [4];
static uint16_t gmosDriverGpioPulldownFlags [4];

// This is the set of GPIO output enable flags.
static uint16_t gmosDriverGpioOutputEnFlags [4];

// This is the map of GPIO interrupt IDs to ISR slots.
static uint8_t gmosDriverGpioIsrMap [12];

// Allocate the specified number of ISR slots, using an array of GPIO
// pin IDs and an array of associated callback functions.
static gmosDriverGpioIsr_t gpioIsrHandlers [GMOS_CONFIG_EFR32_GPIO_MAX_ISRS];
static uint16_t gpioIsrPinIds [GMOS_CONFIG_EFR32_GPIO_MAX_ISRS];
static uint8_t gpioIsrIntIds [GMOS_CONFIG_EFR32_GPIO_MAX_ISRS];
static void* gpioIsrDataItems [GMOS_CONFIG_EFR32_GPIO_MAX_ISRS];

/*
 * Sets the slew rate for conventional GPIO pins. The slew rate is set
 * for each GPIO bank, so the maximum requested slew rate for all pins
 * on that bank is the one which will be used.
 */
static inline void gmosDriverGpioSetSlewRate (
    GPIO_Port_TypeDef gpioPort, uint8_t driveStrength)
{
    uint32_t gpioCtrlReg;
    uint32_t currentSlewRate;

    gpioCtrlReg = GPIO->P[gpioPort].CTRL;
    currentSlewRate = (gpioCtrlReg & _GPIO_P_CTRL_SLEWRATE_MASK) >>
        _GPIO_P_CTRL_SLEWRATE_SHIFT;
    if (driveStrength > currentSlewRate) {
        GPIO_SlewrateSet (gpioPort, driveStrength, 0);
    }
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
    GPIO_Port_TypeDef gpioPort;
    uint_fast16_t gpioPin;
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
    gmosDriverGpioSetSlewRate (
        gpioPort, driveStrength & EFR32_GPIO_DRIVER_SLEW_MASK);

    // Once all the pin options have been set, it is configured as an
    // input by default.
    return gmosDriverGpioSetAsInput (gpioPinId);
}

/*
 * Sets a general purpose IO pin as a conventional input, using the
 * configuration previously assigned by the 'gmosDriverGpioPinInit'
 * function.
 */
bool gmosDriverGpioSetAsInput (uint16_t gpioPinId)
{
    GPIO_Port_TypeDef gpioPort;
    uint_fast16_t gpioPin;
    uint_fast16_t gpioPinMask;
    uint_fast16_t gpioFilterEn;
    uint_fast16_t gpioPullupEn;
    uint_fast16_t gpioPulldownEn;
    GPIO_Mode_TypeDef gpioMode;
    uint32_t gpioOption;

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
        gpioMode = gpioModeInput;
        gpioOption = (gpioFilterEn == 0) ? 0 : 1;

    // Select input with bias resistor and no filter. Setting the option
    // flag enables pullup operation.
    } else if (gpioFilterEn == 0) {
        gpioMode = gpioModeInputPull;
        gpioOption = (gpioPullupEn == 0) ? 0 : 1;

    // Select input with bias resistor and input glitch filter. Setting
    // the option flag enables pullup operation.
    } else {
        gpioMode = gpioModeInputPullFilter;
        gpioOption = (gpioPullupEn == 0) ? 0 : 1;
    }

    // Set the input pin mode via the SDK.
    GPIO_PinModeSet (gpioPort, gpioPin, gpioMode, gpioOption);
    return true;
}

/*
 * Sets a general purpose IO pin as a conventional output, using the
 * configuration previously assigned by the 'gmosDriverGpioPinInit'
 * function.
 */
bool gmosDriverGpioSetAsOutput (uint16_t gpioPinId)
{
    GPIO_Port_TypeDef gpioPort;
    uint_fast16_t gpioPin;
    uint_fast16_t gpioPinMask;
    uint_fast16_t gpioOpenDrainEn;
    uint_fast16_t gpioFilterEn;
    uint_fast16_t gpioPullupEn;
    GPIO_Mode_TypeDef gpioMode;
    uint32_t gpioOutput;

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
        gpioMode = gpioModePushPull;
        gpioOutput = 0;

    // Select conventional open drain. The output is not driven.
    } else if ((gpioFilterEn == 0) && (gpioPullupEn == 0)) {
        gpioMode = gpioModeWiredAnd;
        gpioOutput = 1;

    // Select open drain with pullup.
    } else if ((gpioFilterEn == 0) && (gpioPullupEn != 0)) {
        gpioMode = gpioModeWiredAndPullUp;
        gpioOutput = 1;

    // Select open drain with input filter.
    } else if ((gpioFilterEn != 0) && (gpioPullupEn == 0)) {
        gpioMode = gpioModeWiredAndFilter;
        gpioOutput = 1;

    // Select open drain with both pullup and input filter.
    } else {
        gpioMode = gpioModeWiredAndPullUpFilter;
        gpioOutput = 1;
    }

    // Set the output pin mode via the SDK.
    GPIO_PinModeSet (gpioPort, gpioPin, gpioMode, gpioOutput);
    return true;
}

/*
 * Sets the GPIO pin state. If the GPIO is configured as an output this
 * will update the output value.
 */
void gmosDriverGpioSetPinState (uint16_t gpioPinId, bool pinState)
{
    GPIO_Port_TypeDef gpioPort;
    uint_fast16_t gpioPin;
    uint_fast16_t gpioPinMask;
    uint_fast16_t gpioOutputEn;

    // Extract the GPIO port and pin ID.
    gpioPort = (gpioPinId >> 8) & 0x03;
    gpioPin = gpioPinId & 0x0F;
    gpioPinMask = (1 << gpioPin);

    // Extract the GPIO status flags.
    gpioOutputEn = gmosDriverGpioOutputEnFlags [gpioPort] & gpioPinMask;

    // Only set the pin state if the pin output is enabled.
    if (gpioOutputEn != 0) {
        if (pinState) {
            GPIO_PinOutSet (gpioPort, gpioPin);
        } else {
            GPIO_PinOutClear (gpioPort, gpioPin);
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
    GPIO_Port_TypeDef gpioPort;
    uint_fast16_t gpioPin;
    uint32_t pinState;

    // Extract the GPIO port and pin ID.
    gpioPort = (gpioPinId >> 8) & 0x03;
    gpioPin = gpioPinId & 0x0F;

    // Read the state of the input buffer.
    pinState = GPIO_PinInGet (gpioPort, gpioPin);
    return (pinState == 0) ? false : true;
}

/*
 * Initialises a general purpose IO pin for interrupt generation. This
 * should be called for each interrupt input GPIO pin prior to accessing
 * it via any of the other API functions. The interrupt is not enabled
 * at this stage.
 */
bool gmosDriverGpioInterruptInit (uint16_t gpioPinId,
    gmosDriverGpioIsr_t gpioIsr, void* gpioIsrData,
    int8_t biasResistor)
{
    GPIO_Port_TypeDef gpioPort;
    uint_fast16_t gpioPin;
    uint_fast8_t intIdBase;
    uint_fast8_t intIdTop;
    uint_fast8_t intId;
    uint_fast8_t slot;

    // Select an interrupt ID for the specified GPIO. This is done in
    // blocks of four, as described in the datasheet.
    gpioPort = (gpioPinId >> 8) & 0x03;
    gpioPin = gpioPinId & 0x0F;
    if (gpioPin < 4) {
        intIdBase = 0;
        intIdTop = 3;
    } else if (gpioPin < 8) {
        intIdBase = 4;
        intIdTop = 7;
    } else if (gpioPin < 10) {
        intIdBase = 8;
        intIdTop = 11;
    } else {
        return false;
    }
    for (intId = intIdBase; intId <= intIdTop; intId++) {
        if (gmosDriverGpioIsrMap [intId] == 0xFF) {
            break;
        }
    }
    if (intId > intIdTop) {
        return false;
    }

    // Select the next available ISR callback slot.
    for (slot = 0; slot < GMOS_CONFIG_EFR32_GPIO_MAX_ISRS; slot++) {
        if (gpioIsrHandlers [slot] == NULL) {
            break;
        }
    }
    if (slot >= GMOS_CONFIG_EFR32_GPIO_MAX_ISRS) {
        return false;
    }

    // Configure the GPIO pin and set it as an input.
    if (!gmosDriverGpioPinInit (gpioPinId, false, 0, biasResistor)) {
        return false;
    }

    // Set up the interrupt routing registers with interrupts disabled.
    GPIO_ExtIntConfig (gpioPort, gpioPin, intId, false, false, false);

    // Populate the local ISR slot.
    gmosDriverGpioIsrMap [intId] = slot;
    gpioIsrHandlers [slot] = gpioIsr;
    gpioIsrPinIds [slot] = gpioPinId;
    gpioIsrIntIds [slot] = intId;
    gpioIsrDataItems [slot] = gpioIsrData;
    return true;
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

    // Set the rising and falling edge options.
    BUS_RegBitWrite (&(GPIO->EXTIRISE), intId, risingEdge);
    BUS_RegBitWrite (&(GPIO->EXTIFALL), intId, fallingEdge);

    // Clear any pending interrupts before setting interrupt enable.
    GPIO_IntClear (1 << intId);
    GPIO_IntEnable (1 << intId);
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

    // Disable the interrupt.
    GPIO_IntDisable (1 << intId);
}

/*
 * Initialises the GPIO platform abstraction layer on startup.
 */
void gmosPalGpioInit (void)
{
    uint32_t i;

    // Always enable the GPIO module clock prior to register access.
    CMU_ClockEnable (cmuClock_GPIO, true);

    // Clear the local configuration registers and set the bank slew
    // rates to their minimum setting.
    for (i = 0; i < 4; i++) {
        gmosDriverGpioOpenDrainFlags [i] = 0;
        gmosDriverGpioFilterFlags [i] = 0;
        gmosDriverGpioPullupFlags [i] = 0;
        gmosDriverGpioPulldownFlags [i] = 0;
        gmosDriverGpioOutputEnFlags [i] = 0;
        GPIO_SlewrateSet (i, 0, 0);
    }

    // Clear the interrupt index fields and ISR callback slots.
    for (i = 0; i < 12; i++) {
        gmosDriverGpioIsrMap [i] = 0xFF;
    }
    for (i = 0; i < GMOS_CONFIG_EFR32_GPIO_MAX_ISRS; i++) {
        gpioIsrHandlers [i] = NULL;
    }

    // Disable all low level GPIO interrupts.
    GPIO_IntDisable (0xFFF);

    // Enable top level GPIO interrupts in the NVIC.
    if (sl_interrupt_manager_is_irq_disabled (GPIO_ODD_IRQn)) {
        NVIC_ClearPendingIRQ (GPIO_ODD_IRQn);
        NVIC_EnableIRQ (GPIO_ODD_IRQn);
    }
    if (sl_interrupt_manager_is_irq_disabled (GPIO_EVEN_IRQn)) {
        NVIC_ClearPendingIRQ (GPIO_EVEN_IRQn);
        NVIC_EnableIRQ (GPIO_EVEN_IRQn);
    }
}

/*
 * Dispatch the selected GPIO interrupt to the approriate interrupt
 * service routine.
 */
static void gmosPalGpioIsrDispatch (uint8_t intId)
{
    uint_fast8_t slot = gmosDriverGpioIsrMap [intId];

    // Clear the interrupt flag before processing.
    GPIO_IntClear (1L << intId);

    // Dispatch to the interrupt handler.
    if (slot < GMOS_CONFIG_EFR32_GPIO_MAX_ISRS) {
        gmosDriverGpioIsr_t gpioIsrHandler = gpioIsrHandlers [slot];
        if (gpioIsrHandler != NULL) {
            gpioIsrHandler (gpioIsrDataItems [slot]);
        }
    }
}

/*
 * Provide interrupt handler for GPIO interrupts in even bit positions.
 */
void GPIO_EVEN_IRQHandler(void)
{
    uint32_t intFlags;
    uint_fast8_t intId;

    // Get all even interrupts on entry.
    intFlags = GPIO_IntGetEnabled ();
    while ((intFlags & 0x555) != 0) {

        // Select the next interrupt to process.
        for (intId = 0; intId < 12; intId += 2) {
            if ((intFlags & 1) != 0) {
                break;
            } else {
                intFlags >>= 2;
            }
        }
        gmosPalGpioIsrDispatch (intId);

        // Check for more interrupts to process.
        intFlags = GPIO_IntGetEnabled ();
    }
}

/*
 * Provide interrupt handler for GPIO interrupts in odd bit positions.
 */
void GPIO_ODD_IRQHandler(void)
{
    uint32_t intFlags;
    uint_fast8_t intId;

    // Get all odd interrupts on entry.
    intFlags = GPIO_IntGetEnabled ();
    while ((intFlags & 0xAAA) != 0) {

        // Select the next interrupt to process.
        for (intId = 1; intId < 12; intId += 2) {
            if ((intFlags & 2) != 0) {
                break;
            } else {
                intFlags >>= 2;
            }
        }
        gmosPalGpioIsrDispatch (intId);

        // Check for more interrupts to process.
        intFlags = GPIO_IntGetEnabled ();
    }
}
