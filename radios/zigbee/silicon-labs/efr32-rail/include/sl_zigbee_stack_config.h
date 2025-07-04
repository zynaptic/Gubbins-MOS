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
 * the EmberZNet Zigbee network stack.
 */

#include "gmos-zigbee-config.h"

/*
 * Specify the fixed device CPU and SoC options for the Zigbee stack.
 */
#define CORTEXM3
#define CORTEXM3_EFR32
#define CORTEXM3_EFM32_MICRO
#define PHY_EFR32

/*
 * Select the modular components for the Zigbee stack.
 */
#define SL_COMPONENT_CATALOG_PRESENT

/*
 * Use the single network Zigbee Pro stack profile.
 */
#define SL_ZIGBEE_STACK_PROFILE 2
#define SL_ZIGBEE_MULTI_NETWORK_STRIPPED

/*
 * Enable common stack callbacks for all device types.
 */
#define SL_ZIGBEE_APPLICATION_HANDLES_UNSUPPORTED_ZDO_REQUESTS
#define SL_ZIGBEE_APPLICATION_HANDLES_ENDPOINT_ZDO_REQUESTS

/*
 * Enable coordinator specific stack callbacks.
 */
#if (GMOS_CONFIG_ZIGBEE_NODE_TYPE == GMOS_ZIGBEE_COORDINATOR_NODE)
#define SL_ZIGBEE_APPLICATION_HAS_ENERGY_SCAN_RESULT_HANDLER
#define SL_ZIGBEE_APPLICATION_HAS_TRUST_CENTER_JOIN_HANDLER
#endif

/*
 * Enable source routing library for concentrator nodes.
 */
#if (GMOS_CONFIG_ZIGBEE_CONCENTRATOR_NODE == true)
#define SL_CATALOG_ZIGBEE_SOURCE_ROUTE_PRESENT
#define SL_ZIGBEE_APPLICATION_HAS_OVERRIDE_SOURCE_ROUTING
#endif
