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
 * Implements GPIO driver functionality for the Raspberry Pi Pico RP2040
 * series of microcontrollers.
 */

#include <stdint.h>
#include <stdbool.h>

#include "gmos-config.h"
#include "gmos-driver-gpio.h"
#include "pico-driver-gpio.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"

// If concurrent GPIO access from both processor cores is supported, all
// accesses to the GPIO routines need to be protected by a mutex. Since
// all these functions are fast, the main platform mutex can be used.
#if GMOS_CONFIG_PICO_GPIO_MULTICORE_ACCESS
#define GPIO_MUTEX_CLAIM()   gmosPalMutexLock()
#define GPIO_MUTEX_RELEASE() gmosPalMutexUnlock()
#else
#define GPIO_MUTEX_CLAIM()
#define GPIO_MUTEX_RELEASE()
#endif

// This is a GPIO status bitmap. Bits 0 to 29 are used to indicate which
// GPIO pins are configured as open drain drivers. Bit 30 is used to
// indicate that the GPIO ISR has been registered for core 0 and bit 31
// is used to indicate that the GPIO ISR has been registered for core 1.
static uint32_t gpioStatusFlags;

// Allocate the specified number of ISR slots, using an array of GPIO
// pin IDs and an array of associated callback functions.
static gmosDriverGpioIsr_t gpioIsrHandlers [GMOS_CONFIG_PICO_GPIO_MAX_ISRS];
static uint8_t gpioIsrPinIds [GMOS_CONFIG_PICO_GPIO_MAX_ISRS];
static void* gpioIsrDataItems [GMOS_CONFIG_PICO_GPIO_MAX_ISRS];

/*
 * Initialises a general purpose IO pin for conventional use. For the
 * Raspberry Pi Pico RP2040 series of devices, the upper nibble of the
 * GPIO pin ID is used to select the GPIO bank and the remaining bits
 * are used to select the pin number.
 */
bool gmosDriverGpioPinInit (uint16_t gpioPinId, bool openDrain,
    uint8_t driveStrength, int8_t biasResistor)
{
    uint8_t pinBank = (gpioPinId >> 12) & 0x01;
    uint8_t pinIndex = gpioPinId & 0x1F;
    uint8_t pinSlew = (driveStrength >> 4) & 0x01;
    uint8_t pinDrive = driveStrength & 0x03;
    bool pullUp = (biasResistor > 0);
    bool pullDown = (biasResistor < 0);
    bool pinAvailable = false;

    // Reuse of the program interface pins is not currently supported.
    if ((pinBank != PICO_GPIO_BANK_U) || (pinIndex > 29)) {
        return false;
    }

    // Check for pin conflicts before configuring the pin.
    GPIO_MUTEX_CLAIM ();
    if (gpio_get_function (pinIndex) == GPIO_FUNC_NULL) {
        pinAvailable = true;

        // Initialise the pin as software controlled GPIO. This also
        // ensures that the GPIO pin defaults to an input.
        gpio_init (pinIndex);

        // Set the open drain flag to control the GPIO output behaviour.
        if (openDrain) {
            gpioStatusFlags |= (1 << pinIndex);
        } else {
            gpioStatusFlags &= ~(1 << pinIndex);
        }

        // Set the drive strength and slew rate.
        gpio_set_drive_strength (pinIndex, pinDrive);
        gpio_set_slew_rate (pinIndex, pinSlew);

        // Set the bias resistor.
        gpio_set_pulls (pinIndex, pullUp, pullDown);
    }
    GPIO_MUTEX_RELEASE ();
    return pinAvailable;
}

/*
 * Sets up one of the RP2040 GPIO pins for alternate function use.
 */
bool gmosDriverGpioAltModeInit (uint16_t gpioPinId,
    uint8_t driveStrength, int8_t biasResistor, uint8_t altFunction)
{
    uint8_t pinBank = (gpioPinId >> 12) & 0x01;
    uint8_t pinIndex = gpioPinId & 0x1F;
    uint8_t pinSlew = (driveStrength >> 4) & 0x01;
    uint8_t pinDrive = driveStrength & 0x03;
    bool pullUp = (biasResistor > 0);
    bool pullDown = (biasResistor < 0);
    bool pinAvailable = false;

    // Reuse of the program interface pins is not currently supported.
    if ((pinBank != PICO_GPIO_BANK_U) || (pinIndex > 29)) {
        return false;
    }

    // Check for pin conflicts before configuring the pin.
    GPIO_MUTEX_CLAIM ();
    if (gpio_get_function (pinIndex) == GPIO_FUNC_NULL) {
        pinAvailable = true;

        // Initialise the pin and select the alternate mode of operation.
        gpio_init (pinIndex);
        gpio_set_function (pinIndex, altFunction);

        // Set the drive strength and slew rate.
        gpio_set_drive_strength (pinIndex, pinDrive);
        gpio_set_slew_rate (pinIndex, pinSlew);

        // Set the bias resistor.
        gpio_set_pulls (pinIndex, pullUp, pullDown);
    }
    GPIO_MUTEX_RELEASE ();
    return pinAvailable;
}

/*
 * Sets the GPIO pin direction.
 */
static bool gmosDriverGpioSetDirection (uint16_t gpioPinId, bool isOutput)
{
    uint8_t pinBank = (gpioPinId >> 12) & 0x01;
    uint8_t pinIndex = gpioPinId & 0x1F;
    bool setOk = true;

    // Reuse of the program interface pins is not currently supported.
    if ((pinBank != PICO_GPIO_BANK_U) || (pinIndex > 29)) {
        return false;
    }

    // Check that the GPIO pin is not in use for an alternate function
    // or ADC input.
    GPIO_MUTEX_CLAIM ();
    if (gpio_get_function (pinIndex) != GPIO_FUNC_SIO) {
        setOk = false;
        goto out;
    }

    // Handle open drain outputs that default to active high.
    if ((gpioStatusFlags & (1 << pinIndex)) != 0) {
        if (gpio_get_out_level (pinIndex)) {
            isOutput = false;
        }
    }

    // Set the GPIO pin direction.
    gpio_set_dir (pinIndex, isOutput);

    // Always release the GPIO mutex on exit.
out:
    GPIO_MUTEX_RELEASE ();
    return setOk;
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
    uint8_t pinBank = (gpioPinId >> 12) & 0x01;
    uint8_t pinIndex = gpioPinId & 0x1F;
    bool isOpenDrain = ((gpioStatusFlags & (1 << pinIndex)) != 0);

    // Reuse of the program interface pins is not currently supported.
    if ((pinBank != PICO_GPIO_BANK_U) || (pinIndex > 29)) {
        return;
    }

    // Set the pin high, including open drain control if required.
    GPIO_MUTEX_CLAIM ();
    if (pinState) {
        if (isOpenDrain) {
            gpio_set_dir (pinIndex, false);
        }
        gpio_put (pinIndex, true);
    }

    // Set the pin low, including open drain control if required.
    else {
        gpio_put (pinIndex, false);
        if (isOpenDrain) {
            gpio_set_dir (pinIndex, true);
        }
    }
    GPIO_MUTEX_RELEASE ();
}

/*
 * Gets the GPIO pin state. If the GPIO is configured as an input this
 * will be the sampled value and if configured as an output this will
 * be the current output value.
 */
bool gmosDriverGpioGetPinState (uint16_t gpioPinId)
{
    uint8_t pinBank = (gpioPinId >> 12) & 0x01;
    uint8_t pinIndex = gpioPinId & 0x1F;
    bool pinState = false;

    // Reuse of the program interface pins is not currently supported.
    if ((pinBank != PICO_GPIO_BANK_U) || (pinIndex > 29)) {
        return false;
    }

    // Read back the pin state.
    GPIO_MUTEX_CLAIM ();
    pinState = (gpio_get (pinIndex)) ? true : false;
    GPIO_MUTEX_RELEASE ();
    return pinState;
}

/*
 * Implement the common GPIO ISR handler, which will dispatch the
 * received interrupt to the appropriate GPIO specific ISR. The
 * corresponding interrupt condition is automatically cleared by the
 * SDK interrupt handler.
 */
static void gmosDriverGpioInterruptHandler (
    uint pinIndex, uint32_t eventMask)
{
    uint slot;
    for (slot = 0; slot < GMOS_CONFIG_PICO_GPIO_MAX_ISRS; slot++) {
        if (pinIndex == gpioIsrPinIds [slot]) {
            gpioIsrHandlers [slot] (gpioIsrDataItems [slot]);
            break;
        }
    }
}

/*
 * Initialises a general purpose IO pin for interrupt generation. The
 * interrupt is not enabled at this stage.
 */
bool gmosDriverGpioInterruptInit (uint16_t gpioPinId,
    gmosDriverGpioIsr_t gpioIsr, void* gpioIsrData, int8_t biasResistor)
{
    uint8_t pinBank = (gpioPinId >> 12) & 0x01;
    uint8_t pinIndex = gpioPinId & 0x1F;
    uint32_t isrActiveMask = (1 << (30 + get_core_num ()));
    bool initOk;
    uint slot;

    // Reuse of the program interface pins is not currently supported.
    if ((pinBank != PICO_GPIO_BANK_U) || (pinIndex > 29)) {
        return false;
    }

    // Search for an available ISR slot, checking for potential pin
    // conflicts.
    GPIO_MUTEX_CLAIM ();
    for (slot = 0; slot < GMOS_CONFIG_PICO_GPIO_MAX_ISRS; slot++) {
        if (gpioIsrPinIds [slot] == 0xFF) {
            break;
        } else if (gpioIsrPinIds [slot] == pinIndex) {
            initOk = false;
            goto out;
        }
    }
    if (slot >= GMOS_CONFIG_PICO_GPIO_MAX_ISRS) {
        initOk = false;
        goto out;
    }

    // Initialise the pin as an input with the specified bias resistor.
    if (!gmosDriverGpioPinInit (gpioPinId,
        GMOS_DRIVER_GPIO_OUTPUT_PUSH_PULL,
        GMOS_DRIVER_GPIO_SLEW_MINIMUM, biasResistor)) {
        initOk = false;
        goto out;
    }

    // On first call from a specific core, register the common GPIO ISR.
    if ((gpioStatusFlags & isrActiveMask) == 0) {
        gpio_set_irq_callback (gmosDriverGpioInterruptHandler);
        irq_set_enabled (IO_IRQ_BANK0, true);
        gpioStatusFlags |= isrActiveMask;
    }

    // Populate the ISR handler slot.
    gpioIsrPinIds [slot] = pinIndex;
    gpioIsrHandlers [slot] = gpioIsr;
    gpioIsrDataItems [slot] = gpioIsrData;
    initOk = true;

    // Release the GPIO mutex on exit.
out:
    GPIO_MUTEX_RELEASE ();
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
    uint8_t pinBank = (gpioPinId >> 12) & 0x01;
    uint8_t pinIndex = gpioPinId & 0x1F;
    uint8_t eventFlags;

    // Reuse of the program interface pins is not currently supported.
    if ((pinBank != PICO_GPIO_BANK_U) || (pinIndex > 29)) {
        return;
    }

    // Set the required event flags.
    eventFlags = 0;
    if (risingEdge) {
        eventFlags |= GPIO_IRQ_EDGE_RISE;
    }
    if (fallingEdge) {
        eventFlags |= GPIO_IRQ_EDGE_FALL;
    }

    // Disable unused event flags then set required flags.
    GPIO_MUTEX_CLAIM ();
    gpio_set_irq_enabled (pinIndex, 0x0F & ~eventFlags, false);
    gpio_set_irq_enabled (pinIndex, eventFlags, true);
    GPIO_MUTEX_RELEASE ();
}

/*
 * Disables a GPIO interrupt for the specified GPIO pin. This should be
 * called after enabling a general purpose IO pin as an interrupt source
 * in order to stop receiving interrupt notifications.
 */
void gmosDriverGpioInterruptDisable (uint16_t gpioPinId)
{
    uint8_t pinBank = (gpioPinId >> 12) & 0x01;
    uint8_t pinIndex = gpioPinId & 0x1F;

    // Reuse of the program interface pins is not currently supported.
    if ((pinBank != PICO_GPIO_BANK_U) || (pinIndex > 29)) {
        return;
    }

    // Disable all event flags.
    GPIO_MUTEX_CLAIM ();
    gpio_set_irq_enabled (pinIndex, 0x0F, false);
    GPIO_MUTEX_RELEASE ();
}

/*
 * Initialises the GPIO platform abstraction layer on startup.
 */
void gmosPalGpioInit (void)
{
    uint slot;

    // Clear all the local GPIO status flags.
    gpioStatusFlags = 0;

    // Mark all ISR slots as unused.
    for (slot = 0; slot < GMOS_CONFIG_PICO_GPIO_MAX_ISRS; slot++) {
        gpioIsrPinIds [slot] = 0xFF;
    }
}
