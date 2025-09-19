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
 * This header defines the configuration options for the MQTT client
 * library.
 */

#ifndef GMOS_MQTT_CONFIG_H
#define GMOS_MQTT_CONFIG_H

#include <stdbool.h>
#include "gmos-config.h"

/**
 * Specify the MQTT connection keep alive period as an integer number
 * of seconds. The default value is set to 5 minutes.
 */
#ifndef GMOS_CONFIG_MQTT_KEEP_ALIVE_PERIOD
#define GMOS_CONFIG_MQTT_KEEP_ALIVE_PERIOD (5 * 60)
#endif

/**
 * Specify the MQTT response timeout period as an integer number of
 * seconds. This is the time for which the client will wait for an
 * expected response before closing the connection. The default value is
 * set to 15 seconds.
 */
#ifndef GMOS_CONFIG_MQTT_TIMEOUT_PERIOD
#define GMOS_CONFIG_MQTT_TIMEOUT_PERIOD (15)
#endif

/**
 * Specify the maximum size of MQTT packets that can be handled by this
 * MQTT client implementation. This must be significantly less than the
 * maximum available buffer memory and must be representable as a 16-bit
 * integer value. Any received packets that exceed this size will be
 * treated as a protocol error.
 */
#ifndef GMOS_CONFIG_MQTT_MAX_PACKET_SIZE
#define GMOS_CONFIG_MQTT_MAX_PACKET_SIZE 1024
#endif

/**
 * Specify whether the MQTT client should automatically attempt to
 * reconnect to the broker after a link error condition.
 */
#ifndef GMOS_CONFIG_MQTT_LINK_ERROR_AUTO_RECONNECT
#define GMOS_CONFIG_MQTT_LINK_ERROR_AUTO_RECONNECT true
#endif

#endif // GMOS_MQTT_CONFIG_H
