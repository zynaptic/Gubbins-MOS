/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2023-2026 Zynaptic Limited
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
#include "gmos-openthread-sddns.h"
#include "openthread/sntp.h"

// Select the SD-DNS service type to use.
#define GMOS_OPENTHREAD_SNTP_SERVICE_TYPE \
    "_ntp._udp.default.service.arpa"

// Specify the NTP synchronisation interval to use.
#define GMOS_OPENTHREAD_SNTP_SYNC_INTERVAL 300

// Specify the NTP synchronisation retry interval to use.
// TODO: This has been observed to get stuck in a failure loop with a
// 'service busy' error code. The retry interval has been increased as
// an interim measure, but retries really need to be reimplemented with
// exponential backoff.
#define GMOS_OPENTHREAD_SNTP_RETRY_INTERVAL 60

/*
 * Specify the state space for the OpenThread SNTP client state machine.
 */
typedef enum {
    GMOS_OPENTHREAD_SNTP_CLIENT_STATE_INIT,
    GMOS_OPENTHREAD_SNTP_CLIENT_STATE_IDLE,
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
        "OpenThread : SNTP Sync to epoch %d, time %d",
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
    uint8_t* ntpAddr = sntpClient->sdDnsClient.serviceAddr;
    uint16_t ntpPort = sntpClient->sdDnsClient.servicePort;
    otMessageInfo ntpAddrInfo = { 0 };
    otSntpQuery ntpQuery = { &ntpAddrInfo };
    otError otStatus;
    uint_fast8_t i;

    // Set the NTP server peer address.
    for (i = 0; i < 16; i++) {
        ntpAddrInfo.mPeerAddr.mFields.m8 [i] = ntpAddr [i];
    }
    ntpAddrInfo.mPeerPort = ntpPort;

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
    if (!gmosOpenThreadSdDnsClientDataValid (&(sntpClient->sdDnsClient))) {
        return gmosOpenThreadSdDnsClientPoll (
            sntpClient->openThreadStack, &(sntpClient->sdDnsClient));
    }

    // Issue an SNTP synchronisation request.
    else {
        *nextState = GMOS_OPENTHREAD_SNTP_CLIENT_STATE_QUERY_SEND;
    }
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

        // Send the SNTP synchronisation request to the NTP server.
        case GMOS_OPENTHREAD_SNTP_CLIENT_STATE_QUERY_SEND :
            if (gmosOpenThreadSntpClientQuery (sntpClient)) {
                nextState = GMOS_OPENTHREAD_SNTP_CLIENT_STATE_QUERY_CALLBACK;
                taskStatus = GMOS_TASK_SUSPEND;
            } else {
                nextState = GMOS_OPENTHREAD_SNTP_CLIENT_STATE_IDLE;
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
    // Reset the SNTP synchronisation state.
    sntpClient->lastNtpTime = 0;
    sntpClient->lastNtpTimestamp = 0;

    // Reset the SNTP client state machine.
    sntpClient->openThreadStack = openThreadStack;
    sntpClient->sntpClientState =
        GMOS_OPENTHREAD_SNTP_CLIENT_STATE_INIT;

    // Reset the SD-DNS state.
    gmosOpenThreadSdDnsClientInit (&(sntpClient->sdDnsClient),
        GMOS_OPENTHREAD_SNTP_SERVICE_TYPE);

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
