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
 * This file implements the local CoRE Link resource directory client
 * which is responsible for maintaining the directory entry for the
 * device.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-scheduler.h"
#include "gmos-openthread.h"
#include "gmos-openthread-resdir.h"
#include "openthread/dns_client.h"
#include "openthread/coap.h"

// Provide stringification macros.
#define STRING_TXT(_x_) #_x_
#define STRING_VAL(_x_) STRING_TXT(_x_)

// Select the SD-DNS service type to use. The current implementation
// uses plain CoAP accesses, but DTLS based CoAP would be preferable.
#define GMOS_OPENTHREAD_RESDIR_SERVICE_TYPE \
    "_core-rd._udp.default.service.arpa"

// Specify the resource directory entry lifetime to be used as an
// integer value and option string representation.
#define GMOS_OPENTHREAD_RESDIR_ENTRY_LIFETIME 120

/*
 * Specify the state space for the OpenThread CoRE resource directory
 * client state machine.
 */
typedef enum {
    GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_INIT,
    GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_IDLE,
    GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_SDDNS_BROWSE,
    GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_SDDNS_CALLBACK,
    GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_SDDNS_RETRY,
    GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_DISC_SEND,
    GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_DISC_CALLBACK,
    GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_DISC_RETRY,
    GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_REG_SEND,
    GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_REG_CALLBACK,
    GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_REG_RETRY,
    GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_UPD_SEND,
    GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_UPD_CALLBACK,
    GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_UPD_RETRY,
    GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_FAILED
} gmosOpenThreadResDirClientState_t;

/*
 * From the initialisation state, wait for the OpenThread network to
 * come online.
 */
static inline bool gmosOpenThreadResDirClientInitWait (
    gmosOpenThreadResDirClient_t* resDirClient)
{
    otInstance* otStack = resDirClient->openThreadStack->otInstance;
    bool initOk = false;
    gmosOpenThreadStatus_t netStatus =
        gmosOpenThreadNetStatus (resDirClient->openThreadStack);

    // TODO: Attempt to start the CoAP process here for now. There will
    // be a better place to do this.
    if (netStatus == GMOS_OPENTHREAD_STATUS_SUCCESS) {
        otError otStatus = otCoapStart (otStack, OT_DEFAULT_COAP_PORT);
        GMOS_LOG_FMT (LOG_DEBUG,
            "OpenThread : CoAP startup status %d.", otStatus);
        if (otStatus == OT_ERROR_NONE) {
            initOk = true;
        }
    }
    return initOk;
}

/*
 * Implement callback handler for SD-DNS browse requests.
 */
static void gmosOpenThreadResDirClientSdDnsCallback (otError otStatus,
    const otDnsBrowseResponse *sdDnsResponse, void *callbackData)
{
    gmosOpenThreadResDirClient_t* resDirClient =
        (gmosOpenThreadResDirClient_t*) callbackData;
    char labelBuffer [sizeof (resDirClient->sdDnsLabel)];
    otDnsServiceInfo serviceInfo;
    uint32_t i;

    // Drop responses received in an invalid state.
    if (resDirClient->resDirClientState !=
        GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_SDDNS_CALLBACK) {
        return;
    }

    // On startup use the first entry in the response list. On
    // subsequent requests the service label must match.
    for (i = 0; otStatus == OT_ERROR_NONE; i++) {
        otStatus = otDnsBrowseResponseGetServiceInstance (
            sdDnsResponse, i, labelBuffer, sizeof (labelBuffer));
        if (resDirClient->sdDnsLabel [0] == '\0') {
            break;
        }
        if (strncmp (labelBuffer, resDirClient->sdDnsLabel,
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
            resDirClient->resDirAddr [i] = addrBytes [i];
        }
        resDirClient->resDirPort = serviceInfo.mPort;

        // Take a local copy of the service label if required.
        if (resDirClient->sdDnsLabel [0] == '\0') {
            memcpy (resDirClient->sdDnsLabel,
                labelBuffer, sizeof (labelBuffer));
        }

        // Force DNS refresh at 80% of the service information TTL.
        resDirClient->sdDnsTimeout = gmosPalGetTimer () +
            GMOS_MS_TO_TICKS (serviceInfo.mTtl * 800);
        resDirClient->resDirClientState =
            GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_IDLE;

        // Log new DNS information if required.
        GMOS_LOG_FMT (LOG_DEBUG,
            "OpenThread : SD-DNS browse found service '%s'.",
                resDirClient->sdDnsLabel);
        GMOS_LOG_FMT (LOG_VERBOSE,
            "OpenThread : SD-DNS address [%02x%02x:%02x%02x:%02x%02x:"
            "%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x]:%d",
            addrBytes [0], addrBytes [1], addrBytes [2], addrBytes [3],
            addrBytes [4], addrBytes [5], addrBytes [6], addrBytes [7],
            addrBytes [8], addrBytes [9], addrBytes [10], addrBytes [11],
            addrBytes [12], addrBytes [13], addrBytes [14], addrBytes [15],
            serviceInfo.mPort);
        GMOS_LOG_FMT (LOG_VERBOSE,
            "OpenThread : SD-DNS service TTL %ds.", serviceInfo.mTtl);
    }

    // Attempt a retry if the request was not successful.
    else {
        GMOS_LOG_FMT (LOG_DEBUG,
            "OpenThread : SD-DNS browse callback failure status %d.",
            otStatus);
        resDirClient->resDirClientState =
            GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_SDDNS_RETRY;
    }

    // Resume state machine task execution.
    gmosSchedulerTaskResume (&(resDirClient->resDirTask));
    return;
}

/*
 * Initiate an SD-DNS browse request to search for the CoRE Link
 * resource directory.
 */
static inline bool gmosOpenThreadResDirClientSdDnsBrowse (
    gmosOpenThreadResDirClient_t* resDirClient)
{
    otInstance* otStack = resDirClient->openThreadStack->otInstance;
    otError otStatus;

    // Issue the SD-DNS service browsing request.
    otStatus = otDnsClientBrowse (
        otStack, GMOS_OPENTHREAD_RESDIR_SERVICE_TYPE,
        gmosOpenThreadResDirClientSdDnsCallback, resDirClient, NULL);

    // Attempt a retry if the request was not successful.
    if (otStatus != OT_ERROR_NONE) {
        GMOS_LOG_FMT (LOG_DEBUG,
            "OpenThread : SD-DNS browse request failure status %d",
            otStatus);
    }
    return (otStatus == OT_ERROR_NONE) ? true : false;
}

/*
 * Parse the resource directory discovery message which is contained
 * in a null terminated string. Returns false on parsing failure.
 * This currently assumes that the resource directory performs attribute
 * filtering correctly and only returns the resource link of interest.
 * It could be made more robust by checking the contents of the 'ct' and
 * 'rt' attributes before accepting the resource link.
 */
static otError gmosOpenThreadResDirClientDiscParse (
    gmosOpenThreadResDirClient_t* resDirClient, uint8_t* msgBuf)
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

    // Check that the resource path does not exceed the local allocated
    // storage.
    if (pathSize >= sizeof (resDirClient->resDirRegPath)) {
        return OT_ERROR_NO_BUFS;
    }

    // Store the resource path component locally as a null terminated
    // string.
    memcpy (resDirClient->resDirRegPath, &(msgBuf [2]), pathSize);
    resDirClient->resDirRegPath [pathSize] = '\0';
    GMOS_LOG_FMT (LOG_DEBUG,
        "OpenThread : CoRE-RD resource registration path : '%s'",
        resDirClient->resDirRegPath);
    return OT_ERROR_NONE;
}

/*
 * Callback handler for CoAP requests to discover the registration URI
 * on the resource directory.
 */
static void gmosOpenThreadResDirClientDiscCallback (void* callbackData,
    otMessage* coapMessage, const otMessageInfo* coapMessageInfo,
    otError otStatus)
{
    gmosOpenThreadResDirClient_t* resDirClient =
        (gmosOpenThreadResDirClient_t*) callbackData;
    (void) coapMessageInfo;
    otCoapCode coapStatus;
    uint8_t msgBuf [sizeof (resDirClient->resDirRegPath) + 32];
    uint_fast16_t msgOffset;
    uint_fast16_t msgLen;
    uint_fast16_t msgSize;

    // Drop responses received in an invalid state.
    if (resDirClient->resDirClientState !=
        GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_DISC_CALLBACK) {
        return;
    }

    // Check the CoAP return code. This should be 2.05 (content).
    if (otStatus == OT_ERROR_NONE) {
        coapStatus = otCoapMessageGetCode (coapMessage);
        GMOS_LOG_FMT (LOG_DEBUG,
            "OpenThread : CoRE-RD discovery CoAP status %d.%02d.",
            (coapStatus >> 5) & 0x07, coapStatus & 0x1F);
        if (coapStatus != OT_COAP_CODE_CONTENT) {
            otStatus = OT_ERROR_REJECTED;
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
            otStatus = OT_ERROR_GENERIC;
        }
    }

    // Parse the resource directory discovery response.
    if (otStatus == OT_ERROR_NONE) {
        otStatus = gmosOpenThreadResDirClientDiscParse (
            resDirClient, msgBuf);
    }

    // Attempt a retry if the request was not successful.
    if (otStatus == OT_ERROR_NONE) {
        resDirClient->resDirClientState =
            GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_IDLE;
    } else {
        GMOS_LOG_FMT (LOG_DEBUG,
            "OpenThread : CoRE-RD discovery callback failure status %d.",
            otStatus);
        resDirClient->resDirClientState =
            GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_DISC_RETRY;
    }

    // Resume state machine task execution.
    gmosSchedulerTaskResume (&(resDirClient->resDirTask));
    return;
}

/*
 * Initiate a CoAP request to discover the registration URI on the
 * resource directory.
 */
static inline bool gmosOpenThreadResDirClientDiscSend (
    gmosOpenThreadResDirClient_t* resDirClient)
{
    otInstance* otStack = resDirClient->openThreadStack->otInstance;
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
    otStatus = otCoapMessageAppendUriQueryOption (coapMessage,
        "rt=core.rd");
    if (otStatus != OT_ERROR_NONE) {
        goto fail;
    }

    // Set the CoAP message destination and send the request. All
    // additional options are left as zero to select the defaults.
    for (i = 0; i < 16; i++) {
        coapMessageInfo.mPeerAddr.mFields.m8 [i] =
            resDirClient->resDirAddr [i];
    }
    coapMessageInfo.mPeerPort = OT_DEFAULT_COAP_PORT;
    otStatus = otCoapSendRequest (otStack, coapMessage,
        &coapMessageInfo, gmosOpenThreadResDirClientDiscCallback,
        resDirClient);
    if (otStatus != OT_ERROR_NONE) {
        goto fail;
    }
    return true;

    // Release allocated memory on failure.
fail :
    GMOS_LOG_FMT (LOG_DEBUG,
        "OpenThread : CoRE-RD discovery request failure status %d.",
        otStatus);
    if (coapMessage != NULL) {
        otMessageFree (coapMessage);
    }
    return false;
}

/*
 * Callback handler for CoAP requests to register the device with the
 * resource directory.
 */
static void gmosOpenThreadResDirClientRegCallback (void* callbackData,
    otMessage* coapMessage, const otMessageInfo* coapMessageInfo,
    otError otStatus)
{
    gmosOpenThreadResDirClient_t* resDirClient =
        (gmosOpenThreadResDirClient_t*) callbackData;
    (void) coapMessageInfo;
    otCoapCode coapStatus;
    otCoapOptionIterator coapOptionIter;
    const otCoapOption* coapOption;
    uint8_t locBuf [sizeof (resDirClient->resDirEntryPath)];
    uint_fast16_t locOffset;

    // Drop responses received in an invalid state.
    if (resDirClient->resDirClientState !=
        GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_REG_CALLBACK) {
        return;
    }

    // Check the CoAP return code. This should be 2.01 (created).
    if (otStatus == OT_ERROR_NONE) {
        coapStatus = otCoapMessageGetCode (coapMessage);
        GMOS_LOG_FMT (LOG_DEBUG,
            "OpenThread : CoRE-RD registration CoAP status %d.%02d.",
            (coapStatus >> 5) & 0x07, coapStatus & 0x1F);
        if (coapStatus != OT_COAP_CODE_CREATED) {
            otStatus = OT_ERROR_REJECTED;
        }
    }

    // The location path option indicates the resource path to be used
    // for managing the resource directory entry. It needs to be
    // assembled from multiple option entries.
    if (otStatus == OT_ERROR_NONE) {
        otStatus = otCoapOptionIteratorInit (&coapOptionIter, coapMessage);
        locOffset = 0;
    }
    while (otStatus == OT_ERROR_NONE) {
        if (locOffset == 0) {
            coapOption = otCoapOptionIteratorGetFirstOptionMatching (
                &coapOptionIter, OT_COAP_OPTION_LOCATION_PATH);
        } else {
            coapOption = otCoapOptionIteratorGetNextOptionMatching (
                &coapOptionIter, OT_COAP_OPTION_LOCATION_PATH);
        }

        // Add string terminator or path separators as required.
        if (coapOption == NULL) {
            locBuf [locOffset] = '\0';
            break;
        } else if (locOffset != 0) {
            locBuf [locOffset++] = '/';
        }

        // Append the path segment.
        if (locOffset + coapOption->mLength >= sizeof (locBuf)) {
            otStatus = OT_ERROR_NO_BUFS;
            break;
        }
        otStatus = otCoapOptionIteratorGetOptionValue (
            &coapOptionIter, &(locBuf [locOffset]));
        locOffset += coapOption->mLength;
    }

    // Store the valid resource location on success and schedule a
    // refresh cycle at 80% of the registered entry lifetime.
    if (otStatus == OT_ERROR_NONE) {
        memcpy (resDirClient->resDirEntryPath, locBuf, sizeof (locBuf));
        GMOS_LOG_FMT (LOG_DEBUG,
            "OpenThread : CoRE-RD resource access path : '%s'",
        resDirClient->resDirEntryPath);
        resDirClient->resDirEntryTimeout = gmosPalGetTimer () +
            GMOS_MS_TO_TICKS (GMOS_OPENTHREAD_RESDIR_ENTRY_LIFETIME * 800);
        resDirClient->resDirClientState =
            GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_IDLE;
    }

    // Attempt a retry if the request was not successful.
    else {
        GMOS_LOG_FMT (LOG_DEBUG,
            "OpenThread : CoRE-RD registration callback failure status %d.",
            otStatus);
        resDirClient->resDirClientState =
            GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_REG_RETRY;
    }

    // Resume state machine task execution.
    gmosSchedulerTaskResume (&(resDirClient->resDirTask));
    return;
}

/*
 * Initiate a CoAP request to register the CoRE Link data with the
 * resource directory.
 */
static inline bool gmosOpenThreadResDirClientRegSend (
    gmosOpenThreadResDirClient_t* resDirClient)
{
    otInstance* otStack = resDirClient->openThreadStack->otInstance;
    otMessage* coapMessage;
    otMessageInfo coapMessageInfo = { 0 };
    otError otStatus;
    char optBuf [67];
    uint_fast8_t i;

    // Create the new CoAP message and initialise the header with the
    // URI path for the registration resource.
    coapMessage = otCoapNewMessage (otStack, NULL);
    if (coapMessage == NULL) {
        return false;
    }
    otCoapMessageInit (coapMessage,
        OT_COAP_TYPE_CONFIRMABLE, OT_COAP_CODE_POST);
    otCoapMessageGenerateToken (coapMessage,
        OT_COAP_DEFAULT_TOKEN_LENGTH);
    otStatus = otCoapMessageAppendUriPathOptions (coapMessage,
        resDirClient->resDirRegPath);

    // Specify the CoRE Link data format for the payload encoding.
    if (otStatus == OT_ERROR_NONE) {
        otStatus = otCoapMessageAppendContentFormatOption (coapMessage,
            OT_COAP_OPTION_CONTENT_FORMAT_LINK_FORMAT);
    }

    // Specify the endpoint ID which uniquely identifies this device to
    // resource directory.
    if (otStatus == OT_ERROR_NONE) {
        optBuf [0] = 'e';
        optBuf [1] = 'p';
        optBuf [2] = '=';
        memcpy (&(optBuf [3]), resDirClient->endpointId, 64);
        otStatus = otCoapMessageAppendUriQueryOption (
            coapMessage, optBuf);
    }

    // Specify the sector ID which optionally identifies this device to
    // the resource directory.
    if ((otStatus == OT_ERROR_NONE) && (resDirClient->sectorId != NULL)) {
        optBuf [0] = 'd';
        optBuf [1] = '=';
        memcpy (&(optBuf [2]), resDirClient->sectorId, 64);
        otStatus = otCoapMessageAppendUriQueryOption (
            coapMessage, optBuf);
    }

    // Specify the resource directory entry lifetime to be used.
    if (otStatus == OT_ERROR_NONE) {
        otStatus = otCoapMessageAppendUriQueryOption (coapMessage,
            "lt=" STRING_VAL (GMOS_OPENTHREAD_RESDIR_ENTRY_LIFETIME));
    }

    // Append the payload which contains the CoRE Link descriptors for
    // the local device.
    if (otStatus == OT_ERROR_NONE) {
        otStatus = otCoapMessageSetPayloadMarker (coapMessage);
    }
    if (otStatus == OT_ERROR_NONE) {
        otStatus = otMessageAppend (coapMessage,
            resDirClient->resDirEntryData,
            strlen ((const char*) resDirClient->resDirEntryData));
    }

    // Set the CoAP message destination and send the request. All
    // additional options are left as zero to select the defaults.
    if (otStatus == OT_ERROR_NONE) {
        for (i = 0; i < 16; i++) {
            coapMessageInfo.mPeerAddr.mFields.m8 [i] =
                resDirClient->resDirAddr [i];
        }
        coapMessageInfo.mPeerPort = OT_DEFAULT_COAP_PORT;
        otStatus = otCoapSendRequest (otStack, coapMessage,
            &coapMessageInfo, gmosOpenThreadResDirClientRegCallback,
            resDirClient);
    }

    // Release allocated memory on failure.
    if (otStatus != OT_ERROR_NONE) {
        GMOS_LOG_FMT (LOG_DEBUG,
            "OpenThread : CoRE-RD registration request failure status %d.",
            otStatus);
        if (coapMessage != NULL) {
            otMessageFree (coapMessage);
        }
    }
    return (otStatus == OT_ERROR_NONE) ? true : false;
}

/*
 * Callback handler for CoAP requests to update the device registration
 * with the resource directory.
 */
static void gmosOpenThreadResDirClientUpdCallback (void* callbackData,
    otMessage* coapMessage, const otMessageInfo* coapMessageInfo,
    otError otStatus)
{
    gmosOpenThreadResDirClient_t* resDirClient =
        (gmosOpenThreadResDirClient_t*) callbackData;
    (void) coapMessageInfo;
    otCoapCode coapStatus;

    // Drop responses received in an invalid state.
    if (resDirClient->resDirClientState !=
        GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_UPD_CALLBACK) {
        return;
    }

    // Check the CoAP return code. This should be 2.04 (changed).
    if (otStatus == OT_ERROR_NONE) {
        coapStatus = otCoapMessageGetCode (coapMessage);
        GMOS_LOG_FMT (LOG_DEBUG,
            "OpenThread : CoRE-RD update CoAP status %d.%02d.",
            (coapStatus >> 5) & 0x07, coapStatus & 0x1F);
        if (coapStatus != OT_COAP_CODE_CHANGED) {
            otStatus = OT_ERROR_REJECTED;
        }
    }

    // Schedule a refresh cycle at 80% of the registered entry lifetime.
    if (otStatus == OT_ERROR_NONE) {
        resDirClient->resDirEntryTimeout = gmosPalGetTimer () +
            GMOS_MS_TO_TICKS (GMOS_OPENTHREAD_RESDIR_ENTRY_LIFETIME * 800);
        resDirClient->resDirClientState =
            GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_IDLE;
    }

    // Attempt a retry if the request was not successful.
    else {
        GMOS_LOG_FMT (LOG_DEBUG,
            "OpenThread : CoRE-RD update callback failure status %d.",
            otStatus);
        resDirClient->resDirClientState =
            GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_UPD_RETRY;
    }

    // Resume state machine task execution.
    gmosSchedulerTaskResume (&(resDirClient->resDirTask));
    return;
}

/*
 * Initiate a CoAP request to update the CoRE Link data entry on the
 * resource directory.
 * TODO: Consider including the base URI to recover from SLAAC IP
 * address changes.
 */
static inline bool gmosOpenThreadResDirClientUpdSend (
    gmosOpenThreadResDirClient_t* resDirClient)
{
    otInstance* otStack = resDirClient->openThreadStack->otInstance;
    otMessage* coapMessage;
    otMessageInfo coapMessageInfo = { 0 };
    otError otStatus;
    uint_fast8_t i;

    // Create the new CoAP message and initialise the header with the
    // URI path for the registered resource.
    coapMessage = otCoapNewMessage (otStack, NULL);
    if (coapMessage == NULL) {
        return false;
    }
    otCoapMessageInit (coapMessage,
        OT_COAP_TYPE_CONFIRMABLE, OT_COAP_CODE_POST);
    otCoapMessageGenerateToken (coapMessage,
        OT_COAP_DEFAULT_TOKEN_LENGTH);
    otStatus = otCoapMessageAppendUriPathOptions (coapMessage,
        resDirClient->resDirEntryPath);

    // Specify the resource directory entry lifetime to be used.
    if (otStatus == OT_ERROR_NONE) {
        otStatus = otCoapMessageAppendUriQueryOption (coapMessage,
            "lt=" STRING_VAL (GMOS_OPENTHREAD_RESDIR_ENTRY_LIFETIME));
    }

    // Set the CoAP message destination and send the request. All
    // additional options are left as zero to select the defaults.
    if (otStatus == OT_ERROR_NONE) {
        for (i = 0; i < 16; i++) {
            coapMessageInfo.mPeerAddr.mFields.m8 [i] =
                resDirClient->resDirAddr [i];
        }
        coapMessageInfo.mPeerPort = OT_DEFAULT_COAP_PORT;
        otStatus = otCoapSendRequest (otStack, coapMessage,
            &coapMessageInfo, gmosOpenThreadResDirClientUpdCallback,
            resDirClient);
    }

    // Release allocated memory on failure.
    if (otStatus != OT_ERROR_NONE) {
        GMOS_LOG_FMT (LOG_DEBUG,
            "OpenThread : CoRE-RD update request failure status %d.",
            otStatus);
        if (coapMessage != NULL) {
            otMessageFree (coapMessage);
        }
    }
    return (otStatus == OT_ERROR_NONE) ? true : false;
}

/*
 * Select the next processing step from the idle state.
 */
static inline gmosTaskStatus_t gmosOpenThreadResDirClientActionSelect (
    gmosOpenThreadResDirClient_t* resDirClient, uint8_t* nextState)
{
    uint32_t currentTime = gmosPalGetTimer ();
    int32_t delay;

    // Suspend processing if there is no resource directory entry to
    // be assigned.
    if (resDirClient->resDirEntryData == NULL) {
        return GMOS_TASK_SUSPEND;
    }

    // No processing is required if the directory entry timeout has not
    // already expired.
    delay = (int32_t) (resDirClient->resDirEntryTimeout - currentTime);
    if (delay > 0) {
        return GMOS_TASK_RUN_LATER (delay);
    }

    // Issue a service discovery DNS request if the local cached entry
    // is stale.
    delay = (int32_t) (resDirClient->sdDnsTimeout - currentTime);
    if (delay <= 0) {
        *nextState = GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_SDDNS_BROWSE;
        return GMOS_TASK_RUN_IMMEDIATE;
    }

    // Issue a resource directory discovery request if the resource
    // directory URI path component is not known.
    if (resDirClient->resDirRegPath [0] == '\0') {
        *nextState = GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_DISC_SEND;
        return GMOS_TASK_RUN_IMMEDIATE;
    }

    // Issue a resource directory registration request if the device
    // is not currently registered.
    if (resDirClient->resDirEntryPath [0] == '\0') {
        *nextState = GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_REG_SEND;
        return GMOS_TASK_RUN_IMMEDIATE;
    }

    // Issue a resource directory refresh request.
    *nextState = GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_UPD_SEND;
    return GMOS_TASK_RUN_IMMEDIATE;
}

/*
 * Restart the registration process on failure. Ideally, this should
 * search for an alternative resource directory from the one which
 * failed registration, but this is not currently supported.
 */
static inline gmosTaskStatus_t gmosOpenThreadResDirClientRestart (
    gmosOpenThreadResDirClient_t* resDirClient)
{
    uint32_t restartTimeout = gmosPalGetTimer ();

    // Specify the restart delay. This currently just uses a fixed
    // retry interval. Adaptive backoff would be preferable.
    uint32_t delay = GMOS_MS_TO_TICKS (60000);

    // Reset the resource directory entry.
    resDirClient->resDirEntryTimeout = restartTimeout + delay;
    resDirClient->resDirRegPath [0] = '\0';
    resDirClient->resDirEntryPath [0] = '\0';

    // Reset the SD-DNS state and mark it as stale.
    resDirClient->sdDnsTimeout = restartTimeout;
    resDirClient->sdDnsLabel [0] = '\0';

    return GMOS_TASK_RUN_LATER (delay);
}

/*
 * Implement the CoRE Link resource directory client task.
 */
static inline gmosTaskStatus_t gmosOpenThreadResDirClientTaskFn (
    gmosOpenThreadResDirClient_t* resDirClient)
{
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    uint8_t nextState = resDirClient->resDirClientState;

    // Run the resource directory client state machine.
    switch (resDirClient->resDirClientState) {

        // Wait for network initialisation.
        case GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_INIT :
            if (gmosOpenThreadResDirClientInitWait (resDirClient)) {
                nextState = GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_IDLE;
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (1000));
            }
            break;

        // From the idle state select the next processing step by
        // checking the appropriate timeouts.
        case GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_IDLE :
            taskStatus = gmosOpenThreadResDirClientActionSelect (
                resDirClient, &nextState);
            break;

        // Initiate an SD-DNS browse request to obtain the location of
        // the CoRE link resource directory.
        case GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_SDDNS_BROWSE :
            if (gmosOpenThreadResDirClientSdDnsBrowse (resDirClient)) {
                nextState = GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_SDDNS_CALLBACK;
                taskStatus = GMOS_TASK_SUSPEND;
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (1000));
            }
            break;

        // Suspend task processing while the SD-DNS callbacks are being
        // processed.
        case GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_SDDNS_CALLBACK :
            taskStatus = GMOS_TASK_SUSPEND;
            break;

        // Retry the SD-DNS request after a short delay.
        case GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_SDDNS_RETRY :
            nextState = GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_SDDNS_BROWSE;
            taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (8000));
            break;

        // Send the CoRE-RD discovery request to the resource directory.
        case GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_DISC_SEND :
            if (gmosOpenThreadResDirClientDiscSend (resDirClient)) {
                nextState = GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_DISC_CALLBACK;
                taskStatus = GMOS_TASK_SUSPEND;
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (1000));
            }
            break;

        // Suspend task processing while the resource directory
        // discovery callbacks are being processed.
        case GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_DISC_CALLBACK :
            taskStatus = GMOS_TASK_SUSPEND;
            break;

        // Retry the CoRE-RD discovery request after a short delay.
        case GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_DISC_RETRY :
            nextState = GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_DISC_SEND;
            taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (8000));
            break;

        // Send the registration data to the resource directory.
        case GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_REG_SEND :
            if (gmosOpenThreadResDirClientRegSend (resDirClient)) {
                nextState = GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_REG_CALLBACK;
                taskStatus = GMOS_TASK_SUSPEND;
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (1000));
            }
            break;

        // Suspend task processing while the resource directory
        // registration callbacks are being processed.
        case GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_REG_CALLBACK :
            taskStatus = GMOS_TASK_SUSPEND;
            break;

        // The full discovery process needs to be re-run on failure to
        // register with the resource directory.
        case GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_REG_RETRY :
            nextState = GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_IDLE;
            taskStatus = gmosOpenThreadResDirClientRestart (resDirClient);
            break;

        // Send the registration update data to the resource directory.
        case GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_UPD_SEND :
            if (gmosOpenThreadResDirClientUpdSend (resDirClient)) {
                nextState = GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_UPD_CALLBACK;
                taskStatus = GMOS_TASK_SUSPEND;
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (1000));
            }
            break;

        // Suspend task processing while the resource directory
        // registration update callbacks are being processed.
        case GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_UPD_CALLBACK :
            taskStatus = GMOS_TASK_SUSPEND;
            break;

        // The full discovery process will be re-run on failure to
        // update the resource directory.
        case GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_UPD_RETRY :
            nextState = GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_IDLE;
            taskStatus = gmosOpenThreadResDirClientRestart (resDirClient);
            break;

        // Handle failure conditions.
        default :
            // TODO: Failure mode.
            taskStatus = GMOS_TASK_SUSPEND;
            break;
    }
    resDirClient->resDirClientState = nextState;
    return taskStatus;
}

// Define the CoRE Link resource directory client task.
GMOS_TASK_DEFINITION (gmosOpenThreadResDirClientTask,
    gmosOpenThreadResDirClientTaskFn, gmosOpenThreadResDirClient_t);

/*
 * Initialises the CoRE Link resource directory client on startup.
 */
bool gmosOpenThreadResDirClientInit (
    gmosOpenThreadResDirClient_t* resDirClient,
    gmosOpenThreadStack_t* openThreadStack, const char* sectorId,
    const char* endpointId)
{
    uint32_t initTimeout = gmosPalGetTimer ();

    // Check for valid sector ID.
    if ((sectorId == NULL) || (strlen (sectorId) < 64)) {
        resDirClient->sectorId = sectorId;
    } else {
        return false;
    }

    // Check for valid endpoint ID.
    if ((endpointId != NULL) && (strlen (endpointId) < 64)) {
        resDirClient->endpointId = endpointId;
    } else {
        return false;
    }

    // Reset the resource directory client state machine.
    resDirClient->openThreadStack = openThreadStack;
    resDirClient->resDirClientState =
        GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_INIT;

    // Reset the resource directory entry.
    resDirClient->resDirEntryData = NULL;
    resDirClient->resDirEntrySize = 0;
    resDirClient->resDirEntryTimeout = initTimeout;
    resDirClient->resDirRegPath [0] = '\0';
    resDirClient->resDirEntryPath [0] = '\0';

    // Reset the SD-DNS state.
    resDirClient->sdDnsTimeout = initTimeout;
    resDirClient->sdDnsLabel [0] = '\0';

    // Run the resource directory client task.
    gmosOpenThreadResDirClientTask_start (&(resDirClient->resDirTask),
        resDirClient, "OpenThread CoRE RD Client");
    return true;
}

/*
 * Sets the resource directory entry to be advertised via the CoRE Link
 * resource directory. Calling this function should immediately start
 * the resource registration or update process.
 */
bool gmosOpenThreadResDirSetEntry (
    gmosOpenThreadResDirClient_t* resDirClient,
    const uint8_t* resDirEntryData, uint16_t resDirEntrySize)
{
    uint8_t clientState = resDirClient->resDirClientState;

    // Only modify the resource directory entry during initialisation
    // or when in the idle state.
    if ((clientState != GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_INIT) &&
        (clientState != GMOS_OPENTHREAD_RESDIR_CLIENT_STATE_IDLE)) {
        return false;
    }

    // Update the directory entry and reschedule the processing task.
    // Note that this clears the resource entry path to force a new
    // registration cycle, since payload updates to the resource entry
    // path are not supported.
    resDirClient->resDirEntryData = resDirEntryData;
    resDirClient->resDirEntrySize = resDirEntrySize;
    resDirClient->resDirEntryTimeout = gmosPalGetTimer ();
    resDirClient->resDirEntryPath [0] = '\0';
    gmosSchedulerTaskResume (&(resDirClient->resDirTask));
    return true;
}
