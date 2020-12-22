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
 * Demonstrates the use of the GPIO drivers using the RGB LED and
 * joystick switch on the ARM MBed development shield.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-platform.h"
#include "gmos-scheduler.h"
#include "gmos-driver-gpio.h"
#include "stm32-device.h"
#include "stm32-driver-gpio.h"

// Specify the GPIO pins to use for the STM32L010 Nucleo Board.
#if (TARGET_DEVICE == STM32L010RB)
#define RGB_LED_RED_PIN   (STM32_GPIO_BANK_B | 4) // Arduino D5
#define RGB_LED_GREEN_PIN (STM32_GPIO_BANK_C | 7) // Arduino D9
#define RGB_LED_BLUE_PIN  (STM32_GPIO_BANK_A | 9) // Arduino D8
#define COLOUR_SWITCH_PIN (STM32_GPIO_BANK_B | 5) // Arduino D4

// Specify the GPIO pins to use for the STM32 LoRa Discovery Kit.
#elif (TARGET_DEVICE == STM32L072CZ)
#define RGB_LED_RED_PIN   (STM32_GPIO_BANK_B | 7)  // Arduino D5
#define RGB_LED_GREEN_PIN (STM32_GPIO_BANK_B | 12) // Arduino D9
#define RGB_LED_BLUE_PIN  (STM32_GPIO_BANK_A | 9)  // Arduino D8
#define COLOUR_SWITCH_PIN (STM32_GPIO_BANK_B | 5)  // Arduino D4
#endif

// Defines the current RGB LED on/off state.
static bool rgbLedIsOn = false;

// Defines the current RGB LED colour.
static volatile uint8_t rgbLedColour = 0;

// Specifies the current RGB LED flashing interval.
static uint32_t rgbLedFlashInterval = GMOS_MS_TO_TICKS (1000);

// Allocate the LED flasher task state data structure.
static gmosTaskState_t ledFlashingTaskState;

/*
 * Implements the LED timed flashing task.
 */
static gmosTaskStatus_t ledFlashingHandler (void* nullData)
{
    // Turn all the RGB LED outputs off. Note that the LED state is
    // inverted from the pin state.
    if (rgbLedIsOn) {
        gmosDriverGpioSetPinState (RGB_LED_RED_PIN, true);
        gmosDriverGpioSetPinState (RGB_LED_GREEN_PIN, true);
        gmosDriverGpioSetPinState (RGB_LED_BLUE_PIN, true);
        rgbLedIsOn = false;
    }

    // Turn on the selected LED output.
    else {
        switch (rgbLedColour) {
            case 0 :
                gmosDriverGpioSetPinState (RGB_LED_RED_PIN, false);
                break;
            case 1 :
                gmosDriverGpioSetPinState (RGB_LED_GREEN_PIN, false);
                break;
            default :
                gmosDriverGpioSetPinState (RGB_LED_BLUE_PIN, false);
                break;
        }
        rgbLedIsOn = true;
    }

    // Schedule an LED state update after the specified interval.
    return GMOS_TASK_RUN_LATER (rgbLedFlashInterval);
}

// Define the LED flashing task.
GMOS_TASK_DEFINITION (ledFlashing, ledFlashingHandler, void);

/*
 * Implement the 'joystick' centre switch ISR. This just cycles through
 * the available LED colours.
 */
void ledColourCycleIsr (void)
{
    uint8_t newRgbLedColour = rgbLedColour + 1;
    if (newRgbLedColour >= 3) {
        newRgbLedColour = 0;
    }
    rgbLedColour = newRgbLedColour;
}

/*
 * Initialise the GPIO demo tasks.
 */
void demoGpioInit (void)
{
    // Initialise the RGB LED GPIO pins.
    gmosDriverGpioPinInit (RGB_LED_RED_PIN, GMOS_DRIVER_GPIO_OUTPUT_PUSH_PULL,
        STM32_GPIO_DRIVER_SLEW_SLOW, GMOS_DRIVER_GPIO_INPUT_PULL_NONE);
    gmosDriverGpioPinInit (RGB_LED_GREEN_PIN, GMOS_DRIVER_GPIO_OUTPUT_PUSH_PULL,
        STM32_GPIO_DRIVER_SLEW_SLOW, GMOS_DRIVER_GPIO_INPUT_PULL_NONE);
    gmosDriverGpioPinInit (RGB_LED_BLUE_PIN, GMOS_DRIVER_GPIO_OUTPUT_PUSH_PULL,
        STM32_GPIO_DRIVER_SLEW_SLOW, GMOS_DRIVER_GPIO_INPUT_PULL_NONE);

    // Initialise the 'joystick' centre switch as an interrupt source.
    gmosDriverGpioInterruptInit (COLOUR_SWITCH_PIN,
        ledColourCycleIsr, GMOS_DRIVER_GPIO_INPUT_PULL_NONE);
    gmosDriverGpioInterruptEnable (COLOUR_SWITCH_PIN, true, false);

    // Configure the RGB LED GPIO pins as outputs.
    gmosDriverGpioSetAsOutput (RGB_LED_RED_PIN);
    gmosDriverGpioSetAsOutput (RGB_LED_GREEN_PIN);
    gmosDriverGpioSetAsOutput (RGB_LED_BLUE_PIN);

    // Turn all the RGB LED outputs off. Note that the LED state is
    // inverted from the pin state.
    gmosDriverGpioSetPinState (RGB_LED_RED_PIN, true);
    gmosDriverGpioSetPinState (RGB_LED_GREEN_PIN, true);
    gmosDriverGpioSetPinState (RGB_LED_BLUE_PIN, true);

    // Run the LED flashing task.
    ledFlashing_start (&ledFlashingTaskState, NULL, "LED Flashing Task");
}
