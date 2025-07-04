/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2025 Zynaptic Limited
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
 * This header defines various utility functions for working with the
 * EmberZNet Zigbee network stack.
 */

#ifndef GMOS_ZIGBEE_EMBER_UTILS_H
#define GMOS_ZIGBEE_EMBER_UTILS_H

#include <stdbool.h>
#include "gmos-zigbee-stack.h"
#include "gmos-zigbee-aps.h"
#include "sl_zigbee.h"

/**
 * Specify the maximum APS message buffer size to be used internally.
 */
#define GMOS_ZIGBEE_APS_MESSAGE_BUFFER_SIZE 96

/**
 * Maps the EmberZNet stack status code values to their corresponding
 * GubbinsMOS Zigbee status values.
 * @param slStatus This is the EmberZNet status value, specified using
 *     the vendor specific status enumeration.
 * @return Returns the status value, specified using the GubbinsMOS
 *     vendor independent enumeration.
 */
gmosZigbeeStatus_t gmosZigbeeConvertEmberStatusCode (
    sl_status_t slStatus);

/**
 * Maps all the EmberZNet stack status values to their corresponding
 * GubbinsMOS Zigbee status values, with inline evaluation of the most
 * commonly used success code.
 * @param slStatus This is the EmberZNet status value, specified using
 *     the vendor specific status enumeration.
 * @return Returns the status value, specified using the GubbinsMOS
 *     vendor independent enumeration.
 */
static inline gmosZigbeeStatus_t gmosZigbeeConvertEmberStatus (
    sl_status_t slStatus)
{
    return (slStatus == SL_STATUS_OK) ?
        GMOS_ZIGBEE_STATUS_SUCCESS :
        gmosZigbeeConvertEmberStatusCode (slStatus);
}

/**
 * Maps the EmberZNet incoming message type encodings to their
 * corresponding GubbinsMOS APS message types.
 * @param slMessageType This is the incoming APS message type, specified
 *     using the EmberZNet stack enumeration.
 * @return Returns the incoming APS message type, specified using the
 *     GubbinsMOS vendor independent enumeration.
 */
gmosZigbeeApsMsgType_t gmosZigbeeConvertEmberIncomingMessageType (
    sl_zigbee_incoming_message_type_t slMessageType);

/**
 * Maps the EmberZNet outgoing message type encodings to their
 * corresponding GubbinsMOS APS message types.
 * @param slMessageType This is the outgoing APS message type, specified
 *     using the EmberZNet stack enumeration.
 * @return Returns the outgoing APS message type, specified using the
 *     GubbinsMOS vendor independent enumeration.
 */
gmosZigbeeApsMsgType_t gmosZigbeeConvertEmberOutgoingMessageType (
    sl_zigbee_outgoing_message_type_t slMessageType);

/**
 * Maps the EmberZNet APS message option flags to their corresponding
 * GubbinsMOS APS message flags.
 * @param slMessageFlags This is the set of APS message flags, specified
 *     using the EmberZNet bitfield layout.
 * @return Returns the set of APS message flags, specified using the
 *     GubbinsMOS vendor independent bitfield layout.
 */
uint8_t gmosZigbeeConvertEmberApsMessageFlags (
    sl_zigbee_aps_option_t slMessageFlags);

#endif // GMOS_ZIGBEE_EMBER_UTILS_H
