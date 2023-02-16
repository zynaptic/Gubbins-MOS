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
 * Implements GPIO driver functionality for the Silicon Labs EFR32xG2x
 * series of microcontrollers.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-driver-gpio.h"
#include "efr32-driver-gpio.h"
#include "em_cmu.h"
#include "em_gpio.h"

// This is the set of GPIO open drain configuration flags.
static uint16_t gmosDriverGpioOpenDrainFlags [4];

// This is the set of GPIO filter mode configuration flags.
static uint16_t gmosDriverGpioFilterFlags [4];

// This is the set of GPIO pullup and pulldown configuration flags.
static uint16_t gmosDriverGpioPullupFlags [4];
static uint16_t gmosDriverGpioPulldownFlags [4];

// This is the set of GPIO output enable flags.
static uint16_t gmosDriverGpioOutputEnFlags [4];

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
        GPIO->P[gpioPort].CTRL =
            (gpioCtrlReg & ~_GPIO_P_CTRL_SLEWRATE_MASK) |
            (driveStrength << _GPIO_P_CTRL_SLEWRATE_SHIFT);
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
    uint16_t gpioPin;
    uint16_t gpioPinMask;

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
    uint16_t gpioPin;
    uint16_t gpioPinMask;
    uint16_t gpioFilterEn;
    uint16_t gpioPullupEn;
    uint16_t gpioPulldownEn;
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
    uint16_t gpioPin;
    uint16_t gpioPinMask;
    uint16_t gpioOpenDrainEn;
    uint16_t gpioFilterEn;
    uint16_t gpioPullupEn;
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
    uint16_t gpioPin;
    uint16_t gpioPinMask;
    uint16_t gpioOutputEn;

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
    uint16_t gpioPin;
    uint32_t pinState;

    // Extract the GPIO port and pin ID.
    gpioPort = (gpioPinId >> 8) & 0x03;
    gpioPin = gpioPinId & 0x0F;

    // Read the state of the input buffer.
    pinState = GPIO_PinInGet (gpioPort, gpioPin);
    return (pinState == 0) ? false : true;
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
}
