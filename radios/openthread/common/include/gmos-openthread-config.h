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
 * This header defines the common configuration options for the
 * OpenThread stack.
 */

#ifndef GMOS_OPENTHREAD_CONFIG_H
#define GMOS_OPENTHREAD_CONFIG_H

#include <stdbool.h>
#include <stddef.h>
#include "gmos-config.h"
#include "gmos-driver-gpio.h"

/**
 * This configuration option is a fixed string which specifies the
 * OpenThread provisioning URL to be used during the joining process.
 */
#ifndef GMOS_CONFIG_OPENTHREAD_PROVISIONING_URL
#define GMOS_CONFIG_OPENTHREAD_PROVISIONING_URL NULL
#endif

/**
 * This configuration option is a fixed string which specifies the
 * OpenThread vendor name to be used during the joining process.
 */
#ifndef GMOS_CONFIG_OPENTHREAD_VENDOR_NAME
#define GMOS_CONFIG_OPENTHREAD_VENDOR_NAME NULL
#endif

/**
 * This configuration option is a fixed string which specifies the
 * OpenThread vendor device model name to be used during the joining
 * process.
 */
#ifndef GMOS_CONFIG_OPENTHREAD_VENDOR_MODEL
#define GMOS_CONFIG_OPENTHREAD_VENDOR_MODEL NULL
#endif

/**
 * This configuration option is a fixed string which specifies the
 * OpenThread vendor software version to be used during the joining
 * process.
 */
#ifndef GMOS_CONFIG_OPENTHREAD_VENDOR_SW_VERSION
#define GMOS_CONFIG_OPENTHREAD_VENDOR_SW_VERSION NULL
#endif

/**
 * This configuration option is a fixed string which specifies the
 * OpenThread vendor data field to be used during the joining process.
 */
#ifndef GMOS_CONFIG_OPENTHREAD_VENDOR_DATA
#define GMOS_CONFIG_OPENTHREAD_VENDOR_DATA NULL
#endif

/**
 * This configuration option is used to enable OpenThread CLI support
 * for interactive device debugging. This feature will not normally be
 * required.
 */
#ifndef GMOS_CONFIG_OPENTHREAD_ENABLE_INTERACTIVE_CLI
#define GMOS_CONFIG_OPENTHREAD_ENABLE_INTERACTIVE_CLI false
#endif

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

#endif // GMOS_OPENTHREAD_CONFIG_H
