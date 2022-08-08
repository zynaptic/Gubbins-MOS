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
 * Implements general purpose timer functionality for the Raspberry Pi
 * RP2040 series of microcontrollers.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-driver-timer.h"
#include "pico-driver-timer.h"
#include "hardware/timer.h"

// Provide reverse mapping of timer IDs to timer state data structures.
static gmosDriverTimer_t* timerDataMap [] = { NULL, NULL, NULL, NULL };

/*
 * Implement common hardware alarm callback handler.
 */
static void hardwareAlarmCallback (uint timerIndex)
{
    gmosDriverTimer_t* timer = NULL;
    gmosPalTimerState_t* palData;
    uint32_t alarmDelay;
    absolute_time_t alarmTime;
    bool alarmMissed;

    // Select the appropriate timer data structure, discarding spurious
    // callbacks.
    if (timerIndex < 4) {
        timer = timerDataMap [timerIndex];
    }
    if (timer == NULL) {
        return;
    }

    // Update one-shot timer state.
    if (timer->activeState == GMOS_DRIVER_TIMER_STATE_ONE_SHOT) {
        if (timer->timerIsr != NULL) {
            timer->timerIsr (timer->timerIsrData);
        }
        timer->activeState = GMOS_DRIVER_TIMER_STATE_RESET;
    }

    // Update continuously repeating timer state. This includes multiple
    // timer ISR callbacks in the extremely unlikely event that the
    // timer is running slow. Note that when updating the alarm period,
    // an additional tick period is included in order to model the
    // wrapping cycle of a conventional hardware counter.
    else if (timer->activeState == GMOS_DRIVER_TIMER_STATE_CONTINUOUS) {
        palData = timer->palData;
        alarmDelay = ((uint32_t) palData->tickPeriod) *
            (1 + (uint32_t) palData->timerPeriod);
        do {
            if (timer->timerIsr != NULL) {
                timer->timerIsr (timer->timerIsrData);
            }
            palData->timestamp += alarmDelay;
            update_us_since_boot (&alarmTime, palData->timestamp + alarmDelay);
            alarmMissed = hardware_alarm_set_target (timerIndex, alarmTime);
        } while (alarmMissed);
    }
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
    const gmosPalTimerConfig_t* palConfig = timer->palConfig;
    gmosPalTimerState_t* palData = timer->palData;
    uint8_t timerIndex;
    uint32_t tickPeriod;
    uint32_t tickFrequency;

    // Check for a valid timer selection.
    timerIndex = palConfig->timerAlarmId;
    if ((timerIndex >= 4) || (timerDataMap [timerIndex] != NULL) ||
        hardware_alarm_is_claimed (timerIndex)) {
        return false;
    }

    // Calculate the closest exact frequency to the requested one.
    tickPeriod = 1000000 / frequency;
    tickFrequency = frequency;
    while ((tickPeriod * tickFrequency) != 1000000) {
        if ((tickPeriod * tickFrequency) > 1000000) {
            tickFrequency -= 1;
        } else {
            tickPeriod += 1;
        }
    }
    if (frequency != tickFrequency) {
        GMOS_LOG_FMT (LOG_WARNING,
            "Timer %d requested frequency %ldHz, actual %ldHz.",
            timerIndex, frequency, tickFrequency);
    }
    GMOS_LOG_FMT (LOG_VERBOSE,
        "Timer %d tick period set to %d us.", timerIndex, tickPeriod);

    // Claim the hardware alarm for exclusive use by the timer.
    hardware_alarm_claim (timerIndex);

    // Set up the timer data structures.
    timer->timerIsr = timerIsr;
    timer->timerIsrData = timerIsrData;
    timer->frequency = tickFrequency;
    timer->maxValue = 0xFFFF;
    timer->activeState = GMOS_DRIVER_TIMER_STATE_RESET;
    palData->tickPeriod = tickPeriod;

    // Populate the timer data slot.
    timerDataMap [timerIndex] = timer;
    return true;
}

/*
 * Enables a timer and associated interrupt for subsequent use. The
 * timer will be placed in its reset hold state once it has been
 * enabled.
 */
bool gmosDriverTimerEnable (gmosDriverTimer_t* timer)
{
    const gmosPalTimerConfig_t* palConfig = timer->palConfig;
    uint8_t timerId = palConfig->timerAlarmId;

    // Place the timer in the reset hold state.
    gmosDriverTimerReset (timer, true);

    // Enable the interrupt callbacks.
    hardware_alarm_set_callback (timerId, hardwareAlarmCallback);
    return true;
}

/*
 * Disables a timer and associated interrupt for subsequent use. This
 * allows the timer counter to be placed in a low power state.
 */
bool gmosDriverTimerDisable (gmosDriverTimer_t* timer)
{
    const gmosPalTimerConfig_t* palConfig = timer->palConfig;
    uint8_t timerId = palConfig->timerAlarmId;

    // Place the timer in the reset hold state.
    gmosDriverTimerReset (timer, true);

    // Disable the interrupt callbacks.
    hardware_alarm_set_callback (timerId, NULL);
    return true;
}

/*
 * Resets the current value of the timer counter to zero. The timer must
 * be enabled prior to performing a timer reset.
 */
bool gmosDriverTimerReset (gmosDriverTimer_t* timer, bool resetHold)
{
    gmosPalTimerState_t* palData = timer->palData;
    bool resetOk = true;

    // Place the timer in the reset hold state.
    if (resetHold) {
        timer->activeState = GMOS_DRIVER_TIMER_STATE_RESET;
    }

    // Restart the timer if required.
    if (timer->activeState == GMOS_DRIVER_TIMER_STATE_ONE_SHOT) {
        resetOk = gmosDriverTimerRunOneShot (timer, palData->timerPeriod);
    } else if (timer->activeState == GMOS_DRIVER_TIMER_STATE_CONTINUOUS) {
        resetOk = gmosDriverTimerRunRepeating (timer, palData->timerPeriod);
    }
    return resetOk;
}

/*
 * Accesses the current timer counter value.
 */
uint16_t gmosDriverTimerGetValue (gmosDriverTimer_t* timer)
{
    gmosPalTimerState_t* palData = timer->palData;
    uint64_t currentTime;
    uint32_t elapsedTime;
    uint32_t elapsedTicks;

    // The reset hold state always returns zero.
    if (timer->activeState == GMOS_DRIVER_TIMER_STATE_RESET) {
        return 0;
    }

    // The timer value is calculated as the number of tick periods that
    // have elapsed since the last timestamp.
    currentTime = time_us_64 ();
    if (currentTime < palData->timestamp) {
        elapsedTime = 0;
    } else {
        elapsedTime = (uint32_t) (currentTime - palData->timestamp);
    }
    elapsedTicks = elapsedTime / palData->tickPeriod;
    return (uint16_t) elapsedTicks;
}

/*
 * Implement common timer setup process.
 */
static bool gmosDriverTimerRunCommon (
    gmosDriverTimer_t* timer, uint16_t alarm)
{
    const gmosPalTimerConfig_t* palConfig = timer->palConfig;
    gmosPalTimerState_t* palData = timer->palData;
    uint8_t timerId = palConfig->timerAlarmId;
    uint64_t currentTime;
    uint32_t alarmDelay;
    absolute_time_t alarmTime;
    bool alarmMissed;

    // Set the timestamp for the start of the timer period.
    currentTime = time_us_64 ();
    palData->timestamp = currentTime;

    // Set the hardware timer alarm time. To model the hardware timer
    // behaviour where the interrupt is raised on the timer tick
    // following a match, the final tick period needs to be added to
    // the requested alarm value.
    palData->timerPeriod = alarm;
    alarmDelay = ((uint32_t) palData->tickPeriod) * (1 + (uint32_t) alarm);
    update_us_since_boot (&alarmTime, currentTime + alarmDelay);
    alarmMissed = hardware_alarm_set_target (timerId, alarmTime);

    // Indicate potential failure to set the alarm.
    return alarmMissed ? false : true;
}

/*
 * Sets a one-shot alarm for the timer counter. This is a 16-bit value
 * which will be compared against the current timer counter value,
 * triggering a call to the interrupt service routine on the timer clock
 * tick following a match. If the timer is currently in its reset hold
 * state, it is released from reset and the counter will immediately
 * start incrementing. After triggering the interrupt, the timer will
 * always be placed in the reset hold state.
 */
bool gmosDriverTimerRunOneShot (gmosDriverTimer_t* timer, uint16_t alarm)
{
    bool timerStarted = gmosDriverTimerRunCommon (timer, alarm);
    if (timerStarted) {
        timer->activeState = GMOS_DRIVER_TIMER_STATE_ONE_SHOT;
    }
    return timerStarted;
}

/*
 * Sets a repeating alarm for the timer counter. This is a 16-bit value
 * which will be compared against the current timer counter value,
 * triggering a call to the interrupt service routine on the timer clock
 * tick following a match. If the timer is currently in its reset hold
 * state, it is released from reset and the counter will immediately
 * start incrementing. After triggering the interrupt, the timer will be
 * reset to zero and then continue counting.
 */
bool gmosDriverTimerRunRepeating (gmosDriverTimer_t* timer, uint16_t alarm)
{
    bool timerStarted = gmosDriverTimerRunCommon (timer, alarm);
    if (timerStarted) {
        timer->activeState = GMOS_DRIVER_TIMER_STATE_CONTINUOUS;
    }
    return timerStarted;
}
