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
 * Microchip/Atmel ATMEGA series of microcontrollers.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-scheduler.h"
#include "gmos-driver-timer.h"
#include "atmega-device.h"
#include "atmega-driver-timer.h"

// Provide reverse mapping of timer IDs to timer state data structures.
static gmosDriverTimer_t* timerDataMap [] = {
    NULL, NULL};

/*
 * Sets the timer clock frequency by configuring the clock prescaler.
 */
static inline uint32_t atmegaDriverTimerSetClock
    (gmosDriverTimer_t* timer, uint32_t frequency)
{
    gmosPalTimerState_t* timerState = timer->palData;
    const gmosPalTimerConfig_t* timerConfig = timer->palConfig;
    uint8_t timerIndex = timerConfig->timerId;
    uint8_t clockSelect;
    uint32_t frequencySelect;

    // Derive the preferred prescaler for the requested frequency. If an
    // approximate frequency is used, it will be lower than requested,
    // unless it is less than the minimum clock frequency.
    if (frequency >= GMOS_CONFIG_ATMEGA_SYSTEM_CLOCK) {
        frequencySelect = GMOS_CONFIG_ATMEGA_SYSTEM_CLOCK;
        clockSelect = 1;
    } else if (frequency >= GMOS_CONFIG_ATMEGA_SYSTEM_CLOCK / 8) {
        frequencySelect = GMOS_CONFIG_ATMEGA_SYSTEM_CLOCK / 8;
        clockSelect = 2;
    } else if (frequency >= GMOS_CONFIG_ATMEGA_SYSTEM_CLOCK / 64) {
        frequencySelect = GMOS_CONFIG_ATMEGA_SYSTEM_CLOCK / 64;
        clockSelect = 3;
    } else if (frequency >= GMOS_CONFIG_ATMEGA_SYSTEM_CLOCK / 256) {
        frequencySelect = GMOS_CONFIG_ATMEGA_SYSTEM_CLOCK / 256;
        clockSelect = 4;
    } else {
        frequencySelect = GMOS_CONFIG_ATMEGA_SYSTEM_CLOCK / 1024;
        clockSelect = 5;
    }
    if (frequency != frequencySelect) {
        GMOS_LOG (LOG_WARNING,
            "Timer %d clock requested %ldHz, actual %ldHz.",
            timerIndex, frequency, frequencySelect);
    }

    // Sets the timer frequency being used and enable CTC mode
    // operation, but disable the timer clock until it is subsequently
    // enabled.
    timer->frequency = frequencySelect;
    timerState->clockSelect = clockSelect;
    if (timerIndex == 0) {
        ATMEGA_TIMER0_TCFG_REG |= (1 << ATMEGA_TIMER0_CTC_BIT);
        timer->maxValue = 0xFF;
    } else {
        ATMEGA_TIMER1_TCFG_REG |= (1 << ATMEGA_TIMER1_CTC_BIT);
        timer->maxValue = 0xFFFF;
    }
    return true;
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
    uint8_t timerIndex = timerConfig->timerId;

    // Check for a valid timer selection.
    if ((timerIndex >= 2) || (timerDataMap [timerIndex] != NULL)) {
        return false;
    }

    // Configure the timer clocks.
    if (!atmegaDriverTimerSetClock (timer, frequency)) {
        return false;
    }

    // Set the OCR register to full range. This prevents spurious
    // interrupts when the counter value is zero.
    if (timerIndex == 0) {
        ATMEGA_TIMER0_MATCH_REG = 0xFF;
    } else {
        ATMEGA_TIMER1_MATCH_REG_H = 0xFF;
        ATMEGA_TIMER1_MATCH_REG_L = 0xFF;
    }

    // Register the timer and timer interrupt.
    timerDataMap [timerIndex] = timer;
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
    // Place the timer in the reset hold state and enable interrupts.
    gmosSchedulerStayAwake ();
    return gmosDriverTimerReset (timer, true) &&
        gmosDriverTimerIsrMask (timer, false);
}

/*
 * Disables a timer and associated interrupt for subsequent use. This
 * allows the timer counter to be placed in a low power state.
 */
bool gmosDriverTimerDisable (gmosDriverTimer_t* timer)
{
    // Stop the timer counter and disable interrupts.
    gmosSchedulerCanSleep ();
    return gmosDriverTimerReset (timer, true) &&
        gmosDriverTimerIsrMask (timer, true);
}

/*
 * Masks the timer interrupts, controlling when the timer interrupt
 * service routine will be allowed to run.
 */
bool gmosDriverTimerIsrMask (gmosDriverTimer_t* timer, bool isrMask)
{
    const gmosPalTimerConfig_t* timerConfig = timer->palConfig;
    uint8_t timerIndex = timerConfig->timerId;
    uint8_t regValue;

    // Check for a valid timer selection.
    if ((timerIndex >= 2) || (timerDataMap [timerIndex] != timer)) {
        return false;
    }

    // Set the interrupt mask flag for timer 0.
    if (timerIndex == 0) {
        regValue = ATMEGA_TIMER0_INT_MASK_REG;
        if (isrMask) {
            regValue &= ~(1 << ATMEGA_TIMER0_INT_MASK_BIT);
        } else {
            regValue |= (1 << ATMEGA_TIMER0_INT_MASK_BIT);
        }
        ATMEGA_TIMER0_INT_MASK_REG = regValue;
    }

    // Set the interrupt mask flag for timer 1.
    else {
        regValue = ATMEGA_TIMER1_INT_MASK_REG;
        if (isrMask) {
            regValue &= ~(1 << ATMEGA_TIMER1_INT_MASK_BIT);
        } else {
            regValue |= (1 << ATMEGA_TIMER1_INT_MASK_BIT);
        }
        ATMEGA_TIMER1_INT_MASK_REG = regValue;
    }
    return true;
}

/*
 * Resets the current value of the timer counter to zero. The timer must
 * be enabled prior to performing a timer reset.
 */
bool gmosDriverTimerReset (gmosDriverTimer_t* timer, bool resetHold)
{
    const gmosPalTimerConfig_t* timerConfig = timer->palConfig;
    uint8_t timerIndex = timerConfig->timerId;

    // Check for a valid timer selection.
    if ((timerIndex >= 2) || (timerDataMap [timerIndex] != timer)) {
        return false;
    }

    // Disable the timer source clock and reset timer counter 0.
    if (timerIndex == 0) {
        if (resetHold) {
            ATMEGA_TIMER0_TCLK_REG &= ~(7 << CS00);
        }
        TCNT0 = 0x00;
    }

    // Disable the timer source clock and reset timer counter 1.
    else {
        if (resetHold) {
            ATMEGA_TIMER1_TCLK_REG &= ~(7 << CS10);
        }
        TCNT1H = 0x00;
        TCNT1L = 0x00;
    }
    if (resetHold) {
        timer->activeState = GMOS_DRIVER_TIMER_STATE_RESET;
    }
    return true;
}

/*
 * Accesses the current timer counter value.
 */
uint16_t gmosDriverTimerGetValue (gmosDriverTimer_t* timer)
{
    const gmosPalTimerConfig_t* timerConfig = timer->palConfig;
    uint8_t timerIndex = timerConfig->timerId;
    uint16_t result;

    // Check for a valid timer selection.
    if ((timerIndex >= 2) || (timerDataMap [timerIndex] != timer)) {
        return 0;
    }

    // Read 8 bit timer counter 0.
    if (timerIndex == 0) {
        result = (uint16_t) TCNT0;
    }

    // Read 16 bit timer counter 1. Reading 16 bit registers should
    // be done low byte first in order to latch the high byte.
    else {
        result = (uint16_t) TCNT1L;
        result |= ((uint16_t) TCNT1H) << 8;
    }
    return result;
}

/*
 * Implements common setup for timer run requests.
 */
static bool atmegaDriverTimerRun (gmosDriverTimer_t* timer,
    uint16_t alarm, bool runOneShot)
{
    gmosPalTimerState_t* timerState = timer->palData;
    const gmosPalTimerConfig_t* timerConfig = timer->palConfig;
    uint8_t timerIndex = timerConfig->timerId;

    // Check for a valid timer selection and alarm value.
    if ((timerIndex >= 2) || (timerDataMap [timerIndex] != timer) ||
        (alarm > timer->maxValue) || (alarm == 0)) {
        return false;
    }

    // Ensure timer interrupts are disabled before making changes.
    if (timerIndex == 0) {
        ATMEGA_TIMER0_INT_MASK_REG &= ~(1 << ATMEGA_TIMER0_INT_MASK_BIT);
    } else {
        ATMEGA_TIMER1_INT_MASK_REG &= ~(1 << ATMEGA_TIMER1_INT_MASK_BIT);
    }

    // Set the new timer state.
    if (runOneShot) {
        timer->activeState = GMOS_DRIVER_TIMER_STATE_ONE_SHOT;
    } else {
        timer->activeState = GMOS_DRIVER_TIMER_STATE_CONTINUOUS;
    }

    // Set the compare match register value and enable the timer clock.
    if (timerIndex == 0) {
        ATMEGA_TIMER0_MATCH_REG = (uint8_t) alarm;
        ATMEGA_TIMER0_TCLK_REG |= timerState->clockSelect << CS00;
    } else {
        ATMEGA_TIMER1_MATCH_REG_H = (uint8_t) (alarm >> 8);
        ATMEGA_TIMER1_MATCH_REG_L = (uint8_t) alarm;
        ATMEGA_TIMER1_TCLK_REG |= timerState->clockSelect << CS10;
    }

    // Enable timer interrupts on exit.
    if (timerIndex == 0) {
        ATMEGA_TIMER0_INT_MASK_REG |= (1 << ATMEGA_TIMER0_INT_MASK_BIT);
    } else {
        ATMEGA_TIMER1_INT_MASK_REG |= (1 << ATMEGA_TIMER1_INT_MASK_BIT);
    }
    return true;
}

/*
 * Implement common interrupt handling for ATMEGA timers.
 */
static void atmegaDriverTimerIsr (gmosDriverTimer_t* timer)
{
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
 * Implements ISR for timer 0.
 */
ISR (ATMEGA_TIMER0_INT_VECT)
{
    gmosDriverTimer_t* timer = timerDataMap [0];
    if (timer != NULL) {
        atmegaDriverTimerIsr (timer);
    }
}

/*
 * Implements ISR for timer 1.
 */
ISR (ATMEGA_TIMER1_INT_VECT)
{
    gmosDriverTimer_t* timer = timerDataMap [1];
    if (timer != NULL) {
        atmegaDriverTimerIsr (timer);
    }
}

/*
 * Sets a one-shot alarm for the timer counter.
 */
bool gmosDriverTimerRunOneShot (gmosDriverTimer_t* timer, uint16_t alarm)
{
    return atmegaDriverTimerRun (timer, alarm, true);
}

/*
 * Sets a repeating alarm for the timer counter.
 */
bool gmosDriverTimerRunRepeating (gmosDriverTimer_t* timer, uint16_t alarm)
{
    return atmegaDriverTimerRun (timer, alarm, false);
}
