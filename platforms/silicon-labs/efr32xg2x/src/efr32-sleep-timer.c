/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2023-2025 Zynaptic Limited
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
 * Implements the Silicon Labs EFR32xG2x platform timer using the Gecko
 * SDK sleep timer API.
 */

#include <stdint.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "em_core.h"
#include "sl_sleeptimer.h"
#include "sl_power_manager.h"

/**
 * Enable additional hash table debug tracing.
 */
#ifndef GMOS_CONFIG_EFR32_SLEEP_TIMER_DEBUG_TRACE
#define GMOS_CONFIG_EFR32_SLEEP_TIMER_DEBUG_TRACE false
#endif

// Store the last high bit read back from the fast sleep timer.
static uint8_t lastTickCountHighBit;

// Hold the high order bits of the slow GubbinsMOS system timer.
static uint8_t timerHighOrderBits;

// Store the sleep timer state.
static sl_sleeptimer_timer_handle_t sleepTimerHandle;

// Store the power notification callback state.
static sl_power_manager_em_transition_event_handle_t powerEventHandle;

// A boolean flag which indicates whether the application can sleep.
static volatile bool applicationCanSleep = false;

/*
 * Add power management events to the debug trace.
 */
static void gmosPalSystemPowerLogging (
    sl_power_manager_em_t from, sl_power_manager_em_t to)
{
    GMOS_LOG_FMT (LOG_VERBOSE,
        "Sleep Timer: Transition from 0x%02X to 0x%02X at tick %d.",
        from, to, gmosPalGetTimer ());
}

/*
 * Add sleep timer callbacks to the debug trace.
 */
static void gmosPalSystemSleepLogging (
    sl_sleeptimer_timer_handle_t *handle, void *data)
{
    (void) handle;
    (void) data;

    if (GMOS_CONFIG_EFR32_SLEEP_TIMER_DEBUG_TRACE) {
        GMOS_LOG_FMT (LOG_VERBOSE,
            "Sleep Timer: Expired at tick %d.", gmosPalGetTimer ());
    }
}

/*
 * Define the power management callback event.
 */
static const sl_power_manager_em_transition_event_info_t powerEventInfo = {
    .event_mask =
        SL_POWER_MANAGER_EVENT_TRANSITION_ENTERING_EM0 |
        SL_POWER_MANAGER_EVENT_TRANSITION_LEAVING_EM0 |
        SL_POWER_MANAGER_EVENT_TRANSITION_ENTERING_EM1 |
        SL_POWER_MANAGER_EVENT_TRANSITION_LEAVING_EM1 |
        SL_POWER_MANAGER_EVENT_TRANSITION_ENTERING_EM2 |
        SL_POWER_MANAGER_EVENT_TRANSITION_LEAVING_EM2,
    .on_event = gmosPalSystemPowerLogging};

/*
 * Initialises the low power sleep timer.
 */
void gmosPalSystemTimerInit (void)
{
    // Initialise the power management service. This also ensures that
    // the sleep timer service is initialised.
    sl_power_manager_init ();

    // RAM memory must be maintained during sleep, so full EM4 shutoff
    // mode will never be used.
    sl_power_manager_add_em_requirement (SL_POWER_MANAGER_EM3);

    // Initialise the sleep timer ready for use.
    lastTickCountHighBit =
        (uint8_t) ((sl_sleeptimer_get_tick_count ()) >> 31);
    timerHighOrderBits = 0;

    // Add power management callback handler for debugging.
    if (GMOS_CONFIG_EFR32_SLEEP_TIMER_DEBUG_TRACE) {
        sl_power_manager_subscribe_em_transition_event (
            &powerEventHandle, &powerEventInfo);
    }
}

/*
 * Read the the 32-bit sleep timer counter with scaling.
 */
uint32_t gmosPalGetTimer (void)
{
    uint32_t timerValue;
    uint32_t tickCount;
    uint8_t tickCountHighBit;

    // Implement atomic updates on fast counter wrap.
    CORE_DECLARE_IRQ_STATE;
    CORE_ENTER_CRITICAL ();
    tickCount = sl_sleeptimer_get_tick_count ();
    tickCountHighBit = (uint8_t) (tickCount >> 31);

    // Increment the high order bit counter on tick counter wrap.
    if ((lastTickCountHighBit != 0) && (tickCountHighBit == 0)) {
        timerHighOrderBits += 1;
    }
    lastTickCountHighBit = tickCountHighBit;

    // Divide the 32.768 kHz tick counter by 32 to give the expected
    // GubbinsMOS system timer frequency.
    timerValue = timerHighOrderBits;
    timerValue = (timerValue << 27) | (tickCount >> 5);
    CORE_EXIT_CRITICAL ();

    return timerValue;
}

/*
 * Enter a low power idle state for the specified duration.
 */
void gmosPalIdle (uint32_t duration)
{
    sl_status_t slStatus;

    // In order to ensure correct behaviour for the hardware timer
    // overflow into the high order bits, any sleep duration needs to
    // be restricted to less than half the period of the hardware timer.
    // Therefore a maximum sleep duration of 6 hours is imposed here.
    // A minimum duration is also imposed in order to prevent excessive
    // power cycling.
    uint32_t maxDuration = GMOS_MS_TO_TICKS (6 * 60 * 60 * 1000);
    uint32_t minDuration = GMOS_MS_TO_TICKS (250);
    if (duration > maxDuration) {
        duration = maxDuration;
    } else if (duration < minDuration) {
        duration = 0;
    }

    // Attempt to sleep using the Silicon Labs sleep timer, which has a
    // tick frequency 32 times the GubbinsMOS tick frequency.
    if (duration > 0) {
        uint32_t systemTicks = duration * 32;

        // Set the sleep timer to the specified duration.
        slStatus = sl_sleeptimer_start_timer (&sleepTimerHandle,
            systemTicks, gmosPalSystemSleepLogging, NULL, 0, 0);
        if (GMOS_CONFIG_EFR32_SLEEP_TIMER_DEBUG_TRACE) {
            GMOS_LOG_FMT (LOG_VERBOSE,
                "Sleep Timer: Started with status 0x%04X at tick %d.",
                slStatus, gmosPalGetTimer ());
        }

        // Power down the device. It will sleep until the sleep timer
        // expires or there is an external wakeup request.
        applicationCanSleep = true;
        sl_power_manager_sleep ();
        applicationCanSleep = false;

        // Cancel the sleep timer if another event was responsible for
        // waking the device.
        slStatus = sl_sleeptimer_stop_timer (&sleepTimerHandle);
        if (GMOS_CONFIG_EFR32_SLEEP_TIMER_DEBUG_TRACE) {
            GMOS_LOG_FMT (LOG_VERBOSE,
                "Sleep Timer: Stopped with status 0x%04X at tick %d.",
                slStatus, gmosPalGetTimer ());
        }
    }
    return;
}

/*
 * Wake from a low power idle state under external control. Not required
 * for this platform.
 */
void gmosPalWake (void)
{
    return;
}

/*
 * Implement callback from the sleep management service to indicate that
 * the application is ready to sleep.
 */
bool app_is_ok_to_sleep (void)
{
    return applicationCanSleep;
}

/*
 * Implement callback from the sleep management service to indicate that
 * the processor should remain awake after a interrupt event.
 */
sl_power_manager_on_isr_exit_t app_sleep_on_isr_exit (void)
{
    return SL_POWER_MANAGER_WAKEUP;
}
