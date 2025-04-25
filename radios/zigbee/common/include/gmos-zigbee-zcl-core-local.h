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
 * This header provides command processing support for Zigbee Cluster
 * Library (ZCL) foundation components that are local to this device.
 */

#ifndef GMOS_ZIGBEE_ZCL_CORE_LOCAL_H
#define GMOS_ZIGBEE_ZCL_CORE_LOCAL_H

#include <stdint.h>
#include "gmos-config.h"
#include "gmos-buffers.h"
#include "gmos-streams.h"
#include "gmos-zigbee-config.h"
#include "gmos-zigbee-zcl-core.h"

/**
 * This data structure provides a common encapsulation for a Zigbee
 * Cluster Library (ZCL) local access endpoint instance.
 */
typedef struct gmosZigbeeZclEndpointLocal_t {

    // This is the local endpoint processing task.
    gmosTaskState_t task;

    // This is the local command queue used for forwarding requests.
    gmosStream_t commandQueue;

    // This is the request payload buffer for the current command.
    gmosBuffer_t requestBuffer;

    // This is the response message buffer for the current command.
    gmosBuffer_t responseBuffer;

    // This is the currently active cluster for the endpoint.
    gmosZigbeeZclCluster_t* cluster;

    // This is the manufacturer ID for the current command.
    uint16_t vendorId;

    // This is the initiating node for the current command.
    uint16_t peerNodeId;

    // This is the initiating endpoint for the current command.
    uint8_t peerEndpointId;

    // This is the ZCL sequence number for the current command.
    uint8_t zclSequence;

    // This is the currently active processing state.
    uint8_t state;

    // This is the currently active index counter.
    uint8_t count;

    // This is the currently active buffer offset.
    uint8_t offset;

} gmosZigbeeZclEndpointLocal_t;

/**
 * This data structure defines the contents of a local endpoint command
 * queue entry, that is used for forwarding new ZCL commands to the
 * local command handler.
 */
typedef struct gmosZigbeeZclLocalCommandQueueEntry_t {

    // Wrap the command buffer which holds the ZCL command payload.
    gmosBuffer_t zclPayloadBuffer;

    // Specify the cluster associated with the command.
    gmosZigbeeZclCluster_t* zclCluster;

    // Specify the manufacturer vendor ID extracted from the header.
    uint16_t vendorId;

    // Specify the peer node ID, extracted from the APS header.
    uint16_t peerNodeId;

    // Specify the peer endpoint ID, extracted from the APS header.
    uint8_t peerEndpointId;

    // Specify the ZCL frame ID extracted from the header.
    uint8_t zclFrameId;

    // Specify the ZCL sequence number extracted from the header.
    uint8_t zclSequence;

} gmosZigbeeZclLocalCommandQueueEntry_t;

/**
 * Performs a one-time initialisation of a ZCL endpoint local message
 * handler.
 * @param zclEndpoint This is the ZCL endpoint data structure that is
 *     to be initialised.
 */
void gmosZigbeeZclLocalEndpointInit (
    gmosZigbeeZclEndpoint_t* zclEndpoint);

/**
 * Queues a long running ZCL attribute command request for subsequent
 * processing.
 * @param zclCluster This is the local cluster for which the processing
 *     request is being queued.
 * @param peerNodeId This is the node identifier for the peer device
 *     which issued the ZCL request.
 * @param peerEndpointId This is the endpoint on the peer device which
 *     issued the ZCL request.
 * @param zclFrameHeader This is a pointer to the parsed ZCL frame
 *     header parameters.
 * @param zclPayloadBuffer This is a buffer which contains the ZCL
 *     request payload data after the ZCL header has been removed.
 * @return Returns a ZCL status value for the ZCL default response, or
 *     a null status value if the request is queued and no default
 *     response is to be sent.
 */
gmosZigbeeZclStatusCode_t gmosZigbeeZclLocalAttrCommandQueueRequest (
    gmosZigbeeZclCluster_t* zclCluster, uint16_t peerNodeId,
    uint8_t peerEndpointId, gmosZigbeeZclFrameHeader_t* zclFrameHeader,
    gmosBuffer_t* zclPayloadBuffer);

/**
 * Implement local attribute access complete handler. This should be
 * called from each attribute access function or state machine to
 * indicate that the attribute processing has completed.
 * @param zclEndpoint This is the ZCL endpoint for which attribute
 *     processing has completed.
 * @param status This is the status of the completed ZCL attribute
 *     processing operation.
 */
void gmosZigbeeZclLocalAttrAccessComplete (
    gmosZigbeeZclEndpoint_t* zclEndpoint, uint8_t status);

#endif // GMOS_ZIGBEE_ZCL_CORE_LOCAL_H
