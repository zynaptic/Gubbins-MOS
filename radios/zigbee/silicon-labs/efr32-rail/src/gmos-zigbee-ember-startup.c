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
 * This file implements the startup state machine for the EmberZNet
 * Zigbee stack implementation.
 */

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-zigbee-config.h"
#include "gmos-zigbee-stack.h"
#include "gmos-zigbee-ember-ral.h"
#include "sl_zigbee.h"
#include "stack-info.h"

/*
 * Perform stack setup on initialisation.
 */
static inline void gmosZigbeeRalEmberStartupInit (
    gmosZigbeeStack_t* zigbeeStack)
{
    union {
        uint32_t value;
        uint16_t source [2];
    } randomSeed;
    sl_status_t slStatus;

    // Set the APS payload size limit.
    zigbeeStack->apsMaxMessageSize =
        sl_zigbee_maximum_aps_payload_length ();
    GMOS_LOG_FMT (LOG_VERBOSE,
        "EmberZNet maximum APS payload size set to %d octets.",
        zigbeeStack->apsMaxMessageSize);

    // Add some radio entropy to the platform pseudo-random number
    // generator.
    slStatus = sl_zigbee_get_strong_random_number_array (
        randomSeed.source, 2);
    if (slStatus != SL_STATUS_OK) {
        GMOS_LOG (LOG_WARNING,
            "EmberZNet strong random number generation not available.");
        randomSeed.source [0] = sl_zigbee_get_pseudo_random_number ();
        randomSeed.source [1] = sl_zigbee_get_pseudo_random_number ();
    }
    GMOS_LOG_FMT (LOG_VERBOSE,
        "EmberZNet entropy added with seed 0x%08X.", randomSeed.value);
    gmosPalAddRandomEntropy (randomSeed.value);

    // Set the initial stack network state.
    gmosZigbeeSetNetworkState (zigbeeStack,
        GMOS_ZIGBEE_NETWORK_STATE_INITIALISING);
}

/*
 * Implement the EmberZNet startup state machine.
 */
gmosTaskStatus_t gmosZigbeeRalEmberStartupPhase (
    gmosZigbeeStack_t* zigbeeStack)
{
    gmosZigbeeRalState_t* ralData = zigbeeStack->ralData;
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    zigbeeStackPhase_t nextPhase = ralData->zigbeeStackPhase;
    zigbeeStackStateStartup_t nextState = ralData->zigbeeStackState;
    sl_zigbee_network_init_struct_t networkInitData;
    sl_status_t slStatus;

    // Implement the startup state machine.
    switch (ralData->zigbeeStackState) {

        // On startup insert a short delay for stack initialisation.
        case ZIGBEE_STACK_STATE_STARTUP_IDLE :
            gmosZigbeeRalEmberStartupInit (zigbeeStack);
            nextState = ZIGBEE_STACK_STATE_STARTUP_NETWORK_INIT;
            taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (100));
            break;

        // Attempt to perform Zigbee network initialisation.
        case ZIGBEE_STACK_STATE_STARTUP_NETWORK_INIT :
            networkInitData.bitmask = SL_ZIGBEE_NETWORK_INIT_NO_OPTIONS;
            slStatus = sl_zigbee_network_init (&networkInitData);
            if (slStatus == SL_STATUS_OK) {
                nextState = ZIGBEE_STACK_STATE_STARTUP_NETWORK_CHECK;
            } else if (slStatus == SL_STATUS_NOT_JOINED) {
                nextState = ZIGBEE_STACK_STATE_STARTUP_NETWORK_DOWN;
            } else {
                nextState = ZIGBEE_STACK_STATE_STARTUP_FAILED;
            }
            taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (100));
            break;

        // Check whether the current network configuration is valid.
        case ZIGBEE_STACK_STATE_STARTUP_NETWORK_CHECK :
            if (sl_zigbee_stack_is_up ()) {
                nextState = ZIGBEE_STACK_STATE_STARTUP_NETWORK_UP;
            } else {
                nextState = ZIGBEE_STACK_STATE_STARTUP_NETWORK_DOWN;
            }
            taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (100));
            break;

        // Complete network processing if a configured Zigbee network
        // is available.
        case ZIGBEE_STACK_STATE_STARTUP_NETWORK_UP :
            nextPhase = ZIGBEE_STACK_PHASE_ACTIVE;
            nextState = ZIGBEE_STACK_STATE_STARTUP_IDLE;
            break;

        // Select network formation or network joining if a configured
        // Zigbee network is not available.
        case ZIGBEE_STACK_STATE_STARTUP_NETWORK_DOWN :
            if (GMOS_CONFIG_ZIGBEE_NODE_TYPE ==
                GMOS_ZIGBEE_COORDINATOR_NODE) {
                nextPhase = ZIGBEE_STACK_PHASE_FORMING;
            } else {
                nextPhase = ZIGBEE_STACK_PHASE_JOINING;
            }
            nextState = ZIGBEE_STACK_STATE_STARTUP_IDLE;
            break;

        // On failure enter the stack failure state.
        default :
            nextPhase = ZIGBEE_STACK_PHASE_FAILED;
            nextState = ZIGBEE_STACK_STATE_STARTUP_FAILED;
            break;
    }
    ralData->zigbeeStackPhase = nextPhase;
    ralData->zigbeeStackState = nextState;
    return taskStatus;
}
