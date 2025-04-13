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
 * This file provides various common Zigbee ZDO processing routines that
 * can be used for ZDO client and server support.
 */

#include <stdint.h>
#include <stdbool.h>

#include "gmos-buffers.h"
#include "gmos-zigbee-zdo-common.h"

/*
 * Read a 16-bit value from the specified offset in the ZDO response
 * buffer.
 */
static inline bool gmosZigbeeZdoParseUInt16 (
    gmosBuffer_t* responseBuffer, uint16_t offset, uint16_t* value)
{
    uint8_t dataBytes [2];
    uint_fast16_t parsedValue;
    bool readOk;

    // Read 16-bit value from the response buffer.
    readOk = gmosBufferRead (responseBuffer, offset, dataBytes, 2);
    if (readOk) {
        parsedValue = (uint16_t) dataBytes [0];
        parsedValue |= ((uint16_t) dataBytes [1]) << 8;
        *value = parsedValue;
    }
    return readOk;
}

/*
 * Parses a ZDO reponse that contains the network address of interest
 * field immediately after the status field, returning the network
 * address of interest value.
 */
bool gmosZigbeeZdoParseNwkAddrOfInterest (
    gmosBuffer_t* responseBuffer, uint16_t* nwkAddrOfInterest)
{
    return gmosZigbeeZdoParseUInt16 (
        responseBuffer, 2, nwkAddrOfInterest);
}

/*
 * Parses a ZDO node descriptor response, returning the standard node
 * descriptor fields.
 */
bool gmosZigbeeZdoParseNodeDescriptor (
    gmosBuffer_t* responseBuffer,
    gmosZigbeeZdoNodeDescriptor_t* nodeDescriptor)
{
    uint8_t descriptorData [13];
    uint_fast16_t manufacturerId;
    uint_fast16_t maxInputTransferSize;
    uint_fast16_t maxOutputTransferSize;

    // Read the descriptor data from the response buffer.
    if (!gmosBufferRead (responseBuffer, 4,
        descriptorData, sizeof (descriptorData))) {
        return false;
    }

    // Extract 16-bit fields.
    manufacturerId = (uint_fast16_t) descriptorData [3];
    manufacturerId |= ((uint_fast16_t) descriptorData [4]) << 8;
    maxInputTransferSize = (uint_fast16_t) descriptorData [6];
    maxInputTransferSize |= ((uint_fast16_t) descriptorData [7]) << 8;
    maxOutputTransferSize = (uint_fast16_t) descriptorData [10];
    maxOutputTransferSize |= ((uint_fast16_t) descriptorData [11]) << 8;

    // Populate the node descriptor fields.
    nodeDescriptor->manufacturerId = manufacturerId;
    nodeDescriptor->maxInputTransferSize = maxInputTransferSize;
    nodeDescriptor->maxOutputTransferSize = maxOutputTransferSize;
    nodeDescriptor->zdoNodeType = descriptorData [0] & 0x07;
    nodeDescriptor->macCapabilityFlags = descriptorData [2];
    nodeDescriptor->maxBufferSize = descriptorData [5];
    nodeDescriptor->serverCapabilityFlags = descriptorData [8];
    nodeDescriptor->stackComplianceRevision = descriptorData [9] >> 1;
    return true;
}

/*
 * Parses a ZDO power descriptor response, returning a 16-bit unsigned
 * integer value that may be interpreted using the ZDO power descriptor
 * flag enumeration.
 */
bool gmosZigbeeZdoParsePowerDescriptor (
    gmosBuffer_t* responseBuffer, uint16_t* powerDescriptor)
{
    return gmosZigbeeZdoParseUInt16 (
        responseBuffer, 4, powerDescriptor);
}

/*
 * Parses a ZDO endpoint list response, returning the number of
 * endpoints in the list.
 */
bool gmosZigbeeZdoParseEndpointListLength (
    gmosBuffer_t* responseBuffer, uint8_t* listLength)
{
    uint8_t endpointCount;
    uint_fast16_t bufferSize;

    // This reads the endpoint count field and checks that it is
    // consistent with the ZDO response message length.
    if (gmosBufferRead (responseBuffer, 4, &endpointCount, 1)) {
        bufferSize = gmosBufferGetSize (responseBuffer);
        if (bufferSize == ((uint_fast16_t) endpointCount) + 5) {
            *listLength = endpointCount;
            return true;
        }
    }
    return false;
}

/*
 * Parses a ZDO endpoint list response, returning the endpoint
 * identifier stored at the specified list index.
 */
bool gmosZigbeeZdoParseEndpointListEntry (
    gmosBuffer_t* responseBuffer, uint8_t index, uint8_t* endpointId)
{
    return gmosBufferRead (responseBuffer, 5 + index, endpointId, 1);
}

/*
 * Parses a ZDO simple descriptor response, returning the common simple
 * descriptor fields. The cluster lists are omitted and must be parsed
 * independently.
 */
bool gmosZigbeeZdoParseSimpleDescriptor (
    gmosBuffer_t* responseBuffer,
    gmosZigbeeZdoSimpleDescriptor_t* simpleDescriptor)
{
    uint8_t fixedFields [8];
    uint_fast8_t inputClusterCount;
    uint_fast8_t outputClusterOffset;
    uint8_t outputClusterCount;
    uint_fast8_t descriptorSize;
    uint_fast16_t bufferSize;
    uint_fast16_t appProfileId;
    uint_fast16_t appDeviceId;

    // Read the fixed format data at the start of the descriptor.
    if (!gmosBufferRead (responseBuffer, 4, fixedFields, 8)) {
        return false;
    }

    // Read the output cluster count at the expected offset.
    inputClusterCount = fixedFields [7];
    outputClusterOffset = 12 + 2 * inputClusterCount;
    if (!gmosBufferRead (responseBuffer,
        outputClusterOffset, &outputClusterCount, 1)) {
        return false;
    }

    // Check for consistency between the descriptor length fields.
    descriptorSize = fixedFields [0];
    bufferSize = gmosBufferGetSize (responseBuffer);
    if ((bufferSize != 5 + descriptorSize) ||
        (descriptorSize != 8 + 2 * (inputClusterCount + outputClusterCount))) {
        return false;
    }

    // Extract 16-bit fields.
    appProfileId = (uint_fast16_t) fixedFields [2];
    appProfileId |= ((uint_fast16_t) fixedFields [3]) << 8;
    appDeviceId = (uint_fast16_t) fixedFields [4];
    appDeviceId |= ((uint_fast16_t) fixedFields [5]) << 8;

    // Populate the simple descriptor fields.
    simpleDescriptor->endpointId = fixedFields [1];
    simpleDescriptor->appProfileId = appProfileId;
    simpleDescriptor->appDeviceId = appDeviceId;
    simpleDescriptor->appDeviceVersion = fixedFields [6];
    simpleDescriptor->inputClusterCount = inputClusterCount;
    simpleDescriptor->outputClusterCount = outputClusterCount;
    return true;
}

/*
 * Parses a ZDO simple descriptor response for the input cluster ID
 * at a given index position.
 */
bool gmosZigbeeZdoParseInputClusterId (
    gmosBuffer_t* responseBuffer, uint8_t index, uint16_t* clusterId)
{
    uint8_t inputClusterCount;

    // Check that the specified index value is valid.
    if ((!gmosBufferRead (responseBuffer, 11, &inputClusterCount, 1)) ||
        (index >= inputClusterCount)) {
        return false;
    }

    // Read the 16-bit cluster ID.
    return gmosZigbeeZdoParseUInt16 (
        responseBuffer, 12 + 2 * index, clusterId);
}

/*
 * Parses a ZDO simple descriptor response for the output cluster ID
 * at a given index position.
 */
bool gmosZigbeeZdoParseOutputClusterId (
    gmosBuffer_t* responseBuffer, uint8_t index, uint16_t* clusterId)
{
    uint8_t inputClusterCount;
    uint_fast8_t outputClusterOffset;
    uint8_t outputClusterCount;

    // The input cluster count is required for calculating the offset
    // to the output cluster list.
    if (!gmosBufferRead (responseBuffer, 11, &inputClusterCount, 1)) {
        return false;
    }

    // Check that the specified index value is valid.
    outputClusterOffset = 12 + 2 * inputClusterCount;
    if ((!gmosBufferRead (responseBuffer,
        outputClusterOffset, &outputClusterCount, 1)) ||
        (index >= outputClusterCount)) {
        return false;
    }

    // Read the 16-bit cluster ID.
    return gmosZigbeeZdoParseUInt16 (
        responseBuffer, outputClusterOffset + 1 + 2 * index, clusterId);
}
