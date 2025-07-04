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
 * This header file specifies the Simplicity SDK components which need
 * to be included in the EmberZNet stack build.
 */

#ifndef SL_COMPONENT_CATALOG_H
#define SL_COMPONENT_CATALOG_H

/*
 * Select the modular components for the Zigbee stack.
 */
#define SL_CATALOG_RAIL_LIB_PRESENT
#define SL_CATALOG_EMLIB_RMU_PRESENT
#define SL_CATALOG_ZIGBEE_PRO_STACK_PRESENT
#define SL_CATALOG_RADIO_PRIORITY_15_4_PRESENT
#define SL_CATALOG_ZIGBEE_STRONG_RANDOM_API_PSA_PRESENT

#endif // SL_COMPONENT_CATALOG_H
