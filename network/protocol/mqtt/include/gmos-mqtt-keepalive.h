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
 * This header defines the internal API for processing MQTT client keep
 * alive ping requests. It supports the version 3.1.1 MQTT protocol.
 */

#ifndef GMOS_MQTT_KEEPALIVE_H
#define GMOS_MQTT_KEEPALIVE_H

#include <stdint.h>
#include <stdbool.h>
#include "gmos-scheduler.h"
#include "gmos-network.h"
#include "gmos-mqtt-client.h"

/**
 * Internally used function for starting up the MQTT client keep alive
 * state machine on connection to the MQTT broker.
 * @param mqttClient This is the MQTT client instance that is to be
 *     started up for keep alive processing.
 */
void gmosMqttClientKeepAliveStartup (gmosMqttClient_t* mqttClient);

/**
 * Internally used function for implementing a keep alive state machine
 * tick while connected to the MQTT broker.
 * @param mqttClient This is the MQTT client instance that is to be
 *     processed for the keep alive state machine.
 * @return Returns a task status value which provides task scheduling
 *     information for the state machine to the main MQTT client task.
 */
gmosTaskStatus_t gmosMqttClientKeepAliveTick (gmosMqttClient_t* mqttClient);

/**
 * Internally used function for implementing a keep alive received
 * packet handler for processing ping response packets.
 * @param mqttClient This is the MQTT client instance that is receiving
 *     the ping response packets.
 * @param packetFlags These are the packet flags that were extracted
 *     from the MQTT packet.
 * @param headerSize This is the size of the MQTT fixed header that was
 *     extracted from the MQTT packet.
 * @param remainingSize This is the size of the remaining data in the
 *     MQTT packet that follows the MQTT fixed header.
 * @return Returns a network status value which indicates whether the
 *     ping response packet was successfully processed.
 */
gmosNetworkStatus_t gmosMqttClientKeepAliveRxPacket (
    gmosMqttClient_t* mqttClient, uint8_t packetFlags,
    uint8_t headerSize, uint32_t remainingSize);

/**
 * Internally used function for polling the keep alive state machine to
 * ensure that a keep alive timeout condition has not occurred.
 * @param mqttClient This is the MQTT client instance that is to be
 *     queried for the keep alive status.
 * @return Returns a boolean value which will be set to 'true' if the
 *     keep alive status is OK and 'false' if a timeout has occurred.
 */
bool gmosMqttClientKeepAliveOk (gmosMqttClient_t* mqttClient);

#endif // GMOS_MQTT_KEEPALIVE_H
