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
 * This header provides device specific data types and definitions for
 * general purpose timers on the STM32L0XX range of devices.
 */

#ifndef STM32_DRIVER_TIMER_H
#define STM32_DRIVER_TIMER_H

// Map the STM32L0XX timer names to useful timer ID values.
#define STM32_DRIVER_TIMER_ID_TIM2  0
#define STM32_DRIVER_TIMER_ID_TIM3  1
#define STM32_DRIVER_TIMER_ID_TIM6  2
#define STM32_DRIVER_TIMER_ID_TIM7  3
#define STM32_DRIVER_TIMER_ID_TIM21 4
#define STM32_DRIVER_TIMER_ID_TIM22 5

// Specify the available timer clock sources.
#define STM32_DRIVER_TIMER_CLK_APB 0
#define STM32_DRIVER_TIMER_CLK_LSE 1

/**
 * Defines the platform specific hardware timer configuration settings
 * data structure.
 */
typedef struct gmosPalTimerConfig_t {

    // Specify the timer instance to use, taken from the list of defined
    // timer ID values.
    uint8_t timerId;

    // Specify the timer clock source to use, taken from the list of
    // defined timer clock sources.
    uint8_t timerClk;

} gmosPalTimerConfig_t;

/**
 * Defines the platform specific hardware timer dynamic data structure.
 */
typedef struct gmosPalTimerState_t {

} gmosPalTimerState_t;

#endif // STM32_DRIVER_TIMER_H
