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
 * This file implements the network coordinator support functions for
 * the EmberZNet Zigbee stack implementation.
 */

// This file only needs to be compiled for the coordinator node.
#include "gmos-zigbee-config.h"
#if (GMOS_CONFIG_ZIGBEE_NODE_TYPE == GMOS_ZIGBEE_COORDINATOR_NODE)

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-zigbee-stack.h"
#include "gmos-zigbee-concentrator.h"
#include "gmos-zigbee-zdo-client.h"
#include "gmos-zigbee-ember-ral.h"
#include "sl_zigbee.h"
#include "zigbee-security-manager.h"
#include "app/util/security/security.h"

// Specify whether device joining should be enabled for just the
// coordinator, or whether joining should be enabled for all router
// nodes in the network.
#define ZIGBEE_COORDINATOR_NETWORK_JOIN_ALL_ROUTERS true

// Specify the network joining request broadcast interval in seconds.
#define ZIGBEE_COORDINATOR_NETWORK_JOIN_INTERVAL 60

// Specify the network joining request timeout period in seconds.
#define ZIGBEE_COORDINATOR_NETWORK_JOIN_TIMEOUT 90

/*
 * This enumeration defines the state space used by the EmberZNet
 * Zigbee coordinator state machine.
 */
typedef enum {
    ZIGBEE_COORDINATOR_STATE_IDLE,
    ZIGBEE_COORDINATOR_STATE_FAILED,
    ZIGBEE_COORDINATOR_STATE_JOINING
} zigbeeCoordinatorState_t;

/*
 * Implement processing for trust centre join requests.
 */
static inline sl_zigbee_join_decision_t trustCenterJoinHandler (
    gmosZigbeeStack_t* zigbeeStack, uint16_t newNodeId,
    uint8_t* newNodeEui64, uint16_t parentNodeId)
{
    gmosZigbeeRalState_t* ralData = zigbeeStack->ralData;
    uint_fast8_t joiningMode = ralData->phase.active.tcJoiningMode;
    sl_zigbee_join_decision_t joinDecision = SL_ZIGBEE_NO_ACTION;

    // Always deny joining if the coordinator is not in the joining
    // enable state.
    if (ralData->phase.active.coordinatorState !=
        ZIGBEE_COORDINATOR_STATE_JOINING) {
        joinDecision = SL_ZIGBEE_DENY_JOIN;
    }

    // Process join requests while in preset or temporary link key
    // joining mode, where link keys have been preassigned to the
    // joining devices.
    else if ((joiningMode == GMOS_ZIGBEE_JOINING_MODE_PRESET_LINK_KEY) ||
        (joiningMode == GMOS_ZIGBEE_JOINING_MODE_TEMPORARY_LINK_KEY)) {
        joinDecision = SL_ZIGBEE_USE_PRECONFIGURED_KEY;
    }

    // Add the new device to the concentrator device tables and attempt
    // to bootstrap the source routing table.
    if ((joinDecision != SL_ZIGBEE_DENY_JOIN) &&
        (joinDecision != SL_ZIGBEE_NO_ACTION)) {
        if (gmosZigbeeConcentratorStoreAddress (
            zigbeeStack, newNodeId, newNodeEui64)) {
            GMOS_LOG_FMT (LOG_DEBUG,
                "EmberZNet stored address for node 0x%04X.",
                newNodeId);
            if (gmosZigbeeConcentratorBootstrapRoute (
                zigbeeStack, newNodeId, parentNodeId)) {
                GMOS_LOG_FMT (LOG_DEBUG,
                    "EmberZNet bootstrap route for node 0x%04X via 0x%04X.",
                    newNodeId, parentNodeId);
            }
        } else {
            joinDecision = SL_ZIGBEE_DENY_JOIN;
        }
    }
    return joinDecision;
}

/*
 * Implement processing for trust centre secured rejoin requests.
 */
static inline sl_zigbee_join_decision_t trustCenterSecuredRejoinHandler (
    gmosZigbeeStack_t* zigbeeStack, sl_802154_short_addr_t newNodeId,
    sl_802154_long_addr_t newNodeEui64, uint16_t parentNodeId)
{
    // Update the device address in the concentrator device tables and
    // attempt to bootstrap the source routing table using the new
    // parent node ID.
    if (gmosZigbeeConcentratorStoreAddress (
        zigbeeStack, newNodeId, newNodeEui64)) {
        GMOS_LOG_FMT (LOG_DEBUG,
            "EmberZNet updated address tables for node 0x%04X.",
            newNodeId);
        if (gmosZigbeeConcentratorBootstrapRoute (
            zigbeeStack, newNodeId, parentNodeId)) {
            GMOS_LOG_FMT (LOG_DEBUG,
                "EmberZNet bootstrap route for node 0x%04X via 0x%04X.",
                newNodeId, parentNodeId);
        }
    }

    // No trust center action is required on a secured rejoin.
    return SL_ZIGBEE_NO_ACTION;
}

/*
 * Implement processing for trust centre unsecured rejoin requests.
 */
static inline sl_zigbee_join_decision_t trustCenterUnsecuredRejoinHandler (
    gmosZigbeeStack_t* zigbeeStack, sl_802154_short_addr_t newNodeId,
    sl_802154_long_addr_t newNodeEui64, uint16_t parentNodeId)
{
    gmosZigbeeRalState_t* ralData = zigbeeStack->ralData;
    uint_fast8_t joiningMode = ralData->phase.active.tcJoiningMode;
    sl_zigbee_join_decision_t joinDecision = SL_ZIGBEE_NO_ACTION;

    // Process unsecured rejoin requests unless explicitly disabled.
    // Rejoining devices are expected to use their existing link keys.
    if (joiningMode != GMOS_ZIGBEE_JOINING_MODE_DISALLOW_ALL) {
        joinDecision = SL_ZIGBEE_USE_PRECONFIGURED_KEY;
    }

    // Update the new device in the concentrator device tables and
    // attempt to bootstrap the source routing table.
    if (joinDecision != SL_ZIGBEE_NO_ACTION) {
        if (gmosZigbeeConcentratorStoreAddress (
            zigbeeStack, newNodeId, newNodeEui64)) {
            GMOS_LOG_FMT (LOG_DEBUG,
                "EmberZNet updated address tables for node 0x%04X.",
                newNodeId);
            if (gmosZigbeeConcentratorBootstrapRoute (
                zigbeeStack, newNodeId, parentNodeId)) {
                GMOS_LOG_FMT (LOG_DEBUG,
                    "EmberZNet bootstrap route for node 0x%04X via 0x%04X.",
                    newNodeId, parentNodeId);
            }
        } else {
            joinDecision = SL_ZIGBEE_NO_ACTION;
        }
    }
    return joinDecision;
}

/*
 * Implement processing for trust centre leave requests.
 */
static inline sl_zigbee_join_decision_t trustCenterLeaveHandler (
    gmosZigbeeStack_t* zigbeeStack, sl_802154_short_addr_t newNodeId,
    sl_802154_long_addr_t newNodeEui64)
{
    // Delete table entries by node ID and EUI64 for good measure.
    gmosZigbeeConcentratorDeleteNodeId (zigbeeStack, newNodeId);
    gmosZigbeeConcentratorDeleteAddress (zigbeeStack, newNodeEui64);
    return SL_ZIGBEE_NO_ACTION;
}

/*
 * Implement the EmberZNet trust center join handler which is used to
 * determine which devices are allowed to join the network.
 */
sl_zigbee_join_decision_t sl_zigbee_internal_trust_center_join_handler (
    sl_802154_short_addr_t newNodeId, sl_802154_long_addr_t newNodeEui64,
    sl_zigbee_device_update_t update, sl_802154_short_addr_t parentNodeId)
{
    gmosZigbeeStack_t* zigbeeStack = gmosZigbeeRalEmberStackInstance;
    gmosZigbeeRalState_t* ralData = zigbeeStack->ralData;
    sl_zigbee_join_decision_t joinDecision;

    // Trust centre processing should only be carried out in the active
    // ready stack state.
    if ((ralData->zigbeeStackPhase != ZIGBEE_STACK_PHASE_ACTIVE) ||
        (ralData->zigbeeStackState != ZIGBEE_STACK_STATE_ACTIVE_READY)) {
        joinDecision = SL_ZIGBEE_NO_ACTION;
    }

    // Select the trust centre action based on the device update type.
    else switch (update) {

        // Unsecured join covers all joining scenarios.
        case SL_ZIGBEE_STANDARD_SECURITY_UNSECURED_JOIN :
            joinDecision = trustCenterJoinHandler (
                zigbeeStack, newNodeId, newNodeEui64, parentNodeId);
            break;

        // Unsecured rejoin implies that the device is rejoining without
        // a valid network key.
        case SL_ZIGBEE_STANDARD_SECURITY_UNSECURED_REJOIN :
            joinDecision = trustCenterUnsecuredRejoinHandler (
                zigbeeStack, newNodeId, newNodeEui64, parentNodeId);
            break;

        // Secured rejoin implies that the device is rejoining with a
        // valid network key.
        case SL_ZIGBEE_STANDARD_SECURITY_SECURED_REJOIN :
            joinDecision = trustCenterSecuredRejoinHandler (
                zigbeeStack, newNodeId, newNodeEui64, parentNodeId);
            break;

        // On leaving the device should be removed from all concentrator
        // and trust center tables.
        case SL_ZIGBEE_DEVICE_LEFT :
            joinDecision = trustCenterLeaveHandler (
                zigbeeStack, newNodeId, newNodeEui64);
            break;
        default :
            joinDecision = SL_ZIGBEE_NO_ACTION;
            break;
    }

    // Trace requests for debugging purposes.
    GMOS_LOG_FMT (LOG_VERBOSE, "EmberZNet trust centre "
        "join request from EUI64 %02X%02X%02X%02X%02X%02X%02X%02X",
        newNodeEui64 [0], newNodeEui64 [1], newNodeEui64 [2], newNodeEui64 [3],
        newNodeEui64 [4], newNodeEui64 [5], newNodeEui64 [6], newNodeEui64 [7]);
    GMOS_LOG_FMT (LOG_VERBOSE,
        "  New Node ID %04X, Parent Node ID %04X, Update %02X, Decision %02X.",
        newNodeId, parentNodeId, update, joinDecision);
    return joinDecision;
}

/*
 * Enable device joining for the specified joining period. This variant
 * of the function only enables network association for the coordinator
 * node.
 */
#if (ZIGBEE_COORDINATOR_NETWORK_JOIN_ALL_ROUTERS == false)
static inline bool gmosZigbeeRalJoiningEnable (
    gmosZigbeeStack_t* zigbeeStack, uint_fast8_t joiningPeriod)
{
    (void) zigbeeStack;
    sl_status_t slStatus = sl_zigbee_permit_joining (joiningPeriod);
    GMOS_LOG_FMT (LOG_VERBOSE,
        "EmberZNet set local joining for %d seconds (status 0x%04X)",
        joiningPeriod, slStatus);
    return (slStatus == SL_STATUS_OK) ? true : false;
}
#endif

/*
 * Enable device joining for the specified joining period. This variant
 * of the function enables network association for all routers on the
 * network by broadcasting the appropriate ZDO command.
 */
#if (ZIGBEE_COORDINATOR_NETWORK_JOIN_ALL_ROUTERS == true)
static inline bool gmosZigbeeRalJoiningEnable (
    gmosZigbeeStack_t* zigbeeStack, uint_fast8_t joiningPeriod)
{
    bool zdoSendOk = gmosZigbeeZdoClientPermitJoiningBroadcast (
        zigbeeStack->zdoClient, joiningPeriod);
    GMOS_LOG_FMT (LOG_VERBOSE,
        "EmberZNet set global joining for %d seconds (status %d)",
        joiningPeriod, zdoSendOk);
    return zdoSendOk;
}
#endif

/*
 * Check for network joining timeout conditions.
 */
static inline gmosTaskStatus_t gmosZigbeeRalJoiningTimeouts (
    gmosZigbeeStack_t* zigbeeStack, zigbeeCoordinatorState_t* nextState)
{
    gmosZigbeeRalState_t* ralData = zigbeeStack->ralData;
    uint32_t currentTimer = gmosPalGetTimer ();
    int32_t windowDelay;
    int32_t refreshDelay;
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;

    // Calculate the joining window and refresh timeout delays.
    windowDelay = (int32_t)
        (ralData->phase.active.tcJoiningTimeout - currentTimer);
    refreshDelay = (int32_t)
        (ralData->phase.active.netJoiningTimeout - currentTimer);

    // Check for expiry of the trust centre joining timeout. This will
    // revert the trust centre joining mode to the default policy and
    // attempt to cancel network association.
    if (windowDelay <= 0) {
        ralData->phase.active.tcJoiningMode =
            GMOS_ZIGBEE_JOINING_MODE_REJOINS_ONLY;
        sl_zigbee_set_trust_center_link_key_request_policy (
            SL_ZIGBEE_DENY_TC_LINK_KEY_REQUESTS);
        if (gmosZigbeeRalJoiningEnable (zigbeeStack, 0)) {
            *nextState = ZIGBEE_COORDINATOR_STATE_IDLE;
        } else {
            ralData->phase.active.tcJoiningTimeout =
                currentTimer + GMOS_MS_TO_TICKS (5);
        }
    }

    // Check for expiry of the network association refresh timeout.
    else if (refreshDelay <= 0) {
        if (gmosZigbeeRalJoiningEnable (zigbeeStack,
            ZIGBEE_COORDINATOR_NETWORK_JOIN_TIMEOUT)) {
            ralData->phase.active.netJoiningTimeout =
                currentTimer + GMOS_MS_TO_TICKS (1000 *
                ZIGBEE_COORDINATOR_NETWORK_JOIN_INTERVAL);
        } else {
            ralData->phase.active.netJoiningTimeout =
                currentTimer + GMOS_MS_TO_TICKS (50);
        }
    }

    // If no timeouts have occurred, select the shortest remaining delay
    // before evaluating them again.
    else {
        taskStatus = gmosSchedulerPrioritise (
            GMOS_TASK_RUN_LATER ((uint32_t) windowDelay),
            GMOS_TASK_RUN_LATER ((uint32_t) refreshDelay));
    }
    return taskStatus;
}

/*
 * Initialises the Zigbee coordinator state machine for Zigbee nodes
 * that are configured as network coordinators.
 */
void gmosZigbeeRalCoordinatorInit (gmosZigbeeStack_t* zigbeeStack)
{
    gmosZigbeeRalState_t* ralData = zigbeeStack->ralData;
    ralData->phase.active.coordinatorState = ZIGBEE_COORDINATOR_STATE_IDLE;
    sl_zigbee_set_trust_center_link_key_request_policy (
        SL_ZIGBEE_DENY_TC_LINK_KEY_REQUESTS);

    // The trust centre is allowed to use all available entries in the
    // stack address table. If support for application level access to
    // the stack address table is implemented at a later date, this will
    // need to be changed so that entries from the application and trust
    // centre do not conflict.
    securityAddressCacheInit (0, SL_ZIGBEE_ADDRESS_TABLE_SIZE);
}

/*
 * Processes the Zigbee coordinator state machine while in the network
 * active state.
 */
gmosTaskStatus_t gmosZigbeeRalCoordinatorTick (
    gmosZigbeeStack_t* zigbeeStack)
{
    gmosZigbeeRalState_t* ralData = zigbeeStack->ralData;
    zigbeeCoordinatorState_t nextState = ralData->phase.active.coordinatorState;
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;

    // Implement the network coordinator state machine.
    switch (ralData->phase.active.coordinatorState) {

        // Idle the coordinator state machine until an external request.
        case ZIGBEE_COORDINATOR_STATE_IDLE :
            taskStatus = GMOS_TASK_SUSPEND;
            break;

        // Manage the various coordinator device joining timeouts.
        case ZIGBEE_COORDINATOR_STATE_JOINING :
            taskStatus = gmosZigbeeRalJoiningTimeouts (
                zigbeeStack, &nextState);
            break;

        // Force the main state machine into its failure state if
        // required.
        default :
            ralData->zigbeeStackState = ZIGBEE_STACK_STATE_ACTIVE_FAILED;
            break;
    }
    ralData->phase.active.coordinatorState = nextState;
    return taskStatus;
}

/*
 * Enables device joining for the Zigbee network. This capability is
 * only supported for coordinator nodes that have previously formed an
 * active network.
 */
gmosZigbeeStatus_t gmosZigbeeSetJoiningMode (
    gmosZigbeeStack_t* zigbeeStack, gmosZigbeeJoiningMode_t joiningMode,
    uint32_t joiningTimeout, uint8_t* joinerKey, uint8_t* joinerEui64)
{
    gmosZigbeeRalState_t* ralData = zigbeeStack->ralData;
    zigbeeCoordinatorState_t currentState =
        ralData->phase.active.coordinatorState;
    uint32_t currentTimer = gmosPalGetTimer ();
    sl_zigbee_tc_link_key_request_policy_t keyReqPolicy;
    sl_status_t slStatus;
    gmosZigbeeStatus_t status = GMOS_ZIGBEE_STATUS_SUCCESS;

    // The joining process can only be initiated from the active ready
    // stack state.
    if ((ralData->zigbeeStackPhase != ZIGBEE_STACK_PHASE_ACTIVE) ||
        (ralData->zigbeeStackState != ZIGBEE_STACK_STATE_ACTIVE_READY)) {
        status = GMOS_ZIGBEE_STATUS_INVALID_CALL;
        goto out;
    }

    // The joining process can be initiated from the coordinator idle
    // state or updated in the coordinator joining state.
    if ((currentState != ZIGBEE_COORDINATOR_STATE_IDLE) &&
        (currentState != ZIGBEE_COORDINATOR_STATE_JOINING)) {
        status = GMOS_ZIGBEE_STATUS_INVALID_CALL;
        goto out;
    }

    // Check whether a temporary link key is required.
    if (joiningMode == GMOS_ZIGBEE_JOINING_MODE_TEMPORARY_LINK_KEY) {
        if (joinerKey == NULL) {
            status = GMOS_ZIGBEE_STATUS_INVALID_ARGUMENT;
            goto out;
        }
    }

    // Set the device joining mode to be used and the timestamp for the
    // end of the device joining window. The window is set to zero if
    // joining is being disabled.
    ralData->phase.active.tcJoiningMode = joiningMode;
    if ((joiningMode == GMOS_ZIGBEE_JOINING_MODE_PRESET_LINK_KEY) ||
        (joiningMode == GMOS_ZIGBEE_JOINING_MODE_TEMPORARY_LINK_KEY)) {
        ralData->phase.active.tcJoiningTimeout = currentTimer +
            GMOS_MS_TO_TICKS (joiningTimeout * 1000);
    } else {
        ralData->phase.active.tcJoiningTimeout = currentTimer;
    }

    // Establish the temporary link key if required.
    if (joiningMode == GMOS_ZIGBEE_JOINING_MODE_TEMPORARY_LINK_KEY) {
        uint8_t wildcardEui64 [] =
            { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
        uint8_t* activeEui64 =
            (joinerEui64 == NULL) ? wildcardEui64 : joinerEui64;
        sl_zigbee_set_transient_key_timeout_s (joiningTimeout);
        slStatus = sl_zigbee_sec_man_import_transient_key (
            activeEui64, (sl_zigbee_sec_man_key_t*) joinerKey);
        if (slStatus != SL_STATUS_OK) {
            status = GMOS_ZIGBEE_STATUS_RETRY;
            goto out;
        }
    }

    // Set the policy to use for distributing trust centre link keys.
    if (joiningMode == GMOS_ZIGBEE_JOINING_MODE_TEMPORARY_LINK_KEY) {
        keyReqPolicy =
            SL_ZIGBEE_ALLOW_TC_LINK_KEY_REQUEST_AND_SEND_CURRENT_KEY;
    } else {
        keyReqPolicy = SL_ZIGBEE_DENY_TC_LINK_KEY_REQUESTS;
    }
    sl_zigbee_set_trust_center_link_key_request_policy (keyReqPolicy);

    // Start the joining process from the idle state.
    if (currentState == ZIGBEE_COORDINATOR_STATE_IDLE) {
        ralData->phase.active.netJoiningTimeout = currentTimer;
        ralData->phase.active.coordinatorState =
            ZIGBEE_COORDINATOR_STATE_JOINING;
    }
    gmosSchedulerTaskResume (&(ralData->emberWorkerTask));

    // Return request status.
out :
    return status;
}

/*
 * Gets the current device joining mode in use by the Zigbee network.
 * This capability is only supported for coordinator nodes that have
 * previously formed an active network.
 */
gmosZigbeeJoiningMode_t gmosZigbeeGetJoiningMode (
    gmosZigbeeStack_t* zigbeeStack)
{
    gmosZigbeeRalState_t* ralData = zigbeeStack->ralData;

    // The joining mode can only be accessed from the active ready
    // stack state.
    if ((ralData->zigbeeStackPhase != ZIGBEE_STACK_PHASE_ACTIVE) ||
        (ralData->zigbeeStackState != ZIGBEE_STACK_STATE_ACTIVE_READY)) {
        return GMOS_ZIGBEE_JOINING_MODE_DISALLOW_ALL;
    }

    // Return the currently active coordinator joining mode.
    return ralData->phase.active.tcJoiningMode;
}

#endif // GMOS_CONFIG_ZIGBEE_NODE_TYPE
