/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2023-2025 Zynaptic Limited
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
 * This header defines the common API for maintaining the device entry
 * in a CoRE Link resource directory according to RFC 9176.
 */

#ifndef GMOS_OPENTHREAD_RESDIR_H
#define GMOS_OPENTHREAD_RESDIR_H

#include <stdint.h>
#include <stdbool.h>
#include "gmos-openthread.h"

/**
 * Defines the GubbinsMOS OpenThread resource directory client structure
 * that is used for encapsulating all the client data.
 */
typedef struct gmosOpenThreadResDirClient_t {

    // This is a pointer to the GubbinsMOS OpenThread stack instance
    // that is to be used for communication with the resource directory.
    gmosOpenThreadStack_t* openThreadStack;

    // This is the GubbinsMOS scheduler task state that is used to
    // run the resource directory access task.
    gmosTaskState_t resDirTask;

    // This is the sector ID to be used when registering the device
    // with the resource directory.
    const char* sectorId;

    // This is the endpoint ID to be used when registering the device
    // with the resource directory.
    const char* endpointId;

    // This is a pointer to the CoRE Link resource directory entry which
    // is to be periodically sent to the resource directory. It should
    // be the same data that is exposed as the local CoRE Link
    // descriptor.
    const uint8_t* resDirEntryData;

    // This is the timeout that is used to force a resource directory
    // update cycle.
    uint32_t resDirEntryTimeout;

    // This is the timeout that is used to force an SD-DNS refresh cycle
    // when the SD-DNS entry is stale.
    uint32_t sdDnsTimeout;

    // This specifies the size of the resource directory entry as an
    // integer number of bytes.
    uint16_t resDirEntrySize;

    // This is the remote CoAP UDP port number to be used for accessing
    // the resource directory.
    uint16_t resDirPort;

    // This is the IPv6 address to be used for accessing the resource
    // directory.
    uint8_t resDirAddr [16];

    // This is the URI path component of the resource directory
    // registration location.
    char resDirRegPath [32];

    // This is the URI path component for the resource directory
    // enhtry management location.
    char resDirEntryPath [32];

    // This is the SD-DNS service label which is used to identify the
    // correct SD-DNS service during refresh cycles (the first 63 octet
    // label in the fully qualified service name).
    char sdDnsLabel [64];

    // This is the current state of the OpenThread CoRE resource
    // directory client state machine.
    uint8_t resDirClientState;

    // This specifies the current backoff delay for SD-DNS requests.
    uint8_t sdDnsBackoffDelay;

} gmosOpenThreadResDirClient_t;

/**
 * Initialises the CoRE Link resource directory client on startup.
 * @param resDirClient This is the resource directory client instance
 *     that is to be initialised on startup.
 * @param openThreadStack This is the OpenThread stack instance that is
 *     to be used for accessing the resource directory.
 * @param sectorId This is a pointer to a null terminated string of up
 *     to 63 characters which contains the sector ID to be used when
 *     registering the device with the resource directory. This is an
 *     optional field, and a null value may be used to indicate that
 *     the sector ID is not to be used. The contents of this string must
 *     remain valid and unchanged for the lifetime of the resource
 *     directory client.
 * @param endpointId This is a pointer to a null terminated string of up
 *     to 63 characters which contains the endpoint ID to be used when
 *     registering the device with the resource directory. The
 *     combination of sector ID and endpoint ID must be unique to the
 *     device. The contents of this string must remain valid and
 *     unchanged for the lifetime of the resource directory client.
 * @return Returns a boolean value which will be set to true on
 *     successfully initialising the resource directory client and false
 *     otherwise.
 */
bool gmosOpenThreadResDirClientInit (
    gmosOpenThreadResDirClient_t* resDirClient,
    gmosOpenThreadStack_t* openThreadStack, const char* sectorId,
    const char* endpointId);

/**
 * Sets the resource directory entry to be advertised via the CoRE Link
 * resource directory. Calling this function should immediately start
 * the resource registration or update process.
 * @param resDirClient This is the resource directory client instance
 *     that is to be used during resource registration.
 * @param resDirEntryData This is a pointer to the resource directory
 *     entry data to be used during registration. It should remain
 *     valid for the lifetime of the resource directory entry. A null
 *     reference may be used to disable an existing resource directory
 *     entry.
 * @param resDirEntrySize This is the size of the supplied resource
 *     directory entry as an integer number of octets.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully updating the resource directory entry and 'false'
 *     if the resource directory entry can not be updated at this time.
 */
bool gmosOpenThreadResDirSetEntry (
    gmosOpenThreadResDirClient_t* resDirClient,
    const uint8_t* resDirEntryData, uint16_t resDirEntrySize);

#endif // GMOS_OPENTHREAD_RESDIR_H
