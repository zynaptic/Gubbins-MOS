/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2024-2025 Zynaptic Limited
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
 * This file implements command processing support for Zigbee Cluster
 * Library (ZCL) foundation components that are located on a remote
 * device.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-scheduler.h"
#include "gmos-buffers.h"
#include "gmos-zigbee-zcl-core.h"
#include "gmos-zigbee-zcl-core-remote.h"

/*
 * Specify the ZCL transaction timeout. This is set at twice the
 * standard APS indirect transmission timeout for Zigbee Pro networks
 * (7680 ms) plus a bit extra for good measure.
 */
#define ZCL_TRANSACTION_TIMEOUT (GMOS_MS_TO_TICKS (20000))

/*
 * Implement the ZCL sequence counter as a static variable, so that it
 * can be shared by multiple ZCL client instances if required.
 */
static uint8_t zclSequenceCounter = 0;

/*
 * Implement ZCL transaction sequence number generation. Bit 8 will
 * be used to differentiate between host initiated unicast (clear) and
 * broadcast (set) transactions.
 */
static inline uint8_t gmosZigbeeZclRemoteGetUnicastSequenceId (void)
{
    return (zclSequenceCounter & 0x7F);
}

static inline uint8_t gmosZigbeeZclRemoteGetBroadcastSequenceId (void)
{
    return (zclSequenceCounter | 0x80);
}

static inline bool gmosZigbeeZclRemoteIsHostUnicastResponse (
    uint8_t sequenceId)
{
    return ((sequenceId & 0x80) == 0);
}

/*
 * Implement ZCL transaction timeout management task.
 */
static gmosTaskStatus_t gmosZigbeeZclRemoteTaskHandler (void* taskData)
{
    gmosZigbeeZclEndpoint_t* zclEndpoint =
        (gmosZigbeeZclEndpoint_t*) taskData;
    gmosZigbeeZclEndpointRemote_t* zclRemote = zclEndpoint->remote;
    gmosZigbeeCluster_t* appCluster;
    gmosZigbeeZclCluster_t* zclCluster;
    gmosZigbeeZclRemoteResultHandler_t resultHandler;
    gmosZigbeeZclFrameHeader_t frameHeader;
    void* localData;
    uint_fast16_t clusterId;
    uint_fast8_t slot;
    int32_t timeout;
    uint32_t nextDelay = 0xFFFFFFFF;

    // Check each active ZCL transaction slot in turn.
    for (slot = 0; slot < GMOS_CONFIG_ZIGBEE_ZCL_REMOTE_MAX_REQUESTS; slot++) {
        resultHandler = zclRemote->resultHandlers [slot];
        if (resultHandler != NULL) {

            // Resolve the cluster instance from the stored cluster ID.
            clusterId = zclRemote->activeClusters [slot];
            appCluster = gmosZigbeeClusterInstance (
                &(zclEndpoint->baseEndpoint), clusterId);
            zclCluster = (gmosZigbeeZclCluster_t*) appCluster;

            // If the timeout has expired, generate a timeout callback
            // with no message payload and complete the transaction.
            timeout = (int32_t) zclRemote->requestTimeouts [slot] -
                (int32_t) gmosPalGetTimer ();
            if (timeout <= 0) {
                gmosBuffer_t emptyBuffer = GMOS_BUFFER_INIT ();
                localData = zclRemote->localDataItems [slot];
                frameHeader.vendorId = GMOS_ZIGBEE_ZCL_STANDARD_VENDOR_ID;
                frameHeader.frameControl = 0xFF;
                frameHeader.zclSequence = zclRemote->sequenceValues [slot];
                frameHeader.zclFrameId = 0xFF;
                resultHandler (zclCluster, localData,
                    GMOS_ZIGBEE_ZCL_STATUS_TIMEOUT, true,
                    0xFFFF, 0xFF, &frameHeader, &emptyBuffer);
                zclRemote->resultHandlers [slot] = NULL;
                gmosBufferReset (&emptyBuffer, 0);
            }

            // If the timeout has not expired, reshedule the timeout
            // handler task to run again.
            else if (((uint32_t) timeout) < nextDelay) {
                nextDelay = (uint32_t) timeout;
            }
        }
    }

    // Reschedule the task if another timeout is pending.
    if (nextDelay == 0xFFFFFFFF) {
        return GMOS_TASK_SUSPEND;
    } else {
        GMOS_LOG_FMT (LOG_DEBUG,
            "Setting next ZCL transaction timeout to %d ticks.",
            nextDelay);
        return GMOS_TASK_RUN_LATER (nextDelay);
    }
}

/*
 * Implement common unicast request logic.
 */
static bool gmosZigbeeZclRemoteUnicastRequest (
    gmosZigbeeZclCluster_t* zclCluster,
    gmosZigbeeZclRemoteResultHandler_t resultHandler,
    void* localData, uint8_t zclSequenceId, uint16_t remoteNodeId,
    uint8_t remoteEndpointId, gmosBuffer_t* payload)
{
    gmosZigbeeStack_t* zigbeeStack;
    gmosZigbeeEndpoint_t* appEndpoint;
    gmosZigbeeZclEndpoint_t* zclEndpoint;
    gmosZigbeeZclEndpointRemote_t* zclRemote;
    gmosZigbeeApsFrame_t apsFrame;
    gmosZigbeeStatus_t status;
    uint32_t zclTimeout;
    uint_fast8_t slot;

    // Resolve the endpoint instance from the ZCL cluster.
    appEndpoint = zclCluster->baseCluster.hostEndpoint;
    zigbeeStack = appEndpoint->zigbeeStack;
    zclEndpoint = (gmosZigbeeZclEndpoint_t*) appEndpoint;
    zclRemote = zclEndpoint->remote;

    // Check for an available ZCL transaction slot.
    for (slot = 0; slot < GMOS_CONFIG_ZIGBEE_ZCL_REMOTE_MAX_REQUESTS; slot++) {
        if (zclRemote->resultHandlers [slot] == NULL) {
            break;
        }
    }
    if (slot >= GMOS_CONFIG_ZIGBEE_ZCL_REMOTE_MAX_REQUESTS) {
        gmosBufferReset (payload, 0);
        return false;
    }

    // Fill in the APS parameters.
    apsFrame.apsMsgType = GMOS_ZIGBEE_APS_MSG_TYPE_TX_UNICAST_DIRECT;
    apsFrame.apsMsgFlags = GMOS_ZIGBEE_APS_OPTION_RETRY;
    apsFrame.profileId = appEndpoint->appProfileId;
    apsFrame.clusterId = zclCluster->baseCluster.clusterId;
    apsFrame.groupId = 0x0000;
    apsFrame.peer.nodeId = remoteNodeId;
    apsFrame.sourceEndpoint = appEndpoint->endpointId;
    apsFrame.targetEndpoint = remoteEndpointId;
    apsFrame.apsSequence = 0x00;

    // Perform a zero copy move to the APS message payload buffer.
    gmosBufferInit (&(apsFrame.payloadBuffer));
    gmosBufferMove (payload, &(apsFrame.payloadBuffer));

    // Attempt to send the APS message. Note that no message sent
    // callback is included, since failures are treated in the same way
    // as in-flight message loss and handled by the ZCL transaction
    // timeout.
    status = gmosZigbeeApsUnicastTransmit (
        zigbeeStack, &apsFrame, NULL, NULL);

    // All error conditions are used to imply a retry.
    if (status != GMOS_ZIGBEE_STATUS_SUCCESS) {
        gmosBufferReset (&(apsFrame.payloadBuffer), 0);
        return false;
    }

    // On success, populate the ZCL transaction slot.
    zclTimeout = gmosPalGetTimer () + ZCL_TRANSACTION_TIMEOUT;
    zclRemote->resultHandlers [slot] = resultHandler;
    zclRemote->localDataItems [slot] = localData;
    zclRemote->sequenceValues [slot] = zclSequenceId;
    zclRemote->requestTimeouts [slot] = zclTimeout;
    zclRemote->activeClusters [slot] = zclCluster->baseCluster.clusterId;
    zclSequenceCounter += 1;

    // Ensure that the transaction timeout task is running.
    gmosSchedulerTaskResume (&(zclRemote->task));
    return true;
}

/*
 * Performs a one-time initialisation of a ZCL endpoint remote message
 * handler. This will only be called if a valid remote message
 * processing data structure is present.
 */
void gmosZigbeeZclRemoteEndpointInit (
    gmosZigbeeZclEndpoint_t* zclEndpoint)
{
    gmosZigbeeZclEndpointRemote_t* zclRemote = zclEndpoint->remote;
    uint_fast8_t i;

    // Reset all the ZCL transaction states.
    for (i = 0; i < GMOS_CONFIG_ZIGBEE_ZCL_REMOTE_MAX_REQUESTS; i++) {
        zclRemote->resultHandlers [i] = NULL;
    }

    // Initialise remote message processing task.
    zclRemote->task.taskTickFn = gmosZigbeeZclRemoteTaskHandler;
    zclRemote->task.taskData = zclEndpoint;
    zclRemote->task.taskName = "ZCL Remote Endpoint";
    gmosSchedulerTaskStart (&(zclRemote->task));
}

/*
 * Issues a unicast attribute discovery request with the specified
 * parameters.
 */
bool gmosZigbeeZclRemoteAttrDiscoveryRequest (
    gmosZigbeeZclCluster_t* zclCluster,
    gmosZigbeeZclRemoteResultHandler_t resultHandler, void* localData,
    uint16_t remoteNodeId, uint8_t remoteEndpointId,
    uint16_t vendorId, uint16_t startAttrId, uint8_t maxAttrIds)
{
    gmosZigbeeZclFrameHeader_t frameHeader;
    gmosBuffer_t dataBuffer = GMOS_BUFFER_INIT ();
    uint8_t payloadData [3];
    uint_fast8_t zclSequenceId;

    // Construct the frame header for the request.
    zclSequenceId = gmosZigbeeZclRemoteGetUnicastSequenceId ();
    frameHeader.vendorId = vendorId;
    frameHeader.zclSequence = zclSequenceId;
    frameHeader.frameControl =
        GMOS_ZIGBEE_ZCL_FRAME_CONTROL_TYPE_GENERAL;
    frameHeader.zclFrameId =
        GMOS_ZIGBEE_ZCL_PROFILE_DISCOVER_ATTRS_REQUEST;

    // Initialise the ZCL data buffer with the frame header.
    if (!gmosZigbeeZclPrependHeader (
        zclCluster, &dataBuffer, &frameHeader)) {
        return false;
    }

    // Format and append the ZCL payload.
    payloadData [0] = (uint8_t) startAttrId;
    payloadData [1] = (uint8_t) (startAttrId >> 8);
    payloadData [2] = maxAttrIds;
    if (!gmosBufferAppend (
        &dataBuffer, payloadData, sizeof (payloadData))) {
        gmosBufferReset (&dataBuffer, 0);
        return false;
    }

    // Attempt to send the unicast request.
    return gmosZigbeeZclRemoteUnicastRequest (zclCluster,
        resultHandler, localData, zclSequenceId, remoteNodeId,
        remoteEndpointId, &dataBuffer);
}

/*
 * Issues a unicast ZCL attribute read request with the specified
 * parameters.
 */
bool gmosZigbeeZclRemoteAttrReadRequest (
    gmosZigbeeZclCluster_t* zclCluster,
    gmosZigbeeZclRemoteResultHandler_t resultHandler, void* localData,
    uint16_t remoteNodeId, uint8_t remoteEndpointId, uint16_t vendorId,
    uint16_t attrIdList[], uint8_t attrIdCount)
{
    gmosZigbeeZclFrameHeader_t frameHeader;
    gmosBuffer_t dataBuffer = GMOS_BUFFER_INIT ();
    uint8_t attrIdData [2];
    uint_fast8_t zclSequenceId;
    uint_fast8_t i;

    // Construct the frame header for the request.
    zclSequenceId = gmosZigbeeZclRemoteGetUnicastSequenceId ();
    frameHeader.vendorId = vendorId;
    frameHeader.zclSequence = zclSequenceId;
    frameHeader.frameControl =
        GMOS_ZIGBEE_ZCL_FRAME_CONTROL_TYPE_GENERAL;
    frameHeader.zclFrameId =
        GMOS_ZIGBEE_ZCL_PROFILE_READ_ATTRS_REQUEST;

    // Initialise the ZCL data buffer with the frame header.
    if (!gmosZigbeeZclPrependHeader (
        zclCluster, &dataBuffer, &frameHeader)) {
        return false;
    }

    // Format and append the attribute ID list.
    for (i = 0; i < attrIdCount; i++) {
        uint16_t attrIdValue = attrIdList [i];
        attrIdData [0] = (uint8_t) attrIdValue;
        attrIdData [1] = (uint8_t) (attrIdValue >> 8);
        if (!gmosBufferAppend (
            &dataBuffer, attrIdData, sizeof (attrIdData))) {
            gmosBufferReset (&dataBuffer, 0);
            return false;
        }
    }

    // Attempt to send the unicast request.
    return gmosZigbeeZclRemoteUnicastRequest (zclCluster,
        resultHandler, localData, zclSequenceId, remoteNodeId,
        remoteEndpointId, &dataBuffer);
}

/*
 * Issues a unicast ZCL attribute write request with the specified
 * parameters.
 */
bool gmosZigbeeZclRemoteAttrWriteRequest (
    gmosZigbeeZclCluster_t* zclCluster,
    gmosZigbeeZclRemoteResultHandler_t resultHandler, void* localData,
    uint16_t remoteNodeId, uint8_t remoteEndpointId, uint16_t vendorId,
    gmosZigbeeZclDataRecord_t attrDataList[], uint8_t attrDataCount,
    bool atomicWrite)
{
    gmosZigbeeZclFrameHeader_t frameHeader;
    gmosBuffer_t dataBuffer = GMOS_BUFFER_INIT ();
    uint_fast8_t zclSequenceId;
    uint_fast8_t i;

    // Construct the frame header for the request.
    zclSequenceId = gmosZigbeeZclRemoteGetUnicastSequenceId ();
    frameHeader.vendorId = vendorId;
    frameHeader.zclSequence = zclSequenceId;
    frameHeader.frameControl =
        GMOS_ZIGBEE_ZCL_FRAME_CONTROL_TYPE_GENERAL;

    // Select the write request mode.
    if (resultHandler == NULL) {
        frameHeader.zclFrameId =
            GMOS_ZIGBEE_ZCL_PROFILE_WRITE_ATTRS_SILENT_REQUEST;
    } else if (atomicWrite) {
        frameHeader.zclFrameId =
            GMOS_ZIGBEE_ZCL_PROFILE_WRITE_ATTRS_ATOMIC_REQUEST;
    } else {
        frameHeader.zclFrameId =
            GMOS_ZIGBEE_ZCL_PROFILE_WRITE_ATTRS_REQUEST;
    }

    // Initialise the ZCL data buffer with the frame header.
    if (!gmosZigbeeZclPrependHeader (
        zclCluster, &dataBuffer, &frameHeader)) {
        return false;
    }

    // Format and append the attribute data record list.
    for (i = 0; i < attrDataCount; i++) {
        if (!gmosZigbeeZclSerializeDataRecord
            (attrDataList + i, &dataBuffer)) {
            gmosBufferReset (&dataBuffer, 0);
            return false;
        }
    }

    // Attempt to send the unicast request.
    return gmosZigbeeZclRemoteUnicastRequest (zclCluster,
        resultHandler, localData, zclSequenceId, remoteNodeId,
        remoteEndpointId, &dataBuffer);
}

/*
 * Handles incoming ZCL remote attribute access responses.
 */
gmosZigbeeZclStatusCode_t gmosZigbeeZclRemoteAttrResponseHandler (
    gmosZigbeeZclCluster_t* zclCluster, uint16_t peerNodeId,
    uint8_t peerEndpointId, gmosZigbeeZclFrameHeader_t* zclFrameHeader,
    gmosBuffer_t* zclPayloadBuffer)
{
    gmosZigbeeEndpoint_t* appEndpoint;
    gmosZigbeeZclEndpoint_t* zclEndpoint;
    gmosZigbeeZclEndpointRemote_t* zclRemote;
    gmosZigbeeZclRemoteResultHandler_t resultHandler;
    uint_fast8_t slot;
    uint_fast8_t zclSequence;
    bool completed;
    void* localData;

    // Resolve the endpoint instance from the ZCL cluster.
    appEndpoint = zclCluster->baseCluster.hostEndpoint;
    zclEndpoint = (gmosZigbeeZclEndpoint_t*) appEndpoint;
    zclRemote = zclEndpoint->remote;

    // Check for a matching ZCL transaction sequence number. Silently
    // discard responses with no matching transaction.
    zclSequence = zclFrameHeader->zclSequence;
    for (slot = 0; slot < GMOS_CONFIG_ZIGBEE_ZCL_REMOTE_MAX_REQUESTS; slot++) {
        if ((zclRemote->resultHandlers [slot] != NULL) &&
            (zclRemote->sequenceValues [slot] == zclSequence)) {
            break;
        }
    }
    if (slot >= GMOS_CONFIG_ZIGBEE_ZCL_REMOTE_MAX_REQUESTS) {
        return GMOS_ZIGBEE_ZCL_STATUS_NULL;
    }

    // Dispatch the response to the appropriate response handler.
    resultHandler = zclRemote->resultHandlers [slot];
    localData = zclRemote->localDataItems [slot];
    completed = gmosZigbeeZclRemoteIsHostUnicastResponse (zclSequence);
    resultHandler (zclCluster, localData,
        GMOS_ZIGBEE_ZCL_STATUS_SUCCESS, completed, peerNodeId,
        peerEndpointId, zclFrameHeader, zclPayloadBuffer);

    // Remove the callback handler on completion.
    if (completed) {
        zclRemote->resultHandlers [slot] = NULL;
    }
    return GMOS_ZIGBEE_ZCL_STATUS_NULL;
}
