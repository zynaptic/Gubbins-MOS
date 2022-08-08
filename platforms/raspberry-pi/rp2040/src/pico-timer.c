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
 * Implements the Raspberry Pi Pico RP2040 platform timer using the SDK
 * system timer functions.
 */

#include <stdint.h>

#include "hardware/timer.h"

/*
 * Read the Pico SDK timer value and convert it to the appropriate
 * GubbinsMOS tick value. To keep this fast, a power of two scaling
 * is used that can be converted into a simple shift.
 */
uint32_t gmosPalGetTimer (void)
{
    uint64_t usTime = time_us_64 ();
    return (uint32_t) (usTime / 1024);
}

/*
 * Requests that the platform abstraction layer enter idle mode for
 * the specified number of platform timer ticks. This currently returns
 * immediately, such that the scheduler performs busy waiting. This is
 * because the Pico SDK sleep API does not support early wakeup on
 * external interrupts.
 */
void gmosPalIdle (uint32_t duration)
{
    return;
}

/*
 * Requests that the platform abstraction layer wakes from idle mode.
 */
void gmosPalWake (void)
{
    return;
}
