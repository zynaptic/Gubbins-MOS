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
 * This file implements the common API for accessing the Zigbee
 * concentrator support functions.
 */

// This file only needs to be compiled for concentrator nodes.
#include "gmos-zigbee-config.h"
#if GMOS_CONFIG_ZIGBEE_CONCENTRATOR_NODE

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-hashmap.h"
#include "gmos-zigbee-stack.h"
#include "gmos-zigbee-concentrator.h"

/*
 * Specify the node information data structure used by the concentrator.
 */
typedef struct gmosZigbeeConcentratorNodeInfo_t {
    uint16_t sourceRoute [GMOS_CONFIG_ZIGBEE_SOURCE_ROUTE_BLOCK_SIZE];
    uint8_t  eui64 [GMOS_ZIGBEE_MAC_ADDRESS_SIZE];
    uint8_t  flags;
} gmosZigbeeConcentratorNodeInfo_t;

/*
 * Specify the node information data flags used by the concentrator.
 */
typedef enum {
    GMOS_ZIGBEE_CONCENTRATOR_NODE_INFO_FLAG_ROUTE_VALID  = 0x01,
    GMOS_ZIGBEE_CONCENTRATOR_NODE_INFO_FLAG_ROUTE_EXTEND = 0x02,
    GMOS_ZIGBEE_CONCENTRATOR_NODE_INFO_FLAG_EUI64_VALID  = 0x04
} gmosZigbeeConcentratorNodeInfoFlags_t;

/*
 * Initialises Zigbee concentrator support on startup.
 */
bool gmosZigbeeConcentratorInit (gmosZigbeeStack_t* zigbeeStack)
{
    bool initOk = true;

    // Initialise the concentrator data tables.
    initOk = initOk && gmosHashMapInit (&(zigbeeStack->nodeInfoTable),
        2, sizeof (gmosZigbeeConcentratorNodeInfo_t), NULL);
    initOk = initOk && gmosHashMapInit (&(zigbeeStack->nodeAddrTable),
        GMOS_ZIGBEE_MAC_ADDRESS_SIZE, 2, NULL);

    return initOk;
}

/*
 * Store a Zigbee node ID and corresponding EUI64 MAC address to the
 * concentrator node device tables.
 */
bool gmosZigbeeConcentratorStoreAddress (
    gmosZigbeeStack_t* zigbeeStack, uint16_t nodeId, uint8_t* nodeEui64)
{
    gmosZigbeeConcentratorNodeInfo_t nodeInfo;
    gmosHashMap_t* nodeAddrTable = &(zigbeeStack->nodeAddrTable);
    gmosHashMap_t* nodeInfoTable = &(zigbeeStack->nodeInfoTable);
    uint16_t currentNodeId;
    uint_fast8_t i;
    bool updateNodeAddr = true;
    bool updateNodeInfo = true;
    bool storeOk = true;

    // Attempt to look up the EUI64 in the address table and delete
    // stale node ID references if required.
    if (gmosHashMapGet (nodeAddrTable,
        nodeEui64, (uint8_t*) &currentNodeId)) {
        if (currentNodeId == nodeId) {
            updateNodeAddr = false;
        } else {
            gmosHashMapDelete (nodeInfoTable, (uint8_t*) &currentNodeId);
        }
    }

    // Attempt to look up the node ID in the node information table or
    // set the default values for a new node.
    if (!gmosHashMapGet (nodeInfoTable,
        (uint8_t*) &nodeId, (uint8_t*) &nodeInfo)) {
        nodeInfo.flags = 0x00;
    }

    // Check for consistent EUI64 in the node information table entry
    // and reset the node information state if required.
    if ((nodeInfo.flags &
        GMOS_ZIGBEE_CONCENTRATOR_NODE_INFO_FLAG_EUI64_VALID) != 0) {
        for (i = 0; i < GMOS_ZIGBEE_MAC_ADDRESS_SIZE; i++) {
            if (nodeInfo.eui64 [i] != nodeEui64 [i]) {
                nodeInfo.flags = 0x00;
            }
        }
        if (nodeInfo.flags != 0x00) {
            updateNodeInfo = false;
        }
    }

    // Update the address table if required.
    if (storeOk && updateNodeAddr) {
        storeOk = gmosHashMapPut (nodeAddrTable,
            nodeEui64, (uint8_t*) &nodeId);
    }

    // Update the node information table if required.
    if (storeOk && updateNodeInfo) {
        for (i = 0; i < GMOS_ZIGBEE_MAC_ADDRESS_SIZE; i++) {
            nodeInfo.eui64 [i] = nodeEui64 [i];
        }
        nodeInfo.flags |=
            GMOS_ZIGBEE_CONCENTRATOR_NODE_INFO_FLAG_EUI64_VALID;
        storeOk = gmosHashMapPut (nodeInfoTable,
            (uint8_t*) &nodeId, (uint8_t*) &nodeInfo);
    }
    return storeOk;
}

/*
 * Removes the specified Zigbee node address and all associated records
 * from the concentrator node device tables.
 */
bool gmosZigbeeConcentratorDeleteAddress (
    gmosZigbeeStack_t* zigbeeStack, uint8_t* nodeEui64)
{
    gmosHashMap_t* nodeInfoTable = &(zigbeeStack->nodeInfoTable);
    gmosHashMap_t* nodeAddrTable = &(zigbeeStack->nodeAddrTable);
    uint16_t nodeId;
    bool deleteOk = false;

    // Check for an existing node address and delete all associated
    // table entries.
    if (gmosHashMapGet (nodeAddrTable, nodeEui64, (uint8_t*) &nodeId)) {
        gmosHashMapDelete (nodeAddrTable, nodeEui64);
        gmosHashMapDelete (nodeInfoTable, (uint8_t*) &nodeId);
        deleteOk = true;
    }
    return deleteOk;
}

/*
 * Removes the specified Zigbee node ID and all associated records from
 * the concentrator node device tables.
 */
bool gmosZigbeeConcentratorDeleteNodeId (
    gmosZigbeeStack_t* zigbeeStack, uint16_t nodeId)
{
    gmosHashMap_t* nodeInfoTable = &(zigbeeStack->nodeInfoTable);
    gmosHashMap_t* nodeAddrTable = &(zigbeeStack->nodeAddrTable);
    gmosZigbeeConcentratorNodeInfo_t nodeInfo;
    bool deleteOk = false;

    // Check for an existing node information record and delete all
    // associated table entries.
    if (gmosHashMapGet (nodeInfoTable,
        (uint8_t*) &nodeId, (uint8_t*) &nodeInfo)) {
        if ((nodeInfo.flags &
            GMOS_ZIGBEE_CONCENTRATOR_NODE_INFO_FLAG_EUI64_VALID) != 0) {
            gmosHashMapDelete (nodeAddrTable, nodeInfo.eui64);
        }
        gmosHashMapDelete (nodeInfoTable, (uint8_t*) &nodeId);
        deleteOk = true;
    }
    return deleteOk;
}

/*
 * Store a received source routing update to the concentrator node
 * device tables.
 */
bool gmosZigbeeConcentratorStoreRoute (
    gmosZigbeeStack_t* zigbeeStack, uint16_t nodeId,
    uint8_t relayCount, uint8_t* relayList)
{
    gmosHashMap_t* nodeInfoTable = &(zigbeeStack->nodeInfoTable);
    gmosZigbeeConcentratorNodeInfo_t nodeInfo;
    uint16_t currentNodeId = nodeId;
    uint_fast16_t nextNodeId;
    uint_fast8_t i;
    uint8_t* relayPtr = relayList;
    bool routeMatched;
    bool routeExtend;
    bool storeOk = true;

    // The route may be split into multiple intermediate node updates.
    while (true) {

        // Attempt to read back an existing node information table entry
        // or set the default values for a new node.
        if (!gmosHashMapGet (nodeInfoTable,
            (uint8_t*) &currentNodeId, (uint8_t*) &nodeInfo)) {
            nodeInfo.flags = 0x00;
        } else {
            nodeInfo.flags &=
                ~(GMOS_ZIGBEE_CONCENTRATOR_NODE_INFO_FLAG_ROUTE_VALID |
                GMOS_ZIGBEE_CONCENTRATOR_NODE_INFO_FLAG_ROUTE_EXTEND);
        }

        // Populate the table source routing information. Skip further
        // processing if the new route matches the existing route.
        routeMatched = ((nodeInfo.flags &
            GMOS_ZIGBEE_CONCENTRATOR_NODE_INFO_FLAG_ROUTE_VALID) != 0) ?
            true : false;
        for (i = 0; i < GMOS_CONFIG_ZIGBEE_SOURCE_ROUTE_BLOCK_SIZE; i++) {
            if (i >= relayCount) {
                nextNodeId = GMOS_ZIGBEE_NULL_NODE_ID;
            } else {
                nextNodeId = (uint16_t) *(relayPtr++);
                nextNodeId |= ((uint16_t) *(relayPtr++)) << 8;
            }
            if ((!routeMatched) || (nodeInfo.sourceRoute [i] != nextNodeId)) {
                nodeInfo.sourceRoute [i] = nextNodeId;
                routeMatched = false;
            }
        }
        if (routeMatched) {
            break;
        }

        // Set the node information flags and determine if an extended
        // source route is required.
        if (relayCount > GMOS_CONFIG_ZIGBEE_SOURCE_ROUTE_BLOCK_SIZE) {
            routeExtend = true;
            nodeInfo.flags |=
                GMOS_ZIGBEE_CONCENTRATOR_NODE_INFO_FLAG_ROUTE_VALID |
                GMOS_ZIGBEE_CONCENTRATOR_NODE_INFO_FLAG_ROUTE_EXTEND;
        } else {
            routeExtend = false;
            nodeInfo.flags |=
                GMOS_ZIGBEE_CONCENTRATOR_NODE_INFO_FLAG_ROUTE_VALID;
        }

        // Store the table entry.
        if (!gmosHashMapPut (nodeInfoTable,
            (uint8_t*) &currentNodeId, (uint8_t*) &nodeInfo)) {
            storeOk = false;
            break;
        }

        // Loop until the route is complete.
        if (routeExtend) {
            currentNodeId = nextNodeId;
            relayCount -= GMOS_CONFIG_ZIGBEE_SOURCE_ROUTE_BLOCK_SIZE;
        } else {
            break;
        }
    }
    return storeOk;
}

/*
 * Bootstraps a source routing table entry on device joining. Given a
 * known parent node ID, this will set up a bootstrapped source routing
 * table entry that routes to the new node via the parent node.
 */
bool gmosZigbeeConcentratorBootstrapRoute (
    gmosZigbeeStack_t* zigbeeStack, uint16_t nodeId,
    uint16_t parentNodeId)
{
    gmosHashMap_t* nodeInfoTable = &(zigbeeStack->nodeInfoTable);
    gmosZigbeeConcentratorNodeInfo_t nodeInfo;
    uint_fast8_t i;
    bool bootstrapOk;

    // Attempt to read back the parent node information table entry and
    // check that it has a valid route. This is not required for nodes
    // that are joined directly to the concentrator.
    bootstrapOk = false;
    if (parentNodeId == zigbeeStack->currentNodeId) {
        parentNodeId = GMOS_ZIGBEE_NULL_NODE_ID;
        bootstrapOk = true;
    } else if (gmosHashMapGet (nodeInfoTable,
        (uint8_t*) &parentNodeId, (uint8_t*) &nodeInfo)) {
        if ((nodeInfo.flags &
            GMOS_ZIGBEE_CONCENTRATOR_NODE_INFO_FLAG_ROUTE_VALID) != 0) {
            bootstrapOk = true;
        }
    }

    // Attempt to read back an existing node information table entry or
    // set the default values for a new node.
    if (bootstrapOk) {
        if (!gmosHashMapGet (nodeInfoTable,
            (uint8_t*) &nodeId, (uint8_t*) &nodeInfo)) {
            nodeInfo.flags = 0x00;
        } else {
            nodeInfo.flags &=
                ~(GMOS_ZIGBEE_CONCENTRATOR_NODE_INFO_FLAG_ROUTE_VALID |
                GMOS_ZIGBEE_CONCENTRATOR_NODE_INFO_FLAG_ROUTE_EXTEND);
        }

        // Populate the source routing table with a single entry for the
        // parent node.
        nodeInfo.sourceRoute [0] = parentNodeId;
        for (i = 1; i < GMOS_CONFIG_ZIGBEE_SOURCE_ROUTE_BLOCK_SIZE; i++) {
            nodeInfo.sourceRoute [i] = GMOS_ZIGBEE_NULL_NODE_ID;
        }

        // Set the flags according to whether the new node is joined
        // directly to the concentrator or whether it is reachable
        // through an intermediate node.
        if (parentNodeId == GMOS_ZIGBEE_NULL_NODE_ID) {
            nodeInfo.flags |=
                GMOS_ZIGBEE_CONCENTRATOR_NODE_INFO_FLAG_ROUTE_VALID;
        } else {
            nodeInfo.flags |=
                GMOS_ZIGBEE_CONCENTRATOR_NODE_INFO_FLAG_ROUTE_VALID |
                GMOS_ZIGBEE_CONCENTRATOR_NODE_INFO_FLAG_ROUTE_EXTEND;
        }

        // Attempt to update the routing table entry.
        bootstrapOk = gmosHashMapPut (nodeInfoTable,
            (uint8_t*) &nodeId, (uint8_t*) &nodeInfo);
    }
    return bootstrapOk;
}

/*
 * Invalidates a source routing entry in the concentrator node device
 * tables. This will usually occur after a failure to deliver a message
 * using the existing source routing information.
 */
bool gmosZigbeeConcentratorInvalidateRoute (
    gmosZigbeeStack_t* zigbeeStack, uint16_t nodeId)
{
    gmosHashMap_t* nodeInfoTable = &(zigbeeStack->nodeInfoTable);
    gmosZigbeeConcentratorNodeInfo_t nodeInfo;
    bool invalidateOk = false;

    // Check for an existing node information record and reset the
    // source routing flags.
    if (gmosHashMapGet (nodeInfoTable,
        (uint8_t*) &nodeId, (uint8_t*) &nodeInfo)) {
        if ((nodeInfo.flags &
            GMOS_ZIGBEE_CONCENTRATOR_NODE_INFO_FLAG_ROUTE_VALID) != 0) {
            nodeInfo.flags &=
                ~(GMOS_ZIGBEE_CONCENTRATOR_NODE_INFO_FLAG_ROUTE_VALID |
                GMOS_ZIGBEE_CONCENTRATOR_NODE_INFO_FLAG_ROUTE_EXTEND);
            gmosHashMapPut (nodeInfoTable,
                (uint8_t*) &nodeId, (uint8_t*) &nodeInfo);
        }
        invalidateOk = true;
    }
    return invalidateOk;
}

/*
 * Read back a stored source route from the concentrator node device
 * tables.
 */
bool gmosZigbeeConcentratorReadRoute (
    gmosZigbeeStack_t* zigbeeStack, uint16_t nodeId,
    uint8_t maxRelayCount, uint8_t* relayCount, uint8_t* relayList)
{
    gmosHashMap_t* nodeInfoTable = &(zigbeeStack->nodeInfoTable);
    gmosZigbeeConcentratorNodeInfo_t nodeInfo;
    uint16_t currentNodeId = nodeId;
    uint_fast16_t nextNodeId;
    uint_fast8_t i;
    uint8_t* relayPtr;
    uint_fast8_t newRelayCount;
    uint_fast8_t totalRelayCount = 0;
    bool readOk = true;

    // The route may be split into multiple intermediate node records.
    while (true) {

        // Read back an existing node information table entry.
        if (!gmosHashMapGet (nodeInfoTable,
            (uint8_t*) &currentNodeId, (uint8_t*) &nodeInfo)) {
            readOk = false;
            break;
        }

        // Check for a valid route flag.
        if ((nodeInfo.flags &
            GMOS_ZIGBEE_CONCENTRATOR_NODE_INFO_FLAG_ROUTE_VALID) == 0) {
            readOk = false;
            break;
        }

        // Determine the number of new relays for the current node.
        for (i = 0; i < GMOS_CONFIG_ZIGBEE_SOURCE_ROUTE_BLOCK_SIZE; i++) {
            if (nodeInfo.sourceRoute [i] == GMOS_ZIGBEE_NULL_NODE_ID) {
                break;
            }
        }
        newRelayCount = i;

        // Shift the current relay list contents to make space for the
        // new relay entries.
        if ((totalRelayCount + newRelayCount) > maxRelayCount) {
            readOk = false;
            break;
        } else if ((totalRelayCount > 0) && (newRelayCount > 0)) {
            relayPtr = relayList + 2 * totalRelayCount - 1;
            for (i = 0; i < 2 * totalRelayCount; i++) {
                *(relayPtr + 2 * newRelayCount) = *(relayPtr);
                relayPtr -= 1;
            }
        }

        // Insert the new relay entries as required.
        nextNodeId = GMOS_ZIGBEE_NULL_NODE_ID;
        relayPtr = relayList + 2 * newRelayCount - 1;
        for (i = 0; i < newRelayCount; i++) {
            nextNodeId = nodeInfo.sourceRoute [i];
            *(relayPtr--) = (uint8_t) (nextNodeId >> 8);
            *(relayPtr--) = (uint8_t) (nextNodeId);
        }

        // Process the next node in the chain or complete when the
        // extended route flag is clear.
        totalRelayCount += newRelayCount;
        if ((nodeInfo.flags &
            GMOS_ZIGBEE_CONCENTRATOR_NODE_INFO_FLAG_ROUTE_EXTEND) != 0) {
            currentNodeId = nextNodeId;
        } else {
            break;
        }
    }
    if (readOk) {
        *relayCount = totalRelayCount;
    }
    return readOk;
}

#endif // GMOS_CONFIG_ZIGBEE_CONCENTRATOR_NODE
