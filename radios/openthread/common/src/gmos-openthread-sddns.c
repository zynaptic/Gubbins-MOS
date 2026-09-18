/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2026 Zynaptic Limited
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
 * This file implements the common API for processing service discovery
 * DNS requests. It is not an independent service, but is designed for
 * direct integration into other service client modules.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-openthread.h"
#include "gmos-openthread-sddns.h"
#include "openthread/dns_client.h"

// Specify the default polling loop interval for the SD-DNS client.
#define GMOS_OPENTHREAD_SDDNS_CLIENT_TASK_POLL \
    GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (100))

// Specify the initial SD-DNS request backoff delay as an integer number
// of seconds.
#define GMOS_OPENTHREAD_SDDNS_BACKOFF_INIT 8

// Specify the maximum SD-DNS backoff delay. This must be an integer
// number of seconds less than 255.
#define GMOS_OPENTHREAD_SDDNS_BACKOFF_MAX 150

// Specify the exponential SD-DNS backoff delay multiplier. The actual
// value used is N/256.
#define GMOS_OPENTHREAD_SDDNS_BACKOFF_MULT 352

/*
 * Specify the state space for the OpenThread SD-DNS discovery client
 * state machine.
 */
typedef enum {
    GMOS_OPENTHREAD_SDDNS_CLIENT_STATE_IDLE,
    GMOS_OPENTHREAD_SDDNS_CLIENT_STATE_CALLBACK,
    GMOS_OPENTHREAD_SDDNS_CLIENT_STATE_RETRY
} gmosOpenThreadSdDnsClientState_t;

/*
 * Implement callback handler for SD-DNS browse requests.
 */
static void gmosOpenThreadSdDnsClientCallback (otError otStatus,
    const otDnsBrowseResponse *sdDnsResponse, void *callbackData)
{
    gmosOpenThreadSdDnsClient_t* sdDnsClient =
        (gmosOpenThreadSdDnsClient_t*) callbackData;
    char labelBuffer [sizeof (sdDnsClient->sdDnsLabel)];
    otDnsServiceInfo serviceInfo;
    uint32_t i;

    // Drop responses received in an invalid state.
    if (sdDnsClient->sdDnsClientState !=
        GMOS_OPENTHREAD_SDDNS_CLIENT_STATE_CALLBACK) {
        return;
    }

    // On startup use the first entry in the response list. On
    // subsequent requests the service label must match.
    for (i = 0; otStatus == OT_ERROR_NONE; i++) {
        otStatus = otDnsBrowseResponseGetServiceInstance (
            sdDnsResponse, i, labelBuffer, sizeof (labelBuffer));
        if (sdDnsClient->sdDnsLabel [0] == '\0') {
            break;
        }
        if (strncmp (labelBuffer, sdDnsClient->sdDnsLabel,
            sizeof (labelBuffer)) == 0) {
            break;
        }
    }

    // Get the service information for the selected entry. The host name
    // and txt data are not required, so the buffers are set to NULL.
    if (otStatus == OT_ERROR_NONE) {
        serviceInfo.mHostNameBuffer = NULL;
        serviceInfo.mTxtData = NULL;
        otStatus = otDnsBrowseResponseGetServiceInfo (
            sdDnsResponse, labelBuffer, &serviceInfo);
    }

    // Extract the IP address and port number for the resource
    // directory.
    if (otStatus == OT_ERROR_NONE) {
        uint8_t* addrBytes = serviceInfo.mHostAddress.mFields.m8;
        for (i = 0; i < 16; i++) {
            sdDnsClient->serviceAddr [i] = addrBytes [i];
        }
        sdDnsClient->servicePort = serviceInfo.mPort;

        // Take a local copy of the service label if required.
        if (sdDnsClient->sdDnsLabel [0] == '\0') {
            memcpy (sdDnsClient->sdDnsLabel,
                labelBuffer, sizeof (labelBuffer));
        }

        // Force DNS refresh at 80% of the service information TTL.
        sdDnsClient->sdDnsTimeout = gmosPalGetTimer () +
            GMOS_MS_TO_TICKS (serviceInfo.mTtl * 800);

        // Reset the retry backoff state after a successful request.
        sdDnsClient->sdDnsClientState =
            GMOS_OPENTHREAD_SDDNS_CLIENT_STATE_IDLE;
        sdDnsClient->sdDnsBackoffDelay =
            GMOS_OPENTHREAD_SDDNS_BACKOFF_INIT;

        // Log new DNS information if required.
        GMOS_LOG_FMT (LOG_DEBUG,
            "OpenThread : SD-DNS browse found service '%s'.",
                sdDnsClient->sdDnsLabel);
        GMOS_LOG_FMT (LOG_VERBOSE,
            "OpenThread : SD-DNS address [%02x%02x:%02x%02x:%02x%02x:"
            "%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x]:%d",
            addrBytes [0], addrBytes [1], addrBytes [2], addrBytes [3],
            addrBytes [4], addrBytes [5], addrBytes [6], addrBytes [7],
            addrBytes [8], addrBytes [9], addrBytes [10], addrBytes [11],
            addrBytes [12], addrBytes [13], addrBytes [14], addrBytes [15],
            serviceInfo.mPort);
    }

    // Attempt a retry if the request was not successful.
    else {
        GMOS_LOG_FMT (LOG_DEBUG,
            "OpenThread : SD-DNS browse callback failure status %d.",
            otStatus);
        sdDnsClient->sdDnsClientState =
            GMOS_OPENTHREAD_SDDNS_CLIENT_STATE_RETRY;
    }

    // Timestamp the transaction completion.
    sdDnsClient->sdDnsTimestamp = gmosPalGetTimer ();
}

/*
 * Initiate an SD-DNS browse request to search for the specified service.
 */
static inline bool gmosOpenThreadSdSDnsClientBrowse (
    gmosOpenThreadStack_t* openThreadStack,
    gmosOpenThreadSdDnsClient_t* sdDnsClient)
{
    otInstance* otStack = openThreadStack->otInstance;
    otError otStatus;

    // Issue the SD-DNS service browsing request.
    otStatus = otDnsClientBrowse (otStack, sdDnsClient->serviceName,
        gmosOpenThreadSdDnsClientCallback, sdDnsClient, NULL);

    // Attempt a retry if the request was not successful.
    if (otStatus != OT_ERROR_NONE) {
        GMOS_LOG_FMT (LOG_DEBUG,
            "OpenThread : SD-DNS browse request failure status %d.",
            otStatus);
    }
    return (otStatus == OT_ERROR_NONE) ? true : false;
}

/*
 * Calculate the SD-DNS request backoff delay.
 */
static inline uint32_t gmosOpenThreadSdDnsClientBackoff (
    gmosOpenThreadSdDnsClient_t* sdDnsClient)
{
    uint32_t backoffDelay;
    uint32_t nextDelay;
    int32_t remainingTime;

    // Calculate the current backoff delay as the number of timer ticks.
    backoffDelay = GMOS_MS_TO_TICKS (
        ((uint32_t) sdDnsClient->sdDnsBackoffDelay) * 1000);

    // Check for a running timeout.
    remainingTime = (int32_t) (sdDnsClient->sdDnsTimestamp +
        backoffDelay - gmosPalGetTimer ());
    if (remainingTime > 0) {
        return (uint32_t) remainingTime;
    }

    // Update the backoff delay for the next retry.
    nextDelay = ((((uint32_t) sdDnsClient->sdDnsBackoffDelay) *
        GMOS_OPENTHREAD_SDDNS_BACKOFF_MULT) / 256);
    if (nextDelay <= GMOS_OPENTHREAD_SDDNS_BACKOFF_MAX) {
        sdDnsClient->sdDnsBackoffDelay = (uint8_t) nextDelay;
    }

    // Randomise the backoff delay when it reaches the maximum value.
    else {
        uint8_t randomDelay = 0;
        while ((randomDelay < GMOS_OPENTHREAD_SDDNS_BACKOFF_INIT) ||
            (randomDelay > GMOS_OPENTHREAD_SDDNS_BACKOFF_MAX)) {
            gmosPalGetRandomBytes (&randomDelay, 1);
        }
        sdDnsClient->sdDnsBackoffDelay = randomDelay;
    }
    return 0;
}

/*
 * Initialises the SD-DNS client on startup.
 */
void gmosOpenThreadSdDnsClientInit (
    gmosOpenThreadSdDnsClient_t* sdDnsClient, const char* serviceName)
{
    // Set the service name reference.
    sdDnsClient->serviceName = serviceName;

    // Reset the SD-DNS client state.
    gmosOpenThreadSdDnsClientReset (sdDnsClient);
}

/*
 * Resets the SD-DNS client state.
 */
void gmosOpenThreadSdDnsClientReset (
    gmosOpenThreadSdDnsClient_t* sdDnsClient)
{
    uint32_t currentTimestamp = gmosPalGetTimer ();

    // Reset the SD-DNS state variables.
    sdDnsClient->sdDnsClientState = GMOS_OPENTHREAD_SDDNS_CLIENT_STATE_IDLE;
    sdDnsClient->sdDnsBackoffDelay = GMOS_OPENTHREAD_SDDNS_BACKOFF_INIT;
    sdDnsClient->sdDnsTimeout = currentTimestamp;
    sdDnsClient->sdDnsTimestamp = currentTimestamp;
    sdDnsClient->sdDnsLabel [0] = '\0';
}

/*
 * Indicates whether the current SD-DNS client data is valid.
 */
bool gmosOpenThreadSdDnsClientDataValid (
    gmosOpenThreadSdDnsClient_t* sdDnsClient)
{
    int32_t delay;
    bool dataValid = false;

    // DNS data is valid in the idle state if it has not timed out.
    if (sdDnsClient->sdDnsClientState ==
        GMOS_OPENTHREAD_SDDNS_CLIENT_STATE_IDLE) {
        delay = (int32_t) (sdDnsClient->sdDnsTimeout - gmosPalGetTimer ());
        dataValid = (delay > 0) ? true : false;
    }
    return dataValid;
}

/*
 * Periodically polls the SD-DNS client instance while an SD-DNS lookup
 * is in progress.
 */
gmosTaskStatus_t gmosOpenThreadSdDnsClientPoll (
    gmosOpenThreadStack_t* openThreadStack,
    gmosOpenThreadSdDnsClient_t* sdDnsClient)
{
    gmosTaskStatus_t taskStatus = GMOS_TASK_SUSPEND;
    uint8_t nextState = sdDnsClient->sdDnsClientState;
    uint32_t retryDelay;

    // Run the SD-DNS discovery client state machine.
    switch (sdDnsClient->sdDnsClientState) {

        // From the idle state initiate a DNS-SD browse request.
        case GMOS_OPENTHREAD_SDDNS_CLIENT_STATE_IDLE :
            if (gmosOpenThreadSdDnsClientDataValid (sdDnsClient)) {
                taskStatus = GMOS_TASK_RUN_IMMEDIATE;
            } else if (gmosOpenThreadSdSDnsClientBrowse (
                openThreadStack, sdDnsClient)) {
                nextState = GMOS_OPENTHREAD_SDDNS_CLIENT_STATE_CALLBACK;
                taskStatus = GMOS_OPENTHREAD_SDDNS_CLIENT_TASK_POLL;
            } else {
                taskStatus = GMOS_OPENTHREAD_SDDNS_CLIENT_TASK_POLL;
            }
            break;

        // Poll for SD-DNS results from the callback state.
        case GMOS_OPENTHREAD_SDDNS_CLIENT_STATE_CALLBACK :
            taskStatus = GMOS_OPENTHREAD_SDDNS_CLIENT_TASK_POLL;
            break;

        // Poll for retry attempt.
        case GMOS_OPENTHREAD_SDDNS_CLIENT_STATE_RETRY :
            retryDelay = gmosOpenThreadSdDnsClientBackoff (sdDnsClient);
            if (retryDelay > 0) {
                taskStatus = GMOS_TASK_RUN_LATER (
                    GMOS_MS_TO_TICKS (retryDelay));
            } else {
                nextState = GMOS_OPENTHREAD_SDDNS_CLIENT_STATE_IDLE;
                taskStatus = GMOS_OPENTHREAD_SDDNS_CLIENT_TASK_POLL;
            }
            break;
    }
    sdDnsClient->sdDnsClientState = nextState;
    return taskStatus;
}
