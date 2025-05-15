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
 * This header defines the common API for working with the Zigbee
 * Cluster Library (ZCL) general purpose basic cluster.
 */

#ifndef GMOS_ZIGBEE_ZCL_GENERAL_BASIC_H
#define GMOS_ZIGBEE_ZCL_GENERAL_BASIC_H

#include <stdint.h>
#include <stdbool.h>
#include "gmos-config.h"
#include "gmos-zigbee-zcl-core.h"

/**
 * Specify the standard ZCL basic cluster ID.
 */
#define GMOS_ZIGBEE_ZCL_GENERAL_BASIC_CLUSTER_ID 0x0000

/**
 * Specify the optional application version number. By default this is
 * not included in the attribute set.
 */
#ifndef GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_APP_VERSION
// Undefined by default.
#endif

/**
 * Specify the optional Zigbee stack version number. By default this is
 * not included in the attribute set.
 */
#ifndef GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_STACK_VERSION
// Undefined by default.
#endif

/**
 * Specify the optional device hardware version number. By default this
 * is not included in the attribute set.
 */
#ifndef GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_HARDWARE_VERSION
// Undefined by default.
#endif

/**
 * Specify the optional device manufacturer or vendor name. By default
 * this is not included in the attribute set.
 */
#ifndef GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_VENDOR_NAME
// Undefined by default.
#endif

/**
 * Specify the optional device model or product name. By default this is
 * not included in the attribute set.
 */
#ifndef GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_PRODUCT_NAME
// Undefined by default.
#endif

/**
 * Define the default power source attribute value if not otherwise
 * configured.
 */
#ifndef GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_POWER_SOURCE
#define GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_POWER_SOURCE \
    GMOS_ZIGBEE_ZCL_GENERAL_BASIC_POWER_SOURCE_UNKNOWN
#endif

/**
 * Specify whether the optional location description attribute value is
 * supported.
 */
#ifndef GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_EN_LOCATION_DESCR
#define GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_EN_LOCATION_DESCR false
#endif

/**
 * Specify whether the optional physical environment attribute value is
 * supported.
 */
#ifndef GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_EN_PHYSICAL_ENVIRON
#define GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_EN_PHYSICAL_ENVIRON false
#endif

/**
 * Specify whether the optional device enabled attribute is supported.
 */
#ifndef GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_EN_DEVICE_ENABLED
#define GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_EN_DEVICE_ENABLED false
#endif

/**
 * Specify whether the optional alarm mask attribute is supported.
 */
#ifndef GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_EN_ALARM_MASK
#define GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_EN_ALARM_MASK false
#endif

/**
 * Specify whether the optional disable local configuration attribute is
 * supported.
 */
#ifndef GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_EN_DISABLE_CONFIG
#define GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_EN_DISABLE_CONFIG false
#endif

/**
 * This enumeration specifies the list of supported general purpose
 * basic cluster attribute IDs.
 */
typedef enum {
    GMOS_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_ID_ZCL_VERSION      = 0x0000,
    GMOS_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_ID_APP_VERSION      = 0x0001,
    GMOS_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_ID_STACK_VERSION    = 0x0002,
    GMOS_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_ID_HARDWARE_VERSION = 0x0003,
    GMOS_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_ID_VENDOR_NAME      = 0x0004,
    GMOS_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_ID_PRODUCT_NAME     = 0x0005,
    GMOS_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_ID_DATE_CODE        = 0x0006,
    GMOS_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_ID_POWER_SOURCE     = 0x0007,
    GMOS_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_ID_LOCATION_DESCR   = 0x0010,
    GMOS_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_ID_PHYSICAL_ENVIRON = 0x0011,
    GMOS_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_ID_DEVICE_ENABLED   = 0x0012,
    GMOS_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_ID_ALARM_MASK       = 0x0013,
    GMOS_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_ID_DISABLE_CONFIG   = 0x0014
} gmosZigbeeZclGeneralBasicAttrIds_t;

/**
 * Ths enumeration specifies the supported power supply sources for the
 * device.
 */
typedef enum {
    GMOS_ZIGBEE_ZCL_GENERAL_BASIC_POWER_SOURCE_UNKNOWN             = 0x00,
    GMOS_ZIGBEE_ZCL_GENERAL_BASIC_POWER_SOURCE_MAINS_SINGLE_PHASE  = 0x01,
    GMOS_ZIGBEE_ZCL_GENERAL_BASIC_POWER_SOURCE_MAINS_THREE_PHASE   = 0x02,
    GMOS_ZIGBEE_ZCL_GENERAL_BASIC_POWER_SOURCE_BATTERY             = 0x03,
    GMOS_ZIGBEE_ZCL_GENERAL_BASIC_POWER_SOURCE_EXTERNAL_DC         = 0x04,
    GMOS_ZIGBEE_ZCL_GENERAL_BASIC_POWER_SOURCE_EMERGENCY_ALWAYS_ON = 0x05,
    GMOS_ZIGBEE_ZCL_GENERAL_BASIC_POWER_SOURCE_EMERGENCY_SWITCHED  = 0x06,
    GMOS_ZIGBEE_ZCL_GENERAL_BASIC_POWER_SOURCE_BATTERY_BACKUP_FLAG = 0x80
} gmosZigbeeZclGeneralBasicPowerSources_t;

/**
 * This structure defines the data elements that will be persisted in
 * EEPROM memory. It must not change between firmware versions unless
 * a factory reset is forced at the end of the firmware upgrade process.
 */
typedef struct gmosZigbeeZclGeneralBasicEepromData_t {

    // Allocate persistent memory for the optional location description.
    // This is a ZCL formatted character string consisting of a length
    // byte followed by the string contents.
#if GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_EN_LOCATION_DESCR
    uint8_t locationDescr [17];
#endif

    // Allocate persistent memory for the optional physical environment.
#if GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_EN_PHYSICAL_ENVIRON
    uint8_t physicalEnviron;
#endif

    // Allocate persistent memory for the optional device enabled flag.
#if GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_EN_DEVICE_ENABLED
    uint8_t deviceEnabled;
#endif

    // Allocate persistent memory for the optional alarm mask.
#if GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_EN_ALARM_MASK
    uint8_t alarmMask;
#endif

    // Allocate persistent memory for the optional config disable flags.
#if GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_EN_DISABLE_CONFIG
    uint8_t configDisable;
#endif

} gmosZigbeeZclGeneralBasicEepromData_t;

/**
 * This structure encapsulates the configuration and state information
 * for a single general purpose basic cluster server.
 */
typedef struct gmosZigbeeZclGeneralBasicServer_t {

    // Allocate memory for ZCL cluster instance that can be cast
    // to the enclosing server data structure as required.
    gmosZigbeeZclCluster_t zclCluster;

    // Allocate attribute data for mandatory standard attributes.
    gmosZigbeeZclAttr_t zclAttrZclVersion;
    gmosZigbeeZclAttr_t zclAttrPowerSource;

    // Allocate attribute data for optional application version.
#ifdef GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_APP_VERSION
    gmosZigbeeZclAttr_t zclAttrAppVersion;
#endif

    // Allocate attribute data for optional stack version number.
#ifdef GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_STACK_VERSION
    gmosZigbeeZclAttr_t zclAttrStackVersion;
#endif

    // Allocate attribute data for optional hardware version number.
#ifdef GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_HARDWARE_VERSION
    gmosZigbeeZclAttr_t zclAttrHardwareVersion;
#endif

    // Allocate attribute data for optional vendor name.
#ifdef GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_VENDOR_NAME
    gmosZigbeeZclAttr_t zclAttrVendorName;
#endif

    // Allocate attribute data for optional product name.
#ifdef GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_PRODUCT_NAME
    gmosZigbeeZclAttr_t zclAttrProductName;
#endif

    // Allocate attribute data for optional production date code.
    // Note that this requires dynamic access to determine the
    // manufacturing date, which needs to be added to the core GMOS
    // support. Therefore this attribute is not currently supported.
    // gmosZigbeeZclAttr_t zclAttrDateCode;

    // Allocate attribute data for the optional location description.
#if GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_EN_LOCATION_DESCR
    gmosZigbeeZclAttr_t zclAttrLocationDescr;
#endif

    // Allocate attribute data for the optional physical environment.
#if GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_EN_PHYSICAL_ENVIRON
    gmosZigbeeZclAttr_t zclAttrPhysicalEnviron;
#endif

    // Allocate attribute data for the optional device enabled flag.
#if GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_EN_DEVICE_ENABLED
    gmosZigbeeZclAttr_t zclAttrDeviceEnabled;
#endif

    // Allocate attribute data for the optional alarm mask.
#if GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_EN_ALARM_MASK
    gmosZigbeeZclAttr_t zclAttrAlarmMask;
#endif

    // Allocate attribute data for the optional config disable flags.
#if GMOS_CONFIG_ZIGBEE_ZCL_GENERAL_BASIC_ATTR_EN_DISABLE_CONFIG
    gmosZigbeeZclAttr_t zclAttrDisableConfig;
#endif

    // Allocate memory for EEPROM write data buffer.
    gmosZigbeeZclGeneralBasicEepromData_t eepromData;

} gmosZigbeeZclGeneralBasicServer_t;

/**
 * Perform one-time initialisation of a ZCL general purpose basic
 * cluster server.
 * @param zclServer This is the ZCL general purpose basic cluster server
 *     that is to be initialised.
 * @param eepromTag This is the cluster specific EEPROM tag to be used
 *     for persistent data storage, if required.
 * @return Returns a boolean value which will be set to 'true' on
 *     successful initialisation and 'false' otherwise.
 */
bool gmosZigbeeZclGeneralBasicServerInit (
    gmosZigbeeZclGeneralBasicServer_t* zclServer,
    uint32_t eepromTag);

#endif // GMOS_ZIGBEE_ZCL_GENERAL_BASIC_H
