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
 * Implements general purpose timer functionality for the Silicon Labs
 * EFR32XG2X series of microcontrollers.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-driver-timer.h"
#include "efr32-driver-timer.h"
#include "em_device.h"
#include "em_core.h"
#include "em_cmu.h"

// Provide mapping of timer IDs to register sets.
static TIMER_TypeDef* timerRegisterMap [] = {
    TIMER0, TIMER1, TIMER2, TIMER3, TIMER4};

// Provide mapping of timer IDs to NVIC IRQ numbers.
static const IRQn_Type timerNvicIrqMap [] = {
    TIMER0_IRQn, TIMER1_IRQn, TIMER2_IRQn, TIMER3_IRQn, TIMER4_IRQn};

// Provide mapping of timer IDs to peripheral clock configurations.
static CMU_Clock_TypeDef timerClockConfigs [] = {
    cmuClock_TIMER0, cmuClock_TIMER1, cmuClock_TIMER2, cmuClock_TIMER3,
    cmuClock_TIMER4};

// Provide reverse mapping of timer IDs to timer state data structures.
static gmosDriverTimer_t* timerDataMap [] = {
    NULL, NULL, NULL, NULL, NULL};

/*
 * Implement common ISR handling.
 */
static void efr32DriverTimerCommonIsr (uint_fast8_t timerIndex)
{
    gmosDriverTimer_t* timer = timerDataMap [timerIndex];
    TIMER_TypeDef* timerRegs = timerRegisterMap [timerIndex];

    // Place the timer in the reset hold state if this is a one-shot
    // interrupt.
    if (timer != NULL) {
        if (timer->activeState == GMOS_DRIVER_TIMER_STATE_ONE_SHOT) {
            gmosDriverTimerReset (timer, true);
        }

        // Call user ISR if it is configured.
        if (timer->timerIsr != NULL) {
            timer->timerIsr (timer->timerIsrData);
        }
    }

    // Clear the interrupt status register.
    timerRegs->IF_CLR = TIMER_IF_OF;
}

/*
 * Implement ISR for timer 0.
 */
#ifdef TIMER0
void TIMER0_IRQHandler (void) {
    efr32DriverTimerCommonIsr (0);
}
#endif

/*
 * Implement ISR for timer 1.
 */
#ifdef TIMER1
void TIMER1_IRQHandler (void) {
    efr32DriverTimerCommonIsr (1);
}
#endif

/*
 * Implement ISR for timer 2.
 */
#ifdef TIMER2
void TIMER2_IRQHandler (void) {
    efr32DriverTimerCommonIsr (2);
}
#endif

/*
 * Implement ISR for timer 3.
 */
#ifdef TIMER3
void TIMER3_IRQHandler (void) {
    efr32DriverTimerCommonIsr (3);
}
#endif

/*
 * Implement ISR for timer 4.
 */
#ifdef TIMER4
void TIMER4_IRQHandler (void) {
    efr32DriverTimerCommonIsr (4);
}
#endif

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
    uint_fast8_t timerIndex;
    uint32_t clockFrequency;
    uint32_t prescaler;
    uint32_t scaledFrequency;
    TIMER_TypeDef* timerRegs;
    CMU_Clock_TypeDef clockConfig;

    // Check for a valid timer selection.
    timerIndex = timerConfig->timerId;
    if ((timerIndex > 4) || (timerRegisterMap [timerIndex] == NULL) ||
        (timerDataMap [timerIndex] != NULL)) {
        return false;
    }
    timerRegs = timerRegisterMap [timerIndex];

    // Enable the bus clock for the timer peripheral.
    clockConfig = timerClockConfigs [timerIndex];
    CMU_ClockEnable (clockConfig, true);
    clockFrequency = CMU_ClockFreqGet (clockConfig);
    GMOS_LOG_FMT (LOG_DEBUG,
        "Timer %d detected source clock frequency %ldHz.",
        timerConfig->timerId, clockFrequency);

    // Derive the clock prescaler.
    prescaler = clockFrequency / frequency;
    if ((frequency * prescaler) != clockFrequency) {
        prescaler += 1;
        scaledFrequency = clockFrequency / prescaler;
        GMOS_LOG_FMT (LOG_WARNING,
            "Timer %d clock requested %ldHz, actual %ldHz.",
            timerConfig->timerId, frequency, scaledFrequency);
    } else {
        scaledFrequency = frequency;
    }

    // Check that the prescaler is within the EFR32 10 bit counter
    // range.
    if (prescaler > 0x400) {
        GMOS_LOG_FMT (LOG_ERROR,
            "Timer %d clock prescaler out of range (divides by %ld).",
            timerConfig->timerId, prescaler);
        return false;
    }

    // Set the timer configuration register. The timer is used as a
    // simple up counter. Note that the value to be programmed into the
    // prescaler register subtracts 1 from the actual prescale value.
    timerRegs->CFG = TIMER_CFG_MODE_UP |
        ((prescaler - 1) << _TIMER_CFG_PRESC_SHIFT);

    // Populate the local timer data structures.
    timerDataMap [timerIndex] = timer;
    timer->timerIsr = timerIsr;
    timer->timerIsrData = timerIsrData;
    timer->frequency = scaledFrequency;
    timer->maxValue = (1 << TIMER_CNTWIDTH (timerIndex)) - 1;
    timer->activeState = GMOS_DRIVER_TIMER_STATE_RESET;
    return true;
}

/*
 * Enables a timer and associated interrupt for subsequent use.
 */
bool gmosDriverTimerEnable (gmosDriverTimer_t* timer)
{
    const gmosPalTimerConfig_t* timerConfig = timer->palConfig;
    TIMER_TypeDef* timerRegs = timerRegisterMap [timerConfig->timerId];
    IRQn_Type timerNvicIrq = timerNvicIrqMap [timerConfig->timerId];

    // Enable the timer hardware internal logic.
    timerRegs->EN_SET = TIMER_EN_EN;

    // Place the timer in the reset hold state.
    gmosDriverTimerReset (timer, true);

    // Enable the associated NVIC interrupt.
    NVIC_ClearPendingIRQ(timerNvicIrq);
    NVIC_EnableIRQ(timerNvicIrq);

    return true;
}

/*
 * Disables a timer and associated interrupt for subsequent use. This
 * allows the timer counter to be placed in a low power state.
 */
bool gmosDriverTimerDisable (gmosDriverTimer_t* timer)
{
    const gmosPalTimerConfig_t* timerConfig = timer->palConfig;
    TIMER_TypeDef* timerRegs = timerRegisterMap [timerConfig->timerId];
    IRQn_Type timerNvicIrq = timerNvicIrqMap [timerConfig->timerId];
    uint32_t enRegValue;

    // Place the timer in the reset hold state.
    gmosDriverTimerReset (timer, true);

    // Disable the associated NVIC interrupt.
    NVIC_DisableIRQ(timerNvicIrq);

    // Disable the timer hardware internal logic, polling until shutdown
    // is complete.
    timerRegs->EN_CLR = TIMER_EN_EN;
    do {
        enRegValue = timerRegs->EN;
    } while ((enRegValue & TIMER_EN_DISABLING) != 0);

    return true;
}

/*
 * Resets the current value of the timer counter to zero. The timer must
 * be enabled prior to performing a timer reset.
 */
bool gmosDriverTimerReset (gmosDriverTimer_t* timer, bool resetHold)
{
    const gmosPalTimerConfig_t* timerConfig = timer->palConfig;
    TIMER_TypeDef* timerRegs = timerRegisterMap [timerConfig->timerId];

    // Place the timer in the reset hold state.
    timerRegs->IEN_CLR = TIMER_IEN_OF;
    timerRegs->CMD_SET = TIMER_CMD_STOP;
    timerRegs->CNT = 0;
    timerRegs->TOP = timer->maxValue;

    // Release from reset if required.
    if (resetHold) {
        timer->activeState = GMOS_DRIVER_TIMER_STATE_RESET;
    } else {
        timer->activeState = GMOS_DRIVER_TIMER_STATE_FREE_RUNNING;
        timerRegs->CMD_SET = TIMER_CMD_START;
    }
    return true;
}

/*
 * Accesses the current timer counter value.
 */
uint32_t gmosDriverTimerGetValue (gmosDriverTimer_t* timer)
{
    const gmosPalTimerConfig_t* timerConfig = timer->palConfig;
    TIMER_TypeDef* timerRegs = timerRegisterMap [timerConfig->timerId];

    return timerRegs->CNT;
}

/*
 * Sets a one-shot alarm for the timer counter.
 */
bool gmosDriverTimerRunOneShot (
    gmosDriverTimer_t* timer, uint32_t alarm)
{
    const gmosPalTimerConfig_t* timerConfig = timer->palConfig;
    TIMER_TypeDef* timerRegs = timerRegisterMap [timerConfig->timerId];

    // Place the timer in one-shot mode.
    timer->activeState = GMOS_DRIVER_TIMER_STATE_ONE_SHOT;

    // Set the counter top register to the alarm value.
    timerRegs->TOP = alarm;

    // Enable the counter with overflow interrupts.
    timerRegs->IF_CLR = TIMER_IF_OF;
    timerRegs->IEN_SET = TIMER_IEN_OF;
    timerRegs->CMD_SET = TIMER_CMD_START;

    return true;
}

/*
 * Sets a repeating alarm for the timer counter.
 */
bool gmosDriverTimerRunRepeating (
    gmosDriverTimer_t* timer, uint32_t alarm)
{
    const gmosPalTimerConfig_t* timerConfig = timer->palConfig;
    TIMER_TypeDef* timerRegs = timerRegisterMap [timerConfig->timerId];

    // Place the timer in one-shot mode.
    timer->activeState = GMOS_DRIVER_TIMER_STATE_CONTINUOUS;

    // Set the counter top register to the alarm value.
    timerRegs->TOP = alarm;

    // Enable the counter with overflow interrupts.
    timerRegs->IF_CLR = TIMER_IF_OF;
    timerRegs->IEN_SET = TIMER_IEN_OF;
    timerRegs->CMD_SET = TIMER_CMD_START;

    return true;
}
