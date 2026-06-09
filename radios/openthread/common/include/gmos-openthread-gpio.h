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

#include "gmos-driver-gpio.h"
#include "gmos-openthread.h"

/**
 * This configuration option sets the GPIO pin to be used for the device
 * network status indicator LED. If set to a valid GPIO pin, progress
 * through the device joining process will be displayed using the LED.
 */
#ifndef GMOS_CONFIG_OPENTHREAD_NETWORK_INDICATOR_LED_PIN_ID
#define GMOS_CONFIG_OPENTHREAD_NETWORK_INDICATOR_LED_PIN_ID \
        GMOS_DRIVER_GPIO_UNUSED_PIN_ID
#endif

/**
 * This configuration option is used to specify whether the device
 * network status indicator LED is driven with an active high output
 * signal or an inverted active low signal. The default is to use a
 * standard active high output.
 */
#ifndef GMOS_CONFIG_OPENTHREAD_NETWORK_INDICATOR_LED_INVERT
#define GMOS_CONFIG_OPENTHREAD_NETWORK_INDICATOR_LED_INVERT false
#endif

/**
 * This configuration option is used to specify whether the device
 * network status indicator LED is driven with an open drain output. The
 * default is to use a standard active high output.
 */
#ifndef GMOS_CONFIG_OPENTHREAD_NETWORK_INDICATOR_LED_OPEN_DRAIN
#define GMOS_CONFIG_OPENTHREAD_NETWORK_INDICATOR_LED_OPEN_DRAIN false
#endif

/**
 * This configuration option sets the GPIO pin to be used for the device
 * factory reset. If set to a valid GPIO pin, device factory reset will
 * be handled by the OpenThread common library.
 */
#ifndef GMOS_CONFIG_OPENTHREAD_FACTORY_RESET_PIN_ID
#define GMOS_CONFIG_OPENTHREAD_FACTORY_RESET_PIN_ID \
        GMOS_DRIVER_GPIO_UNUSED_PIN_ID
#endif

/**
 * This configuration option is used to restrict the factory reset
 * process so that it can only occur immediately after a power on or
 * standard reset cycle.
 */
#ifndef GMOS_CONFIG_OPENTHREAD_FACTORY_RESET_POWER_ON_ONLY
#define GMOS_CONFIG_OPENTHREAD_FACTORY_RESET_POWER_ON_ONLY true
#endif

/**
 * This configuration option is used to specify whether the device
 * factory reset pin is active high (the default) or active low
 * (inverted).
 */
#ifndef GMOS_CONFIG_OPENTHREAD_FACTORY_RESET_INVERT
#define GMOS_CONFIG_OPENTHREAD_FACTORY_RESET_INVERT false
#endif

/**
 * This configuration option is used to specify the debounce period for
 * reset button pushes, expressed as an integer number of milliseconds.
 */
#ifndef GMOS_CONFIG_OPENTHREAD_FACTORY_RESET_DEBOUNCE_DELAY
#define GMOS_CONFIG_OPENTHREAD_FACTORY_RESET_DEBOUNCE_DELAY 2000
#endif

/**
 * This configuration option is used to specify the minimum button press
 * interval that will result in a full factory reset, expressed as an
 * integer number of milliseconds. Anything less will result in a
 * conventional reset.
 */
#ifndef GMOS_CONFIG_OPENTHREAD_FACTORY_RESET_MIN_DELAY
#define GMOS_CONFIG_OPENTHREAD_FACTORY_RESET_MIN_DELAY 10000
#endif

/**
 * This configuration option is used to specify the maximum button press
 * interval that will result in a full factory reset, expressed as an
 * integer number of milliseconds. Anything more will be ignored.
 */
#ifndef GMOS_CONFIG_OPENTHREAD_FACTORY_RESET_MAX_DELAY
#define GMOS_CONFIG_OPENTHREAD_FACTORY_RESET_MAX_DELAY 15000
#endif

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
#define gmosOpenThreadSetIndicatorLed (_ledMode_) {}
#endif

#endif // GMOS_OPENTHREAD_GPIO_H
