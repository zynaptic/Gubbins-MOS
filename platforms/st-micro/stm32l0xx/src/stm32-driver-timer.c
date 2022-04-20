/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2021 Zynaptic Limited
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
 * Implements general purpose timer functionality for the STM32L0XX
 * series of microcontrollers.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-driver-timer.h"
#include "stm32-device.h"
#include "stm32-driver-timer.h"

// Add dummy definitions for unsupported timer 3.
#ifndef TIM3
#define TIM3 NULL
#define TIM3_IRQn 0
#define RCC_APB1ENR_TIM3EN 0
#endif

// Add dummy definitions for unsupported timer 6.
#ifndef TIM6
#define TIM6 NULL
#define TIM6_DAC_IRQn 0
#define RCC_APB1ENR_TIM6EN 0
#endif

// Add dummy definitions for unsupported timer 7.
#ifndef TIM7
#define TIM7 NULL
#define TIM7_IRQn 0
#define RCC_APB1ENR_TIM7EN 0
#endif

// Add dummy definitions for unsupported timer 22.
#ifndef TIM22
#define TIM22 NULL
#define TIM22_IRQn 0
#define RCC_APB2ENR_TIM22EN 0
#endif

// Provide mapping of timer IDs to register sets.
static TIM_TypeDef* timRegisterMap [] = {
    TIM2, TIM3, TIM6, TIM7, TIM21, TIM22};

// Provide mapping of timer IDs to NVIC IRQ numbers.
static IRQn_Type timNvicIrqMap [] = {
    TIM2_IRQn, TIM3_IRQn, TIM6_DAC_IRQn,
    TIM7_IRQn, TIM21_IRQn, TIM22_IRQn};

// Provide mapping of timer IDs to APB clock enable bits.
static uint32_t timClockEnMap [] = {
    RCC_APB1ENR_TIM2EN, RCC_APB1ENR_TIM3EN, RCC_APB1ENR_TIM6EN,
    RCC_APB1ENR_TIM7EN, RCC_APB2ENR_TIM21EN, RCC_APB2ENR_TIM22EN};

// Provide reverse mapping of timer IDs to timer state data structures.
static gmosDriverTimer_t* timerDataMap [] = {
    NULL, NULL, NULL, NULL, NULL, NULL};

/*
 * Implement common ISR handling.
 */
static void stm32DriverTimerCommonIsr (gmosDriverTimer_t* timer)
{
    const gmosPalTimerConfig_t* timerConfig = timer->palConfig;
    TIM_TypeDef* timRegs = timRegisterMap [timerConfig->timerId];

    // Call user ISR if it is configured.
    if (timer->timerIsr != NULL) {
        timer->timerIsr (timer->timerIsrData);
    }

    // Place the timer in the reset hold state if this is a one-shot
    // interrupt.
    if ((timRegs->CR1 & TIM_CR1_OPM) != 0) {
        gmosDriverTimerReset (timer, true);
    }

    // Clear the interrupt status register.
    timRegs->SR &= ~TIM_SR_UIF;
}

/*
 * Process ISR for timer 2.
 */
void gmosPalIsrTIM2 (void)
{
    stm32DriverTimerCommonIsr (timerDataMap [STM32_DRIVER_TIMER_ID_TIM2]);
}

/*
 * Process ISR for timer 3 if supported.
 */
#if (RCC_APB1ENR_TIM3EN != 0)
void gmosPalIsrTIM3 (void)
{
    stm32DriverTimerCommonIsr (timerDataMap [STM32_DRIVER_TIMER_ID_TIM3]);
}
#endif

/*
 * Process ISR for timer 6 if supported.
 */
#if (RCC_APB1ENR_TIM6EN != 0)
void gmosPalIsrTIM6 (void)
{
    stm32DriverTimerCommonIsr (timerDataMap [STM32_DRIVER_TIMER_ID_TIM6]);
}
#endif

/*
 * Process ISR for timer 7 if supported.
 */
#if (RCC_APB1ENR_TIM7EN != 0)
void gmosPalIsrTIM7 (void)
{
    stm32DriverTimerCommonIsr (timerDataMap [STM32_DRIVER_TIMER_ID_TIM7]);
}
#endif

/*
 * Process ISR for timer 21.
 */
void gmosPalIsrTIM21 (void)
{
    stm32DriverTimerCommonIsr (timerDataMap [STM32_DRIVER_TIMER_ID_TIM21]);
}

/*
 * Process ISR for timer 22 if supported.
 */
#if (RCC_APB2ENR_TIM22EN != 0)
void gmosPalIsrTIM22 (void)
{
    stm32DriverTimerCommonIsr (timerDataMap [STM32_DRIVER_TIMER_ID_TIM22]);
}
#endif

/*
 * Configures the timer base clock source during initialisation. Clock
 * source selection varies between timers so not all configurations are
 * valid.
 */
static inline bool stm32DriverTimerSetBaseClock
    (const gmosPalTimerConfig_t* timerConfig)
{
    TIM_TypeDef* timRegs = timRegisterMap [timerConfig->timerId];
    bool clockConfigOk = false;

    // The APB clock source is the default reset configuration and is
    // supported by all timers.
    if (timerConfig->timerClk == STM32_DRIVER_TIMER_CLK_APB) {
        clockConfigOk = true;
    }

    // The LSE clock source is only supported by a subset of the timers
    // and requires configuration.
    else if (timerConfig->timerClk == STM32_DRIVER_TIMER_CLK_LSE) {

        // The clock source option register differs between the timers.
        switch (timerConfig->timerId) {
            case STM32_DRIVER_TIMER_ID_TIM2 :
                timRegs->OR |= TIM2_OR_ETR_RMP_0 | TIM2_OR_ETR_RMP_2;
                clockConfigOk = true;
                break;
            case STM32_DRIVER_TIMER_ID_TIM21 :
                timRegs->OR |= TIM21_OR_ETR_RMP_0 | TIM21_OR_ETR_RMP_1;
                clockConfigOk = true;
                break;
            case STM32_DRIVER_TIMER_ID_TIM22 :
                timRegs->OR |= TIM22_OR_ETR_RMP_0 | TIM22_OR_ETR_RMP_1;
                clockConfigOk = true;
                break;
        }

        // The external trigger configuration is common for all timers.
        if (clockConfigOk) {
            timRegs->SMCR |= TIM_SMCR_ECE;
        }
    }
    return clockConfigOk;
}

/*
 * Gets the base clock frequency for the timer, based on the provided
 * timer configuration.
 */
static inline uint32_t stm32DriverTimerGetBaseClock
    (const gmosPalTimerConfig_t* timerConfig)
{
    uint32_t baseClock = 0;

    // Select the 32.768 kHz crystal as the clock source.
    if (timerConfig->timerClk == STM32_DRIVER_TIMER_CLK_LSE) {
        baseClock = 32768;
    }

    // Select the APB clock frequency based on the timer ID. This is 1x
    // the APB clock if no APB clock division is being used or 2x the
    // APB clock if APB clock division is being used.
    else if (timerConfig->timerClk == STM32_DRIVER_TIMER_CLK_APB) {
        switch (timerConfig->timerId) {
            case STM32_DRIVER_TIMER_ID_TIM2 :
            case STM32_DRIVER_TIMER_ID_TIM3 :
            case STM32_DRIVER_TIMER_ID_TIM6 :
            case STM32_DRIVER_TIMER_ID_TIM7 :
                if (GMOS_CONFIG_STM32_APB1_CLOCK_DIV == 1) {
                    baseClock = GMOS_CONFIG_STM32_APB1_CLOCK;
                } else {
                    baseClock = 2 * GMOS_CONFIG_STM32_APB1_CLOCK;
                }
                break;
            case STM32_DRIVER_TIMER_ID_TIM21 :
            case STM32_DRIVER_TIMER_ID_TIM22 :
                if (GMOS_CONFIG_STM32_APB2_CLOCK_DIV == 1) {
                    baseClock = GMOS_CONFIG_STM32_APB2_CLOCK;
                } else {
                    baseClock = 2 * GMOS_CONFIG_STM32_APB2_CLOCK;
                }
                break;
        }
    }
    return baseClock;
}

/*
 * Sets the timer clock frequency by configuring the clock prescaler.
 */
static inline uint32_t stm32DriverTimerSetClock
    (gmosDriverTimer_t* timer, uint32_t frequency)
{
    const gmosPalTimerConfig_t* timerConfig = timer->palConfig;
    TIM_TypeDef* timRegs = timRegisterMap [timerConfig->timerId];
    uint32_t baseClock;
    uint32_t prescaler;

    // Configure the base clock source.
    if (!stm32DriverTimerSetBaseClock (timerConfig)) {
        return false;
    }

    // Derive the preferred prescaler for the requested frequency. If an
    // approximate frequency is used, it will be lower than requested.
    baseClock = stm32DriverTimerGetBaseClock (timerConfig);
    prescaler = baseClock / frequency;
    if ((frequency * prescaler) != baseClock) {
        prescaler += 1;
        GMOS_LOG_FMT (LOG_WARNING,
            "Timer %d clock requested %ldHz, actual %ldHz.",
            timerConfig->timerId, frequency, baseClock / prescaler);
    }

    // Check that the prescaler is within the STM32 16 bit counter
    // range. Note that the value to be programmed into the prescaler
    // register subtracts 1 from the actual prescale value.
    if (prescaler > 0x10000) {
        return false;
    }

    // Sets the timer frequency being used.
    timer->frequency = baseClock / prescaler;
    timer->maxValue = 0xFFFF;

    // Set the prescale register value.
    timRegs->PSC = prescaler - 1;
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
    uint8_t timerIndex;

    // Check for a valid timer selection.
    timerIndex = timerConfig->timerId;
    if ((timerIndex >= 6) || (timRegisterMap [timerIndex] == NULL) ||
        (timerDataMap [timerIndex] != NULL)) {
        return false;
    }

    // Enable the main clock for the timer peripheral, which will allow
    // the timer registers to be accessed when the processor is awake.
    // Note that the corresponding sleep mode clock enable bit is only
    // set while the timer is explicitly enabled.
    switch (timerIndex) {
        case STM32_DRIVER_TIMER_ID_TIM21 :
        case STM32_DRIVER_TIMER_ID_TIM22 :
            RCC->APB2ENR |= timClockEnMap [timerIndex];
            break;
        default :
            RCC->APB1ENR |= timClockEnMap [timerIndex];
            break;
    }

    // Configure the timer clocks.
    if (!stm32DriverTimerSetClock (timer, frequency)) {
        return false;
    }

    // Register the timer and timer interrupt.
    timerDataMap [timerIndex] = timer;
    timer->timerIsr = timerIsr;
    timer->timerIsrData = timerIsrData;
    return true;
}

/*
 * Enables a timer and associated interrupt for subsequent use. This
 * allows the timer counter to increment at the configured timer clock
 * frequency.
 */
bool gmosDriverTimerEnable (gmosDriverTimer_t* timer)
{
    const gmosPalTimerConfig_t* timerConfig = timer->palConfig;
    uint8_t timerIndex = timerConfig->timerId;

    // Place the timer in the reset hold state.
    gmosDriverTimerReset (timer, true);

    // Enable NVIC interrupt for the timer.
    NVIC_EnableIRQ (timNvicIrqMap [timerIndex]);

    // Enable timer operation in processor sleep mode.
    switch (timerIndex) {
        case STM32_DRIVER_TIMER_ID_TIM21 :
        case STM32_DRIVER_TIMER_ID_TIM22 :
            RCC->APB2SMENR |= timClockEnMap [timerIndex];
            break;
        default :
            RCC->APB1SMENR |= timClockEnMap [timerIndex];
            break;
    }
    return true;
}

/*
 * Disables a timer and associated interrupt for subsequent use. This
 * allows the timer counter to be placed in a low power state.
 */
bool gmosDriverTimerDisable (gmosDriverTimer_t* timer)
{
    const gmosPalTimerConfig_t* timerConfig = timer->palConfig;
    uint8_t timerIndex = timerConfig->timerId;

    // Stop the timer counter.
    gmosDriverTimerReset (timer, true);

    // Disable NVIC interrupt for the timer.
    NVIC_DisableIRQ (timNvicIrqMap [timerIndex]);

    // Disable timer operation in processor sleep mode.
    switch (timerIndex) {
        case STM32_DRIVER_TIMER_ID_TIM21 :
        case STM32_DRIVER_TIMER_ID_TIM22 :
            RCC->APB2SMENR &= ~(timClockEnMap [timerIndex]);
            break;
        default :
            RCC->APB1SMENR &= ~(timClockEnMap [timerIndex]);
            break;
    }
    return true;
}

/*
 * Masks the timer interrupts, controlling when the timer interrupt
 * service routine will be allowed to run.
 */
bool gmosDriverTimerIsrMask (gmosDriverTimer_t* timer, bool isrMask)
{
    const gmosPalTimerConfig_t* timerConfig = timer->palConfig;
    uint8_t timerIndex = timerConfig->timerId;

    // Mask the interrupts using the NVIC, to avoid potential conflicts
    // from manipulating the timer registers directly.
    if (isrMask) {
        NVIC_DisableIRQ (timNvicIrqMap [timerIndex]);
    } else {
        NVIC_EnableIRQ (timNvicIrqMap [timerIndex]);
    }
    return true;
}

/*
 * Resets the current value of the 16-bit timer counter to zero.
 */
bool gmosDriverTimerReset (gmosDriverTimer_t* timer, bool resetHold)
{
    const gmosPalTimerConfig_t* timerConfig = timer->palConfig;
    TIM_TypeDef* timRegs = timRegisterMap [timerConfig->timerId];

    // Place the timer in the reset hold state.
    timRegs->DIER &= ~TIM_DIER_UIE;
    timRegs->CR1 &= ~(TIM_CR1_CEN | TIM_CR1_OPM);
    timRegs->CNT = 0;
    timRegs->ARR = 0xFFFF;

    // Release from reset if required.
    if (!resetHold) {
        timRegs->CR1 |= TIM_CR1_CEN;
    }
    return true;
}

/*
 * Accesses the current 16-bit timer counter value.
 */
uint16_t gmosDriverTimerGetValue (gmosDriverTimer_t* timer)
{
    const gmosPalTimerConfig_t* timerConfig = timer->palConfig;
    TIM_TypeDef* timRegs = timRegisterMap [timerConfig->timerId];

    // Read directly from the counter register.
    return timRegs->CNT;
}

/*
 * Sets a one-shot alarm for the 16-bit timer counter.
 */
bool gmosDriverTimerRunOneShot (gmosDriverTimer_t* timer, uint16_t alarm)
{
    const gmosPalTimerConfig_t* timerConfig = timer->palConfig;
    TIM_TypeDef* timRegs = timRegisterMap [timerConfig->timerId];

    // Set the auto reload register to the alarm value.
    timRegs->ARR = alarm;

    // Enable the counter for auto reload interrupts and one shot
    // operation.
    timRegs->DIER |= TIM_DIER_UIE;
    timRegs->CR1 |= TIM_CR1_CEN | TIM_CR1_OPM;

    return true;
}

/*
 * Sets a repeating alarm for the 16-bit timer counter.
 */
bool gmosDriverTimerRunRepeating (gmosDriverTimer_t* timer, uint16_t alarm)
{
    const gmosPalTimerConfig_t* timerConfig = timer->palConfig;
    TIM_TypeDef* timRegs = timRegisterMap [timerConfig->timerId];
    uint32_t regValue;

    // Set the auto reload register to the alarm value.
    timRegs->ARR = alarm;

    // Enable the counter for auto reload interrupts.
    timRegs->DIER |= TIM_DIER_UIE;
    regValue = timRegs->CR1;
    regValue |= TIM_CR1_CEN;
    regValue &= ~TIM_CR1_OPM;
    timRegs->CR1 = regValue;

    return true;
}
