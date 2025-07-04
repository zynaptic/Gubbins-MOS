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
 * This file implements the network active state machine for the
 * EmberZNet Zigbee stack implementation.
 */

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-scheduler.h"
#include "gmos-zigbee-config.h"
#include "gmos-zigbee-stack.h"
#include "gmos-zigbee-ember-ral.h"
#include "sl_zigbee.h"
#include "stack-info.h"

/*
 * Implement the EmberZNet active network state machine.
 */
gmosTaskStatus_t gmosZigbeeRalEmberActivePhase (
    gmosZigbeeStack_t* zigbeeStack)
{
    gmosZigbeeRalState_t* ralData = zigbeeStack->ralData;
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    zigbeeStackPhase_t nextPhase = ralData->zigbeeStackPhase;
    zigbeeStackStateActive_t nextState = ralData->zigbeeStackState;
    sl_status_t slStatus;

    // Implement the active network state machine.
    switch (ralData->zigbeeStackState) {

        // From the idle state, initialise the local state variables and
        // then enter the ready state for subsequent command processing.
        case ZIGBEE_STACK_STATE_ACTIVE_IDLE :
            gmosZigbeeSetNetworkState (zigbeeStack,
                GMOS_ZIGBEE_NETWORK_STATE_CONNECTED);
            nextState = ZIGBEE_STACK_STATE_ACTIVE_READY;

            // Initialise network coordinator state variables.
            #if (GMOS_CONFIG_ZIGBEE_NODE_TYPE == GMOS_ZIGBEE_COORDINATOR_NODE)
            gmosZigbeeRalCoordinatorInit (zigbeeStack);
            #endif
            break;

        // In the active state process the state machines for the
        // network coordinator and message concentrators if required.
        case ZIGBEE_STACK_STATE_ACTIVE_READY :
            taskStatus = GMOS_TASK_SUSPEND;

            // Run network coordinator state machine if required.
            #if (GMOS_CONFIG_ZIGBEE_NODE_TYPE == GMOS_ZIGBEE_COORDINATOR_NODE)
            taskStatus = gmosSchedulerPrioritise (taskStatus,
                gmosZigbeeRalCoordinatorTick (zigbeeStack));
            #endif
            break;

        // Initiate the network leaving process.
        case ZIGBEE_STACK_STATE_ACTIVE_LEAVE_PENDING :
            gmosZigbeeSetNetworkState (zigbeeStack,
                GMOS_ZIGBEE_NETWORK_STATE_LEAVING);
            nextState = ZIGBEE_STACK_STATE_ACTIVE_LEAVE_NETWORK_REQ;
            break;

        // Issue the leave network request.
        case ZIGBEE_STACK_STATE_ACTIVE_LEAVE_NETWORK_REQ :
            slStatus = sl_zigbee_leave_network (
                SL_ZIGBEE_LEAVE_NWK_WITH_NO_OPTION);
            if (slStatus == SL_STATUS_OK) {
                nextState = ZIGBEE_STACK_STATE_ACTIVE_LEAVE_NETWORK_CHECK;
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (10));
            } else {
                nextState = ZIGBEE_STACK_STATE_ACTIVE_FAILED;
            }
            break;

        // Poll network state until the network is shut down.
        case ZIGBEE_STACK_STATE_ACTIVE_LEAVE_NETWORK_CHECK :
            if (sl_zigbee_stack_is_up ()) {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (10));
            } else if (GMOS_CONFIG_ZIGBEE_NODE_TYPE ==
                GMOS_ZIGBEE_COORDINATOR_NODE) {
                nextPhase = ZIGBEE_STACK_PHASE_FORMING;
                nextState = ZIGBEE_STACK_STATE_ACTIVE_IDLE;
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (1000));
            } else {
                nextPhase = ZIGBEE_STACK_PHASE_JOINING;
                nextState = ZIGBEE_STACK_STATE_ACTIVE_IDLE;
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (1000));
            }
            break;

        // On failure enter the stack failure state.
        default :
            nextPhase = ZIGBEE_STACK_PHASE_FAILED;
            nextState = ZIGBEE_STACK_STATE_ACTIVE_FAILED;
            break;
    }
    ralData->zigbeeStackPhase = nextPhase;
    ralData->zigbeeStackState = nextState;
    return taskStatus;
}

/*
 * Initiates the network leaving process. This causes the specified
 * Zigbee radio interface to be disconnected from the current network.
 */
gmosZigbeeStatus_t gmosZigbeeLeaveNetwork (
    gmosZigbeeStack_t* zigbeeStack)
{
    gmosZigbeeRalState_t* ralData = zigbeeStack->ralData;

    // Indicate that the network is not currently active.
    if ((ralData->zigbeeStackPhase != ZIGBEE_STACK_PHASE_ACTIVE) ||
        (ralData->zigbeeStackState != ZIGBEE_STACK_STATE_ACTIVE_READY)) {
        return GMOS_ZIGBEE_STATUS_INVALID_CALL;
    }

    // Force the state machine into the leave pending state.
    ralData->zigbeeStackState = ZIGBEE_STACK_STATE_ACTIVE_LEAVE_PENDING;
    gmosSchedulerTaskResume (&(ralData->emberWorkerTask));
    return GMOS_ZIGBEE_STATUS_SUCCESS;
}
