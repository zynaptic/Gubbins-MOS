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
 * Implements general purpose hardware timer functionality for the
 * Microchip Harmony vendor framework.
 */

#include <stdint.h>
#include <stdbool.h>

#include "gmos-platform.h"
#include "gmos-driver-timer.h"
#include "harmony-driver-timer.h"

/*
 * Sets the timer clock frequency by configuring the clock prescaler.
 */
static inline bool harmonyDriverTimerSetClock (
    DRV_HANDLE drvHandle, gmosDriverTimer_t* timer, uint32_t frequency)
{
    uint32_t frequencySelect;
    TMR_PRESCALE prescaler;

    // Derive the preferred prescaler for the requested frequency. If an
    // approximate frequency is used, it will be lower than requested,
    // unless it is less than the minimum clock frequency.
    if (frequency >= HARMONY_DRIVER_TIMER_CLOCK) {
        frequencySelect = HARMONY_DRIVER_TIMER_CLOCK;
        prescaler = TMR_PRESCALE_VALUE_1;
    } else if (frequency >= HARMONY_DRIVER_TIMER_CLOCK / 2) {
        frequencySelect = HARMONY_DRIVER_TIMER_CLOCK / 2;
        prescaler = TMR_PRESCALE_VALUE_2;
    } else if (frequency >= HARMONY_DRIVER_TIMER_CLOCK / 4) {
        frequencySelect = HARMONY_DRIVER_TIMER_CLOCK / 4;
        prescaler = TMR_PRESCALE_VALUE_4;
    } else if (frequency >= HARMONY_DRIVER_TIMER_CLOCK / 8) {
        frequencySelect = HARMONY_DRIVER_TIMER_CLOCK / 8;
        prescaler = TMR_PRESCALE_VALUE_8;
    } else if (frequency >= HARMONY_DRIVER_TIMER_CLOCK / 16) {
        frequencySelect = HARMONY_DRIVER_TIMER_CLOCK / 16;
        prescaler = TMR_PRESCALE_VALUE_16;
    } else if (frequency >= HARMONY_DRIVER_TIMER_CLOCK / 32) {
        frequencySelect = HARMONY_DRIVER_TIMER_CLOCK / 32;
        prescaler = TMR_PRESCALE_VALUE_32;
    } else if (frequency >= HARMONY_DRIVER_TIMER_CLOCK / 64) {
        frequencySelect = HARMONY_DRIVER_TIMER_CLOCK / 64;
        prescaler = TMR_PRESCALE_VALUE_64;
    } else {
        frequencySelect = HARMONY_DRIVER_TIMER_CLOCK / 256;
        prescaler = TMR_PRESCALE_VALUE_256;
    }
    if (frequency != frequencySelect) {
        GMOS_LOG_FMT (LOG_WARNING,
            "Timer clock requested %ldHz, actual %ldHz.",
            frequency, frequencySelect);
    }
    timer->frequency = frequencySelect;

    // Set the timer prescaler being used.
    return DRV_TMR_ClockSet (
        drvHandle, DRV_TMR_CLKSOURCE_INTERNAL, prescaler);
}

/*
 * Initialises a timer for interrupt generation. This should be called
 * for each timer prior to accessing it via any of the other API
 * functions. The timer and associated interrupt are not enabled at this
 * stage.
 */
bool gmosDriverTimerInit (gmosDriverTimer_t* timer, uint32_t frequency,
    gmosDriverTimerIsr_t timerIsr, void* timerIsrData)
{
    const gmosPalTimerConfig_t* timerConfig = timer->palConfig;
    gmosPalTimerState_t* timerState = timer->palData;
    DRV_HANDLE drvHandle;
    DRV_TMR_DIVIDER_RANGE timerRange;
    DRV_TMR_OPERATION_MODE timerMode;

    // Open the Harmony timer driver ready for use.
    drvHandle = DRV_TMR_Open (
        timerConfig->harmonyDeviceIndex, DRV_IO_INTENT_EXCLUSIVE);
    if (drvHandle == DRV_HANDLE_INVALID) {
        GMOS_LOG (LOG_ERROR, "Failed to open timer instance.");
        return false;
    }

    // Configure the timer clocks.
    if (!harmonyDriverTimerSetClock (drvHandle, timer, frequency)) {
        return false;
    }

    // Set the maximum supported timer value.
    timerMode = DRV_TMR_DividerRangeGet (drvHandle, &timerRange);
    if (timerMode != DRV_TMR_OPERATION_MODE_16_BIT) {
        return false;
    }
    timer->maxValue = (uint16_t) timerRange.dividerMax;

    // Update the timer driver state.
    timerState->harmonyDriver = drvHandle;
    timer->timerIsr = timerIsr;
    timer->timerIsrData = timerIsrData;
    timer->activeState = GMOS_DRIVER_TIMER_STATE_RESET;
    return true;
}

/*
 * Enables a timer and associated interrupt for subsequent use. The
 * timer will be placed in its reset hold state once it has been
 * enabled.
 */
bool gmosDriverTimerEnable (gmosDriverTimer_t* timer)
{
    return gmosDriverTimerReset (timer, true) &&
        gmosDriverTimerIsrMask (timer, false);
}

/*
 * Disables a timer and associated interrupt for subsequent use.
 */
bool gmosDriverTimerDisable (gmosDriverTimer_t* timer)
{
    return gmosDriverTimerReset (timer, true) &&
        gmosDriverTimerIsrMask (timer, true);
}

/*
 * Masks the timer interrupts, controlling when the timer interrupt
 * service routine will be allowed to run.
 */
bool gmosDriverTimerIsrMask (gmosDriverTimer_t* timer, bool isrMask)
{
    gmosPalTimerState_t* timerState = timer->palData;
    DRV_HANDLE drvHandle = timerState->harmonyDriver;

    if (timer->activeState != GMOS_DRIVER_TIMER_STATE_RESET) {
        if (isrMask) {
            DRV_TMR_AlarmDisable (drvHandle);
        } else {
            DRV_TMR_AlarmEnable (drvHandle, true);
        }
    }
    return true;
}

/*
 * Resets the current value of the timer counter to zero. The timer must
 * be enabled prior to performing a timer reset.
 */
bool gmosDriverTimerReset (gmosDriverTimer_t* timer, bool resetHold)
{
    gmosPalTimerState_t* timerState = timer->palData;
    DRV_HANDLE drvHandle = timerState->harmonyDriver;

    if ((resetHold) &&
        (timer->activeState != GMOS_DRIVER_TIMER_STATE_RESET)) {
        DRV_TMR_Stop (drvHandle);
        DRV_TMR_AlarmDisable (drvHandle);
        DRV_TMR_AlarmDeregister (drvHandle);
        timer->activeState = GMOS_DRIVER_TIMER_STATE_RESET;
    }
    DRV_TMR_CounterClear (drvHandle);
    return true;
}

/*
 * Accesses the current timer counter value.
 */
uint16_t gmosDriverTimerGetValue (gmosDriverTimer_t* timer)
{
    gmosPalTimerState_t* timerState = timer->palData;
    DRV_HANDLE drvHandle = timerState->harmonyDriver;

    return (uint16_t) DRV_TMR_CounterValueGet (drvHandle);
}

/*
 * Implement common interrupt handling for microchip Harmony timers.
 */
static void harmonyDriverTimerCallback (
    uintptr_t context, uint32_t alarmCount)
{
    gmosDriverTimer_t* timer = (gmosDriverTimer_t*) context;

    // Place the timer in reset if a one-shot timer is used.
    if (timer->activeState == GMOS_DRIVER_TIMER_STATE_ONE_SHOT) {
        gmosDriverTimerReset (timer, true);
    }

    // Invoke the user ISR.
    if (timer->timerIsr != NULL) {
        timer->timerIsr (timer->timerIsrData);
    }
}

/*
 * Implements common setup for timer run requests.
 */
static bool harmonyDriverTimerRun (
    gmosDriverTimer_t* timer, uint16_t alarm, bool runOneShot)
{
    gmosPalTimerState_t* timerState = timer->palData;
    DRV_HANDLE drvHandle = timerState->harmonyDriver;

    // Ensure that the timer is in the reset state before making any
    // changes.
    if (timer->activeState != GMOS_DRIVER_TIMER_STATE_RESET) {
        return false;
    }

    // Register a new alarm callback.
    if (!DRV_TMR_AlarmRegister (drvHandle, alarm, !runOneShot,
        (uintptr_t) timer, harmonyDriverTimerCallback)) {
        return false;
    }

    // Set the new timer state.
    if (runOneShot) {
        timer->activeState = GMOS_DRIVER_TIMER_STATE_ONE_SHOT;
    } else {
        timer->activeState = GMOS_DRIVER_TIMER_STATE_CONTINUOUS;
    }

    // Start the timer running.
    DRV_TMR_AlarmEnable (drvHandle, true);
    DRV_TMR_Start (drvHandle);
    return true;
}

/*
 * Sets a one-shot alarm for the timer counter.
 */
bool gmosDriverTimerRunOneShot (gmosDriverTimer_t* timer, uint16_t alarm)
{
    return harmonyDriverTimerRun (timer, alarm, true);
}

/*
 * Sets a repeating alarm for the timer counter.
 */
bool gmosDriverTimerRunRepeating (gmosDriverTimer_t* timer, uint16_t alarm)
{
    return harmonyDriverTimerRun (timer, alarm, false);
}
