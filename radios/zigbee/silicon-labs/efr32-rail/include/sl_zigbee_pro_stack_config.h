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
 * the EmberZNet Zigbee network stack for router and coordinator nodes.
 * Only those values which differ from the default settings specified in
 * the Simplicity SDK 'sl_zigbee_configuration_defaults.h' header file
 * are included here, but the application configuration file can
 * override any settings as required.
 */

#ifndef SL_ZIGBEE_PRO_STACK_CONFIG_H
#define SL_ZIGBEE_PRO_STACK_CONFIG_H

#include "gmos-zigbee-config.h"

/**
 * This configuration option specifies the child table size to be used,
 * which sets the maximum number of end devices that can be supported by
 * a single router node. This may be configured to support up to 64 end
 * devices.
 */
#ifndef SL_ZIGBEE_MAX_END_DEVICE_CHILDREN
#define SL_ZIGBEE_MAX_END_DEVICE_CHILDREN 8
#endif

/**
 * This configuration option specifies the amount of memory to be
 * allocated for Zigbee packet buffer storage. Sizes from 512 to 16384
 * bytes are supported, but the size must be aligned to a 4-byte
 * boundary. Each packet buffer has an allocation overhead of 8 bytes.
 */
#ifndef SL_ZIGBEE_PACKET_BUFFER_HEAP_SIZE
#define SL_ZIGBEE_PACKET_BUFFER_HEAP_SIZE 8192
#endif

/**
 * This configuration option sets the end device poll timeout value,
 * which is the amount of time that must pass without hearing a MAC data
 * poll from an end device before it is removed from the child table.
 * A value of 0 selects a timeout of 10 seconds, with other values from
 * 1 to 14 specifying an integer number of minutes as 2^N. The default
 * value of 6 gives a poll timeout of just over 1 hour.
 */
#ifndef SL_ZIGBEE_END_DEVICE_POLL_TIMEOUT
#define SL_ZIGBEE_END_DEVICE_POLL_TIMEOUT 6
#endif

#endif // SL_ZIGBEE_PRO_STACK_CONFIG_H
