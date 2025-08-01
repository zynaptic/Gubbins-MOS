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
 * This header defines the common API for accessing the Zigbee
 * concentrator support functions.
 */

#ifndef GMOS_ZIGBEE_CONCENTRATOR_H
#define GMOS_ZIGBEE_CONCENTRATOR_H

#include <stdint.h>
#include <stdbool.h>
#include "gmos-zigbee-stack.h"

/**
 * Specifies the number of entries in a source route block.
 */
#ifndef GMOS_CONFIG_ZIGBEE_SOURCE_ROUTE_BLOCK_SIZE
#define GMOS_CONFIG_ZIGBEE_SOURCE_ROUTE_BLOCK_SIZE 4
#endif

/**
 * Specifies the nominal interval at which the concentrator will
 * broadcast many to one route requests to the network, expressed as
 * an integer number of seconds.
 */
#ifndef GMOS_CONFIG_ZIGBEE_SOURCE_ROUTE_MTORR_INTERVAL
#define GMOS_CONFIG_ZIGBEE_SOURCE_ROUTE_MTORR_INTERVAL 60
#endif

/**
 * Initialises Zigbee concentrator support on startup.
 * @param zigbeeStack This is the Zigbee stack instance for the
 *     concentrator node.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully completing the initialisation process and 'false'
 *     otherwise.
 */
bool gmosZigbeeConcentratorInit (gmosZigbeeStack_t* zigbeeStack);

/**
 * Store a Zigbee node ID and corresponding EUI64 MAC address to the
 * concentrator node device tables.
 * @param zigbeeStack This is the Zigbee stack instance for the
 *     concentrator that is to store the specified address information.
 * @param nodeId This is the short node identifier which is to be stored
 *     in the Zigbee stack address tables.
 * @param nodeEui64 This is a pointer to the 64-bit MAC address which is
 *     to be stored in the Zigbee stack address tables.
 * @return Returns a boolean value which will be set to 'true' if the
 *     specified address information was successfully stored and 'false'
 *     if there is insufficient capacity in the tables.
 */
bool gmosZigbeeConcentratorStoreAddress (
    gmosZigbeeStack_t* zigbeeStack, uint16_t nodeId, uint8_t* nodeEui64);

/**
 * Removes the specified Zigbee node address and all associated records
 * from the concentrator node device tables.
 * @param zigbeeStack This is the Zigbee stack instance for the
 *     concentrator that holds the specified node information.
 * @param nodeEui64 This is a pointer to the 64-bit MAC address which is
 *     to be removed from the concentrator node device tables.
 * @return Returns a boolean value which will be set to 'true' if the
 *     specified node address was successfully removed and 'false' if
 *     the node address could not be found in the tables.
 */
bool gmosZigbeeConcentratorDeleteAddress (
    gmosZigbeeStack_t* zigbeeStack, uint8_t* nodeEui64);

/**
 * Removes the specified Zigbee node ID and all associated records from
 * the concentrator node device tables.
 * @param zigbeeStack This is the Zigbee stack instance for the
 *     concentrator that holds the specified node information.
 * @param nodeId This is the short node identifier which is to be
 *     removed from the concentrator node device tables.
 * @return Returns a boolean value which will be set to 'true' if the
 *     specified node ID was successfully removed and 'false' if the
 *     node ID could not be found in the tables.
 */
bool gmosZigbeeConcentratorDeleteNodeId (
    gmosZigbeeStack_t* zigbeeStack, uint16_t nodeId);

/**
 * Store a received source routing update to the concentrator node
 * device tables.
 * @param zigbeeStack This is the Zigbee stack instance for the
 *     concentrator that is to store the specified source routing
 *     information.
 * @param nodeId This is the short node identifier of the remote node
 *     for which the source routing information is being stored.
 * @param relayCount This specifies the number of intermediate relay
 *     nodes that are used to reach the remote node.
 * @param relayList This is a byte array which contains the list of
 *     intermediate nodes in the standard Zigbee route record command
 *     format. Node ID values are formatted least significant byte
 *     first, and they are ordered with the node closest to the remote
 *     node being the first in the list.
 * @return Returns a boolean value which will be set to 'true' if the
 *     specified routing table information was successfully stored and
 *     'false' if there is insufficient capacity in the tables.
 */
bool gmosZigbeeConcentratorStoreRoute (
    gmosZigbeeStack_t* zigbeeStack, uint16_t nodeId,
    uint8_t relayCount, uint8_t* relayList);

/**
 * Bootstraps a source routing table entry on device joining. Given a
 * known parent node ID, this will set up a bootstrapped source routing
 * table entry that routes to the new node via the parent node.
 * @param zigbeeStack This is the Zigbee stack instance for the
 *     concentrator that is to store the specified source routing
 *     information.
 * @param nodeId This is the short node identifier of the remote node
 *     for which the source routing information is being stored.
 * @param parentNodeId This is the short node identifier of the parent
 *     node that will initially be used for routing messages to the
 *     new node.
 * @return Returns a boolean value which will be set to 'true' if the
 *     specified routing table information was successfully stored and
 *     'false' if there is insufficient capacity in the tables or there
 *     is no valid routing information for the specified parent node.
 */
bool gmosZigbeeConcentratorBootstrapRoute (
    gmosZigbeeStack_t* zigbeeStack, uint16_t nodeId,
    uint16_t parentNodeId);

/**
 * Invalidates a source routing entry in the concentrator node device
 * tables. This will usually occur after a failure to deliver a message
 * using the existing source routing information.
 * @param zigbeeStack This is the Zigbee stack instance for the
 *     concentrator that is to invalidate the specified source routing
 *     information.
 * @param nodeId This is the short node identifier of the remote node
 *     for which the source routing information is being invalidated.
 * @return Returns a boolean value which will be set to 'true' if the
 *     specified routing table information was successfully invalidated
 *     and 'false' if the node ID could not be found in the tables.
 */
bool gmosZigbeeConcentratorInvalidateRoute (
    gmosZigbeeStack_t* zigbeeStack, uint16_t nodeId);

/**
 * Read back a stored source route from the concentrator node device
 * tables.
 * @param zigbeeStack This is the Zigbee stack instance for the
 *     concentrator that is to be accessed for the stored source routing
 *     information.
 * @param nodeId This is the short node identifier of the remote node
 *     for which the source routing information is being requested.
 * @param maxRelayCount This is the maximum number of source route relay
 *     entries that may be stored in the relay list.
 * @param relayCount This is a pointer to a relay count variable that
 *     on successful completion will be updated with the number of
 *     entries added to the relay list.
 * @param relayList This is a byte array which on successful completion
 *     will contain the list of intermediate nodes in the standard
 *     Zigbee source route header format. Node ID values are formatted
 *     least significant byte first, and they are ordered with the node
 *     closest to the concentrator node being the first in the list.
 * @return Returns a boolean value which will be set to 'true' if the
 *     specified routing table information was successfully retrieved
 *     and 'false' if a suitable route was not available or if the route
 *     exceeded the specified maximum relay count.
 */
bool gmosZigbeeConcentratorReadRoute (
    gmosZigbeeStack_t* zigbeeStack, uint16_t nodeId,
    uint8_t maxRelayCount, uint8_t* relayCount, uint8_t* relayList);

#endif // GMOS_ZIGBEE_CONCENTRATOR_H
