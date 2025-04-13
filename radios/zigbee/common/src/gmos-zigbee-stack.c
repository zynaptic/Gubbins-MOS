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
 * This file implements the main functions for integrating the
 * Zigbee stack into the GubbinsMOS runtime framework.
 */

#include <stdbool.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-zigbee-stack.h"

/*
 * Initialise the Zigbee stack on startup.
 */
bool gmosZigbeeStackInit (gmosZigbeeStack_t* zigbeeStack)
{
    // Initialise the common stack data.
    zigbeeStack->networkState = GMOS_ZIGBEE_NETWORK_STATE_INITIALISING;

    // Initialise the platform specific Zigbee RAL.
    return gmosZigbeeRalInit (zigbeeStack);
}

/*
 * Sets the current Zigbee network state for the specified Zigbee stack
 * instance.
 */
void gmosZigbeeSetNetworkState (gmosZigbeeStack_t* zigbeeStack,
    gmosZigbeeNetworkState_t networkState)
{
    GMOS_LOG_FMT (LOG_DEBUG,
        "Setting Zigbee network state to %d.", networkState);
    zigbeeStack->networkState = networkState;
}

/*
 * Accesses the current Zigbee network state for the specified Zigbee
 * stack instance.
 */
gmosZigbeeNetworkState_t gmosZigbeeGetNetworkState (
    gmosZigbeeStack_t* zigbeeStack)
{
    return zigbeeStack->networkState;
}
