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
 * subscriber instance. It supports the version 3.1.1 MQTT protocol.
 */

#ifndef GMOS_MQTT_SUBSCRIBE_H
#define GMOS_MQTT_SUBSCRIBE_H

#include <stdint.h>
#include <stdbool.h>
#include "gmos-buffers.h"
#include "gmos-scheduler.h"
#include "gmos-network.h"
#include "gmos-mqtt-client.h"

/**
 * Defines the function prototype for message subscriber status callback
 * handlers. Callbacks are used to determine the status of subscribe or
 * unsubscribe requests after the full transaction handshake has
 * completed.
 * @param mqttSubscriber This is the MQTT subscriber instance which
 *     generated the status callback.
 * @param status This is the status of the latest subscribe or
 *     unsubscribe request after completion of the transaction
 *     handshake.
 * @param subscribing This is a boolean flag which will be set to 'true'
 *     if the callback is in response to a subscribe request and 'false'
 *     if it is in response to an unsubscribe request.
 * @param maxQosLevel This is the maximum MQTT QoS level that will be
 *     supported by the MQTT broker after completion of a subscribe
 *     request.
 */
typedef void (*gmosMqttSubscriberStatusHandler_t) (
    gmosMqttSubscriber_t* mqttSubscriber, gmosNetworkStatus_t status,
    bool subscribing, uint8_t maxQosLevel);

/**
 * Defines the function prototype to be used for application callbacks
 * which process messages received by an MQTT subscriber instance.
 * @param mqttSubscriber This is the MQTT subscriber instance which
 *     generated the received message callback.
 * @param mqttPacket This is a pointer to a buffer which contains the
 *     complete MQTT packet that encapsulates the received message.
 * @param payloadOffset This specifies the offset of the received
 *     message payload in the MQTT packet buffer.
 * @param qosLevel This is the MQTT QoS level which was used to transfer
 *     the received message from the broker to the client.
 */
typedef void (*gmosMqttSubscriberMessageHandler_t) (
    gmosMqttSubscriber_t* mqttSubscriber, gmosBuffer_t* mqttPacket,
    uint16_t payloadOffset, uint8_t qosLevel);

/**
 * Defines the GubbinsMOS MQTT client subscriber state that is used for
 * managing a single MQTT message subscriber.
 */
typedef struct gmosMqttSubscriber_t {

    // Link to the associated MQTT client instance.
    gmosMqttClient_t* mqttClient;

    // Link to the next subscriber instance in the subscriber list.
    gmosMqttSubscriber_t* nextSubscriber;

    // The MQTT topic filter which must remain valid for the duration
    // of current subscription.
    const char* mqttTopicFilter;

    // Specify the application callback for handling status updates.
    gmosMqttSubscriberStatusHandler_t statusHandler;

    // Specify the application callback for processing messages.
    gmosMqttSubscriberMessageHandler_t messageHandler;

    // Specify a buffer to be used for storing subscriber messages.
    gmosBuffer_t subMessage;

    // Specify the current packet ID for subscribe request messages.
    uint16_t packetId;

    // Specify the maximum QoS level for subscriber messages.
    uint8_t maxQosLevel;

    // Specify the current subscriber state.
    uint8_t subState;

    // Specify the subscriber status.
    uint8_t subStatus;

} gmosMqttSubscriber_t;

/**
 * Initialises an MQTT client subscriber instance and attaches it to the
 * MQTT client. This should be called once client initialisation is
 * complete and before the MQTT connection is established.
 * @param mqttClient This is the MQTT client instance that is to be
 *     associated with the new MQTT subscriber instance.
 * @param mqttSubscriber This is the MQTT subscriber instance that is to
 *     be initialised and associated with the MQTT client.
 * @param statusHandler This is the message subscriber status handler
 *     which will be used to indicate the completion status of
 *     subscribe and unsubscribe requests. A null reference may be used
 *     if status notifications are not required.
 * @param messageHandler This is the application callback function to be
 *     used for processing all messages received by the subscriber. A
 *     null reference may be used if received message notifications are
 *     not required.
 * @return Returns a boolean value which will be set to 'true' if the
 *     MQTT subscriber was successfully added and 'false' otherwise.
 */
bool gmosMqttClientAddSubscriber (gmosMqttClient_t* mqttClient,
    gmosMqttSubscriber_t* mqttSubscriber,
    gmosMqttSubscriberStatusHandler_t statusHandler,
    gmosMqttSubscriberMessageHandler_t messageHandler);

/**
 * Issues a new MQTT subscribe request message using the specified
 * subscriber instance.
 * @param mqttSubscriber This is the MQTT subscriber instance that will
 *     be used to issue the new MQTT subscribe request.
 * @param mqttTopicFilter This is a pointer to a string which contains
 *     the MQTT topic filter to be used in the subscribe request. This
 *     must remain valid until a corresponding unsubscribe request is
 *     issued for the same subscriber instance.
 * @param maxQosLevel This specifies the maximum MQTT quality of service
 *     level supported by the client. QoS levels of 0 and 1 are
 *     supported for subscribing.
 * @return Returns a network status value which indicates whether or not
 *     the subscribe request was successful, whether it needs to be
 *     retried or the reason for failure.
 */
gmosNetworkStatus_t gmosMqttSubscriberSubscribe (
    gmosMqttSubscriber_t* mqttSubscriber, const char* mqttTopicFilter,
    uint8_t maxQosLevel);

/**
 * Issues a new MQTT unsubscribe request message using the specified
 * subscriber instance.
 * @param mqttSubscriber This is the MQTT subscriber instance that will
 *     be used to issue the new MQTT unsubscribe request. It must
 *     previously have been subscribed to a valid MQTT topic.
 * @return Returns a network status value which indicates whether or not
 *     the unsubscribe request was successful, whether it needs to be
 *     retried or the reason for failure.
 */
gmosNetworkStatus_t gmosMqttSubscriberUnsubscribe (
    gmosMqttSubscriber_t* mqttSubscriber);

/**
 * Internally used function which starts up the MQTT client subscriber
 * state machine on connection to the MQTT broker.
 * @param mqttClient This is the MQTT client instance that is to be
 *     started up for message subscribing.
 */
void gmosMqttClientSubscribeStartup (gmosMqttClient_t* mqttClient);

/**
 * Internally used function which implements a subscriber state machine
 * tick while connected to the MQTT broker.
 * @param mqttClient This is the MQTT client instance that is to be
 *     processed for the subscriber state machine.
 * @return Returns a task status value which provides task scheduling
 *     information for the state machine to the main MQTT client task.
 */
gmosTaskStatus_t gmosMqttClientSubscribeTick (gmosMqttClient_t* mqttClient);

/**
 * Internally used function which implements a received packet handler
 * for processing subscriber response packets.
 * @param mqttClient This is the MQTT client instance that is receiving
 *     the subscriber response packets.
 * @param packetType This is the packet type that was extracted from
 *     MQTT packet.
 * @param packetFlags These are the packet flags that were extracted
 *     from the MQTT packet.
 * @param headerSize This is the size of the MQTT fixed header that was
 *     extracted from the MQTT packet.
 * @param remainingSize This is the size of the remaining data in the
 *     MQTT packet that follows the MQTT fixed header.
 * @return Returns a network status value which indicates whether the
 *     subscriber response packet was successfully processed.
 */
gmosNetworkStatus_t gmosMqttClientSubscribeRxPacket (
    gmosMqttClient_t* mqttClient, uint8_t packetType, uint8_t packetFlags,
    uint8_t headerSize, uint32_t remainingSize);

/**
 * Internally used function which implements a received packet handler
 * for processing subscriber data feed packets.
 * @param mqttClient This is the MQTT client instance that is receiving
 *     the subscriber data feed packets.
 * @param headerSize This is the size of the MQTT fixed header that was
 *     extracted from the MQTT packet.
 * @param remainingSize This is the size of the remaining data in the
 *     MQTT packet that follows the MQTT fixed header.
 * @return Returns a network status value which indicates whether the
 *     subscriber data feed packet was successfully processed.
 */
gmosNetworkStatus_t gmosMqttClientSubscribeRxDataFeed (
    gmosMqttClient_t* mqttClient, uint8_t headerSize,
    uint32_t remainingSize);

#endif // GMOS_MQTT_SUBSCRIBE_H
