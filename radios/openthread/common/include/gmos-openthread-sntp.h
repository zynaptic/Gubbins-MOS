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
 * This header defines the common API for maintaining the local UNIX
 * epoch wallclock time using the SNTP protocol.
 */

#ifndef GMOS_OPENTHREAD_SNTP_H
#define GMOS_OPENTHREAD_SNTP_H

#include <stdint.h>
#include <stdbool.h>
#include "gmos-openthread.h"

/**
 * Defines the GubbinsMOS OpenThread SNTP client structure that is used
 * for encapsulating all the client data.
 */
typedef struct gmosOpenThreadSntpClient_t {

    // This is a pointer to the GubbinsMOS OpenThread stack instance
    // that is to be used for communication with the NTP server.
    gmosOpenThreadStack_t* openThreadStack;

    // This is the GubbinsMOS scheduler task state that is used to
    // run the SNTP access task.
    gmosTaskState_t sntpTask;

    // This is the last NTP synchronisation time value.
    uint32_t lastNtpTime;

    // This is the local timestamp of the last NTP synchronisation.
    uint32_t lastNtpTimestamp;

    // This is the timeout that is used to force an SNTP synchronisation
    // cycle.
    uint32_t sntpSyncTimeout;

    // This is the timeout that is used to force an SD-DNS refresh cycle
    // when the SD-DNS entry is stale.
    uint32_t sdDnsTimeout;

    // This is the remote UDP port number to be used for accessing the
    // NTP server.
    uint16_t ntpPort;

    // This is the IPv6 address to be used for accessing the NTP server.
    uint8_t ntpAddr [16];

    // This is the current state of the OpenThread SNTP client state
    // machine.
    uint8_t sntpClientState;

    // This specifies the current backoff delay for SD-DNS requests.
    uint8_t sdDnsBackoffDelay;

} gmosOpenThreadSntpClient_t;

/**
 * Initialises the SNTP client on startup.
 * @param sntpClient This is the SNTP client instance that is to be
 *     initialised on startup.
 * @param openThreadStack This is the OpenThread stack instance that is
 *     to be used for accessing the NTP server.
 * @return Returns a boolean value which will be set to true on
 *     successfully initialising the SNTP client and false otherwise.
 */
bool gmosOpenThreadSntpClientInit (
    gmosOpenThreadSntpClient_t* sntpClient,
    gmosOpenThreadStack_t* openThreadStack);

/**
 * Accesses the current SNTP network time, expressed as the integer
 * number of milliseconds since the UNIX epoch.
 * @param This is the SNTP client instance that is to be used for
 *     determining the current time.
 * @return Returns the integer number of milliseconds since the UNIX
 *     epoch, or a zero value if the SNTP time is not synchronised.
 */
uint64_t gmosOpenThreadSntpClientGetTime (
    gmosOpenThreadSntpClient_t* sntpClient);

#endif // GMOS_OPENTHREAD_SNTP_H
