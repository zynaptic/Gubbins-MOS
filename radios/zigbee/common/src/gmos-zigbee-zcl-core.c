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
 * This file implements the API for the Zigbee Cluster Library (ZCL)
 * common core components.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-driver-eeprom.h"
#include "gmos-zigbee-config.h"
#include "gmos-zigbee-stack.h"
#include "gmos-zigbee-endpoint.h"
#include "gmos-zigbee-zcl-core.h"
#include "gmos-zigbee-zcl-core-local.h"
#include "gmos-zigbee-zcl-core-remote.h"

/*
 * If used, check for the expected floating point data sizes.
 */
#if GMOS_CONFIG_ZIGBEE_ZCL_FLOATING_POINT_SUPPORT
#if ((sizeof (float) != 4) || (sizeof (double) != 8))
#error "Non-standard 'float' or 'double' representation detected."
#endif
#endif

/*
 * Map fixed size data type identifiers onto the corresponding
 * serialized data sizes. Returns the serialized data size or 0xFF if
 * the data type is not a known fixed size.
 */
static uint8_t getZclDataSize (uint8_t dataType)
{
    uint_fast8_t typeCategory;
    uint_fast8_t dataSize;

    // Process the fixed width type categories, where the data size can
    // be derived directly from the type encoding.
    typeCategory = dataType & 0xF8;
    if ((typeCategory == GMOS_ZIGBEE_ZCL_DATA_TYPE_GENERAL_X8) ||
        (typeCategory == GMOS_ZIGBEE_ZCL_DATA_TYPE_BITMAP_X8) ||
        (typeCategory == GMOS_ZIGBEE_ZCL_DATA_TYPE_INTEGER_U8) ||
        (typeCategory == GMOS_ZIGBEE_ZCL_DATA_TYPE_INTEGER_S8)) {
        dataSize = 1 + (dataType & 0x07);
    }

    // Process remaining type encodings.
    else switch (dataType) {

        // List of zero width data types.
        case GMOS_ZIGBEE_ZCL_DATA_TYPE_NO_DATA :
        case GMOS_ZIGBEE_ZCL_DATA_TYPE_UNKNOWN :
            dataSize = 0;
            break;

        // List of 8-bit fixed length data types.
        case GMOS_ZIGBEE_ZCL_DATA_TYPE_BOOLEAN :
        case GMOS_ZIGBEE_ZCL_DATA_TYPE_ENUM_X8 :
            dataSize = 1;
            break;

        // List of 16-bit fixed length data types.
        case GMOS_ZIGBEE_ZCL_DATA_TYPE_ENUM_X16 :
        case GMOS_ZIGBEE_ZCL_DATA_TYPE_CLUSTER_ID :
        case GMOS_ZIGBEE_ZCL_DATA_TYPE_ATTRIBUTE_ID :
            dataSize = 2;
            break;

        // List of 32-bit fixed length data types. Note that the float
        // type maps to a NaN when the default integer value is
        // interpreted as a floating point value.
#if GMOS_CONFIG_ZIGBEE_ZCL_FLOATING_POINT_SUPPORT
        case GMOS_ZIGBEE_ZCL_DATA_TYPE_FLOAT_F32 :
#endif
        case GMOS_ZIGBEE_ZCL_DATA_TYPE_TIME_OF_DAY :
        case GMOS_ZIGBEE_ZCL_DATA_TYPE_CALENDAR_DATE :
        case GMOS_ZIGBEE_ZCL_DATA_TYPE_UTC_TIME :
        case GMOS_ZIGBEE_ZCL_DATA_TYPE_BACNET_OID :
            dataSize = 4;
            break;

        // List of 64-bit fixed length data types. Note that the double
        // precision float type maps to a NaN when the default integer
        // value is interpreted as a floating point value.
#if GMOS_CONFIG_ZIGBEE_ZCL_FLOATING_POINT_SUPPORT
        case GMOS_ZIGBEE_ZCL_DATA_TYPE_FLOAT_F64 :
#endif
        case GMOS_ZIGBEE_ZCL_DATA_TYPE_IEEE_MAC_ADDR :
            dataSize = 8;
            break;

        // The remaining data type encodings are not supported, which
        // means the associated command field size cannot be determined.
        default :
            dataSize = 0xFF;
            break;
    }
    return dataSize;
}

/*
 * Format the default response message, placing it in the specified
 * response buffer.
 */
static inline void formatZclDefaultResponse (
    gmosZigbeeZclCluster_t* zclCluster, int32_t vendorId,
    uint8_t zclSequence, uint8_t zclCommand,
    uint8_t zclStatus, gmosBuffer_t* responseBuffer)
{
    gmosZigbeeZclFrameHeader_t frameHeader;
    uint8_t response [2];

    // Populate the ZCL frame header.
    frameHeader.frameControl =
        GMOS_ZIGBEE_ZCL_FRAME_CONTROL_TYPE_GENERAL |
        GMOS_ZIGBEE_ZCL_FRAME_CONTROL_NO_DEFAULT_RESP;
    frameHeader.vendorId = vendorId;
    frameHeader.zclSequence = zclSequence;
    frameHeader.zclFrameId = GMOS_ZIGBEE_ZCL_PROFILE_DEFAULT_RESPONSE;
    if (!gmosZigbeeZclPrependHeader (
        zclCluster, responseBuffer, &frameHeader)) {
        return;
    }

    // Fill in the default response payload.
    response [0] = zclCommand;
    response [1] = zclStatus;

    // Copy the formatted response to the response buffer.
    if (!gmosBufferAppend (responseBuffer, response, 2)) {
        gmosBufferReset (responseBuffer, 0);
    }
}

/*
 * Parses a ZCL frame header, extracting the header fields from the
 * specified data buffer and placing them in the frame header data
 * structure. After successful parsing, the frame header octets will be
 * removed from the start of the data buffer.
 */
static inline bool extractZclFrameHeader (
    gmosZigbeeZclCluster_t* zclCluster, gmosBuffer_t* dataBuffer,
    gmosZigbeeZclFrameHeader_t* frameHeader)
{
    uint8_t frameData [3];
    uint_fast8_t frameType;
    uint_fast8_t headerLength;
    bool clusterIsServer;
    bool frameSourceIsClient;

    // Extract the standard ZCL frame header.
    if (!gmosBufferRead (dataBuffer, 0, frameData, 3)) {
        return false;
    }

    // Process conventional headers without the manufacturer code.
    frameHeader->frameControl = frameData [0];
    if ((frameHeader->frameControl &
        GMOS_ZIGBEE_ZCL_FRAME_CONTROL_VENDOR_SPECIFIC) == 0) {
        frameHeader->vendorId = GMOS_ZIGBEE_ZCL_STANDARD_VENDOR_ID;
        headerLength = 3;
    }

    // Read the additional header fields for manufacturer specific
    // frames.
    else {
        frameHeader->vendorId = frameData [1];
        frameHeader->vendorId |= ((int32_t) frameData [2]) << 8;
        if (!gmosBufferRead (dataBuffer, 3, &frameData [1], 2)) {
            return false;
        }
        headerLength = 5;
    }
    frameHeader->zclSequence = frameData [1];
    frameHeader->zclFrameId = frameData [2];
    frameType = frameHeader->frameControl &
        GMOS_ZIGBEE_ZCL_FRAME_CONTROL_TYPE_MASK;

    // Incoming commands should always have a valid frame type. Commands
    // with an invalid header are silently discarded.
    if ((frameType != GMOS_ZIGBEE_ZCL_FRAME_CONTROL_TYPE_GENERAL) &&
        (frameType != GMOS_ZIGBEE_ZCL_FRAME_CONTROL_TYPE_CLUSTER)) {
        return false;
    }

    // Commands received from a server should have direction flag set.
    // Commands received from a client should have direction flag clear.
    clusterIsServer = ((zclCluster->baseCluster.clusterOptions &
        GMOS_ZIGBEE_CLUSTER_OPTION_OUTPUT) == 0) ? true : false;
    frameSourceIsClient = ((frameHeader->frameControl &
        GMOS_ZIGBEE_ZCL_FRAME_CONTROL_SOURCE_IS_SERVER) == 0) ?
        true : false;
    if (clusterIsServer != frameSourceIsClient) {
        return false;
    }

    // Trim the header bytes on successful completion.
    return gmosBufferRebase (dataBuffer,
        gmosBufferGetSize (dataBuffer) - headerLength);
}

/*
 * Process ZCL general commands.
 */
static inline uint8_t processZclGeneralCommand (
    gmosZigbeeZclCluster_t* zclCluster, uint16_t peerNodeId,
    uint8_t peerEndpointId, gmosZigbeeZclFrameHeader_t* zclFrameHeader,
    gmosBuffer_t* zclPayloadBuffer, gmosBuffer_t* responseBuffer)
{
    (void) responseBuffer;
    uint_fast8_t zclStatus;

    // Select the appropriate general command handler.
    switch (zclFrameHeader->zclFrameId) {

        // Queue long running local attribute commands for processing.
        case GMOS_ZIGBEE_ZCL_PROFILE_DISCOVER_ATTRS_REQUEST :
        case GMOS_ZIGBEE_ZCL_PROFILE_READ_ATTRS_REQUEST :
        case GMOS_ZIGBEE_ZCL_PROFILE_WRITE_ATTRS_REQUEST :
            zclStatus = gmosZigbeeZclLocalAttrCommandQueueRequest (
                zclCluster, peerNodeId, peerEndpointId, zclFrameHeader,
                zclPayloadBuffer);
            break;

        // Forward all response messages to the remote attribute
        // processing entity. Default responses can theoretically be
        // sent in response to another non-default response, but this
        // mode of operation is not currently supported, and all default
        // responses are sent to the remote attribute processing entity.
        case GMOS_ZIGBEE_ZCL_PROFILE_DISCOVER_ATTRS_RESPONSE :
        case GMOS_ZIGBEE_ZCL_PROFILE_READ_ATTRS_RESPONSE :
        case GMOS_ZIGBEE_ZCL_PROFILE_WRITE_ATTRS_RESPONSE :
        case GMOS_ZIGBEE_ZCL_PROFILE_DEFAULT_RESPONSE :
            zclStatus = gmosZigbeeZclRemoteAttrResponseHandler (
                zclCluster, peerNodeId, peerEndpointId, zclFrameHeader,
                zclPayloadBuffer);
            break;

        // Report unsupported commands.
        default :
            zclStatus = GMOS_ZIGBEE_ZCL_STATUS_UNSUP_COMMAND;
            break;
    }
    return zclStatus;
}

/*
 * Implement received message handler for ZCL clusters.
 */
static void zclClusterRxMessageHandler (
    gmosZigbeeStack_t* zigbeeStack, gmosZigbeeCluster_t* zigbeeCluster,
    gmosZigbeeApsFrame_t* apsFrame, gmosBuffer_t* responseBuffer)
{
    uint_fast8_t frameType;
    uint_fast8_t zclStatus;
    gmosBuffer_t* requestBuffer = &(apsFrame->payloadBuffer);
    gmosZigbeeZclFrameHeader_t requestHeader;
    gmosZigbeeZclCluster_t* zclCluster =
        (gmosZigbeeZclCluster_t*) zigbeeCluster->clusterData;
    (void) zigbeeStack;

    // Attempt to parse the request header.
    if (!extractZclFrameHeader (
        zclCluster, requestBuffer, &requestHeader)) {
        return;
    }
    frameType = requestHeader.frameControl &
        GMOS_ZIGBEE_ZCL_FRAME_CONTROL_TYPE_MASK;

    // Process general profile wide ZCL commands.
    if (frameType == GMOS_ZIGBEE_ZCL_FRAME_CONTROL_TYPE_GENERAL) {
        zclStatus = processZclGeneralCommand (zclCluster,
            apsFrame->peer.nodeId, apsFrame->sourceEndpoint,
            &requestHeader, requestBuffer, responseBuffer);
    }

    // Process cluster specific commands.
    else {
        zclStatus = GMOS_ZIGBEE_ZCL_STATUS_UNSUP_COMMAND;
    }

    // A null status value indicates that processing is complete and
    // that no default response is required.
    if (zclStatus == GMOS_ZIGBEE_ZCL_STATUS_NULL) {
        return;
    }

    // Default responses are only generated for unicast messages.
    if ((apsFrame->apsMsgType != GMOS_ZIGBEE_APS_MSG_TYPE_RX_UNICAST) &&
        (apsFrame->apsMsgType != GMOS_ZIGBEE_APS_MSG_TYPE_RX_UNICAST_REPLY)) {
        return;
    }

    // Default responses are not generated in response to other default
    // messages or if the disable default response flag is set.
    if ((requestHeader.zclFrameId ==
        GMOS_ZIGBEE_ZCL_PROFILE_DEFAULT_RESPONSE) ||
        ((requestHeader.frameControl &
        GMOS_ZIGBEE_ZCL_FRAME_CONTROL_NO_DEFAULT_RESP) != 0)) {
        return;
    }

    // Format the default response and return it in the immediate
    // response buffer.
    formatZclDefaultResponse (zclCluster, requestHeader.vendorId,
        requestHeader.zclSequence, requestHeader.zclFrameId, zclStatus,
        responseBuffer);
}

/*
 * Performs a one-time initialisation of a ZCL endpoint data structure.
 * This should be called during initialisation to set up the ZCL
 * endpoint for subsequent use.
 */
void gmosZigbeeZclEndpointInit (
    gmosZigbeeZclEndpoint_t* zclEndpoint,
    gmosZigbeeZclEndpointLocal_t* zclLocal,
    gmosZigbeeZclEndpointRemote_t* zclRemote,
    uint8_t endpointId, uint16_t appProfileId, uint16_t appDeviceId)
{
    // Initialise the local endpoint message handler if required.
    zclEndpoint->local = zclLocal;
    if (zclLocal != NULL) {
        gmosZigbeeZclLocalEndpointInit (zclEndpoint);
    }

    // Initialise the remote endpoint message handler if required.
    zclEndpoint->remote = zclRemote;
    if (zclRemote != NULL) {
        gmosZigbeeZclRemoteEndpointInit (zclEndpoint);
    }

    // Initialise the Zigbee application endpoint.
    gmosZigbeeEndpointInit (&(zclEndpoint->baseEndpoint),
        endpointId, appProfileId, appDeviceId);
}

/*
 * Perform a one-time initialisation of a ZCL cluster data structure.
 * This should be called during initialisation to set up the ZCL
 * cluster for subsequent use.
 */
bool gmosZigbeeZclClusterInit (gmosZigbeeZclCluster_t* zclCluster,
    uint16_t clusterId, bool isServer, gmosDriverEepromTag_t eepromTag,
    uint8_t* eepromDefaultData, uint16_t eepromLength)
{
    uint_fast8_t clusterOptions;
    gmosDriverEeprom_t* eeprom = gmosDriverEepromGetInstance ();
    gmosDriverEepromStatus_t eepromStatus;

    // Set the initial server instance state.
    zclCluster->attrList = NULL;

    // Set the appropriate application cluster options.
    if (isServer) {
        clusterOptions = GMOS_ZIGBEE_CLUSTER_OPTION_INPUT;
    } else {
        clusterOptions = GMOS_ZIGBEE_CLUSTER_OPTION_OUTPUT;
    }

    // Initialise the EEPROM record if required. Note that record
    // creation on factory reset is assumed to run correctly to
    // completion, so no completion callback is provided.
    if (eepromLength > 0) {
        zclCluster->eepromTag = eepromTag;
        eepromStatus = gmosDriverEepromRecordCreate (eeprom,
            eepromTag, eepromDefaultData, eepromLength, NULL, NULL);
        if ((eepromStatus != GMOS_DRIVER_EEPROM_STATUS_SUCCESS) &&
            (eepromStatus != GMOS_DRIVER_EEPROM_STATUS_TAG_EXISTS)) {
            return false;
        }
    } else {
        zclCluster->eepromTag = GMOS_DRIVER_EEPROM_TAG_INVALID;
    }

    // Initialise the general purpose application cluster.
    gmosZigbeeClusterInit (&(zclCluster->baseCluster), clusterId,
        clusterOptions, zclCluster, zclClusterRxMessageHandler);
    return true;
}

/*
 * Performs a one-time initialisation of a ZCL cluster attribute data
 * structure that uses fixed size data types. Attributes are initialised
 * with the appropriate 'non-value' data.
 */
bool gmosZigbeeZclAttrInit (gmosZigbeeZclAttr_t* zclAttr,
    uint16_t vendorId, uint16_t attrId, uint8_t attrType,
    bool remoteWriteEn)
{
    uint_fast8_t attrSize;
    uint_fast8_t attrOptions;
    uint_fast8_t typeCategory;

    // Look up the size of the fixed width data type, checking that it
    // is a supported type.
    attrSize = getZclDataSize (attrType);
    if (attrSize == 0xFF) {
        return false;
    }

    // Select the default value for the various types. Where appropriate
    // this will be the ZCL 'invalid number' representation.
    typeCategory = attrType & 0xF8;
    if ((typeCategory == GMOS_ZIGBEE_ZCL_DATA_TYPE_GENERAL_X8) ||
        (typeCategory == GMOS_ZIGBEE_ZCL_DATA_TYPE_BITMAP_X8)) {
        if (attrSize <= 4) {
            zclAttr->attrData.valueInt32U = 0;
        } else {
            zclAttr->attrData.valueInt64U = 0;
        }
    } else if (typeCategory == GMOS_ZIGBEE_ZCL_DATA_TYPE_INTEGER_S8) {
        if (attrSize <= 4) {
            zclAttr->attrData.valueInt32S =
                0xFFFFFFFFL << (8 * attrSize - 1);
        } else {
            zclAttr->attrData.valueInt64S =
                0xFFFFFFFFFFFFFFFFLL << (8 * attrSize - 1);
        }
    } else {
        if (attrSize <= 4) {
            zclAttr->attrData.valueInt32U = 0xFFFFFFFFL;
        } else {
            zclAttr->attrData.valueInt64U = 0xFFFFFFFFFFFFFFFFLL;
        }
    }

    // Select attribute option flags. The attribute size is encoded in
    // the lower nibble of the options byte.
    attrOptions = attrSize;
    if (remoteWriteEn) {
        attrOptions |= GMOS_ZIGBEE_ZCL_ATTR_OPTION_REMOTE_WRITE_EN;
    }

    // Populate the remaining attribute data fields.
    zclAttr->nextAttr = NULL;
    zclAttr->report.producer = NULL;
    zclAttr->vendorId = vendorId;
    zclAttr->attrId = attrId;
    zclAttr->attrType = attrType;
    zclAttr->attrOptions = attrOptions;
    return true;
}

/*
 * Performs a one-time initialisation of a ZCL cluster attribute data
 * structure that uses string data types stored in an octet array.
 */
bool gmosZigbeeZclAttrInitString (gmosZigbeeZclAttr_t* zclAttr,
    uint16_t vendorId, uint16_t attrId, uint8_t attrType,
    bool remoteWriteEn, uint8_t* attrData, uint8_t attrDataSize,
    uint8_t initDataSize)
{
    uint_fast8_t attrOptions;

    // Check for valid parameters.
    if ((attrType != GMOS_ZIGBEE_ZCL_DATA_TYPE_OCTET_STRING) &&
        (attrType != GMOS_ZIGBEE_ZCL_DATA_TYPE_CHAR_STRING)) {
        return false;
    }

    // Select attribute option flags.
    attrOptions = GMOS_ZIGBEE_ZCL_ATTR_OPTION_OCTET_ARRAY;
    if (remoteWriteEn) {
        attrOptions |= GMOS_ZIGBEE_ZCL_ATTR_OPTION_REMOTE_WRITE_EN;
    }

    // Populate the attribute data fields.
    zclAttr->nextAttr = NULL;
    zclAttr->report.producer = NULL;
    zclAttr->vendorId = vendorId;
    zclAttr->attrId = attrId;
    zclAttr->attrType = attrType;
    zclAttr->attrOptions = attrOptions;
    zclAttr->attrData.octetArray.dataPtr = attrData;
    zclAttr->attrData.octetArray.dataLength = initDataSize;
    zclAttr->attrData.octetArray.maxDataLength = attrDataSize;
    return true;
}

/*
 * Performs a one-time initialisation of a ZCL cluster attribute data
 * structure for dynamic attribute value access via getter and setter
 * functions.
 */
bool gmosZigbeeZclAttrInitDynamic (gmosZigbeeZclAttr_t* zclAttr,
    uint16_t vendorId, uint16_t attrId, uint8_t attrType,
    gmosZigbeeZclAttrSetter_t attrSetter,
    gmosZigbeeZclAttrGetter_t attrGetter)
{
    uint_fast8_t attrOptions;

    // Select the appropriate attribute options.
    attrOptions = GMOS_ZIGBEE_ZCL_ATTR_OPTION_DYNAMIC_ACCESS;
    if (attrSetter != NULL) {
        attrOptions |= GMOS_ZIGBEE_ZCL_ATTR_OPTION_REMOTE_WRITE_EN;
    }

    // Populate the common attribute fields.
    zclAttr->nextAttr = NULL;
    zclAttr->report.producer = NULL;
    zclAttr->vendorId = vendorId;
    zclAttr->attrId = attrId;
    zclAttr->attrType = attrType;
    zclAttr->attrOptions = attrOptions;
    zclAttr->attrData.dynamic.setter = attrSetter;
    zclAttr->attrData.dynamic.getter = attrGetter;
    return true;
}

/*
 * Attaches an attribute report producer to an attribute and configures
 * it.
 */
void gmosZigbeeZclAttrSetProducer (gmosZigbeeZclAttr_t* zclAttr,
    gmosZigbeeZclReportProducer_t* producerData, uint8_t eepromOffset)
{
    // Configure the producer data structure.
    producerData->eepromOffset = eepromOffset;

    // Attach the producer data structure to the attribute.
    zclAttr->attrOptions |= GMOS_ZIGBEE_ZCL_ATTR_OPTION_REPORT_PRODUCER;
    zclAttr->report.producer = producerData;
}

/*
 * Attaches an attribute report consumer to an attribute and configures
 * it.
 */
void gmosZigbeeZclAttrSetConsumer (gmosZigbeeZclAttr_t* zclAttr,
    gmosZigbeeZclReportConsumer_t* consumerData, uint8_t eepromOffset)
{
    // Configure the consumer data structure.
    consumerData->eepromOffset = eepromOffset;

    // Attach the consumer data structure to the attribute.
    zclAttr->attrOptions &= ~GMOS_ZIGBEE_ZCL_ATTR_OPTION_REPORT_PRODUCER;
    zclAttr->report.consumer = consumerData;
}

/*
 * Attaches a new ZCL local attribute to a ZCL cluster instance.
 * Attributes are inserted into the attribute list in order of ascending
 * attribute ID.
 */
bool gmosZigbeeZclClusterAddAttr (gmosZigbeeZclCluster_t* zclCluster,
    gmosZigbeeZclAttr_t* zclAttr)
{
    gmosZigbeeZclAttr_t** zclAttrPtr = &(zclCluster->attrList);

    // Search for the correct attribute insertion point.
    while (*zclAttrPtr != NULL) {
        uint_fast16_t currentVendorId = (*zclAttrPtr)->vendorId;
        uint_fast16_t currentAttrId = (*zclAttrPtr)->attrId;

        // Indicate failure on attribute ID conflicts. Note that the
        // specification permits the same attribute ID to be present for
        // both standard and vendor specific forms.
        if ((currentAttrId == zclAttr->attrId) &&
            (currentVendorId == zclAttr->vendorId)) {
            return false;
        }

        // Found valid attribute insertion point.
        else if (currentAttrId >= zclAttr->attrId) {
            break;
        }

        // Move on to next list entry.
        else {
            zclAttrPtr = &(*zclAttrPtr)->nextAttr;
        }
    }

    // Insert the new attribute into the list at the specified location.
    zclAttr->nextAttr = *zclAttrPtr;
    *zclAttrPtr = zclAttr;
    return true;
}

/*
 * Requests the ZCL attribute instance on a ZCL cluster, given the
 * attribute ID.
 */
gmosZigbeeZclAttr_t* gmosZigbeeZclGetAttrInstance (
    gmosZigbeeZclCluster_t* zclCluster,
    uint16_t vendorId, uint16_t attrId)
{
    gmosZigbeeZclAttr_t* zclAttr = zclCluster->attrList;
    while (zclAttr != NULL) {
        if ((zclAttr->vendorId == vendorId) &&
            (zclAttr->attrId == attrId)) {
            break;
        } else {
            zclAttr = zclAttr->nextAttr;
        }
    }
    return zclAttr;
}

/*
 * Determines the number of octets used to represent a ZCL serialized
 * data value.
 */
gmosZigbeeZclStatusCode_t gmosZigbeeZclParseDataSize (
    gmosBuffer_t* dataBuffer, uint16_t dataItemOffset,
    uint8_t* dataSize)
{
    uint8_t dataType;
    uint8_t stringSize;
    uint_fast8_t fixedDataSize;
    gmosZigbeeZclStatusCode_t status = GMOS_ZIGBEE_ZCL_STATUS_SUCCESS;

    // Extract the data type byte from the data buffer.
    if (!gmosBufferRead (dataBuffer, dataItemOffset, &dataType, 1)) {
        status = GMOS_ZIGBEE_ZCL_STATUS_ABORT;
    }

    // Process variable length string data types.
    else if ((dataType == GMOS_ZIGBEE_ZCL_DATA_TYPE_OCTET_STRING) ||
        (dataType == GMOS_ZIGBEE_ZCL_DATA_TYPE_CHAR_STRING)) {
        if (!gmosBufferRead (
            dataBuffer, dataItemOffset + 1, &stringSize, 1)) {
            status = GMOS_ZIGBEE_ZCL_STATUS_ABORT;
        }
        if (stringSize < 0xFF) {
            *dataSize = stringSize + 1;
        } else {
            *dataSize = 1;
        }
    }

    // Attempt to determine the fixed data size. Unknown data type
    // encodings are not supported, which means the associated command
    // field size cannot be determined.
    else {
        fixedDataSize = getZclDataSize (dataType);
        if (fixedDataSize == 0xFF) {
            status = GMOS_ZIGBEE_ZCL_STATUS_ABORT;
        } else {
            *dataSize = fixedDataSize;
        }
    }
    return status;
}

/*
 * Parses a complete attribute data record from a buffer, as included
 * in read attribute response messages and attribute reporting messages.
 */
gmosZigbeeZclStatusCode_t gmosZigbeeZclParseDataRecord (
    gmosBuffer_t* dataBuffer, uint16_t recordOffset, bool checkStatus,
    gmosZigbeeZclDataRecord_t* dataRecord, uint8_t* octetArray,
    uint8_t octetArraySize, uint8_t* recordSize)
{
    uint8_t dataArray [8];
    uint8_t dataType;
    uint_fast8_t dataSize;
    uint_fast8_t headerSize;
    uint_fast8_t dataItemOffset;
    uint_fast8_t stringSize;
    uint_fast8_t typeCategory;
    uint_fast8_t i;
    uint_fast8_t parsedRecordSize = 0;

    // Set the attribute status value to a null value by default. This
    // indicates that a status field was not present in the record.
    dataRecord->attrStatus = GMOS_ZIGBEE_ZCL_STATUS_NULL;

    // Extract the attribute ID and optional status byte from the data
    // buffer.
    if (!gmosBufferRead (dataBuffer, recordOffset, dataArray, 3)) {
        return GMOS_ZIGBEE_ZCL_STATUS_ABORT;
    }
    dataRecord->attrId = (uint16_t) dataArray [0];
    dataRecord->attrId |= ((uint16_t) dataArray [1]) << 8;

    // Check the parsed attribute status if required.
    if (checkStatus) {
        dataRecord->attrStatus = dataArray [2];
        if (dataRecord->attrStatus != GMOS_ZIGBEE_ZCL_STATUS_SUCCESS) {
            return dataRecord->attrStatus;
        }
        headerSize = 3;
    } else {
        headerSize = 2;
    }

    // Extract the data type byte from the data buffer.
    dataItemOffset = recordOffset + headerSize;
    if (!gmosBufferRead (dataBuffer, dataItemOffset, &dataType, 1)) {
        return GMOS_ZIGBEE_ZCL_STATUS_ABORT;
    }
    dataRecord->attrType = dataType;
    typeCategory = dataType & 0xF8;

    // Select the initial number of data bytes to be read back.
    if ((dataType == GMOS_ZIGBEE_ZCL_DATA_TYPE_OCTET_STRING) ||
        (dataType == GMOS_ZIGBEE_ZCL_DATA_TYPE_CHAR_STRING)) {
        dataRecord->attrData.octetArray.dataPtr = octetArray;
        dataRecord->attrData.octetArray.dataLength = 0xFF;
        dataRecord->attrData.octetArray.maxDataLength = octetArraySize;
        dataSize = 1;
    } else {
        dataSize = getZclDataSize (dataType);
        if (dataSize > 8) {
            return GMOS_ZIGBEE_ZCL_STATUS_ABORT;
        }
    }

    // Read the fixed data bytes.
    if (!gmosBufferRead (dataBuffer,
        dataItemOffset + 1, dataArray, dataSize)) {
        return GMOS_ZIGBEE_ZCL_STATUS_ABORT;
    }

    // Process variable length string data types.
    if ((dataType == GMOS_ZIGBEE_ZCL_DATA_TYPE_OCTET_STRING) ||
        (dataType == GMOS_ZIGBEE_ZCL_DATA_TYPE_CHAR_STRING)) {
        stringSize = dataArray [0];
        if (stringSize == 0xFF) {
            stringSize = 0;
        }
        parsedRecordSize = headerSize + stringSize + 2;
        if (octetArray == NULL) {
            return GMOS_ZIGBEE_ZCL_STATUS_INVALID_DATA_TYPE;
        }
        if (stringSize > octetArraySize) {
            return GMOS_ZIGBEE_ZCL_STATUS_INVALID_VALUE;
        }
        if (!gmosBufferRead (
            dataBuffer, dataItemOffset + 2, octetArray, stringSize)) {
            return GMOS_ZIGBEE_ZCL_STATUS_ABORT;
        }
        dataRecord->attrData.octetArray.dataLength = dataArray [0];
    }

    // Process fixed format values that fit into a 32-bit storage type.
    // This includes sign extension if required.
    else if (dataSize <= 4) {
        uint32_t dataValue = 0;
        for (i = 0; i < dataSize; i++) {
            dataValue |= dataArray [i] << (8 * i);
        }
        if (typeCategory == GMOS_ZIGBEE_ZCL_DATA_TYPE_INTEGER_S8) {
            int32_t signedValue = (int32_t) dataValue;
            if ((dataSize < 4) &&
                ((signedValue & (1 << (8 * dataSize - 1))) != 0)) {
                signedValue |= (0xFFFFFFFFL << (8 * dataSize));
            }
            dataRecord->attrData.valueInt32S = signedValue;
        } else {
            dataRecord->attrData.valueInt32U = dataValue;
        }
        parsedRecordSize = headerSize + dataSize + 1;
    }

    // Process fixed format values that fit into a 64-bit storage type.
    else {
        uint64_t dataValue = 0;
        for (i = 0; i < dataSize; i++) {
            dataValue |= dataArray [i] << (8 * i);
        }
        if (typeCategory == GMOS_ZIGBEE_ZCL_DATA_TYPE_INTEGER_S8) {
            int64_t signedValue = (int64_t) dataValue;
            if ((dataSize < 8) &&
                ((signedValue & (1 << (8 * dataSize - 1))) != 0)) {
                signedValue |= (0xFFFFFFFFFFFFFFFFLL << (8 * dataSize));
            }
            dataRecord->attrData.valueInt64S = signedValue;
        } else {
            dataRecord->attrData.valueInt64U = dataValue;
        }
        parsedRecordSize = headerSize + dataSize + 1;
    }
    if (recordSize != NULL) {
        *recordSize = parsedRecordSize;
    }
    return GMOS_ZIGBEE_ZCL_STATUS_SUCCESS;
}

/*
 * Parses attribute data from a buffer, updating the locally stored
 * attribute value. The data is only parsed if the data type at the
 * specified data item offset matches the data type specified in the
 * attribute data structure.
 */
gmosZigbeeZclStatusCode_t gmosZigbeeZclParseAttrData (
    gmosZigbeeZclAttr_t* zclAttr, gmosBuffer_t* dataBuffer,
    uint16_t dataItemOffset, bool commitWrite)
{
    uint8_t dataArray [9];
    uint_fast8_t dataSize;
    uint_fast8_t attrOptions = zclAttr->attrOptions;
    uint_fast8_t typeCategory = zclAttr->attrType & 0xF8;
    uint_fast8_t i;

    // Read the fixed size attribute data or the variable size attribute
    // header.
    if ((attrOptions & GMOS_ZIGBEE_ZCL_ATTR_OPTION_OCTET_ARRAY) != 0) {
        dataSize = 1;
    } else {
        dataSize = zclAttr->attrOptions &
            GMOS_ZIGBEE_ZCL_ATTR_OPTION_FIXED_SIZE_MASK;
    }
    if ((dataSize > 8) || (!gmosBufferRead (
        dataBuffer, dataItemOffset, dataArray, dataSize + 1))) {
        return GMOS_ZIGBEE_ZCL_STATUS_ABORT;
    }

    // Extract the data type byte from the data buffer and check that
    // it is consistent with the attribute data type.
    if (dataArray [0] != zclAttr->attrType) {
        return GMOS_ZIGBEE_ZCL_STATUS_INVALID_DATA_TYPE;
    }

    // Update the attribute octet array if required.
    if ((attrOptions & GMOS_ZIGBEE_ZCL_ATTR_OPTION_OCTET_ARRAY) != 0) {
        uint_fast8_t arrayLength = dataArray [1];
        if ((arrayLength > 0x00) && (arrayLength < 0xFF)) {
            if (arrayLength > zclAttr->attrData.octetArray.maxDataLength) {
                return GMOS_ZIGBEE_ZCL_STATUS_INVALID_VALUE;
            }
            if (commitWrite) {
                if (!gmosBufferRead (dataBuffer, dataItemOffset + 2,
                    zclAttr->attrData.octetArray.dataPtr, arrayLength)) {
                    return GMOS_ZIGBEE_ZCL_STATUS_ABORT;
                }
            }
        }
        if (commitWrite) {
            zclAttr->attrData.octetArray.dataLength = arrayLength;
        }
    }

    // Process boolean values which have restricted range.
    else if (dataArray [0] == GMOS_ZIGBEE_ZCL_DATA_TYPE_BOOLEAN) {
        uint_fast8_t booleanValue = dataArray [1];
        if ((booleanValue > 0x01) && (booleanValue < 0xFF)) {
            return GMOS_ZIGBEE_ZCL_STATUS_INVALID_VALUE;
        } else if (commitWrite) {
            zclAttr->attrData.valueInt32U = booleanValue;
        }
    }

    // Process fixed format values that fit into a 32-bit storage type.
    // This includes sign extension if required.
    else if (commitWrite && (dataSize <= 4)) {
        uint32_t dataValue = 0;
        for (i = 1; i <= dataSize; i++) {
            dataValue |= dataArray [i] << (8 * (i - 1));
        }
        if (typeCategory == GMOS_ZIGBEE_ZCL_DATA_TYPE_INTEGER_S8) {
            int32_t signedValue = (int32_t) dataValue;
            if ((dataSize < 4) &&
                ((signedValue & (1 << (8 * dataSize - 1))) != 0)) {
                signedValue |= (0xFFFFFFFFL << (8 * dataSize));
            }
            zclAttr->attrData.valueInt32S = signedValue;
        } else {
            zclAttr->attrData.valueInt32U = dataValue;
        }
    }

    // Process fixed format values that fit into a 64-bit storage type.
    else if (commitWrite) {
        uint64_t dataValue = 0;
        for (i = 1; i <= dataSize; i++) {
            dataValue |= dataArray [i] << (8 * (i - 1));
        }
        if (typeCategory == GMOS_ZIGBEE_ZCL_DATA_TYPE_INTEGER_S8) {
            int64_t signedValue = (int64_t) dataValue;
            if ((dataSize < 8) &&
                ((signedValue & (1 << (8 * dataSize - 1))) != 0)) {
                signedValue |= (0xFFFFFFFFFFFFFFFFLL << (8 * dataSize));
            }
            zclAttr->attrData.valueInt64S = signedValue;
        } else {
            zclAttr->attrData.valueInt64U = dataValue;
        }
    }
    return GMOS_ZIGBEE_ZCL_STATUS_SUCCESS;
}

/*
 * Serialize the attribute data, appending it to the provided data
 * buffer.
 */
bool gmosZigbeeZclSerializeAttrData (
    gmosZigbeeZclAttr_t* zclAttr, gmosBuffer_t* dataBuffer)
{
    uint8_t dataArray [9];
    uint8_t* dataPtr = dataArray;
    uint_fast8_t dataSize = zclAttr->attrOptions &
        GMOS_ZIGBEE_ZCL_ATTR_OPTION_FIXED_SIZE_MASK;
    uint_fast8_t attrOptions = zclAttr->attrOptions;
    uint_fast8_t arrayLength = 0xFF;
    uint_fast8_t i;

    // Always include the attribute data type.
    *(dataPtr++) = zclAttr->attrType;

    // Add the octet array length field if required, selecting the
    // 'invalid data' representation if the length is not valid.
    if ((attrOptions & GMOS_ZIGBEE_ZCL_ATTR_OPTION_OCTET_ARRAY) != 0) {
        arrayLength = zclAttr->attrData.octetArray.dataLength;
        if (arrayLength > zclAttr->attrData.octetArray.maxDataLength) {
            arrayLength = 0xFF;
        }
        *(dataPtr++) = arrayLength;
        dataSize = 1;
    }

    // Process fixed format values that fit into a 32-bit storage type.
    else if (dataSize <= 4) {
        uint32_t dataValue = zclAttr->attrData.valueInt32U;
        for (i = 0; i < dataSize; i++) {
            *(dataPtr++) = (uint8_t) dataValue;
            dataValue >>= 8;
        }
    }

    // Process fixed format values that fit into a 64-bit storage type.
    else if (dataSize <= 8) {
        uint64_t dataValue = zclAttr->attrData.valueInt64U;
        for (i = 0; i < dataSize; i++) {
            *(dataPtr++) = (uint8_t) dataValue;
            dataValue >>= 8;
        }
    } else {
        return false;
    }

    // Append the fixed format data or octet array header.
    if (!gmosBufferAppend (dataBuffer, dataArray, dataSize + 1)) {
        return false;
    }

    // Append the octet array data if required.
    if ((attrOptions & GMOS_ZIGBEE_ZCL_ATTR_OPTION_OCTET_ARRAY) != 0) {
        if ((arrayLength != 0) && (arrayLength != 0xFF)) {
            if (!gmosBufferAppend (dataBuffer,
                zclAttr->attrData.octetArray.dataPtr, arrayLength)) {
                return false;
            }
        }
    }
    return true;
}

/*
 * Serializes an attribute data record, appending it to the provided
 * data buffer.
 */
bool gmosZigbeeZclSerializeDataRecord (
    gmosZigbeeZclDataRecord_t* zclDataRecord, gmosBuffer_t* dataBuffer)
{
    uint8_t dataArray [11];
    uint8_t* dataPtr = dataArray;
    uint_fast8_t dataType = zclDataRecord->attrType;
    uint_fast8_t dataSize;
    uint_fast8_t arrayLength;
    uint_fast8_t i;
    bool isOctetArray;

    // Always include the attribute ID and data type.
    *(dataPtr++) = (uint8_t) zclDataRecord->attrId;
    *(dataPtr++) = (uint8_t) (zclDataRecord->attrId >> 8);
    *(dataPtr++) = dataType;

    // Determine whether the data record corresponds to an octet based
    // data array.
    if ((dataType == GMOS_ZIGBEE_ZCL_DATA_TYPE_OCTET_STRING) ||
        (dataType == GMOS_ZIGBEE_ZCL_DATA_TYPE_CHAR_STRING)) {
        isOctetArray = true;
        dataSize = 1;
    } else {
        isOctetArray = false;
        dataSize = getZclDataSize (dataType);
    }

    // Add the octet array length field if required.
    if (isOctetArray) {
        arrayLength = zclDataRecord->attrData.octetArray.dataLength;
        *(dataPtr++) = arrayLength;
    }

    // Process fixed format values that fit into a 32-bit storage type.
    else if (dataSize <= 4) {
        uint32_t dataValue = zclDataRecord->attrData.valueInt32U;
        for (i = 0; i < dataSize; i++) {
            *(dataPtr++) = (uint8_t) dataValue;
            dataValue >>= 8;
        }
    }

    // Process fixed format values that fit into a 64-bit storage type.
    else if (dataSize <= 8) {
        uint64_t dataValue = zclDataRecord->attrData.valueInt64U;
        for (i = 0; i < dataSize; i++) {
            *(dataPtr++) = (uint8_t) dataValue;
            dataValue >>= 8;
        }
    } else {
        return false;
    }

    // Append the fixed format data or octet array header.
    if (!gmosBufferAppend (dataBuffer, dataArray, dataSize + 3)) {
        return false;
    }

    // Append the octet array data if required.
    if (isOctetArray) {
        if ((arrayLength != 0) && (arrayLength != 0xFF)) {
            if (!gmosBufferAppend (dataBuffer,
                zclDataRecord->attrData.octetArray.dataPtr, arrayLength)) {
                return false;
            }
        }
    }
    return true;
}

/*
 * Formats a ZCL frame header, prepending the result to the specified
 * data buffer.
 */
bool gmosZigbeeZclPrependHeader (gmosZigbeeZclCluster_t* zclCluster,
    gmosBuffer_t* dataBuffer, gmosZigbeeZclFrameHeader_t* frameHeader)
{
    uint8_t  header [5];
    uint8_t* headerPtr = header;
    uint_fast16_t headerSize;

    // Set the ZCL frame direction flag.
    if ((zclCluster->baseCluster.clusterOptions &
        GMOS_ZIGBEE_CLUSTER_OPTION_OUTPUT) == 0) {
        frameHeader->frameControl |=
            GMOS_ZIGBEE_ZCL_FRAME_CONTROL_SOURCE_IS_SERVER;
    } else {
        frameHeader->frameControl &=
            ~GMOS_ZIGBEE_ZCL_FRAME_CONTROL_SOURCE_IS_SERVER;
    }

    // Format frame control code with no manufacturer ID.
    if (frameHeader->vendorId == GMOS_ZIGBEE_ZCL_STANDARD_VENDOR_ID) {
        frameHeader->frameControl &=
            ~GMOS_ZIGBEE_ZCL_FRAME_CONTROL_VENDOR_SPECIFIC;
        *(headerPtr++) = frameHeader->frameControl;
        headerSize = 3;
    }

    // Format frame control with manufacturer ID.
    else {
        frameHeader->frameControl |=
            GMOS_ZIGBEE_ZCL_FRAME_CONTROL_VENDOR_SPECIFIC;
        *(headerPtr++) = frameHeader->frameControl;
        *(headerPtr++) = (uint8_t) frameHeader->vendorId;
        *(headerPtr++) = (uint8_t) (frameHeader->vendorId >> 8);
        headerSize = 5;
    }

    // Set the response transaction sequence number and frame ID.
    *(headerPtr++) = frameHeader->zclSequence;
    *(headerPtr++) = frameHeader->zclFrameId;

    // Copy the formatted response to the response buffer.
    return gmosBufferPrepend (dataBuffer, header, headerSize);
}

/*
 * Determines the maximum size of ZCL messages that can be transmitted
 * for a given ZCL cluster. This refers to the full size of the ZCL
 * message, including the ZCL frame header. The standard implementation
 * does not support fragmentation and uses the APS maximum payload size
 * specified for the Zigbee interface driver.
 */
uint16_t gmosZigbeeZclMaxMessageSize (
    gmosZigbeeZclCluster_t* zclCluster)
{
    gmosZigbeeStack_t* zigbeeStack =
        zclCluster->baseCluster.hostEndpoint->zigbeeStack;
    return zigbeeStack->apsMaxMessageSize;
}
