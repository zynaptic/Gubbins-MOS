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
 * This header defines the common API for integrating the Zigbee stack
 * into the GubbinsMOS runtime framework.
 */

#ifndef GMOS_ZIGBEE_STACK_H
#define GMOS_ZIGBEE_STACK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "gmos-zigbee-config.h"

// Specify some common Zigbee protocol constants.
#define GMOS_ZIGBEE_ENCRYPTION_KEY_SIZE          16
#define GMOS_ZIGBEE_MAC_ADDRESS_SIZE              8
#define GMOS_ZIGBEE_EXTENDED_PAN_ID_SIZE          8
#define GMOS_ZIGBEE_CHANNEL_MASK         0x07FFF800
#define GMOS_ZIGBEE_INVALID_PAN_ID           0xFFFF
#define GMOS_ZIGBEE_NULL_NODE_ID             0xFFFF

/**
 * Specify the set of Zigbee status codes supported by all GubbinsMOS
 * Zigbee implementations.
 */
typedef enum {

    // The Zigbee operation was successful.
    GMOS_ZIGBEE_STATUS_SUCCESS          = 0x00,

    // Indicates that a fatal error condition has been encountered.
    GMOS_ZIGBEE_STATUS_FATAL_ERROR      = 0x01,

    // Indicates that the Zigbee request contained an invalid parameter.
    GMOS_ZIGBEE_STATUS_INVALID_ARGUMENT = 0x02,

    // The Zigbee operation could not be completed at this time and
    // should be retried later.
    GMOS_ZIGBEE_STATUS_RETRY            = 0x03,

    // Specifies that no valid binding table entry could be found for
    // the specified binding table parameters.
    GMOS_ZIGBEE_STATUS_NO_VALID_BINDING = 0x6C,

    // Indicates that the Zigbee request was invalid, given the current
    // stack status.
    GMOS_ZIGBEE_STATUS_INVALID_CALL     = 0x70,

    // Indicates that a Zigbee message is too long to fit in a MAC
    // layer frame.
    GMOS_ZIGBEE_STATUS_MESSAGE_TOO_LONG = 0x74,

    // Indicates that a device is a member of a Zigbee network.
    GMOS_ZIGBEE_STATUS_NETWORK_UP       = 0x90,

    // Indicates that a device is not a member of a Zigbee network.
    GMOS_ZIGBEE_STATUS_NETWORK_DOWN     = 0x91

} gmosZigbeeStatus_t;

/**
 * Specify the set of Zigbee network operating states supported by all
 * GubbinsMOS Zigbee stack implementations.
 */
typedef enum {

    // The Zigbee network stack is currently being initialised.
    GMOS_ZIGBEE_NETWORK_STATE_INITIALISING,

    // The Zigbee network stack is not currently associated with a
    // Zigbee network.
    GMOS_ZIGBEE_NETWORK_STATE_DOWN,

    // The Zigbee network stack is currently in the process of forming a
    // new network.
    GMOS_ZIGBEE_NETWORK_STATE_FORMING,

    // The Zigbee network stack is currently in the process of joining
    // a new network.
    GMOS_ZIGBEE_NETWORK_STATE_JOINING,

    // The Zigbee network stack is currently in the process of leaving
    // a network.
    GMOS_ZIGBEE_NETWORK_STATE_LEAVING,

    // The Zigbee network stack is currently in the process of rejoining
    // an existing network.
    GMOS_ZIGBEE_NETWORK_STATE_REJOINING,

    // The Zigbee network stack is connected to a Zigbee network.
    GMOS_ZIGBEE_NETWORK_STATE_CONNECTED,

    // The Zigbee network stack is in a persistent fault condition and
    // can not be recovered without a system restart.
    GMOS_ZIGBEE_NETWORK_STATE_STACK_FAULT

} gmosZigbeeNetworkState_t;

/**
 * Specify the set of supported Zigbee network security modes.
 */
typedef enum {

    // The Zigbee network supports the use of a common link key.
    GMOS_ZIGBEE_SECURITY_MODE_COMMON_LINK_KEY,

    // The Zigbee network supports the use of hashed link keys.
    GMOS_ZIGBEE_SECURITY_MODE_HASHED_LINK_KEYS

} gmosZigbeeSecurityMode_t;

/**
 * Specify the set of supported Zigbee device joining modes.
 */
typedef enum {

    // Enable device joining using a link key that is provided by the
    // trust centre. This does not require out of band device
    // configuration but can allow a potential attacker to intercept
    // key information during the joining process.
    GMOS_ZIGBEE_JOINING_MODE_UNKNOWN_LINK_KEY,

    // Enable device joining using a preassigned link key which has
    // been provided to the joining device using an out of band
    // configuration process.
    GMOS_ZIGBEE_JOINING_MODE_PRESET_LINK_KEY,

    // Disallow device joining, but allow previously joined devices to
    // rejoin the network after losing their network connection.
    GMOS_ZIGBEE_JOINING_MODE_REJOINS_ONLY,

    // Disallow all device joining, including device rejoin attempts.
    GMOS_ZIGBEE_JOINING_MODE_DISALLOW_ALL

} gmosZigbeeJoiningMode_t;

/**
 * Defines the Zigbee radio specific I/O state data structure. The
 * full type definition must be provided by the associated radio
 * abstraction layer.
 */
typedef struct gmosZigbeeRalState_t gmosZigbeeRalState_t;

/**
 * Defines the Zigbee radio specific I/O configuration options. The
 * full type definition must be provided by the associated radio
 * abstraction layer.
 */
typedef struct gmosZigbeeRalConfig_t gmosZigbeeRalConfig_t;

/**
 * Defines the Zigbee cluster instance data type. The full type
 * definition is provided in the Zigbee endpoint header.
 */
typedef struct gmosZigbeeCluster_t gmosZigbeeCluster_t;

/**
 * Defines the Zigbee endpoint instance data type. The full type
 * definition is provided in the Zigbee endpoint header.
 */
typedef struct gmosZigbeeEndpoint_t gmosZigbeeEndpoint_t;

/**
 * Defines the Zigbee ZDO client data type. The full type definition
 * is provided in the Zigbee ZDO client header.
 */
typedef struct gmosZigbeeZdoClient_t gmosZigbeeZdoClient_t;

/**
 * Defines the GubbinsMOS Zigbee stack data structure that is used for
 * encapsulating all the Zigbee stack data.
 */
typedef struct gmosZigbeeStack_t {

    // This is an opaque pointer to the Zigbee radio abstraction layer
    // data structure that is used for accessing the Zigbee radio
    // hardware. The data structure will be radio device specific.
    gmosZigbeeRalState_t* ralData;

    // This is an opaque pointer to the Zigbee radio abstraction layer
    // configuration data structure that is used for setting up the
    // Zigbee radio hardware. The data structure will be radio device
    // specific.
    const gmosZigbeeRalConfig_t* ralConfig;

    // This is a pointer to the Zigbee application endpoint list.
    gmosZigbeeEndpoint_t* endpointList;

    // This is a pointer to the associated Zigbee ZDO client instance.
    gmosZigbeeZdoClient_t* zdoClient;

    // Store the callback handlers for APS transaction completion.
    void* apsTxMsgCallbacks
        [GMOS_CONFIG_ZIGBEE_APS_TRANSMIT_MAX_REQUESTS];

    // Store the callback handlers for ZDO device announcements.
    void* zdoDevAnnceCallbacks
        [GMOS_CONFIG_ZIGBEE_ZDO_SERVER_MAX_DEV_ANNCE_HANDLERS];

    // Store the callback data pointers for ZDO device announcements.
    void* zdoDevAnnceCallbackData
        [GMOS_CONFIG_ZIGBEE_ZDO_SERVER_MAX_DEV_ANNCE_HANDLERS];

    // This is the Zigbee network channel mask to be used during network
    // formation and joining.
    uint32_t channelMask;

    // This is the current setting for the network's short PAN ID.
    uint16_t currentPanId;

    // Specify the currently active local node ID.
    uint16_t currentNodeId;

    // This is the current Zigbee network operating state.
    uint8_t networkState;

    // This is the current Zigbee network channel ID.
    uint8_t currentChannelId;

    // Specify the Zigbee network extended PAN ID.
    uint8_t extendedPanId [GMOS_ZIGBEE_EXTENDED_PAN_ID_SIZE];

    // Store the current tags of active APS transactions.
    uint8_t apsTxMsgTags [GMOS_CONFIG_ZIGBEE_APS_TRANSMIT_MAX_REQUESTS];

    // Specify the maximum supported APS message size. This is
    // calculated on initialisation using the assigned network
    // configuration options.
    uint8_t apsMaxMessageSize;

} gmosZigbeeStack_t;

/**
 * Provides a radio hardware configuration setup macro to be used when
 * allocating Zigbee stack data structures. Assigning this macro to a
 * Zigbee stack data structure on declaration will set the radio
 * specific configuration. Refer to the radio specific Zigbee
 * implementation for full details of the configuration options.
 * @param _ralData_ This is the radio abstraction layer state data
 *     structure that is to be used for accessing the radio specific
 *     hardware.
 * @param _ralConfig_ This is a radio hardware specific configuration
 *     data structure that defines a set of fixed configuration options
 *     to be used with the Zigbee radio.
 */
#define GMOS_ZIGBEE_RAL_CONFIG(_ralData_, _ralConfig_) {               \
    .ralData                 = _ralData_,                              \
    .ralConfig               = _ralConfig_,                            \
    .endpointList            = NULL,                                   \
    .zdoClient               = NULL,                                   \
    .apsTxMsgCallbacks       = { NULL },                               \
    .zdoDevAnnceCallbacks    = { NULL },                               \
    .zdoDevAnnceCallbackData = { NULL },                               \
    .channelMask             = 0,                                      \
    .currentPanId            = 0,                                      \
    .currentNodeId           = 0xFFFF,                                 \
    .networkState            = GMOS_ZIGBEE_NETWORK_STATE_INITIALISING, \
    .currentChannelId        = 0,                                      \
    .extendedPanId           = { 0 },                                  \
    .apsTxMsgTags            = { 0 }}

/**
 * Initialises a Zigbee stack on startup.
 * @param zigbeeStack This is the Zigbee stack data structure that will
 *     be used for managing GubbinsMOS access to the Zigbee stack.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully completing the initialisation process and 'false'
 *     otherwise.
 */
bool gmosZigbeeStackInit (gmosZigbeeStack_t* zigbeeStack);

/**
 * Initialises a Zigbee radio abstraction on startup. This will be
 * called by the Zigbee initialisation function in order to set up the
 * radio abstraction layer prior to any further processing. The radio
 * specific configuration options should already have been populated
 * using the 'GMOS_ZIGBEE_RAL_CONFIG' macro.
 * @param zigbeeStack This is the Zigbee stack data structure that will
 *     be used for managing GubbinsMOS access to the Zigbee stack.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully completing the initialisation process and 'false'
 *     otherwise.
 */
bool gmosZigbeeRalInit (gmosZigbeeStack_t* zigbeeStack);

/**
 * Sets the current Zigbee network state for the specified Zigbee stack
 * instance. This will normally be called by the radio abstraction layer
 * to update the current network state and notify any registered network
 * state monitors.
 * @param zigbeeStack This is the Zigbee stack instance for which the
 *     current network state is being updated.
 * @param networkState This is the new network state that is being
 *     assigned.
 */
void gmosZigbeeSetNetworkState (gmosZigbeeStack_t* zigbeeStack,
    gmosZigbeeNetworkState_t networkState);

/**
 * Accesses the current Zigbee network state for the specified Zigbee
 * stack instance.
 * @param zigbeeStack This is the Zigbee stack instance for which the
 *     current network state is being requested.
 * @return Returns the current network state for the specified Zigbee
 *     stack instance.
 */
gmosZigbeeNetworkState_t gmosZigbeeGetNetworkState (
    gmosZigbeeStack_t* zigbeeStack);

/**
 * Initiates the formation of a new Zigbee network. This capability is
 * only supported for coordinator nodes and will only progress if the
 * Zigbee device is not currently joined to a Zigbee network.
 * @param zigbeeStack This is the Zigbee stack instance for the radio
 *     interface that should be used when forming the new Zigbee
 *     network.
 * @param securityMode This is the Zigbee security mode to be used when
 *     forming the network.
 * @param channelMask This is a bit mask which specifies the Zigbee
 *     radio channels which may be used when forming the network.
 *     802.15.4 channels 11 to 26 are supported for 2.4GHz operation,
 *     so this will be a subset of the mask value 0x07FFF800.
 * @param commonLinkKey This is a pointer to a byte array that contains
 *     the common link key to be used for devices joining the network.
 *     Setting this value to NULL indicates that a randomly generated
 *     key should be used.
 * @param networkKey This is a pointer to a byte array that contains
 *     the initial network key for the newly formed network. Setting
 *     this value to NULL indicates that a randomly generated key should
 *     be used.
 * @param extendedPanId This is a pointer to a byte array that contains
 *     the extended PAN ID to be used by the network. Setting this value
 *     to NULL indicates that a randomly generated extended PAN ID
 *     should be used.
 * @return Returns a Zigbee network status value which will be set to
 *     'GMOS_ZIGBEE_STATUS_SUCCESS' on successfully starting the network
 *     formation process and the appropriate status code on failure.
 */
gmosZigbeeStatus_t gmosZigbeeFormNetwork (gmosZigbeeStack_t* zigbeeStack,
    gmosZigbeeSecurityMode_t securityMode, uint32_t channelMask,
    uint8_t* commonLinkKey, uint8_t* networkKey, uint8_t* extendedPanId);

/**
 * Initiates the joining process for an existing Zigbee network. This
 * capability is not supported for coordinator nodes and will only
 * progress if the Zigbee device is not currently joined to a Zigbee
 * network.
 * @param zigbeeStack This is the Zigbee stack instance for the radio
 *     interface that should be used when joining the new Zigbee
 *     network.
 * @param channelMask This is a bit mask which specifies the Zigbee
 *     radio channels which will be searched when joining the network.
 *     802.15.4 channels 11 to 26 are supported for 2.4GHz operation,
 *     so this will be a subset of the mask value 0x07FFF800.
 * @param deviceLinkKey This is a pointer to a byte array that contains
 *     the device link key to be used when joining the network. Setting
 *     this value to NULL indicates that the link key will be provided
 *     by the network trust centre in unsecured joining mode.
 * @param extendedPanId This is a pointer to a byte array that contains
 *     the extended PAN ID of the network to be joined. Setting this
 *     value to NULL indicates that any available network can be joined.
 * @return Returns a Zigbee network status value which will be set to
 *     'GMOS_ZIGBEE_STATUS_SUCCESS' on successfully starting the network
 *     joining process and the appropriate status code on failure.
 */
gmosZigbeeStatus_t gmosZigbeeJoinNetwork (
    gmosZigbeeStack_t* zigbeeStack, uint32_t channelMask,
    uint8_t* deviceLinkKey, uint8_t* extendedPanId);

/**
 * Initiates the network leaving process. This causes the specified
 * Zigbee radio interface to be disconnected from the current network.
 * @param zigbeeStack This is the Zigbee stack instance for the radio
 *     interface that should be used when leaving the Zigbee network.
 * @return Returns a Zigbee network status value which will be set to
 *     'GMOS_ZIGBEE_STATUS_SUCCESS' on successfully starting the network
 *     joining process and the appropriate status code on failure.
 */
gmosZigbeeStatus_t gmosZigbeeLeaveNetwork (
    gmosZigbeeStack_t* zigbeeStack);

/**
 * Enables device joining for the Zigbee network. This capability is
 * only supported for coordinator nodes that have previously formed an
 * active network.
 * @param zigbeeStack This is the Zigbee stack instance for the radio
 *     interface that should be used when setting the device joining
 *     mode.
 * @param joiningMode This is the joining mode that should be used by
 *     the coordinator to control device joining.
 * @param joiningTimeout This timeout specifies the number of seconds
 *     after which the joining mode will revert to the default policy.
 * @return Returns a Zigbee network status value which will be set to
 *     'GMOS_ZIGBEE_STATUS_SUCCESS' on successfully starting the network
 *     joining process and the appropriate status code on failure.
 */
gmosZigbeeStatus_t gmosZigbeeSetJoiningMode (
    gmosZigbeeStack_t* zigbeeStack, gmosZigbeeJoiningMode_t joiningMode,
    uint32_t joiningTimeout);

/**
 * Gets the current device joining mode in use by the Zigbee network.
 * This capability is only supported for coordinator nodes that have
 * previously formed an active network.
 * @param zigbeeStack This is the Zigbee stack instance for the radio
 *     interface that should be used when getting the device joining
 *     mode.
 * @return Returns the joining mode that is currently being used by the
 *     coordinator to control device joining.
 */
gmosZigbeeJoiningMode_t gmosZigbeeGetJoiningMode (
    gmosZigbeeStack_t* zigbeeStack);

#endif // GMOS_ZIGBEE_STACK_H
