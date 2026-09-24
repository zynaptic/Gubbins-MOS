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
 * This header defines the common API for requesting remote device
 * resource information from a CoRE Link resource directory according to
 * RFC 9176.
 */

#ifndef GMOS_OPENTHREAD_RESREQ_H
#define GMOS_OPENTHREAD_RESREQ_H

#include <stdint.h>
#include <stdbool.h>
#include "gmos-scheduler.h"
#include "gmos-buffers.h"
#include "gmos-openthread-sddns.h"
#include "gmos-openthread-wkcreq.h"

/**
 * Defines the GubbinsMOS OpenThread resource directory request client
 * structure that is used for encapsulating all the client data.
 */
typedef struct gmosOpenThreadResReqClient_t {

    // This is a pointer to the GubbinsMOS OpenThread stack instance
    // that is to be used for communication with the resource directory.
    gmosOpenThreadStack_t* openThreadStack;

    // This is a pointer to the string which specifies the URL query
    // option to be used in requests.
    const char* queryString;

    // This is the GubbinsMOS scheduler task state that is used to
    // run the resource directory request access task.
    gmosTaskState_t resReqTask;

    // This is a GubbinsMOS buffer that is used to store the result of
    // the last resource directory request.
    gmosBuffer_t resultData;

    // This is the SD-DNS client instance that is used for service
    // discovery.
    gmosOpenThreadSdDnsClient_t sdDnsClient;

    // This is the well-known CoRE request client instance that is used
    // for service discovery.
    gmosOpenThreadWkcReqClient_t wkcReqClient;

    // This is the current state of the OpenThread CoRE resource
    // directory request client state machine.
    uint8_t resReqClientState;

} gmosOpenThreadResReqClient_t;

/**
 * Defines the GubbinsMOS OpenThread resource directory request result
 * structure that is used for encapsulating the result of a resource
 * directory request.
 */
typedef struct gmosOpenThreadResReqResult_t {

    // This is a GubbinsMOS buffer that is used to store the result of
    // the last resource directory request.
    gmosBuffer_t resultData;

} gmosOpenThreadResReqResult_t;

/**
 * Initialises the CoRE Link resource directory request client on
 * startup.
 * @param resReqClient This is the resource directory request client
 *     instance that is to be initialised on startup.
 * @param openThreadStack This is the OpenThread stack instance that is
 *     to be used for accessing the resource directory.
 * @return Returns a boolean value which will be set to true on
 *     successfully initialising the resource directory request client
 *     and false otherwise.
 */
bool gmosOpenThreadResReqClientInit (
    gmosOpenThreadResReqClient_t* resReqClient,
    gmosOpenThreadStack_t* openThreadStack);

/**
 * Initiates a CoRE Link resource directory request. This searches for a
 * CoRE Link resource directory using SD-DNS and then requests a list of
 * resources that match the specified query string parameter.
 * @param resReqClient This is the resource directory request client
 *     instance that is to be used for the resource query.
 * @param queryString This is a pointer to the query string that is to
 *     be used in the query request. It must remain valid for the
 *     duration of the query request transaction.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully initiating the query request and 'false' if the
 *     request client is not ready and the request should be retried at
 *     a later time.
 */
bool gmosOpenThreadResReqClientStartQuery (
    gmosOpenThreadResReqClient_t* resReqClient,
    const char* queryString);

/**
 * Accesses the response to a CoRE Link resource directory request. This
 * copies the response to the previous request into a local result data
 * structure for subsequent parsing and resets the CoRE Link resource
 * directory request client after use.
 * @param resReqClient This is the resource directory request client
 *     instance for which the response is being requested.
 * @param resReqResult This is a local result data structure, which on
 *     successful completion will contain the response to the prior CoRE
 *     Link resource directory request.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully transferring the response data to the local buffer
 *     and 'false' if the transfer should be retried at a later time.
 */
bool gmosOpenThreadResReqClientGetResult (
    gmosOpenThreadResReqClient_t* resReqClient,
    gmosOpenThreadResReqResult_t* resReqResult);

/**
 * Resets a local result data structure after use, releasing any
 * allocated resources.
 * @param resReqResult This is a local result data structure which is
 *     to be reset after use.
 */
void gmosOpenThreadResReqResultReset (
    gmosOpenThreadResReqResult_t* resReqResult);

/**
 * Gets the remote IPv6 address, port and resource path for the
 * specified entry in the CoRE Link resource directory result data
 * structure.
 * @param resReqResult This is a local result data structure which is
 *     being accessed for the remote server address.
 * @param index The local result data structure may contain multiple
 *     CoRE link resource directory entries, and this specifies the
 *     index of the entry to be accessed.
 * @param remoteAddr This is a pointer to a 16 entry byte array which
 *     on successful completion will be populated with the IPv6 address
 *     of the remote server.
 * @param remotePort This is a pointer to a 16-bit integer value which
 *     on successful completion will be populated with the port number
 *     to be used when accessing the remote server.
 * @param remotePath This is a pointer to a character array which on
 *     successful completion will be populated with a null terminated
 *     string representing the resource path on the remote server,
 *     including the leading backslash character.
 * @param remotePathSize This specifies the size of the remote path
 *     character array.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully extracting the remote server address, port number
 *     and resource path and 'false' otherwise.
 */
bool gmosOpenThreadResReqResultGetRemoteAddr (
    gmosOpenThreadResReqResult_t* resReqResult, uint8_t index,
    uint8_t* remoteAddr, uint16_t* remotePort, char* remotePath,
    uint16_t remotePathSize);

#endif // GMOS_OPENTHREAD_RESREQ_H
