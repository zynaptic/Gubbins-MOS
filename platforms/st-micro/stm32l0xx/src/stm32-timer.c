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
 * Implements the STM32L0XX platform timer using the 16-bit low power
 * timer counter.
 */

#include <stdint.h>

#include "gmos-platform.h"
#include "gmos-scheduler.h"
#include "stm32-device.h"

// Statically allocate the extended counter value.
static uint16_t interruptCount = 0;

// Store the last counter value to be read back.
static int32_t lastCounterValue = 0;

/*
 * Initialises the low power hardware timer.
 */
void gmosPalSystemTimerInit (void)
{
    // Set the ARR register to use the full counter range and the
    // compare register to half the full counter range.
    LPTIM1->ARR = 0xFFFF;
    LPTIM1->CMP = 0x7FFF;

    // Enable timer interrupts on compare and wrap.
    LPTIM1->IER = LPTIM_IER_ARRMIE | LPTIM_IER_CMPMIE;

    // Enable interrupt and wake events on compare and wrap.
    EXTI->IMR |= EXTI_IMR_IM29;
    EXTI->EMR |= EXTI_EMR_EM29;

    // Enable interrupts via NVIC.
    NVIC_EnableIRQ (LPTIM1_IRQn);
    NVIC_SetPriority (LPTIM1_IRQn, 0);

    // Start the low power timer in continuous count mode.
    LPTIM1->CR |= LPTIM_CR_CNTSTRT;
}

/*
 * Reads the current value of the low power timer counter.
 */
uint16_t gmosPalGetHardwareTimer (void)
{
    uint16_t value1;
    uint16_t value2;

    // Since the timer counter is not running on the main system clock,
    // it is only valid if two consecutive reads have the same value.
    do {
        value1 = (uint16_t) LPTIM1->CNT;
        value2 = (uint16_t) LPTIM1->CNT;
    } while (value1 != value2);

    return value1;
}

/*
 * Places the device in a deep sleep mode which will be exited via the
 * standard low power timer interrupt sequence.
 */
static inline void gmosPalSystemTimerDeepSleep (void)
{
    // Call the CMSIS WFI wrapper to wait for next interrupt event.
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
    __WFI ();
    SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
}

/*
 * Places the device in a low power mode which will be exited via the
 * standard low power timer interrupt sequence.
 */
static inline void gmosPalSystemTimerPowerSave (void)
{
    // Call the CMSIS WFI wrapper to wait for next interrupt event.
    __WFI ();
}

/*
 * Implements the interrupt handler for the low power timer.
 */
void gmosPalIsrLPTIM1 (void)
{
    // Check for comparison register matches. Always reverts to the
    // standard timer compare value which aliases with the auto-reload
    // interrupt.
    if ((LPTIM1->ISR & LPTIM_ISR_CMPM) != 0) {
        LPTIM1->CMP = 0xFFFF;
        LPTIM1->ICR = LPTIM_ICR_CMPMCF;
    }

    // On an auto-reload interrupt, always increment the interrupt
    // counter.
    if ((LPTIM1->ISR & LPTIM_ISR_ARRM) != 0) {
        interruptCount += 1;
        LPTIM1->ICR = LPTIM_ICR_ARRMCF;
    }
}

/*
 * Reads the combined hardware timer value and interrupt count value.
 * Note that this only needs to support correct operation from the task
 * execution context.
 */
uint32_t gmosPalGetTimer (void)
{
    uint16_t lpTimerValue;
    int32_t counterValue;

    // Since there is a potential race condition when accessing the
    // hardware timer value and the interrupt counter, loop until they
    // are consistent. The race condition is detected by the timer
    // counter value going 'backward'. The wrapped increment by 1 on the
    // hardware timer compensates for the fact that the timer interrupts
    // occur one tick earlier than an expected 'carry out'.
    do {
        NVIC_DisableIRQ (LPTIM1_IRQn);
        lpTimerValue = 1 + gmosPalGetHardwareTimer ();
        counterValue = (((int32_t) (interruptCount)) << 16) |
            ((int32_t) lpTimerValue);
        NVIC_EnableIRQ (LPTIM1_IRQn);
    } while ((counterValue - lastCounterValue) < 0);

    // Return the combined timer value.
    lastCounterValue = counterValue;
    return (uint32_t) counterValue;
}

/*
 * Requests that the platform abstraction layer enter idle mode for
 * the specified number of platform timer ticks.
 */
void gmosPalIdle (uint32_t duration)
{
    uint32_t lpTimerValue;
    uint32_t sleepTime;

    // Ignore the idle request if the requested duration is too short.
    if (duration <= GMOS_CONFIG_STM32_STAY_AWAKE_THRESHOLD) {
        return;
    }

    // If the requested period would span a regular timer interrupt,
    // calculate the sleep time based on that.
    lpTimerValue = gmosPalGetHardwareTimer ();
    if (lpTimerValue + duration >= 0xFFFF) {
        sleepTime = 0xFFFF - lpTimerValue;
    }

    // If the requested period would preempt a regular timer interrupt,
    // update the compare register.
    else {
        LPTIM1->CMP = (lpTimerValue & 0xFFFF) + duration;
        sleepTime = duration;
    }

    // Use deep sleep for long durations.
    if (sleepTime > GMOS_CONFIG_STM32_DEEP_SLEEP_THRESHOLD) {
        if (gmosLifecycleNotify (SCHEDULER_ENTER_DEEP_SLEEP)) {
            gmosPalSystemTimerDeepSleep ();
        }
        gmosLifecycleNotify (SCHEDULER_EXIT_DEEP_SLEEP);
    }

    // Use power save for short durations.
    else if (sleepTime > GMOS_CONFIG_STM32_STAY_AWAKE_THRESHOLD) {
        if (gmosLifecycleNotify (SCHEDULER_ENTER_POWER_SAVE)) {
            gmosPalSystemTimerPowerSave ();
        }
        gmosLifecycleNotify (SCHEDULER_EXIT_POWER_SAVE);
    }
}
