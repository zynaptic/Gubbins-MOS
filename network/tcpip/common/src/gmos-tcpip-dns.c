/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2022 Zynaptic Limited
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
 * This file provides a common DNS client implementation for use with
 * vendor supplied and hardware accelerated TCP/IP stacks.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-buffers.h"
#include "gmos-scheduler.h"
#include "gmos-tcpip-config.h"
#include "gmos-tcpip-stack.h"
#include "gmos-tcpip-dns.h"
#include "gmos-driver-tcpip.h"

/*
 * Specify the standard DNS port for local use.
 */
#define GMOS_TCPIP_DNS_COMMON_PORT 53

/*
 * Specify the DNS record type for IPv4 'A' records.
 */
#define GMOS_TCPIP_DNS_RECORD_TYPE_A 1

/*
 * Specify the DNS record type for IPv6 'AAAA' records.
 */
#define GMOS_TCPIP_DNS_RECORD_TYPE_AAAA 28

/*
 * Defines the DNS cache entry state values.
 */
typedef enum {
    GMOS_TCPIP_DNS_CACHE_ENTRY_STATE_OPEN,
    GMOS_TCPIP_DNS_CACHE_ENTRY_STATE_REQUEST,
    GMOS_TCPIP_DNS_CACHE_ENTRY_STATE_WAIT,
    GMOS_TCPIP_DNS_CACHE_ENTRY_STATE_VALID,
    GMOS_TCPIP_DNS_CACHE_ENTRY_STATE_TIMEOUT,
    GMOS_TCPIP_DNS_CACHE_ENTRY_STATE_NOT_VALID,
    GMOS_TCPIP_DNS_CACHE_ENTRY_STATE_EXPIRED
} gmosTcpipDnsCacheEntryState_t;

/*
 * Defines the fixed fields of a DNS cache entry, which are stored at
 * the start of each DNS cache entry buffer.
 */
typedef struct gmosTcpipDnsCacheEntry_t {

    // Specifies the cache entry expiry timestamp.
    uint32_t expiryTimestamp;

    // Specifies the transaction retry timestamp.
    uint32_t retryTimestamp;

    // Specifies the resolved IPv4 or IPv6 address.
    uint8_t resolvedAddr [GMOS_CONFIG_TCPIP_DNS_MAX_ADDR_SIZE];

    // Specify the current DNS table entry state.
    uint8_t dnsEntryState;

    // Specify the DNS retry count.
    uint8_t retryCount;

    // Specify the DNS transaction ID used to resolve the address.
    uint16_t transactionId;

    // Indicate that the cache entry contains an IPv6 address.
#if (GMOS_CONFIG_TCPIP_DNS_SUPPORT_IPV6)
    uint8_t dnsEntryIsIpv6;
#endif

} gmosTcpipDnsCacheEntry_t;

/*
 * Determine if a specified character is a valid ASCII letter.
 */
static inline bool gmosTcpipDnsClientCharIsLetter (const char testChar)
{
    if (((testChar >= 'a') && (testChar <= 'z')) ||
        ((testChar >= 'A') && (testChar <= 'Z'))) {
        return true;
    } else {
        return false;
    }
}

/*
 * Determine if a specified character is a valid label character,
 * including the hyphen.
 */
static inline bool gmosTcpipDnsClientCharIsValid (const char testChar)
{
    if (((testChar >= 'a') && (testChar <= 'z')) ||
        ((testChar >= 'A') && (testChar <= 'Z')) ||
        ((testChar >= '0') && (testChar <= '9')) ||
        (testChar == '-')) {
        return true;
    } else {
        return false;
    }
}

/*
 * Processes a DNS label string, terminated by a null character or
 * period separator. This checks for correct formatting according to
 * RFC1035 section 2.3.1.
 */
static inline uint8_t gmosTcpipDnsClientCheckLabel (
    const char* dnsLabel, bool* dnsLabelError)
{
    const char* nextChar = dnsLabel;
    uint8_t i;

    // Check each label character in turn.
    for (i = 0; i < 64; i++) {

        // The first character must be a letter.
        if (i == 0) {
            if (!gmosTcpipDnsClientCharIsLetter (*nextChar)) {
                goto fail;
            }
        }

        // Check for termination character.
        else if ((*nextChar == '.') || (*nextChar == '\0')) {
            break;
        }

        // Check remaining characters.
        else if (!gmosTcpipDnsClientCharIsValid (*nextChar)) {
            goto fail;
        }
        nextChar ++;
    }

    // Check for invalid label length and final character.
    if ((i >= 64) || (*(nextChar - 1) == '-')) {
        goto fail;
    }
    *dnsLabelError = false;
    return i;

    // Indicate that the label is invalid.
fail :
    *dnsLabelError = true;
    return 0;
}

/*
 * Appends a DNS name string to a GubbinsMOS buffer, converting it from
 * dot separated form into DNS protocol format.
 */
static inline bool gmosTcpipDnsClientFormatName (
    const char* dnsName, gmosBuffer_t* buffer, bool* dnsNameError)
{
    const char* segmentStart;
    size_t residualSize;
    uint8_t segmentSize;

    // Check that the lookup name does not exceed the maximum size
    // according to RFC1035 section 2.3.4 (including the implicit
    // terminating separator).
    residualSize = 1 + strlen (dnsName);
    if ((residualSize == 0) || (residualSize > 255)) {
        *dnsNameError = true;
        return false;
    } else {
        *dnsNameError = false;
    }

    // Convert the DNS lookup name from a conventional string to a
    // series of text segments.
    segmentStart = dnsName;
    while (residualSize > 0) {

        // Check for a valid label segment.
        segmentSize = gmosTcpipDnsClientCheckLabel (
            segmentStart, dnsNameError);
        if (*dnsNameError) {
            goto fail;
        }

        // Append the segment size followed by the segment data to
        // the buffer.
        if ((!gmosBufferAppend (buffer, &segmentSize, 1)) ||
            (!gmosBufferAppend (buffer,
                (uint8_t*) segmentStart, segmentSize))) {
            goto fail;
        }

        // Update pointer to the start of the next segment.
        segmentStart += segmentSize + 1;
        residualSize -= segmentSize + 1;
    }

    // Append the zero length segment as a terminator.
    segmentSize = 0;
    if (!gmosBufferAppend (buffer, &segmentSize, 1)) {
        goto fail;
    }
    return true;

    // Release buffer resource on failure.
fail :
    gmosBufferReset (buffer, 0);
    return false;
}

/*
 * Formats the DNS query message for a fully qualified DNS lookup into
 * a message buffer, using the DNS name stored in the DNS cache buffer.
 */
static inline bool gmosTcpipDnsClientFormatQuery (
    gmosBuffer_t* dnsCacheBuffer,
    gmosTcpipDnsCacheEntry_t* dnsCacheEntry,
    gmosBuffer_t* dnsMessage)
{
    uint16_t dnsSequence;
    uint16_t dnsNameLength;
    uint8_t dnsHeader [12];
    uint8_t dnsFooter [4];
    uint8_t recordType;

    // Select 'A' record type for IPv4 and 'AAAA' record type for IPv6.
    recordType = GMOS_TCPIP_DNS_RECORD_TYPE_A;
#if (GMOS_CONFIG_TCPIP_DNS_SUPPORT_IPV6)
    if (dnsCacheEntry->dnsEntryIsIpv6 != 0) {
        recordType = GMOS_TCPIP_DNS_RECORD_TYPE_AAAA;
    }
#endif

    // Extract the required DNS cache entry parameters.
    dnsSequence = dnsCacheEntry->transactionId;
    dnsNameLength = gmosBufferGetSize (dnsCacheBuffer) -
        sizeof (gmosTcpipDnsCacheEntry_t);

    // Copy the DNS name from the DNS cache buffer to the message
    // buffer.
    if (!gmosBufferCopy (dnsCacheBuffer, dnsMessage)) {
        goto fail;
    }
    gmosBufferRebase (dnsMessage, dnsNameLength);

    // Set the DNS message sequence number using native byte order.
    dnsHeader [0] = ((uint8_t*) &dnsSequence) [0];
    dnsHeader [1] = ((uint8_t*) &dnsSequence) [1];

    // Set the DNS option flags. These are fixed for a standard query
    // with recursion request.
    dnsHeader [2] = 0x01;   // Request recursion.
    dnsHeader [3] = 0x00;   // Response fields left as zero.

    // Specify a single entry in the question section.
    dnsHeader [4] = 0x00;
    dnsHeader [5] = 0x01;

    // The remaining sections have no entries.
    memset (&dnsHeader [6], 0, 6);

    // Add the header to the start of the message buffer.
    if (!gmosBufferPrepend (dnsMessage, dnsHeader, sizeof (dnsHeader))) {
        goto fail;
    }

    // Format the DNS footer, specifying record type and Internet class.
    dnsFooter [0] = 0x00;
    dnsFooter [1] = recordType;
    dnsFooter [2] = 0x00;
    dnsFooter [3] = 0x01;

    // Append the DNS footer.
    if (!gmosBufferAppend (dnsMessage, dnsFooter, sizeof (dnsFooter))) {
        goto fail;
    }
    return true;

    // Release buffer resource on failure.
fail :
    gmosBufferReset (dnsMessage, 0);
    return false;
}

/*
 * Perform a DNS name string match against a cache table entry.
 */
static inline uint8_t gmosTcpipDnsClientCacheMatchString (
    gmosBuffer_t* dnsCacheBuffer, const char* dnsName)
{
    uint8_t* dnsLabel;
    uint8_t cacheLabelSize;
    uint8_t cacheLabelData [64];
    size_t cacheBufferOffset;
    uint8_t matchLength;

    // Attempt to read the size of the first cache label. This will
    // always fail for an empty cache entry.
    cacheBufferOffset = sizeof (gmosTcpipDnsCacheEntry_t);
    if (!gmosBufferRead (dnsCacheBuffer,
        cacheBufferOffset, &cacheLabelSize, 1)) {
        return 0;
    }

    // Ignore malformed entries.
    if ((cacheLabelSize <= 0) || (cacheLabelSize > 63)) {
        return 0;
    }

    // Attempt to match each label in the DNS name. Note that an exact
    // match is used, so case variations are not matched. The match
    // length includes the terminating empty label.
    matchLength = 1;
    dnsLabel = (uint8_t*) dnsName;
    while (true) {

        // Read the next DNS label from the cache buffer, including the
        // next length byte.
        cacheBufferOffset += 1;
        if (!gmosBufferRead (dnsCacheBuffer, cacheBufferOffset,
            cacheLabelData, cacheLabelSize + 1)) {
            matchLength = 0;
            break;
        }

        // Perform an exact match on the DNS label.
        if (memcmp (dnsLabel, cacheLabelData, cacheLabelSize) != 0) {
            matchLength = 0;
            break;
        }

        // Get the next cache label length.
        matchLength += 1 + cacheLabelSize;
        dnsLabel += cacheLabelSize;
        cacheBufferOffset += cacheLabelSize;
        cacheLabelSize = cacheLabelData [cacheLabelSize];

        // Check for expected name termination character.
        if ((cacheLabelSize == 0) && (*dnsLabel == '\0')) {
            break;
        }

        // Detect invalid label termination characters.
        if ((cacheLabelSize > 63) || (*dnsLabel != '.')) {
            matchLength = 0;
            break;
        }
        dnsLabel += 1;
    }
    return matchLength;
}

/*
 * Perform a DNS name buffer match against a cache table entry.
 */
static inline uint8_t gmosTcpipDnsClientCacheMatchBuffer (
    gmosBuffer_t* dnsCacheBuffer, gmosBuffer_t* dnsNameBuffer,
    uint16_t dnsNameBufferOffset)
{
    uint8_t cacheLabelSize;
    uint8_t dnsNameLabelSize;
    uint8_t cacheLabelData [64];
    uint8_t dnsNameLabelData [64];
    size_t cacheBufferOffset;
    uint8_t matchLength;

    // Attempt to read the size of the first cache label. This will
    // always fail for an empty cache entry.
    cacheBufferOffset = sizeof (gmosTcpipDnsCacheEntry_t);
    if (!gmosBufferRead (dnsCacheBuffer,
        cacheBufferOffset, &cacheLabelSize, 1)) {
        return 0;
    }
    if (!gmosBufferRead (dnsNameBuffer,
        dnsNameBufferOffset, &dnsNameLabelSize, 1)) {
        return 0;
    }

    // Ignore malformed or mismatched entries.
    if ((cacheLabelSize != dnsNameLabelSize) ||
        (cacheLabelSize <= 0) || (cacheLabelSize > 63)) {
        return 0;
    }

    // Attempt to match each label in the DNS name. Note that an exact
    // match is used, so case variations are not matched. The match
    // length includes the terminating empty label.
    matchLength = 1;
    while (true) {

        // Read the next DNS label from the cache buffer and the name
        // buffer, including the next length byte.
        cacheBufferOffset += 1;
        if (!gmosBufferRead (dnsCacheBuffer, cacheBufferOffset,
            cacheLabelData, cacheLabelSize + 1)) {
            matchLength = 0;
            break;
        }
        dnsNameBufferOffset += 1;
        if (!gmosBufferRead (dnsNameBuffer, dnsNameBufferOffset,
            dnsNameLabelData, dnsNameLabelSize + 1)) {
            matchLength = 0;
            break;
        }

        // Perform an exact match on the DNS label.
        if (memcmp (dnsNameLabelData,
            cacheLabelData, cacheLabelSize) != 0) {
            matchLength = 0;
            break;
        }

        // Get the next cache label length.
        matchLength += 1 + cacheLabelSize;
        dnsNameBufferOffset += cacheLabelSize;
        cacheBufferOffset += cacheLabelSize;
        cacheLabelSize = cacheLabelData [cacheLabelSize];
        dnsNameLabelSize = dnsNameLabelData [dnsNameLabelSize];

        // Check for expected name termination character.
        if ((cacheLabelSize == 0) && (dnsNameLabelSize == 0)) {
            break;
        }

        // Detect invalid label termination characters.
        if ((cacheLabelSize > 63) ||
            (cacheLabelSize != dnsNameLabelSize)) {
            matchLength = 0;
            break;
        }
    }
    return matchLength;
}

/*
 * Perform a DNS lookup for an existing DNS cache table entry, returning
 * a pointer to the cache table entry buffer or a null reference.
 */
static inline gmosBuffer_t* gmosTcpipDnsClientCacheLookup (
    gmosTcpipDnsClient_t* dnsClient, const char* dnsName,
    bool useIpv6, gmosTcpipDnsCacheEntry_t* dnsCacheEntry)
{
    gmosBuffer_t* dnsCacheBuffer;
    bool cacheHit;
    uint8_t i;

    // Perform a linear search through the cache table.
    cacheHit = false;
    for (i = 0; i < GMOS_CONFIG_TCPIP_DNS_CACHE_SIZE; i++) {
        dnsCacheBuffer = &(dnsClient->dnsCache [i]);
        if (gmosTcpipDnsClientCacheMatchString (
            dnsCacheBuffer, dnsName) > 0) {
            gmosBufferRead (dnsCacheBuffer, 0, (uint8_t*) dnsCacheEntry,
                sizeof (gmosTcpipDnsCacheEntry_t));

            // Differentiate between entries with the same name in the
            // cache for both IPv4 and IPv6 lookups.
            cacheHit = true;
#if (GMOS_CONFIG_TCPIP_DNS_SUPPORT_IPV6)
            if ((useIpv6 && (dnsCacheEntry->dnsEntryIsIpv6 == 0)) ||
                (!useIpv6 && (dnsCacheEntry->dnsEntryIsIpv6 != 0))) {
                cacheHit = false;
            }
#endif
            if (cacheHit) {
                break;
            }
        }
    }

    // On a cache hit for a valid DNS entry, update the DNS cache entry
    // timestamp to indicate that it has been touched.
    if (cacheHit) {
        if (dnsCacheEntry->dnsEntryState ==
            GMOS_TCPIP_DNS_CACHE_ENTRY_STATE_VALID) {
            dnsCacheEntry->expiryTimestamp = gmosPalGetTimer () +
                GMOS_MS_TO_TICKS (1000 * GMOS_CONFIG_TCPIP_DNS_RETENTION_TIME);
            gmosBufferWrite (dnsCacheBuffer, 0, (uint8_t*) dnsCacheEntry,
                sizeof (gmosTcpipDnsCacheEntry_t));
        }
        return dnsCacheBuffer;
    } else {
        return NULL;
    }
}

/*
 * Allocate a new entry in the DNS cache table. If the cache is full,
 * this will discard the least recently used cache entry.
 */
static inline bool gmosTcpipDnsClientCacheAlloc (
    gmosTcpipDnsClient_t* dnsClient, const char* dnsName,
    bool useIpv6, bool* dnsNameError)
{
    gmosTcpipDnsCacheEntry_t dnsCacheEntry;
    gmosBuffer_t* dnsCacheBuffer;
    gmosBuffer_t* localCacheBuffer;
    int32_t lruTimestamp;
    int32_t localTimestamp;
    uint8_t i;
    bool cacheFull;

    // The supplied DNS name is initially assumed to be valid.
    *dnsNameError = false;

    // Perform a linear search for the first free entry.
    cacheFull = true;
    for (i = 0; i < GMOS_CONFIG_TCPIP_DNS_CACHE_SIZE; i++) {
        dnsCacheBuffer = &(dnsClient->dnsCache [i]);
        if (gmosBufferGetSize (dnsCacheBuffer) < sizeof (dnsCacheEntry)) {
            cacheFull = false;
            break;
        }
    }

    // If required, find the least recently used cache entry instead.
    if (cacheFull) {
        for (i = 0; i < GMOS_CONFIG_TCPIP_DNS_CACHE_SIZE; i++) {
            localCacheBuffer = &(dnsClient->dnsCache [i]);
            gmosBufferRead (localCacheBuffer, 0,
                (uint8_t*) &dnsCacheEntry, sizeof (dnsCacheEntry));
            if (i == 0) {
                dnsCacheBuffer = localCacheBuffer;
                lruTimestamp = (int32_t) dnsCacheEntry.expiryTimestamp;
            } else {
                localTimestamp = (int32_t) dnsCacheEntry.expiryTimestamp;
                if (localTimestamp - lruTimestamp < 0) {
                    dnsCacheBuffer = localCacheBuffer;
                    lruTimestamp = localTimestamp;
                }
            }
        }
    }

    // Set the initial cache entry state.
    dnsCacheEntry.expiryTimestamp = gmosPalGetTimer () +
        GMOS_MS_TO_TICKS (1000 * GMOS_CONFIG_TCPIP_DNS_RETENTION_TIME);
    dnsCacheEntry.dnsEntryState = GMOS_TCPIP_DNS_CACHE_ENTRY_STATE_OPEN;
    dnsCacheEntry.retryCount = 0;
    dnsCacheEntry.transactionId = dnsClient->dnsXid;

    // Set the IPv6 request flag if required.
#if (GMOS_CONFIG_TCPIP_DNS_SUPPORT_IPV6)
    dnsCacheEntry.dnsEntryIsIpv6 = useIpv6 ? 1 : 0;
#endif

    // Store the initial cache entry state at the start of the DNS
    // cache buffer.
    gmosBufferReset (dnsCacheBuffer, 0);
    if (!gmosBufferAppend (dnsCacheBuffer, (uint8_t*) &dnsCacheEntry,
        sizeof (dnsCacheEntry))) {
        goto fail;
    }

    // Append the formatted DNS name to the DNS cache buffer.
    if (!gmosTcpipDnsClientFormatName (
        dnsName, dnsCacheBuffer, dnsNameError)) {
        goto fail;
    }

    // Increment the transaction counter for subsequent cache entries.
    dnsClient->dnsXid += 1;
    return true;

    // Clean up on failure to allocate the new DNS cache entry.
fail :
    gmosBufferReset (dnsCacheBuffer, 0);
    return false;
}

/*
 * Selects the DNS server to use for the current retry attempt on a
 * specific cache entry.
 */
static gmosTcpipDnsServerInfo_t* gmosTcpipDnsServerSelect (
    gmosTcpipDnsClient_t* dnsClient,
    gmosTcpipDnsCacheEntry_t* dnsCacheEntry)
{
    uint8_t i;
    gmosTcpipDnsServerInfo_t* serverInfo;

    // Return a null reference if no servers are configured or the
    // retry count has expired.
    if ((dnsClient->dnsServerList == NULL) ||
        (dnsCacheEntry->retryCount > GMOS_CONFIG_TCPIP_DNS_RETRY_COUNT)) {
        return NULL;
    }

    // The retry count is limited, so a simple linear search through
    // the list is adequate.
    serverInfo = dnsClient->dnsServerList;
    for (i = 0; i < dnsCacheEntry->retryCount; i++) {
        serverInfo = serverInfo->nextServer;
        if (serverInfo == NULL) {
            serverInfo = dnsClient->dnsServerList;
        }
    }
    return serverInfo;
}

/*
 * Checks for an active UDP socket or opens one if required.
 */
static inline bool gmosTcpipDnsClientOpenUdp (
    gmosTcpipDnsClient_t* dnsClient, gmosTcpipDnsServerInfo_t* dnsServer)
{
    // Select IPv6 connection if the DNS server has an IPv6 address.
#if (GMOS_CONFIG_TCPIP_DNS_SUPPORT_IPV6)
    if (dnsServer->addressIsIpv6 != 0) {
        if (dnsClient->udpSocketIpv6 == NULL) {
            dnsClient->udpSocketIpv6 = gmosTcpipStackUdpOpen (
                dnsClient->tcpipStack, true, GMOS_TCPIP_DNS_COMMON_PORT,
                &(dnsClient->dnsWorkerTask), NULL, NULL);
        }
        return (dnsClient->udpSocketIpv6 != NULL) ? true : false;
    }
#endif

    // Open the IPv4 socket by default.
    if (dnsClient->udpSocketIpv4 == NULL) {
        dnsClient->udpSocketIpv4 = gmosTcpipStackUdpOpen (
            dnsClient->tcpipStack, false, GMOS_TCPIP_DNS_COMMON_PORT,
            &(dnsClient->dnsWorkerTask), NULL, NULL);
    }
    return (dnsClient->udpSocketIpv4 != NULL) ? true : false;
}

/*
 * Sends a new DNS request to the appropriate DNS server.
 */
static inline bool gmosTcpipDnsClientSendRequest (
    gmosTcpipDnsClient_t* dnsClient, gmosTcpipDnsServerInfo_t* dnsServer,
    gmosBuffer_t* dnsCacheBuffer, gmosTcpipDnsCacheEntry_t* dnsCacheEntry)
{
    gmosBuffer_t dnsMessage = GMOS_BUFFER_INIT ();
    gmosNetworkStatus_t stackStatus;
    uint32_t retryIntervalTicks = GMOS_MS_TO_TICKS (
        1000 * GMOS_CONFIG_TCPIP_DNS_RETRY_INTERVAL);
    gmosTcpipStackSocket_t* udpSocket;

    // Select the IPv4 or IPv6 socket to use for the request.
    udpSocket = dnsClient->udpSocketIpv4;
#if (GMOS_CONFIG_TCPIP_DNS_SUPPORT_IPV6)
    if (dnsServer->addressIsIpv6 != 0) {
        udpSocket = dnsClient->udpSocketIpv6;
    }
#endif

    // Attempt to format the DNS request message.
    if (!gmosTcpipDnsClientFormatQuery (
        dnsCacheBuffer, dnsCacheEntry, &dnsMessage)) {
        goto fail;
    }

    // Send the DNS request message to the selected server.
    stackStatus = gmosTcpipStackUdpSendTo (udpSocket,
        dnsServer->address, GMOS_TCPIP_DNS_COMMON_PORT,
        &dnsMessage);
    if (stackStatus != GMOS_NETWORK_STATUS_SUCCESS) {
        goto fail;
    } else {
        dnsCacheEntry->retryTimestamp =
            gmosPalGetTimer () + retryIntervalTicks;
        return true;
    }

    // Release allocated resources on failure.
fail :
    gmosBufferReset (&dnsMessage, 0);
    return false;
}

/*
 * Checks for DNS retry timeout condition.
 */
static inline gmosTaskStatus_t gmosTcpipDnsClientRetryTimeout (
    gmosTcpipDnsCacheEntry_t* dnsCacheEntry)
{
    // Gets the number of ticks remaining before the retry timeout.
    int32_t remainingTicks =
        (int32_t) (dnsCacheEntry->retryTimestamp - gmosPalGetTimer ());

    // Increment the retry counter on timeout.
    if (remainingTicks <= 0) {
        dnsCacheEntry->retryCount += 1;
        return GMOS_TASK_RUN_IMMEDIATE;
    } else {
        return GMOS_TASK_RUN_LATER ((uint32_t) remainingTicks);
    }
}

/*
 * Checks for DNS cache entry expiry timeout condition.
 */
static inline gmosTaskStatus_t gmosTcpipDnsClientExpiryTimeout (
    gmosTcpipDnsCacheEntry_t* dnsCacheEntry)
{
    // Gets the number of ticks remaining before the retry timeout.
    int32_t remainingTicks =
        (int32_t) (dnsCacheEntry->expiryTimestamp - gmosPalGetTimer ());

    // Increment the retry counter on timeout.
    if (remainingTicks <= 0) {
        return GMOS_TASK_SUSPEND;
    } else {
        return GMOS_TASK_RUN_LATER ((uint32_t) remainingTicks);
    }
}

/*
 * Implement the state machine for processing each DNS cache entry.
 */
static inline gmosTaskStatus_t gmosTcpipDnsClientCacheProcess (
    gmosTcpipDnsClient_t* dnsClient, gmosBuffer_t* dnsCacheBuffer,
    bool* udpKeepOpen)
{
    gmosTcpipDnsCacheEntry_t dnsCacheEntry;
    gmosTcpipDnsServerInfo_t* dnsServer;
    gmosTaskStatus_t taskStatus = GMOS_TASK_SUSPEND;
    uint8_t nextState;

    // Extract the DNS cache entry data. Skips further processing for
    // empty cache entries.
    if (!gmosBufferRead (dnsCacheBuffer, 0,
        (uint8_t*) &dnsCacheEntry, sizeof (dnsCacheEntry))) {
        return GMOS_TASK_SUSPEND;
    }

    // Implement the main DNS cache entry state machine.
    nextState = dnsCacheEntry.dnsEntryState;
    switch (dnsCacheEntry.dnsEntryState) {

        // Check for an active UDP socket or open one if required.
        case GMOS_TCPIP_DNS_CACHE_ENTRY_STATE_OPEN :
            dnsServer = gmosTcpipDnsServerSelect (
                dnsClient, &dnsCacheEntry);
            if (dnsServer == NULL) {
                nextState = GMOS_TCPIP_DNS_CACHE_ENTRY_STATE_NOT_VALID;
            } else if (gmosTcpipDnsClientOpenUdp (dnsClient, dnsServer)) {
                GMOS_LOG (LOG_VERBOSE, "DNS : Opened UDP port.");
                *udpKeepOpen = true;
                nextState = GMOS_TCPIP_DNS_CACHE_ENTRY_STATE_REQUEST;
            }
            taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (10));
            break;

        // Attempt to send a DNS request message and then set the
        // retry request timeout.
        case GMOS_TCPIP_DNS_CACHE_ENTRY_STATE_REQUEST :
            dnsServer = gmosTcpipDnsServerSelect (
                dnsClient, &dnsCacheEntry);
            if (dnsServer == NULL) {
                nextState = GMOS_TCPIP_DNS_CACHE_ENTRY_STATE_NOT_VALID;
            } else if (gmosTcpipDnsClientSendRequest (
                dnsClient, dnsServer, dnsCacheBuffer, &dnsCacheEntry)) {
                GMOS_LOG (LOG_VERBOSE, "DNS : Sent DNS request.");
                nextState = GMOS_TCPIP_DNS_CACHE_ENTRY_STATE_WAIT;
            }
            *udpKeepOpen = true;
            taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (10));
            break;

        // Determine if a DNS request timeout has occurred.
        case GMOS_TCPIP_DNS_CACHE_ENTRY_STATE_WAIT :
            taskStatus = gmosTcpipDnsClientRetryTimeout (&dnsCacheEntry);
            if (taskStatus == GMOS_TASK_RUN_IMMEDIATE) {
                if (dnsCacheEntry.retryCount >
                    GMOS_CONFIG_TCPIP_DNS_RETRY_COUNT) {
                    nextState = GMOS_TCPIP_DNS_CACHE_ENTRY_STATE_TIMEOUT;
                } else {
                    nextState = GMOS_TCPIP_DNS_CACHE_ENTRY_STATE_REQUEST;
                }
            }
            *udpKeepOpen = true;
            break;

        // In all other states check for a DNS cache expiry timeout.
        default :
            taskStatus = gmosTcpipDnsClientExpiryTimeout (&dnsCacheEntry);
            if (taskStatus == GMOS_TASK_SUSPEND) {
                nextState = GMOS_TCPIP_DNS_CACHE_ENTRY_STATE_EXPIRED;
            }
            break;
    }

    // Discard DNS cache entries that have expired, releasing the
    // associated buffer memory.
    if (nextState == GMOS_TCPIP_DNS_CACHE_ENTRY_STATE_EXPIRED) {
        GMOS_LOG (LOG_VERBOSE, "DNS : Cache entry expired.");
        gmosBufferReset (dnsCacheBuffer, 0);
    }

    // Update the DNS cache entry state on a state change.
    else if (nextState != dnsCacheEntry.dnsEntryState) {
        dnsCacheEntry.dnsEntryState = nextState;
        gmosBufferWrite (dnsCacheBuffer, 0,
            (uint8_t*) &dnsCacheEntry, sizeof (dnsCacheEntry));
    }
    return taskStatus;
}

/*
 * Skip over a DNS name in a response message.
 */
static inline uint16_t gmosTcpipDnsClientResponseSkipDnsName (
    gmosBuffer_t* payload, uint16_t payloadOffset)
{
    uint8_t segmentSize;

    // Read the size of each label segment in turn.
    while (true) {
        if (!gmosBufferRead (payload, payloadOffset, &segmentSize, 1)) {
            payloadOffset = 0;
            break;
        }

        // Check for single octet empty label at the end of a DNS name.
        if (segmentSize == 0) {
            payloadOffset += 1;
            break;
        }

        // Check for a two octet pointer at the end of a list of labels.
        else if ((segmentSize & 0xC0) == 0xC0) {
            payloadOffset += 2;
            break;
        }

        // Update the offset for a conventional label.
        else {
            payloadOffset += 1 + segmentSize;
        }
    }
    return payloadOffset;
}

/*
 * Check then remove the common header and query section from the
 * payload, leaving the answer section contents and returning the number
 * of answer section entries.
 */
static inline uint16_t gmosTcpipDnsClientResponseCheckHeader (
    gmosBuffer_t* payloadBuffer, gmosBuffer_t* dnsCacheBuffer,
    gmosTcpipDnsCacheEntry_t* dnsCacheEntry, uint8_t* nextState)
{
    uint8_t dnsHeader [12];
    uint8_t queryData [4];
    uint8_t responseCode;
    uint16_t qdCount;
    uint16_t anCount;
    uint16_t payloadSize;
    uint16_t payloadOffset;
    uint8_t matchSize;
    uint8_t recordType;

    // Select 'A' record type for IPv4 and 'AAAA' record type for IPv6.
    recordType = GMOS_TCPIP_DNS_RECORD_TYPE_A;
#if (GMOS_CONFIG_TCPIP_DNS_SUPPORT_IPV6)
    if (dnsCacheEntry->dnsEntryIsIpv6 != 0) {
        recordType = GMOS_TCPIP_DNS_RECORD_TYPE_AAAA;
    }
#endif

    // Extract the common header fields.
    if (!gmosBufferRead (payloadBuffer, 0,
        dnsHeader, sizeof (dnsHeader))) {
        return 0;
    }

    // Check the first set of DNS option flags. A valid response should
    // have the QR bit set, an opcode of zero and no truncation. The
    // authoritative answer bit is not checked. The recursion desired
    // and available flags should also be set.
    if (((dnsHeader [2] & 0xFB) != 0x81) ||
        ((dnsHeader [3] & 0x80) != 0x80)) {
        return 0;
    }

    // Check the response code. Format errors and name error conditions
    // indicate that the request is not valid. All other error codes
    // are taken to represent an 'unresponsive' server.
    responseCode = dnsHeader [3] & 0x0F;
    if ((responseCode == 1) || (responseCode == 3)) {
        *nextState = GMOS_TCPIP_DNS_CACHE_ENTRY_STATE_NOT_VALID;
    }
    if (responseCode != 0) {
        return 0;
    }

    // Extract the number of question and answer section entries. The
    // authority and additional sections are ignored.
    qdCount = ((uint16_t) (dnsHeader [4])) << 8;
    qdCount += (uint16_t) (dnsHeader [5]);
    anCount = ((uint16_t) (dnsHeader [6])) << 8;
    anCount += (uint16_t) (dnsHeader [7]);

    // RFC1035 section 7.3 recommends that the question section is
    // sanity checked against the original request. There should be a
    // single question field with all matching parameters.
    if (qdCount != 1) {
        *nextState = GMOS_TCPIP_DNS_CACHE_ENTRY_STATE_NOT_VALID;
        return 0;
    }
    payloadSize = gmosBufferGetSize (payloadBuffer);
    payloadOffset = sizeof (dnsHeader);

    // Match the DNS name against the contents of the cache buffer.
    matchSize = gmosTcpipDnsClientCacheMatchBuffer (
        dnsCacheBuffer, payloadBuffer, payloadOffset);
    if (matchSize == 0) {
        *nextState = GMOS_TCPIP_DNS_CACHE_ENTRY_STATE_NOT_VALID;
        return 0;
    }
    payloadOffset += matchSize;

    // Match the expected record type and Internet class fields.
    if (!gmosBufferRead (payloadBuffer, payloadOffset,
        queryData, sizeof (queryData))) {
        return 0;
    }
    if ((queryData [0] != 0x00) || (queryData [1] != recordType) ||
        (queryData [2] != 0x00) || (queryData [3] != 0x01)) {
        return 0;
    }
    payloadOffset += 4;

    // Remove the header and question section from the payload buffer.
    if (payloadOffset >= payloadSize) {
        return 0;
    } else {
        gmosBufferRebase (payloadBuffer, payloadSize - payloadOffset);
        return anCount;
    }
}

/*
 * Scan the answer section records returned by the DNS server for 'A'
 * record IPv4 addresses. These may correspond to the original DNS name
 * or a canonical name record returned by the server.
 */
static inline void gmosTcpipDnsClientResponseScanRecords (
    gmosBuffer_t* payload, gmosTcpipDnsCacheEntry_t* dnsCacheEntry,
    uint16_t anCount, uint8_t* nextState)
{
    uint16_t payloadOffset;
    uint8_t resourceRecord [10];
    uint8_t resolvedAddr [GMOS_CONFIG_TCPIP_DNS_MAX_ADDR_SIZE];
    uint16_t recordDataSize;
    uint16_t aTypeRecordCount;
    uint32_t randValue;
    uint8_t recordType;
    size_t resolvedAddrSize;

    // Select 'A' record type for IPv4 and 'AAAA' record type for IPv6.
    recordType = GMOS_TCPIP_DNS_RECORD_TYPE_A;
    resolvedAddrSize = 4;
#if (GMOS_CONFIG_TCPIP_DNS_SUPPORT_IPV6)
    if (dnsCacheEntry->dnsEntryIsIpv6 != 0) {
        recordType = GMOS_TCPIP_DNS_RECORD_TYPE_AAAA;
        resolvedAddrSize = 16;
    }
#endif

    // Scan each answer record in turn.
    payloadOffset = 0;
    aTypeRecordCount = 0;
    while (anCount > 0) {

        // Skip over the DNS name - this is assumed to be valid and
        // will not be checked.
        payloadOffset = gmosTcpipDnsClientResponseSkipDnsName (
            payload, payloadOffset);
        if (payloadOffset == 0) {
            return;
        }

        // Read the common resource record fields.
        if (!gmosBufferRead (payload, payloadOffset,
            resourceRecord, sizeof (resourceRecord))) {
            return;
        }
        payloadOffset += sizeof (resourceRecord);

        // The DNS records form a 'tree' of canonical name references
        // with 'A' or 'AAAA' records at the leaf nodes. Therefore it is
        // sufficient to just process the Internet class type 'A' or
        // 'AAAA' records. If there are multiple such records, the one
        // to use will be selected at random.
        if ((resourceRecord [0] == 0) &&
            (resourceRecord [1] == recordType) &&
            (resourceRecord [2] == 0) && (resourceRecord [3] == 1)) {
            if (!gmosBufferRead (payload, payloadOffset,
                resolvedAddr, resolvedAddrSize)) {
                return;
            }
            if (recordType == GMOS_TCPIP_DNS_RECORD_TYPE_A) {
                GMOS_LOG_FMT (LOG_VERBOSE,
                    "DNS : Found valid 'A' record : %d.%d.%d.%d",
                    resolvedAddr [0], resolvedAddr [1],
                    resolvedAddr [2], resolvedAddr [3]);
            } else if (GMOS_CONFIG_TCPIP_DNS_SUPPORT_IPV6) {
                GMOS_LOG_FMT (LOG_VERBOSE,
                    "DNS : Found valid 'AAAA' record : "
                    "%02x%02x:%02x%02x:%02x%02x:%02x%02x:"
                    "%02x%02x:%02x%02x:%02x%02x:%02x%02x",
                    resolvedAddr [0], resolvedAddr [1],
                    resolvedAddr [2], resolvedAddr [3],
                    resolvedAddr [4], resolvedAddr [5],
                    resolvedAddr [6], resolvedAddr [7],
                    resolvedAddr [8], resolvedAddr [9],
                    resolvedAddr [10], resolvedAddr [11],
                    resolvedAddr [12], resolvedAddr [13],
                    resolvedAddr [14], resolvedAddr [15]);
            }

            // Randomly replace the existing cache address.
            aTypeRecordCount += 1;
            gmosPalGetRandomBytes (
                (uint8_t*) &randValue, sizeof (randValue));
            randValue = aTypeRecordCount * (randValue & 0xFFFF);
            if (randValue < 0x10000) {
                memcpy (dnsCacheEntry->resolvedAddr,
                    resolvedAddr, resolvedAddrSize);
                *nextState = GMOS_TCPIP_DNS_CACHE_ENTRY_STATE_VALID;
            }
        }

        // Update the payload offset to the start of the next record.
        recordDataSize = ((uint16_t) (resourceRecord [8]) << 8);
        recordDataSize += (uint16_t) (resourceRecord [9]);
        payloadOffset += recordDataSize;
        anCount -= 1;
    }
}

/*
 * Process the next DNS response message if ready.
 */
static gmosTaskStatus_t gmosTcpipDnsClientResponseProcess (
    gmosTcpipDnsClient_t* dnsClient, bool responseIsIpv6)
{
    gmosTcpipDnsCacheEntry_t dnsCacheEntry;
    gmosBuffer_t* dnsCacheBuffer = NULL;
    gmosTcpipStackSocket_t* udpSocket;
    gmosNetworkStatus_t stackStatus;
    uint8_t nextState;
    uint8_t remoteAddr [GMOS_CONFIG_TCPIP_DNS_MAX_ADDR_SIZE];
    uint16_t remotePort;
    uint16_t dnsSequence;
    gmosBuffer_t payload = GMOS_BUFFER_INIT ();
    uint16_t anCount;
    uint8_t i;
    bool cacheHit;

    // Select the UDP socket to use.
    udpSocket = dnsClient->udpSocketIpv4;
#if (GMOS_CONFIG_TCPIP_DNS_SUPPORT_IPV6)
    if (responseIsIpv6) {
        udpSocket = dnsClient->udpSocketIpv6;
    }
#endif

    // Attempt to read the next UDP message from the socket.
    stackStatus = gmosTcpipStackUdpReceiveFrom (udpSocket,
        remoteAddr, &remotePort, &payload);
    if (stackStatus != GMOS_NETWORK_STATUS_SUCCESS) {
        return GMOS_TASK_SUSPEND;
    }

    // Match the response message to a DNS cache entry using the DNS
    // transaction ID. This uses native byte ordering. No check is made
    // on the UDP source address since if an attacker is already
    // generating fake responses this is trivially easy to spoof.
    if (!gmosBufferRead (&payload, 0, (uint8_t*) &dnsSequence, 2)) {
        goto exit;
    }
    cacheHit = false;
    for (i = 0; i < GMOS_CONFIG_TCPIP_DNS_CACHE_SIZE; i++) {
        dnsCacheBuffer = &(dnsClient->dnsCache [i]);
        if (gmosBufferRead (dnsCacheBuffer, 0,
            (uint8_t*) &dnsCacheEntry, sizeof (dnsCacheEntry))) {
            if (dnsCacheEntry.transactionId == dnsSequence) {
                nextState = dnsCacheEntry.dnsEntryState;
                cacheHit = true;
                break;
            }
        }
    }
    if (!cacheHit) {
        dnsCacheBuffer = NULL;
        goto exit;
    }

    // Check the common header and query section from the payload.
    anCount = gmosTcpipDnsClientResponseCheckHeader (
        &payload, dnsCacheBuffer, &dnsCacheEntry, &nextState);
    if (anCount == 0) {
        goto exit;
    }
    if (!responseIsIpv6) {
        GMOS_LOG_FMT (LOG_VERBOSE,
            "DNS : Received valid response from %d.%d.%d.%d:%d",
            remoteAddr [0], remoteAddr [1], remoteAddr [2],
            remoteAddr [3], remotePort);
    } else if (GMOS_CONFIG_TCPIP_DNS_SUPPORT_IPV6) {
        GMOS_LOG_FMT (LOG_VERBOSE,
            "DNS : Received valid response from "
            "[%02x%02x:%02x%02x:%02x%02x:%02x%02x:"
            "%02x%02x:%02x%02x:%02x%02x:%02x%02x]:%d",
            remoteAddr [0], remoteAddr [1], remoteAddr [2],
            remoteAddr [3], remoteAddr [4], remoteAddr [5],
            remoteAddr [6], remoteAddr [7], remoteAddr [8],
            remoteAddr [9], remoteAddr [10], remoteAddr [11],
            remoteAddr [12], remoteAddr [13], remoteAddr [14],
            remoteAddr [15], remotePort);
    }

    // Scan and select an 'A' record from the DNS response.
    gmosTcpipDnsClientResponseScanRecords (
        &payload, &dnsCacheEntry, anCount, &nextState);

    // Implement common exit processing, updating the DNS entry state
    // if required and releasing the contents of the payload buffer.
exit :
    if (dnsCacheBuffer != NULL) {
        if (nextState != dnsCacheEntry.dnsEntryState) {
            dnsCacheEntry.dnsEntryState = nextState;
            gmosBufferWrite (dnsCacheBuffer, 0,
                (uint8_t*) &dnsCacheEntry, sizeof (dnsCacheEntry));
        }
    }
    gmosBufferReset (&payload, 0);
    return GMOS_TASK_RUN_IMMEDIATE;
}

/*
 * Implement the main task loop for DNS client protocol processing.
 */
static gmosTaskStatus_t gmosTcpipDnsClientWorkerTaskFn (void* taskData)
{
    gmosTcpipDnsClient_t* dnsClient = (gmosTcpipDnsClient_t*) taskData;
    gmosBuffer_t* dnsCacheBuffer;
    gmosTaskStatus_t nextStatus;
    gmosTaskStatus_t taskStatus;
    uint8_t i;
    bool udpKeepOpen = false;

    // Ensure that the network is available and that at least one DNS
    // server has a valid configuration before any further processing.
    // If there are no configured servers, the DNS cache is cleared.
    // Suspend the worker task until the next valid query has been
    // initiated, which depends on valid server settings being restored.
    if ((!gmosDriverTcpipPhyLinkIsUp (dnsClient->tcpipStack->tcpipDriver)) ||
        (dnsClient->dnsServerList == NULL)) {
        for (i = 0; i < GMOS_CONFIG_TCPIP_DNS_CACHE_SIZE; i++) {
            dnsCacheBuffer = &(dnsClient->dnsCache [i]);
            gmosBufferReset (dnsCacheBuffer, 0);
        }
        return GMOS_TASK_SUSPEND;
    }

    // Process the next DNS response message on an open IPv4 socket.
    taskStatus = GMOS_TASK_SUSPEND;
    if (dnsClient->udpSocketIpv4 != NULL) {
        nextStatus = gmosTcpipDnsClientResponseProcess (dnsClient, false);
        gmosSchedulerPrioritise (taskStatus, nextStatus);
    }

    // Process the next DNS response message on an open IPv6 socket.
#if (GMOS_CONFIG_TCPIP_DNS_SUPPORT_IPV6)
    if (dnsClient->udpSocketIpv6 != NULL) {
        nextStatus = gmosTcpipDnsClientResponseProcess (dnsClient, true);
        gmosSchedulerPrioritise (taskStatus, nextStatus);
    }
#endif

    // Process each DNS cache entry state machine in turn.
    for (i = 0; i < GMOS_CONFIG_TCPIP_DNS_CACHE_SIZE; i++) {
        dnsCacheBuffer = &(dnsClient->dnsCache [i]);
        nextStatus = gmosTcpipDnsClientCacheProcess (
            dnsClient, dnsCacheBuffer, &udpKeepOpen);
        taskStatus = gmosSchedulerPrioritise (taskStatus, nextStatus);
    }

    // Attempt to close the IPv4 UDP socket if it is no longer required.
    if ((dnsClient->udpSocketIpv4 != NULL) && (!udpKeepOpen)) {
        gmosNetworkStatus_t stackStatus;
        stackStatus = gmosTcpipStackUdpClose (dnsClient->udpSocketIpv4);
        if (stackStatus != GMOS_NETWORK_STATUS_RETRY) {
            dnsClient->udpSocketIpv4 = NULL;
        }
    }

    // Attempt to close the IPv6 UDP socket if it is no longer required.
#if (GMOS_CONFIG_TCPIP_DNS_SUPPORT_IPV6)
    if ((dnsClient->udpSocketIpv6 != NULL) && (!udpKeepOpen)) {
        gmosNetworkStatus_t stackStatus;
        stackStatus = gmosTcpipStackUdpClose (dnsClient->udpSocketIpv6);
        if (stackStatus != GMOS_NETWORK_STATUS_RETRY) {
            dnsClient->udpSocketIpv6 = NULL;
        }
    }
#endif
    return taskStatus;
}

/*
 * Initialise the DNS client on startup, using the specified DHCP client
 * for accessing the TCP/IP interface and DNS server information.
 */
bool gmosTcpipDnsClientInit (gmosTcpipDnsClient_t* dnsClient,
    gmosTcpipStack_t* tcpipStack)
{
    gmosTaskState_t* dnsWorkerTask = &dnsClient->dnsWorkerTask;
    uint16_t randomValue;
    uint8_t i;

    // Initialise the DNS client data structure.
    dnsClient->tcpipStack = tcpipStack;
    dnsClient->dnsServerList = NULL;
    dnsClient->udpSocketIpv4 = NULL;
#if (GMOS_CONFIG_TCPIP_DNS_SUPPORT_IPV6)
    dnsClient->udpSocketIpv6 = NULL;
#endif

    // Select a random transaction ID on startup.
    gmosPalGetRandomBytes ((uint8_t*) &randomValue, sizeof (randomValue));
    dnsClient->dnsXid = randomValue;

    // Initialise the DNS cache buffers.
    for (i = 0; i < GMOS_CONFIG_TCPIP_DNS_CACHE_SIZE; i++) {
        gmosBufferInit (&(dnsClient->dnsCache [i]));
    }

    // Initialise the DNS worker task and schedule it for immediate
    // execution.
    dnsWorkerTask->taskTickFn = gmosTcpipDnsClientWorkerTaskFn;
    dnsWorkerTask->taskData = dnsClient;
    dnsWorkerTask->taskName =
        GMOS_TASK_NAME_WRAPPER ("TCP/IP DNS Client");
    gmosSchedulerTaskStart (dnsWorkerTask);

    return true;
}

/*
 * Adds a new DNS server to the list of available servers.
 */
bool gmosTcpipDnsClientAddServer (gmosTcpipDnsClient_t* dnsClient,
    gmosTcpipDnsServerInfo_t* dnsServerInfo, bool useIpv6,
    uint8_t* serverAddr, uint8_t priority)
{
    size_t serverAddrSize;
    gmosTcpipDnsServerInfo_t** serverInfoPtr;

    // Unlink the existing server data structure if required.
    gmosTcpipDnsClientRemoveServer (dnsClient, dnsServerInfo);

    // Specify sixteen octet IPv6 address if required.
#if (GMOS_CONFIG_TCPIP_DNS_SUPPORT_IPV6)
    if (useIpv6) {
        dnsServerInfo->addressIsIpv6 = 1;
        serverAddrSize = 16;
    } else {
        dnsServerInfo->addressIsIpv6 = 0;
        serverAddrSize = 4;
    }
#else
    if (useIpv6) {
        return false;
    } else {
        serverAddrSize = 4;
    }
#endif

    // Search the DNS server list for the appropriate insertion point.
    serverInfoPtr = &(dnsClient->dnsServerList);
    while (*serverInfoPtr != NULL) {
        if (priority >= (*serverInfoPtr)->priority) {
            break;
        } else {
            serverInfoPtr = &((*serverInfoPtr)->nextServer);
        }
    }

    // Insert the new DNS server information into the list.
    memcpy (dnsServerInfo->address, serverAddr, serverAddrSize);
    dnsServerInfo->priority = priority;
    dnsServerInfo->nextServer = *serverInfoPtr;
    *serverInfoPtr = dnsServerInfo;
    return true;
}

/*
 * Removes a DNS server from the list of available servers.
 */
bool gmosTcpipDnsClientRemoveServer (gmosTcpipDnsClient_t* dnsClient,
    gmosTcpipDnsServerInfo_t* dnsServerInfo)
{
    bool removedOk = false;
    gmosTcpipDnsServerInfo_t** serverInfoPtr;

    // Perform matching on the server information data pointer.
    serverInfoPtr = &(dnsClient->dnsServerList);
    while (*serverInfoPtr != NULL) {
        if (*serverInfoPtr == dnsServerInfo) {
            *serverInfoPtr = (*serverInfoPtr)->nextServer;
            removedOk = true;
            break;
        } else {
            serverInfoPtr = &((*serverInfoPtr)->nextServer);
        }
    }
    return removedOk;
}

/*
 * Performs a DNS query for resolving a given DNS name to an IPv4
 * address.
 */
gmosNetworkStatus_t gmosTcpipDnsClientQuery (
    gmosTcpipDnsClient_t* dnsClient, const char* dnsName,
    bool useIpv6, uint8_t* dnsAddress)
{
    gmosBuffer_t* dnsCacheBuffer;
    gmosTcpipDnsCacheEntry_t dnsCacheEntry;
    bool dnsNameError;
    gmosNetworkStatus_t dnsStatus;
    bool dnsEntryIsIpv6 = false;
    size_t dnsAddressSize = 4;

    // Check for IPv6 support.
    if (useIpv6 && !GMOS_CONFIG_TCPIP_DNS_SUPPORT_IPV6) {
        return GMOS_NETWORK_STATUS_UNSUPPORTED;
    }

    // Ensure that the network is available and that at least one DNS
    // server has been configured.
    if ((!gmosDriverTcpipPhyLinkIsUp (dnsClient->tcpipStack->tcpipDriver)) ||
        (dnsClient->dnsServerList == NULL)) {
        return GMOS_NETWORK_STATUS_NETWORK_DOWN;
    }

    // Perform local cache lookup.
    dnsCacheBuffer = gmosTcpipDnsClientCacheLookup (
        dnsClient, dnsName, useIpv6, &dnsCacheEntry);

    // Process an existing DNS cache entry.
    if (dnsCacheBuffer != NULL) {
        GMOS_LOG_FMT (LOG_VERBOSE,
            "DNS : Cache entry 0x%04X hit state : %d.",
            dnsCacheEntry.transactionId, dnsCacheEntry.dnsEntryState);

        // Extract IPv6 flag setting if required.
#if (GMOS_CONFIG_TCPIP_DNS_SUPPORT_IPV6)
        if (dnsCacheEntry.dnsEntryIsIpv6 != 0) {
            dnsEntryIsIpv6 = true;
            dnsAddressSize = 16;
        }
#endif

        // Select the apprioriate status response.
        if (useIpv6 != dnsEntryIsIpv6) {
            dnsStatus = GMOS_NETWORK_STATUS_NOT_VALID;
        } else switch (dnsCacheEntry.dnsEntryState) {
            case GMOS_TCPIP_DNS_CACHE_ENTRY_STATE_VALID :
                memcpy (dnsAddress,
                    &(dnsCacheEntry.resolvedAddr), dnsAddressSize);
                dnsStatus = GMOS_NETWORK_STATUS_SUCCESS;
                break;
            case GMOS_TCPIP_DNS_CACHE_ENTRY_STATE_TIMEOUT :
                dnsStatus = GMOS_NETWORK_STATUS_TIMEOUT;
                break;
            case GMOS_TCPIP_DNS_CACHE_ENTRY_STATE_NOT_VALID :
                dnsStatus = GMOS_NETWORK_STATUS_NOT_VALID;
                break;
            default :
                dnsStatus = GMOS_NETWORK_STATUS_RETRY;
                break;
        }
    }

    // Attempt to allocate a new DNS cache entry and initiate the
    // DNS request process.
    else if (gmosTcpipDnsClientCacheAlloc (
        dnsClient, dnsName, useIpv6, &dnsNameError)) {
        gmosSchedulerTaskResume (&(dnsClient->dnsWorkerTask));
        dnsStatus = GMOS_NETWORK_STATUS_RETRY;
    } else if (dnsNameError) {
        dnsStatus = GMOS_NETWORK_STATUS_NOT_VALID;
    } else {
        dnsStatus = GMOS_NETWORK_STATUS_RETRY;
    }
    return dnsStatus;
}
