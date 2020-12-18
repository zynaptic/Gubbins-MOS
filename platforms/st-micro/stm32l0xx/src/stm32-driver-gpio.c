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
 * Implements GPIO driver functionality for the STM32L0XX series of
 * microcontrollers.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-driver-gpio.h"
#include "stm32-device.h"

// Provide mapping of pin bank values to GPIO register sets.
static GPIO_TypeDef* gpioRegisterMap [] = {
    GPIOA, GPIOB, GPIOC, GPIOD, GPIOE, NULL, NULL, GPIOH};

/*
 * Initialises a general purpose IO pin for conventional use. For the
 * STM32L0XX series of devices, the upper byte of the GPIO pin ID is
 * used to select the GPIO bank and the lower byte is used to select
 * the pin number.
 */
bool gmosDriverGpioPinInit (uint16_t gpioPinId, bool openDrain,
    uint8_t driveStrength, int8_t biasResistor)
{
    uint8_t pinBank = (gpioPinId >> 8) &0x07;
    uint8_t pinIndex = gpioPinId & 0x0F;
    uint32_t configBits;
    uint32_t regValue;
    GPIO_TypeDef* gpio = gpioRegisterMap [pinBank];

    // Check for valid GPIO register set.
    if (gpio == NULL) {
        return false;
    }

    // Enable clocks for the selected GPIO bank.
    RCC->IOPENR |= (1 << pinBank);

    // Ensure the GPIO pin defaults to an input.
    gpio->MODER &= ~(3 << (2 * pinIndex));

    // Select open drain output if required.
    regValue = gpio->OTYPER;
    if (openDrain) {
        regValue |= (1 << pinIndex);
    } else {
        regValue &= ~(1 << pinIndex);
    }
    gpio->OTYPER = regValue;

    // Select the output drive strength.
    regValue = gpio->OSPEEDR;
    regValue &= ~(3 << (2 * pinIndex));
    configBits = (driveStrength > 3) ? 3 : driveStrength;
    regValue |= (configBits << (2 * pinIndex));
    gpio->OSPEEDR = regValue;

    // Select the bias resistor configuration.
    regValue = gpio->PUPDR;
    regValue &= ~(3 << (2 * pinIndex));
    configBits = (biasResistor >= 1) ? 1 : (biasResistor <= -1) ? 2 : 0;
    regValue |= (configBits << (2 * pinIndex));
    gpio->PUPDR = regValue;

    return true;
}

/*
 * Sets up one of the STM32 GPIO pins for alternate function use.
 */
bool gmosDriverGpioAltModeInit (uint16_t gpioPinId, bool openDrain,
    uint8_t driveStrength, int8_t biasResistor, uint8_t altFunction)
{
    uint8_t pinBank = (gpioPinId >> 8) & 0x07;
    uint8_t pinIndex = gpioPinId & 0x0F;
    uint32_t configBits;
    uint32_t regValue;
    uint32_t gpioClockEnables;
    GPIO_TypeDef* gpio = gpioRegisterMap [pinBank];

    // Check for valid GPIO register set.
    if (gpio == NULL) {
        return false;
    }

    // Enable clocks for the selected GPIO bank.
    gpioClockEnables = RCC->IOPENR;
    RCC->IOPENR |= (1 << pinBank);

    // Configure the GPIO pin for alternate function use.
    regValue = gpio->MODER;
    regValue &= ~(3 << (2 * pinIndex));
    regValue |= (2 << (2 * pinIndex));
    gpio->MODER = regValue;

    // Select open drain output if required.
    regValue = gpio->OTYPER;
    if (openDrain) {
        regValue |= (1 << pinIndex);
    } else {
        regValue &= ~(1 << pinIndex);
    }
    gpio->OTYPER = regValue;

    // Select the output drive strength.
    regValue = gpio->OSPEEDR;
    regValue &= ~(3 << (2 * pinIndex));
    configBits = (driveStrength > 3) ? 3 : driveStrength;
    regValue |= (configBits << (2 * pinIndex));
    gpio->OSPEEDR = regValue;

    // Select the bias resistor configuration.
    regValue = gpio->PUPDR;
    regValue &= ~(3 << (2 * pinIndex));
    configBits = (biasResistor >= 1) ? 1 : (biasResistor <= -1) ? 2 : 0;
    regValue |= (configBits << (2 * pinIndex));
    gpio->PUPDR = regValue;

    // Set the alternate function to use.
    if (pinIndex < 8) {
        regValue = gpio->AFR[0];
        regValue &= ~(15 << (4 * pinIndex));
        regValue |= ((altFunction & 15) << (4 * pinIndex));
        gpio->AFR[0] = regValue;
    } else {
        regValue = gpio->AFR[1];
        regValue &= ~(15 << (4 * (pinIndex - 8)));
        regValue |= ((altFunction & 15) << (4 * (pinIndex - 8)));
        gpio->AFR[1] = regValue;
    }

    // Revert the GPIO clocks back to their previous setting.
    RCC->IOPENR = gpioClockEnables;

    return true;
}

/*
 * Sets the GPIO pin direction.
 */
static bool gmosDriverGpioSetDirection (uint16_t gpioPinId, bool isOutput)
{
    uint8_t pinBank = (gpioPinId >> 8) &0x07;
    uint8_t pinIndex = gpioPinId & 0x0F;
    uint32_t regValue;
    GPIO_TypeDef* gpio = gpioRegisterMap [pinBank];

    // Check for valid GPIO register set.
    if (gpio == NULL) {
        return false;
    }

    // Check that the GPIO bank clock has been enabled, otherwise the
    // GPIO port access will hang.
    if ((RCC->IOPENR & (1 << pinBank)) == 0) {
        return false;
    }

    // Check that the GPIO pin is not in use for an alternate function
    // or ADC input.
    regValue = gpio->MODER;
    if ((regValue & (2 << (2 * pinIndex))) != 0) {
        return false;
    }

    // Set the GPIO pin direction.
    regValue &= ~(3 << (2 * pinIndex));
    if (isOutput) {
        regValue |= (1 << (2 * pinIndex));
    }
    gpio->MODER = regValue;
    return true;
}

/*
 * Sets a general purpose IO pin as a conventional input, using the
 * configuration previously assigned by the 'gmosDriverGpioPinInit'
 * function.
 */
bool gmosDriverGpioSetAsInput (uint16_t gpioPinId)
{
    return gmosDriverGpioSetDirection (gpioPinId, false);
}

/*
 * Sets a general purpose IO pin as a conventional output, using the
 * configuration previously assigned by the 'gmosDriverGpioPinInit'
 * function.
 */
bool gmosDriverGpioSetAsOutput (uint16_t gpioPinId)
{
    return gmosDriverGpioSetDirection (gpioPinId, true);
}

/*
 * Sets the GPIO pin state. If the GPIO is configured as an output this
 * will update the output value.
 */
void gmosDriverGpioSetPinState (uint16_t gpioPinId, bool pinState)
{
    uint8_t pinBank = (gpioPinId >> 8) &0x07;
    uint8_t pinIndex = gpioPinId & 0x0F;
    GPIO_TypeDef* gpio = gpioRegisterMap [pinBank];

    // Set or clear the GPIO output register.
    if (gpio != NULL) {
        if (pinState == true) {
            gpio->BSRR = 1 << pinIndex;
        } else {
            gpio->BRR = 1 << pinIndex;
        }
    }
}

/*
 * Gets the GPIO pin state. If the GPIO is configured as an input this
 * will be the sampled value and if configured as an output this will
 * be the current output value.
 */
bool gmosDriverGpioGetPinState (uint16_t gpioPinId)
{
    uint8_t pinBank = (gpioPinId >> 8) &0x07;
    uint8_t pinIndex = gpioPinId & 0x0F;
    GPIO_TypeDef* gpio = gpioRegisterMap [pinBank];
    bool pinState = false;

    // Check for valid GPIO register set and pin state.
    if (gpio != NULL) {
        if ((gpio->IDR & (1 << pinIndex)) != 0) {
            pinState = true;
        }
    }
    return pinState;
}
