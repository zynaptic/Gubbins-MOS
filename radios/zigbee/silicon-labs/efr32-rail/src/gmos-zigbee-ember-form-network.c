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
 * This file implements the network formation state machine for the
 * EmberZNet Zigbee stack implementation. This will only be used for
 * coordinator node implementations.
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
#include "gmos-zigbee-ember-ral.h"
#include "sl_zigbee.h"
#include "stack-info.h"
#include "security.h"
#include "network-formation.h"

// Specify the dwell time per channel when performing the energy scan
// during network formation. The actual dwell time is approximately :
//   5 x 2 ^ (ENERGY_SCAN_DWELL_TIME - 1) milliseconds.
#define ENERGY_SCAN_DWELL_TIME 8

// Specify the dwell time per channel when performing an active network
// scan. Expressed as IEEE MAC scan duration units.
#define ACTIVE_SCAN_DWELL_TIME 8

// Set the RSSI derating factors to be used during active scan.
#define RSSI_DERATING_INCREMENT  5
#define RSSI_DERATING_LIMIT     50

/*
 * Generate a valid PAN ID. Valid PAN IDs are in the range 0x0000 to
 * 0x3FFF, with the two highest order bits being reserved and set to 0.
 * A value of 0xFFFF is reserved to indicate an invalid PAN ID.
 */
static inline void generateNewPanId (gmosZigbeeStack_t* zigbeeStack)
{
    uint16_t newPanId;
    gmosPalGetRandomBytes ((uint8_t*) &newPanId, sizeof (newPanId));
    newPanId &= 0x3FFF;
    zigbeeStack->currentPanId = newPanId;
}

/*
 * Generate the candidate channel mask from the current list of
 * saved channel identifiers.
 */
static inline uint32_t generateCandidateMask
    (gmosZigbeeRalState_t* ralData)
{
    uint_fast8_t i;
    uint8_t* savedChannels = ralData->phase.form.channelIds;
    uint32_t candidateChannelMask = 0;

    for (i = 0; i < GMOS_CONFIG_ZIGBEE_SCAN_CANDIDATE_CHANNELS; i++) {
        if (savedChannels [i] < 0xFF) {
            candidateChannelMask |= (1 << savedChannels [i]);
        }
    }
    GMOS_LOG_FMT (LOG_DEBUG,
        "EmberZNet generated candidate channel mask 0x%08X.",
        candidateChannelMask);
    return candidateChannelMask;
}

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
 * Initialises a network security key using either the supplied key
 * source or a random value.
 */
static void setSecurityKey (uint8_t* keyData, uint8_t* keySource)
{
    uint_fast8_t i;

    if (keySource != NULL) {
        for (i = SL_ZIGBEE_ENCRYPTION_KEY_SIZE; i != 0; i--) {
            *(keyData++) = *(keySource++);
        }
    } else {
        gmosPalGetRandomBytes (keyData, SL_ZIGBEE_ENCRYPTION_KEY_SIZE);
    }
}

/*
 * Sets the initial security state for the network.
 */
static inline sl_status_t setInitialSecurityState (
    uint8_t* commonLinkKey, uint8_t* networkKey)
{
    sl_status_t slStatus;
    sl_zigbee_initial_security_state_t initSecurity = { 0 };
    sl_zigbee_extended_security_bitmask_t extBitmask = 0;

    // Generate random keys if required, placing them in the initial
    // security state data structure.
    setSecurityKey (initSecurity.networkKey.contents, networkKey);
    setSecurityKey (initSecurity.preconfiguredKey.contents, commonLinkKey);

    // Select the security mode settings.
    if (GMOS_CONFIG_ZIGBEE_COORDINATOR_USES_HASHED_LINK_KEYS) {
        initSecurity.bitmask =
            SL_ZIGBEE_STANDARD_SECURITY_MODE |
            SL_ZIGBEE_TRUST_CENTER_GLOBAL_LINK_KEY |
            SL_ZIGBEE_HAVE_PRECONFIGURED_KEY |
            SL_ZIGBEE_HAVE_NETWORK_KEY |
            SL_ZIGBEE_NO_FRAME_COUNTER_RESET |
            SL_ZIGBEE_TRUST_CENTER_USES_HASHED_LINK_KEY;
    } else {
        initSecurity.bitmask =
            SL_ZIGBEE_STANDARD_SECURITY_MODE |
            SL_ZIGBEE_TRUST_CENTER_GLOBAL_LINK_KEY |
            SL_ZIGBEE_HAVE_PRECONFIGURED_KEY |
            SL_ZIGBEE_HAVE_NETWORK_KEY |
            SL_ZIGBEE_NO_FRAME_COUNTER_RESET;
    }

    // Attempt to set the network security parameters.
    slStatus = sl_zigbee_set_initial_security_state (&initSecurity);
    GMOS_LOG_FMT (LOG_DEBUG,
        "EmberZNet set security state with status %04X.", slStatus);
    if (slStatus == SL_STATUS_OK) {
        slStatus = sl_zigbee_set_extended_security_bitmask (extBitmask);
        GMOS_LOG_FMT (LOG_DEBUG,
            "EmberZNet set extended security bitmask with status %04X.",
            slStatus);
    }
    return slStatus;
}

/*
 * Initialises a network parameters data structure for network
 * formation.
 */
static inline void setNetworkParams (gmosZigbeeStack_t* zigbeeStack,
    sl_zigbee_network_parameters_t* networkParams)
{
    uint_fast8_t i;

    networkParams->panId = zigbeeStack->currentPanId;
    networkParams->radioChannel = zigbeeStack->currentChannelId;
    networkParams->joinMethod = SL_ZIGBEE_USE_CONFIGURED_NWK_STATE;
    networkParams->nwkManagerId = 0x0000;
    networkParams->nwkUpdateId = 0x00;
    networkParams->channels = zigbeeStack->channelMask;
    networkParams->radioTxPower = GMOS_CONFIG_ZIGBEE_DEFAULT_TX_POWER;
    for (i = 0; i < GMOS_ZIGBEE_EXTENDED_PAN_ID_SIZE; i++) {
        networkParams->extendedPanId [i] = zigbeeStack->extendedPanId [i];
    }
}

/*
 * Implement EmberZNet stack energy scan callback handler.
 */
void sl_zigbee_energy_scan_result_handler (uint8_t channel, int8_t maxRssi)
{
    gmosZigbeeStack_t* zigbeeStack = gmosZigbeeRalEmberStackInstance;
    gmosZigbeeRalState_t* ralData = zigbeeStack->ralData;
    zigbeeStackPhase_t phase = ralData->zigbeeStackPhase;
    zigbeeStackStateForming_t state = ralData->zigbeeStackState;
    uint8_t* savedChannels = ralData->phase.form.channelIds;
    int8_t* savedRssi = ralData->phase.form.channelRssi;
    uint_fast8_t i;

    // Handle callback processing in the energy scan state.
    if ((phase == ZIGBEE_STACK_PHASE_FORMING) &&
        (state == ZIGBEE_STACK_STATE_FORMING_ENERGY_SCAN_CHECK)) {
        uint8_t worstSlot = 0;
        int8_t worstRssi = -128;
        for (i = 0; i < GMOS_CONFIG_ZIGBEE_SCAN_CANDIDATE_CHANNELS; i++) {
            if (savedChannels [i] == 0xFF) {
                worstSlot = i;
                worstRssi = 127;
            } else if (savedRssi [i] >= worstRssi) {
                worstSlot = i;
                worstRssi = savedRssi [i];
            }
        }
        if (maxRssi < worstRssi) {
            savedChannels [worstSlot] = channel;
            savedRssi [worstSlot] = (uint8_t) maxRssi;
        }
        GMOS_LOG_FMT (LOG_VERBOSE,
            "EmberZNet energy scan result: Channel %d, Max RSSI %d dBm.",
            channel, maxRssi);
    }
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
    zigbeeStackStateForming_t state = ralData->zigbeeStackState;
    uint8_t* savedChannels = ralData->phase.form.channelIds;
    int8_t* savedRssi = ralData->phase.form.channelRssi;
    uint_fast8_t nwkChannel;
    uint_fast16_t nwkPanId;
    uint8_t* nwkExtPanId;
    uint_fast8_t i;
    (void) lqi;
    (void) rssi;

    // Extract required network parameters.
    nwkChannel = networkFound->channel;
    nwkPanId = networkFound->panId;
    nwkExtPanId = networkFound->extendedPanId;

    // Handle callback processing in the active scan state.
    if ((phase == ZIGBEE_STACK_PHASE_FORMING) &&
        (state == ZIGBEE_STACK_STATE_FORMING_ACTIVE_SCAN_CHECK)) {

        // An extended PAN ID conflict is only likely to occur as a
        // result of some form of network misconfiguration. This
        // currently results in a fatal error.
        for (i = 0; i < GMOS_ZIGBEE_EXTENDED_PAN_ID_SIZE; i++) {
            if (nwkExtPanId [i] != zigbeeStack->extendedPanId [i]) {
                break;
            }
        }
        if (i >= GMOS_ZIGBEE_EXTENDED_PAN_ID_SIZE) {
            GMOS_LOG (LOG_ERROR,
                "EmberZNet detected duplicate extended PAN ID.");
            ralData->zigbeeStackState = ZIGBEE_STACK_STATE_FORMING_FAILED;
            return;
        }

        // Derate the detected RSSI value for each active network found
        // on a given channel.
        for (i = 0; i < GMOS_CONFIG_ZIGBEE_SCAN_CANDIDATE_CHANNELS; i++) {
            if (savedChannels [i] == nwkChannel) {
                savedRssi [i] += RSSI_DERATING_INCREMENT;
                if (savedRssi [i] > RSSI_DERATING_LIMIT) {
                    savedRssi [i] = RSSI_DERATING_LIMIT;
                }
                break;
            }
        }

        // Force new scan on matching PAN ID values.
        if (nwkPanId == zigbeeStack->currentPanId) {
            zigbeeStack->currentPanId = GMOS_ZIGBEE_INVALID_PAN_ID;
        }
        GMOS_LOG_FMT (LOG_VERBOSE,
            "EmberZNet active scan result: Channel %d, PAN ID %04X.",
            nwkChannel, nwkPanId);
    }
}

/*
 * Provide the energy scan complete callback handler.
 */
static inline void energyScanCompleteHandler (
    gmosZigbeeRalState_t* ralData, uint8_t channel, sl_status_t slStatus)
{
    // A successful callback marks the end of the energy scan.
    if (slStatus == SL_STATUS_OK) {
        ralData->zigbeeStackState =
            ZIGBEE_STACK_STATE_FORMING_ACTIVE_SCAN_REQ;
        GMOS_LOG (LOG_DEBUG, "EmberZNet energy scan completed OK.");
        gmosSchedulerTaskResume (&(ralData->emberWorkerTask));
    }

    // All other status conditions can be ignored at this stage.
    else {
        GMOS_LOG_FMT (LOG_DEBUG,
            "EmberZNet energy scan failed (channel %d, status 0x%04X).",
            channel, slStatus);
    }
}

/*
 * Provide the active scan complete callback handler.
 */
static inline void activeScanCompleteHandler (
    gmosZigbeeStack_t* zigbeeStack, uint8_t channel, sl_status_t slStatus)
{
    gmosZigbeeRalState_t* ralData = zigbeeStack->ralData;
    uint_fast16_t panId = zigbeeStack->currentPanId;
    uint8_t* savedChannels = ralData->phase.form.channelIds;
    int8_t* savedRssi = ralData->phase.form.channelRssi;
    uint_fast8_t i;

    // Run another network scan if a PAN ID conflict has been detected.
    if ((slStatus == SL_STATUS_OK) &&
        (panId == GMOS_ZIGBEE_INVALID_PAN_ID)) {
        GMOS_LOG (LOG_DEBUG,
            "EmberZNet PAN ID conflict detected. Restarting scan.");
        for (i = 0; i < GMOS_CONFIG_ZIGBEE_SCAN_CANDIDATE_CHANNELS; i++) {
            ralData->phase.form.channelIds [i] = 0xFF;
        }
        ralData->zigbeeStackState =
            ZIGBEE_STACK_STATE_FORMING_ENERGY_SCAN_REQ;
        gmosSchedulerTaskResume (&(ralData->emberWorkerTask));
    }

    // On successful completion, select the channel with the best
    // adjusted RSSI and initiate network formation.
    else if (slStatus == SL_STATUS_OK) {
        uint_fast8_t bestSlot = 0xFF;
        int_fast8_t bestRssi = 127;
        for (i = 0; i < GMOS_CONFIG_ZIGBEE_SCAN_CANDIDATE_CHANNELS; i++) {
            if ((savedChannels [i] != 0xFF) && (savedRssi [i] <= bestRssi)) {
                bestSlot = i;
                bestRssi = savedRssi [i];
            }
        }

        // Restart the scan if there are no valid channels. Otherwise
        // start the network formation process for the selected channel.
        if (bestSlot == 0xFF) {
            ralData->zigbeeStackState =
                ZIGBEE_STACK_STATE_FORMING_ENERGY_SCAN_REQ;
        } else {
            zigbeeStack->currentChannelId = savedChannels [bestSlot];
            ralData->zigbeeStackState =
                ZIGBEE_STACK_STATE_FORMING_FORM_NETWORK_REQ;
            GMOS_LOG_FMT (LOG_DEBUG,
                "EmberZNet selected channel %d with derated RSSI %d dBm.",
                savedChannels [bestSlot], savedRssi [bestSlot]);
        }
        gmosSchedulerTaskResume (&(ralData->emberWorkerTask));
    }

    // Disable the channel on failure to perform an active scan.
    else {
        GMOS_LOG_FMT (LOG_DEBUG,
            "EmberZNet active scan failed (channel %d, status 0x%04X).",
            channel, slStatus);
        for (i = 0; i < GMOS_CONFIG_ZIGBEE_SCAN_CANDIDATE_CHANNELS; i++) {
            if (savedChannels [i] == channel) {
                savedChannels [i] = 0xFF;
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
    zigbeeStackStateForming_t state = ralData->zigbeeStackState;

    // Process scan completion for the energy scan phase.
    if ((phase == ZIGBEE_STACK_PHASE_FORMING) &&
        (state == ZIGBEE_STACK_STATE_FORMING_ENERGY_SCAN_CHECK)) {
        energyScanCompleteHandler (ralData, channel, slStatus);
    }

    // Process scan completion for the active scan phase.
    if ((phase == ZIGBEE_STACK_PHASE_FORMING) &&
        (state == ZIGBEE_STACK_STATE_FORMING_ACTIVE_SCAN_CHECK)) {
        activeScanCompleteHandler (zigbeeStack, channel, slStatus);
    }
}

/*
 * Implement the EmberZNet network formation state machine.
 */
gmosTaskStatus_t gmosZigbeeRalEmberFormNetworkPhase (
    gmosZigbeeStack_t* zigbeeStack)
{
    gmosZigbeeRalState_t* ralData = zigbeeStack->ralData;
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    zigbeeStackPhase_t nextPhase = ralData->zigbeeStackPhase;
    zigbeeStackStateForming_t nextState = ralData->zigbeeStackState;
    uint_fast8_t i;
    sl_status_t slStatus;
    sl_zigbee_network_parameters_t networkParams;

    // Implement the network formation state machine.
    switch (ralData->zigbeeStackState) {

        // In the idle state wait for a network formation request.
        case ZIGBEE_STACK_STATE_FORMING_IDLE :
            gmosZigbeeSetNetworkState (zigbeeStack,
                GMOS_ZIGBEE_NETWORK_STATE_DOWN);
            taskStatus = GMOS_TASK_SUSPEND;
            break;

        // Start the full network formation process.
        case ZIGBEE_STACK_STATE_FORMING_START :
            gmosZigbeeSetNetworkState (zigbeeStack,
                GMOS_ZIGBEE_NETWORK_STATE_FORMING);
            nextState = ZIGBEE_STACK_STATE_FORMING_ENERGY_SCAN_REQ;
            break;

        // Initiate an energy scan over the specified set of radio
        // channels. This resets the temporary data array ready to store
        // the best channel candidates.
        case ZIGBEE_STACK_STATE_FORMING_ENERGY_SCAN_REQ :
            for (i = 0; i < GMOS_CONFIG_ZIGBEE_SCAN_CANDIDATE_CHANNELS; i++) {
                ralData->phase.form.channelIds [i] = 0xFF;
            }
            slStatus = sl_zigbee_start_scan (SL_ZIGBEE_ENERGY_SCAN,
                zigbeeStack->channelMask, ENERGY_SCAN_DWELL_TIME);
            if (slStatus == SL_STATUS_OK) {
                nextState = ZIGBEE_STACK_STATE_FORMING_ENERGY_SCAN_CHECK;
            } else {
                nextState = ZIGBEE_STACK_STATE_FORMING_FAILED;
            }
            break;

        // Wait in the energy scan check state for callback processing
        // to be completed.
        case ZIGBEE_STACK_STATE_FORMING_ENERGY_SCAN_CHECK :
            taskStatus = GMOS_TASK_SUSPEND;
            break;

        // Initiate a scan for active networks over the candidate set
        // of radio channels. This allows channels with larger number
        // of existing networks to be avoided.
        case ZIGBEE_STACK_STATE_FORMING_ACTIVE_SCAN_REQ :
            generateNewPanId (zigbeeStack);
            slStatus = sl_zigbee_start_scan (SL_ZIGBEE_ACTIVE_SCAN,
                generateCandidateMask (ralData), ACTIVE_SCAN_DWELL_TIME);
            if (slStatus == SL_STATUS_OK) {
                nextState = ZIGBEE_STACK_STATE_FORMING_ACTIVE_SCAN_CHECK;
            } else {
                nextState = ZIGBEE_STACK_STATE_FORMING_FAILED;
            }
            break;

        // Wait in the active scan check state for callback processing
        // to be completed.
        case ZIGBEE_STACK_STATE_FORMING_ACTIVE_SCAN_CHECK :
            taskStatus = GMOS_TASK_SUSPEND;
            break;

        // Implement network formation.
        case ZIGBEE_STACK_STATE_FORMING_FORM_NETWORK_REQ :
            setNetworkParams (zigbeeStack, &networkParams);
            slStatus = sl_zigbee_form_network (&networkParams);
            if (slStatus == SL_STATUS_OK) {
                nextState = ZIGBEE_STACK_STATE_FORMING_FORM_NETWORK_CHECK;
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (10));
            } else {
                nextState = ZIGBEE_STACK_STATE_FORMING_FAILED;
            }
            break;

        // Poll network state until the network is ready for use. Assign
        // the network security level to enable network encryption.
        case ZIGBEE_STACK_STATE_FORMING_FORM_NETWORK_CHECK :
            if (sl_zigbee_stack_is_up ()) {
                sl_zigbee_current_security_state_t securityState;
                nextPhase = ZIGBEE_STACK_PHASE_ACTIVE;
                nextState = ZIGBEE_STACK_STATE_FORMING_IDLE;
                zigbeeStack->currentNodeId = 0x0000;
                GMOS_LOG_FMT (LOG_VERBOSE,
                    "EmberZNet using Zigbee profile %d, security level %d.",
                    sl_zigbee_stack_profile (), sl_zigbee_security_level ());
                if (sl_zigbee_get_current_security_state (
                    &securityState) == SL_STATUS_OK) {
                    GMOS_LOG_FMT (LOG_VERBOSE,
                        "EmberZNet using current security bitmask 0x%04X.",
                        securityState.bitmask);
                }
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (10));
            }
            break;

        // On failure enter the stack failure state.
        default :
            nextPhase = ZIGBEE_STACK_PHASE_FAILED;
            nextState = ZIGBEE_STACK_STATE_FORMING_FAILED;
            break;
    }
    ralData->zigbeeStackPhase = nextPhase;
    ralData->zigbeeStackState = nextState;
    return taskStatus;
}

/*
 * Initiates the formation of a new Zigbee network. This capability is
 * only supported for coordinator nodes and will only progress if the
 * Zigbee device is not currently joined to a Zigbee network.
 */
gmosZigbeeStatus_t gmosZigbeeFormNetwork (gmosZigbeeStack_t* zigbeeStack,
    uint32_t channelMask, uint8_t* commonLinkKey, uint8_t* networkKey,
    uint8_t* extendedPanId)
{
    gmosZigbeeRalState_t* ralData = zigbeeStack->ralData;
    uint_fast8_t i;
    sl_status_t slStatus;

    // This call is only supported for coordinator nodes.
    if (GMOS_CONFIG_ZIGBEE_NODE_TYPE != GMOS_ZIGBEE_COORDINATOR_NODE) {
        return GMOS_ZIGBEE_STATUS_INVALID_CALL;
    }

    // Indicate that the network is already active.
    if (ralData->zigbeeStackPhase == ZIGBEE_STACK_PHASE_ACTIVE) {
        return GMOS_ZIGBEE_STATUS_NETWORK_UP;
    }

    // Indicate that network formation is already in progress.
    if ((ralData->zigbeeStackPhase != ZIGBEE_STACK_PHASE_FORMING) ||
        (ralData->zigbeeStackState != ZIGBEE_STACK_STATE_FORMING_IDLE)) {
        return GMOS_ZIGBEE_STATUS_INVALID_CALL;
    }

    // Check for valid channel mask.
    channelMask &= GMOS_ZIGBEE_CHANNEL_MASK;
    if (channelMask == 0) {
        return GMOS_ZIGBEE_STATUS_INVALID_ARGUMENT;
    }
    zigbeeStack->channelMask = channelMask;

    // Either use the supplied extended PAN ID or generate a new one.
    if (extendedPanId != NULL) {
        if (checkExtendedPanId (extendedPanId)) {
            for (i = 0; i < GMOS_ZIGBEE_EXTENDED_PAN_ID_SIZE; i++) {
                zigbeeStack->extendedPanId [i] = extendedPanId [i];
            }
        } else {
            return GMOS_ZIGBEE_STATUS_INVALID_ARGUMENT;
        }
    } else {
        do {
            gmosPalGetRandomBytes (zigbeeStack->extendedPanId,
                GMOS_ZIGBEE_EXTENDED_PAN_ID_SIZE);
        } while (!checkExtendedPanId (zigbeeStack->extendedPanId));
    }

    // Attempt to set the network security parameters.
    slStatus = setInitialSecurityState (commonLinkKey, networkKey);
    if (slStatus != SL_STATUS_OK) {
        return GMOS_ZIGBEE_STATUS_FATAL_ERROR;
    }

    // Start the network formation process.
    ralData->zigbeeStackState = ZIGBEE_STACK_STATE_FORMING_START;
    gmosSchedulerTaskResume (&(ralData->emberWorkerTask));
    return GMOS_ZIGBEE_STATUS_SUCCESS;
}

/*
 * Track the current EmberZNet stack status.
 */
void sl_zigbee_stack_status_handler (sl_status_t slStatus)
{
    GMOS_LOG_FMT (LOG_VERBOSE,
        "EmberZNet in stack status handler with status 0x%04X.",
        slStatus);
}

#endif // GMOS_CONFIG_ZIGBEE_NODE_TYPE
