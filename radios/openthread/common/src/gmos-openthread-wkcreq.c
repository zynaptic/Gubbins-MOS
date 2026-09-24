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
 * This file implements the common API for requesting CoRE link
 * information from the well-known CoRE resource path on a given CoAP
 * endpoint according to RFC 6690. It is not an independent service, but
 * is designed for direct integration into other service client modules.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-openthread.h"
#include "gmos-openthread-sddns.h"
#include "gmos-openthread-wkcreq.h"
#include "openthread/coap.h"

// Specify the default polling loop interval for the well-known CoRE
// request client.
#define GMOS_OPENTHREAD_WKCREQ_CLIENT_TASK_POLL \
    GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (100))

// Specify the period for which the results of a well-known CoRE request
// remain valid, expressed as an integer number of seconds.
#define GMOS_OPENTHREAD_WKCREQ_LIFETIME 60

// Specify the initial well-known CoRE request backoff delay as an
// integer number of seconds.
#define GMOS_OPENTHREAD_WKCREQ_BACKOFF_INIT 8

// Specify the maximum well-known CoRE backoff delay. This must be an
// integer number of seconds less than 255.
#define GMOS_OPENTHREAD_WKCREQ_BACKOFF_MAX 150

// Specify the exponential well-known CoRE backoff delay multiplier. The
// actual value used is N/256.
#define GMOS_OPENTHREAD_WKCREQ_BACKOFF_MULT 352

/*
 * Specify the state space for the OpenThread well-known CoRE request
 * client state machine.
 */
typedef enum {
    GMOS_OPENTHREAD_WKCREQ_CLIENT_STATE_IDLE,
    GMOS_OPENTHREAD_WKCREQ_CLIENT_STATE_CALLBACK,
    GMOS_OPENTHREAD_WKCREQ_CLIENT_STATE_RETRY
} gmosOpenThreadWkcReqClientState_t;

/*
 * Parse the well-known CoRE discovery message which is contained in a
 * null terminated string. Returns an error condition on parsing failure.
 * This currently assumes that the server performs attribute filtering
 * correctly and only returns the resource link of interest. It could be
 * made more robust by checking the contents of the 'ct' and 'rt'
 * attributes before accepting the resource link.
 */
static otError gmosOpenThreadWkcReqClientParse (
    gmosOpenThreadWkcReqClient_t* wkcReqClient, uint8_t* msgBuf)
{
    uint8_t* msgPtr;
    uint_fast8_t pathSize;

    // The first two characters must always be the start of an absolute
    // resource path on the server. The initial path separator is
    // discarded prior to local storage.
    msgPtr = msgBuf;
    if ((*(msgPtr++) != '<') || ((*msgPtr++) != '/')) {
        return OT_ERROR_PARSE;
    }

    // Find the end marker of the path component, which must be present.
    pathSize = 0;
    while (true) {
        uint8_t pathChar = *(msgPtr++);
        if (pathChar == '\0') {
            return OT_ERROR_PARSE;
        } else if (pathChar == '>') {
            break;
        } else {
            pathSize += 1;
        }
    }

    // Check that the resource URI path does not exceed the local
    // allocated storage.
    if (pathSize >= sizeof (wkcReqClient->uriPath)) {
        return OT_ERROR_NO_BUFS;
    }

    // Store the resource path component locally as a null terminated
    // string.
    memcpy (wkcReqClient->uriPath, &(msgBuf [2]), pathSize);
    wkcReqClient->uriPath [pathSize] = '\0';
    GMOS_LOG_FMT (LOG_DEBUG,
        "OpenThread : CoRE discovered resource path : '%s'",
        wkcReqClient->uriPath);
    return OT_ERROR_NONE;
}

/*
 * Callback handler for CoAP requests to the well-known CoRE resource.
 */
static void gmosOpenThreadWkcReqClientCallback (void* callbackData,
    otMessage* coapMessage, const otMessageInfo* coapMessageInfo,
    otError otStatus)
{
    gmosOpenThreadWkcReqClient_t* wkcReqClient =
        (gmosOpenThreadWkcReqClient_t*) callbackData;
    (void) coapMessageInfo;
    otCoapCode coapStatus;
    otCoapOptionIterator coapOptIter;
    const otCoapOption* coapOpt;
    uint64_t coapOptValue;
    uint8_t msgBuf [sizeof (wkcReqClient->uriPath) + 32];
    uint_fast16_t msgOffset;
    uint_fast16_t msgLen;
    uint_fast16_t msgSize;

    // Drop responses received in an invalid state.
    if (wkcReqClient->wkcReqClientState !=
        GMOS_OPENTHREAD_WKCREQ_CLIENT_STATE_CALLBACK) {
        return;
    }

    // Check the CoAP return code. This should be 2.05 (content).
    if (otStatus == OT_ERROR_NONE) {
        coapStatus = otCoapMessageGetCode (coapMessage);
        GMOS_LOG_FMT (LOG_DEBUG,
            "OpenThread : CoRE discovery CoAP status %d.%02d.",
            (coapStatus >> 5) & 0x07, coapStatus & 0x1F);
        if (coapStatus != OT_COAP_CODE_CONTENT) {
            otStatus = OT_ERROR_REJECTED;
        }
    }

    // Check for oversized blockwise transfers. These have a block 2
    // option with the 'more blocks' flag set and are treated as an out
    // of buffer memory error.
    if (otStatus == OT_ERROR_NONE) {
        otStatus = otCoapOptionIteratorInit (&coapOptIter, coapMessage);
    }
    if (otStatus == OT_ERROR_NONE) {
        coapOpt = otCoapOptionIteratorGetFirstOptionMatching (
            &coapOptIter, OT_COAP_OPTION_BLOCK2);
        if (coapOpt != NULL) {
            otStatus = otCoapOptionIteratorGetOptionUintValue (
                &coapOptIter, &coapOptValue);
            GMOS_LOG_FMT (LOG_DEBUG,
                "OpenThread : CoRE discovery block option 0x%08X",
                (uint32_t) coapOptValue);
            if ((coapOptValue & 0x08) != 0) {
                otStatus = OT_ERROR_NO_BUFS;
            }
        }
    }

    // Only process successful responses that can fit in the buffer.
    if (otStatus == OT_ERROR_NONE) {
        msgOffset = otMessageGetOffset (coapMessage);
        msgLen = otMessageGetLength (coapMessage) - msgOffset;
        if (msgLen >= sizeof (msgBuf)) {
            otStatus = OT_ERROR_NO_BUFS;
        }
    }

    // Read the data into the local buffer as a null terminated string.
    if (otStatus == OT_ERROR_NONE) {
        msgSize = otMessageRead (coapMessage, msgOffset, msgBuf, msgLen);
        if (msgSize == msgLen) {
            msgBuf [msgLen] = '\0';
        } else {
            otStatus = OT_ERROR_PARSE;
        }
    }

    // Parse the resource directory discovery response.
    if (otStatus == OT_ERROR_NONE) {
        otStatus = gmosOpenThreadWkcReqClientParse (
            wkcReqClient, msgBuf);
    }

    // Set the result lifetime and reset the retry backoff state after a
    // successful request.
    if (otStatus == OT_ERROR_NONE) {
        wkcReqClient->wkcReqClientState =
            GMOS_OPENTHREAD_WKCREQ_CLIENT_STATE_IDLE;
        wkcReqClient->wkcReqBackoffDelay =
            GMOS_OPENTHREAD_WKCREQ_BACKOFF_INIT;
        wkcReqClient->wkcReqTimeout = gmosPalGetTimer () +
            GMOS_MS_TO_TICKS (GMOS_OPENTHREAD_WKCREQ_LIFETIME * 1000);
    }

    // Attempt a retry if the request was not successful.
    else {
        GMOS_LOG_FMT (LOG_DEBUG,
            "OpenThread : CoRE discovery callback failure status %d.",
            otStatus);
        wkcReqClient->wkcReqClientState =
            GMOS_OPENTHREAD_WKCREQ_CLIENT_STATE_RETRY;
    }

    // Timestamp the transaction completion.
    wkcReqClient->wkcReqTimestamp = gmosPalGetTimer ();
}

/*
 * Initiate a CoAP request to discover a specific CoAP resource from the
 * well-known CoRE link table.
 */
static inline bool gmosOpenThreadWkcReqClientSend (
    gmosOpenThreadStack_t* openThreadStack,
    gmosOpenThreadWkcReqClient_t* wkcReqClient,
    gmosOpenThreadSdDnsClient_t* sdDnsClient)
{
    otInstance* otStack = openThreadStack->otInstance;
    uint8_t* resDirAddr = sdDnsClient->serviceAddr;
    uint16_t resDirPort = sdDnsClient->servicePort;
    otMessage* coapMessage;
    otMessageInfo coapMessageInfo = { 0 };
    otError otStatus;
    uint_fast8_t i;

    // Allocate memory for the new CoAP message.
    coapMessage = otCoapNewMessage (otStack, NULL);
    if (coapMessage == NULL) {
        otStatus = OT_ERROR_NO_BUFS;
        goto fail;
    }

    // Fill in the common CoAP header for CoRE requests.
    otCoapMessageInit (coapMessage,
        OT_COAP_TYPE_CONFIRMABLE, OT_COAP_CODE_GET);
    otCoapMessageGenerateToken (coapMessage,
        OT_COAP_DEFAULT_TOKEN_LENGTH);
    otStatus = otCoapMessageAppendUriPathOptions (coapMessage,
        ".well-known/core");
    if (otStatus != OT_ERROR_NONE) {
        goto fail;
    }

    // Add the query parameter to select the resource directory
    // registration path.
    otStatus = otCoapMessageAppendUriQueryOptions (coapMessage,
        wkcReqClient->queryString);
    if (otStatus != OT_ERROR_NONE) {
        goto fail;
    }

    // Add the acceptable data format parameter.
    otStatus = otCoapMessageAppendUintOption (coapMessage,
        OT_COAP_OPTION_ACCEPT, OT_COAP_OPTION_CONTENT_FORMAT_LINK_FORMAT);
    if (otStatus != OT_ERROR_NONE) {
        goto fail;
    }

    // Set the CoAP message destination and send the request. All
    // additional options are left as zero to select the defaults.
    for (i = 0; i < 16; i++) {
        coapMessageInfo.mPeerAddr.mFields.m8 [i] = resDirAddr [i];
    }
    coapMessageInfo.mPeerPort = resDirPort;
    otStatus = otCoapSendRequest (otStack, coapMessage,
        &coapMessageInfo, gmosOpenThreadWkcReqClientCallback,
        wkcReqClient);
    if (otStatus != OT_ERROR_NONE) {
        goto fail;
    }
    return true;

    // Release allocated memory on failure.
fail :
    GMOS_LOG_FMT (LOG_DEBUG,
        "OpenThread : CoRE discovery request failure status %d.",
        otStatus);
    if (coapMessage != NULL) {
        otMessageFree (coapMessage);
    }
    return false;
}

/*
 * Calculate the well-known CoRE request backoff delay.
 */
static inline uint32_t gmosOpenThreadWkcReqClientBackoff (
    gmosOpenThreadWkcReqClient_t* wkcReqClient)
{
    uint32_t backoffDelay;
    uint32_t nextDelay;
    int32_t remainingTime;

    // Calculate the current backoff delay as the number of timer ticks.
    backoffDelay = GMOS_MS_TO_TICKS (
        ((uint32_t) wkcReqClient->wkcReqBackoffDelay) * 1000);

    // Check for a running timeout.
    remainingTime = (int32_t) (wkcReqClient->wkcReqTimestamp +
        backoffDelay - gmosPalGetTimer ());
    if (remainingTime > 0) {
        return (uint32_t) remainingTime;
    }

    // Update the backoff delay for the next retry.
    nextDelay = ((((uint32_t) wkcReqClient->wkcReqBackoffDelay) *
        GMOS_OPENTHREAD_WKCREQ_BACKOFF_MULT) / 256);
    if (nextDelay <= GMOS_OPENTHREAD_WKCREQ_BACKOFF_MAX) {
        wkcReqClient->wkcReqBackoffDelay = (uint8_t) nextDelay;
    }

    // Randomise the backoff delay when it reaches the maximum value.
    else {
        uint8_t randomDelay = 0;
        while ((randomDelay < GMOS_OPENTHREAD_WKCREQ_BACKOFF_INIT) ||
            (randomDelay > GMOS_OPENTHREAD_WKCREQ_BACKOFF_MAX)) {
            gmosPalGetRandomBytes (&randomDelay, 1);
        }
        wkcReqClient->wkcReqBackoffDelay = randomDelay;
    }
    return 0;
}

/*
 * Initialises the well-known CoRE request client on startup.
 */
void gmosOpenThreadWkcReqClientInit (
    gmosOpenThreadWkcReqClient_t* wkcReqClient, const char* queryString)
{
    // Specify the query string used for request filtering.
    wkcReqClient->queryString = queryString;

    // Reset the well-known CoRE request client state.
    gmosOpenThreadWkcReqClientReset (wkcReqClient);
}

/*
 * Resets the well-known CoRE request client state.
 */
void gmosOpenThreadWkcReqClientReset (
    gmosOpenThreadWkcReqClient_t* wkcReqClient)
{
    uint32_t currentTimestamp = gmosPalGetTimer ();

    // Reset the SD-DNS state variables.
    wkcReqClient->wkcReqClientState = GMOS_OPENTHREAD_WKCREQ_CLIENT_STATE_IDLE;
    wkcReqClient->wkcReqBackoffDelay = GMOS_OPENTHREAD_WKCREQ_BACKOFF_INIT;
    wkcReqClient->wkcReqTimeout = currentTimestamp;
    wkcReqClient->wkcReqTimestamp = currentTimestamp;
    wkcReqClient->uriPath [0] = '\0';
}

/*
 * Indicates whether the current well-known CoRE request client data is
 * valid.
 */
bool gmosOpenThreadWkcReqClientDataValid (
    gmosOpenThreadWkcReqClient_t* wkcReqClient)
{
    int32_t delay;
    bool dataValid = false;

    // Well-known CoRE request data is valid in the idle state if it has
    // not timed out.
    if (wkcReqClient->wkcReqClientState ==
        GMOS_OPENTHREAD_WKCREQ_CLIENT_STATE_IDLE) {
        delay = (int32_t) (wkcReqClient->wkcReqTimeout - gmosPalGetTimer ());
        dataValid = (delay > 0) ? true : false;
    }
    return dataValid;
}

/*
 * Periodically polls the well-known CoRE request client instance while
 * a well-known CoRE request is in progress.
 */
gmosTaskStatus_t gmosOpenThreadWkcReqClientPoll (
    gmosOpenThreadStack_t* openThreadStack,
    gmosOpenThreadWkcReqClient_t* wkcReqClient,
    gmosOpenThreadSdDnsClient_t* sdDnsClient)
{
    gmosTaskStatus_t taskStatus = GMOS_TASK_SUSPEND;
    uint8_t nextState = wkcReqClient->wkcReqClientState;
    uint32_t retryDelay;

    // Run the well-known CoRE request client state machine.
    switch (wkcReqClient->wkcReqClientState) {

        // From the idle state initiate a well-known CoRE request.
        case GMOS_OPENTHREAD_WKCREQ_CLIENT_STATE_IDLE :
            if (gmosOpenThreadWkcReqClientDataValid (wkcReqClient)) {
                taskStatus = GMOS_TASK_RUN_IMMEDIATE;
            } else if (gmosOpenThreadWkcReqClientSend (
                openThreadStack, wkcReqClient, sdDnsClient)) {
                nextState = GMOS_OPENTHREAD_WKCREQ_CLIENT_STATE_CALLBACK;
                taskStatus = GMOS_OPENTHREAD_WKCREQ_CLIENT_TASK_POLL;
            } else {
                taskStatus = GMOS_OPENTHREAD_WKCREQ_CLIENT_TASK_POLL;
            }
            break;

        // Poll for well-known CoRE results from the callback state.
        case GMOS_OPENTHREAD_WKCREQ_CLIENT_STATE_CALLBACK :
            taskStatus = GMOS_OPENTHREAD_WKCREQ_CLIENT_TASK_POLL;
            break;

        // Poll for retry attempt.
        case GMOS_OPENTHREAD_WKCREQ_CLIENT_STATE_RETRY :
            retryDelay = gmosOpenThreadWkcReqClientBackoff (wkcReqClient);
            if (retryDelay > 0) {
                taskStatus = GMOS_TASK_RUN_LATER (
                    GMOS_MS_TO_TICKS (retryDelay));
            } else {
                gmosOpenThreadSdDnsClientReset (sdDnsClient);
                nextState = GMOS_OPENTHREAD_WKCREQ_CLIENT_STATE_IDLE;
                taskStatus = GMOS_OPENTHREAD_WKCREQ_CLIENT_TASK_POLL;
            }
            break;
    }
    wkcReqClient->wkcReqClientState = nextState;
    return taskStatus;
}
