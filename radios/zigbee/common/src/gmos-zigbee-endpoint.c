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
 * This header implements the support functions for managing generic
 * application endpoints on Zigbee devices and the associated endpoint
 * cluster framework.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-buffers.h"
#include "gmos-zigbee-stack.h"
#include "gmos-zigbee-endpoint.h"

/*
 * Performs a one-time initialisation of a Zigbee endpoint data
 * structure.
 */
void gmosZigbeeEndpointInit (gmosZigbeeEndpoint_t* zigbeeEndpoint,
    uint8_t endpointId, uint16_t appProfileId, uint16_t appDeviceId)
{
    zigbeeEndpoint->zigbeeStack = NULL;
    zigbeeEndpoint->nextEndpoint = NULL;
    zigbeeEndpoint->clusterList = NULL;
    zigbeeEndpoint->appProfileId = appProfileId;
    zigbeeEndpoint->appDeviceId = appDeviceId;
    zigbeeEndpoint->endpointId = endpointId;
}

/*
 * Attaches an initialised Zigbee endpoint data structure to a Zigbee
 * stack instance. Endpoints need to be stored in ascending endpoint ID
 * order to meet the requirements for the ZDO active endpoint request.
 */
bool gmosZigbeeEndpointAttach (gmosZigbeeStack_t* zigbeeStack,
    gmosZigbeeEndpoint_t* zigbeeEndpoint)
{
    gmosZigbeeEndpoint_t** endpointPtr;

    // Only endpoint IDs from 1 to 240 inclusive are valid.
    if ((zigbeeEndpoint->endpointId < 1) ||
        (zigbeeEndpoint->endpointId > 240)) {
        return false;
    }

    // Iterate over the endpoint list looking for the correct insertion
    // point. This also checks for duplicate endpoint IDs.
    endpointPtr = &(zigbeeStack->endpointList);
    while (*endpointPtr != NULL) {
        uint8_t endpointId = (*endpointPtr)->endpointId;
        if (zigbeeEndpoint->endpointId < endpointId) {
            break;
        } else if (zigbeeEndpoint->endpointId == endpointId) {
            return false;
        } else {
            endpointPtr = &((*endpointPtr)->nextEndpoint);
        }
    }

    // Insert the new endpoint instance at the appropriate point in
    // the endpoint list.
    zigbeeEndpoint->nextEndpoint = *endpointPtr;
    *endpointPtr = zigbeeEndpoint;

    // Associate the endpoint with the Zigbee stack instance.
    zigbeeEndpoint->zigbeeStack = zigbeeStack;
    return true;
}

/*
 * Requests the Zigbee endpoint instance for a Zigbee stack instance,
 * given the endpoint ID.
 */
gmosZigbeeEndpoint_t* gmosZigbeeEndpointInstance (
    gmosZigbeeStack_t* zigbeeStack, uint8_t endpointId)
{
    gmosZigbeeEndpoint_t* zigbeeEndpoint = zigbeeStack->endpointList;
    while (zigbeeEndpoint != NULL) {
        if (zigbeeEndpoint->endpointId == endpointId) {
            break;
        } else {
            zigbeeEndpoint = zigbeeEndpoint->nextEndpoint;
        }
    }
    return zigbeeEndpoint;
}

/*
 * Performs a one-time initialisation of a Zigbee cluster data
 * structure.
 */
void gmosZigbeeClusterInit (gmosZigbeeCluster_t* zigbeeCluster,
    uint16_t clusterId, uint8_t clusterOptions, void* clusterData,
    gmosZigbeeClusterRxMessageHandler_t rxMessageHandler)
{
    zigbeeCluster->hostEndpoint = NULL;
    zigbeeCluster->nextCluster = NULL;
    zigbeeCluster->clusterId = clusterId;
    zigbeeCluster->clusterOptions = clusterOptions;
    zigbeeCluster->clusterData = clusterData;
    zigbeeCluster->rxMessageHandler = rxMessageHandler;
}

/*
 * Attaches an initialised Zigbee cluster data structure to a Zigbee
 * endpoint. By convention these are ordered by increasing cluster ID.
 */
bool gmosZigbeeClusterAttach (gmosZigbeeEndpoint_t* zigbeeEndpoint,
    gmosZigbeeCluster_t* zigbeeCluster)
{
    gmosZigbeeCluster_t** clusterPtr;

    // Iterate over the cluster list looking for the correct insertion
    // point. This also checks for duplicate cluster IDs.
    clusterPtr = &(zigbeeEndpoint->clusterList);
    while (*clusterPtr != NULL) {
        uint16_t clusterId = (*clusterPtr)->clusterId;
        if (zigbeeCluster->clusterId < clusterId) {
            break;
        } else if (zigbeeCluster->clusterId == clusterId) {
            return false;
        } else {
            clusterPtr = &((*clusterPtr)->nextCluster);
        }
    }

    // Insert the new cluster instance at the appropriate point in the
    // cluster list.
    zigbeeCluster->nextCluster = *clusterPtr;
    *clusterPtr = zigbeeCluster;

    // Associate the cluster with the application endpoint.
    zigbeeCluster->hostEndpoint = zigbeeEndpoint;
    return true;
}

/*
 * Requests the Zigbee cluster instance on a Zigbee endpoint, given the
 * cluster ID.
 */
gmosZigbeeCluster_t* gmosZigbeeClusterInstance (
    gmosZigbeeEndpoint_t* zigbeeEndpoint, uint16_t clusterId)
{
    gmosZigbeeCluster_t* zigbeeCluster = zigbeeEndpoint->clusterList;
    while (zigbeeCluster != NULL) {
        if (zigbeeCluster->clusterId == clusterId) {
            break;
        } else {
            zigbeeCluster = zigbeeCluster->nextCluster;
        }
    }
    return zigbeeCluster;
}

/*
 * Implements the APS received message dispatch handler for Zigbee
 * application endpoints.
 */
void gmosZigbeeEndpointRxMessageDispatch (
    gmosZigbeeStack_t* zigbeeStack, gmosZigbeeEndpoint_t* zigbeeEndpoint,
    gmosZigbeeApsFrame_t* rxMsgApsFrame, gmosBuffer_t* txMsgApsBuffer)
{
    gmosZigbeeCluster_t* appCluster;

    // Silently discard messages if the profile ID does not match that
    // of the target endpoint.
    if (rxMsgApsFrame->profileId != zigbeeEndpoint->appProfileId) {
        return;
    }

    // Scan the cluster list looking for a match.
    appCluster = gmosZigbeeClusterInstance (
        zigbeeEndpoint, rxMsgApsFrame->clusterId);

    // Invoke the cluster message handler.
    if ((appCluster != NULL) &&
        (appCluster->rxMessageHandler != NULL)) {
        appCluster->rxMessageHandler (
            zigbeeStack, appCluster, rxMsgApsFrame, txMsgApsBuffer);
    }

    // TODO: Generate an endpoint response when no matching cluster is
    // found. For ZCL endpoints this would be a default response with
    // the 'UNSUP_CLUSTER' status.
}
