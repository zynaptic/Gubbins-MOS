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
 * This header provides device specific data types and definitions for
 * general purpose timers on the Raspberry Pi RP2040 range of devices.
 * This uses the four RP2040 system timer alarm interrupts to emulate a
 * set of conventional hardware timers.
 */

#ifndef PICO_DRIVER_TIMER_H
#define PICO_DRIVER_TIMER_H

#include <stdint.h>

/**
 * Defines the platform specific hardware timer configuration settings
 * data structure.
 */
typedef struct gmosPalTimerConfig_t {

    // Specify the timer alarm ID. This selects the RP2040 system timer
    // alarm interrupt (0 to 3) that will be used for the timer.
    uint8_t timerAlarmId;

} gmosPalTimerConfig_t;

/**
 * Defines the platform specific hardware timer dynamic data structure.
 */
typedef struct gmosPalTimerState_t {

    // Specify the timestamp value that was assigned at the start of
    // the current timer period.
    uint64_t timestamp;

    // Specify the timer tick interval in microseconds.
    uint16_t tickPeriod;

    // Specify the timer period as an integer number of ticks.
    uint16_t timerPeriod;

} gmosPalTimerState_t;

#endif // PICO_DRIVER_TIMER_H
