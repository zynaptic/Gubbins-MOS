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
 * This file implements the MQTT packet formatting and parsing
 * component. It supports the version 3.1.1 MQTT protocol.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-buffers.h"
#include "gmos-mqtt-config.h"
#include "gmos-mqtt-packet.h"

/*
 * Implement variable length unsigned integer encoding, returning the
 * number of bytes used for the encoding up to a limit of 4.
 */
static inline uint_fast8_t gmosMqttPacketVarIntEncode (
    uint32_t value, uint8_t* encoding)
{
    uint32_t residualValue = value;
    uint_fast8_t byteValue;
    uint_fast8_t i;

    // Iterate for a maximum of 4 bytes.
    for (i = 0; i < 4; i++) {
        byteValue = residualValue & 0x7F;
        residualValue >>= 7;
        if (residualValue != 0) {
            encoding [i] = byteValue | 0x80;
        } else {
            encoding [i] = byteValue;
            break;
        }
    }

    // Return the number of bytes used, or a zero value to indicate
    // that the value is out of range.
    if (residualValue == 0) {
        return i + 1;
    } else {
        return 0;
    }
}

/*
 * Prepends the fixed MQTT packet header to the packet buffer.
 */
static bool gmosMqttPacketPrependFixedHeader (
    gmosBuffer_t* packetBuffer, uint_fast8_t packetTypeByte)
{
    uint_fast16_t length;
    uint8_t headerData [5];
    uint_fast8_t headerSize;

    // The remaining length field is set to the size of the existing
    // packet buffer contents.
    length = gmosBufferGetSize (packetBuffer);

    // The packet type byte contains both the control packet type and
    // the associated packet flags.
    headerData [0] = packetTypeByte;
    headerSize = 1;

    // The remaining length field is set to the size of the existing
    // packet buffer contents. No checks on the length field encoding
    // are required, since the 16-bit length of the original packet
    // buffer will always fall within the valid range.
    headerSize += gmosMqttPacketVarIntEncode (length, &(headerData [1]));

    // Prepend the header contents to the buffer.
    return gmosBufferPrepend (packetBuffer, headerData, headerSize);
}

/*
 * Prepends a packet ID field to the packet buffer.
 */
static bool gmosMqttPacketPrependPacketId (
    gmosBuffer_t* packetBuffer, uint_fast16_t packetId)
{
    uint8_t packetIdData [2];
    packetIdData [0] = (uint8_t) (packetId >> 8);
    packetIdData [1] = (uint8_t) (packetId);

    return gmosBufferPrepend (packetBuffer,
        packetIdData, sizeof (packetIdData));
}

/*
 * Append a string to the specified packet buffer using the MQTT length
 * and data format.
 */
static bool gmosMqttPacketAppendString (
    gmosBuffer_t* packetBuffer, const char* string)
{
    size_t length;
    uint8_t lengthBytes [2];

    // Check for valid string length.
    length = strlen (string);
    if (length > 0xFFFF) {
        return false;
    }

    // Store the string length bytes in big endian format.
    lengthBytes [0] = (uint8_t) (length >> 8);
    lengthBytes [1] = (uint8_t) (length);

    // Append the string length bytes and string data.
    return gmosBufferAppend (packetBuffer, lengthBytes, 2) &&
        gmosBufferAppend (packetBuffer, (uint8_t*) string, length);
}

/*
 * Prepend a string to the specified packet buffer using the MQTT length
 * and data format.
 */
static bool gmosMqttPacketPrependString (
    gmosBuffer_t* packetBuffer, const char* string)
{
    size_t length;
    uint8_t lengthBytes [2];

    // Check for valid string length.
    length = strlen (string);
    if (length > 0xFFFF) {
        return false;
    }

    // Store the string length bytes in big endian format.
    lengthBytes [0] = (uint8_t) (length >> 8);
    lengthBytes [1] = (uint8_t) (length);

    // Prepend the string data and the string length bytes.
    return gmosBufferPrepend (packetBuffer, (uint8_t*) string, length) &&
        gmosBufferPrepend (packetBuffer, lengthBytes, 2);
}

/*
 * Append an array of binary data to the specified packet buffer using
 * the MQTT length and data format.
 */
static bool gmosMqttPacketAppendBinaryData (gmosBuffer_t* packetBuffer,
    const uint8_t* data, uint_fast16_t dataSize)
{
    uint8_t lengthBytes [2];

    // Store the data length bytes in big endian format.
    lengthBytes [0] = (uint8_t) (dataSize >> 8);
    lengthBytes [1] = (uint8_t) (dataSize);

    // Append the data length and binary data.
    return gmosBufferAppend (packetBuffer, lengthBytes, 2) &&
        gmosBufferAppend (packetBuffer, data, dataSize);
}

/*
 * Formats a new MQTT control only packet, which is any packet that
 * does not require a packet ID and which doesn't carry a payload.
 */
bool gmosMqttPacketFormatControl (gmosBuffer_t* packetBuffer,
    uint8_t packetTypeFlags)
{
    uint8_t packetData [2];

    // The control packets all have a fixed format, which avoids the
    // need to dynamically generate the header.
    packetData [0] = packetTypeFlags;
    packetData [1] = 0;

    // Write the packet data to an empty buffer.
    gmosBufferReset (packetBuffer, 0);
    return gmosBufferAppend (packetBuffer,
        packetData, sizeof (packetData));
}

/*
 * Formats a new MQTT handshake packet, which is any packet that
 * requires a packet ID, but which doesn't carry a payload.
 */
bool gmosMqttPacketFormatHandshake (gmosBuffer_t* packetBuffer,
    uint8_t packetTypeFlags, uint16_t packetId)
{
    uint8_t packetData [4];

    // The handshake packets all have a fixed format, which avoids the
    // need to dynamically generate the header.
    packetData [0] = packetTypeFlags;
    packetData [1] = 2;
    packetData [2] = (uint8_t) (packetId >> 8);
    packetData [3] = (uint8_t) (packetId);

    // Write the packet data to an empty buffer.
    gmosBufferReset (packetBuffer, 0);
    return gmosBufferAppend (packetBuffer,
        packetData, sizeof (packetData));
}

/*
 * Formats a new MQTT connect packet, placing it in the specified
 * GubbinsMOS buffer.
 */
bool gmosMqttPacketFormatConnect (gmosBuffer_t* packetBuffer,
    const char* clientId, bool cleanSession, const char* willTopic,
    const uint8_t* willMsgData, uint16_t willMsgSize,
    const char* userName, const char* password)
{
    uint_fast8_t connectFlags;
    uint8_t connectHeader [10];

    // Set the clean session flag if required.
    if (cleanSession) {
        connectFlags = GMOS_MQTT_PACKET_CONNECT_FLAG_CLEAN_SESSION;
    } else {
        connectFlags = 0;
    }

    // Always populate the client ID field of the packet payload.
    gmosBufferReset (packetBuffer, 0);
    if (!gmosMqttPacketAppendString (packetBuffer, clientId)) {
        goto fail;
    }

    // Populate the will components of the packet payload if required.
    // The associated flags are always set to use a will QoS level of 1
    // and not to retain the message.
    if ((willTopic != NULL) && (willMsgData != NULL) && (willMsgSize != 0)) {
        connectFlags |= GMOS_MQTT_PACKET_CONNECT_FLAG_WILL_REQUEST |
            GMOS_MQTT_PACKET_CONNECT_FLAG_WILL_QOS1;
        if (!gmosMqttPacketAppendString (packetBuffer, willTopic)) {
            goto fail;
        }
        if (!gmosMqttPacketAppendBinaryData (
            packetBuffer, willMsgData, willMsgSize)) {
            goto fail;
        }
    }

    // Populate the user component of the packet payload if required.
    if (userName != NULL) {
        connectFlags |= GMOS_MQTT_PACKET_CONNECT_FLAG_USER_NAME;
        if (!gmosMqttPacketAppendString (packetBuffer, userName)) {
            goto fail;
        }
    }

    // Populate the password component of the packet payload if
    // required. If no user name is present, no password should be set.
    if ((userName != NULL) && (password != NULL)) {
        connectFlags |= GMOS_MQTT_PACKET_CONNECT_FLAG_PASSWORD;
        if (!gmosMqttPacketAppendString (packetBuffer, password)) {
            goto fail;
        }
    }

    // Specify MQTT v3.1.1 in the connection request packet header.
    connectHeader [0] = 0x00;
    connectHeader [1] = 0x04;
    connectHeader [2] = 'M';
    connectHeader [3] = 'Q';
    connectHeader [4] = 'T';
    connectHeader [5] = 'T';
    connectHeader [6] = 0x04;

    // Set the MQTT connection flags and keep alive time.
    connectHeader [7] = connectFlags;
    connectHeader [8] = (uint8_t) (GMOS_CONFIG_MQTT_KEEP_ALIVE_PERIOD >> 8);
    connectHeader [9] = (uint8_t) (GMOS_CONFIG_MQTT_KEEP_ALIVE_PERIOD);

    // Prepend the connection request packet header to the buffer.
    if (!gmosBufferPrepend (packetBuffer,
        connectHeader, sizeof (connectHeader))) {
        goto fail;
    }

    // Prepend the fixed header to the buffer.
    if (!gmosMqttPacketPrependFixedHeader (
        packetBuffer, GMOS_MQTT_PACKET_HEADER_TYPE_CONNECT)) {
        goto fail;
    }
    return true;

    // Release allocated packet data on failure.
fail :
    gmosBufferReset (packetBuffer, 0);
    return false;
}

/*
 * Formats a new MQTT publish packet, adding the required headers to
 * the payload data that is already present in the packet buffer.
 */
bool gmosMqttPacketFormatPublish (gmosBuffer_t* packetBuffer,
    uint8_t publishFlags, uint16_t packetId, const char* pubTopic)
{
    uint_fast8_t packetTypeByte;
    uint_fast16_t payloadSize;

    // The packet buffer already contains the message payload.
    payloadSize = gmosBufferGetSize (packetBuffer);

    // For a non-zero QoS setting, the packet ID needs to be prepended
    // the the existing payload data.
    if ((publishFlags & GMOS_MQTT_PACKET_HEADER_FLAG_QOS_MASK) !=
        GMOS_MQTT_PACKET_HEADER_FLAG_QOS0) {
        if (!gmosMqttPacketPrependPacketId (packetBuffer, packetId)) {
            goto fail;
        }
    }

    // Prepend the topic name to the existing payload data.
    if (!gmosMqttPacketPrependString (packetBuffer, pubTopic)) {
        goto fail;
    }

    // Prepend the fixed header to the published packet. Note that the
    // duplicate flag is always forced to 0 when first creating the
    // packet.
    packetTypeByte = GMOS_MQTT_PACKET_HEADER_TYPE_PUBLISH;
    packetTypeByte |= publishFlags & (
        GMOS_MQTT_PACKET_HEADER_FLAG_QOS_MASK |
        GMOS_MQTT_PACKET_HEADER_FLAG_RETAIN);
    if (!gmosMqttPacketPrependFixedHeader (
        packetBuffer, packetTypeByte)) {
        goto fail;
    }
    return true;

    // Release allocated packet data on failure. The payload data is
    // left intact.
fail :
    gmosBufferRebase (packetBuffer, payloadSize);
    return false;
}

/*
 * Formats a new MQTT subscribe packet, placing it in the specified
 * GubbinsMOS buffer.
 */
bool gmosMqttPacketFormatSubscribe (gmosBuffer_t* packetBuffer,
    uint16_t packetId, const char* mqttTopicFilter, uint8_t qosLevel)
{
    // Add the subscriber topic filter to the packet payload. Only a
    // single topic filter per request is supported.
    gmosBufferReset (packetBuffer, 0);
    if (!gmosMqttPacketAppendString (packetBuffer, mqttTopicFilter)) {
        goto fail;
    }
    if (!gmosBufferAppend (packetBuffer, &qosLevel, 1)) {
        goto fail;
    }

    // The packet ID needs to be prepended the the new payload data.
    if (!gmosMqttPacketPrependPacketId (packetBuffer, packetId)) {
        goto fail;
    }

    // Prepend the fixed header to the buffer. Note that the reserved
    // flags for this message type have a non-zero value.
    if (!gmosMqttPacketPrependFixedHeader (
        packetBuffer, GMOS_MQTT_PACKET_HEADER_TYPE_SUBSCRIBE | 0x02)) {
        goto fail;
    }
    return true;

    // Release allocated packet data on failure.
fail :
    gmosBufferReset (packetBuffer, 0);
    return false;
}

/*
 * Formats a new MQTT unsubscribe packet, placing it in the specified
 * GubbinsMOS buffer.
 */
bool gmosMqttPacketFormatUnsubscribe (gmosBuffer_t* packetBuffer,
    uint16_t packetId, const char* mqttTopicFilter)
{
    // Add the unsubscribe topic filter to the packet payload. Only a
    // single topic filter per request is supported.
    gmosBufferReset (packetBuffer, 0);
    if (!gmosMqttPacketAppendString (packetBuffer, mqttTopicFilter)) {
        goto fail;
    }

    // The packet ID needs to be prepended the the new payload data.
    if (!gmosMqttPacketPrependPacketId (packetBuffer, packetId)) {
        goto fail;
    }

    // Prepend the fixed header to the buffer. Note that the reserved
    // flags for this message type have a non-zero value.
    if (!gmosMqttPacketPrependFixedHeader (
        packetBuffer, GMOS_MQTT_PACKET_HEADER_TYPE_UNSUBSCRIBE | 0x02)) {
        goto fail;
    }
    return true;

    // Release allocated packet data on failure.
fail :
    gmosBufferReset (packetBuffer, 0);
    return false;
}

/*
 * Sets the duplicate flag for the specified packet buffer.
 */
bool gmosMqttPacketFormatSetDuplicate (gmosBuffer_t* packetBuffer)
{
    uint8_t headerByte;
    bool setOk = false;

    // Perform a read modify write on the fixed packet header to set
    // the duplicate flag
    if (gmosBufferRead (packetBuffer, 0, &headerByte, 1)) {
        if ((headerByte & GMOS_MQTT_PACKET_HEADER_TYPE_MASK) ==
            GMOS_MQTT_PACKET_HEADER_TYPE_PUBLISH) {
            headerByte |= GMOS_MQTT_PACKET_HEADER_FLAG_DUPLICATE;
            gmosBufferWrite (packetBuffer, 0, &headerByte, 1);
            setOk = true;
        }
    }
    return setOk;
}

/*
 * Parses the common fixed header of a received MQTT packet stored in
 * the specified packet buffer.
 */
bool gmosMqttPacketParseFixedHeader (gmosBuffer_t* packetBuffer,
    uint8_t* packetType, uint8_t* packetFlags, uint8_t* headerSize,
    uint32_t* remainingSize)
{
    bool readOk = true;
    uint_fast8_t i;
    uint32_t sizeByte;
    uint32_t parsedSize;
    uint8_t fixedHeader [2];

    // Attempt to read the first two bytes from the packet buffer. In
    // most cases this will be all that are required for decoding the
    // fixed header.
    readOk = gmosBufferRead (packetBuffer, 0, fixedHeader, 2);
    if (!readOk) {
        goto out;
    }

    // Extract the common header fields.
    *packetType = fixedHeader [0] & GMOS_MQTT_PACKET_HEADER_TYPE_MASK;
    *packetFlags = fixedHeader [0] & GMOS_MQTT_PACKET_HEADER_FLAG_MASK;
    if (*packetType == GMOS_MQTT_PACKET_HEADER_TYPE_MASK) {
        *packetType = GMOS_MQTT_PACKET_HEADER_TYPE_INVALID;
        goto out;
    }

    // Process the remaining length field. Normally this will only
    // require a single iteration using the existing fixed header
    // contents.
    parsedSize = 0;
    for (i = 0; i < 4; i++) {
        sizeByte = (uint32_t) (fixedHeader [1] & 0x7F);
        parsedSize += (sizeByte << (7 * i));
        if ((fixedHeader [1] & 0x80) == 0) {
            break;
        }

        // Check for a malformed header. An invalid packet type is used
        // to indicate a malformed packet.
        if (i >= 3) {
            *packetType = GMOS_MQTT_PACKET_HEADER_TYPE_INVALID;
            break;
        }

        // Read additional bytes from the buffer for larger lengths.
        // These overwrite the original length byte in the local array.
        readOk = gmosBufferRead (packetBuffer, i + 2, fixedHeader + 1, 1);
        if (!readOk) {
            goto out;
        }
    }

    // Update the header and remaining size return values.
    *headerSize = i + 2;
    *remainingSize = parsedSize;
out:
    return readOk;
}

/*
 * Parses the variable header of a received MQTT data packet stored in
 * the specified packet buffer in order to extract the data packet
 * flags, the optional packet ID and the payload offset.
 */
bool gmosMqttPacketParseDataPacket (gmosBuffer_t* packetBuffer,
    uint8_t* packetType, uint8_t* packetFlags, uint16_t* packetId,
    uint16_t* payloadOffset)
{
    bool readOk;
    uint8_t headerBytes [4];
    uint8_t idBytes [2];
    uint_fast16_t topicOffset;
    uint_fast16_t topicLength;
    uint_fast16_t idOffset;
    uint_fast16_t idVal;

    // Read the minimum number of header bytes which may include the
    // topic length.
    readOk = gmosBufferRead (packetBuffer, 0, headerBytes, 4);
    if (!readOk) {
        goto out;
    }
    *packetType = headerBytes [0] & GMOS_MQTT_PACKET_HEADER_TYPE_MASK;
    *packetFlags = headerBytes [0] & GMOS_MQTT_PACKET_HEADER_FLAG_MASK;
    if (*packetType != GMOS_MQTT_PACKET_HEADER_TYPE_PUBLISH) {
        *packetType = GMOS_MQTT_PACKET_HEADER_TYPE_INVALID;
        goto out;
    }

    // Align to the topic length bytes, taking into account the variable
    // length packet length field.
    for (topicOffset = 2; topicOffset <= 5; topicOffset++) {
        if ((headerBytes [1] & 0x80) == 0) {
            break;
        }
        readOk = gmosBufferRead (
            packetBuffer, topicOffset, &(headerBytes [1]), 3);
        if (!readOk) {
            goto out;
        }
    }
    if (topicOffset > 5) {
        *packetType = GMOS_MQTT_PACKET_HEADER_TYPE_INVALID;
        goto out;
    }

    // Determine the length of the topic string and use it to derive the
    // packet ID offset.
    topicLength = (uint_fast16_t) headerBytes [2];
    topicLength = (topicLength << 8) + (uint_fast16_t) headerBytes [3];
    idOffset = topicOffset + topicLength + 2;

    // Use default packet ID for QoS 0 packets.
    if ((headerBytes [0] & GMOS_MQTT_PACKET_HEADER_FLAG_QOS_MASK) ==
        GMOS_MQTT_PACKET_HEADER_FLAG_QOS0) {
        *packetId = 0;
        *payloadOffset = idOffset;
    }

    // Extract the packet ID bytes for QoS 1 and QoS 2 packets.
    else {
        readOk = gmosBufferRead (packetBuffer, idOffset, idBytes, 2);
        if (readOk) {
            idVal = (uint_fast16_t) idBytes [0];
            idVal = (idVal << 8) | (uint_fast16_t) idBytes [1];
            *packetId = idVal;
            *payloadOffset = idOffset + 2;
            if (*packetId == 0) {
                *packetType = GMOS_MQTT_PACKET_HEADER_TYPE_INVALID;
            }
        }
    }
out:
    return readOk;
}

/*
 * Parses the variable header containing the packet ID of a received
 * MQTT handshake packet stored in the specified packet buffer.
 */
bool gmosMqttPacketParseHandshakePacketId (gmosBuffer_t* packetBuffer,
    uint16_t* packetId)
{
    bool readOk;
    uint8_t idBytes [2];
    uint_fast16_t idVal;

    // Extract the packet ID bytes.
    readOk = gmosBufferRead (packetBuffer, 2, idBytes, 2);
    if (readOk) {
        idVal = (uint_fast16_t) idBytes [0];
        idVal = (idVal << 8) | (uint_fast16_t) idBytes [1];
        *packetId = idVal;
    }
    return readOk;
}

/*
 * Parses a received MQTT packet as a connection acknowledgement packet.
 * The packet remains in the buffer after parsing.
 */
bool gmosMqttPacketParseConnectAck (gmosBuffer_t* packetBuffer,
    bool* sessionPresent, uint8_t* connectStatus)
{
    bool readOk;
    uint8_t packetData [4];

    // Check that there is sufficient data in the buffer to parse the
    // fixed packet.
    readOk = gmosBufferRead (packetBuffer, 0, packetData, 4);
    if (!readOk) {
        return false;
    }

    // Check the expected fixed header fields.
    if ((packetData [0] != GMOS_MQTT_PACKET_HEADER_TYPE_CONNACK) ||
        (packetData [1] != 2)) {
        *sessionPresent = false;
        *connectStatus = GMOS_MQTT_PACKET_CONNECT_ACK_STATUS_MALFORMED_PACKET;
    }

    // Extract the variable length header fields.
    else {
        *sessionPresent = ((packetData [2] &
            GMOS_MQTT_PACKET_CONNECT_ACK_FLAG_SESSION_PRESENT) != 0) ?
            true : false;
        *connectStatus = packetData [3];
    }
    return true;
}

/*
 * Parses a received MQTT packet as a subscribe request acknowledgement
 * packet with a single payload byte. The packet remains in the buffer
 * after parsing.
 */
bool gmosMqttPacketParseSubscribeAck (gmosBuffer_t* packetBuffer,
    uint8_t* subscribeStatus)
{
    bool readOk;
    uint8_t packetData [5];

    // Check that there is sufficient data in the buffer to parse the
    // fixed packet.
    readOk = gmosBufferRead (packetBuffer, 0, packetData, 5);
    if (!readOk) {
        return false;
    }

    // Check the expected fixed header fields.
    if ((packetData [0] != GMOS_MQTT_PACKET_HEADER_TYPE_SUBACK) ||
        (packetData [1] != 3)) {
        *subscribeStatus = GMOS_MQTT_PACKET_SUBSCRIBE_ACK_STATUS_MALFORMED_PACKET;
    }

    // Extract the payload fields.
    else {
        *subscribeStatus = packetData [4];
    }
    return true;
}

/*
 * Parses the variable header containing the topic name of a received
 * MQTT data packet stored in the specified packet buffer, and attempts
 * to match it against the supplied MQTT topic filter string.
 */
bool gmosMqttPacketMatchDataPacketTopic (gmosBuffer_t* packetBuffer,
    const char* topicFilter, bool* matchOk)
{
    bool readOk;
    bool matchFound = false;
    uint8_t topicLenBytes [3];
    char topicMatchChars [16];
    uint_fast16_t topicOffset;
    uint_fast16_t topicLength;
    uint_fast16_t topicMatchSize;
    const char* filterPtr;
    uint_fast8_t i;

    // Align to the topic length bytes, taking into account the variable
    // length packet length field.
    for (topicOffset = 2; topicOffset <= 5; topicOffset++) {
        readOk = gmosBufferRead (
            packetBuffer, topicOffset - 1, topicLenBytes, 3);
        if (!readOk) {
            goto out;
        } else if ((topicLenBytes [0] & 0x80) == 0) {
            break;
        }
    }
    if (topicOffset > 5) {
        goto out;
    }

    // Determine the length of the topic string and update the topic
    // offset to the start of the string.
    topicLength = (uint_fast16_t) topicLenBytes [1];
    topicLength = (topicLength << 8) + (uint_fast16_t) topicLenBytes [2];
    topicOffset += 2;

    // Never match to a null topic filter.
    if (topicFilter == NULL) {
        goto out;
    }

    // Process the topic match in blocks.
    filterPtr = topicFilter;
    while (topicLength > 0) {

        // Read topic data into local block array for processing.
        topicMatchSize = sizeof (topicMatchChars);
        if (topicMatchSize > topicLength) {
            topicMatchSize = topicLength;
        }
        readOk = gmosBufferRead (packetBuffer, topicOffset,
            (uint8_t*) topicMatchChars, topicMatchSize);
        if (!readOk) {
            goto out;
        }

        // Process topic bytes according to the current filter byte.
        // Note that this assumes a valid topic filter string is in use.
        for (i = 0; i < topicMatchSize; i++) {

            // On encountering the multi-level wildcard, the rest of the
            // topic automatically matches.
            if (*filterPtr == '#') {
                matchFound = true;
                goto out;
            }

            // On encountering the single level wildcard, the contents
            // of the current hierarchy level up to the separator can
            // be ignored.
            else if (*filterPtr == '+') {
                if (topicMatchChars [i] == '/') {
                    if (*(filterPtr + 1) == '/') {
                        filterPtr += 2;
                    } else {
                        goto out;
                    }
                }
            }

            // Perform an exact match on the remaining characters.
            else if (*filterPtr == topicMatchChars [i]) {
                filterPtr += 1;
            } else {
                goto out;
            }
        }

        // Update buffer pointers for next block.
        topicOffset += topicMatchSize;
        topicLength -= topicMatchSize;
    }

    // A match on the last character of the topic is valid if it is
    // also the last character of the filter string.
    if (*filterPtr == '\0') {
        matchFound = true;
    }

    // A match is also valid if the filter string ends with an active
    // single level wildcard.
    else if ((*filterPtr == '+') && (*(filterPtr + 1) == '\0')) {
        matchFound = true;
    }

    // A match is also valid if the filter string finishes with '/#'
    // to support inclusion of the parent level of the filter hierarchy.
    else if ((*filterPtr == '/') && (*(filterPtr + 1) == '#') &&
        (*(filterPtr + 2) == '\0')) {
        matchFound = true;
    }

out:
    if (matchFound) {
        GMOS_LOG_FMT (LOG_DEBUG,
            "MQTT found topic match for '%s'.", topicFilter);
    }
    *matchOk = matchFound;
    return readOk;
}
