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
 * This header specifies the internal interface to the MQTT packet
 * formatting and parsing component. It supports the version 3.1.1 MQTT
 * protocol.
 */

#ifndef GMOS_MQTT_PACKET_H
#define GMOS_MQTT_PACKET_H

#include <stdint.h>
#include <stdbool.h>
#include "gmos-buffers.h"

/**
 * Specifies the supported MQTT packet type values.
 */
typedef enum {
    GMOS_MQTT_PACKET_HEADER_TYPE_CONNECT     = 0x10,
    GMOS_MQTT_PACKET_HEADER_TYPE_CONNACK     = 0x20,
    GMOS_MQTT_PACKET_HEADER_TYPE_PUBLISH     = 0x30,
    GMOS_MQTT_PACKET_HEADER_TYPE_PUBACK      = 0x40,
    GMOS_MQTT_PACKET_HEADER_TYPE_PUBREC      = 0x50,
    GMOS_MQTT_PACKET_HEADER_TYPE_PUBREL      = 0x60,
    GMOS_MQTT_PACKET_HEADER_TYPE_PUBCOMP     = 0x70,
    GMOS_MQTT_PACKET_HEADER_TYPE_SUBSCRIBE   = 0x80,
    GMOS_MQTT_PACKET_HEADER_TYPE_SUBACK      = 0x90,
    GMOS_MQTT_PACKET_HEADER_TYPE_UNSUBSCRIBE = 0xA0,
    GMOS_MQTT_PACKET_HEADER_TYPE_UNSUBACK    = 0xB0,
    GMOS_MQTT_PACKET_HEADER_TYPE_PINGREQ     = 0xC0,
    GMOS_MQTT_PACKET_HEADER_TYPE_PINGRESP    = 0xD0,
    GMOS_MQTT_PACKET_HEADER_TYPE_DISCONNECT  = 0xE0,
    GMOS_MQTT_PACKET_HEADER_TYPE_MASK        = 0xF0,
    GMOS_MQTT_PACKET_HEADER_TYPE_INVALID     = 0xFF
} gmosMqttPacketHeaderType_t;

/**
 * Specifies the supported MQTT packet flag values.
 */
typedef enum {
    GMOS_MQTT_PACKET_HEADER_FLAG_RETAIN    = 0x01,
    GMOS_MQTT_PACKET_HEADER_FLAG_QOS0      = 0x00,
    GMOS_MQTT_PACKET_HEADER_FLAG_QOS1      = 0x02,
    GMOS_MQTT_PACKET_HEADER_FLAG_QOS2      = 0x04,
    GMOS_MQTT_PACKET_HEADER_FLAG_QOS_MASK  = 0x06,
    GMOS_MQTT_PACKET_HEADER_FLAG_DUPLICATE = 0x08,
    GMOS_MQTT_PACKET_HEADER_FLAG_MASK      = 0x0F
} gmosMqttPacketHeaderFlags_t;

/**
 * Specifies the supported MQTT connection packet flags.
 */
typedef enum {
    GMOS_MQTT_PACKET_CONNECT_FLAG_CLEAN_SESSION = 0x02,
    GMOS_MQTT_PACKET_CONNECT_FLAG_WILL_REQUEST  = 0x04,
    GMOS_MQTT_PACKET_CONNECT_FLAG_WILL_QOS1     = 0x08,
    GMOS_MQTT_PACKET_CONNECT_FLAG_PASSWORD      = 0x40,
    GMOS_MQTT_PACKET_CONNECT_FLAG_USER_NAME     = 0x80
} gmosMqttPacketConnectFlags_t;

/**
 * Specifies the supported MQTT connection acknowledgement packet flags.
 */
typedef enum {
    GMOS_MQTT_PACKET_CONNECT_ACK_FLAG_SESSION_PRESENT = 0x01
} gmosMqttPacketConnectAckFlags_t;

/**
 * Specifies the supported MQTT connection acknowledgement status
 * values.
 */
typedef enum {
    GMOS_MQTT_PACKET_CONNECT_ACK_STATUS_ACCEPTED            = 0x00,
    GMOS_MQTT_PACKET_CONNECT_ACK_STATUS_UNSUPPORTED_VERSION = 0x01,
    GMOS_MQTT_PACKET_CONNECT_ACK_STATUS_CLIENT_ID_REJECTED  = 0x02,
    GMOS_MQTT_PACKET_CONNECT_ACK_STATUS_SERVER_UNAVAILABLE  = 0x03,
    GMOS_MQTT_PACKET_CONNECT_ACK_STATUS_USER_INVALID        = 0x04,
    GMOS_MQTT_PACKET_CONNECT_ACK_STATUS_NOT_AUTHORIZED      = 0x05,
    GMOS_MQTT_PACKET_CONNECT_ACK_STATUS_MALFORMED_PACKET    = 0xFF
} gmosMqttPacketConnectAckStatus_t;

/**
 * Specfies the supported MQTT subscriber acknowledgement status values.
 */
typedef enum {
    GMOS_MQTT_PACKET_SUBSCRIBE_ACK_STATUS_QOS0             = 0x00,
    GMOS_MQTT_PACKET_SUBSCRIBE_ACK_STATUS_QOS1             = 0x01,
    GMOS_MQTT_PACKET_SUBSCRIBE_ACK_STATUS_QOS2             = 0x02,
    GMOS_MQTT_PACKET_SUBSCRIBE_ACK_STATUS_FAILED           = 0x80,
    GMOS_MQTT_PACKET_SUBSCRIBE_ACK_STATUS_MALFORMED_PACKET = 0xFF
} gmosMqttPacketSubscribeAckStatus_t;

/**
 * Formats a new MQTT control only packet, which is any packet that
 * does not require a packet ID and which doesn't carry a payload.
 * @param packetBuffer This is the packet buffer which will be populated
 *     with the new control packet. Any existing buffer contents will be
 *     discarded.
 * @param packetTypeFlags This is the octet containing the packet type
 *     and associated flags which will be used as the first octet of the
 *     formatted packet.
 * @return Returns a boolean value which will be set to 'true' if the
 *     packet was successfully formatted and 'false' if there is
 *     insufficient buffer memory to format the packet.
 */
bool gmosMqttPacketFormatControl (gmosBuffer_t* packetBuffer,
    uint8_t packetTypeFlags);

/**
 * Formats a new MQTT handshake packet, which is any packet that
 * requires a packet ID, but which doesn't carry a payload.
 * @param packetBuffer This is the packet buffer which will be populated
 *     with the new handshake packet. Any existing buffer contents will
 *     be discarded.
 * @param packetTypeFlags This is the octet containing the packet type
 *     and associated flags which will be used as the first octet of the
 *     formatted packet.
 * @param packetId This is a non-zero value which will be used as the
 *     packet identifier for the handshake packet.
 * @return Returns a boolean value which will be set to 'true' if the
 *     packet was successfully formatted and 'false' if there is
 *     insufficient buffer memory to format the packet.
 */
bool gmosMqttPacketFormatHandshake (gmosBuffer_t* packetBuffer,
    uint8_t packetTypeFlags, uint16_t packetId);

/**
 * Formats a new MQTT connect packet, placing it in the specified
 * GubbinsMOS buffer.
 * @param packetBuffer This is the packet buffer which will be populated
 *     with the new connect packet. Any existing buffer contents will
 *     be discarded.
 * @param clientId This is a pointer to a string that contains the
 *     client identifier to be used in the connect packet.
 * @param cleanSession This is a boolean flag which when set to 'true'
 *     indicates that the clean session bit should be set for the
 *     connection request.
 * @param willTopic This is a pointer to a string which contains the
 *     topic name to be used for the will message that is to be
 *     associated with the client. A null reference should be used if
 *     no will message is to be sent.
 * @param willMsgData This is a pointer to a byte array which contains
 *     the will message that is to be associated with the client. A null
 *     reference should be used if no will message is to be sent. By
 *     default, the will message should be sent with a QoS level of 1
 *     and with the 'retain' bit not set.
 * @param willMsgSize This specifies the length of the will message that
 *     is to be associated with the client. A zero value should be used
 *     if no will message is to be sent.
 * @param userName This is a pointer to a string which specifies the
 *     user name to be sent when authenticating the client with the MQTT
 *     broker. A null reference should be used if no user name is to be
 *     sent.
 * @param password This is a pointer to a string which specifies the
 *     password to be sent when authenticating the client with the MQTT
 *     broker. A null reference should be used if no password is to be
 *     sent. Note that passwords are treated here as UTF-8 strings
 *     rather than arbitrary byte arrays.
 * @return Returns a boolean value which will be set to 'true' if the
 *     packet was successfully formatted and 'false' if there is
 *     insufficient buffer memory to format the packet.
 */
bool gmosMqttPacketFormatConnect (gmosBuffer_t* packetBuffer,
    const char* clientId, bool cleanSession, const char* willTopic,
    const uint8_t* willMsgData, uint16_t willMsgSize,
    const char* userName, const char* password);

/**
 * Formats a new MQTT publish packet, adding the required headers to
 * the payload data that is already present in the packet buffer.
 * @param packetBuffer This is the packet buffer which contains the
 *     payload data that is to be included in the MQTT publishing
 *     message. On successful completion the required MQTT header fields
 *     will have been added to the buffer contents to create a valid
 *     MQTT packet. On failure the contents of the packet buffer will
 *     be unchanged.
 * @param publishFlags This is the set of flags to be included in the
 *     first byte of the packet header along with the publish packet
 *     type. It encodes the QoS level for the published message as well
 *     as the retain flag if required. The duplicate flag will always
 *     be left unset for a newly formatted packet.
 * @param packetId This is the non-zero packet identifier to be included
 *     in the variable packet header for QoS level 1 and QoS level 2
 *     messages.
 * @param pubTopic This is a pointer to a string containing the MQTT
 *     topic to be included in the publish packet variable header.
 * @return Returns a boolean value which will be set to 'true' if the
 *     packet was successfully formatted and 'false' if there is
 *     insufficient buffer memory to format the packet.
 */
bool gmosMqttPacketFormatPublish (gmosBuffer_t* packetBuffer,
    uint8_t publishFlags, uint16_t packetId, const char* pubTopic);

/**
 * Formats a new MQTT subscribe packet, placing it in the specified
 * GubbinsMOS buffer. Note that this is limited to subscribe requests
 * that contain a single MQTT topic filter.
 * @param packetBuffer This is the packet buffer which will be populated
 *     with the new subscribe request packet. Any existing buffer
 *     contents will be discarded.
 * @param packetId This is the non-zero packet identifier to be included
 *     in the variable packet header.
 * @param mqttTopicFilter This is a pointer to string that should
 *     contain a valid MQTT topic filter for inclusion in the subscribe
 *     request message.
 * @param qosLevel This specifies the maximum QoS level that can be used
 *     for receiving subscribed messages. It should be an integer value
 *     representing the QoS level (0, 1 or 2).
 * @return Returns a boolean value which will be set to 'true' if the
 *     packet was successfully formatted and 'false' if there is
 *     insufficient buffer memory to format the packet.
 */
bool gmosMqttPacketFormatSubscribe (gmosBuffer_t* packetBuffer,
    uint16_t packetId, const char* mqttTopicFilter, uint8_t qosLevel);

/**
 * Formats a new MQTT unsubscribe packet, placing it in the specified
 * GubbinsMOS buffer. Note that this is limited to unsubscribe requests
 * that contain a single MQTT topic filter.
 * @param packetBuffer This is the packet buffer which will be populated
 *     with the new unsubscribe request packet. Any existing buffer
 *     contents will be discarded.
 * @param packetId This is the non-zero packet identifier to be included
 *     in the variable packet header.
 * @param mqttTopicFilter This is a pointer to string that should
 *     contain a valid MQTT topic filter for inclusion in the
 *     unsubscribe request message.
 * @return Returns a boolean value which will be set to 'true' if the
 *     packet was successfully formatted and 'false' if there is
 *     insufficient buffer memory to format the packet.
 */
bool gmosMqttPacketFormatUnsubscribe (gmosBuffer_t* packetBuffer,
    uint16_t packetId, const char* mqttTopicFilter);

/**
 * Sets the duplicate flag for the specified publish message.
 * @param packetBuffer This is a packet buffer which should contain a
 *     valid MQTT publish message. On successful completion, the
 *     duplicate message flag for the publish message will have been
 *     set.
 * @return Returns a boolean value which will be set to 'true' if the
 *     packet was successfully modified and 'false' if the buffer did
 *     not contain a valid publish message header.
 */
bool gmosMqttPacketFormatSetDuplicate (gmosBuffer_t* packetBuffer);

/**
 * Parses the common fixed header of a received MQTT packet stored in
 * the specified packet buffer. The packet remains in the buffer after
 * parsing.
 * @param packetBuffer This is a packet buffer which should contain a
 *     valid MQTT packet.
 * @param packetType This is a pointer to a byte value which on
 *     successful completion will be populated with the parsed MQTT
 *     packet type. If the MQTT packet header is malformed, this will be
 *     indicated by setting the packet type to the packet invalid value.
 * @param packetFlags This is a pointer to a byte value which on
 *     successful completion will be populated with the parsed MQTT
 *     packet flags.
 * @param headerSize This is a pointer to a byte value which on
 *     successful completion will be populated with the length of the
 *     MQTT fixed header.
 * @param remainingSize This is a pointer to an integer value which on
 *     successful completion will be populated with the remaining packet
 *     length value extracted from the MQTT fixed header.
 * @return Returns a boolean value which will be set to 'true' if the
 *     packet fixed header was successfully parsed and 'false' if the
 *     buffer did not contain sufficient data to parse the MQTT fixed
 *     header.
 */
bool gmosMqttPacketParseFixedHeader (gmosBuffer_t* packetBuffer,
    uint8_t* packetType, uint8_t* packetFlags, uint8_t* headerSize,
    uint32_t* remainingSize);

/**
 * Parses the variable header of a received MQTT data packet stored in
 * the specified packet buffer in order to extract the data packet
 * flags, the optional packet ID and the payload offset. The packet
 * remains in the buffer after parsing.
 * @param packetBuffer This is a packet buffer which should contain a
 *     valid MQTT published data packet.
 * @param packetType This is a pointer to a byte value which on
 *     successful completion will be populated with the parsed MQTT
 *     packet type. If the packet is not a valid published data packet,
 *     this will be indicated by setting the packet type to the packet
 *     invalid value.
 * @param packetFlags This is a pointer to a byte value which on
 *     successful completion will be populated with the parsed MQTT
 *     packet flags.
 * @param packetId This is a pointer to a 16-bit value which on
 *     successful completion will be populated with the parsed MQTT
 *     packet ID.
 * @param payloadOffset This is a pointer to a 16-bit value which on
 *     successful completion will be populated with the offset of the
 *     published data payload from the start of the packet buffer.
 * @return Returns a boolean value which will be set to 'true' if the
 *     packet was successfully parsed and 'false' if the buffer did not
 *     contain sufficient data to parse the MQTT published data packet.
 */
bool gmosMqttPacketParseDataPacket (gmosBuffer_t* packetBuffer,
    uint8_t* packetType, uint8_t* packetFlags, uint16_t* packetId,
    uint16_t* payloadOffset);

/**
 * Parses the variable header containing the packet ID of a received
 * MQTT handshake packet stored in the specified packet buffer. The
 * packet remains in the buffer after parsing.
 * @param packetBuffer This is a packet buffer which should contain a
 *     valid MQTT handshake packet.
 * @param packetId This is a pointer to a 16-bit value which on
 *     successful completion will be populated with the parsed MQTT
 *     packet ID.
 * @return Returns a boolean value which will be set to 'true' if the
 *     packet was successfully parsed and 'false' if the buffer did not
 *     contain sufficient data to parse the MQTT handshake packet.
 */
bool gmosMqttPacketParseHandshakePacketId (gmosBuffer_t* packetBuffer,
    uint16_t* packetId);

/**
 * Parses a received MQTT packet as a connection acknowledgement packet.
 * The packet remains in the buffer after parsing.
 * @param packetBuffer This is a packet buffer which should contain a
 *     valid MQTT connection acknowledgement packet.
 * @param sessionPresent This is a pointer to a boolean value which on
 *     successful completion will be populated with the parsed session
 *     present flag.
 * @param connectStatus This is a pointer to a byte value which on
 *     successful completion will be populated with the parsed
 *     connection status value. A malformed packet status value is used
 *     to indicate that parsing failed.
 * @return Returns a boolean value which will be set to 'true' if the
 *     packet was successfully parsed and 'false' if the buffer did not
 *     contain sufficient data to parse the MQTT packet.
 */
bool gmosMqttPacketParseConnectAck (gmosBuffer_t* packetBuffer,
    bool* sessionPresent, uint8_t* connectStatus);

/**
 * Parses a received MQTT packet as a subscribe request acknowledgement
 * packet with a single payload byte. The packet remains in the buffer
 * after parsing.
 * @param packetBuffer This is a packet buffer which should contain a
 *     valid MQTT subscribe acknowledgement packet.
 * @param subscribeStatus This is a pointer to a byte value which on
 *     successful completion will be populated with the subscription
 *     status returned by the broker. A malformed packet status value is
 *     used to indicate that parsing failed.
 * @return Returns a boolean value which will be set to 'true' if the
 *     packet was successfully parsed and 'false' if the buffer did not
 *     contain sufficient data to parse the MQTT packet.
 */
bool gmosMqttPacketParseSubscribeAck (gmosBuffer_t* packetBuffer,
    uint8_t* subscribeStatus);

/**
 * Parses the variable header containing the topic name of a received
 * MQTT data packet stored in the specified packet buffer, and attempts
 * to match it against the supplied MQTT topic filter string. The packet
 * remains in the buffer after parsing.
 * @param packetBuffer This is a packet buffer which should contain a
 *     valid MQTT published data packet.
 * @param topicFilter This is a pointer to a string which should contain
 *     a valid MQTT topic filter.
 * @param matchOk This is a pointer to a boolean value which on
 *     completion will be set to 'true' if the data packet was well
 *     formed and contained a topic string which matched the specified
 *     topic filter and 'false' otherwise.
 * @return Returns a boolean value which will be set to 'true' if the
 *     packet was successfully parsed and 'false' if the buffer did not
 *     contain sufficient data to parse the MQTT packet.
 */
bool gmosMqttPacketMatchDataPacketTopic (gmosBuffer_t* packetBuffer,
    const char* topicFilter, bool* matchOk);

#endif // GMOS_MQTT_PACKET_H
