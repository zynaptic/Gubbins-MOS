/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2020-2022 Zynaptic Limited
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
 * Implements the STM32L1XX platform timer using TIM11 running on the
 * low speed external clock.
 */

#include <stdint.h>

#include "gmos-platform.h"
#include "gmos-scheduler.h"
#include "stm32-device.h"

// Statically allocate the extended counter value.
static uint32_t interruptCount = 0;

/*
 * Initialises the platform timer hardware.
 */
void gmosPalSystemTimerInit (void)
{
    // Set the ARR and compare registers to use the full counter range.
    TIM11->ARR = 0xFFFF;
    TIM11->CCR1 = 0xFFFF;

    // Enable timer interrupts on timer wrap and compare match.
    TIM11->DIER = TIM_DIER_CC1IE | TIM_DIER_UIE;

    // Enable interrupts via NVIC.
    NVIC_EnableIRQ (TIM11_IRQn);
    NVIC_SetPriority (TIM11_IRQn, 0);

    // Start the timer in continuous count mode. The counter does not
    // correctly select the 32kHz ETR input until a counter wrap event,
    // so this is forced on startup by setting the counter register
    // to its maximum value.
    TIM11->CNT = 0xFFFF;
    TIM11->CR1 |= TIM_CR1_CEN | TIM_CR1_URS;
}

/*
 * Reads the current value of the platform timer hardware.
 */
static uint16_t gmosPalGetHardwareTimer (void)
{
    return (uint16_t) TIM11->CNT;
}

/*
 * Places the device in a deep sleep mode which will be exited via the
 * standard low power timer interrupt sequence.
 * TODO: Not currently implemented.
 */
static inline void gmosPalSystemTimerDeepSleep (void)
{
    // Call the CMSIS WFI wrapper to wait for next interrupt event.
    // SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
    // __WFI ();
    // SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
}

/*
 * Places the device in a low power mode which will be exited via the
 * standard low power timer interrupt sequence.
 * TODO: Not currently implemented.
 */
static inline void gmosPalSystemTimerPowerSave (void)
{
    // Call the CMSIS WFI wrapper to wait for next interrupt event.
    // __WFI ();
}

/*
 * Implements the interrupt handler for the platform timer.
 */
void gmosPalIsrTIM11 (void)
{
    // Check for comparison register matches. Always reverts to the
    // standard timer compare value which aliases with the auto-reload
    // interrupt.
    if ((TIM11->SR & TIM_SR_CC1IF) != 0) {
        TIM11->CCR1 = 0xFFFF;
        TIM11->SR &= ~TIM_SR_CC1IF;
    }

    // On an auto-reload interrupt, always increment the interrupt
    // counter.
    if ((TIM11->SR & TIM_SR_UIF) != 0) {
        interruptCount += 1;
        TIM11->SR &= ~TIM_SR_UIF;
    }
}

/*
 * Reads the combined hardware timer value and interrupt count value.
 * Note that this only needs to support correct operation from the task
 * execution context.
 */
uint32_t gmosPalGetTimer (void)
{
    uint32_t hwTimerValue;
    uint32_t hwTimerCheck;
    uint32_t counterValue;

    // Since there is a potential race condition when accessing the
    // hardware timer value and the interrupt counter, loop until they
    // are consistent. This is done by checking that the hardware
    // timer has the same value before and after accessing the interrupt
    // counter.
    do {
        hwTimerValue = TIM11->CNT;
        NVIC_DisableIRQ (TIM11_IRQn);
        counterValue = (interruptCount << 16) | hwTimerValue;
        NVIC_EnableIRQ (TIM11_IRQn);
        hwTimerCheck = TIM11->CNT;
    } while (hwTimerValue != hwTimerCheck);

    // Return the combined timer value.
    return counterValue;
}

/*
 * Requests that the platform abstraction layer enter idle mode for
 * the specified number of platform timer ticks.
 */
void gmosPalIdle (uint32_t duration)
{
    uint32_t hwTimerValue;
    uint32_t sleepTime;

    // TODO: Not currently implemented.
    return;

    // Ignore the idle request if the requested duration is too short.
    if (duration <= GMOS_CONFIG_STM32_STAY_AWAKE_THRESHOLD) {
        return;
    }

    // If the requested period would span a regular timer interrupt,
    // calculate the sleep time based on that.
    hwTimerValue = gmosPalGetHardwareTimer ();
    if (hwTimerValue + duration >= 0xFFFF) {
        sleepTime = 0xFFFF - hwTimerValue;
    }

    // If the requested period would preempt a regular timer interrupt,
    // update the compare register.
    else {
        TIM11->CCR1 = (hwTimerValue & 0xFFFF) + duration;
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

/*
 * Requests that the platform abstraction layer wakes from idle mode.
 */
void gmosPalWake (void)
{
}
