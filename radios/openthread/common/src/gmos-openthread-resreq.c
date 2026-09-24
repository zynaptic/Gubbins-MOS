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

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-scheduler.h"
#include "gmos-openthread.h"
#include "gmos-openthread-resreq.h"
#include "gmos-openthread-sddns.h"
#include "gmos-openthread-wkcreq.h"
#include "openthread/ip6.h"
#include "openthread/coap.h"

// Select the SD-DNS service type to use. The current implementation
// uses plain CoAP accesses, but DTLS based CoAP would be preferable.
#define GMOS_OPENTHREAD_RESREQ_SERVICE_TYPE \
    "_core-rd._udp.default.service.arpa"

// Select the well-known CoRE request query string parameter which is
// used to select the CoRE link resource directory.
#define GMOS_OPENTHREAD_RESREQ_WKCREQ_QUERY \
    "rt=core.rd-lookup-res"

/*
 * Specify the state space for the OpenThread CoRE resource directory
 * request client state machine.
 */
typedef enum {
    GMOS_OPENTHREAD_RESREQ_CLIENT_STATE_INIT,
    GMOS_OPENTHREAD_RESREQ_CLIENT_STATE_STOPPED,
    GMOS_OPENTHREAD_RESREQ_CLIENT_STATE_IDLE,
    GMOS_OPENTHREAD_RESREQ_CLIENT_STATE_LOOKUP_SEND,
    GMOS_OPENTHREAD_RESREQ_CLIENT_STATE_LOOKUP_CALLBACK,
    GMOS_OPENTHREAD_RESREQ_CLIENT_STATE_LOOKUP_READY,
    GMOS_OPENTHREAD_RESREQ_CLIENT_STATE_RETRY,
    GMOS_OPENTHREAD_RESREQ_CLIENT_STATE_FAILED
} gmosOpenThreadResReqClientState_t;

/*
 * From the initialisation state, wait for the OpenThread network to
 * come online.
 */
static inline bool gmosOpenThreadResReqClientInitWait (
    gmosOpenThreadResReqClient_t* resReqClient)
{
    gmosOpenThreadStatus_t netStatus =
        gmosOpenThreadNetStatus (resReqClient->openThreadStack);
    return (netStatus == GMOS_OPENTHREAD_STATUS_SUCCESS) ? true : false;
}

/*
 * Callback handler for CoAP requests to the resource directory lookup.
 * This currently assumes that the filtered response fits in a single
 * CoAP transaction so that blockwise transfer is not required.
 */
static void gmosOpenThreadResReqClientCallback (void* callbackData,
    otMessage* coapMessage, const otMessageInfo* coapMessageInfo,
    otError otStatus)
{
    gmosOpenThreadResReqClient_t* resReqClient =
        (gmosOpenThreadResReqClient_t*) callbackData;
    (void) coapMessageInfo;
    otCoapCode coapStatus;
    otCoapOptionIterator coapOptIter;
    const otCoapOption* coapOpt;
    uint64_t coapOptValue;
    uint8_t msgData [GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE];
    uint_fast16_t msgOffset;
    uint_fast16_t msgLen;
    uint_fast16_t msgSize;
    uint_fast16_t readSize;

    // Drop responses received in an invalid state.
    if (resReqClient->resReqClientState !=
        GMOS_OPENTHREAD_RESREQ_CLIENT_STATE_LOOKUP_CALLBACK) {
        return;
    }

    // Check the CoAP return code. This should be 2.05 (content).
    if (otStatus == OT_ERROR_NONE) {
        coapStatus = otCoapMessageGetCode (coapMessage);
        GMOS_LOG_FMT (LOG_DEBUG,
            "OpenThread : CoRE-RD lookup CoAP status %d.%02d.",
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
                "OpenThread : CoRE-RD lookup block option 0x%08X",
                (uint32_t) coapOptValue);
            if ((coapOptValue & 0x08) != 0) {
                otStatus = OT_ERROR_NO_BUFS;
            }
        }
    }

    // Store data in a local buffer to support larger payloads.
    if (otStatus == OT_ERROR_NONE) {
        msgOffset = otMessageGetOffset (coapMessage);
        msgLen = otMessageGetLength (coapMessage) - msgOffset;
        while (msgLen > 0) {
            msgSize = msgLen;
            if (msgSize > sizeof (msgData)) {
                msgSize = sizeof (msgData);
            }
            readSize = otMessageRead (
                coapMessage, msgOffset, msgData, msgSize);
            if (readSize != msgSize) {
                otStatus = OT_ERROR_PARSE;
                break;
            }
            if (!gmosBufferAppend (&(resReqClient->resultData),
                msgData, readSize)) {
                otStatus = OT_ERROR_NO_BUFS;
                break;
            }
            msgOffset += readSize;
            msgLen -= readSize;
        }
    }

    // On successful data transfer, initiate parsing. Otherwise initiate
    // a retry attempt.
    if (otStatus == OT_ERROR_NONE) {
        resReqClient->resReqClientState =
            GMOS_OPENTHREAD_RESREQ_CLIENT_STATE_LOOKUP_READY;
    } else {
        gmosBufferReset (&(resReqClient->resultData), 0);
        resReqClient->resReqClientState =
            GMOS_OPENTHREAD_RESREQ_CLIENT_STATE_RETRY;
    }
    gmosSchedulerTaskResume (&(resReqClient->resReqTask));
}

/*
 * Initiate a CoAP request to look up the CoRE Link data from the
 * resource directory.
 */
static inline bool gmosOpenThreadResReqClientLookupSend (
    gmosOpenThreadResReqClient_t* resReqClient)
{
    otInstance* otStack = resReqClient->openThreadStack->otInstance;
    uint8_t* resDirAddr = resReqClient->sdDnsClient.serviceAddr;
    uint16_t resDirPort = resReqClient->sdDnsClient.servicePort;
    otMessage* coapMessage;
    otMessageInfo coapMessageInfo = { 0 };
    otError otStatus;
    uint_fast8_t i;

    // Create the new CoAP message and initialise the header with the
    // URI path for the resource lookup resource.
    coapMessage = otCoapNewMessage (otStack, NULL);
    if (coapMessage == NULL) {
        return false;
    }
    otCoapMessageInit (coapMessage,
        OT_COAP_TYPE_CONFIRMABLE, OT_COAP_CODE_GET);
    otCoapMessageGenerateToken (coapMessage,
        OT_COAP_DEFAULT_TOKEN_LENGTH);
    otStatus = otCoapMessageAppendUriPathOptions (coapMessage,
        resReqClient->wkcReqClient.uriPath);
    if (otStatus != OT_ERROR_NONE) {
        goto fail;
    }

    // Add the query string parameter to perform the resource lookup.
    otStatus = otCoapMessageAppendUriQueryOptions (coapMessage,
        resReqClient->queryString);
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
        &coapMessageInfo, gmosOpenThreadResReqClientCallback,
        resReqClient);
    if (otStatus != OT_ERROR_NONE) {
        goto fail;
    }
    return true;

    // Release allocated memory on failure.
fail :
    GMOS_LOG_FMT (LOG_DEBUG,
        "OpenThread : CoRE-RD lookup request failure status %d.",
        otStatus);
    if (coapMessage != NULL) {
        otMessageFree (coapMessage);
    }
    return false;
}

/*
 * Select the next processing step from the idle state.
 */
static inline gmosTaskStatus_t gmosOpenThreadResReqClientActionSelect (
    gmosOpenThreadResReqClient_t* resReqClient, uint8_t* nextState)
{
    // Issue a service discovery DNS request if the local cached entry
    // is stale.
    if (!gmosOpenThreadSdDnsClientDataValid (
        &(resReqClient->sdDnsClient))) {
        return gmosOpenThreadSdDnsClientPoll (
            resReqClient->openThreadStack, &(resReqClient->sdDnsClient));
    }

    // Issue a resource directory discovery request if the resource
    // directory URI path component is not known.
    else if (!gmosOpenThreadWkcReqClientDataValid (
        &(resReqClient->wkcReqClient))) {
        return gmosOpenThreadWkcReqClientPoll (
            resReqClient->openThreadStack, &(resReqClient->wkcReqClient),
            &(resReqClient->sdDnsClient));
    }

    // Start the resource directory lookup process.
    else {
        *nextState = GMOS_OPENTHREAD_RESREQ_CLIENT_STATE_LOOKUP_SEND;
        return GMOS_TASK_RUN_IMMEDIATE;
    }
}

/*
 * Implement the CoRE Link resource directory request client task.
 */
static inline gmosTaskStatus_t gmosOpenThreadResReqClientTaskFn (
    gmosOpenThreadResReqClient_t* resReqClient)
{
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    uint8_t nextState = resReqClient->resReqClientState;

    // Run the resource directory request client state machine.
    switch (resReqClient->resReqClientState) {

        // Wait for network initialisation.
        case GMOS_OPENTHREAD_RESREQ_CLIENT_STATE_INIT :
            if (gmosOpenThreadResReqClientInitWait (resReqClient)) {
                nextState = GMOS_OPENTHREAD_RESREQ_CLIENT_STATE_STOPPED;
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (1000));
            }
            break;

        // In the stopped state there is nothing to be done.
        case GMOS_OPENTHREAD_RESREQ_CLIENT_STATE_STOPPED :
            taskStatus = GMOS_TASK_SUSPEND;
            break;

        // From the idle state select the next processing step by
        // checking the appropriate timeouts.
        case GMOS_OPENTHREAD_RESREQ_CLIENT_STATE_IDLE :
            taskStatus = gmosOpenThreadResReqClientActionSelect (
                resReqClient, &nextState);
            break;

        // Start the resource directory lookup process.
        case GMOS_OPENTHREAD_RESREQ_CLIENT_STATE_LOOKUP_SEND :
            if (gmosOpenThreadResReqClientLookupSend (resReqClient)) {
                nextState = GMOS_OPENTHREAD_RESREQ_CLIENT_STATE_LOOKUP_CALLBACK;
                taskStatus = GMOS_TASK_SUSPEND;
            } else {
                nextState = GMOS_OPENTHREAD_RESREQ_CLIENT_STATE_IDLE;
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (1000));
            }
            break;

        // Suspend processing while responses to the resource directory
        // lookup are being received.
        case GMOS_OPENTHREAD_RESREQ_CLIENT_STATE_LOOKUP_CALLBACK :
        case GMOS_OPENTHREAD_RESREQ_CLIENT_STATE_LOOKUP_READY :
            taskStatus = GMOS_TASK_SUSPEND;
            break;

        // TODO: Retry may not be the appropriate approach. For now
        // suspend on failure.
        case GMOS_OPENTHREAD_RESREQ_CLIENT_STATE_RETRY :
            taskStatus = GMOS_TASK_SUSPEND;
            break;
    }
    resReqClient->resReqClientState = nextState;
    return taskStatus;
}

// Define the CoRE Link resource directory client task.
GMOS_TASK_DEFINITION (gmosOpenThreadResReqClientTask,
    gmosOpenThreadResReqClientTaskFn, gmosOpenThreadResReqClient_t);

/*
 * Initialises the CoRE Link resource directory request client on
 * startup.
 */
bool gmosOpenThreadResReqClientInit (
    gmosOpenThreadResReqClient_t* resReqClient,
    gmosOpenThreadStack_t* openThreadStack)
{
    // Reset the resource directory request client state machine.
    resReqClient->openThreadStack = openThreadStack;
    resReqClient->queryString = NULL;
    resReqClient->resReqClientState =
        GMOS_OPENTHREAD_RESREQ_CLIENT_STATE_INIT;

    // Initialise the received data buffer.
    gmosBufferInit (&(resReqClient->resultData));

    // Initialise the SD-DNS state.
    gmosOpenThreadSdDnsClientInit (&(resReqClient->sdDnsClient),
        GMOS_OPENTHREAD_RESREQ_SERVICE_TYPE);

    // Initialise the well-known CoRE request state.
    gmosOpenThreadWkcReqClientInit (&(resReqClient->wkcReqClient),
        GMOS_OPENTHREAD_RESREQ_WKCREQ_QUERY);

    // Run the resource directory request client task.
    gmosOpenThreadResReqClientTask_start (&(resReqClient->resReqTask),
        resReqClient, "OpenThread CoRE RD Request");
    return true;
}

/*
 * Initiates a CoRE Link resource directory request. This searches for a
 * CoRE Link resource directory using SD-DNS and then requests a list of
 * resources that match the specified query string parameter.
 */
bool gmosOpenThreadResReqClientStartQuery (
    gmosOpenThreadResReqClient_t* resReqClient,
    const char* queryString)
{
    bool startOk = false;

    // A query can only be initiated from the 'stopped' state.
    if (resReqClient->resReqClientState ==
        GMOS_OPENTHREAD_RESREQ_CLIENT_STATE_STOPPED) {
        resReqClient->queryString = queryString;
        resReqClient->resReqClientState =
            GMOS_OPENTHREAD_RESREQ_CLIENT_STATE_IDLE;
        gmosSchedulerTaskResume (&(resReqClient->resReqTask));
        startOk = true;
    }
    return startOk;
}

/*
 * Accesses the response to a CoRE Link resource directory request. This
 * copies the response to the previous request into a local result data
 * structure for subsequent parsing and resets the CoRE Link resource
 * directory request client after use.
 */
bool gmosOpenThreadResReqClientGetResult (
    gmosOpenThreadResReqClient_t* resReqClient,
    gmosOpenThreadResReqResult_t* resReqResult)
{
    bool responseOk = false;

    // A response can only be requested in the 'lookup ready' state.
    if (resReqClient->resReqClientState ==
        GMOS_OPENTHREAD_RESREQ_CLIENT_STATE_LOOKUP_READY) {

        // Populate the result data structure. Note that the internal
        // buffer must not contain any pre-existing data.
        gmosBufferInit (&(resReqResult->resultData));
        gmosBufferMove (&(resReqClient->resultData),
            &(resReqResult->resultData));

        // Reset the client state machine for subsequent accesses.
        resReqClient->queryString = NULL;
        resReqClient->resReqClientState =
            GMOS_OPENTHREAD_RESREQ_CLIENT_STATE_STOPPED;
        gmosSchedulerTaskResume (&(resReqClient->resReqTask));
        responseOk = true;
    }
    return responseOk;
}

/*
 * Resets a local result data structure after use, releasing any
 * allocated resources.
 */
void gmosOpenThreadResReqResultReset (
    gmosOpenThreadResReqResult_t* resReqResult)
{
    gmosBufferReset (&(resReqResult->resultData), 0);
}

/*
 * Gets the buffer offset for the CoRE Link resource entry at the
 * specified index, or returns a negative value if it does not exist.
 * This is not fully implemented, since initial use cases are not
 * expected to need access to multiple links.
 */
static inline int32_t gmosOpenThreadResReqResultGetOffset (
    gmosOpenThreadResReqResult_t* resReqResult, uint8_t index)
{
    uint_fast16_t bufSize;
    int32_t offset = -1;

    // Check for index 0.
    bufSize = gmosBufferGetSize (&(resReqResult->resultData));
    if (index == 0) {
        offset = (bufSize > 0) ? 0 : -1;
        goto out;
    }

    // TODO: Support index values > 0.

out:
    return offset;
}

/*
 * Checks the scheme part of the CoRE Link resource entry, including the
 * opening angle bracket.
 */
static inline int32_t gmosOpenThreadResReqResultCheckScheme (
    gmosOpenThreadResReqResult_t* resReqResult, uint16_t offset,
    uint16_t* remotePort)
{
    gmosBuffer_t* msgBuf = &(resReqResult->resultData);
    uint8_t msgData [10];
    uint_fast16_t readSize;
    uint_fast16_t sourceSize;
    const char coapPrefix [] = "<coap://";
    int32_t newOffset;

    // Read in the scheme part.
    readSize = sizeof (msgData);
    sourceSize = gmosBufferGetSize (msgBuf) - offset;
    if (readSize > sourceSize) {
        readSize = sourceSize;
    }
    gmosBufferRead (msgBuf, offset, msgData, readSize);

    // Check for CoAP prefix and set the default port for the scheme.
    if ((readSize < sizeof (coapPrefix) - 1) ||
        (memcmp (coapPrefix, msgData, sizeof (coapPrefix) - 1) != 0)) {
        newOffset = -1;
    } else {
        *remotePort = OT_DEFAULT_COAP_PORT;
        newOffset = offset + sizeof (coapPrefix) - 1;
    }
    return newOffset;
}

/*
 * Extracts the IPv6 address part of the CoRE Link resource entry.
 */
static inline int32_t gmosOpenThreadResReqResultGetAddress (
    gmosOpenThreadResReqResult_t* resReqResult, uint16_t offset,
    uint8_t* remoteAddr)
{
    gmosBuffer_t* msgBuf = &(resReqResult->resultData);
    uint8_t msgData [41];
    uint_fast16_t readSize;
    uint_fast16_t sourceSize;
    uint_fast8_t i;
    otIp6Address otAddress;
    otError otStatus;
    int32_t newOffset;

    // Read in the IPv6 address part.
    readSize = sizeof (msgData);
    sourceSize = gmosBufferGetSize (msgBuf) - offset;
    if (readSize > sourceSize) {
        readSize = sourceSize;
    }
    gmosBufferRead (msgBuf, offset, msgData, readSize);

    // Check for opening bracket delimiter. Then find closing bracket
    // delimiter and replace it with a null terminator prior to parsing.
    newOffset = -1;
    if (msgData [0] == '[') {
        for (i = 1; i < readSize; i++) {
            if (msgData [i] == ']') {
                msgData [i] = '\0';
                otStatus = otIp6AddressFromString (
                    (char*) &(msgData [1]), &otAddress);
                if (otStatus == OT_ERROR_NONE) {
                    newOffset = offset + i + 1;
                }
                break;
            }
        }
    }

    // Copy over the IPv6 address on successful completion.
    if (newOffset >= 0) {
        for (i = 0; i < 16; i++) {
            remoteAddr [i] = otAddress.mFields.m8 [i];
        }
    }
    return newOffset;
}

/*
 * Extracts the port number for the remote server if required.
 */
static inline int32_t gmosOpenThreadResReqResultGetPort (
    gmosOpenThreadResReqResult_t* resReqResult, uint16_t offset,
    uint16_t* remotePort)
{
    gmosBuffer_t* msgBuf = &(resReqResult->resultData);
    uint8_t msgData [7];
    uint_fast16_t readSize;
    uint_fast16_t sourceSize;
    uint_fast8_t i;
    uint32_t portNumber;
    int32_t newOffset;

    // Read in the server port part.
    readSize = sizeof (msgData);
    sourceSize = gmosBufferGetSize (msgBuf) - offset;
    if (readSize > sourceSize) {
        readSize = sourceSize;
    }
    gmosBufferRead (msgBuf, offset, msgData, readSize);

    // Leave default port setting if no port is specified.
    newOffset = -1;
    if (msgData [0] == '/') {
        newOffset = offset;
    }

    // Process port number as a sequence of decimal digits.
    else if (msgData [0] == ':') {
        portNumber = 0;
        for (i = 1; i < readSize; i++) {
            char nextChar = msgData [i];
            if ((nextChar >= '0') && (nextChar <= '9')) {
                portNumber = portNumber * 10 + nextChar - '0';
            } else {
                break;
            }
        }
        if ((i < readSize) && (msgData [i] == '/') &&
            (portNumber > 0) && (portNumber <= 0xFFFF)) {
            *remotePort = (uint16_t) portNumber;
            newOffset = offset + i;
        }
    }
    return newOffset;
}

/*
 * Extracts the path component on the remote server.
 */
static inline int32_t gmosOpenThreadResReqResultGetPath (
    gmosOpenThreadResReqResult_t* resReqResult, uint16_t offset,
    char* remotePath, uint16_t remotePathSize)
{
    gmosBuffer_t* msgBuf = &(resReqResult->resultData);
    uint_fast16_t readSize;
    uint_fast16_t sourceSize;
    uint_fast8_t i;
    int32_t newOffset;

    // Read in the server path part.
    readSize = remotePathSize;
    sourceSize = gmosBufferGetSize (msgBuf) - offset;
    if (readSize > sourceSize) {
        readSize = sourceSize;
    }
    gmosBufferRead (msgBuf, offset, (uint8_t*) remotePath, readSize);

    // Search for the closing angle bracket which indicates the end of
    // the URI path component.
    newOffset = -1;
    for (i = 0; i < readSize; i++) {
        if (remotePath [i] == '>') {
            remotePath [i] = '\0';
            newOffset = offset + i + 1;
            break;
        }
    }
    return newOffset;
}

/*
 * Gets the remote IPv6 address, port and resource path for the
 * specified entry in the CoRE Link resource directory result data
 * structure.
 */
bool gmosOpenThreadResReqResultGetRemoteAddr (
    gmosOpenThreadResReqResult_t* resReqResult, uint8_t index,
    uint8_t* remoteAddr, uint16_t* remotePort, char* remotePath,
    uint16_t remotePathSize)
{
    int32_t offset;

    // Get the offset for the given entry index.
    offset = gmosOpenThreadResReqResultGetOffset (resReqResult, index);

    // Check for a valid scheme component.
    if (offset >= 0) {
        offset = gmosOpenThreadResReqResultCheckScheme (
            resReqResult, offset, remotePort);
    }

    // Read IPv6 address component.
    if (offset >= 0) {
        offset = gmosOpenThreadResReqResultGetAddress (
            resReqResult, offset, remoteAddr);
    }

    // Read the IPv6 port component.
    if (offset >= 0) {
        offset = gmosOpenThreadResReqResultGetPort (
            resReqResult, offset, remotePort);
    }

    // Read the remote server path component.
    if (offset > 0) {
        offset = gmosOpenThreadResReqResultGetPath (
            resReqResult, offset, remotePath, remotePathSize);
    }
    return (offset >= 0) ? true : false;
}
