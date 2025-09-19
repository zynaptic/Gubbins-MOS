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
 * publisher instance. It supports the version 3.1.1 MQTT protocol.
 */

#ifndef GMOS_MQTT_PUBLISH_H
#define GMOS_MQTT_PUBLISH_H

#include <stdint.h>
#include <stdbool.h>
#include "gmos-buffers.h"
#include "gmos-scheduler.h"
#include "gmos-network.h"
#include "gmos-mqtt-client.h"

/**
 * Defines the function prototype for message publishing status callback
 * handlers. Callbacks are used to determine the status of a publishing
 * request after the full publishing handshake has completed.
 * @param mqttPublisher This is the MQTT publisher instance which
 *     generated the status callback.
 * @param status This is the status of the latest publishing request
 *     after completion of the publishing handshake.
 */
typedef void (*gmosMqttPublisherStatusHandler_t) (
    gmosMqttPublisher_t* mqttPublisher, gmosNetworkStatus_t status);

/**
 * Defines the GubbinsMOS MQTT client publisher state that is used for
 * managing a single MQTT message publisher.
 */
typedef struct gmosMqttPublisher_t {

    // Link to the associated MQTT client instance.
    gmosMqttClient_t* mqttClient;

    // Link to the next publisher instance in the publisher list.
    gmosMqttPublisher_t* nextPublisher;

    // Specify the application callback for handling status updates.
    gmosMqttPublisherStatusHandler_t statusHandler;

    // Specify a buffer to be used for storing the published message.
    gmosBuffer_t pubMessage;

    // Specify the current packet ID for QoS 1 and QoS 2 messages.
    uint16_t packetId;

    // Specify the current publish message state.
    uint8_t pubState;

    // Specify the message publishing status.
    uint8_t pubStatus;

} gmosMqttPublisher_t;

/**
 * Initialises an MQTT client publisher instance and attaches it to the
 * MQTT client. This should be called once client initialisation is
 * complete and before the MQTT connection is established.
 * @param mqttClient This is the MQTT client instance that is to be
 *     associated with the new MQTT publisher instance.
 * @param mqttPublisher This is the MQTT publisher instance that is to
 *     be initialised and associated with the MQTT client.
 * @param statusHandler This is the message publishing status handler
 *     which will be used to indicate the completion status of
 *     publishing requests. A null reference may be used if status
 *     notifications are not required.
 * @return Returns a boolean value which will be set to 'true' if the
 *     MQTT publisher was successfully added and 'false' otherwise.
 */
bool gmosMqttClientAddPublisher (gmosMqttClient_t* mqttClient,
    gmosMqttPublisher_t* mqttPublisher,
    gmosMqttPublisherStatusHandler_t statusHandler);

/**
 * Publishes a new MQTT message using the specified publisher instance.
 * @param mqttPublisher This is the MQTT publisher instance that will be
 *     used to publish the new MQTT message.
 * @param mqttTopic This is a pointer to the MQTT topic which should be
 *     used for the published message. It does not need to remain valid
 *     after execution of this function.
 * @param qosLevel This is the MQTT quality of service level to be used
 *     when publishing the message. QoS levels of 0, 1 and 2 are
 *     supported for publishing.
 * @param mqttPayload This is a buffer which contains the message
 *     payload to be published. On successful completion the contents of
 *     the buffer will be automatically released.
 * @return Returns a network status value which indicates whether or not
 *     the publishing request was successful, whether it needs to be
 *     retried or the reason for failure.
 */
gmosNetworkStatus_t gmosMqttPublisherSend (
    gmosMqttPublisher_t* mqttPublisher, const char* mqttTopic,
    uint8_t qosLevel, gmosBuffer_t* mqttPayload);

/**
 * Internally used function which starts up the MQTT client publishing
 * state machine on connection to the MQTT broker.
 * @param mqttClient This is the MQTT client instance that is to be
 *     started up for message publishing.
 */
void gmosMqttClientPublishStartup (gmosMqttClient_t* mqttClient);

/**
 * Internally used function which implements a publishing state machine
 * tick while connected to the MQTT broker.
 * @param mqttClient This is the MQTT client instance that is to be
 *     processed for the publishing state machine.
 * @return Returns a task status value which provides task scheduling
 *     information for the state machine to the main MQTT client task.
 */
gmosTaskStatus_t gmosMqttClientPublishTick (gmosMqttClient_t* mqttClient);

/**
 * Internally used function which implements a received packet handler
 * for processing publishing response packets.
 * @param mqttClient This is the MQTT client instance that is receiving
 *     the publishing response packets.
 * @param packetType This is the packet type that was extracted from
 *     MQTT packet.
 * @param packetFlags These are the packet flags that were extracted
 *     from the MQTT packet.
 * @param headerSize This is the size of the MQTT fixed header that was
 *     extracted from the MQTT packet.
 * @param remainingSize This is the size of the remaining data in the
 *     MQTT packet that follows the MQTT fixed header.
 * @return Returns a network status value which indicates whether the
 *     publishing response packet was successfully processed.
 */
gmosNetworkStatus_t gmosMqttClientPublishRxPacket (
    gmosMqttClient_t* mqttClient, uint8_t packetType, uint8_t packetFlags,
    uint8_t headerSize, uint32_t remainingSize);

#endif // GMOS_MQTT_PUBLISH_H
