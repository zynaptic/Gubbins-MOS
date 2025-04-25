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
 * This header provides common definitions for the Zigbee Cluster
 * Library (ZCL) foundation components.
 */

#ifndef GMOS_ZIGBEE_ZCL_CORE_H
#define GMOS_ZIGBEE_ZCL_CORE_H

#include <stdint.h>
#include <stdbool.h>
#include "gmos-buffers.h"
#include "gmos-driver-eeprom.h"
#include "gmos-zigbee-config.h"
#include "gmos-zigbee-stack.h"
#include "gmos-zigbee-endpoint.h"

/**
 * Specify the ZCL library version. This corresponds to the Zigbee
 * Alliance revision 8 release of document 075123 (December 2019).
 */
#define GMOS_ZIGBEE_ZCL_STANDARD_VERSION 8

/**
 * This manufacturer vendor ID is used to indicate standard ZCL
 * attributes and commands.
 */
#define GMOS_ZIGBEE_ZCL_STANDARD_VENDOR_ID 0xFFFF

/**
 * This enumeration specifies the various standard profile wide ZCL
 * frame identifiers.
 */
typedef enum {

    // The ZCL frame contains the read attributes request message.
    GMOS_ZIGBEE_ZCL_PROFILE_READ_ATTRS_REQUEST         = 0x00,

    // The ZCL frame contains the read attributes response message.
    GMOS_ZIGBEE_ZCL_PROFILE_READ_ATTRS_RESPONSE        = 0x01,

    // The ZCL frame contains the write attributes request message. This
    // updates any attributes that can be written.
    GMOS_ZIGBEE_ZCL_PROFILE_WRITE_ATTRS_REQUEST        = 0x02,

    // The ZCL frame contains the atomic write request message. This
    // only updates the attributes if they can all be written.
    GMOS_ZIGBEE_ZCL_PROFILE_WRITE_ATTRS_ATOMIC_REQUEST = 0x03,

    // The ZCL frame contains the write attributes response message.
    GMOS_ZIGBEE_ZCL_PROFILE_WRITE_ATTRS_RESPONSE       = 0x04,

    // The ZCL frame contains the silent write request message. This
    // updates the attributes without acknowledgement.
    GMOS_ZIGBEE_ZCL_PROFILE_WRITE_ATTRS_SILENT_REQUEST = 0x05,

    // The ZCL frame contains the reporting configuration write request
    // message.
    GMOS_ZIGBEE_ZCL_PROFILE_WRITE_REPORT_CFG_REQUEST   = 0x06,

    // The ZCL frame contains the reporting configuration write response
    // message.
    GMOS_ZIGBEE_ZCL_PROFILE_WRITE_REPORT_CFG_RESPONSE  = 0x07,

    // The ZCL frame contains the reporting configuration read request
    // message.
    GMOS_ZIGBEE_ZCL_PROFILE_READ_REPORT_CFG_REQUEST    = 0x08,

    // The ZCL frame contains the reporting configuration read response
    // message.
    GMOS_ZIGBEE_ZCL_PROFILE_READ_REPORT_CFG_RESPONSE   = 0x09,

    // The ZCL frame contains the attribute reporting message.
    GMOS_ZIGBEE_ZCL_PROFILE_REPORT_ATTRS               = 0x0A,

    // The ZCL frame contains the default response message.
    GMOS_ZIGBEE_ZCL_PROFILE_DEFAULT_RESPONSE           = 0x0B,

    // The ZCL frame contains the discover attributes request message.
    GMOS_ZIGBEE_ZCL_PROFILE_DISCOVER_ATTRS_REQUEST     = 0x0C,

    // The ZCL frame contains the discover attributes response message.
    GMOS_ZIGBEE_ZCL_PROFILE_DISCOVER_ATTRS_RESPONSE    = 0x0D

} gmosZigbeeZclProfileFrameId_t;

/**
 * This enumeration specifies the various ZCL status byte encodings.
 */
typedef enum {

    // The requested operation was completed successfully.
    GMOS_ZIGBEE_ZCL_STATUS_SUCCESS           = 0x00,

    // The requested operation was not completed successfully.
    GMOS_ZIGBEE_ZCL_STATUS_FAILURE           = 0x01,

    // The received ZCL command did not have the expected format.
    GMOS_ZIGBEE_ZCL_STATUS_MALFORMED_COMMAND = 0x80,

    // An unsupported ZCL command was received.
    GMOS_ZIGBEE_ZCL_STATUS_UNSUP_COMMAND     = 0x81,

    // A received ZCL message field did not have the expected format.
    GMOS_ZIGBEE_ZCL_STATUS_INVALID_FIELD     = 0x85,

    // An unsupported attribute was referenced in a command.
    GMOS_ZIGBEE_ZCL_STATUS_UNSUP_ATTRIBUTE   = 0x86,

    // An invalid attribute value was referenced in a command.
    GMOS_ZIGBEE_ZCL_STATUS_INVALID_VALUE     = 0x87,

    // A read only attribute was specified in a write command.
    GMOS_ZIGBEE_ZCL_STATUS_READ_ONLY         = 0x88,

    // An invalid attribute data type was specified in a command.
    GMOS_ZIGBEE_ZCL_STATUS_INVALID_DATA_TYPE = 0x8D,

    // The transaction was aborted due to excessive response time.
    GMOS_ZIGBEE_ZCL_STATUS_TIMEOUT           = 0x94,

    // This is a non-standard 'abort' status value. This is used to
    // indicate that further ZCL message processing cannot proceed.
    GMOS_ZIGBEE_ZCL_STATUS_ABORT             = 0xFE,

    // This is a non-standard 'null' status value. This is also used
    // to indicate memory allocation failure conditions.
    GMOS_ZIGBEE_ZCL_STATUS_NULL              = 0xFF

} gmosZigbeeZclStatusCode_t;

/**
 * This enumeration specifies the bit encodings and flags used to format
 * the ZCL frame control byte.
 */
typedef enum {

    // Specify the mask used to extract the frame type field from the
    // frame control byte.
    GMOS_ZIGBEE_ZCL_FRAME_CONTROL_TYPE_MASK        = 0x03,

    // Specify the encoding used to select the ZCL profile wide command
    // set.
    GMOS_ZIGBEE_ZCL_FRAME_CONTROL_TYPE_GENERAL     = 0x00,

    // Specify the encoding used to select the ZCL cluster specific
    // command set.
    GMOS_ZIGBEE_ZCL_FRAME_CONTROL_TYPE_CLUSTER     = 0x01,

    // Specify the vendor specific flag in the frame control byte.
    GMOS_ZIGBEE_ZCL_FRAME_CONTROL_VENDOR_SPECIFIC  = 0x04,

    // Specify the direction flag in the frame control byte.
    GMOS_ZIGBEE_ZCL_FRAME_CONTROL_SOURCE_IS_SERVER = 0x08,

    // Specify the default response disable flag.
    GMOS_ZIGBEE_ZCL_FRAME_CONTROL_NO_DEFAULT_RESP  = 0x10

} gmosZigbeeZclFrameControlFlags_t;

/**
 * This enumeration specifies the various data types supported by the
 * ZCL framework.
 */
typedef enum {

    // Null (empty) data type.
    GMOS_ZIGBEE_ZCL_DATA_TYPE_NO_DATA           = 0x00,
    GMOS_ZIGBEE_ZCL_DATA_TYPE_UNKNOWN           = 0xFF,

    // General purpose data types of various sizes (8 to 64 bits).
    GMOS_ZIGBEE_ZCL_DATA_TYPE_GENERAL_X8        = 0x08,
    GMOS_ZIGBEE_ZCL_DATA_TYPE_GENERAL_X16       = 0x09,
    GMOS_ZIGBEE_ZCL_DATA_TYPE_GENERAL_X24       = 0x0A,
    GMOS_ZIGBEE_ZCL_DATA_TYPE_GENERAL_X32       = 0x0B,
    GMOS_ZIGBEE_ZCL_DATA_TYPE_GENERAL_X40       = 0x0C,
    GMOS_ZIGBEE_ZCL_DATA_TYPE_GENERAL_X48       = 0x0D,
    GMOS_ZIGBEE_ZCL_DATA_TYPE_GENERAL_X56       = 0x0E,
    GMOS_ZIGBEE_ZCL_DATA_TYPE_GENERAL_X64       = 0x0F,

    // Boolean data type encoded as an 8 bit integer (false=0, true=1).
    GMOS_ZIGBEE_ZCL_DATA_TYPE_BOOLEAN           = 0x10,

    // Bitmap data types of various sizes (8 to 64 bits).
    GMOS_ZIGBEE_ZCL_DATA_TYPE_BITMAP_X8         = 0x18,
    GMOS_ZIGBEE_ZCL_DATA_TYPE_BITMAP_X16        = 0x19,
    GMOS_ZIGBEE_ZCL_DATA_TYPE_BITMAP_X24        = 0x1A,
    GMOS_ZIGBEE_ZCL_DATA_TYPE_BITMAP_X32        = 0x1B,
    GMOS_ZIGBEE_ZCL_DATA_TYPE_BITMAP_X40        = 0x1C,
    GMOS_ZIGBEE_ZCL_DATA_TYPE_BITMAP_X48        = 0x1D,
    GMOS_ZIGBEE_ZCL_DATA_TYPE_BITMAP_X56        = 0x1E,
    GMOS_ZIGBEE_ZCL_DATA_TYPE_BITMAP_X64        = 0x1F,

    // Unsigned integer types of various sizes (8 to 64 bits).
    GMOS_ZIGBEE_ZCL_DATA_TYPE_INTEGER_U8        = 0x20,
    GMOS_ZIGBEE_ZCL_DATA_TYPE_INTEGER_U16       = 0x21,
    GMOS_ZIGBEE_ZCL_DATA_TYPE_INTEGER_U24       = 0x22,
    GMOS_ZIGBEE_ZCL_DATA_TYPE_INTEGER_U32       = 0x23,
    GMOS_ZIGBEE_ZCL_DATA_TYPE_INTEGER_U40       = 0x24,
    GMOS_ZIGBEE_ZCL_DATA_TYPE_INTEGER_U48       = 0x25,
    GMOS_ZIGBEE_ZCL_DATA_TYPE_INTEGER_U56       = 0x26,
    GMOS_ZIGBEE_ZCL_DATA_TYPE_INTEGER_U64       = 0x27,

    // Signed integer types of various sizes (8 to 64 bits).
    GMOS_ZIGBEE_ZCL_DATA_TYPE_INTEGER_S8        = 0x28,
    GMOS_ZIGBEE_ZCL_DATA_TYPE_INTEGER_S16       = 0x29,
    GMOS_ZIGBEE_ZCL_DATA_TYPE_INTEGER_S24       = 0x2A,
    GMOS_ZIGBEE_ZCL_DATA_TYPE_INTEGER_S32       = 0x2B,
    GMOS_ZIGBEE_ZCL_DATA_TYPE_INTEGER_S40       = 0x2C,
    GMOS_ZIGBEE_ZCL_DATA_TYPE_INTEGER_S48       = 0x2D,
    GMOS_ZIGBEE_ZCL_DATA_TYPE_INTEGER_S56       = 0x2E,
    GMOS_ZIGBEE_ZCL_DATA_TYPE_INTEGER_S64       = 0x2F,

    // Enumerated types of various sizes (8 to 16 bits).
    GMOS_ZIGBEE_ZCL_DATA_TYPE_ENUM_X8           = 0x30,
    GMOS_ZIGBEE_ZCL_DATA_TYPE_ENUM_X16          = 0x31,

    // Floating point types using 32 and 64 bit formats. The 16-bit
    // format is not a C standard type and is not currently implemented.
#if GMOS_CONFIG_ZIGBEE_ZCL_FLOATING_POINT_SUPPORT
    GMOS_ZIGBEE_ZCL_DATA_TYPE_FLOAT_F32         = 0x39,
    GMOS_ZIGBEE_ZCL_DATA_TYPE_FLOAT_F64         = 0x3A,
#endif

    // String data types stored locally as octet arrays.
    // Long string types are not supported.
    GMOS_ZIGBEE_ZCL_DATA_TYPE_OCTET_STRING      = 0x41,
    GMOS_ZIGBEE_ZCL_DATA_TYPE_CHAR_STRING       = 0x42,

    // Composite data types.
#if GMOS_CONFIG_ZIGBEE_ZCL_COMPOSITE_ATTRIBUTE_SUPPORT
    GMOS_ZIGBEE_ZCL_DATA_TYPE_COMPOSITE_ARRAY   = 0x48,
    GMOS_ZIGBEE_ZCL_DATA_TYPE_COMPOSITE_STRUCT  = 0x4C,
    GMOS_ZIGBEE_ZCL_DATA_TYPE_COMPOSITE_SET     = 0x50,
    GMOS_ZIGBEE_ZCL_DATA_TYPE_COMPOSITE_BAG     = 0x51,
#endif

    // Date and time representations.
    GMOS_ZIGBEE_ZCL_DATA_TYPE_TIME_OF_DAY       = 0xE0,
    GMOS_ZIGBEE_ZCL_DATA_TYPE_CALENDAR_DATE     = 0xE1,
    GMOS_ZIGBEE_ZCL_DATA_TYPE_UTC_TIME          = 0xE2,

    // Network parameter data types.
    GMOS_ZIGBEE_ZCL_DATA_TYPE_CLUSTER_ID        = 0xE8,
    GMOS_ZIGBEE_ZCL_DATA_TYPE_ATTRIBUTE_ID      = 0xE9,
    GMOS_ZIGBEE_ZCL_DATA_TYPE_BACNET_OID        = 0xEA,
    GMOS_ZIGBEE_ZCL_DATA_TYPE_IEEE_MAC_ADDR     = 0xF0,
    GMOS_ZIGBEE_ZCL_DATA_TYPE_SECURITY_KEY      = 0xF1

} gmosZigbeeZclDataTypes_t;

/**
 * This enumeration specifies the ZCL attribute option flag values.
 */
typedef enum {

    // Specifies the ZCL attribute fixed size mask.
    GMOS_ZIGBEE_ZCL_ATTR_OPTION_FIXED_SIZE_MASK = 0x0F,

    // Specifies that the ZCL attribute data is stored in a variable
    // length octet array.
    GMOS_ZIGBEE_ZCL_ATTR_OPTION_OCTET_ARRAY     = 0x10,

    // Specifies that the ZCL attribute acts as a report producer.
    GMOS_ZIGBEE_ZCL_ATTR_OPTION_REPORT_PRODUCER = 0x20,

    // Specifies that the ZCL attribute has remote write access.
    GMOS_ZIGBEE_ZCL_ATTR_OPTION_REMOTE_WRITE_EN = 0x40,

    // Specifies that the ZCL attribute data should be dynamically
    // accessed via getter and setter functions.
    GMOS_ZIGBEE_ZCL_ATTR_OPTION_DYNAMIC_ACCESS  = 0x80

} gmosZigbeeZclAttrOptions_t;

// Provide required data type forward references.
typedef struct gmosZigbeeZclEndpoint_t gmosZigbeeZclEndpoint_t;
typedef struct gmosZigbeeZclCluster_t gmosZigbeeZclCluster_t;
typedef struct gmosZigbeeZclAttr_t gmosZigbeeZclAttr_t;
typedef struct gmosZigbeeZclEndpointLocal_t gmosZigbeeZclEndpointLocal_t;
typedef struct gmosZigbeeZclEndpointRemote_t gmosZigbeeZclEndpointRemote_t;

/**
 * Define the function prototype for implementing attribute setter
 * functions. These are called during attribute write operations, and
 * may run asynchronously, since completion is indicated by making a
 * call to the attribute access completion callback.
 * @param zclEndpoint This is the ZCL endpoint for which the attribute
 *     write request has been submitted.
 * @param zclAttr This is the ZCL attribute for which the attribute
 *     write request has been submitted.
 * @param dataBuffer This is the data buffer that contains the new
 *     attribute value to be written.
 * @param dataItemOffset This is the offset within the data buffer at
 *     which the serialized ZCL data item is located. This consists of
 *     a data type byte followed by a variable number of data value
 *     octets.
 * @param commitWrite This is a boolean flag which when set to 'true'
 *     causes the attribute value to be updated. If this is set to
 *     'false', the write operation will be checked for validity, but
 *     the attribute value will not be updated. This mode is used during
 *     atomic writes to check that all specified attributes can be
 *     updated before carrying out the update.
 */
typedef void (*gmosZigbeeZclAttrSetter_t) (
    gmosZigbeeZclEndpoint_t* zclEndpoint, gmosZigbeeZclAttr_t* zclAttr,
    gmosBuffer_t* dataBuffer, uint16_t dataItemOffset, bool commitWrite);

/**
 * Define the function prototype for implementing attribute getter
 * functions. These are called during attribute read operations, and
 * may run asynchronously, since completion is indicated by making a
 * call to the attribute access completion callback.
 * @param zclEndpoint This is the ZCL endpoint for which the attribute
 *     read request has been submitted.
 * @param zclAttr This is the ZCL attribute for which the attribute
 *     read request has been submitted.
 * @param dataBuffer This is the data buffer to which the attribute data
 *     type and current attribute value will be appended.
 */
typedef void (*gmosZigbeeZclAttrGetter_t) (
    gmosZigbeeZclEndpoint_t* zclEndpoint, gmosZigbeeZclAttr_t* zclAttr,
    gmosBuffer_t* dataBuffer);

/**
 * Define the function prototype for implementing attribute notifier
 * functions. These are called by attribute reporting consumers, and
 * may run asynchronously, since completion is indicated by making a
 * call to the attribute access completion callback.
 */
typedef void (*gmosZigbeeZclAttrNotifier_t) (
    gmosZigbeeZclEndpoint_t* zclEndpoint, gmosZigbeeZclAttr_t* zclAttr,
    gmosBuffer_t* dataBuffer, uint16_t dataOffset);

/**
 * This data structure encapsulates the information required to support
 * value reporting for a single attribute, where the cluster functions
 * as a producer of attribute data. This will normally apply to ZCL
 * server clusters.
 */
typedef struct gmosZigbeeZclReportProducer_t {

    // This specifies the offset within the cluster EEPROM record at
    // which the attribute reporting parameters are stored. The EEPROM
    // will hold the current minimum and maximum reporting intervals
    // and optional reportable change field in the same format as the
    // standard ZCL attribute reporting configuration record.
    uint8_t eepromOffset;

} gmosZigbeeZclReportProducer_t;

/**
 * This data structure encapsulates the information required to support
 * value reporting for a single attribute, where the cluster functions
 * as a consumer of attribute data. This will normally apply to ZCL
 * client clusters.
 */
typedef struct gmosZigbeeZclReportConsumer_t {

    // This specifies the offset within the cluster EEPROM record at
    // which the attribute reporting parameters are stored. The EEPROM
    // will hold the timeout field in the same format as the standard
    // ZCL attribute reporting configuration record.
    uint8_t eepromOffset;

    // This is a pointer to the attribute notifier function that will
    // be called when an attribute value is reported to the report
    // consumer.
    gmosZigbeeZclAttrNotifier_t attrNotifier;

} gmosZigbeeZclReportConsumer_t;

/**
 * This data structure provides a common encapsulation for a Zigbee
 * Cluster Library (ZCL) cluster attribute.
 */
typedef struct gmosZigbeeZclAttr_t {

    // A pointer to the next attribute in the attribute list.
    struct gmosZigbeeZclAttr_t* nextAttr;

    // Link to the appropriate attribute reporting structure, or a null
    // reference if attribute reporting is not supported.
    union {
        gmosZigbeeZclReportProducer_t* producer;
        gmosZigbeeZclReportConsumer_t* consumer;
    } report;

    // This union specifies storage for basic data types up to 64 bits
    // in length, the pointer and length of octet arrays and function
    // pointers for dynamic attribute value access.
    union {

        // Define standard integer types.
        int32_t  valueInt32S; // Signed 32-bit integers.
        int64_t  valueInt64S; // Signed 64-bit integers.
        uint32_t valueInt32U; // Unsigned 32-bit integers.
        uint64_t valueInt64U; // Unsigned 64-bit integers.

        // Define floating point types if supported. The 16-bit format
        // is not a C standard type and is not currently implemented.
#if GMOS_CONFIG_ZIGBEE_ZCL_FLOATING_POINT_SUPPORT
        float  valueFloat32;
        double valueFloat64;
#endif

        // Define pointer and length fields for octet arrays.
        struct {
            uint8_t* dataPtr;
            uint8_t dataLength;
            uint8_t maxDataLength;
        } octetArray;

        // Define getter and setter function pointers for dynamnic
        // attribute value access.
        struct {
            gmosZigbeeZclAttrSetter_t setter;
            gmosZigbeeZclAttrGetter_t getter;
        } dynamic;

        // Define pointer and entry count fields for composite data.
 #if GMOS_CONFIG_ZIGBEE_ZCL_COMPOSITE_ATTRIBUTE_SUPPORT
        struct {
            struct gmosZigbeeZclAttr_t* attrList;
            uint8_t attrListLength;
        } composite;
#endif
    } attrData;

    // Specify the vendor ID that may be used to support manufacturer
    // specific cluster extensions. It should be set to the default
    // value for standard ZCL cluster attributes.
    uint16_t vendorId;

    // This is the 16-bit ZCL attribute ID.
    uint16_t attrId;

    // This is the 8-bit ZCL data type encoding.
    uint8_t attrType;

    // This is the set of attribute option flags.
    uint8_t attrOptions;

} gmosZigbeeZclAttr_t;

/**
 * This data structure provides a common encapsulation for ZCL attribute
 * data records. It is used when constructing remote attribute write
 * request messages and parsing remote attribute read responses.
 */
typedef struct gmosZigbeeZclDataRecord_t {

    // This union specifies storage for basic data types up to 64 bits
    // in length, or the pointer and length of octet arrays.
    union {

        // Define standard integer types.
        int32_t  valueInt32S; // Signed 32-bit integers.
        int64_t  valueInt64S; // Signed 64-bit integers.
        uint32_t valueInt32U; // Unsigned 32-bit integers.
        uint64_t valueInt64U; // Unsigned 64-bit integers.

        // Define floating point types if supported. The 16-bit format
        // is not a C standard type and is not currently implemented.
#if GMOS_CONFIG_ZIGBEE_ZCL_FLOATING_POINT_SUPPORT
        float  valueFloat32;
        double valueFloat64;
#endif

        // Define pointer and length fields for octet arrays.
        struct {
            uint8_t* dataPtr;
            uint8_t dataLength;
            uint8_t maxDataLength;
        } octetArray;

        // Define pointer and entry count fields for composite data.
 #if GMOS_CONFIG_ZIGBEE_ZCL_COMPOSITE_ATTRIBUTE_SUPPORT
        struct {
            struct gmosZigbeeZclAttr_t* attrList;
            uint8_t attrListLength;
        } composite;
#endif
    } attrData;

    // This is the 16-bit ZCL attribute ID.
    uint16_t attrId;

    // This is the 8-bit ZCL data type encoding.
    uint8_t attrType;

    // This is the optional attribute status.
    uint8_t attrStatus;

} gmosZigbeeZclDataRecord_t;

/**
 * This data structure provides a common encapsulation for a Zigbee
 * Cluster Library (ZCL) cluster.
 */
typedef struct gmosZigbeeZclCluster_t {

    // Wrap the associated Zigbee cluster data structure.
    gmosZigbeeCluster_t baseCluster;

    // Define the list of supported attributes for the ZCL cluster.
    gmosZigbeeZclAttr_t* attrList;

    // Specify the EEPROM record tag to be used for persistent data
    // storage. This will be located in the main system information
    // EEPROM.
    gmosDriverEepromTag_t eepromTag;

} gmosZigbeeZclCluster_t;

/**
 * This data structure provides a common encapsulation for a Zigbee
 * Cluster Library (ZCL) endpoint instance.
 */
typedef struct gmosZigbeeZclEndpoint_t {

    // Wrap the associated Zigbee endpoint data structure.
    gmosZigbeeEndpoint_t baseEndpoint;

    // Specify the endpoint state variables used for processing local
    // ZCL attribute command requests. This will be a null reference if
    // the endpoint does not support local command processing.
    gmosZigbeeZclEndpointLocal_t* local;

    // Specify the endpoint state variables used for processing remote
    // ZCL attribute command requests. This will be a null reference if
    // the endpoint does not support remote command processing.
    gmosZigbeeZclEndpointRemote_t* remote;

} gmosZigbeeZclEndpoint_t;

/**
 * This data structure provides a common encapsulation for the ZCL
 * header fields.
 */
typedef struct gmosZigbeeZclFrameHeader_t {

    // Specify the manufacturer vendor ID, or the standard vendor ID.
    uint16_t vendorId;

    // Specify the standard ZCL frame control fields.
    uint8_t frameControl;

    // Specify the ZCL sequence number for the ZCL transaction.
    uint8_t zclSequence;

    // Specify the frame ID for the ZCL transaction.
    uint8_t zclFrameId;

} gmosZigbeeZclFrameHeader_t;

/**
 * Performs a one-time initialisation of a ZCL endpoint data structure.
 * This should be called during initialisation to set up the ZCL
 * endpoint for subsequent use.
 * @param zclEndpoint This is the ZCL endpoint structure that is to be
 *     initialised.
 * @param zclLocal This is the ZCL endpoint local state data structure.
 *     It is only required for ZCL endpoints where local attribute
 *     command processing is required. In most cases this will be ZCL
 *     endpoints that host ZCL server clusters. A null reference may be
 *     used if local command processing is not required.
 * @param zclRemote This is the ZCL endpoint remote state data
 *     structure. It is only required for endpoints where remote
 *     attribute command processing is required. In most cases this will
 *     be ZCL endpoints that host ZCL client clusters. A null reference
 *     may be used if remote command processing is not required.
 * @param endpointId This is the Zigbee endpoint identifier for the
 *     endpoint data structure. It must be in the valid range from 1 to
 *     240 inclusive.
 * @param appProfileId This specifies the Zigbee application profile
 *     which is associated with the endpoint.
 * @param appDeviceId This specifies the Zigbee application profile
 *     device which is implemented by the endpoint.
 */
void gmosZigbeeZclEndpointInit (
    gmosZigbeeZclEndpoint_t* zclEndpoint,
    gmosZigbeeZclEndpointLocal_t* zclLocal,
    gmosZigbeeZclEndpointRemote_t* zclRemote,
    uint8_t endpointId, uint16_t appProfileId, uint16_t appDeviceId);

/**
 * Performs a one-time initialisation of a ZCL cluster data structure.
 * This should be called during initialisation to set up the ZCL
 * cluster for subsequent use.
 * @param zclCluster This is the ZCL cluster data structure that is to
 *     be initialised.
 * @param clusterId This is the Zigbee cluster identifier for the
 *     cluster data structure.
 * @param isServer This is a boolean value which will be set to 'true'
 *     if the ZCL cluster is a server (input) cluster and 'false' if it
 *     is a client (output) cluster.
 * @param eepromTag This is the EEPROM tag that is used to identify the
 *     EEPROM record used for storing persistent cluster data.
 * @param eepromDefaultData This is a pointer to the default EEPROM data
 *     settings that should be used on a factory reset. If set to a null
 *     reference, the entire contents of the EEPROM record will be
 *     set to zero.
 * @param eepromLength This is the length of the EEPROM record used for
 *     storing persistent cluster data. A value of zero may be used to
 *     indicate that no EEPROM storage is required.
 * @return Returns a boolean value which will be set to 'true' on
 *     successful initialisation and 'false' otherwise.
 */
bool gmosZigbeeZclClusterInit (gmosZigbeeZclCluster_t* zclCluster,
    uint16_t clusterId, bool isServer, gmosDriverEepromTag_t eepromTag,
    uint8_t* eepromDefaultData, uint16_t eepromLength);

/**
 * Performs a one-time initialisation of a ZCL cluster attribute data
 * structure that uses fixed size data types. This should be called
 * during initialisation to set up the ZCL cluster attribute for
 * subsequent use. Attributes are initialised with the appropriate
 * 'non-value' data.
 * @param zclAttr This is the ZCL cluster attribute data structure that
 *     is to be initialised.
 * @param vendorId  This is the vendor ID that may be used to support
 *     manufacturer specific cluster extensions. It should be set to the
 *     default vendor ID for standard ZCL attributes.
 * @param attrId This is the attribute identifier to be used.
 * @param attrType This is the attribute data type to be used.
 * @param remoteWriteEn This is a boolean value which should be set to
 *     'true' if the attribute can be written by a remote node and
 *     'false' otherwise.
 * @return Returns a boolean value which will be set to 'true' on
 *     successful initialisation and 'false' otherwise.
 */
bool gmosZigbeeZclAttrInit (gmosZigbeeZclAttr_t* zclAttr,
    uint16_t vendorId, uint16_t attrId, uint8_t attrType,
    bool remoteWriteEn);

/**
 * Performs a one-time initialisation of a ZCL cluster attribute data
 * structure that uses string data types stored in an octet array.
 * This should be called during initialisation to set up the ZCL cluster
 * attribute for subsequent use.
 * @param zclAttr This is the ZCL cluster attribute data structure that
 *     is to be initialised.
 * @param vendorId  This is the vendor ID that may be used to support
 *     manufacturer specific cluster extensions. It should be set to the
 *     default vendor ID for standard ZCL attributes.
 * @param attrId This is the attribute identifier to be used.
 * @param attrType This is the attribute data type to be used.
 * @param remoteWriteEn This is a boolean value which should be set to
 *     'true' if the attribute can be written by a remote node and
 *     'false' otherwise.
 * @param attrData This is a pointer to the byte array that is used to
 *     store the string data item.
 * @param attrDataSize This is the size of the byte array that is used
 *     to store the string data item.
 * @param initDataSize This is the initial data size that specifies the
 *     size of the default data stored in the attribute data array. A
 *     value of 0xFF may be used to indicate that the data is not
 *     initialised.
 * @return Returns a boolean value which will be set to 'true' on
 *     successful initialisation and 'false' otherwise.
 */
bool gmosZigbeeZclAttrInitString (gmosZigbeeZclAttr_t* zclAttr,
    uint16_t vendorId, uint16_t attrId, uint8_t attrType,
    bool remoteWriteEn, uint8_t* attrData, uint8_t attrDataSize,
    uint8_t initDataSize);

/**
 * Performs a one-time initialisation of a ZCL cluster attribute data
 * structure with dynamic data access support. This should be called
 * during initialisation to set up the ZCL cluster attribute for
 * subsequent use.
 * @param zclAttr This is the ZCL cluster attribute data structure that
 *     is to be initialised.
 * @param vendorId  This is the vendor ID that may be used to support
 *     manufacturer specific cluster extensions. It should be set to the
 *     default vendor ID for standard ZCL attributes.
 * @param attrId This is the attribute identifier to be used.
 * @param attrType This is the attribute data type to be used.
 * @param attrSetter This is a pointer to the attribute setter function,
 *     or a null reference if the attribute is read only.
 * @param attrGetter This is a pointer to the attribute getter function.
 * @return Returns a boolean value which will be set to 'true' on
 *     successful initialisation and 'false' otherwise.
 */
bool gmosZigbeeZclAttrInitDynamic (gmosZigbeeZclAttr_t* zclAttr,
    uint16_t vendorId, uint16_t attrId, uint8_t attrType,
    gmosZigbeeZclAttrSetter_t attrSetter,
    gmosZigbeeZclAttrGetter_t attrGetter);

/**
 * Attaches an attribute report producer to an attribute and configures
 * it. This should be called during initialisation to set up the ZCL
 * cluster attribute reporting for subsequent use.
 * @param zclAttr This is the ZCL cluster attribute data structure that
 *     is to be initialised.
 * @param producerData This is a pointer to the attribute report
 *     producer data structure that is to be attached to the attribute.
 * @param eepromOffset This is the offset within the cluster EEPROM
 *     record at which the persisted report producer data is located.
 */
void gmosZigbeeZclAttrSetProducer (gmosZigbeeZclAttr_t* zclAttr,
    gmosZigbeeZclReportProducer_t* producerData, uint8_t eepromOffset);

/**
 * Attaches an attribute report consumer to an attribute and configures
 * it. This should be called during initialisation to set up the ZCL
 * cluster attribute reporting for subsequent use.
 * @param zclAttr This is the ZCL cluster attribute data structure that
 *     is to be initialised.
 * @param consumerData This is a pointer to the attribute report
 *     consumer data structure that is to be attached to the attribute.
 * @param eepromOffset This is the offset within the cluster EEPROM
 *     record at which the persisted report consumer data is located.
 */
void gmosZigbeeZclAttrSetConsumer (gmosZigbeeZclAttr_t* zclAttr,
    gmosZigbeeZclReportConsumer_t* consumerData, uint8_t eepromOffset);

/**
 * Attaches a new ZCL attribute to a ZCL cluster instance.
 * @param zclCluster This is the ZCL cluster instance to which the
 *     attribute is to be attached.
 * @param zclAttr This is the ZCL attribute that is to be attached to
 *     the ZCL cluster instance.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully attaching the ZCL attribute and 'false' on failure.
 */
bool gmosZigbeeZclClusterAddAttr (gmosZigbeeZclCluster_t* zclCluster,
    gmosZigbeeZclAttr_t* zclAttr);

/**
 * Requests the ZCL attribute instance on a ZCL cluster, given the
 * attribute ID.
 * @param zclCluster This is the ZCL cluster to which the attribute is
 *     attached.
 * @param vendorId This is the manufacturer vendor ID that is associated
 *     with the attribute instance.
 * @param attrId This is the ZCL attribute identifier that is associated
 *     with the attribute instance.
 * @return Returns a pointer to the matching ZCL attribute instance or a
 *     null reference if no matching ZCL attribute instance is present
 *     on the endpoint.
 */
gmosZigbeeZclAttr_t* gmosZigbeeZclGetAttrInstance (
    gmosZigbeeZclCluster_t* zclCluster,
    uint16_t vendorId, uint16_t attrId);

/**
 * Determines the number of octets used to represent a ZCL serialized
 * data value.
 * @param dataBuffer This is the data buffer that contains the ZCL
 *     serialized data item.
 * @param dataItemOffset This is the offset within the data buffer at
 *     which the serialized ZCL data item is located. This consists of
 *     a data type byte followed by a variable number of data value
 *     octets.
 * @param dataSize This is a pointer to the data size variable that will
 *     be updated with the size of the data value. This does not include
 *     the leading data type byte.
 * @return Returns a ZCL status code which indicates successful
 *     completion or the reason for failure.
 */
gmosZigbeeZclStatusCode_t gmosZigbeeZclParseDataSize (
    gmosBuffer_t* dataBuffer, uint16_t dataItemOffset,
    uint8_t* dataSize);

/**
 * Parses a complete attribute data record from a buffer, as included
 * in read attribute response messages and attribute reporting messages.
 * @param dataBuffer This is the data buffer that contains the ZCL
 *     serialized data item.
 * @param recordOffset This is the offset within the data buffer at
 *     which the serialized ZCL attribute read record is located. This
 *     consists of an attribute identifier, followed by optional ZCL
 *     status value, data type and value fields.
 * @param checkStatus This is a boolean flag, which when set to 'true'
 *     checks the status field included in read attribute response
 *     records. If set to 'false', the status field is skipped, as
 *     required for attribute reporting records.
 * @param dataRecord This is a pointer to the data record structure that
 *     will be updated with the parsed data record parameters.
 * @param octetArray This is a pointer to an octet array that will be
 *     used to store variable length strings if required. A null
 *     reference may be used if the record is known to have a fixed
 *     format data type.
 * @param octetArraySize This is the length of the data buffer that may
 *     be used to store variable length strings.
 * @param recordSize This is a pointer to the record size variable that
 *     will be updated with the size of parsed data record. A null
 *     reference may be used if this information is not required.
 * @return Returns the ZCL status code which indicates successful
 *     completion, the status value included in the data record or the
 *     reason for failure.
 */
gmosZigbeeZclStatusCode_t gmosZigbeeZclParseDataRecord (
    gmosBuffer_t* dataBuffer, uint16_t recordOffset, bool checkStatus,
    gmosZigbeeZclDataRecord_t* dataRecord, uint8_t* octetArray,
    uint8_t octetArraySize, uint8_t* recordSize);

/**
 * Parses attribute data from a buffer, updating the locally stored
 * attribute value. The data is only parsed if the data type at the
 * specified data item offset matches the data type specified in the
 * attribute data structure.
 * @param zclAttr This is the ZCL cluster attribute which will be
 *     updated with the parsed data value.
 * @param dataBuffer This is the data buffer that contains the ZCL
 *     serialized data item.
 * @param dataItemOffset This is the offset within the data buffer at
 *     which the serialized ZCL data item is located. This consists of
 *     a data type byte followed by a variable number of data value
 *     octets.
 * @param commitWrite This is a boolean flag which when set to 'true'
 *     causes the attribute value to be updated. If this is set to
 *     'false', the write operation will be checked for validity, but
 *     the attribute value will not be updated. This mode is used during
 *     atomic writes to check that all specified attributes can be
 *     updated before carrying out the update.
 * @return Returns a ZCL status code which indicates successful
 *     completion or the reason for failure.
 */
gmosZigbeeZclStatusCode_t gmosZigbeeZclParseAttrData (
    gmosZigbeeZclAttr_t* zclAttr, gmosBuffer_t* dataBuffer,
    uint16_t dataItemOffset, bool commitWrite);

/**
 * Serializes the attribute data, appending it to the provided data
 * buffer. The serialized data consists of the data type field followed
 * by the appropriate number of octets for encoding the attribute data
 * value.
 * @param zclAttr This is the ZCL cluster attribute for which the
 *     data serialization is being requested.
 * @param dataBuffer This is the data buffer to which the serialized
 *     attribute data is to be appended.
 * @return Returns a boolean value which will be set to 'true' if the
 *     attribute data was appended to the data buffer and 'false'
 *     otherwise.
 */
bool gmosZigbeeZclSerializeAttrData (
    gmosZigbeeZclAttr_t* zclAttr, gmosBuffer_t* dataBuffer);

/**
 * Serializes an attribute data record, appending it to the provided
 * data buffer. The serialized data consists of the ZCL attribute ID
 * and the data type field followed by the appropriate number of octets
 * for encoding the attribute data value.
 * @param zclDataRecord This is the ZCL data record for which the data
 *     serialization is being requested.
 * @param dataBuffer This is the data buffer to which the serialized
 *     data record is to be appended.
 * @return Returns a boolean value which will be set to 'true' if the
 *     data record was appended to the data buffer and 'false'
 *     otherwise.
 */
bool gmosZigbeeZclSerializeDataRecord (
    gmosZigbeeZclDataRecord_t* zclDataRecord, gmosBuffer_t* dataBuffer);

/**
 * Formats a ZCL frame header, prepending the result to the specified
 * data buffer.
 * @param zclCluster This is the ZCL cluster which is the source of the
 *     new ZCL frame.
 * @param dataBuffer This is the data buffer to which the new ZCL frame
 *     header is to be prepended. It may be an empty buffer or may
 *     already include the frame payload.
 * @param frameHeader This is the frame header data structure which
 *     contains the various fields that are to be inserted into the
 *     ZCL header.
 * @return Returns a boolean value which will be set to 'true' if the
 *     ZCL header was successfully prepended to the data buffer and
 *    'false' otherwise.
 */
bool gmosZigbeeZclPrependHeader (gmosZigbeeZclCluster_t* zclCluster,
    gmosBuffer_t* dataBuffer, gmosZigbeeZclFrameHeader_t* frameHeader);

/**
 * Determines the maximum size of ZCL messages that can be transmitted
 * for a given ZCL cluster. This refers to the full size of the ZCL
 * message, including the ZCL frame header.
 * @param zclCluster This is the cluster for which the maximum ZCL
 *     message size is being accessed.
 * @return Returns the maximum message size that may be transmitted
 *     for the given ZCL cluster.
 */
uint16_t gmosZigbeeZclMaxMessageSize (
    gmosZigbeeZclCluster_t* zclCluster);

#endif // GMOS_ZIGBEE_ZCL_CORE_COMMON_H
