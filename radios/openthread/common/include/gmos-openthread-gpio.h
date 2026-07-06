/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2023-2026 Zynaptic Limited
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
 * This header defines additional support for interacting with the
 * OpenThread stack via host device GPIO pins. This includes visual
 * feedback of the network state via a network indicator LED and full
 * factory reset support via a GPIO software reset button.
 */

#ifndef GMOS_OPENTHREAD_GPIO_H
#define GMOS_OPENTHREAD_GPIO_H

#include "gmos-openthread.h"

/**
 * This enumeration specifies the available OpenThread network status
 * indicator LED settings.
 */
typedef enum {
    GMOS_OPENTHREAD_NETWORK_INDICATOR_LED_MODE_OFF,
    GMOS_OPENTHREAD_NETWORK_INDICATOR_LED_MODE_ON,
    GMOS_OPENTHREAD_NETWORK_INDICATOR_LED_MODE_FLASH_SLOW,
    GMOS_OPENTHREAD_NETWORK_INDICATOR_LED_MODE_FLASH_FAST
} gmosOpenThreadNetworkIndicatorLedMode_t;

/**
 * Initialises the OpenThread GPIO interaction support tasks.
 * @param openThreadStack This is the OpenThread stack data structure
 *     that will be used in conjunction with the GPIO interaction
 *     support tasks.
 */
void gmosOpenThreadGpioInit (gmosOpenThreadStack_t* openThreadStack);

/**
 * Sets the OpenThread network status indicator LED output mode.
 * @param ledMode This is the network status indicator LED output mode
 *     to be used until changed by a subsequent call to this function.
 */
#if GMOS_CONFIG_OPENTHREAD_NETWORK_INDICATOR_LED_PIN_ID != \
    GMOS_DRIVER_GPIO_UNUSED_PIN_ID
void gmosOpenThreadSetIndicatorLed (
    gmosOpenThreadNetworkIndicatorLedMode_t ledMode);
#else
#define gmosOpenThreadSetIndicatorLed(_ledMode_) {}
#endif

#endif // GMOS_OPENTHREAD_GPIO_H
