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
 * This file implements the common Zigbee API processing for ZDO server
 * support.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-buffers.h"
#include "gmos-zigbee-config.h"
#include "gmos-zigbee-stack.h"
#include "gmos-zigbee-aps.h"
#include "gmos-zigbee-endpoint.h"
#include "gmos-zigbee-zdo-common.h"
#include "gmos-zigbee-zdo-server.h"

/*
 * Send a ZDO unicast response message.
 */
static void sendZdoUnicastResponse (gmosZigbeeStack_t* zigbeeStack,
    gmosZigbeeApsFrame_t* rxApsFrame, gmosBuffer_t* responseBuffer)
{
    gmosZigbeeApsFrame_t txApsFrame;

    // Fill in the APS transmit frame parameters using the receive
    // frame values.
    txApsFrame.apsMsgType = GMOS_ZIGBEE_APS_MSG_TYPE_TX_UNICAST_DIRECT;
    txApsFrame.apsMsgFlags = GMOS_ZIGBEE_APS_OPTION_RETRY;
    txApsFrame.profileId = 0x0000;
    txApsFrame.clusterId = 0x8000 | rxApsFrame->clusterId;
    txApsFrame.groupId = 0x0000;
    txApsFrame.peer.nodeId = rxApsFrame->peer.nodeId;
    txApsFrame.sourceEndpoint = 0x00;
    txApsFrame.targetEndpoint = rxApsFrame->sourceEndpoint;
    txApsFrame.apsSequence = 0x00;

    // Zero copy move the contents of the reponse buffer to the APS
    // transmit frame.
    gmosBufferInit (&(txApsFrame.payloadBuffer));
    gmosBufferMove (responseBuffer, &(txApsFrame.payloadBuffer));

    // The request message context is lost on returning from this
    // callback, so transmit failures are not handled. The APS frame
    // payload buffer is reset in case of failure.
    gmosZigbeeApsUnicastTransmit (zigbeeStack, &txApsFrame, NULL, NULL);
    gmosBufferReset (&(txApsFrame.payloadBuffer), 0);
}

/*
 * Generates a list of active endpoints, appending the list to the
 * specified GubbinsMOS buffer.
 */
static inline bool getActiveEndpoints (
    gmosZigbeeStack_t* zigbeeStack, gmosBuffer_t* writeBuffer)
{
    gmosZigbeeEndpoint_t* appEndpoint;

    // Append the active endpoint IDs.
    appEndpoint = zigbeeStack->endpointList;
    while (appEndpoint != NULL) {
        uint8_t endpointId = appEndpoint->endpointId;
        if (!gmosBufferAppend (writeBuffer, &endpointId, 1)) {
            return false;
        }
        appEndpoint = appEndpoint->nextEndpoint;
    }
    return true;
}

/*
 * Generates a Zigbee device simple descriptor, appending the generated
 * descriptor in the specified GubbinsMOS buffer.
 */
static inline bool getSimpleDescriptor (
    gmosZigbeeEndpoint_t* appEndpoint, gmosBuffer_t* writeBuffer)
{
    uint8_t localData [7];
    uint8_t inputClusterCount;
    uint_fast16_t inputClusterCountIndex;
    uint8_t outputClusterCount;
    uint_fast16_t outputClusterCountIndex;
    uint_fast16_t initialBufferSize = gmosBufferGetSize (writeBuffer);
    gmosZigbeeCluster_t* appCluster;

    // Append the common header fields, including a placeholder for the
    // input cluster count.
    localData [0] = appEndpoint->endpointId;
    localData [1] = (uint8_t) appEndpoint->appProfileId;
    localData [2] = (uint8_t) (appEndpoint->appProfileId >> 8);
    localData [3] = (uint8_t) appEndpoint->appDeviceId;
    localData [4] = (uint8_t) (appEndpoint->appDeviceId >> 8);
    localData [5] = 0;
    localData [6] = 0;
    if (!gmosBufferAppend (writeBuffer, localData, 7)) {
        return false;
    }

    // Initialise the input cluster count.
    inputClusterCount = 0;
    inputClusterCountIndex = initialBufferSize + 6;
    appCluster = appEndpoint->clusterList;

    // Append the input cluster IDs.
    while (appCluster != NULL) {
        if ((appCluster->clusterOptions &
            GMOS_ZIGBEE_CLUSTER_OPTION_INPUT) != 0) {
            uint_fast16_t clusterId = appCluster->clusterId;
            localData [0] = (uint8_t) clusterId;
            localData [1] = (uint8_t) (clusterId >> 8);
            if (!gmosBufferAppend (writeBuffer, localData, 2)) {
                return false;
            }
            inputClusterCount += 1;
        }
        appCluster = appCluster->nextCluster;
    }

    // Append a placeholder for the output cluster count.
    if (!gmosBufferExtend (writeBuffer, 1)) {
        return false;
    }

    // Initialise the output cluster count.
    outputClusterCount = 0;
    outputClusterCountIndex =
        inputClusterCountIndex + 1 + (2 * inputClusterCount);
    appCluster = appEndpoint->clusterList;

    // Append the output cluster IDs.
    while (appCluster != NULL) {
        if ((appCluster->clusterOptions &
            GMOS_ZIGBEE_CLUSTER_OPTION_OUTPUT) != 0) {
            uint_fast16_t clusterId = appCluster->clusterId;
            localData [0] = (uint8_t) clusterId;
            localData [1] = (uint8_t) (clusterId >> 8);
            if (!gmosBufferAppend (writeBuffer, localData, 2)) {
                return false;
            }
            outputClusterCount += 1;
        }
        appCluster = appCluster->nextCluster;
    }

    // Update the cluster count fields.
    gmosBufferWrite (writeBuffer,
        inputClusterCountIndex, &inputClusterCount, 1);
    gmosBufferWrite (writeBuffer,
        outputClusterCountIndex, &outputClusterCount, 1);
    return true;
}

/*
 * Perform an endpoint descriptor match, given a match request that is
 * consistent with the match descriptor request format.
 */
static inline bool getEndpointDescriptorMatch (
    gmosZigbeeEndpoint_t* appEndpoint, gmosBuffer_t* matchBuffer,
    uint16_t matchBufferOffset, bool* match)
{
    uint8_t localData [2];
    uint_fast16_t profileId;
    uint_fast16_t clusterId;
    uint_fast16_t offset = matchBufferOffset;
    uint_fast8_t i;
    gmosZigbeeCluster_t* appCluster;

    // Check for matching profile ID.
    if (!gmosBufferRead (matchBuffer, offset, localData, 2)) {
        return false;
    }
    profileId = (uint_fast16_t) localData [0];
    profileId |= ((uint_fast16_t) (localData [1])) << 8;
    if (profileId != appEndpoint->appProfileId) {
        *match = false;
        return true;
    }
    offset += 2;

    // Get number of requested input clusters.
    if (!gmosBufferRead (matchBuffer, offset, localData, 1)) {
        return false;
    }
    offset += 1;

    // Check each requested input cluster ID in turn.
    for (i = localData [0]; i > 0; i--) {
        if (!gmosBufferRead (matchBuffer, offset, localData, 2)) {
            return false;
        }
        clusterId = (uint_fast16_t) localData [0];
        clusterId |= ((uint_fast16_t) (localData [1])) << 8;
        appCluster = gmosZigbeeClusterInstance (appEndpoint, clusterId);
        if ((appCluster != NULL) && ((appCluster->clusterOptions &
            GMOS_ZIGBEE_CLUSTER_OPTION_INPUT) != 0)) {
            *match = true;
            return true;
        }
        offset += 2;
    }

    // Get number of requested output clusters.
    if (!gmosBufferRead (matchBuffer, offset, localData, 1)) {
        return false;
    }
    offset += 1;

    // Check each requested output cluster ID in turn.
    for (i = localData [0]; i > 0; i--) {
        if (!gmosBufferRead (matchBuffer, offset, localData, 2)) {
            return false;
        }
        clusterId = (uint_fast16_t) localData [0];
        clusterId |= ((uint_fast16_t) (localData [1])) << 8;
        appCluster = gmosZigbeeClusterInstance (appEndpoint, clusterId);
        if ((appCluster != NULL) && ((appCluster->clusterOptions &
            GMOS_ZIGBEE_CLUSTER_OPTION_OUTPUT) != 0)) {
            *match = true;
            return true;
        }
        offset += 2;
    }

    // No matching cluster IDs found.
    *match = false;
    return true;
}

/*
 * Performs simple descriptor matching, appending the list of matching
 * endpoints to the specified GubbinsMOS buffer.
 */
static inline bool matchSimpleDescriptors (
    gmosZigbeeStack_t* zigbeeStack, gmosBuffer_t* writeBuffer,
    gmosBuffer_t* matchBuffer, uint16_t matchBufferOffset)
{
    gmosZigbeeEndpoint_t* appEndpoint;

    // Append the matching endpoint IDs.
    appEndpoint = zigbeeStack->endpointList;
    while (appEndpoint != NULL) {
        bool match;
        if (!getEndpointDescriptorMatch (appEndpoint,
            matchBuffer, matchBufferOffset, &match)) {
            return false;
        }
        if (match) {
            uint8_t endpointId = appEndpoint->endpointId;
            if (!gmosBufferAppend (writeBuffer, &endpointId, 1)) {
                return false;
            }
        }
        appEndpoint = appEndpoint->nextEndpoint;
    }
    return true;
}

/*
 * Process received active endpoint requests.
 */
static inline void processActiveEndpointRequest (
    gmosZigbeeStack_t* zigbeeStack, gmosZigbeeApsFrame_t* rxApsFrame)
{
    uint8_t requestPayload [3];
    uint_fast8_t zdoSequence;
    uint_fast16_t nwkAddrOfInterest;
    uint_fast8_t statusCode = GMOS_ZIGBEE_ZDO_STATUS_INVALID_REQUEST;
    uint8_t responsePayload [5];
    gmosBuffer_t responseBuffer = GMOS_BUFFER_INIT ();

    // Silently discard broadcast requests - only unicast requests are
    // supported for active endpoint requests.
    if (rxApsFrame->apsMsgType != GMOS_ZIGBEE_APS_MSG_TYPE_RX_UNICAST) {
        return;
    }

    // Extract the ZDO frame payload.
    if (!gmosBufferRead (&(rxApsFrame->payloadBuffer),
        0, requestPayload, sizeof (requestPayload))) {
        return;
    }
    zdoSequence = requestPayload [0];
    nwkAddrOfInterest = (uint_fast16_t) requestPayload [1];
    nwkAddrOfInterest |= ((uint_fast16_t) requestPayload [2]) << 8;

    // Allocate buffer space for the common response fields.
    if (!gmosBufferExtend (&responseBuffer, sizeof (responsePayload))) {
        return;
    }

    // Service the request if it is addressed to the local device.
    if (nwkAddrOfInterest == zigbeeStack->currentNodeId) {
        if (getActiveEndpoints (zigbeeStack, &responseBuffer)) {
            statusCode = GMOS_ZIGBEE_ZDO_STATUS_SUCCESS;
        } else {
            gmosBufferReset (&responseBuffer, 0);
            return;
        }
    }

    // End devices must match the network address of interest.
    else if ((GMOS_CONFIG_ZIGBEE_NODE_TYPE == GMOS_ZIGBEE_ACTIVE_CHILD_NODE) ||
        (GMOS_CONFIG_ZIGBEE_NODE_TYPE == GMOS_ZIGBEE_SLEEPY_CHILD_NODE) ||
        (GMOS_CONFIG_ZIGBEE_NODE_TYPE == GMOS_ZIGBEE_MOBILE_CHILD_NODE)) {
        statusCode = GMOS_ZIGBEE_ZDO_STATUS_INVALID_REQUEST;
    }

    // Child device management is not currently supported, so the parent
    // will always report that a child device is not found.
    else {
        statusCode = GMOS_ZIGBEE_ZDO_STATUS_DEVICE_NOT_FOUND;
    }

    // Fill in the common ZDO response fields.
    responsePayload [0] = zdoSequence;
    responsePayload [1] = statusCode;
    responsePayload [2] = (uint8_t) nwkAddrOfInterest;
    responsePayload [3] = (uint8_t) (nwkAddrOfInterest >> 8);
    responsePayload [4] = (uint8_t) (gmosBufferGetSize (&responseBuffer) - 5);
    gmosBufferWrite (&responseBuffer, 0,
        responsePayload, sizeof (responsePayload));

    // Send the APS response message.
    sendZdoUnicastResponse (zigbeeStack, rxApsFrame, &responseBuffer);
}

/*
 * Process received simple descriptor requests.
 */
static inline void processSimpleDescriptorRequest(
    gmosZigbeeStack_t* zigbeeStack, gmosZigbeeApsFrame_t* rxApsFrame)
{
    uint8_t requestPayload [4];
    uint_fast8_t zdoSequence;
    uint_fast16_t nwkAddrOfInterest;
    uint_fast8_t endpointId;
    uint_fast8_t statusCode = GMOS_ZIGBEE_ZDO_STATUS_INVALID_REQUEST;
    uint8_t responsePayload [5];
    gmosBuffer_t responseBuffer = GMOS_BUFFER_INIT ();

    // Silently discard broadcast requests - only unicast requests are
    // supported for simple descriptor requests.
    if (rxApsFrame->apsMsgType != GMOS_ZIGBEE_APS_MSG_TYPE_RX_UNICAST) {
        return;
    }

    // Extract the ZDO frame payload.
    if (!gmosBufferRead (&(rxApsFrame->payloadBuffer),
        0, requestPayload, sizeof (requestPayload))) {
        return;
    }
    zdoSequence = requestPayload [0];
    nwkAddrOfInterest = (uint_fast16_t) requestPayload [1];
    nwkAddrOfInterest |= ((uint_fast16_t) requestPayload [2]) << 8;
    endpointId = requestPayload [3];

    // Allocate buffer space for the common response fields.
    if (!gmosBufferExtend (&responseBuffer, sizeof (responsePayload))) {
        return;
    }

    // First check for invalid endpoint IDs.
    if ((endpointId < 1) || (endpointId > 240)) {
        statusCode = GMOS_ZIGBEE_ZDO_STATUS_INVALID_ENDPOINT;
    }

    // Service the request if it matches an active endpoint on the local
    // device.
    else if (nwkAddrOfInterest == zigbeeStack->currentNodeId) {
        gmosZigbeeEndpoint_t* appEndpoint;
        appEndpoint = gmosZigbeeEndpointInstance (
            zigbeeStack, endpointId);
        if (appEndpoint == NULL) {
            statusCode = GMOS_ZIGBEE_ZDO_STATUS_INACTIVE_ENDPOINT;
        } else if (getSimpleDescriptor (appEndpoint, &responseBuffer)) {
            statusCode = GMOS_ZIGBEE_ZDO_STATUS_SUCCESS;
        } else {
            gmosBufferReset (&responseBuffer, 0);
            return;
        }
    }

    // End devices must match the network address of interest.
    else if ((GMOS_CONFIG_ZIGBEE_NODE_TYPE == GMOS_ZIGBEE_ACTIVE_CHILD_NODE) ||
        (GMOS_CONFIG_ZIGBEE_NODE_TYPE == GMOS_ZIGBEE_SLEEPY_CHILD_NODE) ||
        (GMOS_CONFIG_ZIGBEE_NODE_TYPE == GMOS_ZIGBEE_MOBILE_CHILD_NODE)) {
        statusCode = GMOS_ZIGBEE_ZDO_STATUS_INVALID_REQUEST;
    }

    // Child device management is not currently supported, so the parent
    // will always report that a child device is not found.
    else {
        statusCode = GMOS_ZIGBEE_ZDO_STATUS_DEVICE_NOT_FOUND;
    }

    // Fill in the common ZDO response fields.
    responsePayload [0] = zdoSequence;
    responsePayload [1] = statusCode;
    responsePayload [2] = (uint8_t) nwkAddrOfInterest;
    responsePayload [3] = (uint8_t) (nwkAddrOfInterest >> 8);
    responsePayload [4] = (uint8_t) (gmosBufferGetSize (&responseBuffer) - 5);
    gmosBufferWrite (&responseBuffer, 0,
        responsePayload, sizeof (responsePayload));

    // Send the APS response message.
    sendZdoUnicastResponse (zigbeeStack, rxApsFrame, &responseBuffer);
}

/*
 * Process received match descriptor requests.
 */
static inline void processMatchDescriptorRequest(
    gmosZigbeeStack_t* zigbeeStack, gmosZigbeeApsFrame_t* rxApsFrame)
{
    uint8_t requestPayload [3];
    uint_fast8_t zdoSequence;
    uint_fast16_t nwkAddrOfInterest;
    uint_fast8_t statusCode = GMOS_ZIGBEE_ZDO_STATUS_INVALID_REQUEST;
    uint8_t responsePayload [5];
    uint_fast8_t matchedEndpointCount;
    gmosBuffer_t* requestBuffer;
    gmosBuffer_t responseBuffer = GMOS_BUFFER_INIT ();

    // Accept both unicast and broadcast requests. Broadcast loopbacks
    // are included in order to support match descriptor checks on the
    // local device.
    if ((rxApsFrame->apsMsgType != GMOS_ZIGBEE_APS_MSG_TYPE_RX_UNICAST) &&
        (rxApsFrame->apsMsgType != GMOS_ZIGBEE_APS_MSG_TYPE_RX_BROADCAST) &&
        (rxApsFrame->apsMsgType != GMOS_ZIGBEE_APS_MSG_TYPE_RX_BROADCAST_LOOPBACK)) {
        return;
    }

    // Extract the ZDO frame header.
    requestBuffer = &(rxApsFrame->payloadBuffer);
    if (!gmosBufferRead (requestBuffer, 0, requestPayload,
        sizeof (requestPayload))) {
        return;
    }
    zdoSequence = requestPayload [0];
    nwkAddrOfInterest = (uint_fast16_t) requestPayload [1];
    nwkAddrOfInterest |= ((uint_fast16_t) requestPayload [2]) << 8;

    // Allocate buffer space for the common response fields.
    if (!gmosBufferExtend (&responseBuffer, sizeof (responsePayload))) {
        return;
    }

    // Service the request if it is a broadcast or if it is addressed to
    // the local node.
    if ((nwkAddrOfInterest == GMOS_ZIGBEE_APS_BROADCAST_ALL_RX_IDLE) ||
        (nwkAddrOfInterest == zigbeeStack->currentNodeId)) {
        if (matchSimpleDescriptors (
            zigbeeStack, &responseBuffer, requestBuffer, 3)) {
            nwkAddrOfInterest = zigbeeStack->currentNodeId;
            statusCode = GMOS_ZIGBEE_ZDO_STATUS_SUCCESS;
        } else {
            gmosBufferReset (&responseBuffer, 0);
            return;
        }
    }

    // End devices must match the network address of interest.
    else if ((GMOS_CONFIG_ZIGBEE_NODE_TYPE == GMOS_ZIGBEE_ACTIVE_CHILD_NODE) ||
        (GMOS_CONFIG_ZIGBEE_NODE_TYPE == GMOS_ZIGBEE_SLEEPY_CHILD_NODE) ||
        (GMOS_CONFIG_ZIGBEE_NODE_TYPE == GMOS_ZIGBEE_MOBILE_CHILD_NODE)) {
        statusCode = GMOS_ZIGBEE_ZDO_STATUS_INVALID_REQUEST;
    }

    // Child device management is not currently supported, so the parent
    // will always report that a child device is not found.
    else {
        statusCode = GMOS_ZIGBEE_ZDO_STATUS_DEVICE_NOT_FOUND;
    }

    // A successful response is only generated if at least one endpoint
    // has been added to the matched endpoint list.
    matchedEndpointCount = (uint_fast8_t)
        gmosBufferGetSize (&responseBuffer) - sizeof (responsePayload);
    if ((statusCode == GMOS_ZIGBEE_ZDO_STATUS_SUCCESS) &&
        (matchedEndpointCount == 0)) {
        gmosBufferReset (&responseBuffer, 0);
        return;
    }

    // Fill in the common ZDO response fields.
    responsePayload [0] = zdoSequence;
    responsePayload [1] = statusCode;
    responsePayload [2] = (uint8_t) nwkAddrOfInterest;
    responsePayload [3] = (uint8_t) (nwkAddrOfInterest >> 8);
    responsePayload [4] = matchedEndpointCount;
    gmosBufferWrite (&responseBuffer, 0,
        responsePayload, sizeof (responsePayload));

    // Send the APS response message.
    sendZdoUnicastResponse (zigbeeStack, rxApsFrame, &responseBuffer);
}

/*
 * Process received end device announcements.
 */
static inline void processEndDeviceAnnouncement (
    gmosZigbeeStack_t* zigbeeStack, gmosZigbeeApsFrame_t* rxApsFrame)
{
    uint8_t requestPayload [12];
    uint_fast16_t nodeId;
    uint_fast8_t i;
    uint_fast8_t maxHandlers =
        GMOS_CONFIG_ZIGBEE_ZDO_SERVER_MAX_DEV_ANNCE_HANDLERS;
    gmosZigbeeZdoServerDevAnnceHandler devAnnceHandler;
    void* devAnnceCallbackData;

    // Read the message contents.
    if (!gmosBufferRead (&(rxApsFrame->payloadBuffer), 0,
        requestPayload, sizeof (requestPayload))) {
        return;
    }

    // Parse the 16-bit node ID.
    nodeId = (uint_fast16_t) requestPayload [1];
    nodeId |= ((uint_fast16_t) requestPayload [2]) << 8;
    GMOS_LOG_FMT (LOG_VERBOSE,
        "ZDO received device announcement for node ID %04X.", nodeId);

    // Issue the application callbacks.
    for (i = 0; i < maxHandlers; i++) {
        devAnnceHandler = (gmosZigbeeZdoServerDevAnnceHandler)
            zigbeeStack->zdoDevAnnceCallbacks [i];
        devAnnceCallbackData =
            zigbeeStack->zdoDevAnnceCallbackData [i];
        if (devAnnceHandler != NULL) {
            devAnnceHandler (zigbeeStack, devAnnceCallbackData, nodeId,
                &(requestPayload [3]), requestPayload [11]);
        }
    }
}

/*
 * This is the callback handler which will be called in order to notify
 * the common Zigbee stack implementation of a newly received ZDO
 * request message that should be processed by the ZDO server entity.
 */
void gmosZigbeeZdoServerRequestHandler (
    gmosZigbeeStack_t* zigbeeStack, gmosZigbeeApsFrame_t* rxMsgApsFrame)
{
    // Implement ZDO server handlers for the various supported ZDO
    // cluster IDs.
    switch (rxMsgApsFrame->clusterId) {

        // Process active endpoint requests.
        case GMOS_ZIGBEE_ZDO_CLUSTER_ACTIVE_ENDPOINT_REQUEST :
            processActiveEndpointRequest (zigbeeStack, rxMsgApsFrame);
            break;

        // Process simple descriptor requests.
        case GMOS_ZIGBEE_ZDO_CLUSTER_SIMPLE_DESCRIPTOR_REQUEST :
            processSimpleDescriptorRequest (zigbeeStack, rxMsgApsFrame);
            break;

        // Process match descriptor requests.
        case GMOS_ZIGBEE_ZDO_CLUSTER_MATCH_DESCRIPTOR_REQUEST :
            processMatchDescriptorRequest (zigbeeStack, rxMsgApsFrame);
            break;

        // The device announcement handler is optional, and is usually
        // only required by the coordinator.
        case GMOS_ZIGBEE_ZDO_CLUSTER_DEVICE_ANNOUNCE :
            if (GMOS_CONFIG_ZIGBEE_ZDO_SERVER_MAX_DEV_ANNCE_HANDLERS > 0) {
                processEndDeviceAnnouncement (zigbeeStack, rxMsgApsFrame);
            }
            break;

        // TODO: process unsupported ZDO requests. Broadcasts should be
        // silently discarded and unicasts should send 'not supported'.
        default :
            GMOS_LOG_FMT (LOG_DEBUG,
                "Unsupported ZDO cluster not handled: 0x%04X.",
                rxMsgApsFrame->clusterId);
            break;
    }
}

/*
 * Registers a ZDO server device announcement handler with the stack to
 * process ZDO end device announcements.
 */
bool gmosZigbeeZdoServerAddDevAnnceHandler (
    gmosZigbeeStack_t* zigbeeStack,
    gmosZigbeeZdoServerDevAnnceHandler devAnnceHandler,
    void* devAnnceCallbackData)
{
    bool registeredOk = false;
    uint_fast8_t i;
    uint_fast8_t maxHandlers =
        GMOS_CONFIG_ZIGBEE_ZDO_SERVER_MAX_DEV_ANNCE_HANDLERS;

    // Find an available callback handler slot.
    for (i = 0; i < maxHandlers; i++) {
        if (zigbeeStack->zdoDevAnnceCallbacks [i] == NULL) {
            zigbeeStack->zdoDevAnnceCallbacks [i] = devAnnceHandler;
            zigbeeStack->zdoDevAnnceCallbackData [i] = devAnnceCallbackData;
            registeredOk = true;
            break;
        }
    }
    return registeredOk;
}
