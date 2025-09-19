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
 * This file implements the MQTT client keep alive state machine. It
 * supports the version 3.1.1 MQTT protocol.
 */

#include <stdint.h>
#include <stdbool.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-buffers.h"
#include "gmos-network.h"
#include "gmos-network-links.h"
#include "gmos-mqtt-config.h"
#include "gmos-mqtt-packet.h"
#include "gmos-mqtt-keepalive.h"

/*
 * Specify the state space to be used for the MQTT keep alive state
 * machine.
 */
typedef enum {
    GMOS_MQTT_CLIENT_KEEPALIVE_STATE_IDLE,
    GMOS_MQTT_CLIENT_KEEPALIVE_STATE_SEND,
    GMOS_MQTT_CLIENT_KEEPALIVE_STATE_WAIT,
    GMOS_MQTT_CLIENT_KEEPALIVE_STATE_FAILED
} gmosMqttClientKeepAliveState_t;

/*
 * Send an MQTT ping request packet.
 */
static inline gmosNetworkStatus_t gmosMqttClientSendPingRequest (
    gmosMqttClient_t* mqttClient)
{
    gmosBuffer_t packetBuffer = GMOS_BUFFER_INIT ();
    gmosNetworkStatus_t networkStatus;

    // Attempt to format the MQTT ping request.
    if (!gmosMqttPacketFormatControl (&packetBuffer,
        GMOS_MQTT_PACKET_HEADER_TYPE_PINGREQ)) {
        networkStatus = GMOS_NETWORK_STATUS_RETRY;
        goto out;
    }

    // Attempt to send the MQTT ping request over the network link.
    networkStatus = gmosNetworkLinkSend (
        mqttClient->networkLink, &packetBuffer);
    if (networkStatus != GMOS_NETWORK_STATUS_SUCCESS) {
        gmosBufferReset (&packetBuffer, 0);
    }
out:
    return networkStatus;
}

/*
 * Starts up the MQTT client keep alive state machine on connection to
 * the MQTT broker.
 */
void gmosMqttClientKeepAliveStartup (gmosMqttClient_t* mqttClient)
{
    GMOS_LOG (LOG_DEBUG, "MQTT start up keep alive state machine.");
    mqttClient->keepAliveState = GMOS_MQTT_CLIENT_KEEPALIVE_STATE_IDLE;
    mqttClient->timeout = gmosPalGetTimer ();
}

/*
 * Implement a keep alive state machine tick while connected to the MQTT
 * broker.
 */
gmosTaskStatus_t gmosMqttClientKeepAliveTick (
    gmosMqttClient_t* mqttClient)
{
    gmosNetworkStatus_t networkStatus;
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    gmosMqttClientKeepAliveState_t nextState = mqttClient->keepAliveState;

    // Calculate the delay until the next keep alive event.
    uint32_t currentTimer = gmosPalGetTimer ();
    int32_t delay = (int32_t) (mqttClient->timeout - currentTimer);

    // Implement the main MQTT keep alive state machine.
    switch (mqttClient->keepAliveState) {

        // From the idle state wait until the next keep alive ping
        // request is due.
        case GMOS_MQTT_CLIENT_KEEPALIVE_STATE_IDLE :
            if (delay <= 0) {
                nextState = GMOS_MQTT_CLIENT_KEEPALIVE_STATE_SEND;
            } else {
                taskStatus = GMOS_TASK_RUN_LATER ((uint32_t) delay);
            }
            break;

        // Send the keep alive ping request.
        case GMOS_MQTT_CLIENT_KEEPALIVE_STATE_SEND :
            networkStatus = gmosMqttClientSendPingRequest (mqttClient);
            if (networkStatus == GMOS_NETWORK_STATUS_SUCCESS) {
                GMOS_LOG (LOG_VERBOSE, "MQTT issued ping request.");
                mqttClient->timeout = currentTimer +
                    GMOS_MS_TO_TICKS (GMOS_CONFIG_MQTT_TIMEOUT_PERIOD * 1000);
                nextState = GMOS_MQTT_CLIENT_KEEPALIVE_STATE_WAIT;
            } else if (networkStatus != GMOS_NETWORK_STATUS_RETRY) {
                GMOS_LOG (LOG_DEBUG, "MQTT network error sending ping request.");
                nextState = GMOS_MQTT_CLIENT_KEEPALIVE_STATE_FAILED;
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (10));
            }
            break;

        // Wait for the MQTT keep alive response.
        case GMOS_MQTT_CLIENT_KEEPALIVE_STATE_WAIT :
            if (delay <= 0) {
                GMOS_LOG (LOG_DEBUG, "MQTT timeout waiting for ping response.");
                nextState = GMOS_MQTT_CLIENT_KEEPALIVE_STATE_FAILED;
            } else {
                taskStatus = GMOS_TASK_RUN_LATER ((uint32_t) delay);
            }
            break;

        // Suspend further processing on a timeout condition.
        case GMOS_MQTT_CLIENT_KEEPALIVE_STATE_FAILED :
            GMOS_LOG (LOG_DEBUG, "MQTT keep alive processing failed.");
            taskStatus = GMOS_TASK_SUSPEND;
            break;
    }
    mqttClient->keepAliveState = nextState;
    return taskStatus;
}

/*
 * Implement a keep alive received packet handler for processing ping
 * response packets.
 */
gmosNetworkStatus_t gmosMqttClientKeepAliveRxPacket (
    gmosMqttClient_t* mqttClient, uint8_t packetFlags,
    uint8_t headerSize, uint32_t remainingSize)
{
    gmosNetworkStatus_t networkStatus;

    // Perform protocol checks on the received packet.
    if ((packetFlags != 0x00) || (headerSize != 2) || (remainingSize != 0)) {
        networkStatus = GMOS_NETWORK_STATUS_PROTOCOL_ERROR;
        goto out;
    }

    // Spurious ping response packets are not expected from the server.
    // They could be treated as a protocol error, but the approach used
    // here is to just ignore them.
    if (mqttClient->keepAliveState !=
        GMOS_MQTT_CLIENT_KEEPALIVE_STATE_WAIT) {
        networkStatus = GMOS_NETWORK_STATUS_SUCCESS;
        goto out;
    }

    // Process the expected ping response packets.
    GMOS_LOG (LOG_VERBOSE, "MQTT received ping response.");
    mqttClient->timeout = gmosPalGetTimer () +
        GMOS_MS_TO_TICKS (GMOS_CONFIG_MQTT_KEEP_ALIVE_PERIOD * 900);
    mqttClient->keepAliveState = GMOS_MQTT_CLIENT_KEEPALIVE_STATE_IDLE;
    networkStatus = GMOS_NETWORK_STATUS_SUCCESS;

out :
    return networkStatus;
}

/*
 * Poll the keep alive state machine to ensure that a keep alive timeout
 * condition has not occurred.
 */
bool gmosMqttClientKeepAliveOk (gmosMqttClient_t* mqttClient)
{
    return (mqttClient->keepAliveState !=
        GMOS_MQTT_CLIENT_KEEPALIVE_STATE_FAILED) ? true : false;
}
