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
 * This header defines the public API for accessing an MQTT client
 * instance. It supports the version 3.1.1 MQTT protocol. The client
 * currently supports QoS levels 0, 1 and 2 for publishing messages to
 * the broker and QoS levels 0 and 1 for subscribing to messages from
 * the broker.
 */

#ifndef GMOS_MQTT_CLIENT_H
#define GMOS_MQTT_CLIENT_H

#include <stdint.h>
#include <stdbool.h>
#include "gmos-config.h"
#include "gmos-buffers.h"
#include "gmos-streams.h"
#include "gmos-scheduler.h"
#include "gmos-network.h"
#include "gmos-network-links.h"

// Provide forward reference type definitions.
typedef struct gmosMqttPublisher_t gmosMqttPublisher_t;
typedef struct gmosMqttSubscriber_t gmosMqttSubscriber_t;

/*
 * Specify the session information flags to be used by the MQTT client.
 */
typedef enum {
    GMOS_MQTT_CLIENT_SESSION_FLAG_CLEAN     = 0x01,
    GMOS_MQTT_CLIENT_SESSION_FLAG_RECONNECT = 0x02
} gmosMqttClientSessionFlags_t;

/**
 * Defines the GubbinsMOS MQTT client state that is used for managing a
 * single MQTT connection.
 */
typedef struct gmosMqttClient_t {

    // Specify the network link used by the MQTT connection.
    gmosNetworkLink_t* networkLink;

    // Specify the MQTT client ID that should be used.
    const char* mqttClientId;

    // Specify the MQTT will topic name.
    const char* willTopic;

    // Specify the MQTT will message payload data.
    const uint8_t* willMsgData;

    // Specify the user name for authentication purposes.
    const char* userName;

    // Specify the password data for authentication purposes.
    const char* password;

    // Specify a pointer to the start of the publisher list.
    gmosMqttPublisher_t* publisherList;

    // Specify a pointer to the start of the subscriber list.
    gmosMqttSubscriber_t* subscriberList;

    // Allocate the MQTT client worker task data structure.
    gmosTaskState_t mqttWorkerTask;

    // Allocate the MQTT receive data buffer.
    gmosBuffer_t rxDataBuffer;

    // Allocate the MQTT receive data packet stream.
    gmosStream_t rxDataStream;

    // Specify the timestamp for connection and keep alive timeouts.
    uint32_t timeout;

    // Specify the MQTT will message payload size.
    uint16_t willMsgSize;

    // Specify the transmitted message packet ID counter.
    uint16_t packetIdCounter;

    // Specify the current packet ID for subscriber QoS 1 messages.
    uint16_t subQueuePacketId;

    // Store the current state for the MQTT client worker task.
    uint8_t mqttClientState;

    // Store the current state for the MQTT keep alive ping handler.
    uint8_t keepAliveState;

    // Hold session state information flags.
    uint8_t sessionFlags;

    // Store the current state for the MQTT subscriber packet queue.
    uint8_t subQueueState;

} gmosMqttClient_t;

/**
 * Initialise the MQTT client on startup, using the specified network
 * link for the connection to the MQTT broker.
 * @param mqttClient This is the MQTT client instance that is to be
 *     initialised.
 * @param networkLink This is a pointer to the network link data
 *     structure that should be used for connecting to the MQTT broker.
 * @param mqttClientId This is a pointer to a string which contains the
 *     MQTT client ID that should be used during the connection request.
 *     It must remain valid for the lifetime of the MQTT client.
 * @return Returns a boolean value which will be set to 'true' if the
 *     MQTT client was successfully initialised and 'false' otherwise.
 */
bool gmosMqttClientInit (gmosMqttClient_t* mqttClient,
    gmosNetworkLink_t* networkLink, const char* mqttClientId);

/**
 * Sets the login credentials to be used whenever the MQTT client
 * connects to the broker. This includes the client user name and
 * optional password.
 * @param mqttClient This is the MQTT client instance for which the
 *     login credentials are being set.
 * @param userName This is a pointer to a string which contains the user
 *     name to be included when connecting to the broker. The contents
 *     of the string must remain valid for the lifetime of the MQTT
 *     client instance, or until alternate credentials are assigned. A
 *     null reference may be used to disable the use of login
 *     credentials.
 * @param password This is a pointer to a string which contains the
 *     password to be used when connecting to the broker. The contents
 *     of the string must remain valid for the lifetime of the MQTT
 *     client instance, or until alternate credentials are assigned. A
 *     null reference may be used to disable the use of the login
 *     password.
 */
void gmosMqttClientSetLoginCredentials (gmosMqttClient_t* mqttClient,
    const char* userName, const char* password);

/**
 * Sets the will message to be included whenever the MQTT client
 * connects to the broker. This includes the associated MQTT topic to be
 * used when sending the will message. By default the will message
 * should be sent with a QoS level of 1 and with the message retain flag
 * not set.
 * @param mqttClient This is the MQTT client instance for which the
 *     will message is being set.
 * @param willTopic This is a pointer to a string which contains the
 *     MQTT topic to be used when sending the will message. The contents
 *     of the string must remain valid for the lifetime of the MQTT
 *     client, or until alternate will message settings are assigned. A
 *     null reference may be used to disable the inclusion of the will
 *     message when connecting to the broker.
 * @param willMsgData This is a pointer to a byte array containing the
 *     MQTT will message. The contents of the byte array must remain
 *     valid for the lifetime of the MQTT client, or until alternate
 *     will message settings are assigned. A null reference may be used
 *     to disable the inclusion of the will message when connecting to
 *     the broker.
 * @param willMsgSize This is the size of the MQTT will message provided
 *     in the associated byte array. A size of zero may be used to
 *     disable the inclusion of the will message when connecting to the
 *     broker.
 */
void gmosMqttClientSetWillMessage (gmosMqttClient_t* mqttClient,
    const char* willTopic, const uint8_t* willMsgData,
    uint16_t willMsgSize);

/**
 * Initiates an MQTT client connection request. This will establish the
 * network link connection and then carry out the MQTT connection
 * handshake.
 * @param mqttClient This is the MQTT client instance that is to be
 *     connected to the MQTT broker.
 * @return Returns a network status value which indicates that the
 *     connection process was successfully initiated or the reason for
 *     failure. A retry status may be used to indicate that a connection
 *     can not be established at this time and should be retried later.
 */
gmosNetworkStatus_t gmosMqttClientConnect (
    gmosMqttClient_t* mqttClient);

/**
 * Initiates an MQTT client disconnection request. This will issue the
 * MQTT disconnection request and then close the underlying network
 * link connection.
 * @param mqttClient This is the MQTT client instance that is to be
 *     disconnected from the MQTT broker.
 * @return Returns a network status value which indicates that the
 *     disconnection process was successfully initiated or the reason
 *     for failure. A retry status may be used to indicate that a
 *     connection can not be disconnected at this time and should be
 *     retried later.
 */
gmosNetworkStatus_t gmosMqttClientDisconnect (
    gmosMqttClient_t* mqttClient);

/**
 * Gets the current MQTT client status in order to determine if it is
 * currently connected to the MQTT broker.
 * @param mqttClient This is the MQTT client instance that is to be
 *     checked for the current connection status.
 * @return Returns a network status value which indicates whether the
 *     MQTT client is currently connected or not connected to the broker
 *     A retry status is used to indicate that the client is currently
 *     in a transitional state and the request should be retried later.
 */
gmosNetworkStatus_t gmosMqttClientGetStatus (
    gmosMqttClient_t* mqttClient);

#endif // GMOS_MQTT_CLIENT_H
