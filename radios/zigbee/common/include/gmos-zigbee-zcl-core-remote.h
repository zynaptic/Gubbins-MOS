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
 * Library (ZCL) foundation components that are located on a remote
 * device.
 */

#ifndef GMOS_ZIGBEE_ZCL_CORE_REMOTE_H
#define GMOS_ZIGBEE_ZCL_CORE_REMOTE_H

#include <stdint.h>
#include <stdbool.h>
#include "gmos-config.h"
#include "gmos-buffers.h"
#include "gmos-zigbee-config.h"
#include "gmos-zigbee-zcl-core.h"

/**
 * This is the function prototype for callback handlers which will be
 * called by the ZCL remote message handler to return the results of ZCL
 * transaction requests.
 * @param zclCluster This is the Zigbee ZCL cluster instance which
 *     processed the original ZCL remote request.
 * @param localData This is an opaque pointer to the local data item
 *     that was included with the corresponding ZCL request.
 * @param zclStatus This is the status of the transaction, specified
 *     using the standard ZCL status codes.
 * @param requestComplete This is a boolean flag which will be set on
 *     the final callback for the associated request. This will always
 *     be set for unicast requests. Broadcast requests can generate
 *     multiple callbacks, the last of which will be the broadcast
 *     transaction timeout.
 * @param peerNodeId This is the node ID of the remote node that
 *     generated the response message.
 * @param peerEndpointId This is the endpoint ID on the remote node
 *     that generated the response message.
 * @param zclFrameHeader This is a pointer to the ZCL frame header that
 *     contains the sequence number and other ZCL frame parameters.
 * @param zclPayloadBuffer This is a buffer that contains the full ZCL
 *     response after the ZCL frame header has been removed. An empty
 *     buffer will be used for local timeouts. The buffer will
 *     automatically be reset and the contents discarded on returning
 *     from the callback.
 */
typedef void (*gmosZigbeeZclRemoteResultHandler_t) (
    gmosZigbeeZclCluster_t* zclCluster, void* localData,
    uint8_t zclStatus, bool requestComplete,
    uint16_t peerNodeId, uint8_t peerEndpointId,
    gmosZigbeeZclFrameHeader_t* zclFrameHeader,
    gmosBuffer_t* zclPayloadBuffer);

/**
 * This data structure provides a common encapsulation for a Zigbee
 * Cluster Library (ZCL) remote access endpoint instance.
 */
typedef struct gmosZigbeeZclEndpointRemote_t {

    // This is the array of ZCL transaction result callback handlers.
    gmosZigbeeZclRemoteResultHandler_t
        resultHandlers [GMOS_CONFIG_ZIGBEE_ZCL_REMOTE_MAX_REQUESTS];

    // This is the array of local data item pointers that will be passed
    // back to the result callback handlers.
    void* localDataItems [GMOS_CONFIG_ZIGBEE_ZCL_REMOTE_MAX_REQUESTS];

    // This holds the ZCL remote handler timeout task state.
    gmosTaskState_t task;

    // This is an array of current ZCL transaction timeout values.
    uint32_t requestTimeouts [GMOS_CONFIG_ZIGBEE_ZCL_REMOTE_MAX_REQUESTS];

    // This is the array of current ZCL transaction cluster IDs.
    uint16_t activeClusters [GMOS_CONFIG_ZIGBEE_ZCL_REMOTE_MAX_REQUESTS];

    // This is an array of current ZCL transaction sequence values.
    uint8_t sequenceValues [GMOS_CONFIG_ZIGBEE_ZCL_REMOTE_MAX_REQUESTS];

} gmosZigbeeZclEndpointRemote_t;

/**
 * Performs a one-time initialisation of a ZCL endpoint remote message
 * handler.
 * @param zclEndpoint This is the ZCL endpoint data structure that is
 *     to be initialised.
 */
void gmosZigbeeZclRemoteEndpointInit (
    gmosZigbeeZclEndpoint_t* zclEndpoint);

/**
 * Issues a unicast ZCL attribute discovery request with the specified
 * parameters.
 * @param zclCluster This is the ZCL cluster instance on the local
 *     device which is responsible for initiating the request. It will
 *     normally be a ZCL client cluster.
 * @param resultHandler This is the callback from the remote command
 *     handler which is used to forward the results of the request.
 * @param localData This is an opaque pointer to a local data item that
 *     will be passed to the corresponding result handler. A null
 *     reference may be used if there is no associated local data item.
 * @param remoteNodeId This is the address of the remote node for which
 *     the request is being issued.
 * @param remoteEndpointId This is the endpoint on the remote device for
 *     which the request is being issued.
 * @param vendorId This is the vendor ID to be used for manufacturer
 *     specific extensions, or the ZCL standard vendor ID for normal
 *     operation.
 * @param startAttrId This is the first attribute ID in the ordered
 *     attribute list at which the attribute discovery will start.
 * @param maxAttrIds This is the maximum number of attribute IDs which
 *     may be returned in the attribute discovery response message.
 * @return Returns a boolean value which will be set to 'true' on
 *     sucessfully issuing the attribute discovery request and 'false'
 *     otherwise.
 */
bool gmosZigbeeZclRemoteAttrDiscoveryRequest (
    gmosZigbeeZclCluster_t* zclCluster,
    gmosZigbeeZclRemoteResultHandler_t resultHandler, void* localData,
    uint16_t remoteNodeId, uint8_t remoteEndpointId, uint16_t vendorId,
    uint16_t startAttrId, uint8_t maxAttrIds);

/**
 * Issues a unicast ZCL attribute read request with the specified
 * parameters.
 * @param zclCluster This is the ZCL cluster instance on the local
 *     device which is responsible for initiating the request. It will
 *     normally be a ZCL client cluster.
 * @param resultHandler This is the callback from the remote command
 *     handler which is used to forward the results of the request.
 * @param localData This is an opaque pointer to a local data item that
 *     will be passed to the corresponding result handler. A null
 *     reference may be used if there is no associated local data item.
 * @param remoteNodeId This is the address of the remote node for which
 *     the request is being issued.
 * @param remoteEndpointId This is the endpoint on the remote device for
 *     which the request is being issued.
 * @param vendorId This is the vendor ID to be used for manufacturer
 *     specific extensions, or the ZCL standard vendor ID for normal
 *     operation.
 * @param attrIdList This is a list of attribute IDs that are to be read
 *     from the remote endpoint.
 * @param attrIdCount This is the number of attribute IDs that are
 *     present in the attribute ID list.
 * @return Returns a boolean value which will be set to 'true' on
 *     sucessfully issuing the attribute read request and 'false'
 *     otherwise.
 */
bool gmosZigbeeZclRemoteAttrReadRequest (
    gmosZigbeeZclCluster_t* zclCluster,
    gmosZigbeeZclRemoteResultHandler_t resultHandler, void* localData,
    uint16_t remoteNodeId, uint8_t remoteEndpointId, uint16_t vendorId,
    uint16_t attrIdList[], uint8_t attrIdCount);

/**
 * Issues a unicast ZCL attribute write request with the specified
 * parameters.
 * @param zclCluster This is the ZCL cluster instance on the local
 *     device which is responsible for initiating the request. It will
 *     normally be a ZCL client cluster.
 * @param resultHandler This is the callback from the remote command
 *     handler which is used to forward the results of the request. A
 *     null reference may be used if no write status response is
 *     required.
 * @param localData This is an opaque pointer to a local data item that
 *     will be passed to the corresponding result handler. A null
 *     reference may be used if there is no associated local data item.
 * @param remoteNodeId This is the address of the remote node for which
 *     the request is being issued.
 * @param remoteEndpointId This is the endpoint on the remote device for
 *     which the request is being issued.
 * @param vendorId This is the vendor ID to be used for manufacturer
 *     specific extensions, or the ZCL standard vendor ID for normal
 *     operation.
 * @param attrDataList This is a list of attribute data records that are
 *     to be written to the remote endpoint.
 * @param attrDataCount This is the number of attribute data records
 *     that are present in the attribute data record list.
 * @param atomicWrite This is a boolean flag which is used to select
 *     atomic writes, where all attribute updates will either succeed
 *     or fail.
 * @return Returns a boolean value which will be set to 'true' on
 *     sucessfully issuing the attribute write request and 'false'
 *     otherwise.
 */
bool gmosZigbeeZclRemoteAttrWriteRequest (
    gmosZigbeeZclCluster_t* zclCluster,
    gmosZigbeeZclRemoteResultHandler_t resultHandler, void* localData,
    uint16_t remoteNodeId, uint8_t remoteEndpointId, uint16_t vendorId,
    gmosZigbeeZclDataRecord_t attrDataList[], uint8_t attrDataCount,
    bool atomicWrite);

/**
 * Handles incoming ZCL remote attribute responses.
 * @param zclCluster This is the local cluster for which the incoming
 *     ZCL remote attribute response was received.
 * @param peerNodeId This is the node ID of the remote node that
 *     generated the ZCL remote attribute response message.
 * @param peerEndpointId This is the endpoint ID on the remote node
 *     that generated the ZCL remote attribute response message.
 * @param zclFrameHeader This is a pointer to the parsed ZCL frame
 *     header parameters.
 * @param zclPayloadBuffer This is a buffer which contains the ZCL
 *     remote attribute response payload data after the ZCL header has
 *     been removed.
 * @return Returns a ZCL status value for the ZCL default response, or
 *     a null status value if the response message has been accepted.
 */
gmosZigbeeZclStatusCode_t gmosZigbeeZclRemoteAttrResponseHandler (
    gmosZigbeeZclCluster_t* zclCluster, uint16_t peerNodeId,
    uint8_t peerEndpointId, gmosZigbeeZclFrameHeader_t* zclFrameHeader,
    gmosBuffer_t* zclPayloadBuffer);

#endif // GMOS_ZIGBEE_ZCL_CORE_REMOTE_H
