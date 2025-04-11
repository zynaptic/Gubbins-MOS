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
 * This header defines the common API for accessing the standard Zigbee
 * APS messaging layer.
 */

#ifndef GMOS_ZIGBEE_APS_H
#define GMOS_ZIGBEE_APS_H

#include <stdint.h>
#include "gmos-buffers.h"
#include "gmos-zigbee-stack.h"

/**
 * This enumeration specifies the supported APS message types.
 */
typedef enum {

    // Specify an APS unicast message sent with direct addressing.
    GMOS_ZIGBEE_APS_MSG_TYPE_TX_UNICAST_DIRECT        = 0x00,

    // Specify an APS unicast message sent using the address cache.
    GMOS_ZIGBEE_APS_MSG_TYPE_TX_UNICAST_ADDRESS_CACHE = 0x01,

    // Specify an APS unicast message sent using the binding table.
    GMOS_ZIGBEE_APS_MSG_TYPE_TX_UNICAST_BINDING_TABLE = 0x02,

    // Specify an APS transmitted multicast message.
    GMOS_ZIGBEE_APS_MSG_TYPE_TX_MULTICAST             = 0x03,

    // Specify an APS transmitted broadcast message.
    GMOS_ZIGBEE_APS_MSG_TYPE_TX_BROADCAST             = 0x04,

    // Specify an APS received unicast message.
    GMOS_ZIGBEE_APS_MSG_TYPE_RX_UNICAST               = 0x80,

    // Specify an APS received unicast reply message.
    GMOS_ZIGBEE_APS_MSG_TYPE_RX_UNICAST_REPLY         = 0x81,

    // Specify an APS received multicast message.
    GMOS_ZIGBEE_APS_MSG_TYPE_RX_MULTICAST             = 0x82,

    // Specify an APS received multicast loopback message.
    GMOS_ZIGBEE_APS_MSG_TYPE_RX_MULTICAST_LOOPBACK    = 0x83,

    // Specify an APS received broadcast message.
    GMOS_ZIGBEE_APS_MSG_TYPE_RX_BROADCAST             = 0x84,

    // Specify an APS received broadcast loopback message.
    GMOS_ZIGBEE_APS_MSG_TYPE_RX_BROADCAST_LOOPBACK    = 0x85,

    // Specify an unknown APS message type.
    GMOS_ZIGBEE_APS_MSG_TYPE_UNKNOWN                  = 0xFF

} gmosZigbeeApsMsgType_t;

/**
 * This enumeration specifies the supported APS message option flags.
 */
typedef enum {

    // Specify no APS message option flags.
    GMOS_ZIGBEE_APS_OPTION_NONE                  = 0x00,

    // Use the APS retry mechanism for unicast messages.
    GMOS_ZIGBEE_APS_OPTION_RETRY                 = 0x01,

    // Specify that a ZDO response message is required. Certain vendor
    // stack configurations can automatically respond to some ZDO
    // requests, which is indicated by leaving this flag unset in ZDO
    // request messages.
    GMOS_ZIGBEE_APS_OPTION_ZDO_RESPONSE_REQUIRED = 0x80

} gmosZigbeeApsOptions_t;

/**
 * This enumeration specifies the supported APS broadcast modes.
 */
typedef enum {

    // Specify broadcasts to all nodes.
    GMOS_ZIGBEE_APS_BROADCAST_ALL_NODES    = 0xFFFF,

    // Specify broadcasts to all 'always listening' nodes.
    GMOS_ZIGBEE_APS_BROADCAST_ALL_RX_IDLE  = 0xFFFD,

    // Specify broadcasts to all routers and coordinator.
    GMOS_ZIGBEE_APS_BROADCAST_ROUTERS_ONLY = 0xFFFC

} gmosZigbeeApsBroadcastType_t;

/**
 * This data structure provides a common encapsulation for a Zigbee
 * APS message, including all the required APS message frame fields.
 */
typedef struct gmosZigbeeApsFrame_t {

    // Specify the APS message type.
    uint8_t apsMsgType;

    // Specify the APS message option flags.
    uint8_t apsMsgFlags;

    // Specify the application profile ID for the APS frame.
    uint16_t profileId;

    // Specify the application cluster ID for the APS frame.
    uint16_t clusterId;

    // Specify the optional multicast group.
    uint16_t groupId;

    // Specify the node ID of the device peer node, which is either the
    // message source or destination depending on context. This field
    // may also contain the binding table index when sending message
    // via the binding table or the broadcast address for broadcast
    // messages.
    union {
        uint16_t nodeId;        // Specifies the 16-bit peer node ID.
        uint16_t index;         // Specifies the binding table index.
        uint16_t broadcastType; // Specifies the APS broadcast type.
    } peer;

    // Specify the source endpoint used when transmitting the message.
    uint8_t sourceEndpoint;

    // Specify the target endpoint used when receiving the message.
    uint8_t targetEndpoint;

    // Specify the APS frame sequence number.
    uint8_t apsSequence;

    // Specify the number of hops that a transmitted frame will be
    // allowed to travel through the network.
    uint8_t apsMsgRadius;

    // Specify the buffer that contains the message payload data.
    gmosBuffer_t payloadBuffer;

} gmosZigbeeApsFrame_t;

/**
 * This is the function prototype for callback handlers which will be
 * called by the Zigbee stack in order to notify the common Zigbee
 * framework that an APS message has been transmitted.
 * @param zigbeeStack This is the Zigbee stack which was used to
 *     transmit the outgoing APS message.
 * @param txMsgApsFrame This is the APS frame data structure which
 *     encapsulates the transmitted APS message.
 * @param txMsgStatus This is a status value which will indicate
 *     whether the message was successfully transmitted.
 * @param txMsgTag This is an 8-bit message tag that may be used to
 *     match transmit requests with their corresponding message sent
 *     callbacks.
 */
typedef void (*gmosZigbeeApsMsgSentHandler_t) (
    gmosZigbeeStack_t* zigbeeStack, gmosZigbeeApsFrame_t* txMsgApsFrame,
    gmosZigbeeStatus_t txMsgStatus, uint8_t txMsgTag);

/**
 * This is the public APS message unicast transmit function which may be
 * called to send the specified APS message. All the message transmit
 * options are encapsulated in the APS frame data structure.
 * @param zigbeeStack This is the Zigbee stack instance which is to
 *     transmit the outgoing APS message.
 * @param txMsgApsFrame This is the APS frame data structure which
 *     specifies the transmit parameters to be used and includes the
 *     transmit message buffer. On successfully initiating a message
 *     transmit operation the transmit message buffer will be empty.
 * @param txMsgSentHandler This is a pointer to the callback function
 *     which should be called on completion of the unicast transmit
 *     operation.
 * @param txMsgTag This is a pointer to an 8-bit message tag which
 *     will be populated with a value that may be used to match transmit
 *     requests with their corresponding message sent callbacks. A null
 *     reference may be specified if the tag value is not required.
 * @return Returns a status value which will indicate success, a retry
 *     request or the reason for failure.
 */
gmosZigbeeStatus_t gmosZigbeeApsUnicastTransmit (
    gmosZigbeeStack_t* zigbeeStack, gmosZigbeeApsFrame_t* txMsgApsFrame,
    gmosZigbeeApsMsgSentHandler_t txMsgSentHandler, uint8_t* txMsgTag);

/**
 * This is the public APS message broadcast transmit function which may
 * be called to send the specified APS message. All the message transmit
 * options are encapsulated in the APS frame data structure.
 * @param zigbeeStack This is the Zigbee stack instance which is to
 *     transmit the outgoing APS message.
 * @param txMsgApsFrame This is the APS frame data structure which
 *     specifies the transmit parameters to be used and includes the
 *     transmit message buffer. On successfully initiating a message
 *     transmit operation the transmit message buffer will be empty.
 * @param txMsgSentHandler This is a pointer to the callback function
 *     which should be called on completion of the broadcast transmit
 *     operation.
 * @param txMsgTag This is a pointer to an 8-bit message tag which
 *     will be populated with a value that may be used to match transmit
 *     requests with their corresponding message sent callbacks. A null
 *     reference may be specified if the tag value is not required.
 * @return Returns a status value which will indicate success, a retry
 *     request or the reason for failure.
 */
gmosZigbeeStatus_t gmosZigbeeApsBroadcastTransmit (
    gmosZigbeeStack_t* zigbeeStack, gmosZigbeeApsFrame_t* txMsgApsFrame,
    gmosZigbeeApsMsgSentHandler_t txMsgSentHandler, uint8_t* txMsgTag);

/**
 * This is the Zigbee radio specific APS message unicast transmit
 * function which will be called by the common APS messaging layer to
 * send the specified APS message. All the message transmit options are
 * encapsulated in the APS frame data structure.
 * @param zigbeeStack This is the Zigbee stack instance which is to
 *     transmit the outgoing APS message.
 * @param txMsgApsFrame This is the APS frame data structure which
 *     specifies the transmit parameters to be used and includes the
 *     transmit message buffer. On successfully initiating a message
 *     transmit operation the transmit message buffer will be empty.
 * @param txMsgTag This is a pointer to an 8-bit message tag which
 *     will be populated with a value that may be used to match transmit
 *     requests with their corresponding message sent callbacks.
 * @return Returns a status value which will indicate success, a retry
 *     request or the reason for failure.
 */
gmosZigbeeStatus_t gmosZigbeeStackApsUnicastTransmit (
    gmosZigbeeStack_t* zigbeeStack, gmosZigbeeApsFrame_t* txMsgApsFrame,
    uint8_t* txMsgTag);

/**
 * This is the Zigbee radio specific APS message broadcast transmit
 * function which will be called by the common APS messaging layer to
 * send the specified APS message. All the message transmit options are
 * encapsulated in the APS frame data structure.
 * @param zigbeeStack This is the Zigbee stack instance which is to
 *     transmit the outgoing APS message.
 * @param txMsgApsFrame This is the APS frame data structure which
 *     specifies the transmit parameters to be used and includes the
 *     transmit message buffer. On successfully initiating a message
 *     transmit operation the transmit message buffer will be empty.
 * @param txMsgTag This is a pointer to an 8-bit message tag which
 *     will be populated with a value that may be used to match transmit
 *     requests with their corresponding message sent callbacks.
 * @return Returns a status value which will indicate success, a retry
 *     request or the reason for failure.
 */
gmosZigbeeStatus_t gmosZigbeeStackApsBroadcastTransmit (
    gmosZigbeeStack_t* zigbeeStack, gmosZigbeeApsFrame_t* txMsgApsFrame,
    uint8_t* txMsgTag);

/**
 * This is a notification handler which will be called by the Zigbee
 * radio specific stack in order to notify the common GubbinsMOS Zigbee
 * framework implementation of a newly received APS message.
 * @param zigbeeStack This is the Zigbee stack instance which received
 *     the incoming APS message.
 * @param rxMsgApsFrame This is the APS frame data structure which
 *     encapsulates the received APS message. The APS frame contents
 *     are only guaranteed to remain valid for the duration of the
 *     callback.
 * @param txMsgApsBuffer This is a GubbinsMOS buffer that may be used
 *     to supply a response message that will be transmitted to the
 *     originating node of the received message using the same APS
 *     parameters as the original request. The APS message payload
 *     should be placed in this buffer if required. By default this
 *     buffer is empty, so no response will be sent.
 */
void gmosZigbeeStackApsMessageReceived (
    gmosZigbeeStack_t* zigbeeStack, gmosZigbeeApsFrame_t* rxMsgApsFrame,
    gmosBuffer_t* txMsgApsBuffer);

/**
 * This is a notification handler which will be called by the Zigbee
 * radio specific stack in order to notify the common GubbinsMOS Zigbee
 * framework implementation that an APS message has been transmitted.
 * @param zigbeeStack This is the Zigbee stack instance which was to
 *     transmit the outgoing APS message.
 * @param txMsgApsFrame This is the APS frame data structure which
 *     encapsulates the transmitted APS message.
 * @param txMsgStatus This is a status value which will indicate
 *     whether the message was successfully transmitted.
 * @param txMsgTag This is an 8-bit message tag that may be used to
 *     match transmit requests with their corresponding message sent
 *     callbacks.
 */
void gmosZigbeeStackApsMessageTransmitted (
    gmosZigbeeStack_t* zigbeeStack, gmosZigbeeApsFrame_t* txMsgApsFrame,
    gmosZigbeeStatus_t txMsgStatus, uint8_t txMsgTag);

#endif // GMOS_ZIGBEE_APS_H
