/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2023-2026 Zynaptic Limited
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
 * This file implements common GPIO functions for OpenThread related LED
 * and push button functions.
 */

#include <stdint.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-scheduler.h"
#include "gmos-events.h"
#include "gmos-driver-gpio.h"
#include "gmos-openthread.h"
#include "gmos-openthread-gpio.h"

/*
 * Only compile network state indicator LED code if a valid LED output
 * pin has been specified in the configuration.
 */
#if GMOS_CONFIG_OPENTHREAD_NETWORK_INDICATOR_LED_PIN_ID != \
    GMOS_DRIVER_GPIO_UNUSED_PIN_ID

// Only a single LED is supported, so state variables can be allocated
// locally.
static gmosTaskState_t indicatorLedTaskData;
static uint8_t         indicatorLedMode;
static uint8_t         indicatorLedState;

/*
 * Implement the network state indicator LED processing task.
 */
static inline gmosTaskStatus_t indicatorLedTaskFn (
    void* nullData)
{
    gmosTaskStatus_t taskStatus;
    (void) nullData;

    // Select the appropriate indicator mode.
    switch (indicatorLedMode) {

        // Implement fast flashing.
        case GMOS_OPENTHREAD_NETWORK_INDICATOR_LED_MODE_FLASH_FAST :
            taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (250));
            indicatorLedState = !indicatorLedState;
            break;

        // Implement slow flashing.
        case GMOS_OPENTHREAD_NETWORK_INDICATOR_LED_MODE_FLASH_SLOW :
            taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (500));
            indicatorLedState = !indicatorLedState;
            break;

        // Turn the indicator LED on.
        case GMOS_OPENTHREAD_NETWORK_INDICATOR_LED_MODE_ON :
            taskStatus = GMOS_TASK_SUSPEND;
            indicatorLedState =
                !GMOS_CONFIG_OPENTHREAD_NETWORK_INDICATOR_LED_INVERT;
            break;

        // Turn the indicator LED off.
        default :
            taskStatus = GMOS_TASK_SUSPEND;
            indicatorLedState =
                GMOS_CONFIG_OPENTHREAD_NETWORK_INDICATOR_LED_INVERT;
            break;
    }

    // Set the output pin state and reschedule the task if required.
    gmosDriverGpioSetPinState (
        GMOS_CONFIG_OPENTHREAD_NETWORK_INDICATOR_LED_PIN_ID,
            indicatorLedState);
    return taskStatus;
}

// Define the network state indicator LED processing task.
GMOS_TASK_DEFINITION (indicatorLedTask, indicatorLedTaskFn, void);

/*
 * Initialises the network state indicator LED.
 */
static inline void indicatorLedInit (void)
{
    // Initialise the local state variables.
    indicatorLedMode =
        GMOS_OPENTHREAD_NETWORK_INDICATOR_LED_MODE_OFF;
    indicatorLedState =
        GMOS_CONFIG_OPENTHREAD_NETWORK_INDICATOR_LED_INVERT;

    // Set up the LED driver pin.
    gmosDriverGpioPinInit (
        GMOS_CONFIG_OPENTHREAD_NETWORK_INDICATOR_LED_PIN_ID,
        GMOS_CONFIG_OPENTHREAD_NETWORK_INDICATOR_LED_OPEN_DRAIN,
        GMOS_DRIVER_GPIO_SLEW_MINIMUM, GMOS_DRIVER_GPIO_INPUT_PULL_NONE);
    gmosDriverGpioSetAsOutput (
        GMOS_CONFIG_OPENTHREAD_NETWORK_INDICATOR_LED_PIN_ID);
    gmosDriverGpioSetPinState (
        GMOS_CONFIG_OPENTHREAD_NETWORK_INDICATOR_LED_PIN_ID,
        GMOS_CONFIG_OPENTHREAD_NETWORK_INDICATOR_LED_INVERT);

    // Run the network state indicator LED task.
    indicatorLedTask_start (
        &indicatorLedTaskData, NULL, "OpenThread Stack LED");
}

/*
 * Sets the OpenThread network status indicator LED output mode.
 */
void gmosOpenThreadSetIndicatorLed (
    gmosOpenThreadNetworkIndicatorLedMode_t ledMode)
{
    // Ignore calls which do not change the state.
    if (ledMode == indicatorLedMode) {
        return;
    }

    // Turn LED off. The LED indicator task can be left idle.
    else if (ledMode == GMOS_OPENTHREAD_NETWORK_INDICATOR_LED_MODE_OFF) {
        indicatorLedState =
            GMOS_CONFIG_OPENTHREAD_NETWORK_INDICATOR_LED_INVERT;
    }

    // Turn LED on. The LED indicator task can be left idle.
    else if (ledMode == GMOS_OPENTHREAD_NETWORK_INDICATOR_LED_MODE_ON) {
        indicatorLedState =
            !GMOS_CONFIG_OPENTHREAD_NETWORK_INDICATOR_LED_INVERT;
    }

    // Start LED flashing. The LED indicator task needs to be resumed.
    else {
        indicatorLedState =
            GMOS_CONFIG_OPENTHREAD_NETWORK_INDICATOR_LED_INVERT;
        gmosSchedulerTaskResume (&indicatorLedTaskData);
    }

    // Update the current LED indicator mode and GPIO output state.
    indicatorLedMode = ledMode;
    gmosDriverGpioSetPinState (
        GMOS_CONFIG_OPENTHREAD_NETWORK_INDICATOR_LED_PIN_ID,
            indicatorLedState);
}
#endif // GMOS_CONFIG_OPENTHREAD_NETWORK_INDICATOR_LED_PIN_ID

/*
 * Only compile factory reset button code if a valid software reset pin
 * has been specified in the configuration.
 */
#if GMOS_CONFIG_OPENTHREAD_FACTORY_RESET_PIN_ID != \
    GMOS_DRIVER_GPIO_UNUSED_PIN_ID

// Only a single software reset pin is supported, so state variables can
// be allocated locally.
static gmosTaskState_t factoryResetTaskData;
static gmosEvent_t     factoryResetEvent;
static uint8_t         factoryResetState;
static uint32_t        factoryResetTimestamp;

/*
 * Define the state space for the rest handler state machine.
 */
typedef enum {
    FACTORY_RESET_TASK_STATE_INIT,
    FACTORY_RESET_TASK_HARDWARE_ENABLE,
    FACTORY_RESET_TASK_FIRST_SAMPLE,
    FACTORY_RESET_TASK_IDLE,
    FACTORY_RESET_TASK_TIME_SOFTWARE_RESET,
    FACTORY_RESET_TASK_TIME_FACTORY_RESET,
    FACTORY_RESET_TASK_RUN_SOFTWARE_RESET,
    FACTORY_RESET_TASK_RUN_FACTORY_RESET,
    FACTORY_RESET_TASK_COMPLETE
} factoryResetTaskState_t;

/*
 * Specify the event flags used to notify changes in the button state.
 */
typedef enum {
    FACTORY_RESET_EVENT_BUTTON_PRESSED  = 0x01,
    FACTORY_RESET_EVENT_BUTTON_RELEASED = 0x02
} factoryResetEventFlags_t;

/*
 * Implements the factory reset pin interrupt service routine.
 */
static void factoryResetIsr (void* nullData)
{
    uint32_t eventFlags;
    bool buttonPressed;
    (void) nullData;

    // Read back the current button state. Invert it if required.
    buttonPressed = gmosDriverGpioGetPinState (
        GMOS_CONFIG_OPENTHREAD_FACTORY_RESET_PIN_ID);
    if (GMOS_CONFIG_OPENTHREAD_FACTORY_RESET_INVERT) {
        buttonPressed = !buttonPressed;
    }

    // Set the appropriate event flags.
    if (buttonPressed) {
        eventFlags = FACTORY_RESET_EVENT_BUTTON_PRESSED;
    } else {
        eventFlags = FACTORY_RESET_EVENT_BUTTON_RELEASED;
    }
    gmosEventSetBits (&factoryResetEvent, eventFlags);
}

/*
 * Time software reset button pushes.
 */
static inline gmosTaskStatus_t timeSoftwareResetEvents (
    uint_fast8_t* nextState)
{
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    uint32_t elapsedTicks;
    uint32_t eventFlags;

    // Get the current timer and flag values.
    elapsedTicks = gmosPalGetTimer () - factoryResetTimestamp;
    eventFlags = gmosEventResetBits (&factoryResetEvent);

    // Check for timer expiry.
    if (elapsedTicks >= GMOS_MS_TO_TICKS (
        GMOS_CONFIG_OPENTHREAD_FACTORY_RESET_MIN_DELAY)) {
        GMOS_LOG (LOG_DEBUG, "Software reset timer expired.");
        if (GMOS_CONFIG_OPENTHREAD_FACTORY_RESET_POWER_ON_ONLY) {
            *nextState = FACTORY_RESET_TASK_IDLE;
        } else {
            *nextState = FACTORY_RESET_TASK_TIME_FACTORY_RESET;
        }
    }

    // Check for button release event within the timer window.
    else if ((eventFlags & FACTORY_RESET_EVENT_BUTTON_RELEASED) != 0) {
        if (elapsedTicks >= GMOS_MS_TO_TICKS (
            GMOS_CONFIG_OPENTHREAD_FACTORY_RESET_DEBOUNCE_DELAY)) {
            GMOS_LOG (LOG_WARNING, "*** OpenThread Software Reset ***");
            *nextState = FACTORY_RESET_TASK_RUN_SOFTWARE_RESET;
            taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (100));
        } else {
            GMOS_LOG (LOG_DEBUG, "Software reset timer ignored.");
            *nextState = FACTORY_RESET_TASK_IDLE;
        }
    }

    // Reschedule the timer.
    else {
        GMOS_LOG (LOG_DEBUG, "Software reset timer rescheduled.");
        taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (
            GMOS_CONFIG_OPENTHREAD_FACTORY_RESET_MIN_DELAY) - elapsedTicks);
    }
    return taskStatus;
}

/*
 * Time factory reset button pushes.
 */
static inline gmosTaskStatus_t timeFactoryResetEvents (
    uint_fast8_t* nextState)
{
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    uint32_t elapsedTicks;
    uint32_t eventFlags;

    // Get the current timer and flag values.
    elapsedTicks = gmosPalGetTimer () - factoryResetTimestamp;
    eventFlags = gmosEventResetBits (&factoryResetEvent);

    // Check for timer expiry.
    if (elapsedTicks >= GMOS_MS_TO_TICKS (
        GMOS_CONFIG_OPENTHREAD_FACTORY_RESET_MAX_DELAY)) {
        GMOS_LOG (LOG_DEBUG, "Factory reset timer expired.");
        *nextState = FACTORY_RESET_TASK_IDLE;
    }

    // Check for button release event within the timer window.
    else if ((eventFlags & FACTORY_RESET_EVENT_BUTTON_RELEASED) != 0) {
        if (elapsedTicks >= GMOS_MS_TO_TICKS (
            GMOS_CONFIG_OPENTHREAD_FACTORY_RESET_MIN_DELAY)) {
            GMOS_LOG (LOG_WARNING, "*** OpenThread Factory Reset ***");
            *nextState = FACTORY_RESET_TASK_RUN_FACTORY_RESET;
            taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (100));
        } else {
            GMOS_LOG (LOG_DEBUG, "Factory reset timer ignored.");
            *nextState = FACTORY_RESET_TASK_IDLE;
        }
    }

    // Reschedule the timer.
    else {
        GMOS_LOG (LOG_DEBUG, "Factory reset timer rescheduled.");
        taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (
            GMOS_CONFIG_OPENTHREAD_FACTORY_RESET_MAX_DELAY) - elapsedTicks);
    }
    return taskStatus;
}

/*
 * Implements the device factory reset task.
 */
static inline gmosTaskStatus_t factoryResetTaskFn (
    gmosOpenThreadStack_t* openThreadStack)
{
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    uint_fast8_t nextState = factoryResetState;
    uint32_t eventFlags;

    // Run the IoT Sync framework reset state machine.
    switch (factoryResetState) {

        // Always enable factory reset support on startup.
        case FACTORY_RESET_TASK_STATE_INIT :
            nextState = FACTORY_RESET_TASK_HARDWARE_ENABLE;
            break;

        // Enable hardware reset support.
        case FACTORY_RESET_TASK_HARDWARE_ENABLE :
            gmosDriverGpioInterruptEnable (
                GMOS_CONFIG_OPENTHREAD_FACTORY_RESET_PIN_ID, true, true);
            nextState = FACTORY_RESET_TASK_FIRST_SAMPLE;
            break;

        // Sample the reset button state on power up.
        case FACTORY_RESET_TASK_FIRST_SAMPLE :
            eventFlags = gmosEventResetBits (&factoryResetEvent);
            if ((eventFlags & FACTORY_RESET_EVENT_BUTTON_PRESSED) != 0) {
                factoryResetTimestamp = gmosPalGetTimer ();
                nextState = FACTORY_RESET_TASK_TIME_FACTORY_RESET;
            } else {
                nextState = FACTORY_RESET_TASK_IDLE;
            }
            break;

        // Wait for reset cycle to start.
        case FACTORY_RESET_TASK_IDLE :
            eventFlags = gmosEventResetBits (&factoryResetEvent);
            if ((eventFlags & FACTORY_RESET_EVENT_BUTTON_PRESSED) != 0) {
                factoryResetTimestamp = gmosPalGetTimer ();
                nextState = FACTORY_RESET_TASK_TIME_SOFTWARE_RESET;
            } else {
                taskStatus = GMOS_TASK_SUSPEND;
            }
            break;

        // Time software reset button pushes.
        case FACTORY_RESET_TASK_TIME_SOFTWARE_RESET :
            taskStatus = timeSoftwareResetEvents (&nextState);
            break;

        // Time factory reset button pushes.
        case FACTORY_RESET_TASK_TIME_FACTORY_RESET :
            taskStatus = timeFactoryResetEvents (&nextState);
            break;

        // Run the software reset process.
        case FACTORY_RESET_TASK_RUN_SOFTWARE_RESET :
            gmosOpenThreadReset (openThreadStack,
                GMOS_OPENTHREAD_RESET_TYPE_DEFAULT);
            nextState = FACTORY_RESET_TASK_COMPLETE;
            break;

        // Run the factory reset process.
        case FACTORY_RESET_TASK_RUN_FACTORY_RESET :
            gmosOpenThreadReset (openThreadStack,
                GMOS_OPENTHREAD_RESET_TYPE_FACTORY);
            nextState = FACTORY_RESET_TASK_COMPLETE;
            break;

        // Stop when reset request is complete.
        case FACTORY_RESET_TASK_COMPLETE :
            GMOS_LOG (LOG_DEBUG, "Reset processing complete.");
            taskStatus = GMOS_TASK_SUSPEND;
            break;
    }
    factoryResetState = nextState;
    return taskStatus;
}

// Define the device factory reset task.
GMOS_TASK_DEFINITION (factoryResetTask,
    factoryResetTaskFn, gmosOpenThreadStack_t);

/*
 * Initialises the device factory reset handler on device startup.
 */
void factoryResetInit (gmosOpenThreadStack_t* openThreadStack)
{
    // Set up the factory reset pin as an interrupt source.
    gmosDriverGpioInterruptInit (
        GMOS_CONFIG_OPENTHREAD_FACTORY_RESET_PIN_ID, factoryResetIsr,
        NULL, GMOS_DRIVER_GPIO_INPUT_PULL_NONE);

    // Initialise the factory reset task state.
    factoryResetState = FACTORY_RESET_TASK_STATE_INIT;
    factoryResetTimestamp = gmosPalGetTimer ();
    gmosEventInit (&factoryResetEvent, &factoryResetTaskData);

    // Sample the reset pin on startup using the standard ISR routine.
    factoryResetIsr (NULL);

    // Run the device reset handler task.
    factoryResetTask_start (&factoryResetTaskData,
        openThreadStack, "OpenThread Stack Reset");
}
#endif // GMOS_CONFIG_OPENTHREAD_FACTORY_RESET_PIN_ID

/*
 * Initialises the OpenThread GPIO interaction support tasks.
 */
void gmosOpenThreadGpioInit (gmosOpenThreadStack_t* openThreadStack)
{
    // Initialise the network status indicator LED if required.
#if GMOS_CONFIG_OPENTHREAD_NETWORK_INDICATOR_LED_PIN_ID != \
    GMOS_DRIVER_GPIO_UNUSED_PIN_ID
    indicatorLedInit ();
#endif

    // Initialise the factory reset hardware support if required.
#if GMOS_CONFIG_OPENTHREAD_FACTORY_RESET_PIN_ID != \
    GMOS_DRIVER_GPIO_UNUSED_PIN_ID
    factoryResetInit (openThreadStack);
#endif
}
