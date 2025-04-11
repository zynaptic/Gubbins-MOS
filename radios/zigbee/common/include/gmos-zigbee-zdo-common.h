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
 * This header provides common enumerations and data types for use by
 * the ZDO client and server components.
 */

#ifndef GMOS_ZIGBEE_ZDO_COMMON_H
#define GMOS_ZIGBEE_ZDO_COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include "gmos-buffers.h"

/**
 * This enumeration specifies the supported ZDO cluster IDs.
 */
typedef enum {

    // Specify the ZDO node descriptor request cluster ID.
    GMOS_ZIGBEE_ZDO_CLUSTER_NODE_DESCRIPTOR_REQUEST    = 0x0002,

    // Specify the ZDO power descriptor request cluster ID.
    GMOS_ZIGBEE_ZDO_CLUSTER_POWER_DESCRIPTOR_REQUEST   = 0x0003,

    // Specify the ZDO simple descriptor request cluster ID.
    GMOS_ZIGBEE_ZDO_CLUSTER_SIMPLE_DESCRIPTOR_REQUEST  = 0x0004,

    // Specify the ZDO active endpoint request cluster ID.
    GMOS_ZIGBEE_ZDO_CLUSTER_ACTIVE_ENDPOINT_REQUEST    = 0x0005,

    // Specify the ZDO match descriptor request cluster ID.
    GMOS_ZIGBEE_ZDO_CLUSTER_MATCH_DESCRIPTOR_REQUEST   = 0x0006,

    // Specify the ZDO device announcement cluster ID.
    GMOS_ZIGBEE_ZDO_CLUSTER_DEVICE_ANNOUNCE            = 0x0013,

    // Specify the ZDO initiate end device binding cluster ID.
    GMOS_ZIGBEE_ZDO_CLUSTER_INITIATE_BINDING_REQUEST   = 0x0020,

    // Specify the ZDO bind request cluster ID.
    GMOS_ZIGBEE_ZDO_CLUSTER_ADD_BINDING_REQUEST        = 0x0021,

    // Specify the ZDO unbind request cluster ID.
    GMOS_ZIGBEE_ZDO_CLUSTER_REMOVE_BINDING_REQUEST     = 0x0022,

    // Specify the ZDO device management leave request cluster ID.
    GMOS_ZIGBEE_ZDO_CLUSTER_DEVICE_LEAVE_REQUEST       = 0x0034,

    // Specify the ZDO permit joining cluster ID.
    GMOS_ZIGBEE_ZDO_CLUSTER_PERMIT_JOINING_REQUEST     = 0x0036,

    // Specify the ZDO node descriptor response cluster ID.
    GMOS_ZIGBEE_ZDO_CLUSTER_NODE_DESCRIPTOR_RESPONSE   = 0x8002,

    // Specify the ZDO power descriptor response cluster ID.
    GMOS_ZIGBEE_ZDO_CLUSTER_POWER_DESCRIPTOR_RESPONSE  = 0x8003,

    // Specify the ZDO simple descriptor response cluster ID.
    GMOS_ZIGBEE_ZDO_CLUSTER_SIMPLE_DESCRIPTOR_RESPONSE = 0x8004,

    // Specify the ZDO active endpoint response cluster ID.
    GMOS_ZIGBEE_ZDO_CLUSTER_ACTIVE_ENDPOINT_RESPONSE   = 0x8005,

    // Specify the ZDO match descriptor response cluster ID.
    GMOS_ZIGBEE_ZDO_CLUSTER_MATCH_DESCRIPTOR_RESPONSE  = 0x8006,

    // Specify the ZDO initiate end device binding response cluster ID.
    GMOS_ZIGBEE_ZDO_CLUSTER_INITIATE_BINDING_RESPONSE  = 0x8020,

    // Specify the ZDO bind response cluster ID.
    GMOS_ZIGBEE_ZDO_CLUSTER_ADD_BINDING_RESPONSE       = 0x8021,

    // Specify the ZDO unbind response cluster ID.
    GMOS_ZIGBEE_ZDO_CLUSTER_REMOVE_BINDING_RESPONSE    = 0x8022

} gmosZigbeeZdoClusterIds_t;

/**
 * This enumeration specifies the supported ZDO status codes.
 */
typedef enum {

    // The requested operation was completed successfully.
    GMOS_ZIGBEE_ZDO_STATUS_SUCCESS           = 0x00,

    // The supplied ZDO request type was invalid.
    GMOS_ZIGBEE_ZDO_STATUS_INVALID_REQUEST   = 0x80,

    // The specified device node ID was not found following a child
    // descriptor request to a parent.
    GMOS_ZIGBEE_ZDO_STATUS_DEVICE_NOT_FOUND  = 0x81,

    // The endpoint ID was not in the valid range 0x01 to 0xF0.
    GMOS_ZIGBEE_ZDO_STATUS_INVALID_ENDPOINT  = 0x82,

    // The requested endpoint ID has no associated simple descriptor.
    GMOS_ZIGBEE_ZDO_STATUS_INACTIVE_ENDPOINT = 0x83,

    // The requested ZDO transaction timed out. This may indicate
    // failure of a ZDO unicast transaction or completion of a ZDO
    // broadcast transaction.
    GMOS_ZIGBEE_ZDO_STATUS_TIMEOUT           = 0x85

} gmosZigbeeZdoStatusCodes_t;

/**
 * This data structure provides an encapsulation of the standard node
 * descriptor fields, and may be used for serialising and deserialising
 * ZDO node descriptors. Note that complex and user descriptors are not
 * currently supported, and the frequency band support is assumed to be
 * the standard 2.4 GHz band only.
 */
typedef struct gmosZigbeeZdoNodeDescriptor_t {

    // Specify the node type using the ZDO node type enumeration.
    uint8_t zdoNodeType;

    // Specify the MAC layer capability flags using the ZDO flag layout.
    uint8_t macCapabilityFlags;

    // Specify the server capability flags using the ZDO flag layout.
    uint8_t serverCapabilityFlags;

    // Specify the maximum buffer size for fragmented message transfer.
    uint8_t maxBufferSize;

    // Specify the device manufacturer using the Zigbee Alliance
    // manufacturer ID.
    uint16_t manufacturerId;

    // Specify the maximum input size for fragmented message transfer.
    uint16_t maxInputTransferSize;

    // Specify the maximum output size for fragmented message transfer.
    uint16_t maxOutputTransferSize;

    // Specify the Zigbee stack compliance revision (from R21 onwards).
    uint8_t stackComplianceRevision;

} gmosZigbeeZdoNodeDescriptor_t;

/**
 * This data structure provides an encapsulation of the common simple
 * descriptor fields, and may be used for unpacking a ZDO simple
 * descriptor response.
 */
typedef struct gmosZigbeeZdoSimpleDescriptor_t {

    // This is the application profile ID for the simple descriptor.
    uint16_t appProfileId;

    // This is the application device ID for the simple descriptor.
    uint16_t appDeviceId;

    // This is the application device version for the simple descriptor.
    // It includes the extra four reserved bits.
    uint8_t appDeviceVersion;

    // This is the endpoint ID associated with the simple descriptor.
    uint8_t endpointId;

    // This is the total number of input clusters in the descriptor.
    uint8_t inputClusterCount;

    // This is the total number of output clusters in the descriptor.
    uint8_t outputClusterCount;

} gmosZigbeeZdoSimpleDescriptor_t;

/**
 * Parses a ZDO reponse that contains the network address of interest
 * field immediately after the status field, returning the network
 * address of interest value.
 * @param responseBuffer This is the response buffer that was returned
 *     via a ZDO client callback.
 * @param nwkAddrOfInterest This is a pointer to the network address of
 *     interest variable which will be populated with the parsed value.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully parsing the ZDO response and 'false' othewise.
 */
bool gmosZigbeeZdoParseNwkAddrOfInterest (
    gmosBuffer_t* responseBuffer, uint16_t* nwkAddrOfInterest);

/**
 * Parses a ZDO node descriptor response, returning the standard node
 * descriptor fields.
 * @param responseBuffer This is the response buffer that was returned
 *     via a ZDO client callback.
 * @param nodeDescriptor This is a pointer to a node descriptor
 *     structure that will be populated with the parsed node descriptor
 *     data.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully parsing the ZDO response and 'false' othewise.
 */
bool gmosZigbeeZdoParseNodeDescriptor (
    gmosBuffer_t* responseBuffer,
    gmosZigbeeZdoNodeDescriptor_t* nodeDescriptor);

/**
 * Parses a ZDO power descriptor response, returning a 16-bit unsigned
 * integer value that may be interpreted using the ZDO power descriptor
 * flag enumeration.
 * @param responseBuffer This is the response buffer that was returned
 *     via a ZDO client callback.
 * @param powerDescriptor This is a pointer to a 16-bit unsigned integer
 *     that will be populated with the parsed power descriptor data.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully parsing the ZDO response and 'false' othewise.
 */
bool gmosZigbeeZdoParsePowerDescriptor (
    gmosBuffer_t* responseBuffer, uint16_t* powerDescriptor);

/**
 * Parses a ZDO endpoint list response, returning the number of
 * endpoints in the list. This may be used to parse the list of
 * endpoints returned by active endpoint requests and match descriptor
 * requests.
 * @param responseBuffer This is the response buffer that was returned
 *     via a ZDO client callback.
 * @param listLength This is a pointer to the list length variable which
 *     will be populated with the endpoint list length value.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully parsing the ZDO response and 'false' othewise.
 */
bool gmosZigbeeZdoParseEndpointListLength (
    gmosBuffer_t* responseBuffer, uint8_t* listLength);

/**
 * Parses a ZDO endpoint list response, returning the endpoint
 * identifier stored at the specified list index. This may be used to
 * parse the list of endpoints returned by active endpoint requests and
 * match descriptor requests.
 * @param responseBuffer This is the response buffer that was returned
 *     via a ZDO client callback.
 * @param listIndex This is the index into the endpoint list for which
 *     the endpoint list entry is being accessed.
 * @param endpointId This is a pointer to the endpoint ID variable which
 *     will be populated with the requested endpoint ID value.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully parsing the ZDO response and 'false' othewise.
 */
bool gmosZigbeeZdoParseEndpointListEntry (
    gmosBuffer_t* responseBuffer, uint8_t index, uint8_t* endpointId);

/**
 * Parses a ZDO simple descriptor response, returning the common simple
 * descriptor fields. The cluster lists are omitted and must be parsed
 * independently.
 * @param responseBuffer This is the response buffer that was returned
 *     via a ZDO client callback.
 * @param simpleDescriptor This is a pointer to a simple descriptor
 *     structure that will be populated with the parsed simple
 *     descriptor data.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully parsing the ZDO response and 'false' othewise.
 */
bool gmosZigbeeZdoParseSimpleDescriptor (
    gmosBuffer_t* responseBuffer,
    gmosZigbeeZdoSimpleDescriptor_t* simpleDescriptor);

/**
 * Parses a ZDO simple descriptor response for the input cluster ID
 * at a given index position. This should only be called after the same
 * response has been successfully parsed for the simple descriptor,
 * since it assumes that the response buffer contains valid data.
 * @param responseBuffer This is the response buffer that was returned
 *     via a ZDO client callback.
 * @param index This is the index into the simple descriptor input
 *     cluster ID list for which the cluster ID is being accessed.
 * @param clusterId This is a pointer to the cluster ID variable which
 *     will be populated with the cluster ID at the specified input
 *     cluster list index position.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully parsing the ZDO response and 'false' othewise.
 */
bool gmosZigbeeZdoParseInputClusterId (
    gmosBuffer_t* responseBuffer, uint8_t index, uint16_t* clusterId);

/**
 * Parses a ZDO simple descriptor response for the output cluster ID
 * at a given index position. This should only be called after the same
 * response has been successfully parsed for the simple descriptor,
 * since it assumes that the response buffer contains valid data.
 * @param responseBuffer This is the response buffer that was returned
 *     via a ZDO client callback.
 * @param index This is the index into the simple descriptor output
 *     cluster ID list for which the cluster ID is being accessed.
 * @param clusterId This is a pointer to the cluster ID variable which
 *     will be populated with the cluster ID at the specified output
 *     cluster list index position.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully parsing the ZDO response and 'false' othewise.
 */
bool gmosZigbeeZdoParseOutputClusterId (
    gmosBuffer_t* responseBuffer, uint8_t index, uint16_t* clusterId);

#endif // GMOS_ZIGBEE_ZDO_COMMON_H
