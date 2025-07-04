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
 * This file implements the APS message handling functions for the
 * EmberZNet Zigbee stack implementation. This APS message handling
 * implementation is for conventional Zigbee devices that use standard
 * message routing.
 */

#include <stdint.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-buffers.h"
#include "gmos-zigbee-config.h"
#include "gmos-zigbee-stack.h"
#include "gmos-zigbee-aps.h"
#include "gmos-zigbee-ember-ral.h"
#include "gmos-zigbee-ember-utils.h"
#include "sl_zigbee.h"

/*
 * This is the Zigbee radio specific APS message unicast transmit
 * function which will be called to send the specified APS message.
 */
gmosZigbeeStatus_t gmosZigbeeStackApsUnicastTransmit (
    gmosZigbeeStack_t* zigbeeStack, gmosZigbeeApsFrame_t* txMsgApsFrame,
    uint8_t* txMsgTag)
{
    (void) zigbeeStack;
    uint8_t apsFlags = txMsgApsFrame->apsMsgFlags;
    sl_status_t slStatus;
    sl_zigbee_aps_frame_t slApsFrame;
    uint8_t msgData [GMOS_ZIGBEE_APS_MESSAGE_BUFFER_SIZE];
    uint_fast16_t msgSize;
    uint8_t apsSequence;

    // Copy over the message buffer contents.
    msgSize = gmosBufferGetSize (&(txMsgApsFrame->payloadBuffer));
    if (msgSize <= sizeof (msgData)) {
        gmosBufferRead (
            &(txMsgApsFrame->payloadBuffer), 0, msgData, msgSize);
    } else {
        return GMOS_ZIGBEE_STATUS_MESSAGE_TOO_LONG;
    }

    // Construct the EmberZNet stack APS frame data structure.
    slApsFrame.profileId = txMsgApsFrame->profileId;
    slApsFrame.clusterId = txMsgApsFrame->clusterId;
    slApsFrame.sourceEndpoint = txMsgApsFrame->sourceEndpoint;
    slApsFrame.destinationEndpoint = txMsgApsFrame->targetEndpoint;
    slApsFrame.groupId = 0;
    slApsFrame.sequence = 0;
    slApsFrame.radius = 0;

    // Set the appropriate APS options.
    slApsFrame.options = SL_ZIGBEE_APS_OPTION_NONE;
    if ((apsFlags & GMOS_ZIGBEE_APS_OPTION_RETRY) != 0) {
        slApsFrame.options |= SL_ZIGBEE_APS_OPTION_RETRY |
            SL_ZIGBEE_APS_OPTION_ENABLE_ROUTE_DISCOVERY;
    }

    // Send the EmberZNet APS message.
    slStatus = sl_zigbee_send_unicast (SL_ZIGBEE_OUTGOING_DIRECT,
        txMsgApsFrame->peer.nodeId, &slApsFrame, 0, msgSize, msgData,
        &apsSequence);

    // Release allocated buffer memory on successful transmission.
    if (slStatus == SL_STATUS_OK) {
        gmosBufferReset (&(txMsgApsFrame->payloadBuffer), 0);
        if (txMsgTag != NULL) {
            *txMsgTag = apsSequence;
        }
    }

    // Implement error code mapping on exit.
    return gmosZigbeeConvertEmberStatus (slStatus);
}

/*
 * This is the Zigbee radio specific APS message broadcast transmit
 * function which will be called to send the specified APS message.
 */
gmosZigbeeStatus_t gmosZigbeeStackApsBroadcastTransmit (
    gmosZigbeeStack_t* zigbeeStack, gmosZigbeeApsFrame_t* txMsgApsFrame,
    uint8_t* txMsgTag)
{
    (void) zigbeeStack;
    sl_status_t slStatus;
    sl_zigbee_aps_frame_t slApsFrame;
    uint8_t msgData [GMOS_ZIGBEE_APS_MESSAGE_BUFFER_SIZE];
    uint_fast16_t msgSize;
    uint8_t apsSequence;

    // Copy over the message buffer contents.
    msgSize = gmosBufferGetSize (&(txMsgApsFrame->payloadBuffer));
    if (msgSize <= sizeof (msgData)) {
        gmosBufferRead (
            &(txMsgApsFrame->payloadBuffer), 0, msgData, msgSize);
    } else {
        return GMOS_ZIGBEE_STATUS_MESSAGE_TOO_LONG;
    }

    // Construct the EmberZNet stack APS frame data structure.
    slApsFrame.profileId = txMsgApsFrame->profileId;
    slApsFrame.clusterId = txMsgApsFrame->clusterId;
    slApsFrame.sourceEndpoint = txMsgApsFrame->sourceEndpoint;
    slApsFrame.destinationEndpoint = txMsgApsFrame->targetEndpoint;
    slApsFrame.groupId = 0;
    slApsFrame.sequence = 0;
    slApsFrame.radius = txMsgApsFrame->apsMsgRadius;
    slApsFrame.options = SL_ZIGBEE_APS_OPTION_NONE;

    // Send the EmberZNet APS message.
    slStatus = sl_zigbee_send_broadcast (SL_ZIGBEE_NULL_NODE_ID,
        txMsgApsFrame->peer.broadcastType, 0, &slApsFrame,
        txMsgApsFrame->apsMsgRadius, 0, msgSize, msgData, &apsSequence);

    // Release allocated buffer memory on successful transmission.
    if (slStatus == SL_STATUS_OK) {
        gmosBufferReset (&(txMsgApsFrame->payloadBuffer), 0);
        if (txMsgTag != NULL) {
            *txMsgTag = apsSequence;
        }
    }

    // Implement error code mapping on exit.
    return gmosZigbeeConvertEmberStatus (slStatus);
}

/*
 * Implement the EmberZNet message received callback handler. This
 * converts the response to the platform independent message received
 * callback.
 */
void sl_zigbee_incoming_message_handler (
    sl_zigbee_incoming_message_type_t msgType, sl_zigbee_aps_frame_t *apsFrame,
    sl_zigbee_rx_packet_info_t *pktInfo, uint8_t msgSize, uint8_t *msgData)
{
    gmosZigbeeStack_t* zigbeeStack = gmosZigbeeRalEmberStackInstance;
    gmosZigbeeApsFrame_t rxMsgApsFrame;
    gmosBuffer_t txMsgApsBuffer = GMOS_BUFFER_INIT ();
    uint_fast16_t txMsgSize;

    // Copy over the message buffer contents. Just drop the message if
    // there is insufficient buffer memory available.
    GMOS_LOG_FMT (LOG_VERBOSE,
        "EmberZNet incoming APS message size %d and type 0x%02X.",
        msgSize, msgType);
    gmosBufferInit (&(rxMsgApsFrame.payloadBuffer));
    if (!gmosBufferAppend (
        &(rxMsgApsFrame.payloadBuffer), msgData, msgSize)) {
        return;
    }

    // Populate the platform independent APS data structure.
    rxMsgApsFrame.apsMsgType =
        gmosZigbeeConvertEmberIncomingMessageType (msgType);
    rxMsgApsFrame.apsMsgFlags =
        gmosZigbeeConvertEmberApsMessageFlags (apsFrame->options);
    rxMsgApsFrame.profileId = apsFrame->profileId;
    rxMsgApsFrame.clusterId = apsFrame->clusterId;
    rxMsgApsFrame.groupId = apsFrame->groupId;
    rxMsgApsFrame.peer.nodeId = pktInfo->sender_short_id;
    rxMsgApsFrame.sourceEndpoint = apsFrame->sourceEndpoint;
    rxMsgApsFrame.targetEndpoint = apsFrame->destinationEndpoint;
    rxMsgApsFrame.apsSequence = apsFrame->sequence;
    rxMsgApsFrame.apsMsgRadius = apsFrame->radius;

    // Forward the callback to the platform independent callback
    // handler.
    gmosZigbeeStackApsMessageReceived (
        zigbeeStack, &rxMsgApsFrame, &txMsgApsBuffer);

    // Attempt to send a response message if required. Send requests may
    // fail silently if there are insufficient stack resources, in which
    // case the next higher layer should treat it as a conventional
    // packet loss.
    txMsgSize = gmosBufferGetSize (&txMsgApsBuffer);
    if (txMsgSize > 0) {
        uint8_t txMsgData [GMOS_ZIGBEE_APS_MESSAGE_BUFFER_SIZE];
        sl_status_t slStatus;
        if (txMsgSize <= GMOS_ZIGBEE_APS_MESSAGE_BUFFER_SIZE) {
            gmosBufferRead (&txMsgApsBuffer, 0, txMsgData, txMsgSize);
            slStatus = sl_zigbee_send_reply (pktInfo->sender_short_id,
                apsFrame, txMsgSize, txMsgData);
            GMOS_LOG_FMT (LOG_VERBOSE,
                "EmberZNet APS response message size %d and status 0x%04X.",
                txMsgSize, slStatus);
        }
        gmosBufferReset (&txMsgApsBuffer, 0);
    }
}

/*
 * Implement the EmberZNet message sent callback handler. This converts
 * the response to the platform independent message sent callback.
 */
void sl_zigbee_message_sent_handler (sl_status_t slStatus,
    sl_zigbee_outgoing_message_type_t msgType, uint16_t indexOrDestination,
    sl_zigbee_aps_frame_t *apsFrame, uint16_t msgTag, uint8_t msgSize,
    uint8_t *msgData)
{
    (void) msgTag;
    gmosZigbeeStack_t* zigbeeStack = gmosZigbeeRalEmberStackInstance;
    gmosZigbeeStatus_t txMsgStatus;
    gmosZigbeeApsFrame_t txMsgApsFrame;
    uint8_t txMsgTag;

    // Map the reported message status.
    txMsgStatus = gmosZigbeeConvertEmberStatus (slStatus);

    // Copy over the message buffer contents. Just discard the payload
    // if there is insufficient buffer memory available - the next
    // higher layer will have to deal with it.
    gmosBufferInit (&(txMsgApsFrame.payloadBuffer));
    gmosBufferAppend (&(txMsgApsFrame.payloadBuffer), msgData, msgSize);

    // Populate the platform independent APS data structure.
    txMsgApsFrame.apsMsgType =
        gmosZigbeeConvertEmberOutgoingMessageType (msgType);
    txMsgApsFrame.apsMsgFlags =
        gmosZigbeeConvertEmberApsMessageFlags (apsFrame->options);
    txMsgApsFrame.profileId = apsFrame->profileId;
    txMsgApsFrame.clusterId = apsFrame->clusterId;
    txMsgApsFrame.groupId = apsFrame->groupId;
    txMsgApsFrame.peer.nodeId = indexOrDestination;
    txMsgApsFrame.sourceEndpoint = apsFrame->sourceEndpoint;
    txMsgApsFrame.targetEndpoint = apsFrame->destinationEndpoint;
    txMsgApsFrame.apsSequence = apsFrame->sequence;
    txMsgApsFrame.apsMsgRadius = apsFrame->radius;

    // Use the APS sequence number as the transmit message tag.
    txMsgTag = apsFrame->sequence;

    // Forward the callback to the platform independent callback
    // handler.
    gmosZigbeeStackApsMessageTransmitted (zigbeeStack,
        &txMsgApsFrame, txMsgStatus, txMsgTag);
}
