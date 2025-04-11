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
 * This header defines the common API for issuing Zigbee ZDO client
 * requests using the GubbinsMOS runtime framework.
 */

#ifndef GMOS_ZIGBEE_ZDO_CLIENT_H
#define GMOS_ZIGBEE_ZDO_CLIENT_H

#include <stdint.h>
#include <stdbool.h>
#include "gmos-buffers.h"
#include "gmos-scheduler.h"
#include "gmos-zigbee-config.h"
#include "gmos-zigbee-stack.h"
#include "gmos-zigbee-aps.h"
#include "gmos-zigbee-zdo-common.h"

/**
 * This is the function prototype for callback handlers which will be
 * called by the ZDO client to return the results of ZDO transaction
 * requests.
 * @param zdoClient This is the Zigbee ZDO client instance which
 *     processed the original ZDO client request.
 * @param localData This is an opaque pointer to the local data item
 *     that was included in the original ZDO request.
 * @param zdoStatus This is the status of the transaction, specified
 *     using the standard ZDO status codes.
 * @param requestComplete This is a boolean flag which will be set on
 *     the final callback for the associated request. This will always
 *     be set for unicast requests. Broadcast requests can generate
 *     multiple callbacks, the last of which will be the broadcast
 *     transaction timeout.
 * @param responseBuffer This is a buffer that contains the full ZDO
 *     response, including the initial ZDO sequence number. An empty
 *     buffer will be used for local timeouts. The buffer will
 *     automatically be reset and the contents discarded on returning
 *     from the callback.
 */
typedef void (*gmosZigbeeZdoClientResultHandler_t) (
    gmosZigbeeZdoClient_t* zdoClient, void* localData,
    gmosZigbeeZdoStatusCodes_t zdoStatus, bool requestComplete,
    gmosBuffer_t* responseBuffer);

/**
 * Defines the optional ZDO client data structure for a given Zigbee
 * interface.
 */
typedef struct gmosZigbeeZdoClient_t {

    // This is a pointer to the Zigbee stack instance that is associated
    // with the ZDO client.
    gmosZigbeeStack_t* zigbeeStack;

    // This holds the ZDO client handler timeout task state.
    gmosTaskState_t timeoutTask;

    // This is the array of ZDO transaction result callback handlers.
    gmosZigbeeZdoClientResultHandler_t
        resultHandlers [GMOS_CONFIG_ZIGBEE_ZDO_CLIENT_MAX_REQUESTS];

    // This is an array of current ZDO local data items.
    void* localDataItems [GMOS_CONFIG_ZIGBEE_ZDO_CLIENT_MAX_REQUESTS];

    // This is an array of current ZDO transaction timeout values.
    uint32_t requestTimeouts [GMOS_CONFIG_ZIGBEE_ZDO_CLIENT_MAX_REQUESTS];

    // This is an array of current ZDO transaction sequence values.
    uint8_t sequenceValues [GMOS_CONFIG_ZIGBEE_ZDO_CLIENT_MAX_REQUESTS];

    // This is the sequence counter that is used for generating ZDO
    // request sequence numbers.
    uint8_t sequenceCounter;

} gmosZigbeeZdoClient_t;

/**
 * Performs a one-time initialisation of a ZDO client data structure.
 * This should be called during initialisation to set up the Zigbee
 * ZDO client for subsequent use.
 * @param zdoClient This is the ZDO client instance that is to be
 *     initialised.
 * @param zigbeeStack This is the Zigbee stack instance that is to be
 *     associated with the ZDO client. A Zigbee stack may be associated
 *     with at most one ZDO client.
 */
void gmosZigbeeZdoClientInit (gmosZigbeeZdoClient_t* zdoClient,
    gmosZigbeeStack_t* zigbeeStack);

/**
 * This is the callback handler which will be called in order to notify
 * the common Zigbee stack implementation of a newly received ZDO
 * response message that should be processed by the ZDO client entity.
 * @param zigbeeStack This is the Zigbee stack instance which received
 *     the incoming ZDO response message.
 * @param rxMsgApsFrame This is the APS frame data structure which
 *     encapsulates the received APS message. The APS frame contents
 *     are only guaranteed to remain valid for the duration of the
 *     callback.
 */
void gmosZigbeeZdoClientResponseHandler (
    gmosZigbeeStack_t* zigbeeStack, gmosZigbeeApsFrame_t* rxMsgApsFrame);

/**
 * Issues a ZDO client node descriptor request to the specified unicast
 * destination node.
 * @param zdoClient This is the ZDO client instance that should be used
 *     for issuing the node descriptor request.
 * @param resultHandler This is the ZDO result handler that should be
 *     used for processing the response to the node descriptor request.
 * @param localData This is an opaque pointer to a local data item that
 *     will be passed to the corresponding result handler.
 * @param remoteNodeId This is the address of the remote Zigbee node
 *     that will receive the node descriptor request.
 * @param nwkAddrOfInterest This is the network address for which the
 *     node descriptor is being requested. It will normally be the
 *     same as the remote node ID, unless a device discovery cache is
 *     being used on the network.
 * @return Returns a boolean value which will be set to 'true' if the
 *     request was successfuly sent and 'false' if the request needs
 *     to be retried due to a resource limitation.
 */
bool gmosZigbeeZdoClientNodeDescriptorRequest (
    gmosZigbeeZdoClient_t* zdoClient,
    gmosZigbeeZdoClientResultHandler_t resultHandler, void* localData,
    uint16_t remoteNodeId, uint16_t nwkAddrOfInterest);

/**
 * Issues a ZDO client power descriptor request to the specified unicast
 * destination node.
 * @param zdoClient This is the ZDO client instance that should be used
 *     for issuing the power descriptor request.
 * @param resultHandler This is the ZDO result handler that should be
 *     used for processing the response to the power descriptor request.
 * @param localData This is an opaque pointer to a local data item that
 *     will be passed to the corresponding result handler.
 * @param remoteNodeId This is the address of the remote Zigbee node
 *     that will receive the power descriptor request.
 * @param nwkAddrOfInterest This is the network address for which the
 *     power descriptor is being requested. It will normally be the
 *     same as the remote node ID, unless a device discovery cache is
 *     being used on the network.
 * @return Returns a boolean value which will be set to 'true' if the
 *     request was successfuly sent and 'false' if the request needs
 *     to be retried due to a resource limitation.
 */
bool gmosZigbeeZdoClientPowerDescriptorRequest (
    gmosZigbeeZdoClient_t* zdoClient,
    gmosZigbeeZdoClientResultHandler_t resultHandler, void* localData,
    uint16_t remoteNodeId, uint16_t nwkAddrOfInterest);

/**
 * Issues a ZDO client active endpoint request to the specified
 * unicast destination node.
 * @param zdoClient This is the ZDO client instance that should be used
 *     for issuing the active endpoint request.
 * @param resultHandler This is the ZDO result handler that should be
 *     used for processing the response to the active endpoint request.
 * @param localData This is an opaque pointer to a local data item that
 *     will be passed to the corresponding result handler.
 * @param remoteNodeId This is the address of the remote Zigbee node
 *     that will receive the active endpoint request.
 * @param nwkAddrOfInterest This is the network address for which the
 *     active endpoint list is being requested. It will normally be the
 *     same as the remote node ID, unless a device discovery cache is
 *     being used on the network.
 * @return Returns a boolean value which will be set to 'true' if the
 *     request was successfuly sent and 'false' if the request needs
 *     to be retried due to a resource limitation.
 */
bool gmosZigbeeZdoClientActiveEndpointRequest (
    gmosZigbeeZdoClient_t* zdoClient,
    gmosZigbeeZdoClientResultHandler_t resultHandler, void* localData,
    uint16_t remoteNodeId, uint16_t nwkAddrOfInterest);

/**
 * Issues a ZDO client simple descriptor request to the specified
 * unicast destination node.
 * @param zdoClient This is the ZDO client instance that should be used
 *     for issuing the simple descriptor request.
 * @param resultHandler This is the ZDO result handler that should be
 *     used for processing the response to the simple descriptor
 *     request.
 * @param localData This is an opaque pointer to a local data item that
 *     will be passed to the corresponding result handler.
 * @param remoteNodeId This is the address of the remote Zigbee node
 *     that will receive the simple descriptor request.
 * @param nwkAddrOfInterest This is the network address for which the
 *     simple descriptor is being requested. It will normally be the
 *     same as the remote node ID, unless a device discovery cache is
 *     being used on the network.
 * @param endpointOfInterest This is the endpoint ID for which the
 *     simple descriptor is being requested. It should be in the valid
 *     range from 1 to 240.
 * @return Returns a boolean value which will be set to 'true' if the
 *     request was successfuly sent and 'false' if the request needs
 *     to be retried due to a resource limitation.
 */
bool gmosZigbeeZdoClientSimpleDescriptorRequest (
    gmosZigbeeZdoClient_t* zdoClient,
    gmosZigbeeZdoClientResultHandler_t resultHandler, void* localData,
    uint16_t remoteNodeId, uint16_t nwkAddrOfInterest,
    uint8_t endpointOfInterest);

/**
 * Broadcasts a ZDO client permit joining request to all router nodes on
 * the network. The trust centre significance flag will always be set
 * and no responses to the broadcast request will be generated.
 * @param zdoClient This is the ZDO client instance that should be used
 *     for issuing the permit joining request.
 * @param permitDuration This is the duration for which network joining
 *     will be permitted, expressed as an integer number of seconds. A
 *     value of 0x00 will immediately disable network joining and a
 *     value of 0xFF will enable network joining until cancelled by a
 *     subsequent request.
 * @return Returns a boolean value which will be set to 'true' if the
 *     request was successfuly sent and 'false' if the request needs
 *     to be retried due to a resource limitation.
 */
bool gmosZigbeeZdoClientPermitJoiningBroadcast (
    gmosZigbeeZdoClient_t* zdoClient, uint8_t permitDuration);

/**
 * Sends a ZDO client device management leave request to the specified
 * unicast destination node. The remove children and rejoin request
 * flags are always set to 'false'.
 * @param zdoClient This is the ZDO client instance that should be used
 *     for issuing the device management leave request.
 * @param resultHandler This is the ZDO result handler that should be
 *     used for processing the response to the device management leave
 *     request.
 * @param localData This is an opaque pointer to a local data item that
 *     will be passed to the corresponding result handler.
 * @param remoteNodeId This is the address of the remote Zigbee node
 *     that will receive the device management leave request.
 * @param remoteNodeEui64 This is the 64-bit IEEE MAC address of the
 *     device that is to be removed from the network.
 * @return Returns a boolean value which will be set to 'true' if the
 *     request was successfuly sent and 'false' if the request needs
 *     to be retried due to a resource limitation.
 */
bool gmosZigbeeZdoClientDeviceLeaveRequest (
    gmosZigbeeZdoClient_t* zdoClient,
    gmosZigbeeZdoClientResultHandler_t resultHandler, void* localData,
    uint16_t remoteNodeId, uint8_t* remoteNodeEui64);

#endif // GMOS_ZIGBEE_ZDO_CLIENT_H
