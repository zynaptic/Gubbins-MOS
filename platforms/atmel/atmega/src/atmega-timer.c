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
 * Implements the Microchip/Atmel ATMEGA platform timer using the 8-bit
 * low power timer counter.
 */

#include <stdint.h>

#include "gmos-platform.h"
#include "atmega-device.h"

// Statically allocate the extended counter value.
static uint32_t interruptCount = 0;

/*
 * Implement the low power hardware timer overflow interrupt. This just
 * increments the interrupt counter.
 */
ISR (TIMER2_OVF_vect)
{
    interruptCount += 1;
}

/*
 * Initialises the low power hardware timer.
 */
void gmosPalSystemTimerInit (void)
{
    uint8_t prescale;

    // Select timer clock source and prescaler value.
    if (GMOS_CONFIG_ATMEGA_USE_LSE_OSC) {
        ASSR = (1 << AS2);
        prescale = 3;
    } else {
        ASSR = 0;
        prescale = 7;
    }

    // Set up the timer and enable interrupts.
    ATMEGA_TIMER_TCCR_REG = (prescale << CS20);
    ATMEGA_TIMER_TIMSK_REG |= (1 << TOIE2);
}

/*
 * Reads the combined hardware timer value and interrupt count value.
 * Note that this only needs to support correct operation from the task
 * execution context.
 */
uint32_t gmosPalGetTimer (void)
{
    uint8_t  lpTimerValue;
    uint8_t  lpTimerCheck;
    uint32_t counterValue;

    // Since there is a potential race condition when accessing the
    // hardware timer value and the interrupt counter, loop until they
    // are consistent. This is done by checking that the hardware
    // timer has the same value before and after accessing the interrupt
    // counter. This test also checks for inconsistent reads on the
    // hardware timer due to accessing it over a clock boundary.
    do {
        lpTimerValue = TCNT2;
        ATMEGA_TIMER_TIMSK_REG &= ~(1 << TOIE2);
        if (GMOS_CONFIG_ATMEGA_USE_LSE_OSC) {
            counterValue = (interruptCount << 8) | lpTimerValue;
        } else {
            counterValue = interruptCount <<
                (8 - GMOS_CONFIG_ATMEGA_SYSTEM_TIMER_POSTSCALE);
            counterValue |= lpTimerValue >>
                GMOS_CONFIG_ATMEGA_SYSTEM_TIMER_POSTSCALE;
        }
        ATMEGA_TIMER_TIMSK_REG |= (1 << TOIE2);
        lpTimerCheck = TCNT2;
    } while (lpTimerValue != lpTimerCheck);

    // Return the combined timer value.
    return counterValue;
}

/*
 * Requests that the platform abstraction layer enter idle mode for
 * the specified number of platform timer ticks.
 */
void gmosPalIdle (uint32_t duration)
{
}

/*
 * Requests that the platform abstraction layer wakes from idle mode.
 */
void gmosPalWake (void)
{
}
