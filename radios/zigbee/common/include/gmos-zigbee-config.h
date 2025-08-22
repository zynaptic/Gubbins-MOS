/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2022-2025 Zynaptic Limited
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
 * This header defines the default configuration options for integrating
 * the Zigbee stack into the GubbinsMOS runtime framework.
 */

#ifndef GMOS_ZIGBEE_CONFIG_H
#define GMOS_ZIGBEE_CONFIG_H

#include <stdbool.h>
#include "gmos-config.h"

/**
 * Specify the type of Zigbee node being implemented by the device.
 */
#ifndef GMOS_CONFIG_ZIGBEE_NODE_TYPE
#define GMOS_CONFIG_ZIGBEE_NODE_TYPE GMOS_ZIGBEE_COORDINATOR_NODE
#endif

// Provide an enumerated list of supported device node types.
#define GMOS_ZIGBEE_COORDINATOR_NODE  1
#define GMOS_ZIGBEE_ROUTER_NODE       2
#define GMOS_ZIGBEE_ACTIVE_CHILD_NODE 3
#define GMOS_ZIGBEE_SLEEPY_CHILD_NODE 4
#define GMOS_ZIGBEE_MOBILE_CHILD_NODE 5

/**
 * Specify whether the Zigbee node should be configured as a
 * concentrator with source routing support.
 */
#ifndef GMOS_CONFIG_ZIGBEE_CONCENTRATOR_NODE
#if (GMOS_CONFIG_ZIGBEE_NODE_TYPE == GMOS_ZIGBEE_COORDINATOR_NODE)
#define GMOS_CONFIG_ZIGBEE_CONCENTRATOR_NODE true
#else
#define GMOS_CONFIG_ZIGBEE_CONCENTRATOR_NODE false
#endif
#endif

/**
 * Specify whether the Zigbee node should be configured as a parent node
 * with support for the specified number of child nodes.
 */
#ifndef GMOS_CONFIG_ZIGBEE_MAX_END_DEVICE_CHILD_NODES
#if (GMOS_CONFIG_ZIGBEE_NODE_TYPE == GMOS_ZIGBEE_COORDINATOR_NODE)
#define GMOS_CONFIG_ZIGBEE_MAX_END_DEVICE_CHILD_NODES 8
#elif (GMOS_CONFIG_ZIGBEE_NODE_TYPE == GMOS_ZIGBEE_ROUTER_NODE)
#define GMOS_CONFIG_ZIGBEE_MAX_END_DEVICE_CHILD_NODES 8
#else
#define GMOS_CONFIG_ZIGBEE_MAX_END_DEVICE_CHILD_NODES 0
#endif
#endif

/**
 * Specify the fast polling interval to use for sleepy end devices as an
 * integer number of milliseconds. This is the polling interval to use
 * if the last polling request returned data from the parent node.
 */
#ifndef GMOS_CONFIG_ZIGBEE_FAST_POLL_INTERVAL
#define GMOS_CONFIG_ZIGBEE_FAST_POLL_INTERVAL 1000
#endif

/**
 * Specify the slow polling interval to use for sleepy end devices as an
 * integer number of milliseconds. This is the polling interval to use
 * if the last polling request did not return any data from the parent
 * node.
 */
#ifndef GMOS_CONFIG_ZIGBEE_SLOW_POLL_INTERVAL
#define GMOS_CONFIG_ZIGBEE_SLOW_POLL_INTERVAL 5000
#endif

/**
 * Specify the slow polling limit to use for sleepy end devices. This is
 * the number of consecutive polling requests that return no data before
 * the device enters hibernation.
 */
#ifndef GMOS_CONFIG_ZIGBEE_SLOW_POLL_LIMIT
#define GMOS_CONFIG_ZIGBEE_SLOW_POLL_LIMIT 3
#endif

/**
 * Specify the hibernation interval to use for sleepy end devices as an
 * integer number of milliseconds. This is the interval between polling
 * cycles that will be used if there is no other device activity.
 */
#ifndef GMOS_CONFIG_ZIGBEE_HIBERNATE_INTERVAL
#define GMOS_CONFIG_ZIGBEE_HIBERNATE_INTERVAL 60000
#endif

/**
 * Specify the device rejoin interval to use for sleepy end devices as
 * an integer number of milliseconds. This is the interval between
 * rejoin attempts if the device has not been able to communicate with
 * the network.
 */
#ifndef GMOS_CONFIG_ZIGBEE_REJOIN_RETRY_INTERVAL
#define GMOS_CONFIG_ZIGBEE_REJOIN_RETRY_INTERVAL 60000
#endif

/**
 * Specify the default radio transmit power level expressed in dBm.
 */
#ifndef GMOS_CONFIG_ZIGBEE_DEFAULT_TX_POWER
#define GMOS_CONFIG_ZIGBEE_DEFAULT_TX_POWER 10
#endif

/**
 * Specify that device joining should use the standard Zigbee 3.0
 * device joining handshake with link key replacement.
 */
#ifndef GMOS_CONFIG_ZIGBEE_UPDATE_LINK_KEY_ON_JOINING
#define GMOS_CONFIG_ZIGBEE_UPDATE_LINK_KEY_ON_JOINING true
#endif

/**
 * Specify the number of candidate channels to use during the network
 * formation active network scan process.
 */
#ifndef GMOS_CONFIG_ZIGBEE_SCAN_CANDIDATE_CHANNELS
#define GMOS_CONFIG_ZIGBEE_SCAN_CANDIDATE_CHANNELS 4
#endif

/**
 * Specify the maximum number of concurrent in-flight APS message
 * transmit requests that are supported. This is typically limited to
 * the number of APS unicasts supported by the underlying Zigbee stack,
 * since multicasts and broadcasts should be much less frequent.
 */
#ifndef GMOS_CONFIG_ZIGBEE_APS_TRANSMIT_MAX_REQUESTS
#define GMOS_CONFIG_ZIGBEE_APS_TRANSMIT_MAX_REQUESTS 8
#endif

/**
 * Specify the maximum supported transmission radius of APS messages.
 * The default value matches that for the standard Zigbee network
 * profile.
 */
#ifndef GMOS_CONFIG_ZIGBEE_APS_TRANSMIT_MAX_RADIUS
#define GMOS_CONFIG_ZIGBEE_APS_TRANSMIT_MAX_RADIUS 30
#endif

/**
 * Specify the maximum number of concurrent in-flight ZDO client request
 * messages that are supported.
 */
#ifndef GMOS_CONFIG_ZIGBEE_ZDO_CLIENT_MAX_REQUESTS
#define GMOS_CONFIG_ZIGBEE_ZDO_CLIENT_MAX_REQUESTS 2
#endif

/**
 * Specify whether support for processing ZDO device announcements
 * should be included in the ZDO server. This should be set to the
 * maximum number of supported device announcement callback handlers,
 * or zero if device announcement processing is not supported.
 */
#ifndef GMOS_CONFIG_ZIGBEE_ZDO_SERVER_MAX_DEV_ANNCE_HANDLERS
#define GMOS_CONFIG_ZIGBEE_ZDO_SERVER_MAX_DEV_ANNCE_HANDLERS 0
#endif

/**
 * Specify whether floating point data types are supported for use by
 * the Zigbee cluster library.
 */
#ifndef GMOS_CONFIG_ZIGBEE_ZCL_FLOATING_POINT_SUPPORT
#define GMOS_CONFIG_ZIGBEE_ZCL_FLOATING_POINT_SUPPORT false
#endif

/**
 * Specify whether composite attribute data types are supported for use
 * by the Zigbee cluster library.
 */
#ifndef GMOS_CONFIG_ZIGBEE_ZCL_COMPOSITE_ATTRIBUTE_SUPPORT
#define GMOS_CONFIG_ZIGBEE_ZCL_COMPOSITE_ATTRIBUTE_SUPPORT false
#endif

/**
 * Define the default number of concurrent ZCL remote transactions that
 * are supported by each ZCL endpoint.
 */
#ifndef GMOS_CONFIG_ZIGBEE_ZCL_REMOTE_MAX_REQUESTS
#define GMOS_CONFIG_ZIGBEE_ZCL_REMOTE_MAX_REQUESTS 2
#endif

/**
 * Define the default length of the local ZCL command processing queue
 * as an integer number of queued commands.
 */
#ifndef GMOS_CONFIG_ZIGBEE_ZCL_LOCAL_COMMAND_QUEUE_LENGTH
#define GMOS_CONFIG_ZIGBEE_ZCL_LOCAL_COMMAND_QUEUE_LENGTH 2
#endif

/**
 * Specify whether the coordinator trust centre should use a common link
 * key for all devices in the network or whether it should generate
 * device specific hashed link keys.
 */
#ifndef GMOS_CONFIG_ZIGBEE_COORDINATOR_USES_HASHED_LINK_KEYS
#define GMOS_CONFIG_ZIGBEE_COORDINATOR_USES_HASHED_LINK_KEYS true
#endif

#endif // GMOS_ZIGBEE_CONFIG_H
