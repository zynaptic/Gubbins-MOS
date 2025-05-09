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
 * Library (ZCL) foundation components that are local to this device.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-scheduler.h"
#include "gmos-buffers.h"
#include "gmos-zigbee-zcl-core.h"
#include "gmos-zigbee-zcl-core-local.h"

/*
 * This enumeration defines the state space used by the ZCL local
 * message processing task.
 */
typedef enum {
    GMOS_ZIGBEE_ZCL_LOCAL_STATE_IDLE,
    GMOS_ZIGBEE_ZCL_LOCAL_STATE_START_REQUEST,
    GMOS_ZIGBEE_ZCL_LOCAL_STATE_READ_ATTR_REQ,
    GMOS_ZIGBEE_ZCL_LOCAL_STATE_READ_ATTR_WAIT,
    GMOS_ZIGBEE_ZCL_LOCAL_STATE_CHECK_ATTR_REQ,
    GMOS_ZIGBEE_ZCL_LOCAL_STATE_CHECK_ATTR_WAIT,
    GMOS_ZIGBEE_ZCL_LOCAL_STATE_CHECK_COMPLETE,
    GMOS_ZIGBEE_ZCL_LOCAL_STATE_WRITE_ATTR_REQ,
    GMOS_ZIGBEE_ZCL_LOCAL_STATE_WRITE_ATTR_WAIT,
    GMOS_ZIGBEE_ZCL_LOCAL_STATE_WRITE_COMPLETE,
    GMOS_ZIGBEE_ZCL_LOCAL_STATE_RESPONSE_SEND
} gmosZigbeeZclLocalState_t;

/*
 * Define a type safe wrapper for the ZCL local attribute command queue.
 */
GMOS_STREAM_DEFINITION (gmosZigbeeZclLocalCommandQueue,
    gmosZigbeeZclLocalCommandQueueEntry_t);

/*
 * Initiate attribute discovery request processing. This can be carried
 * out immediately, since all the attribute discovery information can be
 * accessed from conventional memory.
 */
static gmosZigbeeZclStatusCode_t gmosZigbeeZclLocalDiscoverAttrsRun (
    gmosZigbeeZclEndpoint_t* zclEndpoint)
{
    gmosZigbeeZclEndpointLocal_t* zclLocal = zclEndpoint->local;
    gmosZigbeeZclCluster_t* zclCluster = zclLocal->cluster;
    uint8_t zclPayload [3];
    uint8_t discoveryCompleteStatus;
    uint_fast16_t startingAttrId;
    uint_fast8_t maxAttrIds;
    uint_fast8_t discoveryCompleteOffset;
    uint_fast8_t attrCount;
    gmosZigbeeZclFrameHeader_t responseHeader;
    gmosBuffer_t* requestBuffer = &(zclLocal->requestBuffer);
    gmosBuffer_t* responseBuffer = &(zclLocal->responseBuffer);
    gmosZigbeeZclAttr_t* currentAttr;
    gmosZigbeeZclStatusCode_t commandStatus;

    // Populate the ZCL response frame header.
    responseHeader.frameControl =
        GMOS_ZIGBEE_ZCL_FRAME_CONTROL_TYPE_GENERAL |
        GMOS_ZIGBEE_ZCL_FRAME_CONTROL_NO_DEFAULT_RESP;
    responseHeader.vendorId = zclLocal->vendorId;
    responseHeader.zclSequence = zclLocal->zclSequence;
    responseHeader.zclFrameId =
        GMOS_ZIGBEE_ZCL_PROFILE_DISCOVER_ATTRS_RESPONSE;
    if (!gmosZigbeeZclPrependHeader (
        zclCluster, responseBuffer, &responseHeader)) {
        commandStatus = GMOS_ZIGBEE_ZCL_STATUS_NULL;
        goto out;
    }

    // Extract the ZCL request parameters from the payload buffer.
    if (gmosBufferGetSize (requestBuffer) != 3) {
        commandStatus = GMOS_ZIGBEE_ZCL_STATUS_MALFORMED_COMMAND;
        goto out;
    } else {
        gmosBufferRead (requestBuffer, 0, zclPayload, 3);
    }
    startingAttrId = zclPayload [0];
    startingAttrId |= ((uint_fast16_t) zclPayload [1]) << 8;
    maxAttrIds = zclPayload [2];

    // Append a placeholder byte for the discovery complete field.
    discoveryCompleteOffset = gmosBufferGetSize (responseBuffer);
    if (!gmosBufferExtend (responseBuffer, 1)) {
        commandStatus = GMOS_ZIGBEE_ZCL_STATUS_NULL;
        goto out;
    }

    // Determine the maximum number of response attribute IDs that can
    // be appended to the response buffer.
    attrCount = (gmosZigbeeZclMaxMessageSize (zclCluster) -
        discoveryCompleteOffset - 1) / 3;
    if (maxAttrIds < attrCount) {
        attrCount = maxAttrIds;
    }

    // Process the attributes in order of increasing attribute ID.
    // Select valid attributes depending on whether they are standard
    // attributes or manufacturer specific.
    currentAttr = zclCluster->attrList;
    discoveryCompleteStatus = 1;
    while (currentAttr != NULL) {
        uint_fast16_t currentVendorId = currentAttr->vendorId;
        uint_fast16_t currentAttrId = currentAttr->attrId;
        bool validAttr = false;
        if ((currentAttrId >= startingAttrId) &&
            (currentVendorId == zclLocal->vendorId)) {
            validAttr = true;
        }

        // Check for remaining attributes that indicate discovery is not
        // complete.
        if (validAttr && (attrCount == 0)) {
            discoveryCompleteStatus = 0;
            break;
        }

        // Append valid attributes to the response message.
        else if (validAttr) {
            uint8_t attrData [3];
            attrData [0] = (uint8_t) currentAttrId;
            attrData [1] = (uint8_t) (currentAttrId >> 8);
            attrData [2] = currentAttr->attrType;
            if (!gmosBufferAppend (responseBuffer, attrData, 3)) {
                commandStatus = GMOS_ZIGBEE_ZCL_STATUS_NULL;
                goto out;
            }
            attrCount -= 1;
        }
        currentAttr = currentAttr->nextAttr;
    }

    // Update the discovery complete status flag on completion.
    gmosBufferWrite (responseBuffer, discoveryCompleteOffset,
        &discoveryCompleteStatus, 1);
    zclLocal->state = GMOS_ZIGBEE_ZCL_LOCAL_STATE_RESPONSE_SEND;
    commandStatus = GMOS_ZIGBEE_ZCL_STATUS_SUCCESS;

    // Return command execution status.
out :
    return commandStatus;
}

/*
 * Initiate read attributes request processing.
 */
static gmosZigbeeZclStatusCode_t gmosZigbeeZclLocalReadAttrsRun (
    gmosZigbeeZclEndpoint_t* zclEndpoint)
{
    gmosZigbeeZclEndpointLocal_t* zclLocal = zclEndpoint->local;
    gmosZigbeeZclCluster_t* zclCluster = zclLocal->cluster;
    gmosZigbeeZclFrameHeader_t responseHeader;
    gmosBuffer_t* responseBuffer = &(zclLocal->responseBuffer);
    gmosZigbeeZclStatusCode_t commandStatus;

    // Populate the ZCL response frame header.
    responseHeader.frameControl =
        GMOS_ZIGBEE_ZCL_FRAME_CONTROL_TYPE_GENERAL |
        GMOS_ZIGBEE_ZCL_FRAME_CONTROL_NO_DEFAULT_RESP;
    responseHeader.vendorId = zclLocal->vendorId;
    responseHeader.zclSequence = zclLocal->zclSequence;
    responseHeader.zclFrameId =
        GMOS_ZIGBEE_ZCL_PROFILE_READ_ATTRS_RESPONSE;

    // Set up the response buffer for attribute read requests.
    gmosBufferReset (responseBuffer, 0);
    if (!gmosZigbeeZclPrependHeader (
        zclCluster, responseBuffer, &responseHeader)) {
        commandStatus = GMOS_ZIGBEE_ZCL_STATUS_NULL;
        goto out;
    }

    // Set up the first attribute read transaction. Select the starting
    // index for the request buffer and the starting offset for the
    // response buffer.
    zclLocal->state = GMOS_ZIGBEE_ZCL_LOCAL_STATE_READ_ATTR_REQ;
    zclLocal->count = 0;
    zclLocal->offset = gmosBufferGetSize (responseBuffer);
    commandStatus = GMOS_ZIGBEE_ZCL_STATUS_SUCCESS;

    // Return command execution status.
out :
    return commandStatus;
}

/*
 * Initiate standard write attributes request processing.
 */
static gmosZigbeeZclStatusCode_t gmosZigbeeZclLocalWriteAttrsRun (
    gmosZigbeeZclEndpoint_t* zclEndpoint)
{
    gmosZigbeeZclEndpointLocal_t* zclLocal = zclEndpoint->local;
    gmosZigbeeZclCluster_t* zclCluster = zclLocal->cluster;
    gmosZigbeeZclFrameHeader_t responseHeader;
    gmosBuffer_t* responseBuffer = &(zclLocal->responseBuffer);
    gmosZigbeeZclStatusCode_t commandStatus;

    // Only populate the ZCL response frame header if a response is
    // required.
    gmosBufferReset (responseBuffer, 0);
    if (zclLocal->zclFrameId !=
        GMOS_ZIGBEE_ZCL_PROFILE_WRITE_ATTRS_SILENT_REQUEST) {
        responseHeader.frameControl =
            GMOS_ZIGBEE_ZCL_FRAME_CONTROL_TYPE_GENERAL |
            GMOS_ZIGBEE_ZCL_FRAME_CONTROL_NO_DEFAULT_RESP;
        responseHeader.vendorId = zclLocal->vendorId;
        responseHeader.zclSequence = zclLocal->zclSequence;
        responseHeader.zclFrameId =
            GMOS_ZIGBEE_ZCL_PROFILE_WRITE_ATTRS_RESPONSE;

        // Set up the response buffer for attribute write requests.
        if (!gmosZigbeeZclPrependHeader (
            zclCluster, responseBuffer, &responseHeader)) {
            commandStatus = GMOS_ZIGBEE_ZCL_STATUS_NULL;
            goto out;
        }
    }

    // Select normal writes or attribute validation for atomic writes.
    if (zclLocal->zclFrameId ==
        GMOS_ZIGBEE_ZCL_PROFILE_WRITE_ATTRS_ATOMIC_REQUEST) {
        zclLocal->state = GMOS_ZIGBEE_ZCL_LOCAL_STATE_CHECK_ATTR_REQ;
    } else {
        zclLocal->state = GMOS_ZIGBEE_ZCL_LOCAL_STATE_WRITE_ATTR_REQ;
    }

    // Select the starting offset for the request buffer and the
    // starting index for the response buffer.
    zclLocal->count = 0;
    zclLocal->offset = 0;
    commandStatus = GMOS_ZIGBEE_ZCL_STATUS_SUCCESS;

    // Return command execution status.
out :
    return commandStatus;
}

/*
 * Initiate a new attribute read request.
 */
static inline gmosTaskStatus_t gmosZigbeeZclLocalReadAttrReq (
    gmosZigbeeZclEndpoint_t* zclEndpoint)
{
    gmosZigbeeZclEndpointLocal_t* zclLocal = zclEndpoint->local;
    gmosZigbeeZclCluster_t* zclCluster = zclLocal->cluster;
    gmosZigbeeZclAttr_t* zclAttr;
    gmosBuffer_t* requestBuffer = &(zclLocal->requestBuffer);
    gmosBuffer_t* responseBuffer = &(zclLocal->responseBuffer);
    uint8_t attrData [3];
    uint_fast8_t attrIndex;
    uint_fast8_t attrCount;
    uint_fast16_t attrId;
    uint_fast16_t responseSize;

    // Check to see if the end of the attribute list has been reached.
    attrIndex = zclLocal->count;
    attrCount = gmosBufferGetSize (requestBuffer) / 2;
    if (attrIndex >= attrCount) {
        zclLocal->state = GMOS_ZIGBEE_ZCL_LOCAL_STATE_RESPONSE_SEND;
        return GMOS_TASK_RUN_IMMEDIATE;
    }

    // Check to see if the maximum response size has been exceeded,
    // in which case the last attribute record is trimmed.
    responseSize = gmosBufferGetSize (responseBuffer);
    if (responseSize > gmosZigbeeZclMaxMessageSize (zclCluster)) {
        gmosBufferResize (responseBuffer, zclLocal->offset);
        zclLocal->state = GMOS_ZIGBEE_ZCL_LOCAL_STATE_RESPONSE_SEND;
        return GMOS_TASK_RUN_IMMEDIATE;
    }
    zclLocal->offset = responseSize;

    // Get the requested attribute instance.
    gmosBufferRead (requestBuffer, attrIndex * 2, attrData, 2);
    attrId = (uint_fast16_t) attrData [0];
    attrId |= ((uint_fast16_t) attrData [1]) << 8;
    zclAttr = gmosZigbeeZclGetAttrInstance (
        zclCluster, zclLocal->vendorId, attrId);

    // Handle requests for missing attributes.
    if (zclAttr == NULL) {
        attrData [2] = GMOS_ZIGBEE_ZCL_STATUS_UNSUP_ATTRIBUTE;
        if (!gmosBufferAppend (responseBuffer, attrData, 3)) {
            zclLocal->state = GMOS_ZIGBEE_ZCL_LOCAL_STATE_RESPONSE_SEND;
        } else {
            zclLocal->count += 1;
        }
        return GMOS_TASK_RUN_IMMEDIATE;
    }

    // Format the common attribute response fields, assuming that the
    // attribute read will be successful.
    attrData [2] = GMOS_ZIGBEE_ZCL_STATUS_SUCCESS;
    if (!gmosBufferAppend (responseBuffer, attrData, 3)) {
        zclLocal->state = GMOS_ZIGBEE_ZCL_LOCAL_STATE_RESPONSE_SEND;
        return GMOS_TASK_RUN_IMMEDIATE;
    }

    // For dynamically accessed attributes, run the attribute getter
    // function to append the value to the response buffer. If the
    // access complete callback is made during this call, the current
    // state will be updated to indicate that this is not a long running
    // operation.
    if ((zclAttr->attrOptions &
        GMOS_ZIGBEE_ZCL_ATTR_OPTION_DYNAMIC_ACCESS) != 0) {
        zclLocal->state = GMOS_ZIGBEE_ZCL_LOCAL_STATE_READ_ATTR_WAIT;
        zclAttr->attrData.dynamic.getter (
            zclEndpoint, zclAttr, responseBuffer);
    }

    // For conventional attributes, use the common data serialization
    // function to append the data to the response buffer. On failure
    // to allocate sufficient buffer memory, discard the attribute and
    // send the truncated response.
    else if (!gmosZigbeeZclSerializeAttrData (zclAttr, responseBuffer)) {
        gmosBufferResize (responseBuffer, zclLocal->offset);
        zclLocal->state = GMOS_ZIGBEE_ZCL_LOCAL_STATE_RESPONSE_SEND;
    }

    // Suspend the task if the attribute request is a long running
    // operation, otherwise immediately move on to the next attribute.
    zclLocal->count += 1;
    if (zclLocal->state == GMOS_ZIGBEE_ZCL_LOCAL_STATE_READ_ATTR_WAIT) {
        return GMOS_TASK_SUSPEND;
    } else {
        return GMOS_TASK_RUN_IMMEDIATE;
    }
}

/*
 * Initiate a new attribute write request.
 */
static inline gmosTaskStatus_t gmosZigbeeZclLocalWriteAttrReq (
    gmosZigbeeZclEndpoint_t* zclEndpoint)
{
    gmosZigbeeZclEndpointLocal_t* zclLocal = zclEndpoint->local;
    gmosZigbeeZclCluster_t* zclCluster = zclLocal->cluster;
    gmosZigbeeZclAttr_t* zclAttr;
    gmosBuffer_t* requestBuffer = &(zclLocal->requestBuffer);
    gmosBuffer_t* responseBuffer = &(zclLocal->responseBuffer);
    uint8_t attrData [3];
    uint8_t attrSize;
    uint_fast8_t attrOffset;
    uint_fast16_t attrId;
    uint_fast8_t attrType;
    uint_fast8_t attrStatus;
    uint_fast16_t requestSize;
    bool validFieldSize;
    bool commitWrite;
    bool responseRequired;

    // Determine if an attribute update or response message is being
    // generated for this write request command.
    if (zclLocal->state == GMOS_ZIGBEE_ZCL_LOCAL_STATE_CHECK_ATTR_REQ) {
        commitWrite = false;
    } else {
        commitWrite = true;
    }
    if (gmosBufferGetSize (responseBuffer) == 0) {
        responseRequired = false;
    } else {
        responseRequired = true;
    }

    // Check to see if the end of the attribute list has been reached.
    // No check is carried out on the response message size, since it
    // will always be smaller than the request message.
    attrOffset = zclLocal->offset;
    requestSize = gmosBufferGetSize (requestBuffer);
    GMOS_LOG_FMT (LOG_VERBOSE,
        "Running write attr request (attr offset %d, request size %d).",
        attrOffset, requestSize);
    if (attrOffset >= requestSize - 3) {
        if (commitWrite) {
            zclLocal->state = GMOS_ZIGBEE_ZCL_LOCAL_STATE_WRITE_COMPLETE;
        } else {
            zclLocal->state = GMOS_ZIGBEE_ZCL_LOCAL_STATE_CHECK_COMPLETE;
        }
        return GMOS_TASK_RUN_IMMEDIATE;
    }

    // Get the requested attribute instance and expected type.
    gmosBufferRead (requestBuffer, attrOffset, attrData, 3);
    attrId = (uint_fast16_t) attrData [0];
    attrId |= ((uint_fast16_t) attrData [1]) << 8;
    attrType = attrData [2];
    zclAttr = gmosZigbeeZclGetAttrInstance (
        zclCluster, zclLocal->vendorId, attrId);

    // Look up the size of the attribute data, as encoded in the write
    // request. If this can not be determined, the current field is
    // taken to contain invalid data.
    attrStatus = gmosZigbeeZclParseDataSize (
        requestBuffer, attrOffset + 2, &attrSize);
    if (attrStatus != GMOS_ZIGBEE_ZCL_STATUS_SUCCESS) {
        validFieldSize = false;
    }

    // Handle requests where the request field can be parsed, but the
    // write transaction can not be executed.
    else {
        validFieldSize = true;
        zclLocal->offset += 3 + attrSize;
        if (zclAttr == NULL) {
            attrStatus = GMOS_ZIGBEE_ZCL_STATUS_UNSUP_ATTRIBUTE;
        } else if (zclAttr->attrType != attrType) {
            attrStatus = GMOS_ZIGBEE_ZCL_STATUS_INVALID_DATA_TYPE;
        } else if ((zclAttr->attrOptions &
            GMOS_ZIGBEE_ZCL_ATTR_OPTION_REMOTE_WRITE_EN) == 0) {
            attrStatus = GMOS_ZIGBEE_ZCL_STATUS_READ_ONLY;
        }
    }

    // For conventional attributes, use the common data parsing function
    // to update the stored attribute value.
    if (attrStatus == GMOS_ZIGBEE_ZCL_STATUS_SUCCESS) {
        if ((zclAttr->attrOptions &
            GMOS_ZIGBEE_ZCL_ATTR_OPTION_DYNAMIC_ACCESS) == 0) {
            attrStatus = gmosZigbeeZclParseAttrData (
                zclAttr, requestBuffer, attrOffset + 2, commitWrite);
        }
    }

    // Skip further processing of invalid requests. The response record
    // is only added if a response message has been requested.
    if (attrStatus != GMOS_ZIGBEE_ZCL_STATUS_SUCCESS) {
        if (responseRequired) {
            attrData [2] = attrStatus;
            if (!gmosBufferAppend (responseBuffer, attrData, 3)) {
                zclLocal->state = GMOS_ZIGBEE_ZCL_LOCAL_STATE_RESPONSE_SEND;
            }
        }

        // Since it was not possible to determine the size of the write
        // attribute data for the current field, any further write
        // attribute data fields cannot be processed.
        if (!validFieldSize) {
            zclLocal->state = GMOS_ZIGBEE_ZCL_LOCAL_STATE_RESPONSE_SEND;
        }
        zclLocal->count += 1;
        return GMOS_TASK_RUN_IMMEDIATE;
    }

    // For dynamically accessed attributes, run the attribute setter
    // function to parse the value from the request buffer. If the
    // access complete callback is made during this call, the current
    // state will be updated to indicate that this is not a long running
    // operation. A default failure response is appended to the response
    // buffer if required.
    if ((zclAttr->attrOptions &
        GMOS_ZIGBEE_ZCL_ATTR_OPTION_DYNAMIC_ACCESS) != 0) {
        if (responseRequired) {
            attrData [2] = GMOS_ZIGBEE_ZCL_STATUS_FAILURE;
            if (!gmosBufferAppend (responseBuffer, attrData, 3)) {
                zclLocal->state = GMOS_ZIGBEE_ZCL_LOCAL_STATE_RESPONSE_SEND;
            }
        }
        if (commitWrite) {
            zclLocal->state = GMOS_ZIGBEE_ZCL_LOCAL_STATE_WRITE_ATTR_WAIT;
        } else {
            zclLocal->state = GMOS_ZIGBEE_ZCL_LOCAL_STATE_CHECK_ATTR_WAIT;
        }
        zclAttr->attrData.dynamic.setter (zclEndpoint, zclAttr,
            requestBuffer, attrOffset + 2, commitWrite);
    }

    // Suspend the task if the attribute request is a long running
    // operation, otherwise immediately move on to the next attribute.
    if ((zclLocal->state == GMOS_ZIGBEE_ZCL_LOCAL_STATE_WRITE_ATTR_WAIT) ||
        (zclLocal->state == GMOS_ZIGBEE_ZCL_LOCAL_STATE_CHECK_ATTR_WAIT)) {
        return GMOS_TASK_SUSPEND;
    } else {
        return GMOS_TASK_RUN_IMMEDIATE;
    }
}

/*
 * Complete processing of ZCL write check cycle.
 */
static inline void gmosZigbeeZclLocalCheckAttrsComplete (
    gmosZigbeeZclEndpoint_t* zclEndpoint)
{
    gmosZigbeeZclEndpointLocal_t* zclLocal = zclEndpoint->local;

    // The count field contains the number of failed attribute write
    // checks. If is is zero, a full attribute write cycle can be
    // initiated.
    if (zclLocal->count == 0) {
        zclLocal->offset = 0;
        zclLocal->state = GMOS_ZIGBEE_ZCL_LOCAL_STATE_WRITE_ATTR_REQ;
    }

    // If any of the attribute write requests has failed, the generated
    // response message will be sent without modification.
    else {
        zclLocal->state = GMOS_ZIGBEE_ZCL_LOCAL_STATE_RESPONSE_SEND;
    }
}

/*
 * Complete processing of ZCL write update cycle.
 */
static inline void gmosZigbeeZclLocalWriteAttrsComplete (
    gmosZigbeeZclEndpoint_t* zclEndpoint)
{
    gmosZigbeeZclEndpointLocal_t* zclLocal = zclEndpoint->local;
    gmosBuffer_t* responseBuffer = &(zclLocal->responseBuffer);
    uint8_t status = GMOS_ZIGBEE_ZCL_STATUS_SUCCESS;

    // No further processing is required if a response message has not
    // been requested.
    if (gmosBufferGetSize (responseBuffer) == 0) {
        zclLocal->state = GMOS_ZIGBEE_ZCL_LOCAL_STATE_IDLE;
    }

    // The count field contains the number of failed attribute write
    // checks. If is is zero, a single success status value needs to
    // be appended to the message. This should always succeed.
    else if (zclLocal->count == 0) {
        gmosBufferAppend (responseBuffer, &status, 1);
        zclLocal->state = GMOS_ZIGBEE_ZCL_LOCAL_STATE_RESPONSE_SEND;
    }

    // Otherwise schedule the response message containing the failure
    // notifications for transmission.
    else {
        zclLocal->state = GMOS_ZIGBEE_ZCL_LOCAL_STATE_RESPONSE_SEND;
    }
}

/*
 * Send a ZCL response message on completion.
 */
static inline gmosTaskStatus_t gmosZigbeeZclLocalTransmitResponse (
    gmosZigbeeZclEndpoint_t* zclEndpoint)
{
    gmosZigbeeStack_t* zigbeeStack =
        zclEndpoint->baseEndpoint.zigbeeStack;
    gmosZigbeeZclEndpointLocal_t* zclLocal = zclEndpoint->local;
    gmosZigbeeZclCluster_t* zclCluster = zclLocal->cluster;
    gmosZigbeeApsFrame_t apsFrame;
    gmosZigbeeStatus_t apsStatus;

    // Discard the contents of the request buffer.
    gmosBufferReset (&(zclLocal->requestBuffer), 0);

    // Copy the response message to the APS frame buffer.
    gmosBufferInit (&(apsFrame.payloadBuffer));
    gmosBufferMove (&(zclLocal->responseBuffer),
        &(apsFrame.payloadBuffer));

    // Populate the APS frame header.
    apsFrame.apsMsgType = GMOS_ZIGBEE_APS_MSG_TYPE_TX_UNICAST_DIRECT;
    apsFrame.apsMsgFlags = GMOS_ZIGBEE_APS_OPTION_RETRY;
    apsFrame.profileId = zclEndpoint->baseEndpoint.appProfileId;
    apsFrame.clusterId = zclCluster->baseCluster.clusterId;
    apsFrame.groupId = 0;
    apsFrame.peer.nodeId = zclLocal->peerNodeId;
    apsFrame.sourceEndpoint = zclEndpoint->baseEndpoint.endpointId;
    apsFrame.targetEndpoint = zclLocal->peerEndpointId;
    apsFrame.apsSequence = 0;

    // Attempt to send the APS message.
    apsStatus = gmosZigbeeApsUnicastTransmit (zigbeeStack,
        &apsFrame, NULL, NULL);

    // On success continue without waiting for a message sent callback,
    // since there is no requirement for application level retry
    // attempts.
    if (apsStatus == GMOS_ZIGBEE_STATUS_SUCCESS) {
        zclLocal->state = GMOS_ZIGBEE_ZCL_LOCAL_STATE_IDLE;
        return GMOS_TASK_RUN_IMMEDIATE;
    }

    // On retry, move the APS payload back to the local response buffer
    // for subsequent retry attempts.
    else if (apsStatus == GMOS_ZIGBEE_STATUS_RETRY) {
        gmosBufferMove (&(apsFrame.payloadBuffer),
            &(zclLocal->responseBuffer));
        return GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (25));
    }

    // On all other error conditions silently discard the request.
    else {
        GMOS_LOG_FMT (LOG_WARNING,
            "Discarded ZCL response (status 0x%02X).", apsStatus);
        zclLocal->state = GMOS_ZIGBEE_ZCL_LOCAL_STATE_IDLE;
        return GMOS_TASK_RUN_IMMEDIATE;
    }
}

/*
 * Wait for a new local command and copy the command parameters to local
 * task storage.
 */
static inline gmosTaskStatus_t gmosZigbeeZclLocalCommandWait (
    gmosZigbeeZclEndpoint_t* zclEndpoint)
{
    gmosZigbeeZclEndpointLocal_t* zclLocal = zclEndpoint->local;
    gmosZigbeeZclLocalCommandQueueEntry_t commandQueueEntry;

    // Always release any residual buffer data from prior requests.
    gmosBufferReset (&(zclLocal->requestBuffer), 0);
    gmosBufferReset (&(zclLocal->responseBuffer), 0);

    // If possible, get the next command from the queue.
    if (!gmosZigbeeZclLocalCommandQueue_read (
        &(zclLocal->commandQueue), &commandQueueEntry)) {
        return GMOS_TASK_SUSPEND;
    }

    // Copy the command parameters to local task storage.
    zclLocal->cluster = commandQueueEntry.zclCluster;
    zclLocal->zclFrameId = commandQueueEntry.zclFrameId;
    zclLocal->zclSequence = commandQueueEntry.zclSequence;
    zclLocal->vendorId = commandQueueEntry.vendorId;
    zclLocal->peerNodeId = commandQueueEntry.peerNodeId;
    zclLocal->peerEndpointId = commandQueueEntry.peerEndpointId;
    gmosBufferMove (&(commandQueueEntry.zclPayloadBuffer),
         &(zclLocal->requestBuffer));

    // Start processing the next command.
    zclLocal->state = GMOS_ZIGBEE_ZCL_LOCAL_STATE_START_REQUEST;
    return GMOS_TASK_RUN_IMMEDIATE;
}

/*
 * Initiate queued command processing using the command parameters
 * previously copied to local task storage.
 */
static inline gmosTaskStatus_t gmosZigbeeZclLocalCommandStart (
    gmosZigbeeZclEndpoint_t* zclEndpoint)
{
    gmosZigbeeZclEndpointLocal_t* zclLocal = zclEndpoint->local;
    gmosZigbeeZclStatusCode_t commandStatus;
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;

    // Select the appropriate command processing state.
    switch (zclLocal->zclFrameId) {

        // Initiate attribute discovery requests.
        case GMOS_ZIGBEE_ZCL_PROFILE_DISCOVER_ATTRS_REQUEST :
            commandStatus =
                gmosZigbeeZclLocalDiscoverAttrsRun (zclEndpoint);
            break;

        // Initiate read attribute requests.
        case GMOS_ZIGBEE_ZCL_PROFILE_READ_ATTRS_REQUEST :
            commandStatus =
                gmosZigbeeZclLocalReadAttrsRun (zclEndpoint);
            break;

        // Initiate write attribute requests.
        case GMOS_ZIGBEE_ZCL_PROFILE_WRITE_ATTRS_REQUEST :
        case GMOS_ZIGBEE_ZCL_PROFILE_WRITE_ATTRS_ATOMIC_REQUEST :
        case GMOS_ZIGBEE_ZCL_PROFILE_WRITE_ATTRS_SILENT_REQUEST :
            commandStatus =
                gmosZigbeeZclLocalWriteAttrsRun (zclEndpoint);
            break;

        // Indicate unsupported request frames.
        default :
            commandStatus = GMOS_ZIGBEE_ZCL_STATUS_UNSUP_COMMAND;
            break;
    }

    // Retry the request at a later time if it cannot be serviced due to
    // lack of resources. Clears out any residual data from the response
    // buffer and tries command processing again after a short delay.
    if (commandStatus == GMOS_ZIGBEE_ZCL_STATUS_NULL) {
        gmosBufferReset (&(zclLocal->responseBuffer), 0);
        taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (25));
    }

    // Release allocated buffer memory on failure.
    // TODO: default status response.
    else if (commandStatus != GMOS_ZIGBEE_ZCL_STATUS_SUCCESS) {
        gmosBufferReset (&(zclLocal->requestBuffer), 0);
        gmosBufferReset (&(zclLocal->responseBuffer), 0);
        zclLocal->state = GMOS_ZIGBEE_ZCL_LOCAL_STATE_IDLE;
    }
    return taskStatus;
}

/*
 * Implement task handler function for processing local messages and
 * generating report messages.
 */
static gmosTaskStatus_t gmosZigbeeZclLocalTaskHandler (void* taskData)
{
    gmosZigbeeZclEndpoint_t* zclEndpoint =
        (gmosZigbeeZclEndpoint_t*) taskData;
    gmosZigbeeZclEndpointLocal_t* zclLocal = zclEndpoint->local;
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;

    // Implement attribute command processing state machine. Note that
    // most state transitions are implemented in the associated state
    // handler functions.
    switch (zclLocal->state) {

        // From the idle state, check the local command queue for new
        // commands or scheduled reports.
        case GMOS_ZIGBEE_ZCL_LOCAL_STATE_IDLE :
            taskStatus = gmosZigbeeZclLocalCommandWait (zclEndpoint);
            break;

        // Start processing a new request by dispatching it to the
        // correct request handler.
        case GMOS_ZIGBEE_ZCL_LOCAL_STATE_START_REQUEST :
            taskStatus = gmosZigbeeZclLocalCommandStart (zclEndpoint);
            break;

        // In the read attribute request state, initiate the read
        // request for the next attribute value.
        case GMOS_ZIGBEE_ZCL_LOCAL_STATE_READ_ATTR_REQ :
            taskStatus = gmosZigbeeZclLocalReadAttrReq (zclEndpoint);
            break;

        // In the write attribute request and check states, initiate the
        // write request for the next attribute value.
        case GMOS_ZIGBEE_ZCL_LOCAL_STATE_CHECK_ATTR_REQ :
        case GMOS_ZIGBEE_ZCL_LOCAL_STATE_WRITE_ATTR_REQ :
            taskStatus = gmosZigbeeZclLocalWriteAttrReq (zclEndpoint);
            break;

        // Process the results of an attribute write checking cycle.
        case GMOS_ZIGBEE_ZCL_LOCAL_STATE_CHECK_COMPLETE :
            gmosZigbeeZclLocalCheckAttrsComplete (zclEndpoint);
            break;

        // Process the results of an attribute write commit cycle.
        case GMOS_ZIGBEE_ZCL_LOCAL_STATE_WRITE_COMPLETE :
            gmosZigbeeZclLocalWriteAttrsComplete (zclEndpoint);
            break;

        // Transmit a ZCL response message after it has been formatted.
        case GMOS_ZIGBEE_ZCL_LOCAL_STATE_RESPONSE_SEND :
            taskStatus = gmosZigbeeZclLocalTransmitResponse (zclEndpoint);
            break;
    }
    return taskStatus;
}

/*
 * Performs a one-time initialisation of a ZCL endpoint local message
 * handler. This will only be called if a valid local message processing
 * data structure is present.
 */
void gmosZigbeeZclLocalEndpointInit (
    gmosZigbeeZclEndpoint_t* zclEndpoint)
{
    gmosZigbeeZclEndpointLocal_t* zclLocal = zclEndpoint->local;

    // Set up local message processing state.
    zclLocal->state = GMOS_ZIGBEE_ZCL_LOCAL_STATE_IDLE;
    zclLocal->zclSequence = 0;
    zclLocal->vendorId = GMOS_ZIGBEE_ZCL_STANDARD_VENDOR_ID;
    zclLocal->cluster = NULL;
    gmosBufferInit (&(zclLocal->requestBuffer));
    gmosBufferInit (&(zclLocal->responseBuffer));

    // Initialise local message processing task.
    zclLocal->task.taskTickFn = gmosZigbeeZclLocalTaskHandler;
    zclLocal->task.taskData = zclEndpoint;
    zclLocal->task.taskName = "ZCL Local Endpoint";
    gmosSchedulerTaskStart (&(zclLocal->task));

    // Initialise the local attribute command queue.
    gmosZigbeeZclLocalCommandQueue_init (
        &(zclLocal->commandQueue), &(zclLocal->task),
        GMOS_CONFIG_ZIGBEE_ZCL_LOCAL_COMMAND_QUEUE_LENGTH);
}

/*
 * Queues a long running ZCL attribute command request for subsequent
 * processing.
 */
gmosZigbeeZclStatusCode_t gmosZigbeeZclLocalAttrCommandQueueRequest (
    gmosZigbeeZclCluster_t* zclCluster, uint16_t peerNodeId,
    uint8_t peerEndpointId, gmosZigbeeZclFrameHeader_t* frameHeader,
    gmosBuffer_t* zclPayloadBuffer)
{
    gmosZigbeeEndpoint_t* appEndpoint;
    gmosZigbeeZclEndpoint_t* zclEndpoint;
    gmosZigbeeZclEndpointLocal_t* zclLocal;
    gmosZigbeeZclLocalCommandQueueEntry_t commandQueueEntry;

    // Resolve the ZCL endpoint instance from the specified ZCL cluster.
    // This relies on the wrapped application endpoint being the first
    // item in the ZCL endpoint data structure.
    appEndpoint = zclCluster->baseCluster.hostEndpoint;
    zclEndpoint = (gmosZigbeeZclEndpoint_t*) appEndpoint;
    zclLocal = zclEndpoint->local;

    // Local attribute processing may not be supported for this cluster.
    if (zclLocal == NULL) {
        return GMOS_ZIGBEE_ZCL_STATUS_UNSUP_COMMAND;
    }

    // Format the standard command queue entry fields.
    commandQueueEntry.zclCluster = zclCluster;
    commandQueueEntry.vendorId = frameHeader->vendorId;
    commandQueueEntry.zclSequence = frameHeader->zclSequence;
    commandQueueEntry.zclFrameId = frameHeader->zclFrameId;
    commandQueueEntry.peerNodeId = peerNodeId;
    commandQueueEntry.peerEndpointId = peerEndpointId;

    // Move the ZCL payload buffer contents into the command queue
    // entry. This resets the contents of the ZCL request buffer.
    gmosBufferInit (&(commandQueueEntry.zclPayloadBuffer));
    gmosBufferMove (zclPayloadBuffer, &(commandQueueEntry.zclPayloadBuffer));

    // Queue the ZCL attribute command request for subsequent
    // processing. On failure discard the payload buffer contents.
    if (!gmosZigbeeZclLocalCommandQueue_write (
        &(zclLocal->commandQueue), &commandQueueEntry)) {
        gmosBufferReset (&(commandQueueEntry.zclPayloadBuffer), 0);
        return GMOS_ZIGBEE_ZCL_STATUS_FAILURE;
    } else {
        return GMOS_ZIGBEE_ZCL_STATUS_NULL;
    }
}

/*
 * Implement local attribute access complete handler. This should be
 * called from each attribute access function or state machine to
 * indicate that the attribute processing has completed. This does not
 * currently support the use of attribute access retries, which may
 * need to be added for heavily loaded systems.
 */
void gmosZigbeeZclLocalAttrAccessComplete (
    gmosZigbeeZclEndpoint_t* zclEndpoint, uint8_t status)
{
    gmosZigbeeZclEndpointLocal_t* zclLocal = zclEndpoint->local;
    gmosBuffer_t* responseBuffer = &(zclLocal->responseBuffer);
    uint_fast8_t responseBufferSize = gmosBufferGetSize (responseBuffer);
    uint_fast8_t nextState;

    // Select default state transition on completion.
    switch (zclLocal->state) {
        case GMOS_ZIGBEE_ZCL_LOCAL_STATE_READ_ATTR_WAIT :
            nextState = GMOS_ZIGBEE_ZCL_LOCAL_STATE_READ_ATTR_REQ;
            break;
        case GMOS_ZIGBEE_ZCL_LOCAL_STATE_CHECK_ATTR_WAIT :
            nextState = GMOS_ZIGBEE_ZCL_LOCAL_STATE_CHECK_ATTR_REQ;
            break;
        case GMOS_ZIGBEE_ZCL_LOCAL_STATE_WRITE_ATTR_WAIT :
            nextState = GMOS_ZIGBEE_ZCL_LOCAL_STATE_WRITE_ATTR_REQ;
            break;
        default :
            nextState = zclLocal->state;
            break;
    }

    // Select the completion handling based on the currently active
    // transaction state.
    switch (zclLocal->state) {

        // Process the completion of an attribute read. On a buffer
        // allocation failure discard the entire record and send the
        // partial response. On other failure conditions, discard any
        // spurious data that may have been added to the buffer and
        // overwrite the status field.
        case GMOS_ZIGBEE_ZCL_LOCAL_STATE_READ_ATTR_WAIT :
            if (status == GMOS_ZIGBEE_ZCL_STATUS_NULL) {
                gmosBufferResize (responseBuffer, zclLocal->offset);
                nextState = GMOS_ZIGBEE_ZCL_LOCAL_STATE_RESPONSE_SEND;
            } else {
                if (status != GMOS_ZIGBEE_ZCL_STATUS_SUCCESS) {
                    gmosBufferResize (responseBuffer,
                        zclLocal->offset + 3);
                    gmosBufferWrite (responseBuffer,
                        zclLocal->offset + 2, &status, 1);
                }
            }
            break;

        // Process the completion of an attribute write or write check
        // operation. If successful, the excess status response is
        // trimmed from the response message. Otherwise the status is
        // updated to the reported value.
        case GMOS_ZIGBEE_ZCL_LOCAL_STATE_CHECK_ATTR_WAIT :
        case GMOS_ZIGBEE_ZCL_LOCAL_STATE_WRITE_ATTR_WAIT :
            if (responseBufferSize != 0) {
                if (status == GMOS_ZIGBEE_ZCL_STATUS_SUCCESS) {
                    gmosBufferResize (
                        responseBuffer, responseBufferSize - 3);
                } else {
                    gmosBufferWrite (responseBuffer,
                        responseBufferSize - 1, &status, 1);
                    zclLocal->count += 1;
                }
            }
            break;
    }
    zclLocal->state = nextState;
    gmosSchedulerTaskResume (&(zclLocal->task));
}
