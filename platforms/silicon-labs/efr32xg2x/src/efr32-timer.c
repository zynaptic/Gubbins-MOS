/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2023 Zynaptic Limited
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

#include "gmos-config.h"
#include "gmos-platform.h"
#include "em_core.h"
#include "sl_sleeptimer.h"

// Store the last high bit read back from the fast sleep timer.
static uint8_t lastTickCountHighBit;

// Hold the high order bits of the slow GubbinsMOS system timer.
static uint8_t timerHighOrderBits;

/*
 * Initialises the low power sleep timer.
 */
void gmosPalSystemTimerInit (void)
{
    sl_sleeptimer_init ();
    lastTickCountHighBit =
        (uint8_t) ((sl_sleeptimer_get_tick_count ()) >> 31);
    timerHighOrderBits = 0;
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
    // In order to ensure correct behaviour for the hardware timer
    // overflow into the high order bits, any sleep duration needs to
    // be restricted to less than half the period of the hardware timer.
    // Therefore a maximum sleep duration of 6 hours is imposed here.
    uint32_t maxDuration = GMOS_MS_TO_TICKS (6 * 60 * 60 * 1000);
    if (duration > maxDuration) {
        duration = maxDuration;
    }

    // TODO: Sleep on idle is not currently implemented.

    return;
}

/*
 * Wake from a low power idle state under external control. Not
 * currently required.
 */
void gmosPalWake (void)
{
    return;
}
