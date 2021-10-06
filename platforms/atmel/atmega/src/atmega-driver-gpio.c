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
 * Implements GPIO driver functionality for the Microchip/Atmel ATMEGA
 * series of microcontrollers.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-driver-gpio.h"
#include "atmega-driver-gpio.h"
#include "avr/io.h"

// Detect unused ports.
#ifdef PORTA
#define PORTA_ADDR &PORTA
#else
#define PORTA_ADDR NULL
#endif

#ifdef PORTB
#define PORTB_ADDR &PORTB
#else
#define PORTB_ADDR NULL
#endif

#ifdef PORTC
#define PORTC_ADDR &PORTC
#else
#define PORTC_ADDR NULL
#endif

#ifdef PORTD
#define PORTD_ADDR &PORTD
#else
#define PORTD_ADDR NULL
#endif

#ifdef PORTE
#define PORTE_ADDR &PORTE
#else
#define PORTE_ADDR NULL
#endif

#ifdef PORTF
#define PORTF_ADDR &PORTF
#else
#define PORTF_ADDR NULL
#endif

#ifdef PORTG
#define PORTG_ADDR &PORTG
#else
#define PORTG_ADDR NULL
#endif

#ifdef PORTH
#define PORTH_ADDR &PORTH
#else
#define PORTH_ADDR NULL
#endif

// Provide mapping of pin bank values to GPIO register sets.
static volatile uint8_t * gpioRegisterMap [8] = {
    PORTA_ADDR, PORTB_ADDR, PORTC_ADDR, PORTD_ADDR,
    PORTE_ADDR, PORTF_ADDR, PORTG_ADDR, PORTH_ADDR };

// Store pullup configuration options for tristate settings.
static uint8_t gpioPullupMap [8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

// Specify the dedicated pins used for external interrupt inputs.
static const uint16_t gpioExtiPinMap [] = ATMEGA_GPIO_EXTINT_PINS;

// Provide mapping of external interrupt lines to interrupt service
// routines.
static gmosDriverGpioIsr_t gpioIsrMap [ATMEGA_GPIO_EXTINT_NUM] = { NULL };

// Provide mapping of external interrupt lines to interrupt service
// routine data items.
static void* gpioIsrDataMap [ATMEGA_GPIO_EXTINT_NUM] = { NULL };

/*
 * Initialises a general purpose IO pin for conventional use. For the
 * ATMEGA series of devices, the upper byte of the GPIO pin ID is used
 * to select the GPIO bank and the lower byte is used to select the pin
 * number. Note that the open drain and drive strength options are not
 * applicable to ATMEGA devices, and the only bias resistor option is
 * the default pullup.
 */
bool gmosDriverGpioPinInit (uint16_t gpioPinId, bool openDrain,
    uint8_t driveStrength, int8_t biasResistor)
{
    volatile uint8_t* portReg;
    volatile uint8_t* ddrReg;
    uint8_t pinBank = (gpioPinId >> 8) &0x07;
    uint8_t pinIndex = gpioPinId & 0x07;
    uint8_t pinMask = (1 << pinIndex);

    // Select the I/O registers to use for configuration.
    portReg = gpioRegisterMap [pinBank];
    if (portReg == NULL) {
        return false;
    }
    ddrReg = portReg - 1;

    // On initialisation, set the pin as an input.
    if (biasResistor == GMOS_DRIVER_GPIO_INPUT_PULL_UP) {
        gpioPullupMap [pinBank] |= pinMask;
        *portReg |= pinMask;
    } else {
        gpioPullupMap [pinBank] &= ~pinMask;
        *portReg &= ~pinMask;
    }
    *ddrReg &= ~pinMask;
    return true;
}

/*
 * Sets a general purpose IO pin as a conventional input, using the
 * configuration previously assigned by the 'gmosDriverGpioPinInit'
 * function.
 */
bool gmosDriverGpioSetAsInput (uint16_t gpioPinId)
{
    volatile uint8_t* portReg;
    volatile uint8_t* ddrReg;
    uint8_t pinBank = (gpioPinId >> 8) &0x07;
    uint8_t pinIndex = gpioPinId & 0x07;
    uint8_t pinMask = (1 << pinIndex);
    uint8_t pinPullups = gpioPullupMap [pinBank];

    // Select the I/O registers to use for configuration.
    portReg = gpioRegisterMap [pinBank];
    if (portReg == NULL) {
        return false;
    }
    ddrReg = portReg - 1;

    // Assign the pullup and then set the pin as an input.
    if ((pinPullups & pinMask) != 0) {
        *portReg |= pinMask;
    } else {
        *portReg &= ~pinMask;
    }
    *ddrReg &= ~pinMask;
    return true;
}

/*
 * Sets a general purpose IO pin as a conventional output, using the
 * configuration previously assigned by the 'gmosDriverGpioPinInit'
 * function.
 */
bool gmosDriverGpioSetAsOutput (uint16_t gpioPinId)
{
    volatile uint8_t* portReg;
    volatile uint8_t* ddrReg;
    uint8_t pinBank = (gpioPinId >> 8) &0x07;
    uint8_t pinIndex = gpioPinId & 0x07;
    uint8_t pinMask = (1 << pinIndex);
    uint8_t pinPullups = gpioPullupMap [pinBank];

    // Select the I/O registers to use for configuration.
    portReg = gpioRegisterMap [pinBank];
    if (portReg == NULL) {
        return false;
    }
    ddrReg = portReg - 1;

    // Assign the initial output state and then set the pin as an
    // output. The initial output state is active high if a pullup
    // is configured and active low otherwise.
    if ((pinPullups & pinMask) != 0) {
        *portReg |= pinMask;
    } else {
        *portReg &= ~pinMask;
    }
    *ddrReg |= pinMask;
    return true;
}

/*
 * Sets the GPIO pin state. If the GPIO is configured as an output this
 * will update the output value.
 */
void gmosDriverGpioSetPinState (uint16_t gpioPinId, bool pinState)
{
    volatile uint8_t* portReg;
    volatile uint8_t* ddrReg;
    uint8_t pinBank = (gpioPinId >> 8) &0x07;
    uint8_t pinIndex = gpioPinId & 0x07;
    uint8_t pinMask = (1 << pinIndex);

    // Select the I/O registers to use for configuration.
    portReg = gpioRegisterMap [pinBank];
    if (portReg == NULL) {
        return;
    }
    ddrReg = portReg - 1;

    // Update the pin state only if it is configured as an output.
    if ((*ddrReg & pinMask) != 0) {
        if (pinState) {
            *portReg |= pinMask;
        } else {
            *portReg &= ~pinMask;
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
    volatile uint8_t* portReg;
    volatile uint8_t* ddrReg;
    volatile uint8_t* pinReg;
    uint8_t pinBank = (gpioPinId >> 8) &0x07;
    uint8_t pinIndex = gpioPinId & 0x07;
    uint8_t pinMask = (1 << pinIndex);
    bool pinState;

    // Select the I/O registers to use for configuration.
    portReg = gpioRegisterMap [pinBank];
    if (portReg == NULL) {
        return false;
    }
    ddrReg = portReg - 1;
    pinReg = portReg - 2;

    // Take the pin state from the input or output register.
    if ((*ddrReg & pinMask) == 0) {
        pinState = ((*pinReg & pinMask) != 0) ? true : false;
    } else {
        pinState = ((*portReg & pinMask) != 0) ? true : false;
    }
    return pinState;
}

/*
 * Initialises a general purpose IO pin for interrupt generation. The
 * interrupt is not enabled at this stage. This implementation only
 * currently supports dedicated external interrupt pins, and not the
 * more general pin change interrupts.
 */
bool gmosDriverGpioInterruptInit (uint16_t gpioPinId,
    gmosDriverGpioIsr_t gpioIsr, void* gpioIsrData,
    int8_t biasResistor)
{
    uint8_t isrIndex;

    // Check that the specified pin is an external interrupt pin.
    for (isrIndex = 0; isrIndex < ATMEGA_GPIO_EXTINT_NUM; isrIndex ++) {
        if (gpioPinId == gpioExtiPinMap [isrIndex]) break;
    }
    if (isrIndex == ATMEGA_GPIO_EXTINT_NUM) {
        return false;
    }

    // Configure the specified pin as an input.
    if (!gmosDriverGpioPinInit (gpioPinId, GMOS_DRIVER_GPIO_OUTPUT_PUSH_PULL,
        ATMEGA_GPIO_DRIVER_SLEW_FIXED, biasResistor)) {
        return false;
    }

    // Register the ISR callback function and data item.
    gpioIsrMap [isrIndex] = gpioIsr;
    gpioIsrDataMap [isrIndex] = gpioIsrData;
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
    uint8_t isrIndex;
    uint8_t regValue;

    // Check that the specified pin is an external interrupt pin.
    for (isrIndex = 0; isrIndex < ATMEGA_GPIO_EXTINT_NUM; isrIndex ++) {
        if (gpioPinId == gpioExtiPinMap [isrIndex]) break;
    }

    // Configure the external interrupt 0 registers directly.
#ifdef INT0_vect
    if (isrIndex == 0) {
        regValue = ATMEGA_GPIO_EXTINT_CFG_REG;
        regValue &= ~((1 << ISC00) | ( 1 << ISC01));
        if (!GMOS_CONFIG_ATMEGA_EXTINT0_ACTIVE_LOW) {
            if (risingEdge && fallingEdge) {
                regValue |= (1 << ISC00);
            } else if (fallingEdge) {
                regValue |= (1 << ISC01);
            } else {
                regValue |= (1 << ISC00) | (1 << ISC01);
            }
        }
        ATMEGA_GPIO_EXTINT_CFG_REG = regValue;
        ATMEGA_GPIO_EXTINT_MSK_REG |= (1 << INT0);
    }
#endif

    // Configure the external interrupt 1 registers directly.
#ifdef INT1_vect
    if (isrIndex == 1) {
        regValue = ATMEGA_GPIO_EXTINT_CFG_REG;
        regValue &= ~((1 << ISC10) | ( 1 << ISC11));
        if (!GMOS_CONFIG_ATMEGA_EXTINT1_ACTIVE_LOW) {
            if (risingEdge && fallingEdge) {
                regValue |= (1 << ISC10);
            } else if (fallingEdge) {
                regValue |= (1 << ISC11);
            } else {
                regValue |= (1 << ISC10) | (1 << ISC11);
            }
        }
        ATMEGA_GPIO_EXTINT_CFG_REG = regValue;
        ATMEGA_GPIO_EXTINT_MSK_REG |= (1 << INT1);
    }
#endif

    // Configure the external interrupt 2 registers directly. These only
    // support rising or falling edge triggers. If both are specified,
    // rising edge will be used.
#ifdef INT2_vect
    if (isrIndex == 2) {
        regValue = MCUCSR;
        if (risingEdge) {
            regValue |= (1 << ISC2);
        } else if (fallingEdge) {
            regValue &= ~(1 << ISC2);
        }
        MCUCSR = regValue;
        ATMEGA_GPIO_EXTINT_MSK_REG |= (1 << INT2);
    }
#endif
}

/*
 * Disables a GPIO interrupt for the specified GPIO pin. This should be
 * called after enabling a general purpose IO pin as an interrupt source
 * in order to stop receiving interrupt notifications.
 */
void gmosDriverGpioInterruptDisable (uint16_t gpioPinId)
{
    uint8_t isrIndex;

    // Check that the specified pin is an external interrupt pin.
    for (isrIndex = 0; isrIndex < ATMEGA_GPIO_EXTINT_NUM; isrIndex ++) {
        if (gpioPinId == gpioExtiPinMap [isrIndex]) break;
    }

    // Disable the external interrupt 0 directly.
#ifdef INT0_vect
    if (isrIndex == 0) {
        ATMEGA_GPIO_EXTINT_MSK_REG &= ~(1 << INT0);
    }
#endif

    // Disable the external interrupt 1 directly.
#ifdef INT1_vect
    if (isrIndex == 1) {
        ATMEGA_GPIO_EXTINT_MSK_REG &= ~(1 << INT1);
    }
#endif

    // Disable the external interrupt 2 directly.
#ifdef INT2_vect
    if (isrIndex == 2) {
        ATMEGA_GPIO_EXTINT_MSK_REG |= (1 << INT2);
    }
#endif
}

/*
 * Forward external interrupt 0 interrupt request.
 */
#ifdef INT0_vect
ISR (INT0_vect)
{
    if (gpioIsrMap [0] != NULL) {
        gpioIsrMap [0] (gpioIsrDataMap [0]);
    }
}
#endif

/*
 * Forward external interrupt 1 interrupt request.
 */
#ifdef INT1_vect
ISR (INT1_vect)
{
    if (gpioIsrMap [1] != NULL) {
        gpioIsrMap [1] (gpioIsrDataMap [1]);
    }
}
#endif

/*
 * Forward external interrupt 2 interrupt request.
 */
#ifdef INT2_vect
ISR (INT2_vect)
{
    if (gpioIsrMap [2] != NULL) {
        gpioIsrMap [2] (gpioIsrDataMap [2]);
    }
}
#endif
