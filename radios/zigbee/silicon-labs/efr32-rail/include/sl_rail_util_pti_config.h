/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2025 Zynaptic Limited
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
 * This header specifies the default configuration options to be used by
 * the EmberZNet Zigbee network stack for packet trace interface
 * support. PTI support is disabled by default. If required for testing,
 * the application configuration file can enable PTI support for a
 * specific target board.
 */

#ifndef SL_RAIL_UTIL_PTI_CONFIG_H
#define SL_RAIL_UTIL_PTI_CONFIG_H

#include "gmos-zigbee-config.h"
#include "rail_types.h"
#include "sl_gpio.h"

/**
 * This configuration option sets the packet trace interface operating
 * mode. The available options are specified in the 'rail_types.h'
 * header file. This is disabled by default.
 */
#ifndef SL_RAIL_UTIL_PTI_MODE
#define SL_RAIL_UTIL_PTI_MODE RAIL_PTI_MODE_DISABLED
#endif

/**
 * This configuration option specifies the packet trace interface baud
 * rate to be used for transferring packet trace data.
 */
#ifndef SL_RAIL_UTIL_PTI_BAUD_RATE_HZ
#define SL_RAIL_UTIL_PTI_BAUD_RATE_HZ 1600000
#endif

/**
 * This configuration option specifies the peripheral interface to use
 * for transferring the packet trace data. The default option is to use
 * the dedicated packet trace interface.
 */
#ifndef SL_RAIL_UTIL_PTI_PERIPHERAL
#define SL_RAIL_UTIL_PTI_PERIPHERAL PTI
#endif

/**
 * These configuration options may be used to specify the PTI data
 * output pin. The choice is usually restricted to the pin supported
 * by the dedicated packet trace interface on a particular device.
 */
#ifndef SL_RAIL_UTIL_PTI_DOUT_PORT
#define SL_RAIL_UTIL_PTI_DOUT_PORT SL_GPIO_PORT_C
#define SL_RAIL_UTIL_PTI_DOUT_PIN  4
#endif

/**
 * These configuration options may be used to specify the PTI data
 * frame synchronisation pin. The choice is usually restricted to the
 * pin supported by the dedicated packet trace interface on a particular
 * device.
 */
#ifndef SL_RAIL_UTIL_PTI_DFRAME_PORT
#define SL_RAIL_UTIL_PTI_DFRAME_PORT SL_GPIO_PORT_C
#define SL_RAIL_UTIL_PTI_DFRAME_PIN  5
#endif

#endif // SL_RAIL_UTIL_PTI_CONFIG_H
