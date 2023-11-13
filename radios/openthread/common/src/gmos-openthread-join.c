/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2023 Zynaptic Limited
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
 * This file implements the common API for managing the OpenThread stack
 * as a network joiner.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-scheduler.h"
#include "gmos-openthread.h"
#include "gmos-openthread-join.h"
#include "openthread/ip6.h"
#include "openthread/thread.h"
#include "openthread/dataset.h"
#include "openthread/joiner.h"
#include "openthread/netdata.h"
#include "openthread/dns_client.h"

// Define the range of startup delays to use before initiating the
// network joining process, expressed in milliseconds.
#define GMOS_OPENTHREAD_JOIN_STARTUP_DELAY_MIN (1 * 1000)
#define GMOS_OPENTHREAD_JOIN_STARTUP_DELAY_MAX (8 * 1000)

/*
 * Specify the state space for the OpenThread joiner state machine.
 */
typedef enum {
    GMOS_OPENTHREAD_JOIN_STATE_INIT,
    GMOS_OPENTHREAD_JOIN_ENABLE_IPV6,
    GMOS_OPENTHREAD_JOIN_IDLE,
    GMOS_OPENTHREAD_JOIN_ACTIVE,
    GMOS_OPENTHREAD_JOIN_ENABLE_THREAD,
    GMOS_OPENTHREAD_JOIN_CONNECTING,
    GMOS_OPENTHREAD_JOIN_CONFIGURE_DNS,
    GMOS_OPENTHREAD_JOIN_MONITOR_NETWORK,
    GMOS_OPENTHREAD_JOIN_DISCONNECTED,
    GMOS_OPENTHREAD_JOIN_FAILED,
} gmosOpenThreadJoinState_t;

/*
 * Gets a random joining startup delay in the specified range.
 */
static inline gmosTaskStatus_t gmosOpenThreadJoinDelay (void)
{
    uint32_t delayRange;
    uint32_t delayTicks;

    // Use the platform random number generator.
    gmosPalGetRandomBytes ((uint8_t*) &delayTicks, sizeof (delayTicks));

    // Limit the delay range. This doesn't give an even distribution
    // but is adequate for calculating delays.
    delayRange = GMOS_MS_TO_TICKS (
        GMOS_OPENTHREAD_JOIN_STARTUP_DELAY_MAX -
        GMOS_OPENTHREAD_JOIN_STARTUP_DELAY_MIN);
    while (delayTicks > 8 * delayRange) {
        delayTicks /= 8;
    }
    while (delayTicks > delayRange) {
        delayTicks -= delayRange;
    }

    // Add in the minimum delay offset.
    delayTicks +=
        GMOS_MS_TO_TICKS (GMOS_OPENTHREAD_JOIN_STARTUP_DELAY_MIN);
    GMOS_LOG_FMT (LOG_DEBUG, "OpenThread : Joiner startup delay %d ms.",
        GMOS_TICKS_TO_MS (delayTicks));

    return GMOS_TASK_RUN_LATER (delayTicks);
}

/*
 * Enable the IPv6 stack and determine if the device is already joined
 * to a network.
 */
static inline bool gmosOpenThreadJoinEnableIpv6 (
    gmosOpenThreadStack_t* openThreadStack, bool* deviceJoined)
{
    otInstance* otStack = openThreadStack->otInstance;
    otError otStatus;

    // Attempt to enable IPv6 interface.
    otStatus = otIp6SetEnabled (otStack, true);
    if (otStatus != OT_ERROR_NONE) {
        GMOS_LOG_FMT (LOG_ERROR,
            "OpenThread : Failed to open IPv6 interface (status %d).",
            otStatus);
        return false;
    }

    // Determine if the device is already joined to a network.
    *deviceJoined = otDatasetIsCommissioned (otStack);
    return true;
}

/*
 * Enable the thread stack with valid network parameters.
 */
static inline bool gmosOpenThreadJoinEnableThread (
    gmosOpenThreadStack_t* openThreadStack)
{
    otInstance* otStack = openThreadStack->otInstance;
    otError otStatus;

    // Attempt to enable the thread stack.
    otStatus = otThreadSetEnabled (otStack, true);
    if (otStatus != OT_ERROR_NONE) {
        GMOS_LOG_FMT (LOG_ERROR,
            "OpenThread : Failed to enable thread stack (status %d).",
            otStatus);
        return false;
    } else {
        return true;
    }
}

/*
 * Monitor the device connecting state and wait for the thread stack to
 * assign a valid operating role to the node.
 */
static inline bool gmosOpenThreadJoinConnecting (
    gmosOpenThreadStack_t* openThreadStack, bool* deviceJoined)
{
    otInstance* otStack = openThreadStack->otInstance;
    otDeviceRole deviceRole;

    // Get the current device role.
    deviceRole = otThreadGetDeviceRole (otStack);

    // Determine if the device has connected to the network.
    if ((deviceRole == OT_DEVICE_ROLE_LEADER) ||
        (deviceRole == OT_DEVICE_ROLE_ROUTER) ||
        (deviceRole == OT_DEVICE_ROLE_CHILD)) {
        *deviceJoined = true;
    } else {
        *deviceJoined = false;
    }
    return true;
}

/*
 * Attempt to configure the DNS server. This requires the presence of
 * a border router that supports the DNS/SRP unicast service, which is
 * identified by the service number 0x5D.
 */
static inline bool gmosOpenThreadJoinConfigureDns (
    gmosOpenThreadStack_t* openThreadStack)
{
    otInstance* otStack = openThreadStack->otInstance;
    otNetworkDataIterator serviceIter = OT_NETWORK_DATA_ITERATOR_INIT;
    bool serviceMatch = false;
    otServiceConfig serviceConfig;
    otError otStatus;
    uint8_t* serverData;
    otDnsQueryConfig dnsConfig = { 0 };
    uint_fast8_t i;

    // Search the available network services for the first instance of
    // the DNS/SRP unicast service.
    do {
        otStatus = otNetDataGetNextService (
            otStack, &serviceIter, &serviceConfig);
        if (otStatus == OT_ERROR_NONE) {
            if ((serviceConfig.mServiceDataLength == 1) &&
                (serviceConfig.mServiceData [0] == 0x5D) &&
                (serviceConfig.mServerConfig.mServerDataLength == 18) &&
                (serviceConfig.mServerConfig.mStable)) {
                serviceMatch = true;
                break;
            }
        }
    } while (otStatus == OT_ERROR_NONE);
    if (!serviceMatch) {
        return false;
    }

    // Extract the DNS server address from the server data. The address
    // is the first 16 bytes of the server data and the port number is
    // ignored, since the default DNS port should be used for queries.
    // Unspecified configuration fields that are set to zero will use
    // the OpenThread default DNS client settings.
    serverData = serviceConfig.mServerConfig.mServerData;
    for (i = 0; i < 16; i++) {
        dnsConfig.mServerSockAddr.mAddress.mFields.m8 [i] = serverData [i];
    }
    otDnsClientSetDefaultConfig (otStack, &dnsConfig);
    return true;
}

/*
 * Monitor the active network state.
 */
static inline bool gmosOpenThreadJoinMonitorNetwork (
    gmosOpenThreadStack_t* openThreadStack, bool* deviceJoined)
{
    otInstance* otStack = openThreadStack->otInstance;
    otDeviceRole deviceRole;

    // Get the current device role.
    deviceRole = otThreadGetDeviceRole (otStack);
    if (GMOS_CONFIG_LOG_LEVEL <= LOG_DEBUG) {
        static otDeviceRole debugDeviceRole = OT_DEVICE_ROLE_DISABLED;
        if (deviceRole != debugDeviceRole) {
            GMOS_LOG_FMT (LOG_DEBUG,
                "OpenThread : Device role changed to %s.",
                otThreadDeviceRoleToString (deviceRole));
            debugDeviceRole = deviceRole;
        }
    }

    // Indicate whether the device is currently joined to the network.
    if ((deviceRole == OT_DEVICE_ROLE_LEADER) ||
        (deviceRole == OT_DEVICE_ROLE_ROUTER) ||
        (deviceRole == OT_DEVICE_ROLE_CHILD)) {
        *deviceJoined = true;
    } else {
        *deviceJoined = false;
    }
    return true;
}

/*
 * Implement the OpenThread network joining callback.
 */
static void gmosOpenThreadJoinCallbackHandler (
    otError otStatus, void* context)
{
    gmosOpenThreadStack_t* openThreadStack =
        (gmosOpenThreadStack_t*) context;

    // Callbacks are only processed in the 'joining active' state.
    if (openThreadStack->netControlState != GMOS_OPENTHREAD_JOIN_ACTIVE) {
        return;
    }
    GMOS_LOG_FMT (LOG_DEBUG,
        "OpenThread : Completed network joining with status %d.", otStatus);

    // Start the thread stack after successful joining. Otherwise go
    // back to the idle state.
    if (otStatus == OT_ERROR_NONE) {
        openThreadStack->netControlState = GMOS_OPENTHREAD_JOIN_ENABLE_THREAD;
    } else {
        openThreadStack->netControlState = GMOS_OPENTHREAD_JOIN_IDLE;
    }
    gmosSchedulerTaskResume (&(openThreadStack->netControlTask));
}

/*
 * Implement the OpenThread network joining task.
 */
static inline gmosTaskStatus_t gmosOpenThreadJoinTaskFn (
    gmosOpenThreadStack_t* openThreadStack)
{
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    uint8_t nextState = openThreadStack->netControlState;
    bool deviceJoined;

    // Run the network joiner state machine.
    switch (openThreadStack->netControlState) {

        // Insert a delay to allow the OpenThread stack to start up.
        case GMOS_OPENTHREAD_JOIN_STATE_INIT :
            nextState = GMOS_OPENTHREAD_JOIN_ENABLE_IPV6;
            taskStatus = gmosOpenThreadJoinDelay ();
            break;

        // Attempt to start up the IPv6 interface.
        case GMOS_OPENTHREAD_JOIN_ENABLE_IPV6 :
            if (!gmosOpenThreadJoinEnableIpv6 (
                openThreadStack, &deviceJoined)) {
                nextState = GMOS_OPENTHREAD_JOIN_FAILED;
            } else if (deviceJoined) {
                nextState = GMOS_OPENTHREAD_JOIN_ENABLE_THREAD;
            } else {
                nextState = GMOS_OPENTHREAD_JOIN_IDLE;
            }
            break;


        // Enter the joining idle state, which waits for the application
        // to provide valid network joining credentials.
        case GMOS_OPENTHREAD_JOIN_IDLE :
            GMOS_LOG (LOG_DEBUG,
                "OpenThread : Ready to start network joining process.");
            taskStatus = GMOS_TASK_SUSPEND;
            break;

        // Attempt to connect to the thread network.
        case GMOS_OPENTHREAD_JOIN_ENABLE_THREAD :
            if (!gmosOpenThreadJoinEnableThread (openThreadStack)) {
                nextState = GMOS_OPENTHREAD_JOIN_FAILED;
            } else {
                nextState = GMOS_OPENTHREAD_JOIN_CONNECTING;
            }
            break;

        // Wait for the device to join the thread network.
        case GMOS_OPENTHREAD_JOIN_CONNECTING :
            if (!gmosOpenThreadJoinConnecting (
                openThreadStack, &deviceJoined)) {
                nextState = GMOS_OPENTHREAD_JOIN_FAILED;
            } else if (deviceJoined) {
                nextState = GMOS_OPENTHREAD_JOIN_CONFIGURE_DNS;
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (500));
            } else {
                taskStatus = GMOS_TASK_RUN_BACKGROUND;
            }
            break;

        // Find the primary backbone router to use as the DNS server.
        case GMOS_OPENTHREAD_JOIN_CONFIGURE_DNS :
            if (gmosOpenThreadJoinConfigureDns (openThreadStack)) {
                nextState = GMOS_OPENTHREAD_JOIN_MONITOR_NETWORK;
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (2500));
            }
            break;

        // Monitor the network state as a background task.
        case GMOS_OPENTHREAD_JOIN_MONITOR_NETWORK :
            if (!gmosOpenThreadJoinMonitorNetwork (
                openThreadStack, &deviceJoined)) {
                nextState = GMOS_OPENTHREAD_JOIN_FAILED;
            } else if (!deviceJoined) {
                nextState = GMOS_OPENTHREAD_JOIN_DISCONNECTED;
            } else {
                taskStatus = GMOS_TASK_RUN_BACKGROUND;
            }
            break;

        // Handle the case where the device has been disconnected from
        // the network.
        case GMOS_OPENTHREAD_JOIN_DISCONNECTED :
            // TODO: This.
            nextState = GMOS_OPENTHREAD_JOIN_FAILED;
            break;

        // Handle failure conditions.
        default :
            // TODO: Failure mode.
            taskStatus = GMOS_TASK_SUSPEND;
            break;
    }
    openThreadStack->netControlState = nextState;
    return taskStatus;
}

// Define the OpenThread network joining task.
GMOS_TASK_DEFINITION (gmosOpenThreadJoinTask,
    gmosOpenThreadJoinTaskFn, gmosOpenThreadStack_t);

/*
 * Initialises the OpenThread network control task on startup, with the
 * OpenThread node acting as a network joiner.
 */
bool gmosOpenThreadNetInit (gmosOpenThreadStack_t* openThreadStack)
{
    // Reset the OpenThread network joining state machine.
    openThreadStack->netControlState = GMOS_OPENTHREAD_JOIN_STATE_INIT;

    // Run the OpenThread network joining task.
    gmosOpenThreadJoinTask_start (&(openThreadStack->netControlTask),
        openThreadStack, "OpenThread Joining");
    return true;
}

/*
 * Determines the current status of the OpenThread joining process. The
 * OpenThread network is ready for use once this returns a successful
 * status value.
 */
gmosOpenThreadStatus_t gmosOpenThreadNetStatus (
    gmosOpenThreadStack_t* openThreadStack)
{
    gmosOpenThreadStatus_t status;

    // Map the current joiner states to the appropriate status values.
    switch (openThreadStack->netControlState) {
        case GMOS_OPENTHREAD_JOIN_MONITOR_NETWORK :
            status = GMOS_OPENTHREAD_STATUS_SUCCESS;
            break;
        case GMOS_OPENTHREAD_JOIN_FAILED :
            status = GMOS_OPENTHREAD_STATUS_FAILED;
            break;
        default :
            status = GMOS_OPENTHREAD_STATUS_NOT_READY;
            break;
    }
    return status;
}

/*
 * Initiates the OpenThread network joining process using the standard
 * commissioning tool authentication process. The supplied password is
 * used as the shared secret for the PAKE authentication process.
 */
gmosOpenThreadStatus_t gmosOpenThreadJoinStartJoiner (
    gmosOpenThreadStack_t* openThreadStack, const char* password)
{
    otInstance* otStack = openThreadStack->otInstance;
    otError otStatus;

    // Specify the fixed joining parameters.
    const char* provisioningUrl = GMOS_CONFIG_OPENTHREAD_PROVISIONING_URL;
    const char* vendorName = GMOS_CONFIG_OPENTHREAD_VENDOR_NAME;
    const char* vendorModel = GMOS_CONFIG_OPENTHREAD_VENDOR_MODEL;
    const char* vendorSwVersion = GMOS_CONFIG_OPENTHREAD_VENDOR_SW_VERSION;
    const char* vendorData = GMOS_CONFIG_OPENTHREAD_VENDOR_DATA;

    // Check that the network joiner is in a valid state to start the
    // joining process.
    switch (openThreadStack->netControlState) {
        case GMOS_OPENTHREAD_JOIN_IDLE :
            break;
        case GMOS_OPENTHREAD_JOIN_STATE_INIT :
        case GMOS_OPENTHREAD_JOIN_ENABLE_IPV6 :
            return GMOS_OPENTHREAD_STATUS_NOT_READY;
        case GMOS_OPENTHREAD_JOIN_FAILED :
            return GMOS_OPENTHREAD_STATUS_FAILED;
        default :
            return GMOS_OPENTHREAD_STATUS_INVALID_STATE;
    }

    // Attempt to start the network joining process with the specified
    // parameters.
    otStatus = otJoinerStart (otStack, password, provisioningUrl,
        vendorName, vendorModel, vendorSwVersion, vendorData,
        gmosOpenThreadJoinCallbackHandler, openThreadStack);
    if (otStatus == OT_ERROR_NONE) {
        GMOS_LOG (LOG_DEBUG,
            "OpenThread : Starting network joining process.");
        openThreadStack->netControlState = GMOS_OPENTHREAD_JOIN_ACTIVE;
    }

    // TODO: Use explicit mapping of status values.
    return otStatus;
}

/*
 * Indicates whether an OpenThread network is already commissioned for
 * the joining device.
 */
bool gmosOpenThreadJoinIsCommissioned (
    gmosOpenThreadStack_t* openThreadStack)
{
    otInstance* otStack = openThreadStack->otInstance;
    return otDatasetIsCommissioned (otStack);
}
