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
 * This file implements the main MQTT client state machine. It supports
 * the version 3.1.1 MQTT protocol.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-buffers.h"
#include "gmos-streams.h"
#include "gmos-scheduler.h"
#include "gmos-network-links.h"
#include "gmos-mqtt-config.h"
#include "gmos-mqtt-client.h"
#include "gmos-mqtt-packet.h"
#include "gmos-mqtt-keepalive.h"
#include "gmos-mqtt-publish.h"
#include "gmos-mqtt-subscribe.h"

/*
 * Specify the retry interval used by the state machine.
 */
#define GMOS_MQTT_CLIENT_TASK_RETRY \
    (GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (10)))

/*
 * Specify the state space to be used for the MQTT client.
 */
typedef enum {
    GMOS_MQTT_CLIENT_STATE_UNCONNECTED,
    GMOS_MQTT_CLIENT_STATE_CONNECTING_LINK,
    GMOS_MQTT_CLIENT_STATE_CONNECT_REQUEST,
    GMOS_MQTT_CLIENT_STATE_CONNECT_WAIT,
    GMOS_MQTT_CLIENT_STATE_CONNECT_DONE,
    GMOS_MQTT_CLIENT_STATE_CONNECTED,
    GMOS_MQTT_CLIENT_STATE_LINK_ERROR,
    GMOS_MQTT_CLIENT_STATE_DISCONNECT_LINK,
    GMOS_MQTT_CLIENT_STATE_CLOSING_LINK,
    GMOS_MQTT_CLIENT_STATE_RECONNECT_LINK,
    GMOS_MQTT_CLIENT_STATE_FAILED
} gmosMqttClientState_t;

/*
 * Poll for new receive data and place it in the receive data buffer.
 */
static gmosNetworkStatus_t gmosMqttClientReceiveData (
    gmosMqttClient_t* mqttClient)
{
    gmosNetworkStatus_t networkStatus;
    gmosBuffer_t payload = GMOS_BUFFER_INIT ();

    // Attempt to read data from the network link.
    networkStatus = gmosNetworkLinkReceive (
        mqttClient->networkLink, &payload);

    // On a successful read, attempt to append the received data to the
    // local buffer.
    if (networkStatus == GMOS_NETWORK_STATUS_SUCCESS) {
        if (!gmosBufferConcatenate (&(mqttClient->rxDataBuffer),
            &payload, &(mqttClient->rxDataBuffer))) {
            networkStatus = GMOS_NETWORK_STATUS_DATA_LOSS;
            gmosBufferReset (&payload, 0);
        }
    }
    return networkStatus;
}

/*
 * Check for received messages and call the appropriate message handlers
 * for subsequent processing.
 */
static inline gmosNetworkStatus_t gmosMqttClientReceiveDispatch (
    gmosMqttClient_t* mqttClient)
{
    gmosNetworkStatus_t networkStatus;
    uint8_t packetType = GMOS_MQTT_PACKET_HEADER_TYPE_INVALID;
    uint8_t packetFlags;
    uint8_t headerSize;
    uint32_t remainingSize;
    uint32_t packetSize = 0;
    uint_fast16_t bufferSize;

    // Attempt to receive any outstanding data from the network link.
    networkStatus = gmosMqttClientReceiveData (mqttClient);
    if ((networkStatus != GMOS_NETWORK_STATUS_SUCCESS) &&
        (networkStatus != GMOS_NETWORK_STATUS_RETRY)) {
        goto out;
    }

    // For a well formed packet, there must be sufficient data in the
    // receive buffer to allow the fixed header to be parsed.
    if (!gmosMqttPacketParseFixedHeader (&(mqttClient->rxDataBuffer),
        &packetType, &packetFlags, &headerSize, &remainingSize)) {
        networkStatus = GMOS_NETWORK_STATUS_RETRY;
        goto out;
    }

    // A malformed fixed header is indicated by an invalid packet type.
    if (packetType == 0xFF) {
        networkStatus = GMOS_NETWORK_STATUS_PROTOCOL_ERROR;
        goto out;
    }

    // Check that the entire packet is present in the buffer for
    // subsequent processing. The maximum packet size is limited by the
    // implementation, so there needs to be a check for packets that are
    // too large and can never be processed. These are treated as a high
    // level protocol error.
    packetSize = headerSize + remainingSize;
    bufferSize = gmosBufferGetSize (&(mqttClient->rxDataBuffer));
    if (packetSize > GMOS_CONFIG_MQTT_MAX_PACKET_SIZE) {
        networkStatus = GMOS_NETWORK_STATUS_PROTOCOL_ERROR;
        goto out;
    } else if (packetSize > bufferSize) {
        networkStatus = GMOS_NETWORK_STATUS_RETRY;
        goto out;
    }

    // Dispatch the packet to the appropriate control packet handler.
    switch (packetType) {
        case GMOS_MQTT_PACKET_HEADER_TYPE_PINGRESP :
            networkStatus = gmosMqttClientKeepAliveRxPacket (
                mqttClient, packetFlags, headerSize, remainingSize);
            break;

        case GMOS_MQTT_PACKET_HEADER_TYPE_PUBACK :
        case GMOS_MQTT_PACKET_HEADER_TYPE_PUBREC :
        case GMOS_MQTT_PACKET_HEADER_TYPE_PUBCOMP :
            networkStatus = gmosMqttClientPublishRxPacket (
                mqttClient, packetType, packetFlags, headerSize,
                remainingSize);
            break;

        case GMOS_MQTT_PACKET_HEADER_TYPE_SUBACK :
        case GMOS_MQTT_PACKET_HEADER_TYPE_UNSUBACK :
            networkStatus = gmosMqttClientSubscribeRxPacket (
                mqttClient, packetType, packetFlags, headerSize,
                remainingSize);
            break;

        case GMOS_MQTT_PACKET_HEADER_TYPE_PUBLISH :
            networkStatus = gmosMqttClientSubscribeRxDataFeed (
                mqttClient, headerSize, remainingSize);
            break;

        default :
            networkStatus = GMOS_NETWORK_STATUS_PROTOCOL_ERROR;
            break;
    }

    // Discard the packet after processing in the dispatch handlers.
    if (networkStatus != GMOS_NETWORK_STATUS_RETRY) {
        gmosBufferRebase (
            &(mqttClient->rxDataBuffer), bufferSize - packetSize);
    }

    // Provide debug information about failed packets.
out:
    if ((networkStatus != GMOS_NETWORK_STATUS_SUCCESS) &&
        (networkStatus != GMOS_NETWORK_STATUS_RETRY)) {
        GMOS_LOG_FMT (LOG_WARNING,
            "MQTT received bad packet (type 0x%02X, size %d, status %d).",
            packetType, packetSize, networkStatus);
    }
    return networkStatus;
}

/*
 * Send the MQTT connection request packet.
 */
static inline gmosNetworkStatus_t gmosMqttClientSendConnectRequest (
    gmosMqttClient_t* mqttClient)
{
    gmosBuffer_t packetBuffer = GMOS_BUFFER_INIT ();
    gmosNetworkStatus_t networkStatus;
    bool cleanSession = ((mqttClient->sessionFlags &
        GMOS_MQTT_CLIENT_SESSION_FLAG_CLEAN) != 0) ? true : false;

    // Attempt to format the MQTT connection request.
    if (!gmosMqttPacketFormatConnect (&packetBuffer,
        mqttClient->mqttClientId, cleanSession, mqttClient->willTopic,
        mqttClient->willMsgData, mqttClient->willMsgSize,
        mqttClient->userName, mqttClient->password)) {
        networkStatus = GMOS_NETWORK_STATUS_RETRY;
        goto out;
    }

    // Attempt to send the MQTT connection request over the network
    // link.
    networkStatus = gmosNetworkLinkSend (
        mqttClient->networkLink, &packetBuffer);
    if (networkStatus != GMOS_NETWORK_STATUS_SUCCESS) {
        gmosBufferReset (&packetBuffer, 0);
    }
out:
    return networkStatus;
}

/*
 * Check for an MQTT connection acknowledgement packet.
 */
static inline gmosNetworkStatus_t gmosMqttClientCheckConnectAck (
    gmosMqttClient_t* mqttClient)
{
    gmosNetworkStatus_t networkStatus;
    uint_fast16_t bufferSize;
    uint8_t connectStatus;
    bool sessionPresent;

    // Transfer any outstanding data to the local receive buffer.
    // The acknowledgment packet size of four octets is fixed.
    bufferSize = gmosBufferGetSize (&(mqttClient->rxDataBuffer));
    while (bufferSize < 4) {
        networkStatus = gmosMqttClientReceiveData (mqttClient);
        if (networkStatus != GMOS_NETWORK_STATUS_SUCCESS) {
            return networkStatus;
        }
        bufferSize = gmosBufferGetSize (&(mqttClient->rxDataBuffer));
    }

    // Parse the first four octets as an MQTT connection
    // acknowledgement.
    gmosMqttPacketParseConnectAck (&(mqttClient->rxDataBuffer),
        &sessionPresent, &connectStatus);
    GMOS_LOG_FMT (LOG_DEBUG,
        "MQTT connected with status %d and session present flag %d.",
        connectStatus, sessionPresent);

    // Translate the connection status value to an appropriate network
    // status value.
    switch (connectStatus) {
        case GMOS_MQTT_PACKET_CONNECT_ACK_STATUS_ACCEPTED :
            networkStatus = GMOS_NETWORK_STATUS_SUCCESS;
            break;
        case GMOS_MQTT_PACKET_CONNECT_ACK_STATUS_UNSUPPORTED_VERSION :
        case GMOS_MQTT_PACKET_CONNECT_ACK_STATUS_SERVER_UNAVAILABLE :
            networkStatus = GMOS_NETWORK_STATUS_UNSUPPORTED;
            break;
        case GMOS_MQTT_PACKET_CONNECT_ACK_STATUS_CLIENT_ID_REJECTED :
        case GMOS_MQTT_PACKET_CONNECT_ACK_STATUS_USER_INVALID :
        case GMOS_MQTT_PACKET_CONNECT_ACK_STATUS_NOT_AUTHORIZED :
            networkStatus = GMOS_NETWORK_STATUS_NOT_VALID;
            break;
        default :
            networkStatus = GMOS_NETWORK_STATUS_PROTOCOL_ERROR;
            break;
    }

    // Update the local clean session flag according to the reported
    // session present state.
    if (sessionPresent) {
        mqttClient->sessionFlags &= ~GMOS_MQTT_CLIENT_SESSION_FLAG_CLEAN;
    } else {
        mqttClient->sessionFlags |= GMOS_MQTT_CLIENT_SESSION_FLAG_CLEAN;
    }

    // Remove the packet from the head of the receive data buffer.
    gmosBufferRebase (&(mqttClient->rxDataBuffer), bufferSize - 4);
    return networkStatus;
}

/*
 * Send the MQTT connection request packet.
 */
static gmosNetworkStatus_t gmosMqttClientSendDisconnectRequest (
    gmosMqttClient_t* mqttClient)
{
    gmosBuffer_t packetBuffer = GMOS_BUFFER_INIT ();
    gmosNetworkStatus_t networkStatus;

    // Attempt to format the MQTT disconnect request.
    if (!gmosMqttPacketFormatControl (&packetBuffer,
        GMOS_MQTT_PACKET_HEADER_TYPE_DISCONNECT)) {
        networkStatus = GMOS_NETWORK_STATUS_RETRY;
        goto out;
    }

    // Send the MQTT disconnect request over the network link.
    networkStatus = gmosNetworkLinkSend (
        mqttClient->networkLink, &packetBuffer);
    if (networkStatus != GMOS_NETWORK_STATUS_SUCCESS) {
        gmosBufferReset (&packetBuffer, 0);
    }
out:
    return networkStatus;
}

/*
 * Flush buffered data on disconnection.
 */
static inline void gmosMqttClientFlushBuffers (
    gmosMqttClient_t* mqttClient)
{
    gmosBuffer_t packetBuffer = GMOS_BUFFER_INIT ();
    gmosBufferReset (&(mqttClient->rxDataBuffer), 0);
    while (gmosStreamAcceptBuffer (
        &(mqttClient->rxDataStream), &packetBuffer)) {
        gmosBufferReset (&packetBuffer, 0);
    }
}

/*
 * Implement the main MQTT client state machine task.
 */
static inline gmosTaskStatus_t gmosMqttClientWorkerTaskFn (
    gmosMqttClient_t* mqttClient)
{
    gmosNetworkStatus_t networkStatus;
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    gmosMqttClientState_t nextState = mqttClient->mqttClientState;
    uint32_t currentTimer = gmosPalGetTimer ();
    int32_t delay = (int32_t) (mqttClient->timeout - currentTimer);

    // Implement the main MQTT client state machine.
    switch (mqttClient->mqttClientState) {

        // Wait in the unconnected state for a connection request.
        case GMOS_MQTT_CLIENT_STATE_UNCONNECTED :
            taskStatus = GMOS_TASK_SUSPEND;
            break;

        // Wait for the underlying network link to connect.
        case GMOS_MQTT_CLIENT_STATE_CONNECTING_LINK :
            networkStatus = gmosNetworkLinkMonitor (mqttClient->networkLink);
            if (networkStatus == GMOS_NETWORK_STATUS_CONNECTED) {
                nextState = GMOS_MQTT_CLIENT_STATE_CONNECT_REQUEST;
            } else if (networkStatus == GMOS_NETWORK_STATUS_RETRY) {
                taskStatus = GMOS_MQTT_CLIENT_TASK_RETRY;
            } else {
                GMOS_LOG (LOG_DEBUG,
                    "MQTT transport link connection failed.");
                nextState = GMOS_MQTT_CLIENT_STATE_FAILED;
            }
            break;

        // Issue the MQTT connection request.
        case GMOS_MQTT_CLIENT_STATE_CONNECT_REQUEST :
            networkStatus = gmosMqttClientSendConnectRequest (mqttClient);
            if (networkStatus == GMOS_NETWORK_STATUS_SUCCESS) {
                GMOS_LOG (LOG_DEBUG, "MQTT issued connection request.");
                mqttClient->timeout = currentTimer +
                    GMOS_MS_TO_TICKS (GMOS_CONFIG_MQTT_TIMEOUT_PERIOD * 1000);
                nextState = GMOS_MQTT_CLIENT_STATE_CONNECT_WAIT;
            } else if (networkStatus == GMOS_NETWORK_STATUS_RETRY) {
                taskStatus = GMOS_MQTT_CLIENT_TASK_RETRY;
            } else {
                GMOS_LOG (LOG_DEBUG,
                    "MQTT connection request failed.");
                nextState = GMOS_MQTT_CLIENT_STATE_FAILED;
            }
            break;

        // Wait for MQTT connection response.
        case GMOS_MQTT_CLIENT_STATE_CONNECT_WAIT :
            networkStatus = gmosMqttClientCheckConnectAck (mqttClient);
            if (networkStatus == GMOS_NETWORK_STATUS_SUCCESS) {
                nextState = GMOS_MQTT_CLIENT_STATE_CONNECT_DONE;
            } else if (networkStatus != GMOS_NETWORK_STATUS_RETRY) {
                GMOS_LOG (LOG_DEBUG,
                    "MQTT connection acknowledgement failed.");
                nextState = GMOS_MQTT_CLIENT_STATE_FAILED;
            } else if (delay <= 0) {
                GMOS_LOG (LOG_DEBUG,
                    "MQTT connection acknowledgement timeout.");
                nextState = GMOS_MQTT_CLIENT_STATE_FAILED;
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (delay);
            }
            break;

        // On completion of the connection process, initialise the
        // various client message handler state machines.
        case GMOS_MQTT_CLIENT_STATE_CONNECT_DONE :
            GMOS_LOG (LOG_DEBUG, "MQTT established connection.");
            gmosMqttClientKeepAliveStartup (mqttClient);
            gmosMqttClientPublishStartup (mqttClient);
            gmosMqttClientSubscribeStartup (mqttClient);
            nextState = GMOS_MQTT_CLIENT_STATE_CONNECTED;
            break;

        // While connected, run the main message dispatch handler and
        // the keep alive, publisher and subscriber state machines.
        // Message dispatch can be suspended once no further data is
        // available.
        case GMOS_MQTT_CLIENT_STATE_CONNECTED :
            networkStatus = gmosMqttClientReceiveDispatch (mqttClient);
            if (networkStatus == GMOS_NETWORK_STATUS_RETRY) {
                taskStatus = GMOS_TASK_SUSPEND;
            } else if (networkStatus != GMOS_NETWORK_STATUS_SUCCESS) {
                nextState = GMOS_MQTT_CLIENT_STATE_LINK_ERROR;
                break;
            }

            // Process loss of connection
            taskStatus = gmosSchedulerPrioritise (taskStatus,
                gmosMqttClientKeepAliveTick (mqttClient));
            if (!gmosMqttClientKeepAliveOk (mqttClient)) {
                GMOS_LOG (LOG_INFO, "MQTT keep alive timed out.");
                nextState = GMOS_MQTT_CLIENT_STATE_LINK_ERROR;
                break;
            }

            // Process publish and subscribe state machines.
            taskStatus = gmosSchedulerPrioritise (taskStatus,
                gmosMqttClientPublishTick (mqttClient));
            taskStatus = gmosSchedulerPrioritise (taskStatus,
                gmosMqttClientSubscribeTick (mqttClient));
            break;

        // Process link error conditions. These should cause the client
        // to immediately close the connection and then follow the
        // appropriate reconnection policy.
        case GMOS_MQTT_CLIENT_STATE_LINK_ERROR :
            networkStatus = gmosMqttClientSendDisconnectRequest (mqttClient);
            if (networkStatus == GMOS_NETWORK_STATUS_RETRY) {
                taskStatus = GMOS_MQTT_CLIENT_TASK_RETRY;
            } else {
                mqttClient->sessionFlags =
                    GMOS_CONFIG_MQTT_LINK_ERROR_AUTO_RECONNECT ?
                    GMOS_MQTT_CLIENT_SESSION_FLAG_RECONNECT :
                    GMOS_MQTT_CLIENT_SESSION_FLAG_CLEAN;
                nextState = GMOS_MQTT_CLIENT_STATE_DISCONNECT_LINK;
            }
            break;

        // Disconnect the underlying network link.
        case GMOS_MQTT_CLIENT_STATE_DISCONNECT_LINK :
            networkStatus = gmosNetworkLinkDisconnect (
                mqttClient->networkLink);
            if (networkStatus == GMOS_NETWORK_STATUS_RETRY) {
                taskStatus = GMOS_MQTT_CLIENT_TASK_RETRY;
            } else {
                GMOS_LOG_FMT (LOG_DEBUG,
                    "MQTT network link close request with status %d.",
                    networkStatus);
                nextState = GMOS_MQTT_CLIENT_STATE_CLOSING_LINK;
            }
            break;

        // Wait for the underlying network link to close. Then reset the
        // MQTT client state to its default values.
        case GMOS_MQTT_CLIENT_STATE_CLOSING_LINK :
            networkStatus = gmosNetworkLinkMonitor (mqttClient->networkLink);
            GMOS_LOG_FMT (LOG_DEBUG,
                "MQTT network link disconnection status %d.",
                networkStatus);
            if (networkStatus == GMOS_NETWORK_STATUS_NOT_CONNECTED) {
                gmosMqttClientFlushBuffers (mqttClient);
                if ((mqttClient->sessionFlags &
                    GMOS_MQTT_CLIENT_SESSION_FLAG_RECONNECT) != 0) {
                    nextState = GMOS_MQTT_CLIENT_STATE_RECONNECT_LINK;
                    taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (1000));
                } else {
                    GMOS_LOG (LOG_DEBUG,
                        "TODO: MQTT publisher and subscriber teardown on disconnection.");
                    nextState = GMOS_MQTT_CLIENT_STATE_UNCONNECTED;
                    mqttClient->sessionFlags = GMOS_MQTT_CLIENT_SESSION_FLAG_CLEAN;
                    taskStatus = GMOS_TASK_RUN_IMMEDIATE;
                }
            } else if (networkStatus == GMOS_NETWORK_STATUS_RETRY) {
                taskStatus = GMOS_MQTT_CLIENT_TASK_RETRY;
            } else {
                GMOS_LOG (LOG_DEBUG,
                    "MQTT transport link disconnection failed.");
                nextState = GMOS_MQTT_CLIENT_STATE_FAILED;
            }
            break;

        // Automatically attempt to reconnect the link after unexpected
        // disconnection.
        case GMOS_MQTT_CLIENT_STATE_RECONNECT_LINK :
            networkStatus = gmosNetworkLinkConnect (
                mqttClient->networkLink);
            if (networkStatus == GMOS_NETWORK_STATUS_SUCCESS) {
                nextState = GMOS_MQTT_CLIENT_STATE_CONNECTING_LINK;
                taskStatus = GMOS_TASK_RUN_IMMEDIATE;
            } else if (networkStatus == GMOS_NETWORK_STATUS_RETRY) {
                taskStatus = GMOS_MQTT_CLIENT_TASK_RETRY;
            } else {
                GMOS_LOG (LOG_DEBUG,
                    "MQTT transport link reconnection failed.");
                nextState = GMOS_MQTT_CLIENT_STATE_FAILED;
            }
            break;

        // Suspend further processing in the failure state.
        default :
            GMOS_LOG (LOG_WARNING,
                "TODO: MQTT client recovery from failure.");
            taskStatus = GMOS_TASK_SUSPEND;
            break;
    }
    mqttClient->mqttClientState = nextState;
    return taskStatus;
}

/*
 * Provide the MQTT worker task definition.
 */
GMOS_TASK_DEFINITION (gmosMqttClientWorkerTask,
    gmosMqttClientWorkerTaskFn, gmosMqttClient_t);

/*
 * Initialise the MQTT client on startup, using the specified network
 * link for the connection to the MQTT broker.
 */
bool gmosMqttClientInit (gmosMqttClient_t* mqttClient,
    gmosNetworkLink_t* networkLink, const char* mqttClientId)
{
    // Initialise the MQTT client state.
    mqttClient->networkLink = networkLink;
    mqttClient->mqttClientId = mqttClientId;
    mqttClient->mqttClientState = GMOS_MQTT_CLIENT_STATE_UNCONNECTED;
    mqttClient->sessionFlags = GMOS_MQTT_CLIENT_SESSION_FLAG_CLEAN;
    mqttClient->willTopic = NULL;
    mqttClient->willMsgData = NULL;
    mqttClient->willMsgSize = 0;
    mqttClient->userName = NULL;
    mqttClient->password = NULL;
    mqttClient->publisherList = NULL;
    mqttClient->subscriberList = NULL;
    gmosBufferInit (&(mqttClient->rxDataBuffer));
    gmosStreamInit (&(mqttClient->rxDataStream),
        NULL, GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE);

    // Select a random non-zero counter value as the starting point for
    // packet handshake transaction IDs.
    do {
        gmosPalGetRandomBytes (
            (uint8_t*) &(mqttClient->packetIdCounter), 2);
    } while (mqttClient->packetIdCounter == 0);

    // Set up the network link with the worker task as the consumer.
    gmosNetworkLinkSetConsumerTask (
        mqttClient->networkLink, &(mqttClient->mqttWorkerTask));

    // Start up the MQTT client worker task.
    gmosMqttClientWorkerTask_start (
        &(mqttClient->mqttWorkerTask), mqttClient,
        GMOS_TASK_NAME_WRAPPER ("MQTT Client"));
    return true;
}

/*
 * Sets the login credentials to be used whenever the MQTT client
 * connects to the broker. This includes the client user name and
 * optional password.
 */
void gmosMqttClientSetLoginCredentials (gmosMqttClient_t* mqttClient,
    const char* userName, const char* password)
{
    if (userName == NULL) {
        mqttClient->userName = NULL;
        mqttClient->password = NULL;
    } else {
        mqttClient->userName = userName;
        mqttClient->password = password;
    }
}

/*
 * Sets the will message to be included whenever the MQTT client
 * connects to the broker.
 */
void gmosMqttClientSetWillMessage (gmosMqttClient_t* mqttClient,
    const char* willTopic, const uint8_t* willMsgData,
    uint16_t willMsgSize)
{
    if ((willTopic == NULL) || (willMsgData == NULL) || (willMsgSize == 0)) {
        mqttClient->willTopic = NULL;
        mqttClient->willMsgData = NULL;
        mqttClient->willMsgSize = 0;
    } else {
        mqttClient->willTopic = willTopic;
        mqttClient->willMsgData = willMsgData;
        mqttClient->willMsgSize = willMsgSize;
    }
}

/*
 * Initiates an MQTT client connection request. This will establish the
 * network link connection and then carry out the MQTT connection
 * handshake.
 */
gmosNetworkStatus_t gmosMqttClientConnect (
    gmosMqttClient_t* mqttClient)
{
    gmosNetworkStatus_t networkStatus;

    // Initiate a network link connection request from the unconnected
    // client state.
    switch (mqttClient->mqttClientState) {
        case GMOS_MQTT_CLIENT_STATE_UNCONNECTED :
            networkStatus = gmosNetworkLinkConnect (
                mqttClient->networkLink);
            break;
        case GMOS_MQTT_CLIENT_STATE_CONNECTED :
            networkStatus = GMOS_NETWORK_STATUS_CONNECTED;
            break;
        default :
            networkStatus = GMOS_NETWORK_STATUS_RETRY;
            break;
    }

    // Set the MQTT client state machine to wait for the link connection
    // process to complete.
    if (networkStatus == GMOS_NETWORK_STATUS_SUCCESS) {
        mqttClient->mqttClientState =
            GMOS_MQTT_CLIENT_STATE_CONNECTING_LINK;
        gmosSchedulerTaskResume (&(mqttClient->mqttWorkerTask));
    }
    return networkStatus;
}

/*
 * Initiates an MQTT client disconnection request. This will issue the
 * MQTT disconnection request and then close the underlying network
 * link connection.
 */
gmosNetworkStatus_t gmosMqttClientDisconnect (
    gmosMqttClient_t* mqttClient)
{
    gmosNetworkStatus_t networkStatus;

    // Initiate an MQTT clean disconnection from the connected client
    // state. Indicate success if the client is already disconnected.
    // Also abandon any automatic reconnection requests that may be in
    // progress.
    switch (mqttClient->mqttClientState) {
        case GMOS_MQTT_CLIENT_STATE_CONNECTED :
            networkStatus = gmosMqttClientSendDisconnectRequest (mqttClient);
            if (networkStatus != GMOS_NETWORK_STATUS_RETRY) {
                mqttClient->mqttClientState =
                    GMOS_MQTT_CLIENT_STATE_DISCONNECT_LINK;
                mqttClient->sessionFlags =
                    GMOS_MQTT_CLIENT_SESSION_FLAG_CLEAN;
                gmosSchedulerTaskResume (&(mqttClient->mqttWorkerTask));
            }
            break;
        case GMOS_MQTT_CLIENT_STATE_UNCONNECTED :
            networkStatus = GMOS_NETWORK_STATUS_SUCCESS;
            break;
        case GMOS_MQTT_CLIENT_STATE_RECONNECT_LINK :
            mqttClient->mqttClientState =
                GMOS_MQTT_CLIENT_STATE_UNCONNECTED;
            networkStatus = GMOS_NETWORK_STATUS_SUCCESS;
            break;
        default :
            networkStatus = GMOS_NETWORK_STATUS_RETRY;
            break;
    }
    return networkStatus;
}

/*
 * Gets the current MQTT client status in order to determine if it is
 * currently connected to the MQTT broker.
 */
gmosNetworkStatus_t gmosMqttClientGetStatus (
    gmosMqttClient_t* mqttClient)
{
    gmosNetworkStatus_t networkStatus;

    // Select the appropriate network status give the current MQTT
    // client state.
    switch (mqttClient->mqttClientState) {
        case GMOS_MQTT_CLIENT_STATE_CONNECTED :
            networkStatus = GMOS_NETWORK_STATUS_CONNECTED;
            break;
        case GMOS_MQTT_CLIENT_STATE_UNCONNECTED :
            networkStatus = GMOS_NETWORK_STATUS_NOT_CONNECTED;
            break;
        default :
            networkStatus = GMOS_NETWORK_STATUS_RETRY;
            break;
    }
    return networkStatus;
}
