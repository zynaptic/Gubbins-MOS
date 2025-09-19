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
 * This file implements the MQTT client publishing state machine. It
 * supports the version 3.1.1 MQTT protocol.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-mqtt-client.h"
#include "gmos-mqtt-packet.h"
#include "gmos-mqtt-publish.h"

/*
 * Specify the retry interval used by the state machine.
 */
#define GMOS_MQTT_CLIENT_PUB_TASK_RETRY \
    (GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (10)))

/*
 * Specify the internal state space for the MQTT publisher instances.
 */
typedef enum {
    GMOS_MQTT_CLIENT_PUB_STATE_RESET,
    GMOS_MQTT_CLIENT_PUB_STATE_IDLE,
    GMOS_MQTT_CLIENT_PUB_STATE_QUEUED_QOS0,
    GMOS_MQTT_CLIENT_PUB_STATE_QUEUED_QOS1,
    GMOS_MQTT_CLIENT_PUB_STATE_QUEUED_QOS2,
    GMOS_MQTT_CLIENT_PUB_STATE_ACK_WAIT_QOS1,
    GMOS_MQTT_CLIENT_PUB_STATE_REC_WAIT_QOS2,
    GMOS_MQTT_CLIENT_PUB_STATE_REL_SEND_QOS2,
    GMOS_MQTT_CLIENT_PUB_STATE_COMP_WAIT_QOS2,
    GMOS_MQTT_CLIENT_PUB_STATE_COMPLETE
} gmosMqttClientPubState_t;

/*
 * Implement startup processing for a single publishing instance.
 */
static inline void gmosMqttClientPublisherStartup (
    gmosMqttClient_t* mqttClient, gmosMqttPublisher_t* mqttPublisher)
{
    uint_fast8_t nextState = mqttPublisher->pubState;

    // This resets the publisher instance for a clean connection.
    if ((mqttClient->sessionFlags &
        GMOS_MQTT_CLIENT_SESSION_FLAG_CLEAN) != 0) {
        GMOS_LOG (LOG_DEBUG, "MQTT client publisher clean startup.");
        mqttPublisher->packetId = 0;
        mqttPublisher->pubStatus = GMOS_NETWORK_STATUS_NOT_VALID;
        gmosBufferReset (&(mqttPublisher->pubMessage), 0);
        nextState = GMOS_MQTT_CLIENT_PUB_STATE_IDLE;
    }

    // For resumed connections, adjust the current transaction state
    // in order to resend any packets that have not been acknowledged.
    else {
        switch (mqttPublisher->pubState) {
            case GMOS_MQTT_CLIENT_PUB_STATE_ACK_WAIT_QOS1 :
                nextState = GMOS_MQTT_CLIENT_PUB_STATE_QUEUED_QOS1;
                break;
            case GMOS_MQTT_CLIENT_PUB_STATE_REC_WAIT_QOS2 :
                nextState = GMOS_MQTT_CLIENT_PUB_STATE_QUEUED_QOS2;
                break;
            case GMOS_MQTT_CLIENT_PUB_STATE_COMP_WAIT_QOS2 :
                nextState = GMOS_MQTT_CLIENT_PUB_STATE_REL_SEND_QOS2;
                break;
        }
        GMOS_LOG_FMT (LOG_DEBUG,
            "MQTT client publisher resumed connection (state 0x%02X -> 0x%02X).",
            mqttPublisher->pubState, nextState);
    }
    mqttPublisher->pubState = nextState;
}

/*
 * Internally used function which starts up the MQTT client publishing
 * state machine for each publisher on connection to the MQTT broker.
 */
void gmosMqttClientPublishStartup (gmosMqttClient_t* mqttClient)
{
    gmosMqttPublisher_t* mqttPublisher = mqttClient->publisherList;

    // Process each publisher instance in turn.
    while (mqttPublisher != NULL) {
        gmosMqttClientPublisherStartup (mqttClient, mqttPublisher);
        mqttPublisher = mqttPublisher->nextPublisher;
    }
}

/*
 * Attempt to transmit a published MQTT message with QoS levels 1 and 2.
 */
static gmosNetworkStatus_t gmosMqttClientPublisherSend (
    gmosMqttClient_t* mqttClient, gmosMqttPublisher_t* mqttPublisher)
{
    gmosNetworkStatus_t networkStatus;
    gmosBuffer_t packetBuffer = GMOS_BUFFER_INIT ();

    // For QoS 1 and QoS 2, the transmit message needs to be copied to
    // an intermediate buffer in case it needs to be retransmitted.
    if (!gmosBufferCopy (&(mqttPublisher->pubMessage), &packetBuffer)) {
        networkStatus = GMOS_NETWORK_STATUS_RETRY;
        goto out;
    }

    // Attempt to transmit the copied buffer.
    networkStatus = gmosNetworkLinkSend (
        mqttClient->networkLink, &packetBuffer);

    // On successfully sending the message, set the duplicate flag in
    // the original message in case it needs to be retransmitted.
    // Otherwise discard the contents of the intermediate buffer.
    if (networkStatus == GMOS_NETWORK_STATUS_SUCCESS) {
        gmosMqttPacketFormatSetDuplicate (&(mqttPublisher->pubMessage));
    } else {
        gmosBufferReset (&packetBuffer, 0);
    }
out:
    return networkStatus;
}

/*
 * Attempt to transmit an MQTT release message for QoS Level 2.
 */
static inline gmosNetworkStatus_t gmosMqttClientPublisherRelease (
    gmosMqttClient_t* mqttClient, gmosMqttPublisher_t* mqttPublisher)
{
    gmosNetworkStatus_t networkStatus;
    gmosBuffer_t packetBuffer = GMOS_BUFFER_INIT ();

    // Format the QoS 2 release control packet. Note the non-zero
    // setting that is required for the reserved flags.
    if (!gmosMqttPacketFormatHandshake (
        &packetBuffer, GMOS_MQTT_PACKET_HEADER_TYPE_PUBREL | 0x02,
        mqttPublisher->packetId)) {
        networkStatus = GMOS_NETWORK_STATUS_RETRY;
        goto out;
    }

    // Attempt to transmit the message.
    networkStatus = gmosNetworkLinkSend (
        mqttClient->networkLink, &packetBuffer);

    // Discard the contents of the packet buffer if the current message
    // needs to be retried or transmission failed.
    if (networkStatus != GMOS_NETWORK_STATUS_SUCCESS) {
        gmosBufferReset (&packetBuffer, 0);
    }
out:
    return networkStatus;
}

/*
 * Implement state machine tick processing for a single publishing
 * instance.
 */
static inline gmosTaskStatus_t gmosMqttClientPublisherTick (
    gmosMqttClient_t* mqttClient, gmosMqttPublisher_t* mqttPublisher)
{
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    gmosMqttClientPubState_t nextState = mqttPublisher->pubState;
    gmosNetworkStatus_t networkStatus;

    // Implement MQTT publisher state machine.
    switch (mqttPublisher->pubState) {

        // Suspend the task in reset and idle states.
        case GMOS_MQTT_CLIENT_PUB_STATE_RESET :
        case GMOS_MQTT_CLIENT_PUB_STATE_IDLE :
            taskStatus = GMOS_TASK_SUSPEND;
            break;

        // Attempt to transmit the queued MQTT message with QoS level 0,
        // which is 'fire and forget'.
        case GMOS_MQTT_CLIENT_PUB_STATE_QUEUED_QOS0 :
            networkStatus = gmosNetworkLinkSend (
                mqttClient->networkLink, &(mqttPublisher->pubMessage));
            if (networkStatus == GMOS_NETWORK_STATUS_RETRY) {
                taskStatus = GMOS_MQTT_CLIENT_PUB_TASK_RETRY;
            } else {
                nextState = GMOS_MQTT_CLIENT_PUB_STATE_COMPLETE;
            }
            mqttPublisher->pubStatus = networkStatus;
            break;

        // Attempt to transmit the queued MQTT message with QoS level
        // of 1, which requires the message to be retained.
        case GMOS_MQTT_CLIENT_PUB_STATE_QUEUED_QOS1 :
            networkStatus = gmosMqttClientPublisherSend (
                mqttClient, mqttPublisher);
            if (networkStatus == GMOS_NETWORK_STATUS_RETRY) {
                taskStatus = GMOS_MQTT_CLIENT_PUB_TASK_RETRY;
            } else if (networkStatus == GMOS_NETWORK_STATUS_SUCCESS) {
                nextState = GMOS_MQTT_CLIENT_PUB_STATE_ACK_WAIT_QOS1;
            } else {
                nextState = GMOS_MQTT_CLIENT_PUB_STATE_COMPLETE;
            }
            mqttPublisher->pubStatus = networkStatus;
            break;

        // Attempt to transmit the queued MQTT message with QoS level
        // of 2, which requires the message to be retained.
        case GMOS_MQTT_CLIENT_PUB_STATE_QUEUED_QOS2 :
            networkStatus = gmosMqttClientPublisherSend (
                mqttClient, mqttPublisher);
            if (networkStatus == GMOS_NETWORK_STATUS_RETRY) {
                taskStatus = GMOS_MQTT_CLIENT_PUB_TASK_RETRY;
            } else if (networkStatus == GMOS_NETWORK_STATUS_SUCCESS) {
                nextState = GMOS_MQTT_CLIENT_PUB_STATE_REC_WAIT_QOS2;
            } else {
                nextState = GMOS_MQTT_CLIENT_PUB_STATE_COMPLETE;
            }
            mqttPublisher->pubStatus = networkStatus;
            break;

        // Wait for the initial message acknowledgement for messages
        // with QoS levels of 1 or 2. No timeout is implemented, since
        // a failure of the network link should be detected by the keep
        // alive handshake, after which retransmission is only required
        // for a reconnection with the clean session flag set to 0.
        case GMOS_MQTT_CLIENT_PUB_STATE_ACK_WAIT_QOS1 :
        case GMOS_MQTT_CLIENT_PUB_STATE_REC_WAIT_QOS2 :
            taskStatus = GMOS_TASK_SUSPEND;
            break;

        // Send a release message as the third stage of the QoS 2
        // handshake.
        case GMOS_MQTT_CLIENT_PUB_STATE_REL_SEND_QOS2 :
            networkStatus = gmosMqttClientPublisherRelease (
                mqttClient, mqttPublisher);
            if (networkStatus == GMOS_NETWORK_STATUS_RETRY) {
                taskStatus = GMOS_MQTT_CLIENT_PUB_TASK_RETRY;
            } else if (networkStatus == GMOS_NETWORK_STATUS_SUCCESS) {
                nextState = GMOS_MQTT_CLIENT_PUB_STATE_COMP_WAIT_QOS2;
            } else {
                nextState = GMOS_MQTT_CLIENT_PUB_STATE_COMPLETE;
            }
            mqttPublisher->pubStatus = networkStatus;
            break;

        // Wait for the transaction complete for messages with QoS
        // level 2. No timeout is implemented, since a failure of the
        // network link should be detected by the keep alive handshake,
        // after which retransmission is only required for a reconnection
        // with the clean session flag set to 0.
        case GMOS_MQTT_CLIENT_PUB_STATE_COMP_WAIT_QOS2 :
            taskStatus = GMOS_TASK_SUSPEND;
            break;

        // Complete transmit message processing. Always reset the
        // message buffer in case there is residual data in it and
        // set the packet ID value to zero to filter out any spurious
        // handshake responses. The status callback is issued after the
        // publisher state has been reset so that a subsequent publish
        // request can be issued from the callback.
        case GMOS_MQTT_CLIENT_PUB_STATE_COMPLETE :
            networkStatus = mqttPublisher->pubStatus;
            mqttPublisher->pubStatus = GMOS_NETWORK_STATUS_NOT_VALID;
            mqttPublisher->packetId = 0;
            gmosBufferReset (&(mqttPublisher->pubMessage), 0);
            nextState = GMOS_MQTT_CLIENT_PUB_STATE_IDLE;
            if (mqttPublisher->statusHandler != NULL) {
                mqttPublisher->statusHandler (mqttPublisher, networkStatus);
            }
            break;
    }
    mqttPublisher->pubState = nextState;
    return taskStatus;
}

/*
 * Internally used function which implements a publishing state machine
 * tick for each publisher while connected to the MQTT broker.
 */
gmosTaskStatus_t gmosMqttClientPublishTick (gmosMqttClient_t* mqttClient)
{
    gmosMqttPublisher_t* mqttPublisher;
    gmosTaskStatus_t taskStatus = GMOS_TASK_SUSPEND;

    // Process each publisher instance in turn.
    mqttPublisher = mqttClient->publisherList;
    while (mqttPublisher != NULL) {
        taskStatus = gmosSchedulerPrioritise (taskStatus,
            gmosMqttClientPublisherTick (mqttClient, mqttPublisher));
        mqttPublisher = mqttPublisher->nextPublisher;
    }
    return taskStatus;
}

/*
 * Implement state machine receive packet processing for a single
 * publishing instance.
 */
static inline gmosNetworkStatus_t gmosMqttClientPublisherRxPacketHandler (
    gmosMqttPublisher_t* mqttPublisher, uint8_t packetType)
{
    gmosMqttClientPubState_t nextState = mqttPublisher->pubState;

    // Update MQTT publisher state machine on received packets.
    switch (mqttPublisher->pubState) {

        // Process QoS 1 acknowledgement messages.
        case GMOS_MQTT_CLIENT_PUB_STATE_ACK_WAIT_QOS1 :
            if (packetType == GMOS_MQTT_PACKET_HEADER_TYPE_PUBACK) {
                nextState = GMOS_MQTT_CLIENT_PUB_STATE_COMPLETE;
            }
            break;

        // Process QoS 2 received messages.
        case GMOS_MQTT_CLIENT_PUB_STATE_REC_WAIT_QOS2 :
            if (packetType == GMOS_MQTT_PACKET_HEADER_TYPE_PUBREC) {
                nextState = GMOS_MQTT_CLIENT_PUB_STATE_REL_SEND_QOS2;
            }
            break;

        // Process QoS 2 complete messages.
        case GMOS_MQTT_CLIENT_PUB_STATE_COMP_WAIT_QOS2 :
            if (packetType == GMOS_MQTT_PACKET_HEADER_TYPE_PUBCOMP) {
                nextState = GMOS_MQTT_CLIENT_PUB_STATE_COMPLETE;
            }
            break;
    }
    mqttPublisher->pubState = nextState;
    return GMOS_NETWORK_STATUS_SUCCESS;
}

/*
 * Internally used function which implements a received packet handler
 * for processing publishing response packets.
 */
gmosNetworkStatus_t gmosMqttClientPublishRxPacket (
    gmosMqttClient_t* mqttClient, uint8_t packetType, uint8_t packetFlags,
    uint8_t headerSize, uint32_t remainingSize)
{
    uint16_t packetId;
    gmosMqttPublisher_t* mqttPublisher;
    gmosNetworkStatus_t networkStatus = GMOS_NETWORK_STATUS_SUCCESS;

    // Perform protocol checks on the received packet. All publishing
    // handshake packets should be basic control packets with a two
    // byte ID.
    if ((packetFlags != 0x00) || (headerSize != 2) || (remainingSize != 2)) {
        networkStatus = GMOS_NETWORK_STATUS_PROTOCOL_ERROR;
        goto out;
    }

    // Extract the packet ID from the variable length header.
    if (!gmosMqttPacketParseHandshakePacketId (
        &(mqttClient->rxDataBuffer), &packetId)) {
        networkStatus = GMOS_NETWORK_STATUS_RETRY;
        goto out;
    }

    // Find a publisher instance which has a transaction ID that matches
    // the parsed packet ID. The invalid packet ID value of zero is used
    // to indicate that no handshake is in progress for the selected
    // publisher.
    mqttPublisher = mqttClient->publisherList;
    while (mqttPublisher != NULL) {
        if ((mqttPublisher->packetId != 0) &&
            (mqttPublisher->packetId == packetId)) {
            break;
        } else {
            mqttPublisher = mqttPublisher->nextPublisher;
        }
    }

    // Spurious handshake response packets for prior transactions are
    // not expected from the server. They could be treated as a protocol
    // error, but the approach used here is to just ignore them.
    if (mqttPublisher == NULL) {
        networkStatus = GMOS_NETWORK_STATUS_SUCCESS;
        goto out;
    }

    // Process the state machine for the selected publisher instance.
    networkStatus = gmosMqttClientPublisherRxPacketHandler (
        mqttPublisher, packetType);

out :
    return networkStatus;
}

/*
 * Publishes a new MQTT message using the specified publisher instance.
 */
gmosNetworkStatus_t gmosMqttPublisherSend (
    gmosMqttPublisher_t* mqttPublisher, const char* mqttTopic,
    uint8_t qosLevel, gmosBuffer_t* mqttPayload)
{
    gmosMqttClient_t* mqttClient = mqttPublisher->mqttClient;
    gmosNetworkStatus_t networkStatus;
    uint_fast8_t publishFlags;
    uint_fast16_t packetId;
    uint_fast8_t queueState;

    // Only initiate a publishing transaction if the publisher is in
    // the idle state.
    if (mqttPublisher->pubState != GMOS_MQTT_CLIENT_PUB_STATE_IDLE) {
        networkStatus = GMOS_NETWORK_STATUS_RETRY;
        goto out;
    }

    // Select the message publishing flags and next transmit state
    // machine state based on the specified QoS level.
    switch (qosLevel) {
        case 0 :
            publishFlags = GMOS_MQTT_PACKET_HEADER_FLAG_QOS0;
            packetId = 0;
            queueState = GMOS_MQTT_CLIENT_PUB_STATE_QUEUED_QOS0;
            break;
        case 1 :
            publishFlags = GMOS_MQTT_PACKET_HEADER_FLAG_QOS1;
            packetId = mqttClient->packetIdCounter;
            queueState = GMOS_MQTT_CLIENT_PUB_STATE_QUEUED_QOS1;
            break;
        case 2 :
            publishFlags = GMOS_MQTT_PACKET_HEADER_FLAG_QOS2;
            packetId = mqttClient->packetIdCounter;
            queueState = GMOS_MQTT_CLIENT_PUB_STATE_QUEUED_QOS2;
            break;
        default :
            networkStatus = GMOS_NETWORK_STATUS_UNSUPPORTED;
            goto out;
    }

    // Prepend the header to the payload buffer. On failure the payload
    // buffer will be left intact.
    if (!gmosMqttPacketFormatPublish (
        mqttPayload, publishFlags, packetId, mqttTopic)) {
        networkStatus = GMOS_NETWORK_STATUS_RETRY;
        goto out;
    }

    // Queue the message for transmission. This removes the contents of
    // the input payload buffer.
    gmosBufferMove (mqttPayload, &(mqttPublisher->pubMessage));
    mqttPublisher->packetId = packetId;
    mqttPublisher->pubState = queueState;

    // Increment the packet ID counter for the next handshake.
    if (qosLevel > 0) {
        do {
            mqttClient->packetIdCounter += 1;
        } while (mqttClient->packetIdCounter == 0);
    }

    // Wake the main client state task to process the queued message.
    gmosSchedulerTaskResume (&(mqttClient->mqttWorkerTask));
    networkStatus = GMOS_NETWORK_STATUS_SUCCESS;

out:
    return networkStatus;
}

/*
 * Initialises an MQTT client publisher instance and attaches it to the
 * MQTT client.
 */
bool gmosMqttClientAddPublisher (gmosMqttClient_t* mqttClient,
    gmosMqttPublisher_t* mqttPublisher,
    gmosMqttPublisherStatusHandler_t statusHandler)
{
    // Initialise the publisher state.
    mqttPublisher->pubState = GMOS_MQTT_CLIENT_PUB_STATE_RESET;
    mqttPublisher->pubStatus = GMOS_NETWORK_STATUS_NOT_VALID;
    mqttPublisher->statusHandler = statusHandler;
    mqttPublisher->packetId = 0;
    gmosBufferInit (&(mqttPublisher->pubMessage));

    // Add the MQTT publisher to the start of the publisher list.
    mqttPublisher->mqttClient = mqttClient;
    mqttPublisher->nextPublisher = mqttClient->publisherList;
    mqttClient->publisherList = mqttPublisher;
    return true;
}
