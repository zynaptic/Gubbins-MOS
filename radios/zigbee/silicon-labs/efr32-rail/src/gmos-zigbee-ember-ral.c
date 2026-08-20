/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2025-2026 Zynaptic Limited
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
 * This file implements the main target platform functions for
 * integrating the Zigbee stack into the GubbinsMOS runtime framework.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-scheduler.h"
#include "gmos-crypto.h"
#include "gmos-zigbee-stack.h"
#include "gmos-zigbee-ember-ral.h"
#include "sl_zigbee.h"
#include "sl_rail_util_pti.h"
#include "pa_conversions_efr32.h"
#include "sl_token_manager_api.h"

/*
 * Only one instance of the EmberZNet stack can be running on a single
 * device. This global variable may be used from the various EmberZNet
 * callback functions to obtain a reference to the stack instance when
 * required.
 */
gmosZigbeeStack_t* gmosZigbeeRalEmberStackInstance = NULL;

/*
 * Implement the periodic EmberZNet stack tick as an independent
 * GubbinsMOS task.
 */
static inline gmosTaskStatus_t gmosZigbeeEmberStackTickFn (
    gmosZigbeeStack_t* zigbeeStack)
{
    gmosTaskStatus_t taskStatus;

    // Implement periodic stack processing for sleepy devices.
    if ((GMOS_CONFIG_ZIGBEE_NODE_TYPE != GMOS_ZIGBEE_COORDINATOR_NODE) &&
        (GMOS_CONFIG_ZIGBEE_NODE_TYPE != GMOS_ZIGBEE_ROUTER_NODE)) {
        taskStatus = gmosZigbeeRalEmberSleepyNodeTick (zigbeeStack);
    }

    // Implement a tight processing loop for receive while idle devices.
    else {
        sl_zigbee_tick ();
        taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    }
    return taskStatus;
}
GMOS_TASK_DEFINITION (gmosZigbeeEmberStackTick,
    gmosZigbeeEmberStackTickFn, gmosZigbeeStack_t);

/*
 * Implement the main task loop for the EmberZNet interface state
 * machine.
 */
static inline gmosTaskStatus_t gmosZigbeeEmberStackWorkerFn (
    gmosZigbeeStack_t* zigbeeStack)
{
    gmosZigbeeRalState_t* ralData = zigbeeStack->ralData;
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;

    // Select the appropriate state machine operating phase.
    switch (ralData->zigbeeStackPhase) {

        // Run the startup state machine in the startup phase.
        case ZIGBEE_STACK_PHASE_STARTUP :
            taskStatus = gmosZigbeeRalEmberStartupPhase (zigbeeStack);
            break;

        // Run the active state machine in the active network phase.
        case ZIGBEE_STACK_PHASE_ACTIVE :
            taskStatus = gmosZigbeeRalEmberActivePhase (zigbeeStack);
            break;

        // Run network formation for a coordinator node.
        #if (GMOS_CONFIG_ZIGBEE_NODE_TYPE == GMOS_ZIGBEE_COORDINATOR_NODE)
        case ZIGBEE_STACK_PHASE_FORMING :
            taskStatus = gmosZigbeeRalEmberFormNetworkPhase (zigbeeStack);
            break;
        #endif

        // Run network joining for conventional nodes.
        #if (GMOS_CONFIG_ZIGBEE_NODE_TYPE != GMOS_ZIGBEE_COORDINATOR_NODE)
        case ZIGBEE_STACK_PHASE_JOINING :
            taskStatus = gmosZigbeeRalEmberJoinNetworkPhase (zigbeeStack);
            break;
        #endif

        // Suspend further processing on failure.
        default :
            taskStatus = GMOS_TASK_SUSPEND;
            break;
    }
    return taskStatus;
}
GMOS_TASK_DEFINITION (gmosZigbeeEmberStackWorker,
    gmosZigbeeEmberStackWorkerFn, gmosZigbeeStack_t);

/*
 * Initialise the Zigbee stack radio abstraction layer on startup.
 */
bool gmosZigbeeRalInit (gmosZigbeeStack_t* zigbeeStack)
{
    gmosZigbeeRalState_t* ralData = zigbeeStack->ralData;
    sl_status_t slStatus;
    bool initOk = true;

    // Only a single EmberZNet stack instance may be used per device.
    if (gmosZigbeeRalEmberStackInstance != NULL) {
        initOk = false;
        goto out;
    } else {
        gmosZigbeeRalEmberStackInstance = zigbeeStack;
    }

    // Ensure that the PSA cryptography library is initialised for
    // Zigbee protocol key storage. This function is safe to call
    // multiple times, so it does not matter if the PSA cryptography
    // library is also initialised elsewhere.
    if (!gmosCryptoPalInit ()) {
        initOk = false;
        goto out;
    }

    // Initialise the legacy buffer manager component.
    sli_legacy_buffer_manager_initialize_buffers ();

    // Initialise the token manager.
    slStatus = sl_token_manager_init ();
    GMOS_LOG_FMT (LOG_INFO,
        "EmberZNet initialised token manager with status 0x%04X.", slStatus);
    if (slStatus != SL_STATUS_OK) {
        initOk = false;
        goto out;
    }

    // Set up the RAIL power amplifier curves.
    sl_rail_util_pa_init ();

    // Enable PTI tracing if required.
    sl_rail_util_pti_init ();

    // Set stack configuration options.
    sl_zigbee_set_stack_profile (SL_ZIGBEE_STACK_PROFILE);
    sl_zigbee_set_security_level (SL_ZIGBEE_SECURITY_LEVEL);

    // Initialise the EmberZNet stack.
    slStatus = sl_zigbee_init ();
    GMOS_LOG_FMT (LOG_INFO,
        "EmberZNet initialised stack with status 0x%04X.", slStatus);
    if (slStatus != SL_STATUS_OK) {
        initOk = false;
        goto out;
    }

    // Initialise the concentrator if required.
    if (GMOS_CONFIG_ZIGBEE_CONCENTRATOR_NODE == true) {
        if (!gmosZigbeeRalConcentratorInit (zigbeeStack)) {
            initOk = false;
            goto out;
        }
    }

    // Initialise the sleepy node state machine if required.
    if ((GMOS_CONFIG_ZIGBEE_NODE_TYPE != GMOS_ZIGBEE_COORDINATOR_NODE) &&
        (GMOS_CONFIG_ZIGBEE_NODE_TYPE != GMOS_ZIGBEE_ROUTER_NODE)) {
        gmosZigbeeRalEmberSleepyNodeInit (zigbeeStack);
    }

    // Initialise the EmberZNet core task state machine.
    ralData->zigbeeStackPhase = ZIGBEE_STACK_PHASE_STARTUP;
    ralData->zigbeeStackState = ZIGBEE_STACK_STATE_STARTUP_IDLE;

    // Run the EmberZNet stack tick task.
    gmosZigbeeEmberStackTick_start (&(ralData->emberTickTask),
        zigbeeStack, "EmberZNet Tick");

    // Run the EmberZNet worker task.
    gmosZigbeeEmberStackWorker_start (&(ralData->emberWorkerTask),
        zigbeeStack, "EmberZNet Worker");

out:
    return initOk;
}
