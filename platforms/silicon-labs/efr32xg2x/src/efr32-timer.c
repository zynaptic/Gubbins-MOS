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
#include "sl_sleeptimer.h"

/*
 * Initialises the low power sleep timer.
 */
void gmosPalSystemTimerInit (void)
{
    sl_sleeptimer_init ();
}

/*
 * Directly reads the 32-bit sleep timer counter.
 */
uint32_t gmosPalGetTimer (void)
{
    return sl_sleeptimer_get_tick_count ();
}

/*
 * Enter a low power idle state for the specified duration. Not
 * currently implemented.
 */
void gmosPalIdle (uint32_t duration)
{
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
