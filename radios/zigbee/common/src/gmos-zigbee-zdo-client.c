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
 * This file implements the common Zigbee API processing for ZDO client
 * support.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-scheduler.h"
#include "gmos-buffers.h"
#include "gmos-zigbee-config.h"
#include "gmos-zigbee-stack.h"
#include "gmos-zigbee-aps.h"
#include "gmos-zigbee-zdo-common.h"
#include "gmos-zigbee-zdo-client.h"

/*
 * Specify the ZDO transaction timeout. This is set at twice the
 * standard APS indirect transmission timeout for Zigbee Pro networks
 * (7680 ms) plus a bit extra for good measure.
 */
#define ZDO_TRANSACTION_TIMEOUT (GMOS_MS_TO_TICKS (20000))

/*
 * Implement ZDO transaction sequence number generation. For the EZSP
 * based network coprocessors, the ZDO sequence number range from 0x80
 * to 0xFF is reserved for ZDO transactions initiated by the NCP
 * firmware. This may not be applicable to other vendor stacks so this
 * may subsequently need to be moved to the RAL. Since bit 8 is used to
 * differentiate between NCP and host initiated transactions, bit 7 will
 * be used to differentiate between host initiated unicast (clear) and
 * broadcast (set) transactions.
 */
static inline uint8_t gmosZigbeeZdoClientGetUnicastSequenceId (
    gmosZigbeeZdoClient_t* zdoClient)
{
    return (zdoClient->sequenceCounter & 0x3F);
}

static inline uint8_t gmosZigbeeZdoClientGetBroadcastSequenceId (
    gmosZigbeeZdoClient_t* zdoClient)
{
    return ((zdoClient->sequenceCounter & 0x3F) | 0x40);
}

static inline bool gmosZigbeeZdoClientIsHostUnicastResponse (
    uint_fast8_t sequenceId)
{
    return ((sequenceId & 0xC0) == 0);
}

/*
 * Implement common unicast request logic.
 */
static bool gmosZigbeeZdoClientUnicastRequest (
    gmosZigbeeZdoClient_t* zdoClient,
    gmosZigbeeZdoClientResultHandler_t resultHandler, void* localData,
    uint_fast8_t zdoSequenceId, uint_fast16_t remoteNodeId,
    uint_fast16_t clusterId, gmosBuffer_t* payload)
{
    gmosZigbeeApsFrame_t apsFrame;
    gmosZigbeeStatus_t status;
    uint32_t zdoTimeout;
    uint_fast8_t slot;

    // Check for an available ZDO transaction slot.
    if (resultHandler != NULL) {
        for (slot = 0;
            slot < GMOS_CONFIG_ZIGBEE_ZDO_CLIENT_MAX_REQUESTS; slot++) {
            if (zdoClient->resultHandlers [slot] == NULL) {
                break;
            }
        }
        if (slot >= GMOS_CONFIG_ZIGBEE_ZDO_CLIENT_MAX_REQUESTS) {
            gmosBufferReset (payload, 0);
            return false;
        }
    }

    // Fill in the APS parameters.
    apsFrame.apsMsgType = GMOS_ZIGBEE_APS_MSG_TYPE_TX_UNICAST_DIRECT;
    apsFrame.apsMsgFlags = GMOS_ZIGBEE_APS_OPTION_RETRY;
    apsFrame.profileId = 0x0000;
    apsFrame.clusterId = clusterId;
    apsFrame.groupId = 0x0000;
    apsFrame.peer.nodeId = remoteNodeId;
    apsFrame.sourceEndpoint = 0x00;
    apsFrame.targetEndpoint = 0x00;
    apsFrame.apsSequence = 0x00;
    apsFrame.apsMsgRadius = 0;

    // Perform a zero copy move to the APS message payload buffer.
    gmosBufferInit (&(apsFrame.payloadBuffer));
    gmosBufferMove (payload, &(apsFrame.payloadBuffer));

    // Attempt to send the APS message. Note that no message sent
    // callback is included, since failures are treated in the same way
    // as in-flight message loss and handled by the ZDO transaction
    // timeout.
    status = gmosZigbeeApsUnicastTransmit (
        zdoClient->zigbeeStack, &apsFrame, NULL, NULL);

    // All error conditions are used to imply a retry.
    if (status != GMOS_ZIGBEE_STATUS_SUCCESS) {
        gmosBufferReset (&(apsFrame.payloadBuffer), 0);
        return false;
    }

    // On success, populate the ZDO transaction slot.
    if (resultHandler != NULL) {
        zdoTimeout = gmosPalGetTimer () + ZDO_TRANSACTION_TIMEOUT;
        zdoClient->resultHandlers [slot] = resultHandler;
        zdoClient->localDataItems [slot] = localData;
        zdoClient->sequenceValues [slot] = zdoSequenceId;
        zdoClient->requestTimeouts [slot] = zdoTimeout;
    }

    // Ensure that the transaction timeout task is running.
    zdoClient->sequenceCounter += 1;
    gmosSchedulerTaskResume (&(zdoClient->timeoutTask));
    return true;
}

/*
 * Implement common broadcast request logic.
 */
static bool gmosZigbeeZdoClientBroadcastRequest (
    gmosZigbeeZdoClient_t* zdoClient,
    gmosZigbeeZdoClientResultHandler_t resultHandler, void* localData,
    uint_fast8_t zdoSequenceId, gmosZigbeeApsBroadcastType_t broadcastType,
    uint_fast16_t clusterId, gmosBuffer_t* payload)
{
    gmosZigbeeApsFrame_t apsFrame;
    gmosZigbeeStatus_t status;
    uint32_t zdoTimeout;
    uint_fast8_t slot;

    // Check for an available ZDO transaction slot.
    if (resultHandler != NULL) {
        for (slot = 0;
            slot < GMOS_CONFIG_ZIGBEE_ZDO_CLIENT_MAX_REQUESTS; slot++) {
            if (zdoClient->resultHandlers [slot] == NULL) {
                break;
            }
        }
        if (slot >= GMOS_CONFIG_ZIGBEE_ZDO_CLIENT_MAX_REQUESTS) {
            gmosBufferReset (payload, 0);
            return false;
        }
    }

    // Fill in the APS parameters.
    apsFrame.apsMsgType = GMOS_ZIGBEE_APS_MSG_TYPE_TX_BROADCAST;
    apsFrame.apsMsgFlags = GMOS_ZIGBEE_APS_OPTION_NONE;
    apsFrame.profileId = 0x0000;
    apsFrame.clusterId = clusterId;
    apsFrame.groupId = 0x0000;
    apsFrame.peer.broadcastType = broadcastType;
    apsFrame.sourceEndpoint = 0x00;
    apsFrame.targetEndpoint = 0x00;
    apsFrame.apsSequence = 0x00;
    apsFrame.apsMsgRadius = 0;

    // Perform a zero copy move to the APS message payload buffer.
    gmosBufferInit (&(apsFrame.payloadBuffer));
    gmosBufferMove (payload, &(apsFrame.payloadBuffer));

    // Attempt to send the APS message. Note that no message sent
    // callback is included, since failures are treated in the same way
    // as in-flight message loss and handled by the ZDO transaction
    // timeout.
    status = gmosZigbeeApsBroadcastTransmit (
        zdoClient->zigbeeStack, &apsFrame, NULL, NULL);

    // All error conditions are used to imply a retry.
    if (status != GMOS_ZIGBEE_STATUS_SUCCESS) {
        gmosBufferReset (&(apsFrame.payloadBuffer), 0);
        return false;
    }

    // On success, populate the ZDO transaction slot, using the
    // broadcast ZDO sequence ID.
    if (resultHandler != NULL) {
        zdoTimeout = gmosPalGetTimer () + ZDO_TRANSACTION_TIMEOUT;
        zdoClient->resultHandlers [slot] = resultHandler;
        zdoClient->localDataItems [slot] = localData;
        zdoClient->sequenceValues [slot] = zdoSequenceId;
        zdoClient->requestTimeouts [slot] = zdoTimeout;
    }

    // Ensure that the transaction timeout task is running.
    zdoClient->sequenceCounter += 1;
    gmosSchedulerTaskResume (&(zdoClient->timeoutTask));
    return true;
}

/*
 * Implement ZDO transaction timeout management task.
 */
static gmosTaskStatus_t gmosZigbeeZdoClientTimeoutHandler (void* taskData)
{
    gmosZigbeeZdoClient_t* zdoClient = (gmosZigbeeZdoClient_t*) taskData;
    gmosZigbeeZdoClientResultHandler_t resultHandler;
    uint_fast8_t slot;
    int32_t timeout;
    uint32_t nextDelay = UINT32_MAX;

    // Check each active ZDO transaction slot in turn.
    for (slot = 0; slot < GMOS_CONFIG_ZIGBEE_ZDO_CLIENT_MAX_REQUESTS; slot++) {
        resultHandler = zdoClient->resultHandlers [slot];
        if (resultHandler != NULL) {
            timeout = (int32_t) zdoClient->requestTimeouts [slot] -
                (int32_t) gmosPalGetTimer ();

            // If the timeout has expired, generate a timeout callback
            // with no message payload and complete the transaction.
            if (timeout <= 0) {
                gmosBuffer_t emptyBuffer = GMOS_BUFFER_INIT ();
                void* localData = zdoClient->localDataItems [slot];
                resultHandler (zdoClient, localData,
                    GMOS_ZIGBEE_ZDO_STATUS_TIMEOUT, true, &emptyBuffer);
                zdoClient->resultHandlers [slot] = NULL;
                gmosBufferReset (&emptyBuffer, 0);
            }

            // If the timeout has not expired, reshedule the timeout
            // handler task to run again.
            else {
                if (((uint32_t) timeout) < nextDelay) {
                    nextDelay = (uint32_t) timeout;
                }
            }
        }
    }

    // Reschedule the task if another timeout is pending.
    if (nextDelay == UINT32_MAX) {
        return GMOS_TASK_SUSPEND;
    } else {
        GMOS_LOG_FMT (LOG_VERBOSE,
            "Setting next ZDO transaction timeout to %d ticks.",
            nextDelay);
        return GMOS_TASK_RUN_LATER (nextDelay);
    }
}

/*
 * Performs a one-time initialisation of a ZDO client data structure.
 */
void gmosZigbeeZdoClientInit (gmosZigbeeZdoClient_t* zdoClient,
    gmosZigbeeStack_t* zigbeeStack)
{
    gmosTaskState_t* timeoutTask = &(zdoClient->timeoutTask);
    uint_fast8_t i;

    // Reset all the ZDO transaction states.
    for (i = 0; i < GMOS_CONFIG_ZIGBEE_ZDO_CLIENT_MAX_REQUESTS; i++) {
        zdoClient->resultHandlers [i] = NULL;
    }

    // Start the ZDO sequence counter at zero.
    zdoClient->sequenceCounter = 0;

    // Link the ZDO client to the Zigbee interface instance.
    zdoClient->zigbeeStack = zigbeeStack;
    zigbeeStack->zdoClient = zdoClient;

    // Initialise the ZDO timeout task.
    timeoutTask->taskTickFn = gmosZigbeeZdoClientTimeoutHandler;
    timeoutTask->taskData = zdoClient;
    timeoutTask->taskName = GMOS_PLATFORM_STRING_WRAPPER ("ZDO Client");
    gmosSchedulerTaskStart (timeoutTask);
}

/*
 * This is the callback handler which will be called in order to notify
 * the common Zigbee stack implementation of a newly received ZDO
 * response message that should be processed by the ZDO client entity.
 */
void gmosZigbeeZdoClientResponseHandler (
    gmosZigbeeStack_t* zigbeeStack, gmosZigbeeApsFrame_t* rxMsgApsFrame)
{
    uint8_t responsePayload [2];
    uint_fast8_t zdoSequence;
    uint_fast8_t zdoStatus;
    uint_fast8_t slot;
    bool requestComplete;
    gmosZigbeeZdoClientResultHandler_t resultHandler;
    gmosZigbeeZdoClient_t* zdoClient = zigbeeStack->zdoClient;

    // Extract the common header fields from the APS frame payload.
    // For all supported transactions the first APS payload bytes will
    // be the ZDO sequence number and the transaction status.
    if (!gmosBufferRead (&(rxMsgApsFrame->payloadBuffer), 0,
        responsePayload, sizeof (responsePayload))) {
        return;
    }
    zdoSequence = responsePayload [0];
    zdoStatus = responsePayload [1];

    // Find the transaction slot for the ZDO sequence number. Silently
    // discard spurious responses, including those generated in response
    // to vendor stack requests.
    resultHandler = NULL;
    for (slot = 0; slot < GMOS_CONFIG_ZIGBEE_ZDO_CLIENT_MAX_REQUESTS; slot++) {
        if ((zdoClient->resultHandlers [slot] != NULL) &&
            (zdoClient->sequenceValues [slot] == zdoSequence)) {
            resultHandler = zdoClient->resultHandlers [slot];
            break;
        }
    }
    if (resultHandler == NULL) {
        return;
    }

    // Unicast transactions always complete after a single transaction.
    // Release the transaction slot by removing the callback handler.
    if (gmosZigbeeZdoClientIsHostUnicastResponse (zdoSequence))  {
        zdoClient->resultHandlers [slot] = NULL;
        requestComplete = true;
    }

    // Broadcast responses only complete after a timeout.
    else {
        requestComplete = false;
    }

    // Issue the transaction callback.
    resultHandler (zdoClient, zdoClient->localDataItems [slot],
        zdoStatus, requestComplete, &(rxMsgApsFrame->payloadBuffer));
}

/*
 * Issues a ZDO client node descriptor request to the specified unicast
 * destination node.
 */
bool gmosZigbeeZdoClientNodeDescriptorRequest (
    gmosZigbeeZdoClient_t* zdoClient,
    gmosZigbeeZdoClientResultHandler_t resultHandler, void* localData,
    uint16_t remoteNodeId, uint16_t nwkAddrOfInterest)
{
    uint_fast8_t zdoSequenceId;
    uint8_t zdoMessage [3];
    gmosBuffer_t payload = GMOS_BUFFER_INIT ();

    // Generate the unicast ZDO sequence number.
    zdoSequenceId = gmosZigbeeZdoClientGetUnicastSequenceId (zdoClient);

    // Format the ZDO message payload.
    zdoMessage [0] = zdoSequenceId;
    zdoMessage [1] = (uint8_t) nwkAddrOfInterest;
    zdoMessage [2] = (uint8_t) (nwkAddrOfInterest >> 8);
    if (!gmosBufferAppend (&payload, zdoMessage, sizeof (zdoMessage))) {
        return false;
    }

    // Unicast the ZDO message payload with the appropriate cluster ID.
    return gmosZigbeeZdoClientUnicastRequest (zdoClient,
        resultHandler, localData, zdoSequenceId, remoteNodeId,
        GMOS_ZIGBEE_ZDO_CLUSTER_NODE_DESCRIPTOR_REQUEST, &payload);
}

/*
 * Issues a ZDO client power descriptor request to the specified unicast
 * destination node.
 */
bool gmosZigbeeZdoClientPowerDescriptorRequest (
    gmosZigbeeZdoClient_t* zdoClient,
    gmosZigbeeZdoClientResultHandler_t resultHandler, void* localData,
    uint16_t remoteNodeId, uint16_t nwkAddrOfInterest)
{
    uint_fast8_t zdoSequenceId;
    uint8_t zdoMessage [3];
    gmosBuffer_t payload = GMOS_BUFFER_INIT ();

    // Generate the unicast ZDO sequence number.
    zdoSequenceId = gmosZigbeeZdoClientGetUnicastSequenceId (zdoClient);

    // Format the ZDO message payload.
    zdoMessage [0] = zdoSequenceId;
    zdoMessage [1] = (uint8_t) nwkAddrOfInterest;
    zdoMessage [2] = (uint8_t) (nwkAddrOfInterest >> 8);
    if (!gmosBufferAppend (&payload, zdoMessage, sizeof (zdoMessage))) {
        return false;
    }

    // Unicast the ZDO message payload with the appropriate cluster ID.
    return gmosZigbeeZdoClientUnicastRequest (zdoClient,
        resultHandler, localData, zdoSequenceId, remoteNodeId,
        GMOS_ZIGBEE_ZDO_CLUSTER_POWER_DESCRIPTOR_REQUEST, &payload);
}

/*
 * Issues a ZDO client active endpoint request to the specified
 * unicast destination node.
 */
bool gmosZigbeeZdoClientActiveEndpointRequest (
    gmosZigbeeZdoClient_t* zdoClient,
    gmosZigbeeZdoClientResultHandler_t resultHandler, void* localData,
    uint16_t remoteNodeId, uint16_t nwkAddrOfInterest)
{
    uint_fast8_t zdoSequenceId;
    uint8_t zdoMessage [3];
    gmosBuffer_t payload = GMOS_BUFFER_INIT ();

    // Generate the unicast ZDO sequence number.
    zdoSequenceId = gmosZigbeeZdoClientGetUnicastSequenceId (zdoClient);

    // Format the ZDO message payload.
    zdoMessage [0] = zdoSequenceId;
    zdoMessage [1] = (uint8_t) nwkAddrOfInterest;
    zdoMessage [2] = (uint8_t) (nwkAddrOfInterest >> 8);
    if (!gmosBufferAppend (&payload, zdoMessage, sizeof (zdoMessage))) {
        return false;
    }

    // Unicast the ZDO message payload with the appropriate cluster ID.
    return gmosZigbeeZdoClientUnicastRequest (zdoClient,
        resultHandler, localData, zdoSequenceId, remoteNodeId,
        GMOS_ZIGBEE_ZDO_CLUSTER_ACTIVE_ENDPOINT_REQUEST, &payload);
}

/*
 * Issues a ZDO client simple descriptor request to the specified
 * unicast destination node.
 */
bool gmosZigbeeZdoClientSimpleDescriptorRequest (
    gmosZigbeeZdoClient_t* zdoClient,
    gmosZigbeeZdoClientResultHandler_t resultHandler, void* localData,
    uint16_t remoteNodeId, uint16_t nwkAddrOfInterest,
    uint8_t endpointOfInterest)
{
    uint_fast8_t zdoSequenceId;
    uint8_t zdoMessage [4];
    gmosBuffer_t payload = GMOS_BUFFER_INIT ();

    // Generate the unicast ZDO sequence number.
    zdoSequenceId = gmosZigbeeZdoClientGetUnicastSequenceId (zdoClient);

    // Format the ZDO message payload.
    zdoMessage [0] = zdoSequenceId;
    zdoMessage [1] = (uint8_t) nwkAddrOfInterest;
    zdoMessage [2] = (uint8_t) (nwkAddrOfInterest >> 8);
    zdoMessage [3] = endpointOfInterest;
    if (!gmosBufferAppend (&payload, zdoMessage, sizeof (zdoMessage))) {
        return false;
    }

    // Unicast the ZDO message payload with the appropriate cluster ID.
    return gmosZigbeeZdoClientUnicastRequest (zdoClient,
        resultHandler, localData, zdoSequenceId, remoteNodeId,
        GMOS_ZIGBEE_ZDO_CLUSTER_SIMPLE_DESCRIPTOR_REQUEST, &payload);
}

/*
 * Broadcasts a ZDO client permit joining request to all router nodes
 * on the network.
 */
bool gmosZigbeeZdoClientPermitJoiningBroadcast (
    gmosZigbeeZdoClient_t* zdoClient, uint8_t permitDuration)
{
    uint_fast8_t zdoSequenceId;
    uint8_t zdoMessage [3];
    gmosBuffer_t payload = GMOS_BUFFER_INIT ();

    // Generate the broadcast ZDO sequence number.
    zdoSequenceId = gmosZigbeeZdoClientGetBroadcastSequenceId (zdoClient);

    // Format the ZDO message payload. The trust centre significance
    // flag must always be set.
    zdoMessage [0] = zdoSequenceId;
    zdoMessage [1] = permitDuration;
    zdoMessage [2] = 0x01;
    if (!gmosBufferAppend (&payload, zdoMessage, sizeof (zdoMessage))) {
        return false;
    }

    // Broadcast the ZDO message payload to all routers using the
    // appropriate cluster ID.
    return gmosZigbeeZdoClientBroadcastRequest (zdoClient, NULL, NULL,
        zdoSequenceId, GMOS_ZIGBEE_APS_BROADCAST_ROUTERS_ONLY,
        GMOS_ZIGBEE_ZDO_CLUSTER_PERMIT_JOINING_REQUEST, &payload);
}

/*
 * Sends a ZDO client device management leave request to the specified
 * unicast destination node. The remove children and rejoin request
 * flags are always set to 'false'.
 */
bool gmosZigbeeZdoClientDeviceLeaveRequest (
    gmosZigbeeZdoClient_t* zdoClient,
    gmosZigbeeZdoClientResultHandler_t resultHandler, void* localData,
    uint16_t remoteNodeId, uint8_t* remoteNodeEui64)
{
    uint_fast8_t zdoSequenceId;
    uint8_t zdoMessage [10];
    uint8_t* addrPtr = remoteNodeEui64;
    uint_fast8_t addrByte = 0x00;
    gmosBuffer_t payload = GMOS_BUFFER_INIT ();
    uint_fast8_t i;

    // Generate the unicast ZDO sequence number.
    zdoSequenceId = gmosZigbeeZdoClientGetUnicastSequenceId (zdoClient);

    // Format the ZDO message payload. The remove children and rejoin
    // request flags are always set to 'false'.
    zdoMessage [0] = zdoSequenceId;
    for (i = 1; i <= 8; i++) {
        if (addrPtr != NULL) {
            addrByte = *(addrPtr++);
        }
        zdoMessage [i] = addrByte;
    }
    zdoMessage [9] = 0x00;
    if (!gmosBufferAppend (&payload, zdoMessage, sizeof (zdoMessage))) {
        return false;
    }

    // Unicast the ZDO message payload with the appropriate cluster ID.
    return gmosZigbeeZdoClientUnicastRequest (zdoClient,
        resultHandler, localData, zdoSequenceId, remoteNodeId,
        GMOS_ZIGBEE_ZDO_CLUSTER_DEVICE_LEAVE_REQUEST, &payload);
}
