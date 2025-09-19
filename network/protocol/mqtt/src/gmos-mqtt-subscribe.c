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
 * This file implements the MQTT client subscriber state machine. It
 * supports the version 3.1.1 MQTT protocol.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-mqtt-client.h"
#include "gmos-mqtt-packet.h"
#include "gmos-mqtt-subscribe.h"

/*
 * Specify the retry interval used by the state machine.
 */
#define GMOS_MQTT_CLIENT_SUB_TASK_RETRY \
    (GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (10)))

/*
 * Specify the internal state space for the MQTT subscriber instances.
 */
typedef enum {
    GMOS_MQTT_CLIENT_SUB_STATE_RESET,
    GMOS_MQTT_CLIENT_SUB_STATE_IDLE,
    GMOS_MQTT_CLIENT_SUB_STATE_QUEUED_SUB_REQ,
    GMOS_MQTT_CLIENT_SUB_STATE_SUBACK_WAIT,
    GMOS_MQTT_CLIENT_SUB_STATE_SUBACK_COMPLETE,
    GMOS_MQTT_CLIENT_SUB_STATE_QUEUED_UNSUB_REQ,
    GMOS_MQTT_CLIENT_SUB_STATE_UNSUBACK_WAIT,
    GMOS_MQTT_CLIENT_SUB_STATE_UNSUBACK_COMPLETE,
    GMOS_MQTT_CLIENT_SUB_STATE_ACTIVE
} gmosMqttClientSubState_t;

/*
 * Specify the internal state space for the MQTT packet queue handler.
 */
typedef enum {
    GMOS_MQTT_CLIENT_SUB_QUEUE_STATE_IDLE,
    GMOS_MQTT_CLIENT_SUB_QUEUE_STATE_ACK_SEND_QOS1
} gmosMqttClientSubQueueState_t;

/*
 * Implement startup processing for a single subscriber instance.
 */
static inline void gmosMqttClientSubscriberStartup (
    gmosMqttClient_t* mqttClient, gmosMqttSubscriber_t* mqttSubscriber)
{
    uint_fast8_t nextState = mqttSubscriber->subState;

    // This resets the subscriber instance for a clean connection.
    if ((mqttClient->sessionFlags &
        GMOS_MQTT_CLIENT_SESSION_FLAG_CLEAN) != 0) {
        GMOS_LOG (LOG_DEBUG, "MQTT client subscriber clean startup.");
        mqttSubscriber->mqttTopicFilter = NULL;
        mqttSubscriber->packetId = 0;
        mqttSubscriber->maxQosLevel = 0;
        mqttSubscriber->subStatus = GMOS_NETWORK_STATUS_NOT_VALID;
        gmosBufferReset (&(mqttSubscriber->subMessage), 0);
        nextState = GMOS_MQTT_CLIENT_SUB_STATE_IDLE;
    }

    // For resumed connections, adjust the current transaction state
    // in order to resend any packets that have not been acknowledged.
    else {
        switch (mqttSubscriber->subState) {
            case GMOS_MQTT_CLIENT_SUB_STATE_SUBACK_WAIT :
                nextState = GMOS_MQTT_CLIENT_SUB_STATE_QUEUED_SUB_REQ;
                break;
            case GMOS_MQTT_CLIENT_SUB_STATE_UNSUBACK_WAIT :
                nextState = GMOS_MQTT_CLIENT_SUB_STATE_QUEUED_UNSUB_REQ;
                break;
        }
        GMOS_LOG_FMT (LOG_DEBUG,
            "MQTT subscriber resumed connection (state 0x%02X -> 0x%02X).",
            mqttClient->subQueueState, nextState);
    }
    mqttSubscriber->subState = nextState;
}

/*
 * Internally used function which starts up the MQTT client subscriber
 * state machine for each subscriber on connection to the MQTT broker.
 */
void gmosMqttClientSubscribeStartup (gmosMqttClient_t* mqttClient)
{
    gmosMqttSubscriber_t* mqttSubscriber = mqttClient->subscriberList;

    // On a clean session startup reset the common subscriber queue
    // handler to the idle state.
    if ((mqttClient->sessionFlags &
        GMOS_MQTT_CLIENT_SESSION_FLAG_CLEAN) != 0) {
        GMOS_LOG (LOG_DEBUG, "MQTT client packet queue clean startup.");
        mqttClient->subQueueState = GMOS_MQTT_CLIENT_SUB_QUEUE_STATE_IDLE;
    }

    // On resumed connections, do not send outstanding acknowledgement
    // messages since the broker is expected to send a duplicate copy of
    // the original message. It is then up to the application layer how
    // to handle duplicates.
    else {
        GMOS_LOG (LOG_DEBUG, "MQTT client subscriber resumed connection.");
        mqttClient->subQueueState = GMOS_MQTT_CLIENT_SUB_QUEUE_STATE_IDLE;
    }

    // Process each subscriber instance in turn.
    while (mqttSubscriber != NULL) {
        gmosMqttClientSubscriberStartup (mqttClient, mqttSubscriber);
        mqttSubscriber = mqttSubscriber->nextSubscriber;
    }
}

/*
 * Attempt to transmit a subscribe or unsubscribe request message with
 * retry support.
 */
static gmosNetworkStatus_t gmosMqttClientSubscriberRequestSend (
    gmosMqttClient_t* mqttClient, gmosMqttSubscriber_t* mqttSubscriber)
{
    gmosNetworkStatus_t networkStatus;
    gmosBuffer_t packetBuffer = GMOS_BUFFER_INIT ();

    // For retry support, the transmit message needs to be copied to
    // an intermediate buffer in case it needs to be retransmitted.
    if (!gmosBufferCopy (&(mqttSubscriber->subMessage), &packetBuffer)) {
        networkStatus = GMOS_NETWORK_STATUS_RETRY;
        goto out;
    }

    // Attempt to transmit the message.
    networkStatus = gmosNetworkLinkSend (
        mqttClient->networkLink, &packetBuffer);

    // Discard the contents of the intermediate buffer if the current
    // message needs to be retried or transmission failed.
    if (networkStatus != GMOS_NETWORK_STATUS_SUCCESS) {
        gmosBufferReset (&packetBuffer, 0);
    }
out:
    return networkStatus;
}

/*
 * Implement state machine tick processing for a single subscriber
 * instance.
 */
static inline gmosTaskStatus_t gmosMqttClientSubscriberTick (
    gmosMqttClient_t* mqttClient, gmosMqttSubscriber_t* mqttSubscriber)
{
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    gmosMqttClientSubState_t nextState = mqttSubscriber->subState;
    gmosNetworkStatus_t networkStatus;

    // Implement MQTT subscriber state machine.
    switch (mqttSubscriber->subState) {

        // Suspend the task in reset and idle states.
        case GMOS_MQTT_CLIENT_SUB_STATE_RESET :
        case GMOS_MQTT_CLIENT_SUB_STATE_IDLE :
            taskStatus = GMOS_TASK_SUSPEND;
            break;

        // Attempt to transmit the queued MQTT subscribe messages with
        // retry support, which requires the message to be retained.
        case GMOS_MQTT_CLIENT_SUB_STATE_QUEUED_SUB_REQ :
            networkStatus = gmosMqttClientSubscriberRequestSend (
                mqttClient, mqttSubscriber);
            if (networkStatus == GMOS_NETWORK_STATUS_RETRY) {
                taskStatus = GMOS_MQTT_CLIENT_SUB_TASK_RETRY;
            } else if (networkStatus == GMOS_NETWORK_STATUS_SUCCESS) {
                nextState = GMOS_MQTT_CLIENT_SUB_STATE_SUBACK_WAIT;
            } else {
                nextState = GMOS_MQTT_CLIENT_SUB_STATE_SUBACK_COMPLETE;
            }
            mqttSubscriber->subStatus = networkStatus;
            break;

        // Attempt to transmit the queued MQTT unsubscribe messages with
        // retry support, which requires the message to be retained.
        case GMOS_MQTT_CLIENT_SUB_STATE_QUEUED_UNSUB_REQ :
            networkStatus = gmosMqttClientSubscriberRequestSend (
                mqttClient, mqttSubscriber);
            if (networkStatus == GMOS_NETWORK_STATUS_RETRY) {
                taskStatus = GMOS_MQTT_CLIENT_SUB_TASK_RETRY;
            } else if (networkStatus == GMOS_NETWORK_STATUS_SUCCESS) {
                nextState = GMOS_MQTT_CLIENT_SUB_STATE_UNSUBACK_WAIT;
            } else {
                nextState = GMOS_MQTT_CLIENT_SUB_STATE_UNSUBACK_COMPLETE;
            }
            mqttSubscriber->subStatus = networkStatus;
            break;

        // Wait for the initial message acknowledgement for messages
        // with retry support. No timeout is implemented, since a
        // failure of the network link should be detected by the keep
        // alive handshake, after which retransmission is only required
        // for a reconnection with the clean session flag set to 0.
        case GMOS_MQTT_CLIENT_SUB_STATE_SUBACK_WAIT :
        case GMOS_MQTT_CLIENT_SUB_STATE_UNSUBACK_WAIT :
            taskStatus = GMOS_TASK_SUSPEND;
            break;

        // Complete the transaction for a subscribe request.
        case GMOS_MQTT_CLIENT_SUB_STATE_SUBACK_COMPLETE :
            networkStatus = mqttSubscriber->subStatus;
            mqttSubscriber->subStatus = GMOS_NETWORK_STATUS_NOT_VALID;
            mqttSubscriber->packetId = 0;
            gmosBufferReset (&(mqttSubscriber->subMessage), 0);
            if (networkStatus == GMOS_NETWORK_STATUS_SUCCESS) {
                nextState = GMOS_MQTT_CLIENT_SUB_STATE_ACTIVE;
            } else {
                nextState = GMOS_MQTT_CLIENT_SUB_STATE_IDLE;
            }
            if (mqttSubscriber->statusHandler != NULL) {
                mqttSubscriber->statusHandler (mqttSubscriber,
                    networkStatus, true, mqttSubscriber->maxQosLevel);
            }
            break;

        // Complete the transaction for an unsubscribe request.
        case GMOS_MQTT_CLIENT_SUB_STATE_UNSUBACK_COMPLETE :
            networkStatus = mqttSubscriber->subStatus;
            mqttSubscriber->subStatus = GMOS_NETWORK_STATUS_NOT_VALID;
            mqttSubscriber->packetId = 0;
            gmosBufferReset (&(mqttSubscriber->subMessage), 0);
            nextState = GMOS_MQTT_CLIENT_SUB_STATE_IDLE;
            if (mqttSubscriber->statusHandler != NULL) {
                mqttSubscriber->statusHandler (mqttSubscriber,
                    networkStatus, false, mqttSubscriber->maxQosLevel);
            }
            break;

        // Wait for received messages while the subscriber is in the
        // active state.
        case GMOS_MQTT_CLIENT_SUB_STATE_ACTIVE :
            taskStatus = GMOS_TASK_SUSPEND;
            break;
    }
    mqttSubscriber->subState = nextState;
    return taskStatus;
}

/*
 * Implement data packet handler which sends a received data packet to
 * the appropriate subscribers.
 */
static inline void gmosMqttClientSubscribedPacketHandler (
    gmosMqttClient_t* mqttClient, gmosBuffer_t* packetBuffer,
    uint_fast16_t payloadOffset, uint_fast8_t qosLevel)
{
    gmosMqttSubscriber_t* mqttSubscriber;
    bool matchOk;

    // Search for subscribers that are ready to receive messages and
    // that have a matching topic filter.
    mqttSubscriber = mqttClient->subscriberList;
    while (mqttSubscriber != NULL) {
        if (mqttSubscriber->subState == GMOS_MQTT_CLIENT_SUB_STATE_ACTIVE) {
            if (gmosMqttPacketMatchDataPacketTopic (packetBuffer,
                mqttSubscriber->mqttTopicFilter, &matchOk)) {
                if (matchOk) {
                    if (mqttSubscriber->messageHandler != NULL) {
                        mqttSubscriber->messageHandler (mqttSubscriber,
                            packetBuffer, payloadOffset, qosLevel);
                    }
                }
            }
        }
        mqttSubscriber = mqttSubscriber->nextSubscriber;
    }
}

/*
 * Attempt to transmit an MQTT handshake message.
 */
static gmosNetworkStatus_t gmosMqttClientSubscribeHandshakeSend (
    gmosMqttClient_t* mqttClient, uint8_t headerByte)
{
    gmosNetworkStatus_t networkStatus;
    gmosBuffer_t packetBuffer = GMOS_BUFFER_INIT ();

    // Format the handshake packet.
    if (!gmosMqttPacketFormatHandshake (&packetBuffer,
        headerByte, mqttClient->subQueuePacketId)) {
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
 * Implement the state machine for processing the received data queue.
 */
static inline gmosTaskStatus_t gmosMqttClientSubscribePacketQueueTick (
    gmosMqttClient_t* mqttClient)
{
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    gmosMqttClientSubQueueState_t nextState = mqttClient->subQueueState;
    gmosBuffer_t packetBuffer = GMOS_BUFFER_INIT ();
    gmosNetworkStatus_t networkStatus;
    uint8_t packetType;
    uint8_t packetFlags;
    uint16_t packetId;
    uint16_t payloadOffset;
    uint_fast8_t qosLevel;

    // Implement MQTT packet queue state machine.
    switch (mqttClient->subQueueState) {

        // From the idle state, extract the QoS flags for a newly
        // received packet and handle it accordingly.
        case GMOS_MQTT_CLIENT_SUB_QUEUE_STATE_IDLE :

            // Attempt to read a new data packet from the queue.
            if (!gmosStreamAcceptBuffer (
                &(mqttClient->rxDataStream), &packetBuffer)) {
                taskStatus = GMOS_TASK_SUSPEND;
                break;
            }

            // Parse the packet for the required parameters. Packets
            // that fail parsing are silently discarded.
            if ((!gmosMqttPacketParseDataPacket (&packetBuffer,
                &packetType, &packetFlags, &packetId, &payloadOffset)) ||
                (packetType != GMOS_MQTT_PACKET_HEADER_TYPE_PUBLISH)) {
                break;
            }

            // Extract the QoS level of the received packet from the
            // header flags. Note that QoS 2 packets are not currently
            // supported and will be silently discarded.
            qosLevel = packetFlags & GMOS_MQTT_PACKET_HEADER_FLAG_QOS_MASK;
            if (qosLevel > GMOS_MQTT_PACKET_HEADER_FLAG_QOS1) {
                break;
            }

            // Store the packet ID for QoS 1 packets.
            mqttClient->subQueuePacketId = packetId;

            // QoS 0 and QoS 1 packets can be processed immediately.
            gmosMqttClientSubscribedPacketHandler (
                mqttClient, &packetBuffer, payloadOffset, qosLevel >> 1);

            // Continue processing the appropriate QoS handshake.
            if (qosLevel == GMOS_MQTT_PACKET_HEADER_FLAG_QOS1) {
                nextState = GMOS_MQTT_CLIENT_SUB_QUEUE_STATE_ACK_SEND_QOS1;
            }
            break;

        // Attempt to send a QoS 1 acknowledgement packet. Failures are
        // ignored since the peer should attempt to resend QoS 1 packets
        // after recovering the connection.
        case GMOS_MQTT_CLIENT_SUB_QUEUE_STATE_ACK_SEND_QOS1 :
            networkStatus = gmosMqttClientSubscribeHandshakeSend (
                mqttClient, GMOS_MQTT_PACKET_HEADER_TYPE_PUBACK);
            if (networkStatus == GMOS_NETWORK_STATUS_SUCCESS) {
                nextState = GMOS_MQTT_CLIENT_SUB_QUEUE_STATE_IDLE;
            } else if (networkStatus == GMOS_NETWORK_STATUS_RETRY) {
                taskStatus = GMOS_MQTT_CLIENT_SUB_TASK_RETRY;
            } else {
                GMOS_LOG_FMT (LOG_DEBUG,
                    "MQTT failed to send QoS 1 acknowledgement (status %d).",
                    networkStatus);
                nextState = GMOS_MQTT_CLIENT_SUB_QUEUE_STATE_IDLE;
            }
            break;
    }

    // Discard the packet buffer contents after processing.
    gmosBufferReset (&packetBuffer, 0);
    mqttClient->subQueueState = nextState;
    return taskStatus;
}

/*
 * Internally used function which implements a subscriber state machine
 * tick for each subscriber while connected to the MQTT broker.
 */
gmosTaskStatus_t gmosMqttClientSubscribeTick (gmosMqttClient_t* mqttClient)
{
    gmosMqttSubscriber_t* mqttSubscriber;
    gmosTaskStatus_t taskStatus;

    // Process the receive data packet queue.
    taskStatus = gmosMqttClientSubscribePacketQueueTick (mqttClient);

    // Process each subscriber instance in turn.
    mqttSubscriber = mqttClient->subscriberList;
    while (mqttSubscriber != NULL) {
        taskStatus = gmosSchedulerPrioritise (taskStatus,
            gmosMqttClientSubscriberTick (mqttClient, mqttSubscriber));
        mqttSubscriber = mqttSubscriber->nextSubscriber;
    }
    return taskStatus;
}

/*
 * Implement state machine receive packet processing for a single
 * subscriber instance.
 */
static inline gmosNetworkStatus_t gmosMqttClientSubscriberRxPacketHandler (
    gmosMqttClient_t* mqttClient, gmosMqttSubscriber_t* mqttSubscriber,
    uint8_t packetType)
{
    gmosMqttClientSubState_t nextState = mqttSubscriber->subState;
    gmosNetworkStatus_t networkStatus = GMOS_NETWORK_STATUS_SUCCESS;
    uint8_t subscribeStatus;

    // Update MQTT subscriber state machine on received packets.
    switch (mqttSubscriber->subState) {

        // Process subscribe request acknowledgement messages. These
        // should contain a single byte of payload status data after the
        // standard header and packet ID fields. This must be set to
        // indicate a maximum QoS level of 0 or 1. QoS 2 support is not
        // currently implemented.
        case GMOS_MQTT_CLIENT_SUB_STATE_SUBACK_WAIT :
            if (packetType == GMOS_MQTT_PACKET_HEADER_TYPE_SUBACK) {
                if (!gmosMqttPacketParseSubscribeAck (
                    &(mqttClient->rxDataBuffer), &subscribeStatus)) {
                    networkStatus = GMOS_NETWORK_STATUS_RETRY;
                } else if (subscribeStatus <=
                    GMOS_MQTT_PACKET_SUBSCRIBE_ACK_STATUS_QOS1) {
                    mqttSubscriber->subStatus = GMOS_NETWORK_STATUS_SUCCESS;
                    mqttSubscriber->maxQosLevel = subscribeStatus;
                    nextState = GMOS_MQTT_CLIENT_SUB_STATE_SUBACK_COMPLETE;
                } else {
                    mqttSubscriber->subStatus = GMOS_NETWORK_STATUS_UNSUPPORTED;
                    mqttSubscriber->maxQosLevel = 0;
                    mqttSubscriber->mqttTopicFilter = NULL;
                    nextState = GMOS_MQTT_CLIENT_SUB_STATE_SUBACK_COMPLETE;
                }
            }
            break;

        // Process unsubscribe request acknowledgement messages. These
        // do not contain any payload data.
        case GMOS_MQTT_CLIENT_SUB_STATE_UNSUBACK_WAIT :
            if (packetType == GMOS_MQTT_PACKET_HEADER_TYPE_UNSUBACK) {
                mqttSubscriber->subStatus = GMOS_NETWORK_STATUS_SUCCESS;
                mqttSubscriber->maxQosLevel = 0;
                mqttSubscriber->mqttTopicFilter = NULL;
                nextState = GMOS_MQTT_CLIENT_SUB_STATE_UNSUBACK_COMPLETE;
            }
            break;
    }
    mqttSubscriber->subState = nextState;
    return networkStatus;
}

/*
 * Internally used function which implements a received packet handler
 * for processing subscriber response packets.
 */
gmosNetworkStatus_t gmosMqttClientSubscribeRxPacket (
    gmosMqttClient_t* mqttClient, uint8_t packetType, uint8_t packetFlags,
    uint8_t headerSize, uint32_t remainingSize)
{
    gmosMqttSubscriber_t* mqttSubscriber;
    uint16_t packetId;
    gmosNetworkStatus_t networkStatus = GMOS_NETWORK_STATUS_SUCCESS;
    uint32_t requiredSize;

    // Select the required size and flag settings for the different
    // response packets.
    requiredSize = (packetType == GMOS_MQTT_PACKET_HEADER_TYPE_SUBACK) ? 3 : 2;

    // Perform protocol checks on the received packet.
    if ((packetFlags != 0) || (headerSize != 2) ||
        (remainingSize != requiredSize)) {
        networkStatus = GMOS_NETWORK_STATUS_PROTOCOL_ERROR;
        goto out;
    }

    // Extract the packet ID from the variable length header.
    if (!gmosMqttPacketParseHandshakePacketId (
        &(mqttClient->rxDataBuffer), &packetId)) {
        networkStatus = GMOS_NETWORK_STATUS_RETRY;
        goto out;
    }

    // Find a subscriber instance which has a transaction ID that
    // matches the parsed packet ID. A packet ID of zero is used to
    // indicate that no handshake is in progress for the selected
    // subscriber.
    mqttSubscriber = mqttClient->subscriberList;
    while (mqttSubscriber != NULL) {
        if ((mqttSubscriber->packetId != 0) &&
            (mqttSubscriber->packetId == packetId)) {
            break;
        } else {
            mqttSubscriber = mqttSubscriber->nextSubscriber;
        }
    }

    // Spurious handshake response packets for prior transactions are
    // not expected from the server. They could be treated as a protocol
    // error, but the approach used here is to just ignore them.
    if (mqttSubscriber == NULL) {
        networkStatus = GMOS_NETWORK_STATUS_SUCCESS;
    }

    // Process the state machine for the selected subscriber instance.
    else {
        networkStatus = gmosMqttClientSubscriberRxPacketHandler (
            mqttClient, mqttSubscriber, packetType);
    }
out :
    return networkStatus;
}

/*
 * Internally used function which implements a received packet handler
 * for processing subscriber data feed packets.
 */
gmosNetworkStatus_t gmosMqttClientSubscribeRxDataFeed (
    gmosMqttClient_t* mqttClient, uint8_t headerSize,
    uint32_t remainingSize)
{
    gmosNetworkStatus_t networkStatus = GMOS_NETWORK_STATUS_SUCCESS;
    gmosBuffer_t packetBuffer = GMOS_BUFFER_INIT ();
    uint_fast16_t packetSize = headerSize + remainingSize;

    // Attempt to copy received data packets from the main input buffer
    // for subsequent processing.
    if (!gmosBufferCopySection (
        &(mqttClient->rxDataBuffer), &packetBuffer, 0, packetSize)) {
        networkStatus = GMOS_NETWORK_STATUS_RETRY;
    }

    // Attempt to queue received data packets for subsequent processing.
    else if (!gmosStreamSendBuffer (
        &(mqttClient->rxDataStream), &packetBuffer)) {
        gmosBufferReset (&packetBuffer, 0);
        networkStatus = GMOS_NETWORK_STATUS_RETRY;
    }
    return networkStatus;
}

/*
 * Issues a new MQTT subscribe request using the specified subscriber
 * instance.
 */
gmosNetworkStatus_t gmosMqttSubscriberSubscribe (
    gmosMqttSubscriber_t* mqttSubscriber, const char* mqttTopicFilter,
    uint8_t maxQosLevel)
{
    gmosMqttClient_t* mqttClient = mqttSubscriber->mqttClient;
    gmosNetworkStatus_t networkStatus;

    // Sanity check the request parameters. Note that QoS level 2 is not
    // currently supported on the subscriber side, since the requirement
    // to support multiple overlapping transactions adds quite a bit of
    // extra complexity. This restriction may be revisited if there is
    // a compelling use case for QoS level 2.
    if ((mqttTopicFilter == NULL) || (maxQosLevel >= 2)) {
        networkStatus = GMOS_NETWORK_STATUS_NOT_VALID;
        goto out;
    }

    // Only initiate a subscriber registration transaction if the
    // subscriber is in the idle state.
    // TODO: Should indicate invalid if already subscribed.
    if (mqttSubscriber->subState != GMOS_MQTT_CLIENT_SUB_STATE_IDLE) {
        networkStatus = GMOS_NETWORK_STATUS_RETRY;
        goto out;
    }

    // Format the subscription request message. Note that only a single
    // subscriber topic is supported for each subscriber instance.
    if (!gmosMqttPacketFormatSubscribe (&(mqttSubscriber->subMessage),
        mqttClient->packetIdCounter, mqttTopicFilter, maxQosLevel)) {
        networkStatus = GMOS_NETWORK_STATUS_RETRY;
        goto out;
    }

    // Queue the message for transmission.
    mqttSubscriber->mqttTopicFilter = mqttTopicFilter;
    mqttSubscriber->maxQosLevel = maxQosLevel;
    mqttSubscriber->packetId = mqttClient->packetIdCounter;
    mqttSubscriber->subState = GMOS_MQTT_CLIENT_SUB_STATE_QUEUED_SUB_REQ;

    // Increment the packet ID counter for the next handshake.
    do {
        mqttClient->packetIdCounter += 1;
    } while (mqttClient->packetIdCounter == 0);

    // Wake the main client state task to process the queued message.
    gmosSchedulerTaskResume (&(mqttClient->mqttWorkerTask));
    networkStatus = GMOS_NETWORK_STATUS_SUCCESS;

out:
    return networkStatus;
}

/*
 * Issues a new MQTT unsubscribe request using the specified subscriber
 * instance.
 */
gmosNetworkStatus_t gmosMqttSubscriberUnsubscribe (
    gmosMqttSubscriber_t* mqttSubscriber)
{
    gmosMqttClient_t* mqttClient = mqttSubscriber->mqttClient;
    gmosNetworkStatus_t networkStatus;

    // Only initiate an unsubscribe transaction if the subscriber is in
    // the active state.
    // TODO: Should indicate invalid if not subscribed.
    if (mqttSubscriber->subState != GMOS_MQTT_CLIENT_SUB_STATE_ACTIVE) {
        networkStatus = GMOS_NETWORK_STATUS_RETRY;
        goto out;
    }

    // Format the unsubscribe request message. Note that only a single
    // subscriber topic is supported for each subscriber instance.
    if (!gmosMqttPacketFormatUnsubscribe (&(mqttSubscriber->subMessage),
        mqttClient->packetIdCounter, mqttSubscriber->mqttTopicFilter)) {
        networkStatus = GMOS_NETWORK_STATUS_RETRY;
        goto out;
    }

    // Queue the message for transmission.
    mqttSubscriber->packetId = mqttClient->packetIdCounter;
    mqttSubscriber->subState = GMOS_MQTT_CLIENT_SUB_STATE_QUEUED_UNSUB_REQ;

    // Increment the packet ID counter for the next handshake.
    do {
        mqttClient->packetIdCounter += 1;
    } while (mqttClient->packetIdCounter == 0);

    // Wake the main client state task to process the queued message.
    gmosSchedulerTaskResume (&(mqttClient->mqttWorkerTask));
    networkStatus = GMOS_NETWORK_STATUS_SUCCESS;

out:
    return networkStatus;
}

/*
 * Initialises an MQTT client subscriber instance and attaches it to the
 * MQTT client. This should be called once client initialisation is
 * complete and before the MQTT connection is established.
 */
bool gmosMqttClientAddSubscriber (gmosMqttClient_t* mqttClient,
    gmosMqttSubscriber_t* mqttSubscriber,
    gmosMqttSubscriberStatusHandler_t statusHandler,
    gmosMqttSubscriberMessageHandler_t messageHandler)
{
    // Initialise the subscriber state.
    mqttSubscriber->subState = GMOS_MQTT_CLIENT_SUB_STATE_RESET;
    mqttSubscriber->subStatus = GMOS_NETWORK_STATUS_NOT_VALID;
    mqttSubscriber->mqttTopicFilter = NULL;
    mqttSubscriber->statusHandler = statusHandler;
    mqttSubscriber->messageHandler = messageHandler;
    mqttSubscriber->packetId = 0;
    mqttSubscriber->maxQosLevel = 0;
    gmosBufferInit (&(mqttSubscriber->subMessage));

    // Add the MQTT subscriber to the start of the subscriber list.
    mqttSubscriber->mqttClient = mqttClient;
    mqttSubscriber->nextSubscriber = mqttClient->subscriberList;
    mqttClient->subscriberList = mqttSubscriber;
    return true;
}
