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
 * This file implements the sleepy node state machine for the EmberZNet
 * Zigbee stack implementation.
 */

// This file only needs to be compiled for sleepy end devices.
#include "gmos-zigbee-config.h"
#if ((GMOS_CONFIG_ZIGBEE_NODE_TYPE != GMOS_ZIGBEE_COORDINATOR_NODE) && \
    (GMOS_CONFIG_ZIGBEE_NODE_TYPE != GMOS_ZIGBEE_ROUTER_NODE))

#include <stdint.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-scheduler.h"
#include "gmos-zigbee-stack.h"
#include "gmos-zigbee-ember-ral.h"
#include "sl_zigbee.h"

/*
 * Implement the parent device poll completion callback handler.
 */
void sl_zigbee_poll_complete_handler (sl_status_t slStatus)
{
    gmosZigbeeStack_t* zigbeeStack = gmosZigbeeRalEmberStackInstance;
    gmosZigbeeRalState_t* ralData = zigbeeStack->ralData;
    uint_fast8_t nextState = ralData->zigbeeSleepState;

    // Process poll completions from the poll check state.
    if (ralData->zigbeeSleepState ==
        ZIGBEE_STACK_STATE_SLEEPING_DATA_POLL_CHECK) {
        if (slStatus == SL_STATUS_OK) {
            nextState = ZIGBEE_STACK_STATE_SLEEPING_DATA_RECEIVED;
        } else if (slStatus == SL_STATUS_MAC_NO_DATA) {
            nextState = ZIGBEE_STACK_STATE_SLEEPING_NO_PENDING_DATA;
        } else {
            nextState = ZIGBEE_STACK_STATE_SLEEPING_DATA_POLL_RETRY;
        }
    }
    ralData->zigbeeSleepState = nextState;
}

/*
 * Implement the EmberZNet stack processing initialisation function for
 * sleepy devices.
 */
void gmosZigbeeRalEmberSleepyNodeInit (
    gmosZigbeeStack_t* zigbeeStack)
{
    gmosZigbeeRalState_t* ralData = zigbeeStack->ralData;
    ralData->zigbeeSleepState = ZIGBEE_STACK_STATE_SLEEPING_INIT;
    ralData->zigbeeNapCount = 0;
}

/*
 * Ensure that the EmberZNet stack on a sleepy device is powered up
 * prior to using any other stack functions.
 */
void gmosZigbeeRalEmberSleepyNodePowerUp (
    gmosZigbeeStack_t* zigbeeStack)
{
    gmosZigbeeRalState_t* ralData = zigbeeStack->ralData;
    if (ralData->zigbeeSleepState ==
        ZIGBEE_STACK_STATE_SLEEPING_POWERED_DOWN) {
        sl_zigbee_stack_power_up ();
        ralData->zigbeeSleepState =
            ZIGBEE_STACK_STATE_SLEEPING_POWERED_UP;
        gmosSchedulerTaskResume (&(ralData->emberTickTask));
    }
}

/*
 * Implement the EmberZNet stack processing tick function for sleepy
 * devices.
 */
gmosTaskStatus_t gmosZigbeeRalEmberSleepyNodeTick (
    gmosZigbeeStack_t* zigbeeStack)
{
    gmosZigbeeRalState_t* ralData = zigbeeStack->ralData;
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    uint_fast8_t stackPhase = ralData->zigbeeStackPhase;
    uint_fast8_t stackState = ralData->zigbeeStackState;
    uint_fast8_t nextState = ralData->zigbeeSleepState;
    sl_status_t slStatus;

    // Implement sleepy node state machine.
    switch (ralData->zigbeeSleepState) {

        // From the initialisation state wait until the startup phase is
        // complete.
        case ZIGBEE_STACK_STATE_SLEEPING_INIT :
            if (stackPhase != ZIGBEE_STACK_PHASE_STARTUP) {
                nextState = ZIGBEE_STACK_STATE_SLEEPING_POWERED_UP;
            }
            break;

        // If the EmberZNet stack is exiting deep sleep mode after a timed
        // sleep, a power up request is required before any subsequent
        // processing.
        case ZIGBEE_STACK_STATE_SLEEPING_POWERED_DOWN :
            sl_zigbee_stack_power_up ();
            nextState = ZIGBEE_STACK_STATE_SLEEPING_POWERED_UP;
            break;

        // After powering up the stack, determine whether active polling
        // of the parent device is required.
        case ZIGBEE_STACK_STATE_SLEEPING_POWERED_UP :
            if (stackPhase == ZIGBEE_STACK_PHASE_ACTIVE) {
                nextState = ZIGBEE_STACK_STATE_SLEEPING_DATA_POLL_REQ;
            } else {
                nextState = ZIGBEE_STACK_STATE_SLEEPING_NOT_JOINED;
            }
            break;

        // Select the appropriate stack behaviour during joining. This
        // will sleep during idle states and poll without hibernating
        // during link key update processing.
        case ZIGBEE_STACK_STATE_SLEEPING_NOT_JOINED :
            if (stackPhase == ZIGBEE_STACK_PHASE_JOINING) {
                switch (stackState) {
                    case ZIGBEE_STACK_STATE_JOINING_IDLE :
                    case ZIGBEE_STACK_STATE_JOINING_RETRY_WAIT :
                        if (sl_zigbee_ok_to_hibernate ()) {
                            nextState = ZIGBEE_STACK_STATE_SLEEPING_POWERED_DOWN;
                            taskStatus = GMOS_TASK_SUSPEND;
                        } else {
                            nextState = ZIGBEE_STACK_STATE_SLEEPING_POWERED_UP;
                        }
                        break;
                    case ZIGBEE_STACK_STATE_JOINING_KEY_UPDATE_CHECK :
                    case ZIGBEE_STACK_STATE_JOINING_LEAVE_NETWORK_CHECK :
                        ralData->zigbeeNapCount = 0;
                        nextState = ZIGBEE_STACK_STATE_SLEEPING_DATA_POLL_REQ;
                        break;
                    default :
                        nextState = ZIGBEE_STACK_STATE_SLEEPING_POWERED_UP;
                        break;
                }
            } else {
                nextState = ZIGBEE_STACK_STATE_SLEEPING_POWERED_UP;
            }
            break;

        // Poll the parent for data. This also works as an implicit check
        // on whether the device is still joined to a network. If there is
        // a resource limitation or the device needs to rejoin a network,
        // polling requests will continue in a tight loop.
        case ZIGBEE_STACK_STATE_SLEEPING_DATA_POLL_REQ :
            slStatus = sl_zigbee_poll_for_data ();
            if (slStatus == SL_STATUS_OK) {
                nextState = ZIGBEE_STACK_STATE_SLEEPING_DATA_POLL_CHECK;
            } else if (slStatus == SL_STATUS_NOT_JOINED) {
                nextState = ZIGBEE_STACK_STATE_SLEEPING_NOT_JOINED;
            } else if (slStatus == SL_STATUS_FAIL) {
                nextState = ZIGBEE_STACK_STATE_SLEEPING_DATA_POLL_RETRY;
            } else if ((slStatus != SL_STATUS_MAC_TRANSMIT_QUEUE_FULL) &&
                (slStatus != SL_STATUS_ALLOCATION_FAILED)) {
                nextState = ZIGBEE_STACK_STATE_SLEEPING_FAILED;
            }
            break;

        // Wait for the poll completion handler callback.
        case ZIGBEE_STACK_STATE_SLEEPING_DATA_POLL_CHECK :
            break;

        // Implement fast polling after receiving data from the parent
        // node.
        case ZIGBEE_STACK_STATE_SLEEPING_DATA_RECEIVED :
            if (sl_zigbee_ok_to_nap ()) {
                ralData->zigbeeNapCount = 0;
                nextState = ZIGBEE_STACK_STATE_SLEEPING_POWERED_DOWN;
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (
                    GMOS_CONFIG_ZIGBEE_FAST_POLL_INTERVAL));
            }
            break;

        // Implement extended polling after receiving no data from the
        // parent node.
        case ZIGBEE_STACK_STATE_SLEEPING_NO_PENDING_DATA :
            if (ralData->zigbeeNapCount < GMOS_CONFIG_ZIGBEE_SLOW_POLL_LIMIT) {
                if (sl_zigbee_ok_to_nap ()) {
                    ralData->zigbeeNapCount += 1;
                    nextState = ZIGBEE_STACK_STATE_SLEEPING_POWERED_DOWN;
                    taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (
                        GMOS_CONFIG_ZIGBEE_SLOW_POLL_INTERVAL));
                }
            } else {
                if (sl_zigbee_ok_to_hibernate ()) {
                    ralData->zigbeeNapCount = 0;
                    nextState = ZIGBEE_STACK_STATE_SLEEPING_POWERED_DOWN;
                    taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (
                        GMOS_CONFIG_ZIGBEE_HIBERNATE_INTERVAL));
                }
            }
            break;

        // Implement extended polling after a poll retry condition, which
        // typically implies that a network rejoin is required.
        // TODO: Limit the number of rejoin attempts.
        case ZIGBEE_STACK_STATE_SLEEPING_DATA_POLL_RETRY :
            if (ralData->zigbeeNapCount < GMOS_CONFIG_ZIGBEE_SLOW_POLL_LIMIT) {
                if (sl_zigbee_ok_to_nap ()) {
                    ralData->zigbeeNapCount += 1;
                    nextState = ZIGBEE_STACK_STATE_SLEEPING_POWERED_DOWN;
                    taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (
                        GMOS_CONFIG_ZIGBEE_SLOW_POLL_INTERVAL));
                }
            } else {
                if (sl_zigbee_ok_to_hibernate ()) {
                    ralData->zigbeeNapCount = 0;
                    nextState = ZIGBEE_STACK_STATE_SLEEPING_POWERED_DOWN;
                    taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (
                        GMOS_CONFIG_ZIGBEE_REJOIN_RETRY_INTERVAL));
                }
            }
            break;
    }

    // Skip further stack processing when stack is powering down. Note
    // that callbacks from the tick function can update this state
    // machine, so calling it needs to be the last thing that this
    // function does.
    ralData->zigbeeSleepState = nextState;
    if (nextState == ZIGBEE_STACK_STATE_SLEEPING_POWERED_DOWN) {
        sl_zigbee_stack_power_down ();
    } else {
        sl_zigbee_tick ();
    }
    return taskStatus;
}

#endif // GMOS_CONFIG_ZIGBEE_NODE_TYPE
