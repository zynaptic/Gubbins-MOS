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
 * This file implements the concentrator node support functions for the
 * EmberZNet Zigbee stack implementation.
 */

// This file only needs to be compiled for concentrator nodes.
#include "gmos-zigbee-config.h"
#if (GMOS_CONFIG_ZIGBEE_CONCENTRATOR_NODE == true)

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-zigbee-stack.h"
#include "gmos-zigbee-concentrator.h"
#include "gmos-zigbee-ember-ral.h"
#include "sl_zigbee.h"
#include "source-route.h"

/*
 * Initialises the Zigbee concentrator state machine for Zigbee nodes
 * that are configured as network concentrators.
 * @param zigbeeStack This is the Zigbee stack data structure that
 *     represents the EmberZNet stack interface being processed.
 */
bool gmosZigbeeRalConcentratorInit (gmosZigbeeStack_t* zigbeeStack)
{
    (void) zigbeeStack;
    sl_status_t slStatus;

    // Periodic route request messages are automatically issued by the
    // EmberZNet stack when it is configured as a concentrator node.
    // Route requests should also be generated after three route error
    // messages have been received or there has been one delivery
    // failure.
    slStatus = sl_zigbee_set_concentrator (true,
        SL_ZIGBEE_HIGH_RAM_CONCENTRATOR,
        GMOS_CONFIG_ZIGBEE_SOURCE_ROUTE_MTORR_INTERVAL / 5,
        GMOS_CONFIG_ZIGBEE_SOURCE_ROUTE_MTORR_INTERVAL, 3, 1, 0);
    GMOS_LOG_FMT (LOG_DEBUG,
        "EmberZNet initialised concentrator with status 0x%04X.",
        slStatus);

    return (slStatus == SL_STATUS_OK) ? true : false;
}

/*
 * Implement stack callback for updating the source routing tables on
 * receiving a new route record from a remote device.
 */
void sl_zigbee_override_incoming_route_record_handler (
    sl_zigbee_rx_packet_info_t *packetInfo, uint8_t relayCount,
    uint8_t *relayList, bool* consumed)
{
    gmosZigbeeStack_t* zigbeeStack = gmosZigbeeRalEmberStackInstance;
    uint16_t nodeId;
    uint8_t* nodeEui64;

    // Make sure the node address information is consistent and then
    // add the new source routing information.
    nodeId = packetInfo->sender_short_id;
    nodeEui64 = packetInfo->sender_long_id;
    if (gmosZigbeeConcentratorStoreAddress (
        zigbeeStack, nodeId, nodeEui64)) {
        if (gmosZigbeeConcentratorStoreRoute (
            zigbeeStack, nodeId, relayCount, relayList)) {
            GMOS_LOG_FMT (LOG_DEBUG,
                "EmberZNet stored route update for node ID 0x%04X.",
                nodeId);
        }
    }
    *consumed = true;
}

/*
 * Implement stack callback for appending the source routing header to
 * outgoing messages.
 */
uint8_t sl_zigbee_internal_override_append_source_route_handler (
    sl_802154_short_addr_t destination,
    sli_buffer_manager_buffer_t* header, bool* consumed)
{
    gmosZigbeeStack_t* zigbeeStack = gmosZigbeeRalEmberStackInstance;
    uint8_t relayList [2 + 2 * SL_ZIGBEE_MAX_SOURCE_ROUTE_RELAY_COUNT];
    uint8_t relayCount = SL_ZIGBEE_MAX_SOURCE_ROUTE_RELAY_COUNT;
    sl_status_t slStatus;
    uint8_t retVal = 0;

    // Attempt to retrieve the stored source route for the device.
    if ((header != NULL) && gmosZigbeeConcentratorReadRoute (
        zigbeeStack, destination, relayCount, &relayCount, relayList + 2)) {

        // Populate the relay list length and index fields.
        relayList [0] = relayCount;
        relayList [1] = 0;

        // Attempt to append the source route to the header.
        slStatus = sl_legacy_buffer_manager_really_append_to_linked_buffers (
            header, relayList, 2 + 2 * relayCount, true);
        if (slStatus == SL_STATUS_OK) {
            retVal = 2 + 2 * relayCount;
        }
    }
    *consumed = true;
    return retVal;
}

/*
 * Implement stack callback for removing conflicting node IDs from the
 * concentrator tables.
 */
void sl_zigbee_id_conflict_handler (sl_802154_short_addr_t nodeId)
{
    gmosZigbeeStack_t* zigbeeStack = gmosZigbeeRalEmberStackInstance;

    // Check for an existing node information record and delete all
    // associated table entries.
    GMOS_LOG_FMT (LOG_DEBUG,
        "EmberZNet received node ID conflict for node ID 0x%04X.",
        nodeId);
    gmosZigbeeConcentratorDeleteNodeId (zigbeeStack, nodeId);
}

/*
 * Implement stack callback for invalidating stored source routing
 * information after a message routing failure.
 */
void sl_zigbee_incoming_route_error_handler (
    sl_status_t status, sl_802154_short_addr_t nodeId)
{
    gmosZigbeeStack_t* zigbeeStack = gmosZigbeeRalEmberStackInstance;

    // Check for an existing node information record and invalidate the
    // stored source routing information.
    GMOS_LOG_FMT (LOG_DEBUG,
        "EmberZNet received route error 0x%04X for node ID 0x%04X.",
            status, nodeId);
    gmosZigbeeConcentratorInvalidateRoute (zigbeeStack, nodeId);
}

// Select the appropriate API stubs. For concentrator nodes these are
// the baremetal adaptor functions and for other nodes, these are the
// source routing stub functions that should override library linking.
#include "internal/src/baremetal/source-route-baremetal-wrapper.c"
#else
#include "zigbee/source-route-stub.c"
#endif // GMOS_CONFIG_ZIGBEE_CONCENTRATOR_NODE
