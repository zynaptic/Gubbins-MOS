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
 * This file implements the common support functions for accessing the
 * standard Zigbee APS messaging layer.
 */

#include <stdint.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-zigbee-config.h"
#include "gmos-zigbee-stack.h"
#include "gmos-zigbee-aps.h"
#include "gmos-zigbee-endpoint.h"
#include "gmos-zigbee-zdo-client.h"
#include "gmos-zigbee-zdo-server.h"

/*
 * Provide APS message debugging information.
 */
static void gmosZigbeeApsMessageDebug (const char* msg,
    gmosZigbeeApsFrame_t* apsFrame)
{
    GMOS_LOG_FMT (LOG_VERBOSE, "%s\r\n"
        "\t\tMessage type      : 0x%02X\r\n"
        "\t\tMessage flags     : 0x%02X\r\n"
        "\t\tZigbee profile ID : 0x%04X\r\n"
        "\t\tZigbee cluster ID : 0x%04X\r\n"
        "\t\tPeer node ID      : 0x%04X\r\n"
        "\t\tSource endpoint   : %d\r\n"
        "\t\tTarget endpoint   : %d\r\n"
        "\t\tTransmit radius   : %d\r\n"
        "\t\tMessage length    : %d",
        msg, apsFrame->apsMsgType, apsFrame->apsMsgFlags,
        apsFrame->profileId, apsFrame->clusterId,
        apsFrame->peer.nodeId, apsFrame->sourceEndpoint,
        apsFrame->targetEndpoint, apsFrame->apsMsgRadius,
        gmosBufferGetSize (&(apsFrame->payloadBuffer)));
}

/*
 * Perform common validation checks on an APS transmit request.
 */
static gmosZigbeeStatus_t gmosZigbeeApsValidateRequest (
    gmosZigbeeStack_t* zigbeeStack, gmosZigbeeApsFrame_t* txMsgApsFrame)
{
    gmosZigbeeStatus_t status = GMOS_ZIGBEE_STATUS_SUCCESS;

    // Check that the stack is ready to transfer messages.
    if (zigbeeStack->networkState != GMOS_ZIGBEE_NETWORK_STATE_CONNECTED) {
        status = GMOS_ZIGBEE_STATUS_INVALID_CALL;
    }

    // Check that the APS message payload does not exceed the maximum
    // payload length.
    else if (gmosBufferGetSize (&txMsgApsFrame->payloadBuffer) >
        zigbeeStack->apsMaxMessageSize) {
        status = GMOS_ZIGBEE_STATUS_MESSAGE_TOO_LONG;
    }
    return status;
}

/*
 * This is the public APS message unicast transmit function which may be
 * called to send the specified APS message.
 */
gmosZigbeeStatus_t gmosZigbeeApsUnicastTransmit (
    gmosZigbeeStack_t* zigbeeStack, gmosZigbeeApsFrame_t* txMsgApsFrame,
    gmosZigbeeApsMsgSentHandler_t txMsgSentHandler, uint8_t* txMsgTag)
{
    gmosZigbeeStatus_t stackStatus;
    uint8_t stackTag;
    uint_fast8_t slot;

    // Perform common validation checks on the transmit request.
    stackStatus = gmosZigbeeApsValidateRequest (zigbeeStack, txMsgApsFrame);
    if (stackStatus != GMOS_ZIGBEE_STATUS_SUCCESS) {
        goto out;
    }

    // Determine whether a slot is available for storing the message
    // sent callback handler.
    if (txMsgSentHandler != NULL) {
        for (slot = 0;
            slot < GMOS_CONFIG_ZIGBEE_APS_TRANSMIT_MAX_REQUESTS; slot++) {
            if (zigbeeStack->apsTxMsgCallbacks [slot] == NULL) {
                break;
            }
        }
        if (slot == GMOS_CONFIG_ZIGBEE_APS_TRANSMIT_MAX_REQUESTS) {
            stackStatus = GMOS_ZIGBEE_STATUS_RETRY;
            goto out;
        }
    }

    // Attempt to send the message using the underlying Zigbee stack.
    stackStatus = gmosZigbeeStackApsUnicastTransmit (
        zigbeeStack, txMsgApsFrame, &stackTag);
    if (stackStatus != GMOS_ZIGBEE_STATUS_SUCCESS) {
        goto out;
    }

    // Populate the message sent callback table and transmit message tag
    // if required.
    if (txMsgSentHandler != NULL) {
        zigbeeStack->apsTxMsgCallbacks [slot] = txMsgSentHandler;
        zigbeeStack->apsTxMsgTags [slot] = stackTag;
    }
    if (txMsgTag != NULL) {
        *txMsgTag = stackTag;
    }
out:
    return stackStatus;
}

/*
 * This is the public APS message broadcast transmit function which may
 * be called to send the specified APS message.
 */
gmosZigbeeStatus_t gmosZigbeeApsBroadcastTransmit (
    gmosZigbeeStack_t* zigbeeStack, gmosZigbeeApsFrame_t* txMsgApsFrame,
    gmosZigbeeApsMsgSentHandler_t txMsgSentHandler, uint8_t* txMsgTag)
{
    gmosZigbeeStatus_t stackStatus;
    gmosZigbeeApsBroadcastType_t broadcastType;
    uint8_t stackTag;
    uint_fast8_t slot;

    // Perform common validation checks on the transmit request.
    stackStatus = gmosZigbeeApsValidateRequest (zigbeeStack, txMsgApsFrame);
    if (stackStatus != GMOS_ZIGBEE_STATUS_SUCCESS) {
        goto out;
    }

    // The broadcast type must be one of the permitted values.
    broadcastType = txMsgApsFrame->peer.broadcastType;
    if ((broadcastType != GMOS_ZIGBEE_APS_BROADCAST_ALL_NODES) &&
        (broadcastType != GMOS_ZIGBEE_APS_BROADCAST_ALL_RX_IDLE) &&
        (broadcastType != GMOS_ZIGBEE_APS_BROADCAST_ROUTERS_ONLY)) {
        stackStatus = GMOS_ZIGBEE_STATUS_INVALID_ARGUMENT;
        goto out;
    }

    // The APS broadcast radius is restricted to the maximum supported
    // value, which is encoded as zero.
    if (txMsgApsFrame->apsMsgRadius >=
        GMOS_CONFIG_ZIGBEE_APS_TRANSMIT_MAX_RADIUS) {
        txMsgApsFrame->apsMsgRadius = 0;
    }

    // Determine whether a slot is available for storing the message
    // sent callback handler.
    if (txMsgSentHandler != NULL) {
        for (slot = 0;
            slot < GMOS_CONFIG_ZIGBEE_APS_TRANSMIT_MAX_REQUESTS; slot++) {
            if (zigbeeStack->apsTxMsgCallbacks [slot] == NULL) {
                break;
            }
        }
        if (slot == GMOS_CONFIG_ZIGBEE_APS_TRANSMIT_MAX_REQUESTS) {
            stackStatus = GMOS_ZIGBEE_STATUS_RETRY;
            goto out;
        }
    }

    // Attempt to send the message using the underlying Zigbee stack.
    stackStatus = gmosZigbeeStackApsBroadcastTransmit (
        zigbeeStack, txMsgApsFrame, &stackTag);
    if (stackStatus != GMOS_ZIGBEE_STATUS_SUCCESS) {
        goto out;
    }

    // Populate the message sent callback table and transmit message tag
    // if required.
    if (txMsgSentHandler != NULL) {
        zigbeeStack->apsTxMsgCallbacks [slot] = txMsgSentHandler;
        zigbeeStack->apsTxMsgTags [slot] = stackTag;
    }
    if (txMsgTag != NULL) {
        *txMsgTag = stackTag;
    }
out:
    return stackStatus;
}

/*
 * This is the callback function from the vendor stack that is used to
 * notify the common application layer when an APS message has been
 * transmitted.
 */
void gmosZigbeeStackApsMessageTransmitted (
    gmosZigbeeStack_t* zigbeeStack, gmosZigbeeApsFrame_t* txMsgApsFrame,
    gmosZigbeeStatus_t txMsgStatus, uint8_t txMsgTag)
{
    uint_fast8_t slot;

    // Search the message sent callback table for a matching transmit
    // message tag.
    for (slot = 0;
        slot < GMOS_CONFIG_ZIGBEE_APS_TRANSMIT_MAX_REQUESTS; slot++) {
        if (zigbeeStack->apsTxMsgTags [slot] == txMsgTag) {
            break;
        }
    }

    // Issue a callback if one is available. Callbacks are stored as
    // anonymous pointers, so the data type needs to be reinstated.
    if (slot < GMOS_CONFIG_ZIGBEE_APS_TRANSMIT_MAX_REQUESTS) {
        void* txMsgSentHandler = zigbeeStack->apsTxMsgCallbacks [slot];
        if (txMsgSentHandler != NULL) {
            ((gmosZigbeeApsMsgSentHandler_t) txMsgSentHandler) (
                zigbeeStack, txMsgApsFrame, txMsgStatus, txMsgTag);
        }
        zigbeeStack->apsTxMsgCallbacks [slot] = NULL;
    }

    // Optionally log APS message parameters for debug purposes
    GMOS_LOG_FMT (LOG_DEBUG,
        "Sent APS message (status 0x%02X, tag 0x%02X)", txMsgStatus, txMsgTag);
    gmosZigbeeApsMessageDebug ("APS TX Message :", txMsgApsFrame);
}

/*
 * This is the callback function from the vendor stack that is used to
 * notify the common application layer when an APS message has been
 * received.
 */
void gmosZigbeeStackApsMessageReceived (
    gmosZigbeeStack_t* zigbeeStack, gmosZigbeeApsFrame_t* rxMsgApsFrame,
    gmosBuffer_t* txMsgApsBuffer)
{
    gmosZigbeeEndpoint_t* appEndpoint;

    // Optionally log APS message parameters for debug purposes
    gmosZigbeeApsMessageDebug ("APS RX Message :", rxMsgApsFrame);

    // Handle inbound ZDO endpoint messages. ZDO request messages are
    // sent to the ZDO server entity and ZDO response messages are sent
    // to the ZDO client entity. Messages which do not have the correct
    // profile ID for the ZDO endpoint are silently discarded.
    if (rxMsgApsFrame->targetEndpoint == 0) {
        if (rxMsgApsFrame->profileId == 0x0000) {
            if ((rxMsgApsFrame->clusterId & 0x8000) == 0) {
                gmosZigbeeZdoServerRequestHandler (
                    zigbeeStack, rxMsgApsFrame);
            } else if (zigbeeStack->zdoClient != NULL) {
                gmosZigbeeZdoClientResponseHandler (
                    zigbeeStack, rxMsgApsFrame);
            }
        }
    }

    // Handle inbound application messages that are addressed to the
    // broadcast endpoint. These are forwarded to all endpoints on the
    // device. Note that any immediate responses generated for endpoint
    // broadcasts will always be discarded, since immediate replies
    // using the broadcast endpoint as the source are not valid.
    else if (rxMsgApsFrame->targetEndpoint == 0xFF) {
        appEndpoint = zigbeeStack->endpointList;
        while (appEndpoint != NULL) {
            gmosZigbeeEndpointRxMessageDispatch (
                zigbeeStack, appEndpoint, rxMsgApsFrame, txMsgApsBuffer);
            gmosBufferReset (txMsgApsBuffer, 0);
            appEndpoint = appEndpoint->nextEndpoint;
        }
    }

    // Handle inbound application messages that are addressed to a
    // specific endpoint.
    else {
        appEndpoint = gmosZigbeeEndpointInstance (
            zigbeeStack, rxMsgApsFrame->targetEndpoint);
        if (appEndpoint != NULL) {
            gmosZigbeeEndpointRxMessageDispatch (
                zigbeeStack, appEndpoint, rxMsgApsFrame, txMsgApsBuffer);
        }
    }
}
