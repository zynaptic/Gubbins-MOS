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
 * This header implements various utility functions for working with the
 * EmberZNet Zigbee network stack.
 */

#include <stdint.h>
#include <stdbool.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-zigbee-stack.h"
#include "gmos-zigbee-aps.h"
#include "gmos-zigbee-ember-utils.h"
#include "sl_zigbee.h"

/*
 * Maps the EmberZNet status code values to their corresponding
 * GubbinsMOS Zigbee status values.
 */
gmosZigbeeStatus_t gmosZigbeeConvertEmberStatusCode (
    sl_status_t slStatus)
{
    gmosZigbeeStatus_t status;
    switch (slStatus) {
        case SL_STATUS_OK :
            status = GMOS_ZIGBEE_STATUS_SUCCESS;
            break;

        case SL_STATUS_WOULD_BLOCK :
        case SL_STATUS_ALLOCATION_FAILED :
        case SL_STATUS_ZIGBEE_MAX_MESSAGE_LIMIT_REACHED :
            status = GMOS_ZIGBEE_STATUS_RETRY;
            break;

        case SL_STATUS_MESSAGE_TOO_LONG :
            status = GMOS_ZIGBEE_STATUS_MESSAGE_TOO_LONG;
            break;

        case SL_STATUS_NOT_FOUND :
        case SL_STATUS_INVALID_INDEX :
            status = GMOS_ZIGBEE_STATUS_NOT_FOUND;
            break;

        case SL_STATUS_NETWORK_UP :
            status = GMOS_ZIGBEE_STATUS_NETWORK_UP;
            break;

        case SL_STATUS_NETWORK_DOWN :
            status = GMOS_ZIGBEE_STATUS_NETWORK_DOWN;
            break;

        case SL_STATUS_INVALID_PARAMETER :
        case SL_STATUS_NULL_POINTER :
        case SL_STATUS_INVALID_CONFIGURATION :
        case SL_STATUS_INVALID_MODE :
        case SL_STATUS_INVALID_HANDLE :
        case SL_STATUS_INVALID_TYPE :
        case SL_STATUS_INVALID_RANGE :
        case SL_STATUS_INVALID_KEY :
        case SL_STATUS_INVALID_CREDENTIALS :
        case SL_STATUS_INVALID_COUNT :
        case SL_STATUS_INVALID_SIGNATURE :
            status = GMOS_ZIGBEE_STATUS_INVALID_ARGUMENT;
            break;

        default :
            GMOS_LOG_FMT (LOG_ERROR,
                "Unknown EmberZNet status code 0x%04X.", slStatus);
            status = GMOS_ZIGBEE_STATUS_FATAL_ERROR;
            break;
    }
    return status;
}

/*
 * Map the EmberZNet incoming message type encodings to their
 * corresponding GubbinsMOS APS message types.
 */
gmosZigbeeApsMsgType_t gmosZigbeeConvertEmberIncomingMessageType (
    sl_zigbee_incoming_message_type_t slMessageType)
{
    gmosZigbeeApsMsgType_t apsMsgType;
    switch (slMessageType) {
        case SL_ZIGBEE_INCOMING_UNICAST :
            apsMsgType = GMOS_ZIGBEE_APS_MSG_TYPE_RX_UNICAST;
            break;
        case SL_ZIGBEE_INCOMING_UNICAST_REPLY :
            apsMsgType = GMOS_ZIGBEE_APS_MSG_TYPE_RX_UNICAST_REPLY;
            break;
        case SL_ZIGBEE_INCOMING_MULTICAST :
            apsMsgType = GMOS_ZIGBEE_APS_MSG_TYPE_RX_MULTICAST;
            break;
        case SL_ZIGBEE_INCOMING_MULTICAST_LOOPBACK :
            apsMsgType = GMOS_ZIGBEE_APS_MSG_TYPE_RX_MULTICAST_LOOPBACK;
            break;
        case SL_ZIGBEE_INCOMING_BROADCAST :
            apsMsgType = GMOS_ZIGBEE_APS_MSG_TYPE_RX_BROADCAST;
            break;
        case SL_ZIGBEE_INCOMING_BROADCAST_LOOPBACK :
            apsMsgType = GMOS_ZIGBEE_APS_MSG_TYPE_RX_BROADCAST_LOOPBACK;
            break;
        default :
            apsMsgType = GMOS_ZIGBEE_APS_MSG_TYPE_UNKNOWN;
            break;
    }
    return apsMsgType;
}

/*
 * Map the EmberZNet outgoing message type encodings to their
 * corresponding GubbinsMOS APS message types.
 */
gmosZigbeeApsMsgType_t gmosZigbeeConvertEmberOutgoingMessageType (
    sl_zigbee_outgoing_message_type_t slMessageType)
{
    gmosZigbeeApsMsgType_t apsMsgType;
    switch (slMessageType) {
        case SL_ZIGBEE_OUTGOING_DIRECT :
            apsMsgType = GMOS_ZIGBEE_APS_MSG_TYPE_TX_UNICAST_DIRECT;
            break;
        case SL_ZIGBEE_OUTGOING_VIA_ADDRESS_TABLE :
            apsMsgType = GMOS_ZIGBEE_APS_MSG_TYPE_TX_UNICAST_ADDRESS_CACHE;
            break;
        case SL_ZIGBEE_OUTGOING_VIA_BINDING :
            apsMsgType = GMOS_ZIGBEE_APS_MSG_TYPE_TX_UNICAST_BINDING_TABLE;
            break;
        case SL_ZIGBEE_OUTGOING_MULTICAST :
            apsMsgType = GMOS_ZIGBEE_APS_MSG_TYPE_TX_MULTICAST;
            break;
        case SL_ZIGBEE_OUTGOING_BROADCAST :
            apsMsgType = GMOS_ZIGBEE_APS_MSG_TYPE_TX_BROADCAST;
            break;
        default :
            apsMsgType = GMOS_ZIGBEE_APS_MSG_TYPE_UNKNOWN;
            break;
    }
    return apsMsgType;
}

/*
 * Map the EmberZNet APS message option flags to their corresponding
 * GubbinsMOS APS message flags.
 */
uint8_t gmosZigbeeConvertEmberApsMessageFlags (
    sl_zigbee_aps_option_t slMessageFlags)
{
    uint8_t apsFlags = GMOS_ZIGBEE_APS_OPTION_NONE;
    if ((slMessageFlags & SL_ZIGBEE_APS_OPTION_RETRY) != 0) {
        apsFlags |= GMOS_ZIGBEE_APS_OPTION_RETRY;
    }
    if ((slMessageFlags & SL_ZIGBEE_APS_OPTION_ZDO_RESPONSE_REQUIRED) != 0) {
        apsFlags |= GMOS_ZIGBEE_APS_OPTION_ZDO_RESPONSE_REQUIRED;
    }
    return apsFlags;
}
