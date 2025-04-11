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
 * This header defines the data types and support functions for managing
 * generic application endpoints on Zigbee devices and the associated
 * endpoint cluster framework.
 */

#ifndef GMOS_ZIGBEE_ENDPOINT_H
#define GMOS_ZIGBEE_ENDPOINT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "gmos-buffers.h"
#include "gmos-zigbee-stack.h"
#include "gmos-zigbee-aps.h"

/**
 * This enumeration specifies the various supported Zigbee cluster
 * option flags.
 */
typedef enum {

    // Specify that the cluster is an input (server) cluster.
    GMOS_ZIGBEE_CLUSTER_OPTION_INPUT  = 0x01,

    // Specify that the cluster is an output (client) cluster.
    GMOS_ZIGBEE_CLUSTER_OPTION_OUTPUT = 0x02

} gmosZigbeeClusterOptionFlags_t;

/**
 * This type definition specifies the function prototype to be used for
 * cluster specific received message handlers.
 * @param zigbeeStack This is the Zigbee stack instance which received
 *     the incoming APS message.
 * @param zigbeeCluster This is the Zigbee application cluster which
 *     received the incoming APS message.
 * @param rxMsgApsFrame This is the APS frame data structure which
 *     encapsulates the received APS message. The APS frame contents
 *     are only guaranteed to remain valid for the duration of the
 *     callback.
 * @param txMsgApsBuffer This is a GubbinsMOS buffer that may be used
 *     to supply a response message that will be transmitted to the
 *     originating node of the received message using the same APS
 *     parameters as the original request. The APS message payload
 *     should be placed in this buffer if required. By default this
 *     buffer is empty, so no response will be sent. The contents of the
 *     response buffer will be ignored if the inbound message was
 *     broadcast to all endpoints.
 */
typedef void (* gmosZigbeeClusterRxMessageHandler_t) (
    gmosZigbeeStack_t* zigbeeStack, gmosZigbeeCluster_t* zigbeeCluster,
    gmosZigbeeApsFrame_t* rxMsgApsFrame, gmosBuffer_t* txMsgApsBuffer);

/**
 * This data structure defines the common format for a Zigbee device
 * application endpoint instance.
 */
typedef struct gmosZigbeeEndpoint_t {

    // This is a pointer to the Zigbee stack instance that is associated
    // with the application endpoint.
    gmosZigbeeStack_t* zigbeeStack;

    // This is a pointer to the next endpoint in the endpoint list.
    struct gmosZigbeeEndpoint_t* nextEndpoint;

    // This is a pointer to the list of clusters supported by the
    // endpoint.
    gmosZigbeeCluster_t* clusterList;

    // This value specifies the application profile ID for the endpoint.
    uint16_t appProfileId;

    // This value specifies the application device ID for the endpoint.
    uint16_t appDeviceId;

    // This value specifies the local endpoint identifier.
    uint8_t endpointId;

} gmosZigbeeEndpoint_t;

/**
 * This data structure defines the common format for a Zigbee endpoint
 * cluster instance.
 */
typedef struct gmosZigbeeCluster_t {

    // This is a pointer to the Zigbee application endpoint that hosts
    // the cluster instance.
    gmosZigbeeEndpoint_t* hostEndpoint;

    // This is a pointer to the next cluster in the cluster list.
    struct gmosZigbeeCluster_t* nextCluster;

    // This function pointer specifies the cluster specific inbound
    // message handler.
    gmosZigbeeClusterRxMessageHandler_t rxMessageHandler;

    // This is an opaque pointer to the cluster's application specific
    // data area.
    void* clusterData;

    // This value specifies the application cluster identifier.
    uint16_t clusterId;

    // This value holds the various cluster option flags.
    uint8_t clusterOptions;

} gmosZigbeeCluster_t;

/**
 * Provides a compile time initialisation macro for a Zigbee endpoint
 * data structure. Assigning this macro value to a Zigbee endpoint
 * variable on declaration may be used instead of a call to the
 * 'gmosZigbeeEndpointInit' function to set up a Zigbee endpoint for
 * subsequent use.
 * @param _endpointId_ This is the Zigbee endpoint identifier for the
 *     endpoint data structure. It must be in the valid range from 1 to
 *     240 inclusive.
 * @param _appProfileId_ This specifies the Zigbee application profile
 *     which is associated with the endpoint.
 * @param _appDeviceId_ This specifies the Zigbee application profile
 *     device which is implemented by the endpoint.
 */
#define GMOS_ZIGBEE_ENDPOINT_INIT(                                     \
    _endpointId_, _appProfileId_, _appDeviceId_) {                     \
    .zigbeeStack  = NULL,                                              \
    .nextEndpoint = NULL,                                              \
    .clusterList  = NULL,                                              \
    .appProfileId = _appProfileId_,                                    \
    .appDeviceId  = _appDeviceId_,                                     \
    .endpointId   = _endpointId_}

/**
 * Performs a one-time initialisation of a Zigbee endpoint data
 * structure. This should be called during initialisation to set up the
 * Zigbee endpoint for subsequent use.
 * @param zigbeeEndpoint This is the Zigbee endpoint data structure that
 *     is to be initialised.
 * @param endpointId This is the Zigbee endpoint identifier for the
 *     endpoint data structure. It must be in the valid range from 1 to
 *     240 inclusive.
 * @param appProfileId This specifies the Zigbee application profile
 *     which is associated with the endpoint.
 * @param appDeviceId This specifies the Zigbee application profile
 *     device which is implemented by the endpoint.
 */
void gmosZigbeeEndpointInit (gmosZigbeeEndpoint_t* zigbeeEndpoint,
    uint8_t endpointId, uint16_t appProfileId, uint16_t appDeviceId);

/**
 * Attaches an initialised Zigbee endpoint data structure to a Zigbee
 * stack instance.
 * @param zigbeeStack This is the Zigbee stack instance to which the
 *     Zigbee endpoint is to be attached.
 * @param zigbeeEndpoint This is the Zigbee endpoint data structure that
 *     is to be attached to the Zigbee stack instance.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully attaching the endpoint and 'false' on failure.
 */
bool gmosZigbeeEndpointAttach (gmosZigbeeStack_t* zigbeeStack,
    gmosZigbeeEndpoint_t* zigbeeEndpoint);

/**
 * Requests the Zigbee endpoint instance for a Zigbee stack instance,
 * given the endpoint ID.
 * @param zigbeeStack This is the Zigbee stack instance for which the
 *     Zigbee endpoint instance is being requested.
 * @param endpointId This is the Zigbee endpoint identifier that is
 *     associated with the endpoint instance.
 * @return Returns a pointer to the active endpoint instance or a null
 *     reference if no active endpoint is present on the device.
 */
gmosZigbeeEndpoint_t* gmosZigbeeEndpointInstance (
    gmosZigbeeStack_t* zigbeeStack, uint8_t endpointId);

/**
 * Performs a one-time initialisation of a Zigbee cluster data
 * structure. This should be called during initialisation to set up
 * the Zigbee cluster for subsequent use.
 * @param zigbeeCluster This is the Zigbee cluster that is to be
 *     initialised.
 * @param clusterId This is the Zigbee cluster identifier for the
 *     cluster data structure.
 * @param clusterOptions This is the set of cluster option flags that
 *     are associated with the cluster.
 * @param clusterData This is an opaque pointer to a cluster specific
 *     data structure.
 * @param rxMessageHandler This is the received message handler to be
 *     used for processing inbound cluster messages.
 */
void gmosZigbeeClusterInit (gmosZigbeeCluster_t* zigbeeCluster,
    uint16_t clusterId, uint8_t clusterOptions, void* clusterData,
    gmosZigbeeClusterRxMessageHandler_t rxMessageHandler);

/**
 * Attaches an initialised Zigbee cluster data structure to a Zigbee
 * endpoint.
 * @param zigbeeEndpoint This is the Zigbee endpoint data structure to
 *     which the cluster is to be attached.
 * @param zigbeeCluster This is the Zigbee cluster data structure that
 *     is to be attached to the Zigbee endpoint.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully attaching the cluster and 'false' on failure.
 */
bool gmosZigbeeClusterAttach (gmosZigbeeEndpoint_t* zigbeeEndpoint,
    gmosZigbeeCluster_t* zigbeeCluster);

/**
 * Requests the Zigbee cluster instance on a Zigbee endpoint, given the
 * cluster ID.
 * @param zigbeeEndpoint This is the Zigbee application endpoint to
 *     which the cluster is attached.
 * @param clusterId This is the Zigbee cluster identifier that is
 *     associated with the cluster instance.
 * @return Returns a pointer to the matching cluster instance or a null
 *     reference if no matching cluster instance is present on the
 *     endpoint.
 */
gmosZigbeeCluster_t* gmosZigbeeClusterInstance (
    gmosZigbeeEndpoint_t* zigbeeEndpoint, uint16_t clusterId);

/**
 * Implements the APS received message dispatch handler for Zigbee
 * application endpoints. This checks that the message is addressed to
 * a matching cluster instance on the endpoint and forwards it to the
 * appropriate cluster handler for further processing.
 * @param zigbeeStack This is the Zigbee stack instance which received
 *     the incoming APS message.
 * @param zigbeeEndpoint This is the Zigbee application endpoint which
 *     received the incoming APS message.
 * @param rxMsgApsFrame This is the APS frame data structure which
 *     encapsulates the received APS message. The APS frame contents
 *     are only guaranteed to remain valid for the duration of the
 *     callback.
 * @param txMsgApsBuffer This is a GubbinsMOS buffer that may be used
 *     to supply a response message that will be transmitted to the
 *     originating node of the received message using the same APS
 *     parameters as the original request. The APS message payload
 *     should be placed in this buffer if required. By default this
 *     buffer is empty, so no response will be sent. The contents of the
 *     response buffer will be ignored if the inbound message was
 *     broadcast to all endpoints.
 */
void gmosZigbeeEndpointRxMessageDispatch (
    gmosZigbeeStack_t* zigbeeStack, gmosZigbeeEndpoint_t* zigbeeEndpoint,
    gmosZigbeeApsFrame_t* rxMsgApsFrame, gmosBuffer_t* txMsgApsBuffer);

#endif // GMOS_ZIGBEE_ENDPOINT_H
