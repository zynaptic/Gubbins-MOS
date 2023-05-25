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
 * This header provides device specific data types and definitions for
 * general purpose timers on the Silicon Labs EFR32XG2X range of
 * devices.
 */

#ifndef EFR32_DRIVER_TIMER_H
#define EFR32_DRIVER_TIMER_H

#include <stdint.h>

// Map the EFR32XG2X timer names to useful timer ID values.
#define EFR32_DRIVER_TIMER_ID_TIMER0 0
#define EFR32_DRIVER_TIMER_ID_TIMER1 1
#define EFR32_DRIVER_TIMER_ID_TIMER2 2
#define EFR32_DRIVER_TIMER_ID_TIMER3 3
#define EFR32_DRIVER_TIMER_ID_TIMER4 4

/**
 * Defines the platform specific hardware timer configuration settings
 * data structure.
 */
typedef struct gmosPalTimerConfig_t {

    // Specify the timer instance to use, taken from the list of defined
    // timer ID values.
    uint8_t timerId;

} gmosPalTimerConfig_t;

/**
 * Defines the platform specific hardware timer dynamic data structure.
 */
typedef struct gmosPalTimerState_t {

} gmosPalTimerState_t;

#endif // EFR32_DRIVER_TIMER_H
