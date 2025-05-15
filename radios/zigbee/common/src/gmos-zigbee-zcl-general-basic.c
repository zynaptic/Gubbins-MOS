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
 * This file implements the common API for working with the Zigbee
 * Cluster Library (ZCL) general purpose basic cluster.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-buffers.h"
#include "gmos-driver-eeprom.h"
#include "gmos-zigbee-zcl-core.h"
#include "gmos-zigbee-zcl-core-local.h"
#include "gmos-zigbee-zcl-general-basic.h"

// Allocate memory for vendor name string constant.
#ifdef GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_VENDOR_NAME
static const uint8_t vendorName [] =
    GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_VENDOR_NAME;
#endif

// Allocate memory for product name string constant.
#ifdef GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_PRODUCT_NAME
static const uint8_t productName [] =
    GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_PRODUCT_NAME;
#endif

/*
 * Forward EEPROM write completion callbacks to the ZCL attribute
 * access completion callback.
 */
static void eepromWriteCompletionHandler (
    gmosDriverEepromStatus_t status, void* callbackData)
{
    gmosZigbeeZclEndpoint_t* zclEndpoint =
        (gmosZigbeeZclEndpoint_t*) callbackData;
    uint_fast8_t zclStatus;

    // Map EEPROM status responses to ZCL status responses.
    if (status == GMOS_DRIVER_EEPROM_STATUS_SUCCESS) {
        zclStatus = GMOS_ZIGBEE_ZCL_STATUS_SUCCESS;
    } else {
        zclStatus = GMOS_ZIGBEE_ZCL_STATUS_FAILURE;
    }

    // Issue ZCL attribute access callback.
    gmosZigbeeZclLocalAttrAccessComplete (zclEndpoint, zclStatus);
}

/*
 * Implement common attribute value setter for the general purpose basic
 * cluster.
 */
static void setGeneralBasicAttr (gmosZigbeeZclEndpoint_t* zclEndpoint,
    gmosZigbeeZclAttr_t* zclAttr, gmosBuffer_t* dataBuffer,
    uint16_t dataItemOffset, bool commitWrite)
{
    gmosZigbeeZclCluster_t* zclCluster = zclEndpoint->local->cluster;
    gmosZigbeeZclGeneralBasicServer_t* zclServer =
        (gmosZigbeeZclGeneralBasicServer_t*) zclCluster;
    gmosDriverEeprom_t* eeprom = gmosDriverEepromGetInstance ();
    gmosDriverEepromStatus_t eepromStatus;
    uint8_t dataBytes [2];
    uint_fast8_t zclStatus;

    // Read the header bytes from the data buffer.
    if (!gmosBufferRead (dataBuffer, dataItemOffset, dataBytes, 2)) {
        zclStatus = GMOS_ZIGBEE_ZCL_STATUS_MALFORMED_COMMAND;
        goto complete;
    }
    if (dataBytes [0] != zclAttr->attrType) {
        zclStatus = GMOS_ZIGBEE_ZCL_STATUS_INVALID_DATA_TYPE;
        goto complete;
    }

    // Read back the current EEPROM data if required.
    if (commitWrite) {
        eepromStatus = gmosDriverEepromRecordRead (eeprom,
            zclCluster->eepromTag, (uint8_t*) &(zclServer->eepromData),
            0, sizeof (gmosZigbeeZclGeneralBasicEepromData_t));
        if (eepromStatus != GMOS_DRIVER_EEPROM_STATUS_SUCCESS) {
            zclStatus = GMOS_ZIGBEE_ZCL_STATUS_FAILURE;
            goto complete;
        }
    }

    // Update the location description attribute.
#if GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_EN_LOCATION_DESCR
    if (zclAttr->attrId ==
        GMOS_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_ID_LOCATION_DESCR) {
        uint_fast8_t length = dataBytes [1];
        if (length == 0xFF) {
            length = 0;
        } else if (length > 16) {
            zclStatus = GMOS_ZIGBEE_ZCL_STATUS_INVALID_VALUE;
            goto complete;
        }

        // Read the string data into the EEPROM write data buffer.
        zclServer->eepromData.locationDescr [0] = length;
        if (length > 0) {
            if (!gmosBufferRead (dataBuffer, dataItemOffset + 2,
                &(zclServer->eepromData.locationDescr [1]), length)) {
                zclStatus = GMOS_ZIGBEE_ZCL_STATUS_MALFORMED_COMMAND;
                goto complete;
            }
        }
    }
#endif

    // Update the physical environment attribute.
#if GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_EN_PHYSICAL_ENVIRON
    if (zclAttr->attrId ==
        GMOS_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_ID_PHYSICAL_ENVIRON) {
        zclServer->eepromData.physicalEnviron = dataBytes [1];
    }
#endif

    // Update the device enabled attribute.
#if GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_EN_DEVICE_ENABLED
    if (zclAttr->attrId ==
        GMOS_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_ID_DEVICE_ENABLED) {
        if (dataBytes [1] > 1) {
            zclStatus = GMOS_ZIGBEE_ZCL_STATUS_INVALID_VALUE;
            goto complete;
        }
        zclServer->eepromData.deviceEnabled = dataBytes [1];
    }
#endif

    // Update the alarm mask attribute.
#if GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_EN_ALARM_MASK
    if (zclAttr->attrId ==
        GMOS_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_ID_ALARM_MASK) {
        if (dataBytes [1] > 0x03) {
            zclStatus = GMOS_ZIGBEE_ZCL_STATUS_INVALID_VALUE;
            goto complete;
        }
        zclServer->eepromData.alarmMask = dataBytes [1];
    }
#endif

    // Update the local configuration disable attribute.
#if GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_EN_DISABLE_CONFIG
    if (zclAttr->attrId ==
        GMOS_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_ID_DISABLE_CONFIG) {
        if (dataBytes [1] > 0x03) {
            zclStatus = GMOS_ZIGBEE_ZCL_STATUS_INVALID_VALUE;
            goto complete;
        }
        zclServer->eepromData.configDisable = dataBytes [1];
    }
#endif

    // Skip further processing if the write is not being committed.
    if (!commitWrite) {
        zclStatus = GMOS_ZIGBEE_ZCL_STATUS_SUCCESS;
        goto complete;
    }

    // Initiate a long-running EEPROM write operation. Note that EEPROM
    // retry status responses are not currently supported. Anything
    // other than a successful status response is mapped to a ZCL write
    // transaction failure.
    eepromStatus = gmosDriverEepromRecordWrite (eeprom,
        zclCluster->eepromTag, (uint8_t*) &(zclServer->eepromData),
        sizeof (gmosZigbeeZclGeneralBasicEepromData_t),
        eepromWriteCompletionHandler, zclEndpoint);
    if (eepromStatus == GMOS_DRIVER_EEPROM_STATUS_SUCCESS) {
        return;
    } else {
        zclStatus = GMOS_ZIGBEE_ZCL_STATUS_FAILURE;
    }

    // Issue the completion callback on failure.
complete :
    gmosZigbeeZclLocalAttrAccessComplete (zclEndpoint, zclStatus);
}

/*
 * Implement common attribute value getter for the general purpose basic
 * cluster.
 */
static void getGeneralBasicAttr (gmosZigbeeZclEndpoint_t* zclEndpoint,
    gmosZigbeeZclAttr_t* zclAttr, gmosBuffer_t* dataBuffer)
{
    gmosZigbeeZclCluster_t* zclCluster = zclEndpoint->local->cluster;
    gmosZigbeeZclGeneralBasicServer_t* zclServer =
        (gmosZigbeeZclGeneralBasicServer_t*) zclCluster;
    gmosDriverEeprom_t* eeprom = gmosDriverEepromGetInstance ();
    gmosDriverEepromStatus_t eepromStatus;
    uint8_t dataBytes [2];
    uint8_t zclStatus = GMOS_ZIGBEE_ZCL_STATUS_SUCCESS;

    // Read back the current EEPROM data.
    eepromStatus = gmosDriverEepromRecordRead (eeprom,
        zclCluster->eepromTag, (uint8_t*) &(zclServer->eepromData),
        0, sizeof (gmosZigbeeZclGeneralBasicEepromData_t));
    if (eepromStatus != GMOS_DRIVER_EEPROM_STATUS_SUCCESS) {
        zclStatus = GMOS_ZIGBEE_ZCL_STATUS_FAILURE;
        goto complete;
    }

    // Include the leading data type field.
    dataBytes [0] = zclAttr->attrType;

    // Read the location description attribute length.
#if GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_EN_LOCATION_DESCR
    if (zclAttr->attrId ==
        GMOS_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_ID_LOCATION_DESCR) {
        dataBytes [1] = zclServer->eepromData.locationDescr [0];
    }
#endif

    // Read the physical environment attribute.
#if GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_EN_PHYSICAL_ENVIRON
    if (zclAttr->attrId ==
        GMOS_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_ID_PHYSICAL_ENVIRON) {
        dataBytes [1] = zclServer->eepromData.physicalEnviron;
    }
#endif

    // Read the device enabled attribute.
#if GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_EN_DEVICE_ENABLED
    if (zclAttr->attrId ==
        GMOS_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_ID_DEVICE_ENABLED) {
        dataBytes [1] = zclServer->eepromData.deviceEnabled;
    }
#endif

    // Read the alarm mask attribute.
#if GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_EN_ALARM_MASK
    if (zclAttr->attrId ==
        GMOS_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_ID_ALARM_MASK) {
        dataBytes [1] = zclServer->eepromData.alarmMask;
    }
#endif

    // Read the local configuration disable attribute.
#if GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_EN_DISABLE_CONFIG
    if (zclAttr->attrId ==
        GMOS_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_ID_DISABLE_CONFIG) {
        dataBytes [1] = zclServer->eepromData.configDisable;
    }
#endif

    // Append the common bytes to the response buffer.
    if (!gmosBufferAppend (dataBuffer, dataBytes, 2)) {
        zclStatus = GMOS_ZIGBEE_ZCL_STATUS_NULL;
        goto complete;
    }

    // Append the location description string to the response buffer.
#if GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_EN_LOCATION_DESCR
    if (zclAttr->attrId ==
        GMOS_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_ID_LOCATION_DESCR) {
        if (!gmosBufferAppend (dataBuffer,
            &(zclServer->eepromData.locationDescr [1]),
            zclServer->eepromData.locationDescr [0])) {
            zclStatus = GMOS_ZIGBEE_ZCL_STATUS_NULL;
            goto complete;
        }
    }
#endif

    // Issue the completion callback.
complete :
    gmosZigbeeZclLocalAttrAccessComplete (zclEndpoint, zclStatus);
}

/*
 * Perform a one-time initialisation for a ZCL general purpose basic
 * cluster server.
 */
bool gmosZigbeeZclGeneralBasicServerInit (
    gmosZigbeeZclGeneralBasicServer_t* zclServer,
    uint32_t eepromTag)
{
    gmosZigbeeZclCluster_t* zclCluster = &(zclServer->zclCluster);
    gmosZigbeeZclAttr_t* zclAttr;

    // Set the EEPROM default data values
    gmosZigbeeZclGeneralBasicEepromData_t eepromDefaultData = {
#if GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_EN_LOCATION_DESCR
        .locationDescr = { 0x00 },
#endif
#if GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_EN_PHYSICAL_ENVIRON
        .physicalEnviron = 0x00,
#endif
#if GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_EN_DEVICE_ENABLED
        .deviceEnabled = 0x01,
#endif
#if GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_EN_ALARM_MASK
        .alarmMask = 0x00,
#endif
#if GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_EN_DISABLE_CONFIG
        .configDisable = 0x00,
#endif
    };

    // Initialise the ZCL cluster instance.
    if (!gmosZigbeeZclClusterInit (&(zclServer->zclCluster),
        GMOS_ZIGBEE_ZCL_GENERAL_BASIC_CLUSTER_ID, true, eepromTag,
        (uint8_t*) &eepromDefaultData, sizeof (eepromDefaultData))) {
        return false;
    }

    // Initialise a mandatory attribute for the ZCL version number.
    zclAttr = &(zclServer->zclAttrZclVersion);
    if (!gmosZigbeeZclAttrInit (zclAttr,
        GMOS_ZIGBEE_ZCL_STANDARD_VENDOR_ID,
        GMOS_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_ID_ZCL_VERSION,
        GMOS_ZIGBEE_ZCL_DATA_TYPE_INTEGER_U8, false)) {
        return false;
    }
    if (!gmosZigbeeZclClusterAddAttr (zclCluster, zclAttr)) {
        return false;
    }
    zclAttr->attrData.valueInt32U = GMOS_ZIGBEE_ZCL_STANDARD_VERSION;

    // Initialise a mandatory attribute for the device power source.
    zclAttr = &(zclServer->zclAttrPowerSource);
    if (!gmosZigbeeZclAttrInit (zclAttr,
        GMOS_ZIGBEE_ZCL_STANDARD_VENDOR_ID,
        GMOS_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_ID_POWER_SOURCE,
        GMOS_ZIGBEE_ZCL_DATA_TYPE_ENUM_X8, false)) {
        return false;
    }
    if (!gmosZigbeeZclClusterAddAttr (zclCluster, zclAttr)) {
        return false;
    }
    zclAttr->attrData.valueInt32U =
        GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_POWER_SOURCE;

    // Initialise an optional attribute for the application version.
#ifdef GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_APP_VERSION
    zclAttr = &(zclServer->zclAttrAppVersion);
    if (!gmosZigbeeZclAttrInit (zclAttr,
        GMOS_ZIGBEE_ZCL_STANDARD_VENDOR_ID,
        GMOS_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_ID_APP_VERSION,
        GMOS_ZIGBEE_ZCL_DATA_TYPE_INTEGER_U8, false)) {
        return false;
    }
    if (!gmosZigbeeZclClusterAddAttr (zclCluster, zclAttr)) {
        return false;
    }
    zclAttr->attrData.valueInt32U =
        GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_APP_VERSION;
#endif

    // Initialise an optional attribute for the stack version.
#ifdef GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_STACK_VERSION
    zclAttr = &(zclServer->zclAttrStackVersion);
    if (!gmosZigbeeZclAttrInit (zclAttr,
        GMOS_ZIGBEE_ZCL_STANDARD_VENDOR_ID,
        GMOS_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_ID_STACK_VERSION,
        GMOS_ZIGBEE_ZCL_DATA_TYPE_INTEGER_U8, false)) {
        return false;
    }
    if (!gmosZigbeeZclClusterAddAttr (zclCluster, zclAttr)) {
        return false;
    }
    zclAttr->attrData.valueInt32U =
        GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_STACK_VERSION;
#endif

    // Initialise an optional attribute for the hardware version.
#ifdef GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_HARDWARE_VERSION
    zclAttr = &(zclServer->zclAttrHardwareVersion);
    if (!gmosZigbeeZclAttrInit (zclAttr,
        GMOS_ZIGBEE_ZCL_STANDARD_VENDOR_ID,
        GMOS_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_ID_HARDWARE_VERSION,
        GMOS_ZIGBEE_ZCL_DATA_TYPE_INTEGER_U8, false)) {
        return false;
    }
    if (!gmosZigbeeZclClusterAddAttr (zclCluster, zclAttr)) {
        return false;
    }
    zclAttr->attrData.valueInt32U =
        GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_HARDWARE_VERSION;
#endif

    // Initialise an optional attribute for the vendor name.
#ifdef GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_VENDOR_NAME
    zclAttr = &(zclServer->zclAttrVendorName);
    if (!gmosZigbeeZclAttrInitString (zclAttr,
        GMOS_ZIGBEE_ZCL_STANDARD_VENDOR_ID,
        GMOS_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_ID_VENDOR_NAME,
        GMOS_ZIGBEE_ZCL_DATA_TYPE_CHAR_STRING, false,
        (uint8_t*) vendorName, sizeof (vendorName) - 1,
        sizeof (vendorName) - 1)) {
        return false;
    }
    if (!gmosZigbeeZclClusterAddAttr (zclCluster, zclAttr)) {
        return false;
    }
#endif

    // Initialise an optional attribute for the product name.
#ifdef GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_PRODUCT_NAME
    zclAttr = &(zclServer->zclAttrProductName);
    if (!gmosZigbeeZclAttrInitString (zclAttr,
        GMOS_ZIGBEE_ZCL_STANDARD_VENDOR_ID,
        GMOS_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_ID_PRODUCT_NAME,
        GMOS_ZIGBEE_ZCL_DATA_TYPE_CHAR_STRING, false,
        (uint8_t*) productName, sizeof (productName) - 1,
        sizeof (productName) - 1)) {
        return false;
    }
    if (!gmosZigbeeZclClusterAddAttr (zclCluster, zclAttr)) {
        return false;
    }
#endif

    // Initialise attribute state for the optional local description.
#if GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_EN_LOCATION_DESCR
    zclAttr = &(zclServer->zclAttrLocationDescr);
    if (!gmosZigbeeZclAttrInitDynamic (zclAttr,
        GMOS_ZIGBEE_ZCL_STANDARD_VENDOR_ID,
        GMOS_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_ID_LOCATION_DESCR,
        GMOS_ZIGBEE_ZCL_DATA_TYPE_CHAR_STRING, setGeneralBasicAttr,
        getGeneralBasicAttr)) {
        return false;
    }
    if (!gmosZigbeeZclClusterAddAttr (zclCluster, zclAttr)) {
        return false;
    }
#endif

    // Initialise attribute state for the optional physical environment.
#if GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_EN_PHYSICAL_ENVIRON
    zclAttr = &(zclServer->zclAttrPhysicalEnviron);
    if (!gmosZigbeeZclAttrInitDynamic (zclAttr,
        GMOS_ZIGBEE_ZCL_STANDARD_VENDOR_ID,
        GMOS_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_ID_PHYSICAL_ENVIRON,
        GMOS_ZIGBEE_ZCL_DATA_TYPE_ENUM_X8, setGeneralBasicAttr,
        getGeneralBasicAttr)) {
        return false;
    }
    if (!gmosZigbeeZclClusterAddAttr (zclCluster, zclAttr)) {
        return false;
    }
#endif

    // Initialise attribute state for the optional device enabled flag.
#if GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_EN_DEVICE_ENABLED
    zclAttr = &(zclServer->zclAttrDeviceEnabled);
    if (!gmosZigbeeZclAttrInitDynamic (zclAttr,
        GMOS_ZIGBEE_ZCL_STANDARD_VENDOR_ID,
        GMOS_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_ID_DEVICE_ENABLED,
        GMOS_ZIGBEE_ZCL_DATA_TYPE_BOOLEAN, setGeneralBasicAttr,
        getGeneralBasicAttr)) {
        return false;
    }
    if (!gmosZigbeeZclClusterAddAttr (zclCluster, zclAttr)) {
        return false;
    }
#endif

    // Initialise attribute state for the optional alarm mask.
#if GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_EN_ALARM_MASK
    zclAttr = &(zclServer->zclAttrAlarmMask);
    if (!gmosZigbeeZclAttrInitDynamic (zclAttr,
        GMOS_ZIGBEE_ZCL_STANDARD_VENDOR_ID,
        GMOS_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_ID_ALARM_MASK,
        GMOS_ZIGBEE_ZCL_DATA_TYPE_BITMAP_X8, setGeneralBasicAttr,
        getGeneralBasicAttr)) {
        return false;
    }
    if (!gmosZigbeeZclClusterAddAttr (zclCluster, zclAttr)) {
        return false;
    }
#endif

    // Initialise attribute state for the optional local device
    // configuration disable flags.
#if GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_EN_DISABLE_CONFIG
    zclAttr = &(zclServer->zclAttrAlarmMask);
    if (!gmosZigbeeZclAttrInitDynamic (zclAttr,
        GMOS_ZIGBEE_ZCL_STANDARD_VENDOR_ID,
        GMOS_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_ID_DISABLE_CONFIG,
        GMOS_ZIGBEE_ZCL_DATA_TYPE_BITMAP_X8, setGeneralBasicAttr,
        getGeneralBasicAttr)) {
        return false;
    }
    if (!gmosZigbeeZclClusterAddAttr (zclCluster, zclAttr)) {
        return false;
    }
#endif
    return true;
}
