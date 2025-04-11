/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2022-2025 Zynaptic Limited
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
 * This header defines the common API for supporting any Zigbee ZDO
 * server requests that are not otherwise handled by the vendor stack.
 */

#ifndef GMOS_ZIGBEE_ZDO_SERVER_H
#define GMOS_ZIGBEE_ZDO_SERVER_H

#include <stdint.h>
#include <stdbool.h>
#include "gmos-zigbee-stack.h"
#include "gmos-zigbee-aps.h"

/**
 * This is the function protoype for callback handlers that process ZDO
 * end device announcements.
 * @param zigbeeStack This is the Zigbee stack instance which received
 *     the incoming ZDO device announcement message.
 * @param callbackData This is an opaque pointer to the callback data
 *     item that was registered with the callback handler.
 * @param networkAddr This is the 16-bit network address that has been
 *     assigned to the device on joining or rejoining the network.
 * @param macAddr This is the 64 bit IEEE MAC address of the device that
 *     is joining or rejoining the network.
 * @param macCapability This is the set of capability flags which define
 *     the MAC layer node capabilities of the device that is joining or
 *     rejoining the network.
 */
typedef void (*gmosZigbeeZdoServerDevAnnceHandler) (
    gmosZigbeeStack_t* zigbeeStack, void* callbackData,
    uint16_t networkAddr, uint8_t* macAddr, uint8_t macCapability);

/**
 * This is the callback handler which will be called in order to notify
 * the common Zigbee stack implementation of a newly received ZDO
 * request message that should be processed by the ZDO server entity.
 * @param zigbeeStack This is the Zigbee stack instance which received
 *     the incoming ZDO request message.
 * @param rxMsgApsFrame This is the APS frame data structure which
 *     encapsulates the received APS message. The APS frame contents
 *     are only guaranteed to remain valid for the duration of the
 *     callback.
 */
void gmosZigbeeZdoServerRequestHandler (
    gmosZigbeeStack_t* zigbeeStack, gmosZigbeeApsFrame_t* rxMsgApsFrame);

/**
 * Registers a ZDO server device announcement handler with the stack to
 * process ZDO end device announcements.
 * @param zigbeeStack This is the Zigbee stack instance for which the
 *     device announcement handler is being registered.
 * @param devAnnceHandler This is the device announcement handler which
 *     is to be registered with the Zigbee stack instance.
 * @param devAnnceCallbackData This is an opaque pointer to the data
 *     item which will be passed to the device announcement callback
 *     handler.
 * @return Returns a boolean value which will be set to 'true' if the
 *     device announcement handler has been registered with the stack or
 *     'false' if the maximum number of configured device announcement
 *     handlers has been exceeded.
 */
bool gmosZigbeeZdoServerAddDevAnnceHandler (
    gmosZigbeeStack_t* zigbeeStack,
    gmosZigbeeZdoServerDevAnnceHandler devAnnceHandler,
    void* devAnnceCallbackData);

#endif // GMOS_ZIGBEE_ZDO_SERVER_H
