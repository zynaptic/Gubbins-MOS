/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2020-2022 Zynaptic Limited
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
 * Implements GPIO driver functionality for the STM32L1XX series of
 * microcontrollers.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-driver-gpio.h"
#include "stm32-driver-gpio.h"
#include "stm32-device.h"

// GPIO bank F is not supported on all devices.
#ifndef GPIOF
#define GPIOF NULL
#define RCC_AHBENR_GPIOFEN 0
#endif

// GPIO bank G is not supported on all devices.
#ifndef GPIOG
#define GPIOG NULL
#define RCC_AHBENR_GPIOGEN 0
#endif

// Provide mapping of pin bank values to GPIO register sets.
static GPIO_TypeDef* gpioRegisterMap [] = {
    GPIOA, GPIOB, GPIOC, GPIOD, GPIOE, GPIOF, GPIOG, GPIOH };

// Provide mapping of pin bank values to AHB clock enable masks.
static uint8_t gpioClockEnableMap [] = {
    RCC_AHBENR_GPIOAEN, RCC_AHBENR_GPIOBEN, RCC_AHBENR_GPIOCEN,
    RCC_AHBENR_GPIODEN, RCC_AHBENR_GPIOEEN, RCC_AHBENR_GPIOFEN,
    RCC_AHBENR_GPIOGEN, RCC_AHBENR_GPIOHEN };

// Provide mapping of pin bank values to external interrupt selection
// values.
static uint8_t gpioExtiSourceMap [] = {
    SYSCFG_EXTICR1_EXTI0_PA, SYSCFG_EXTICR1_EXTI0_PB,
    SYSCFG_EXTICR1_EXTI0_PC, SYSCFG_EXTICR1_EXTI0_PD,
    SYSCFG_EXTICR1_EXTI0_PE, SYSCFG_EXTICR1_EXTI0_PF,
    SYSCFG_EXTICR1_EXTI0_PG, SYSCFG_EXTICR1_EXTI0_PH };

// Provide mapping of external interrupt lines to interrupt service
// routines.
static gmosDriverGpioIsr_t gpioIsrMap [] = {
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };

// Provide mapping of external interrupt lines to interrupt service
// routine data items.
static void* gpioIsrDataMap [] = {
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };

/*
 * Initialises a general purpose IO pin for conventional use. For the
 * STM32L1XX series of devices, the upper byte of the GPIO pin ID is
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
    uint32_t clockEnable;
    GPIO_TypeDef* gpio = gpioRegisterMap [pinBank];

    // Check for valid GPIO register set.
    if (gpio == NULL) {
        return false;
    }

    // Enable clocks for the selected GPIO bank.
    clockEnable = (uint32_t) gpioClockEnableMap [pinBank];
    RCC->AHBENR |= clockEnable;

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
    uint32_t clockEnable;
    uint32_t gpioClockEnables;
    GPIO_TypeDef* gpio = gpioRegisterMap [pinBank];

    // Check for valid GPIO register set.
    if (gpio == NULL) {
        return false;
    }

    // Enable clocks for the selected GPIO bank.
    gpioClockEnables = RCC->AHBENR;
    clockEnable = (uint32_t) gpioClockEnableMap [pinBank];
    RCC->AHBENR |= clockEnable;

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
    RCC->AHBENR = gpioClockEnables;

    return true;
}

/*
 * Sets the GPIO pin direction.
 */
static bool gmosDriverGpioSetDirection (uint16_t gpioPinId, bool isOutput)
{
    uint8_t pinBank = (gpioPinId >> 8) &0x07;
    uint8_t pinIndex = gpioPinId & 0x0F;
    uint32_t clockEnable;
    uint32_t regValue;
    GPIO_TypeDef* gpio = gpioRegisterMap [pinBank];

    // Check for valid GPIO register set.
    if (gpio == NULL) {
        return false;
    }

    // Check that the GPIO bank clock has been enabled, otherwise the
    // GPIO port access will hang.
    clockEnable = (uint32_t) gpioClockEnableMap [pinBank];
    if ((RCC->AHBENR & clockEnable) == 0) {
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
            gpio->BSRR = 1 << (16 + pinIndex);
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

/*
 * Initialises a general purpose IO pin for interrupt generation. The
 * interrupt is not enabled at this stage.
 */
bool gmosDriverGpioInterruptInit (uint16_t gpioPinId,
    gmosDriverGpioIsr_t gpioIsr, void* gpioIsrData, int8_t biasResistor)
{
    uint8_t pinBank = (gpioPinId >> 8) &0x07;
    uint8_t pinIndex = gpioPinId & 0x0F;
    uint32_t regValue;
    uint32_t apb2ClockEnables;
    uint32_t extiSource = (uint32_t) gpioExtiSourceMap [pinBank];

    // Insert the pin specific ISR into the table. Only accept the
    // initialisation request if the ISR slot is free.
    if (gpioIsrMap [pinIndex] != NULL) {
        return false;
    }
    gpioIsrMap [pinIndex] = gpioIsr;
    gpioIsrDataMap [pinIndex] = gpioIsrData;

    // Configure the GPIO pin as an input.
    if (!gmosDriverGpioPinInit (gpioPinId, GMOS_DRIVER_GPIO_OUTPUT_PUSH_PULL,
        STM32_GPIO_DRIVER_SLEW_SLOW, biasResistor)) {
        return false;
    }

    // Enable clock to the system configuration block.
    apb2ClockEnables = RCC->APB2ENR;
    RCC->APB2ENR = apb2ClockEnables | RCC_APB2ENR_SYSCFGEN;

    // Select the external interrupt mapping.
    regValue = SYSCFG->EXTICR [pinIndex / 4];
    regValue &= ~(0x0F << (4 * (pinIndex & 3)));
    regValue |= (extiSource << (4 * (pinIndex & 3)));
    SYSCFG->EXTICR [pinIndex / 4] = regValue;

    // Disable clock to the system configuration block.
    RCC->APB2ENR = apb2ClockEnables;

    // Enable the appropriate NVIC interrupt.
    if (pinIndex == 0) {
        NVIC_EnableIRQ (EXTI0_IRQn);
    } else if (pinIndex == 1) {
        NVIC_EnableIRQ (EXTI1_IRQn);
    } else if (pinIndex == 2) {
        NVIC_EnableIRQ (EXTI2_IRQn);
    } else if (pinIndex == 3) {
        NVIC_EnableIRQ (EXTI3_IRQn);
    } else if (pinIndex == 4) {
        NVIC_EnableIRQ (EXTI4_IRQn);
    } else if (pinIndex <= 9) {
        NVIC_EnableIRQ (EXTI9_5_IRQn);
    } else {
        NVIC_EnableIRQ (EXTI15_10_IRQn);
    }
    return true;
}

/*
 * Enables a GPIO interrupt for rising and/or falling edge detection.
 */
void gmosDriverGpioInterruptEnable (uint16_t gpioPinId,
    bool risingEdge, bool fallingEdge)
{
    uint8_t pinIndex = gpioPinId & 0x0F;

    if (risingEdge) {
        EXTI->RTSR |= (1 << pinIndex);
    } else {
        EXTI->RTSR &= ~(1 << pinIndex);
    }
    if (fallingEdge) {
        EXTI->FTSR |= (1 << pinIndex);
    } else {
        EXTI->FTSR &= ~(1 << pinIndex);
    }
    EXTI->IMR |= (1 << pinIndex);
}

/*
 * Disables a GPIO interrupt for the specified GPIO pin.
 */
void gmosDriverGpioInterruptDisable (uint16_t gpioPinId)
{
    uint8_t pinIndex = gpioPinId & 0x0F;

    EXTI->IMR &= ~(1 << pinIndex);
}

/*
 * Implements common GPIO ISR processing for GPIO lines in the specified
 * index range.
 */
static void gmosDriverGpioCommonIsr (uint8_t indexStart, uint8_t indexEnd)
{
    uint32_t pendingFlags;
    uint32_t activeFlag;
    uint8_t i;
    gmosDriverGpioIsr_t pendingIsr;
    void* pendingIsrData;

    // Loop over the requested ISRs, handling any that are ready to run.
    pendingFlags = EXTI->PR;
    for (i = indexStart; i <= indexEnd; i++) {
        activeFlag = (1 << i);
        if ((pendingFlags & activeFlag) != 0) {
            pendingIsr = gpioIsrMap [i];
            pendingIsrData = gpioIsrDataMap [i];
            if (pendingIsr != NULL) {
                pendingIsr (pendingIsrData);
            }
            EXTI->PR = activeFlag;
        }
    }
}

/*
 * Implements the NVIC interrupt service routine for external interrupts
 * on GPIO line 0.
 */
void gmosPalIsrEXTIA (void)
{
    gmosDriverGpioCommonIsr (0, 0);
}

/*
 * Implements the NVIC interrupt service routine for external interrupts
 * on GPIO line 1.
 */
void gmosPalIsrEXTIB (void)
{
    gmosDriverGpioCommonIsr (1, 1);
}

/*
 * Implements the NVIC interrupt service routine for external interrupts
 * on GPIO line 2.
 */
void gmosPalIsrEXTIC (void)
{
    gmosDriverGpioCommonIsr (2, 2);
}

/*
 * Implements the NVIC interrupt service routine for external interrupts
 * on GPIO line 3.
 */
void gmosPalIsrEXTID (void)
{
    gmosDriverGpioCommonIsr (3, 3);
}

/*
 * Implements the NVIC interrupt service routine for external interrupts
 * on GPIO line 4.
 */
void gmosPalIsrEXTIE (void)
{
    gmosDriverGpioCommonIsr (4, 4);
}

/*
 * Implements the NVIC interrupt service routine for external interrupts
 * on GPIO lines 5 to 9.
 */
void gmosPalIsrEXTIF (void)
{
    gmosDriverGpioCommonIsr (5, 9);
}

/*
 * Implements the NVIC interrupt service routine for external interrupts
 * on GPIO lines 10 to 15.
 */
void gmosPalIsrEXTIG (void)
{
    gmosDriverGpioCommonIsr (10, 15);
}
