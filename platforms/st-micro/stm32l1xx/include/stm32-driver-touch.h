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
 * This header defines the platform specific capacitive touch sensor
 * functions and data structures for the STM32L1XX series of devices.
 * This implementation only supports those devices that can use timer
 * based acquisition with TIM9 and TIM10, which are used for all touch
 * sensor instances.
 */

#ifndef STM32_DRIVER_TOUCH_H
#define STM32_DRIVER_TOUCH_H

#include "gmos-driver-touch.h"

/**
 * Defines the platform specific capacitive touch sensor channel
 * configuration settings data structure.
 */
typedef struct gmosPalTouchConfig_t {

    // Specify the GPIO pin used as the channel sensor input.
    uint16_t sensorPinId;

    // Specify the GPIO pin used for the sampling capacitor.
    uint16_t samplingPinId;

} gmosPalTouchConfig_t;

/**
 * Defines the platform specific capacitive touch sensor channel dynamic
 * data structure.
 */
typedef struct gmosPalTouchState_t {

    // Specifies a link to the next active touch channel.
    gmosDriverTouchChannel_t* nextChannel;

} gmosPalTouchState_t;

#endif // STM32_DRIVER_TOUCH_H
