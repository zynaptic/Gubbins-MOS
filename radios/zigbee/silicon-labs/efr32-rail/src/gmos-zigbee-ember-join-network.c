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
 * This file implements the network joining state machine for the
 * EmberZNet Zigbee stack implementation. This will be used for all
 * nodes apart from coordinator node implementations.
 */

// This file does not need to be compiled for the coordinator node.
#include "gmos-zigbee-config.h"
#if (GMOS_CONFIG_ZIGBEE_NODE_TYPE != GMOS_ZIGBEE_COORDINATOR_NODE)

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-zigbee-stack.h"
#include "gmos-zigbee-ember-ral.h"
#include "sl_zigbee.h"
#include "stack-info.h"
#include "security.h"

// Specify the dwell time per channel when performing an active network
// scan. Expressed as IEEE MAC scan duration units.
#define ACTIVE_SCAN_DWELL_TIME 8

/*
 * Checks for a valid extended PAN ID. This rejects the reserved
 * extended PAN ID values of all zeros or all ones.
 */
static bool checkExtendedPanId (uint8_t* extendedPanId)
{
    uint_fast8_t i;
    bool extendedPanIdOk = true;

    if ((extendedPanId [0] == 0xFF) || (extendedPanId [0] == 0x00)) {
        extendedPanIdOk = false;
        for (i = 1; i < GMOS_ZIGBEE_EXTENDED_PAN_ID_SIZE; i++) {
            if (extendedPanId [0] != extendedPanId [i]) {
                extendedPanIdOk = true;
                break;
            }
        }
    }
    return extendedPanIdOk;
}

/*
 * Implement the EmberZNet active scan network found handler.
 */
void sl_zigbee_network_found_handler (
    sl_zigbee_zigbee_network_t* networkFound, uint8_t lqi, int8_t rssi)
{
    gmosZigbeeStack_t* zigbeeStack = gmosZigbeeRalEmberStackInstance;
    gmosZigbeeRalState_t* ralData = zigbeeStack->ralData;
    zigbeeStackPhase_t phase = ralData->zigbeeStackPhase;
    zigbeeStackStateJoining_t state = ralData->zigbeeStackState;
    uint_fast8_t nwkChannel;
    uint_fast16_t nwkPanId;
    uint8_t* nwkExtPanId;
    bool nwkAllowingJoins;
    uint_fast8_t nwkStackProfile;
    bool nwkIsJoinable;
    uint_fast8_t nwkMatchValue;
    uint_fast8_t i, j;

    // Extract the fields of interest from the callback data.
    nwkChannel = networkFound->channel;
    nwkPanId = networkFound->panId;
    nwkExtPanId = networkFound->extendedPanId;
    nwkAllowingJoins = networkFound->allowingJoin;
    nwkStackProfile = networkFound->stackProfile;

    // Determine if the specified network is a joinable network.
    nwkIsJoinable = ((nwkStackProfile == SL_ZIGBEE_STACK_PROFILE) &&
        (nwkAllowingJoins != 0)) ? true : false;

    // Handle callback processing in the active scan state.
    if ((phase == ZIGBEE_STACK_PHASE_JOINING) &&
        (state == ZIGBEE_STACK_STATE_JOINING_ACTIVE_SCAN_CHECK)) {

        // If required, reject networks that do not have a matching
        // extended PAN ID.
        if (ralData->phase.join.extPanIdMatch) {
            for (i = 0; i < GMOS_ZIGBEE_EXTENDED_PAN_ID_SIZE; i++) {
                if (nwkExtPanId [i] != zigbeeStack->extendedPanId [i]) {
                    nwkIsJoinable = false;
                    break;
                }
            }
        }

        // Log network information for debug purposes. Verbose logging
        // includes all the available networks and standard logging only
        // includes valid candidate networks.
        if ((GMOS_CONFIG_LOG_LEVEL == LOG_VERBOSE) || nwkIsJoinable) {
            GMOS_LOG_FMT (LOG_DEBUG,
                "EmberZNet found network PAN ID 0x%04X on channel %d.",
                nwkPanId, nwkChannel);
            GMOS_LOG_FMT (LOG_VERBOSE,
                "  Extended PAN ID is %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
                nwkExtPanId [7], nwkExtPanId [6], nwkExtPanId [5], nwkExtPanId [4],
                nwkExtPanId [3], nwkExtPanId [2], nwkExtPanId [1], nwkExtPanId [0]);
            GMOS_LOG_FMT (LOG_VERBOSE,
                "  Signal strength is %d LQI, %ddBm RSSI.", lqi, rssi);
            GMOS_LOG_FMT (LOG_VERBOSE,
                "  Stack profile is %d, joining flag is %d.",
                nwkStackProfile, nwkAllowingJoins);
        }

        // Skip further processing if the network is not joinable.
        if (!nwkIsJoinable) {
            return;
        }

        // Count the number of joinable networks to use in the retry
        // backoff calculations.
        if (ralData->phase.join.networkCount < 0xFF) {
            ralData->phase.join.networkCount += 1;
        }

        // Calculate the Hamming distance between the random bytes and
        // the network extended PAN ID.
        nwkMatchValue = 0;
        for (i = 0; i < GMOS_ZIGBEE_EXTENDED_PAN_ID_SIZE; i++) {
            uint_fast8_t xorBits = nwkExtPanId [i] ^
                ralData->phase.join.randomBytes [i];
            for (j = 0; j < 8; j ++) {
                nwkMatchValue += (xorBits & 1);
                xorBits >>= 1;
            }
        }

        // If the new network extended PAN ID is closer to the random
        // bytes, update the stored network information.
        if (nwkMatchValue < ralData->phase.join.bestMatch) {
            GMOS_LOG_FMT (LOG_VERBOSE,
                "  Setting network as join candidate (match value %d).",
                nwkMatchValue);
            ralData->phase.join.bestMatch = nwkMatchValue;
            zigbeeStack->currentChannelId = nwkChannel;
            zigbeeStack->currentPanId = nwkPanId;
            if (!ralData->phase.join.extPanIdMatch) {
                for (i = 0; i < GMOS_ZIGBEE_EXTENDED_PAN_ID_SIZE; i++) {
                    zigbeeStack->extendedPanId [i] = nwkExtPanId [i];
                }
            }
        }
    }
}

/*
 * Implement EmberZNet stack scan completion handler.
 */
void sl_zigbee_scan_complete_handler (
    uint8_t channel, sl_status_t slStatus)
{
    gmosZigbeeStack_t* zigbeeStack = gmosZigbeeRalEmberStackInstance;
    gmosZigbeeRalState_t* ralData = zigbeeStack->ralData;
    zigbeeStackPhase_t phase = ralData->zigbeeStackPhase;
    zigbeeStackStateJoining_t state = ralData->zigbeeStackState;

    // Handle callback processing in the active scan state.
    if ((phase == ZIGBEE_STACK_PHASE_JOINING) &&
        (state == ZIGBEE_STACK_STATE_JOINING_ACTIVE_SCAN_CHECK)) {

        // Check for completion of the scan.
        if (slStatus == SL_STATUS_OK) {
            if (ralData->phase.join.networkCount == 0) {
                state = ZIGBEE_STACK_STATE_JOINING_RETRY_BACKOFF;
            } else {
                state = ZIGBEE_STACK_STATE_JOINING_JOIN_NETWORK_REQ;
            }
            ralData->zigbeeStackState = state;
            gmosSchedulerTaskResume (&(ralData->emberWorkerTask));
        }

        // Log scan errors if required.
        else {
            GMOS_LOG_FMT (LOG_DEBUG,
                "EmberZNet active scan failed (channel %d, status 0x%04X).",
                channel, slStatus);
        }
    }
}

/*
 * Implement stack status handler for receiving network join status
 * notifications.
 */
void sl_zigbee_stack_status_handler (sl_status_t slStatus)
{
    gmosZigbeeStack_t* zigbeeStack = gmosZigbeeRalEmberStackInstance;
    gmosZigbeeRalState_t* ralData = zigbeeStack->ralData;
    zigbeeStackPhase_t phase = ralData->zigbeeStackPhase;
    zigbeeStackStateJoining_t state = ralData->zigbeeStackState;

    // Process status notifications while waiting for the network
    // joining process to complete.
    if ((phase == ZIGBEE_STACK_PHASE_JOINING) &&
        (state == ZIGBEE_STACK_STATE_JOINING_JOIN_NETWORK_CHECK)) {
        if (slStatus == SL_STATUS_NETWORK_UP) {
            zigbeeStack->currentNodeId = sl_zigbee_get_node_id ();
            state = ZIGBEE_STACK_STATE_JOINING_JOIN_NETWORK_DONE;
        } else {
            GMOS_LOG_FMT (LOG_DEBUG,
                "EmberZNet joining failed with status 0x%04X.",
                slStatus);
            state = ZIGBEE_STACK_STATE_JOINING_RETRY_BACKOFF;
        }
        ralData->zigbeeStackState = state;
        gmosSchedulerTaskResume (&(ralData->emberWorkerTask));
    }
}

/*
 * Send the network joining request.
 */
static inline bool gmosZigbeeRalEmberJoinNetworkRequest (
    gmosZigbeeStack_t* zigbeeStack)
{
    sl_status_t slStatus;
    sl_zigbee_node_type_t nodeType;
    sl_zigbee_network_parameters_t netParams;
    uint_fast8_t i;

    // Populate the network information data structure.
    netParams.panId = zigbeeStack->currentPanId;
    netParams.radioChannel = zigbeeStack->currentChannelId;
    netParams.joinMethod = SL_ZIGBEE_USE_MAC_ASSOCIATION;
    netParams.nwkManagerId = 0x0000;
    netParams.nwkUpdateId = 0x00;
    netParams.channels = zigbeeStack->channelMask;
    netParams.radioTxPower = GMOS_CONFIG_ZIGBEE_DEFAULT_TX_POWER;
    for (i = 0; i < EXTENDED_PAN_ID_SIZE; i++) {
        netParams.extendedPanId [i] = zigbeeStack->extendedPanId [i];
    }

    // Select the node type.
    switch (GMOS_CONFIG_ZIGBEE_NODE_TYPE) {
        case GMOS_ZIGBEE_ACTIVE_CHILD_NODE :
            nodeType = SL_ZIGBEE_END_DEVICE;
            break;
        case GMOS_ZIGBEE_SLEEPY_CHILD_NODE :
        case GMOS_ZIGBEE_MOBILE_CHILD_NODE :
            nodeType = SL_ZIGBEE_SLEEPY_END_DEVICE;
            break;
        default :
            nodeType = SL_ZIGBEE_ROUTER;
            break;
    }

    // Initiate device joining.
    slStatus = sl_zigbee_join_network (nodeType, &netParams);
    GMOS_LOG_FMT (LOG_VERBOSE,
        "EmberZNet initiated device joining with status 0x%04X.", slStatus);
    return (slStatus == SL_STATUS_OK) ? true : false;
}

/*
 * Implement the EmberZNet network joining state machine.
 */
gmosTaskStatus_t gmosZigbeeRalEmberJoinNetworkPhase (
    gmosZigbeeStack_t* zigbeeStack)
{
    gmosZigbeeRalState_t* ralData = zigbeeStack->ralData;
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    zigbeeStackPhase_t nextPhase = ralData->zigbeeStackPhase;
    zigbeeStackStateJoining_t nextState = ralData->zigbeeStackState;
    sl_status_t slStatus;

    // Implement the network joining state machine.
    switch (ralData->zigbeeStackState) {

        // In the idle state wait for a network joining request.
        case ZIGBEE_STACK_STATE_JOINING_IDLE :
            gmosZigbeeSetNetworkState (zigbeeStack,
                GMOS_ZIGBEE_NETWORK_STATE_DOWN);
            taskStatus = GMOS_TASK_SUSPEND;
            break;

        // Start the network joining process.
        case ZIGBEE_STACK_STATE_JOINING_START :
            gmosZigbeeSetNetworkState (zigbeeStack,
                GMOS_ZIGBEE_NETWORK_STATE_JOINING);
            nextState = ZIGBEE_STACK_STATE_JOINING_ACTIVE_SCAN_REQ;
            break;

        // Implement joining retry backoff.
        case ZIGBEE_STACK_STATE_JOINING_RETRY_BACKOFF :
            GMOS_LOG (LOG_DEBUG,
                "EmberZNet implement joining retry backoff.");
            gmosZigbeeSetNetworkState (zigbeeStack,
                GMOS_ZIGBEE_NETWORK_STATE_DOWN);
            nextState = ZIGBEE_STACK_STATE_JOINING_START;
            taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (60 * 1000));
            break;

        // Initiate active network scan request. This selects a random
        // extended PAN ID as a byte array. At the end of the scan the
        // network with the closest matching extended PAN ID will be
        // used as the joining candidate.
        case ZIGBEE_STACK_STATE_JOINING_ACTIVE_SCAN_REQ :
            slStatus = sl_zigbee_start_scan (SL_ZIGBEE_ACTIVE_SCAN,
                zigbeeStack->channelMask, ACTIVE_SCAN_DWELL_TIME);
            if (slStatus == SL_STATUS_OK) {
                ralData->phase.join.networkCount = 0;
                ralData->phase.join.bestMatch = 0xFF;
                gmosPalGetRandomBytes (ralData->phase.join.randomBytes,
                    GMOS_ZIGBEE_EXTENDED_PAN_ID_SIZE);
                nextState = ZIGBEE_STACK_STATE_JOINING_ACTIVE_SCAN_CHECK;
            } else {
                nextState = ZIGBEE_STACK_STATE_JOINING_FAILED;
            }
            break;

        // Wait in the active scan check state for callback processing
        // to be completed.
        case ZIGBEE_STACK_STATE_JOINING_ACTIVE_SCAN_CHECK :
            taskStatus = GMOS_TASK_SUSPEND;
            break;

        // Initiate network joining request.
        case ZIGBEE_STACK_STATE_JOINING_JOIN_NETWORK_REQ :
            if (gmosZigbeeRalEmberJoinNetworkRequest (zigbeeStack)) {
                nextState = ZIGBEE_STACK_STATE_JOINING_JOIN_NETWORK_CHECK;
                taskStatus = GMOS_TASK_SUSPEND;
            } else {
                nextState = ZIGBEE_STACK_STATE_JOINING_RETRY_BACKOFF;
            }
            break;

        // Wait for the stack status handler to indicate network joining
        // complete.
        case ZIGBEE_STACK_STATE_JOINING_JOIN_NETWORK_CHECK :
            taskStatus = GMOS_TASK_SUSPEND;
            break;

        // Enter the network active phase on joining complete.
        case ZIGBEE_STACK_STATE_JOINING_JOIN_NETWORK_DONE :
            nextPhase = ZIGBEE_STACK_PHASE_ACTIVE;
            nextState = ZIGBEE_STACK_STATE_JOINING_IDLE;
            break;

        // On failure enter the stack failure state.
        default :
            nextPhase = ZIGBEE_STACK_PHASE_FAILED;
            nextState = ZIGBEE_STACK_STATE_JOINING_FAILED;
            break;
    }
    ralData->zigbeeStackPhase = nextPhase;
    ralData->zigbeeStackState = nextState;
    return taskStatus;
}

/*
 * Initiates the joining process for an existing Zigbee network. This
 * capability is not supported for coordinator nodes and will only
 * progress if the Zigbee device is not currently joined to a Zigbee
 * network.
 */
gmosZigbeeStatus_t gmosZigbeeJoinNetwork (
    gmosZigbeeStack_t* zigbeeStack, uint32_t channelMask,
    uint8_t* deviceLinkKey, uint8_t* extendedPanId)
{
    gmosZigbeeRalState_t* ralData = zigbeeStack->ralData;
    uint_fast8_t i;
    sl_zigbee_initial_security_state_t initSecurity = { 0 };
    sl_zigbee_extended_security_bitmask_t extBitmask;
    sl_status_t slStatus;
    uint8_t defaultKey [SL_ZIGBEE_ENCRYPTION_KEY_SIZE] =
        GMOS_ZIGBEE_INTEROPERABILITY_LINK_KEY;
    uint8_t* linkKeyPtr;

    // This call is not supported for coordinator nodes.
    if (GMOS_CONFIG_ZIGBEE_NODE_TYPE == GMOS_ZIGBEE_COORDINATOR_NODE) {
        return GMOS_ZIGBEE_STATUS_INVALID_CALL;
    }

    // Indicate that the network is already active.
    if (ralData->zigbeeStackPhase == ZIGBEE_STACK_PHASE_ACTIVE) {
        return GMOS_ZIGBEE_STATUS_NETWORK_UP;
    }

    // Indicate that network joining is already in progress.
    if ((ralData->zigbeeStackPhase != ZIGBEE_STACK_PHASE_JOINING) ||
        (ralData->zigbeeStackState != ZIGBEE_STACK_STATE_JOINING_IDLE)) {
        return GMOS_ZIGBEE_STATUS_INVALID_CALL;
    }

    // Check for valid channel mask.
    channelMask &= GMOS_ZIGBEE_CHANNEL_MASK;
    if (channelMask == 0) {
        return GMOS_ZIGBEE_STATUS_INVALID_ARGUMENT;
    }
    zigbeeStack->channelMask = channelMask;

    // Copy the supplied extended PAN ID if present.
    if (extendedPanId != NULL) {
        if (checkExtendedPanId (extendedPanId)) {
            ralData->phase.join.extPanIdMatch = true;
            for (i = 0; i < GMOS_ZIGBEE_EXTENDED_PAN_ID_SIZE; i++) {
                zigbeeStack->extendedPanId [i] = extendedPanId [i];
            }
        } else {
            return GMOS_ZIGBEE_STATUS_INVALID_ARGUMENT;
        }
    } else {
        ralData->phase.join.extPanIdMatch = false;
        for (i = 0; i < GMOS_ZIGBEE_EXTENDED_PAN_ID_SIZE; i++) {
            zigbeeStack->extendedPanId [i] = 0;
        }
    }

    // Select the initial link key to be used during joining. This may
    // be a preconfigured key or the default Zigbee Alliance link key.
    linkKeyPtr = (deviceLinkKey != NULL) ? deviceLinkKey : defaultKey;
    for (i = 0; i < SL_ZIGBEE_ENCRYPTION_KEY_SIZE; i++) {
        initSecurity.preconfiguredKey.contents [i] = *(linkKeyPtr++);
    }

    // Set the common network security options.
    initSecurity.bitmask =
        SL_ZIGBEE_STANDARD_SECURITY_MODE |
        SL_ZIGBEE_HAVE_PRECONFIGURED_KEY |
        SL_ZIGBEE_REQUIRE_ENCRYPTED_KEY |
        SL_ZIGBEE_NO_FRAME_COUNTER_RESET;
    extBitmask =
        SL_ZIGBEE_EXT_NO_FRAME_COUNTER_RESET;

    // Attempt to set the network security parameters.
    slStatus = sl_zigbee_set_initial_security_state (&initSecurity);
    if (slStatus == SL_STATUS_OK) {
        slStatus = sl_zigbee_set_extended_security_bitmask (extBitmask);
    }
    if (slStatus != SL_STATUS_OK) {
        GMOS_LOG_FMT (LOG_DEBUG,
            "EmberZNet security setup failed with status 0x%04X.",
            slStatus);
        return GMOS_ZIGBEE_STATUS_FATAL_ERROR;
    }

    // Start the network joining process.
    ralData->zigbeeStackState = ZIGBEE_STACK_STATE_JOINING_START;
    gmosSchedulerTaskResume (&(ralData->emberWorkerTask));
    return GMOS_ZIGBEE_STATUS_SUCCESS;
}

#endif // GMOS_CONFIG_ZIGBEE_NODE_TYPE
