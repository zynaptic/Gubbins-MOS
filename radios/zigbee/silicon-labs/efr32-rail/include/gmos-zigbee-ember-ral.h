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
 * This file defines the main target platform functions and data
 * structures for integrating the Zigbee stack into the GubbinsMOS
 * runtime framework.
 */

#ifndef GMOS_ZIGBEE_EMBER_RAL_H
#define GMOS_ZIGBEE_EMBER_RAL_H

#include <stdint.h>
#include <stdbool.h>
#include "gmos-scheduler.h"
#include "gmos-hashmap.h"
#include "gmos-zigbee-config.h"

/**
 * Only one instance of the EmberZNet stack can be running on a single
 * device. This global variable may be used from the various EmberZNet
 * callback functions to obtain a reference to the stack instance when
 * required.
 */
extern gmosZigbeeStack_t* gmosZigbeeRalEmberStackInstance;

/**
 * This enumeration defines the operating phases for the EmberZNet
 * stack state machine.
 */
typedef enum {
    ZIGBEE_STACK_PHASE_STARTUP,
    ZIGBEE_STACK_PHASE_FORMING,
    ZIGBEE_STACK_PHASE_JOINING,
    ZIGBEE_STACK_PHASE_ACTIVE,
    ZIGBEE_STACK_PHASE_FAILED
} zigbeeStackPhase_t;

/**
 * This enumeration defines the state space used by the EmberZNet
 * startup phase state machine.
 */
typedef enum {
    ZIGBEE_STACK_STATE_STARTUP_IDLE,
    ZIGBEE_STACK_STATE_STARTUP_FAILED,
    ZIGBEE_STACK_STATE_STARTUP_NETWORK_INIT,
    ZIGBEE_STACK_STATE_STARTUP_NETWORK_UP,
    ZIGBEE_STACK_STATE_STARTUP_NETWORK_CHECK,
    ZIGBEE_STACK_STATE_STARTUP_NETWORK_DOWN
} zigbeeStackStateStartup_t;

/**
 * This enumeration defines the state space used by the EmberZNet
 * network active phase state machine.
 */
typedef enum {
    ZIGBEE_STACK_STATE_ACTIVE_IDLE,
    ZIGBEE_STACK_STATE_ACTIVE_FAILED,
    ZIGBEE_STACK_STATE_ACTIVE_READY,
    ZIGBEE_STACK_STATE_ACTIVE_LEAVE_PENDING,
    ZIGBEE_STACK_STATE_ACTIVE_LEAVE_NETWORK_REQ,
    ZIGBEE_STACK_STATE_ACTIVE_LEAVE_NETWORK_CHECK
} zigbeeStackStateActive_t;

/**
 * This enumeration defines the state space used by the EmberZNet
 * network formation phase state machine.
 */
typedef enum {
    ZIGBEE_STACK_STATE_FORMING_IDLE,
    ZIGBEE_STACK_STATE_FORMING_FAILED,
    ZIGBEE_STACK_STATE_FORMING_START,
    ZIGBEE_STACK_STATE_FORMING_ENERGY_SCAN_REQ,
    ZIGBEE_STACK_STATE_FORMING_ENERGY_SCAN_CHECK,
    ZIGBEE_STACK_STATE_FORMING_ACTIVE_SCAN_REQ,
    ZIGBEE_STACK_STATE_FORMING_ACTIVE_SCAN_CHECK,
    ZIGBEE_STACK_STATE_FORMING_FORM_NETWORK_REQ,
    ZIGBEE_STACK_STATE_FORMING_FORM_NETWORK_CHECK
} zigbeeStackStateForming_t;

/**
 * This enumeration defines the state space used by the EmberZNet
 * network joining phase state machine.
 */
typedef enum {
    ZIGBEE_STACK_STATE_JOINING_IDLE,
    ZIGBEE_STACK_STATE_JOINING_FAILED,
    ZIGBEE_STACK_STATE_JOINING_START,
    ZIGBEE_STACK_STATE_JOINING_ACTIVE_SCAN_REQ,
    ZIGBEE_STACK_STATE_JOINING_ACTIVE_SCAN_CHECK,
    ZIGBEE_STACK_STATE_JOINING_RETRY_BACKOFF,
    ZIGBEE_STACK_STATE_JOINING_RETRY_WAIT,
    ZIGBEE_STACK_STATE_JOINING_JOIN_NETWORK_REQ,
    ZIGBEE_STACK_STATE_JOINING_JOIN_NETWORK_CHECK,
    ZIGBEE_STACK_STATE_JOINING_KEY_UPDATE_REQ,
    ZIGBEE_STACK_STATE_JOINING_KEY_UPDATE_CHECK,
    ZIGBEE_STACK_STATE_JOINING_LEAVE_NETWORK_REQ,
    ZIGBEE_STACK_STATE_JOINING_LEAVE_NETWORK_CHECK,
    ZIGBEE_STACK_STATE_JOINING_JOIN_NETWORK_DONE
} zigbeeStackStateJoining_t;

/**
 * This enumeration defines the state space used by the EmberZNet sleepy
 * node sleep managagement state machine.
 */
typedef enum {
    ZIGBEE_STACK_STATE_SLEEPING_INIT,
    ZIGBEE_STACK_STATE_SLEEPING_FAILED,
    ZIGBEE_STACK_STATE_SLEEPING_NOT_JOINED,
    ZIGBEE_STACK_STATE_SLEEPING_POWERED_UP,
    ZIGBEE_STACK_STATE_SLEEPING_POWERED_DOWN,
    ZIGBEE_STACK_STATE_SLEEPING_DATA_POLL_REQ,
    ZIGBEE_STACK_STATE_SLEEPING_DATA_POLL_CHECK,
    ZIGBEE_STACK_STATE_SLEEPING_DATA_POLL_RETRY,
    ZIGBEE_STACK_STATE_SLEEPING_DATA_RECEIVED,
    ZIGBEE_STACK_STATE_SLEEPING_NO_PENDING_DATA,
} zigbeeStackStateSleeping_t;

/**
 * Defines the Zigbee radio specific I/O state data structure.
 */
typedef struct gmosZigbeeRalState_t {

    // Allocate memory for the EmberZNet stack worker task.
    gmosTaskState_t emberWorkerTask;

    // Allocate memory for the EmberZNet stack tick task.
    gmosTaskState_t emberTickTask;

    // Allocate node information tables for concentrator nodes.
    #if GMOS_CONFIG_ZIGBEE_CONCENTRATOR_NODE
    gmosHashMap_t nodeInfoTable;
    gmosHashMap_t nodeAddrTable;
    #endif

    // Allocate state variables for sleepy nodes.
    #if ((GMOS_CONFIG_ZIGBEE_NODE_TYPE != GMOS_ZIGBEE_COORDINATOR_NODE) && \
        (GMOS_CONFIG_ZIGBEE_NODE_TYPE != GMOS_ZIGBEE_ROUTER_NODE))
    uint8_t zigbeeSleepState;
    uint8_t zigbeeNapCount;
    #endif

    // Specify the current EmberZNet stack core operating phase.
    uint8_t zigbeeStackPhase;

    // Specify the current EmberZNet stack core state.
    uint8_t zigbeeStackState;

    // Provide an operating phase specific data area.
    union {

        // Allocate variables for the network formation phase.
        #if (GMOS_CONFIG_ZIGBEE_NODE_TYPE == GMOS_ZIGBEE_COORDINATOR_NODE)
        struct {
            uint8_t channelIds [GMOS_CONFIG_ZIGBEE_SCAN_CANDIDATE_CHANNELS];
            int8_t  channelRssi [GMOS_CONFIG_ZIGBEE_SCAN_CANDIDATE_CHANNELS];
        } form;
        #endif

        // Allocate variables for the network joining phase.
        #if (GMOS_CONFIG_ZIGBEE_NODE_TYPE != GMOS_ZIGBEE_COORDINATOR_NODE)
        struct {
            uint8_t networkCount;
            uint8_t bestMatch;
            uint8_t extPanIdMatch;
            uint8_t randomBytes [GMOS_ZIGBEE_EXTENDED_PAN_ID_SIZE];
            uint8_t joiningKey [GMOS_ZIGBEE_ENCRYPTION_KEY_SIZE];
        } join;
        #endif

        // Allocate variables for the active processing phase.
        struct {
        #if (GMOS_CONFIG_ZIGBEE_NODE_TYPE == GMOS_ZIGBEE_COORDINATOR_NODE)
            uint32_t netJoiningTimeout;
            uint32_t tcJoiningTimeout;
            uint8_t  tcJoiningMode;
            uint8_t  coordinatorState;
        #endif
        } active;
    } phase;

} gmosZigbeeRalState_t;

/**
 * Defines the Zigbee radio specific I/O configuration options.
 */
typedef struct gmosZigbeeRalConfig_t {

} gmosZigbeeRalConfig_t;

/**
 * Implement the EmberZNet startup state machine.
 * @param zigbeeStack This is the Zigbee stack data structure that
 *     represents the EmberZNet stack interface being processed.
 * @return Returns a GMOS task status that will be used for rescheduling
 *     the associated GMOS task.
 */
gmosTaskStatus_t gmosZigbeeRalEmberStartupPhase (
    gmosZigbeeStack_t* zigbeeStack);

/**
 * Implement the EmberZNet active network state machine.
 * @param zigbeeStack This is the Zigbee stack data structure that
 *     represents the EmberZNet stack interface being processed.
 * @return Returns a GMOS task status that will be used for rescheduling
 *     the associated GMOS task.
 */
gmosTaskStatus_t gmosZigbeeRalEmberActivePhase (
    gmosZigbeeStack_t* zigbeeStack);

/**
 * Implement the EmberZNet network formation state machine.
 * @param zigbeeStack This is the Zigbee stack data structure that
 *     represents the EmberZNet stack interface being processed.
 * @return Returns a GMOS task status that will be used for rescheduling
 *     the associated GMOS task.
 */
gmosTaskStatus_t gmosZigbeeRalEmberFormNetworkPhase (
    gmosZigbeeStack_t* zigbeeStack);

/**
 * Implement the EmberZNet network joining state machine.
 * @param zigbeeStack This is the Zigbee stack data structure that
 *     represents the EmberZNet stack interface being processed.
 * @return Returns a GMOS task status that will be used for rescheduling
 *     the associated GMOS task.
 */
gmosTaskStatus_t gmosZigbeeRalEmberJoinNetworkPhase (
    gmosZigbeeStack_t* zigbeeStack);

/**
 * Implement the EmberZNet stack processing initialisation function for
 * sleepy devices.
 * @param zigbeeStack This is the Zigbee stack data structure that
 *     represents the EmberZNet stack interface being processed.
 */
void gmosZigbeeRalEmberSleepyNodeInit (
    gmosZigbeeStack_t* zigbeeStack);

/**
 * Ensure that the EmberZNet stack on a sleepy device is powered up
 * prior to using any other stack functions.
 * @param zigbeeStack This is the Zigbee stack data structure that
 *     represents the EmberZNet stack interface being processed.
 */
void gmosZigbeeRalEmberSleepyNodePowerUp (
    gmosZigbeeStack_t* zigbeeStack);

/**
 * Implement the EmberZNet stack processing tick function for sleepy
 * devices.
 * @param zigbeeStack This is the Zigbee stack data structure that
 *     represents the EmberZNet stack interface being processed.
 * @return Returns a GMOS task status that will be used for rescheduling
 *     the associated GMOS task.
 */
gmosTaskStatus_t gmosZigbeeRalEmberSleepyNodeTick (
    gmosZigbeeStack_t* zigbeeStack);

/**
 * Initialises the Zigbee coordinator state machine for Zigbee nodes
 * that are configured as network coordinators.
 * @param zigbeeStack This is the Zigbee stack data structure that
 *     represents the EmberZNet stack interface being processed.
 */
void gmosZigbeeRalCoordinatorInit (gmosZigbeeStack_t* zigbeeStack);

/**
 * Processes the Zigbee coordinator state machine while in the network
 * active state.
 * @param zigbeeStack This is the Zigbee stack data structure that
 *     represents the EmberZNet stack interface being processed.
 */
gmosTaskStatus_t gmosZigbeeRalCoordinatorTick (
    gmosZigbeeStack_t* zigbeeStack);

/**
 * Initialises the Zigbee concentrator state machine for Zigbee nodes
 * that are configured as network concentrators.
 * @param zigbeeStack This is the Zigbee stack data structure that
 *     represents the EmberZNet stack interface being processed.
 * @return Returns a boolean value which will be set to 'true' on
 *     successful initialisation and 'false' otherwise.
 */
bool gmosZigbeeRalConcentratorInit (gmosZigbeeStack_t* zigbeeStack);

#endif // GMOS_ZIGBEE_EMBER_RAL_H
