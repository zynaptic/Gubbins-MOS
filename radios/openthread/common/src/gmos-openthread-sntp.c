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
 * This file implements the local SNTP client which is responsible for
 * maintaining the local UNIX epoch wallclock time.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-scheduler.h"
#include "gmos-openthread.h"
#include "gmos-openthread-sntp.h"
#include "openthread/dns_client.h"
#include "openthread/sntp.h"

// Select the SD-DNS service type to use.
#define GMOS_OPENTHREAD_SNTP_SERVICE_TYPE \
    "_ntp._udp.default.service.arpa"

// Specify the NTP synchronisation interval to use.
#define GMOS_OPENTHREAD_SNTP_SYNC_INTERVAL 300

// Specify the NTP synchronisation retry interval to use.
#define GMOS_OPENTHREAD_SNTP_RETRY_INTERVAL 30

/*
 * Specify the state space for the OpenThread SNTP client state machine.
 */
typedef enum {
    GMOS_OPENTHREAD_SNTP_CLIENT_STATE_INIT,
    GMOS_OPENTHREAD_SNTP_CLIENT_STATE_IDLE,
    GMOS_OPENTHREAD_SNTP_CLIENT_STATE_SDDNS_BROWSE,
    GMOS_OPENTHREAD_SNTP_CLIENT_STATE_SDDNS_CALLBACK,
    GMOS_OPENTHREAD_SNTP_CLIENT_STATE_SDDNS_RETRY,
    GMOS_OPENTHREAD_SNTP_CLIENT_STATE_QUERY_SEND,
    GMOS_OPENTHREAD_SNTP_CLIENT_STATE_QUERY_CALLBACK,
    GMOS_OPENTHREAD_SNTP_CLIENT_STATE_FAILED
} gmosOpenThreadSntpClientState_t;

/*
 * Perform a local time synchronisation step. This currently uses a
 * very crude 'algorithm' that timestamps each NTP query result with
 * the local system timer.
 * TODO: Implement a proper synchronisation algorithm to give more
 * accurate timestamps.
 */
static inline void gmosOpenThreadSntpClientTimeSync (
    gmosOpenThreadSntpClient_t* sntpClient, uint64_t ntpTime)
{
    GMOS_LOG_FMT (LOG_VERBOSE,
        "OpenThread :  SNTP Sync to epoch %d, time %d",
        (uint32_t) (ntpTime >> 32), (uint32_t) ntpTime);
    sntpClient->lastNtpTime = (uint32_t) ntpTime;
    sntpClient->lastNtpTimestamp = gmosPalGetTimer ();
}

/*
 * From the initialisation state, wait for the OpenThread network to
 * come online.
 */
static inline bool gmosOpenThreadSntpClientInitWait (
    gmosOpenThreadSntpClient_t* sntpClient)
{
    gmosOpenThreadStatus_t netStatus =
        gmosOpenThreadNetStatus (sntpClient->openThreadStack);
    return (netStatus == GMOS_OPENTHREAD_STATUS_SUCCESS) ? true : false;
}

/*
 * Implement callback handler for SD-DNS browse requests.
 */
static void gmosOpenThreadSntpClientSdDnsCallback (otError otStatus,
    const otDnsBrowseResponse *sdDnsResponse, void *callbackData)
{
    gmosOpenThreadSntpClient_t* sntpClient =
        (gmosOpenThreadSntpClient_t*) callbackData;
    otDnsServiceInfo serviceInfo;
    char labelBuffer [64];
    uint32_t i;

    // Drop responses received in an invalid state.
    if (sntpClient->sntpClientState !=
        GMOS_OPENTHREAD_SNTP_CLIENT_STATE_SDDNS_CALLBACK) {
        return;
    }

    // NTP requests are stateless, so just use the first entry in the
    // response list.
    otStatus = otDnsBrowseResponseGetServiceInstance (
        sdDnsResponse, 0, labelBuffer, sizeof (labelBuffer));

    // Get the service information for the selected entry. The host name
    // and txt data are not required, so the buffers are set to NULL.
    if (otStatus == OT_ERROR_NONE) {
        serviceInfo.mHostNameBuffer = NULL;
        serviceInfo.mTxtData = NULL;
        otStatus = otDnsBrowseResponseGetServiceInfo (
            sdDnsResponse, labelBuffer, &serviceInfo);
    }

    // Extract the IP address and port number for the NTP server.
    if (otStatus == OT_ERROR_NONE) {
        uint8_t* addrBytes = serviceInfo.mHostAddress.mFields.m8;
        for (i = 0; i < 16; i++) {
            sntpClient->ntpAddr [i] = addrBytes [i];
        }
        sntpClient->ntpPort = serviceInfo.mPort;

        // Force DNS refresh at 80% of the service information TTL.
        sntpClient->sdDnsTimeout = gmosPalGetTimer () +
            GMOS_MS_TO_TICKS (serviceInfo.mTtl * 800);
        sntpClient->sntpClientState =
            GMOS_OPENTHREAD_SNTP_CLIENT_STATE_IDLE;

        // Log new DNS information if required.
        GMOS_LOG_FMT (LOG_VERBOSE,
            "OpenThread : SD-DNS NTP server address [%02x%02x:%02x%02x:%02x%02x:"
            "%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x]:%d",
            addrBytes [0], addrBytes [1], addrBytes [2], addrBytes [3],
            addrBytes [4], addrBytes [5], addrBytes [6], addrBytes [7],
            addrBytes [8], addrBytes [9], addrBytes [10], addrBytes [11],
            addrBytes [12], addrBytes [13], addrBytes [14], addrBytes [15],
            serviceInfo.mPort);
        GMOS_LOG_FMT (LOG_VERBOSE,
            "OpenThread : SD-DNS NTP server address TTL %ds.", serviceInfo.mTtl);
    }

    // Attempt a retry if the request was not successful.
    else {
        GMOS_LOG_FMT (LOG_DEBUG,
            "OpenThread : SD-DNS browse callback failure status %d.",
            otStatus);
        sntpClient->sntpClientState =
            GMOS_OPENTHREAD_SNTP_CLIENT_STATE_SDDNS_RETRY;
    }

    // Resume state machine task execution.
    gmosSchedulerTaskResume (&(sntpClient->sntpTask));
    return;
}

/*
 * Initiate an SD-DNS browse request to search for local NTP server.
 */
static inline bool gmosOpenThreadSntpClientSdDnsBrowse (
    gmosOpenThreadSntpClient_t* sntpClient)
{
    otInstance* otStack = sntpClient->openThreadStack->otInstance;
    otError otStatus;

    // Issue the SD-DNS service browsing request.
    otStatus = otDnsClientBrowse (
        otStack, GMOS_OPENTHREAD_SNTP_SERVICE_TYPE,
        gmosOpenThreadSntpClientSdDnsCallback, sntpClient, NULL);

    // Attempt a retry if the request was not successful.
    if (otStatus != OT_ERROR_NONE) {
        GMOS_LOG_FMT (LOG_DEBUG,
            "OpenThread : SD-DNS browse request failure status %d",
            otStatus);
    }
    return (otStatus == OT_ERROR_NONE) ? true : false;
}

/*
 * Implement the SNTP query callback.
 */
static void gmosOpenThreadSntpClientQueryCallback (
    void *callbackData, uint64_t ntpTime, otError otStatus)
{
    gmosOpenThreadSntpClient_t* sntpClient =
        (gmosOpenThreadSntpClient_t*) callbackData;

    // Drop responses received in an invalid state.
    if (sntpClient->sntpClientState !=
        GMOS_OPENTHREAD_SNTP_CLIENT_STATE_QUERY_CALLBACK) {
        return;
    }

    // Synchronise the local timer counter on success and schedule the
    // next synchronisation request.
    if (otStatus == OT_ERROR_NONE) {
        gmosOpenThreadSntpClientTimeSync (sntpClient, ntpTime);
        sntpClient->sntpSyncTimeout = gmosPalGetTimer () +
            GMOS_MS_TO_TICKS (GMOS_OPENTHREAD_SNTP_SYNC_INTERVAL * 1000);
    }

    // Attempt a retry if the request was not successful.
    else {
        GMOS_LOG_FMT (LOG_DEBUG,
            "OpenThread : SNTP query callback failure status %d.",
            otStatus);
        sntpClient->sntpSyncTimeout = gmosPalGetTimer () +
            GMOS_MS_TO_TICKS (GMOS_OPENTHREAD_SNTP_RETRY_INTERVAL * 1000);
    }

    // Resume state machine task execution.
    sntpClient->sntpClientState = GMOS_OPENTHREAD_SNTP_CLIENT_STATE_IDLE;
    gmosSchedulerTaskResume (&(sntpClient->sntpTask));
    return;
}

/*
 * Initiate an SNTP query for the local NTP server.
 */
static inline bool gmosOpenThreadSntpClientQuery (
    gmosOpenThreadSntpClient_t* sntpClient)
{
    otInstance* otStack = sntpClient->openThreadStack->otInstance;
    otMessageInfo ntpAddrInfo = { 0 };
    otSntpQuery ntpQuery = { &ntpAddrInfo };
    otError otStatus;
    uint_fast8_t i;

    // Set the NTP server peer address.
    for (i = 0; i < 16; i++) {
        ntpAddrInfo.mPeerAddr.mFields.m8 [i] = sntpClient->ntpAddr [i];
    }
    ntpAddrInfo.mPeerPort = sntpClient->ntpPort;

    // Issue the SNTP query to the NTP server address.
    otStatus = otSntpClientQuery (otStack, &ntpQuery,
        gmosOpenThreadSntpClientQueryCallback, sntpClient);

    // Attempt a retry if the request was not successful.
    if (otStatus != OT_ERROR_NONE) {
        GMOS_LOG_FMT (LOG_DEBUG,
            "OpenThread : SNTP query request failure status %d",
            otStatus);
    }
    return (otStatus == OT_ERROR_NONE) ? true : false;
}

/*
 * Select the next processing step from the idle state.
 */
static inline gmosTaskStatus_t gmosOpenThreadSntpClientActionSelect (
    gmosOpenThreadSntpClient_t* sntpClient, uint8_t* nextState)
{
    uint32_t currentTime = gmosPalGetTimer ();
    int32_t delay;

    // No processing is required if the SNTP synchronisation timeout has
    // not already expired.
    delay = (int32_t) (sntpClient->sntpSyncTimeout - currentTime);
    if (delay > 0) {
        return GMOS_TASK_RUN_LATER (delay);
    }

    // Issue a service discovery DNS request if the local cached entry
    // is stale.
    delay = (int32_t) (sntpClient->sdDnsTimeout - currentTime);
    if (delay <= 0) {
        *nextState = GMOS_OPENTHREAD_SNTP_CLIENT_STATE_SDDNS_BROWSE;
        return GMOS_TASK_RUN_IMMEDIATE;
    }

    // Issue an SNTP synchronisation request.
    *nextState = GMOS_OPENTHREAD_SNTP_CLIENT_STATE_QUERY_SEND;
    return GMOS_TASK_RUN_IMMEDIATE;
}

/*
 * Implement the SNTP client task.
 */
static inline gmosTaskStatus_t gmosOpenThreadSntpClientTaskFn (
    gmosOpenThreadSntpClient_t* sntpClient)
{
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    uint8_t nextState = sntpClient->sntpClientState;

    // Run the resource directory client state machine.
    switch (sntpClient->sntpClientState) {

        // Wait for network initialisation.
        case GMOS_OPENTHREAD_SNTP_CLIENT_STATE_INIT :
            if (gmosOpenThreadSntpClientInitWait (sntpClient)) {
                nextState = GMOS_OPENTHREAD_SNTP_CLIENT_STATE_IDLE;
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (1000));
            }
            break;

        // From the idle state select the next processing step by
        // checking the appropriate timeouts.
        case GMOS_OPENTHREAD_SNTP_CLIENT_STATE_IDLE :
            taskStatus = gmosOpenThreadSntpClientActionSelect (
                sntpClient, &nextState);
            break;

        // Initiate an SD-DNS browse request to obtain the location of
        // the local NTP server.
        case GMOS_OPENTHREAD_SNTP_CLIENT_STATE_SDDNS_BROWSE :
            if (gmosOpenThreadSntpClientSdDnsBrowse (sntpClient)) {
                nextState = GMOS_OPENTHREAD_SNTP_CLIENT_STATE_SDDNS_CALLBACK;
                taskStatus = GMOS_TASK_SUSPEND;
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (1000));
            }
            break;

        // Suspend task processing while the SD-DNS callbacks are being
        // processed.
        case GMOS_OPENTHREAD_SNTP_CLIENT_STATE_SDDNS_CALLBACK :
            taskStatus = GMOS_TASK_SUSPEND;
            break;

        // Retry the SD-DNS request after a short delay.
        case GMOS_OPENTHREAD_SNTP_CLIENT_STATE_SDDNS_RETRY :
            nextState = GMOS_OPENTHREAD_SNTP_CLIENT_STATE_SDDNS_BROWSE;
            taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (8000));
            break;

        // Send the SNTP synchronisation request to the NTP server.
        case GMOS_OPENTHREAD_SNTP_CLIENT_STATE_QUERY_SEND :
            if (gmosOpenThreadSntpClientQuery (sntpClient)) {
                nextState = GMOS_OPENTHREAD_SNTP_CLIENT_STATE_QUERY_CALLBACK;
                taskStatus = GMOS_TASK_SUSPEND;
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (1000));
            }
            break;

        // Suspend task processing while the query callbacks are being
        // processed.
        case GMOS_OPENTHREAD_SNTP_CLIENT_STATE_QUERY_CALLBACK :
            taskStatus = GMOS_TASK_SUSPEND;
            break;

        // Handle failure conditions.
        default :
            // TODO: Failure mode.
            taskStatus = GMOS_TASK_SUSPEND;
            break;
    }
    sntpClient->sntpClientState = nextState;
    return taskStatus;
}

// Define the SNTP client task.
GMOS_TASK_DEFINITION (gmosOpenThreadSntpClientTask,
    gmosOpenThreadSntpClientTaskFn, gmosOpenThreadSntpClient_t);

/*
 * Initialises the SNTP client on startup.
 */
bool gmosOpenThreadSntpClientInit (
    gmosOpenThreadSntpClient_t* sntpClient,
    gmosOpenThreadStack_t* openThreadStack)
{
    uint32_t initTimeout = gmosPalGetTimer ();

    // Reset the SNTP synchronisation state.
    sntpClient->lastNtpTime = 0;
    sntpClient->lastNtpTimestamp = 0;

    // Reset the SNTP client state machine.
    sntpClient->openThreadStack = openThreadStack;
    sntpClient->sntpClientState =
        GMOS_OPENTHREAD_SNTP_CLIENT_STATE_INIT;

    // Reset the SD-DNS state.
    sntpClient->sdDnsTimeout = initTimeout;

    // Run the SNTP client task.
    gmosOpenThreadSntpClientTask_start (&(sntpClient->sntpTask),
        sntpClient, "OpenThread SNTP Client");
    return true;
}

/*
 * Accesses the current SNTP network time, expressed as the integer
 * number of milliseconds since the UNIX epoch.
 */
uint64_t gmosOpenThreadSntpClientGetTime (
    gmosOpenThreadSntpClient_t* sntpClient)
{
    uint32_t elapsedTime;
    uint64_t currentTime;

    // TODO: Reset the time on loss of synchronisation.

    // No valid NTP time present.
    if (sntpClient->lastNtpTime == 0) {
        return 0;
    }

    // This currently uses a very crude 'algorithm' that uses the last
    // NTP time value as the base for an elapsed time offset.
    elapsedTime = gmosPalGetTimer () - sntpClient->lastNtpTimestamp;
    elapsedTime = GMOS_TICKS_TO_MS (elapsedTime);
    currentTime = (uint64_t) sntpClient->lastNtpTime;
    currentTime = 1000 * currentTime + elapsedTime;
    return currentTime;
}
