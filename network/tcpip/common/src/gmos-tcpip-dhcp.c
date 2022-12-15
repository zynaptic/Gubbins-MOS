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
 * This file provides a common IPv4 DHCP client implementation for use
 * with vendor supplied and hardware accelerated TCP/IP implementations.
 * Note that all DHCP transactions directly access the TCP/IP driver
 * layer, since they need to complete before the TCP/IP stack is fully
 * set up.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-scheduler.h"
#include "gmos-network.h"
#include "gmos-driver-tcpip.h"
#include "gmos-tcpip-config.h"
#include "gmos-tcpip-dhcp.h"

/*
 * Specify the standard DHCP ports for local use.
 */
#define GMOS_TCPIP_DHCP_SERVER_PORT 67
#define GMOS_TCPIP_DHCP_CLIENT_PORT 68
#define GMOS_TCPIP_DISCARD_SERVER_PORT 9

/*
 * Specify the length of the discovery window as an integer number of
 * seconds. This is an implementation specific parameter.
 */
#define GMOS_TCPIP_DHCP_DISCOVERY_WINDOW 12

/*
 * Specify the length of the response waiting window as an integer
 * number of seconds. This is an implementation specific parameter.
 */
#define GMOS_TCPIP_DHCP_RESPONSE_WINDOW 8

/*
 * Specify the minimal interval at which the DHCP client will attempt to
 * retry a lease request after failing to obtain or renew a prior
 * request, expressed as an integer number of seconds. This is the
 * recommended minimum interval from RFC2131 section 4.4.5.
 */
#define GMOS_TCPIP_DHCP_MIN_RETRY_INTERVAL 60

/*
 * Specify the minimal interval at which the DHCP client will attempt
 * to obtain a lease when restarting after failing to obtain or renew
 * a lease. This is an implementation specific parameter.
 */
#define GMOS_TCPIP_DHCP_MIN_RESTART_INTERVAL 150

/*
 * Specify the state space to be used for the DHCP client.
 */
typedef enum {
    GMOS_TCPIP_DHCP_CLIENT_STATE_UNCONNECTED,
    GMOS_TCPIP_DHCP_CLIENT_STATE_RESTARTING,
    GMOS_TCPIP_DHCP_CLIENT_STATE_SET_DEFAULT_ADDR,
    GMOS_TCPIP_DHCP_CLIENT_STATE_DISCOVERY_OPEN,
    GMOS_TCPIP_DHCP_CLIENT_STATE_DISCOVERY_INIT,
    GMOS_TCPIP_DHCP_CLIENT_STATE_SELECTING_WAIT,
    GMOS_TCPIP_DHCP_CLIENT_STATE_SELECTING_DONE,
    GMOS_TCPIP_DHCP_CLIENT_STATE_REQUESTING_WAIT,
    GMOS_TCPIP_DHCP_CLIENT_STATE_ADDR_CHECK_SEND,
    GMOS_TCPIP_DHCP_CLIENT_STATE_ADDR_CHECK_WAIT,
    GMOS_TCPIP_DHCP_CLIENT_STATE_REQUESTING_DECLINE,
    GMOS_TCPIP_DHCP_CLIENT_STATE_REQUESTING_SUCCESS,
    GMOS_TCPIP_DHCP_CLIENT_STATE_SET_ASSIGNED_ADDR,
    GMOS_TCPIP_DHCP_CLIENT_STATE_BOUND,
    GMOS_TCPIP_DHCP_CLIENT_STATE_RENEWAL_OPEN,
    GMOS_TCPIP_DHCP_CLIENT_STATE_RENEWAL_INIT,
    GMOS_TCPIP_DHCP_CLIENT_STATE_RENEWAL_WAIT,
    GMOS_TCPIP_DHCP_CLIENT_STATE_RENEWAL_DONE,
} gmosTcpipDhcpClientState_t;

/*
 * Specify the subset of supported DHCP options.
 */
typedef enum {
    GMOS_TCPIP_DHCP_MESSAGE_OPTION_PADDING         = 0,
    GMOS_TCPIP_DHCP_MESSAGE_OPTION_SUBNET_MASK     = 1,
    GMOS_TCPIP_DHCP_MESSAGE_OPTION_GATEWAY_ROUTERS = 3,
    GMOS_TCPIP_DHCP_MESSAGE_OPTION_DNS_SERVERS     = 6,
    GMOS_TCPIP_DHCP_MESSAGE_OPTION_HOST_NAME       = 12,
    GMOS_TCPIP_DHCP_MESSAGE_OPTION_REQUESTED_IP    = 50,
    GMOS_TCPIP_DHCP_MESSAGE_OPTION_LEASE_TIME      = 51,
    GMOS_TCPIP_DHCP_MESSAGE_OPTION_OVERLOAD_FIELDS = 52,
    GMOS_TCPIP_DHCP_MESSAGE_OPTION_MESSAGE_TYPE    = 53,
    GMOS_TCPIP_DHCP_MESSAGE_OPTION_SERVER_ID       = 54,
    GMOS_TCPIP_DHCP_MESSAGE_OPTION_PARAM_REQ_LIST  = 55,
    GMOS_TCPIP_DHCP_MESSAGE_OPTION_RENEWAL_TIME    = 58,
    GMOS_TCPIP_DHCP_MESSAGE_OPTION_REBINDING_TIME  = 59,
    GMOS_TCPIP_DHCP_MESSAGE_OPTION_CLIENT_ID       = 61,
    GMOS_TCPIP_DHCP_MESSAGE_OPTION_LIST_END        = 255
} gmosTcpipDhcpMessageOptions_t;

/*
 * Specify the DHCP message type values.
 */
typedef enum {
    GMOS_TCPIP_DHCP_MESSAGE_TYPE_INVALID  = 0,
    GMOS_TCPIP_DHCP_MESSAGE_TYPE_DISCOVER = 1,
    GMOS_TCPIP_DHCP_MESSAGE_TYPE_OFFER    = 2,
    GMOS_TCPIP_DHCP_MESSAGE_TYPE_REQUEST  = 3,
    GMOS_TCPIP_DHCP_MESSAGE_TYPE_DECLINE  = 4,
    GMOS_TCPIP_DHCP_MESSAGE_TYPE_ACK      = 5,
    GMOS_TCPIP_DHCP_MESSAGE_TYPE_NAK      = 6,
    GMOS_TCPIP_DHCP_MESSAGE_TYPE_RELEASE  = 7
} gmosTcpipDhcpMessageTypes_t;

/*
 * Specify the DHCP option parsing flags. These are used to indicate
 * which options were parsed in a given DHCP message.
 */
typedef enum {
    GMOS_TCPIP_DHCP_MESSAGE_OPTION_FLAG_MESSAGE_TYPE    = 0x01,
    GMOS_TCPIP_DHCP_MESSAGE_OPTION_FLAG_OVERLOAD_FIELDS = 0x02,
    GMOS_TCPIP_DHCP_MESSAGE_OPTION_FLAG_GATEWAY_ROUTERS = 0x04,
    GMOS_TCPIP_DHCP_MESSAGE_OPTION_FLAG_DNS1_SERVER     = 0x08,
    GMOS_TCPIP_DHCP_MESSAGE_OPTION_FLAG_DNS2_SERVER     = 0x10,
    GMOS_TCPIP_DHCP_MESSAGE_OPTION_FLAG_SERVER_ID       = 0x20,
    GMOS_TCPIP_DHCP_MESSAGE_OPTION_FLAG_LEASE_TIME      = 0x40,
    GMOS_TCPIP_DHCP_MESSAGE_OPTION_FLAG_SUBNET_MASK     = 0x80,
} gmosTcpipDhcpMessageOptionFlags_t;

/*
 * Define the various parameters that may be included in a received DHCP
 * message. This is the subset of the available DHCP message fields that
 * are used in this implementation.
 */
typedef struct gmosTcpipDhcpRxMessage_t {
    uint32_t leaseTime;
    uint32_t assignedAddr;
    uint32_t gatewayAddr;
    uint32_t dhcpServerAddr;
    uint32_t dns1ServerAddr;
    uint32_t dns2ServerAddr;
    uint32_t subnetMask;
    uint8_t messageType;
    uint8_t optOverload;
    uint8_t optValidFlags;
} gmosTcpipDhcpRxMessage_t;

/*
 * Specify the common IPv4 broadcast address.
 */
static uint8_t gmosTcpipBroadcastAddr [] = { 255, 255, 255, 255 };

/*
 * Specify the common IPv4 all zeroes address.
 */
static uint8_t gmosTcpipAllZeroAddr [] = { 0, 0, 0, 0 };

/*
 * Parses a received DHCP message options segment.
 */
static inline bool gmosTcpipDhcpClientParseRxMessageOptions (
    gmosTcpipDhcpClient_t* dhcpClient, gmosBuffer_t* rxBuffer,
    gmosTcpipDhcpRxMessage_t* rxMessage, uint16_t optOffset,
    uint16_t optLimit)
{
    uint8_t optId;
    uint8_t optSize;
    uint32_t optValueU32;
    uint8_t optValidMask;

    // Loop over all options in the option segment.
    while (optOffset < optLimit) {

        // Read the option ID and process basic tags.
        gmosBufferRead (rxBuffer, optOffset, &optId, 1);
        optOffset += 1;
        if (optId == GMOS_TCPIP_DHCP_MESSAGE_OPTION_LIST_END) {
            return true;
        } else if (optId == GMOS_TCPIP_DHCP_MESSAGE_OPTION_PADDING) {
            continue;
        }

        // Check for a valid option length that does not exceed the
        // option range.
        if ((!gmosBufferRead (rxBuffer, optOffset, &optSize, 1)) ||
            (optOffset + optSize + 1 >= optLimit)) {
            return false;
        }
        optOffset += 1;

        // Only process recognised options with the expected length.
        switch (optId) {

            // Support option overloading of 'file' and 'sname' fields.
            case GMOS_TCPIP_DHCP_MESSAGE_OPTION_OVERLOAD_FIELDS :
                optValidMask =
                    GMOS_TCPIP_DHCP_MESSAGE_OPTION_FLAG_OVERLOAD_FIELDS;
                if ((optSize == 1) &&
                    ((rxMessage->optValidFlags & optValidMask) == 0)) {
                    gmosBufferRead (rxBuffer,
                        optOffset, &(rxMessage->optOverload), 1);
                    rxMessage->optValidFlags |= optValidMask;
                }
                break;

            // Read the DHCP message type.
            case GMOS_TCPIP_DHCP_MESSAGE_OPTION_MESSAGE_TYPE :
                optValidMask =
                    GMOS_TCPIP_DHCP_MESSAGE_OPTION_FLAG_MESSAGE_TYPE;
                if ((optSize == 1) &&
                    ((rxMessage->optValidFlags & optValidMask) == 0)) {
                    gmosBufferRead (rxBuffer,
                        optOffset, &(rxMessage->messageType), 1);
                    rxMessage->optValidFlags |= optValidMask;
                }
                break;

            // Read the DHCP lease time, converting to host byte order.
            case GMOS_TCPIP_DHCP_MESSAGE_OPTION_LEASE_TIME :
                optValidMask =
                    GMOS_TCPIP_DHCP_MESSAGE_OPTION_FLAG_LEASE_TIME;
                if ((optSize == 4) &&
                    ((rxMessage->optValidFlags & optValidMask) == 0)) {
                    gmosBufferRead (rxBuffer, optOffset,
                        (uint8_t*) &optValueU32, 4);
                    rxMessage->leaseTime =
                        GMOS_TCPIP_STACK_NTOHL (optValueU32);
                    rxMessage->optValidFlags |= optValidMask;
                }
                break;

            // Read the first entry in the gateway router list.
            case GMOS_TCPIP_DHCP_MESSAGE_OPTION_GATEWAY_ROUTERS :
                optValidMask =
                    GMOS_TCPIP_DHCP_MESSAGE_OPTION_FLAG_GATEWAY_ROUTERS;
                if ((optSize >= 4) &&
                    ((rxMessage->optValidFlags & optValidMask) == 0)) {
                    gmosBufferRead (rxBuffer, optOffset,
                        (uint8_t*) &rxMessage->gatewayAddr, 4);
                    rxMessage->optValidFlags |= optValidMask;
                }
                break;

            // Read the DHCP server address.
            case GMOS_TCPIP_DHCP_MESSAGE_OPTION_SERVER_ID :
                optValidMask =
                    GMOS_TCPIP_DHCP_MESSAGE_OPTION_FLAG_SERVER_ID;
                if ((optSize == 4) &&
                    ((rxMessage->optValidFlags & optValidMask) == 0)) {
                    gmosBufferRead (rxBuffer, optOffset,
                        (uint8_t*) &rxMessage->dhcpServerAddr, 4);
                    rxMessage->optValidFlags |= optValidMask;
                }
                break;

            // Read the subnet mask setting.
            case GMOS_TCPIP_DHCP_MESSAGE_OPTION_SUBNET_MASK :
                optValidMask =
                    GMOS_TCPIP_DHCP_MESSAGE_OPTION_FLAG_SUBNET_MASK;
                if ((optSize == 4) &&
                    ((rxMessage->optValidFlags & optValidMask) == 0)) {
                    gmosBufferRead (rxBuffer, optOffset,
                        (uint8_t*) &rxMessage->subnetMask, 4);
                    rxMessage->optValidFlags |= optValidMask;
                }
                break;

            // Read the first two entries in the DNS server list. The
            // list may be contained in a single option entry or use
            // multiple option entries.
            case GMOS_TCPIP_DHCP_MESSAGE_OPTION_DNS_SERVERS :
                if ((rxMessage->optValidFlags &
                    GMOS_TCPIP_DHCP_MESSAGE_OPTION_FLAG_DNS1_SERVER) == 0) {
                    if (optSize >= 4) {
                        gmosBufferRead (rxBuffer, optOffset,
                            (uint8_t*) &rxMessage->dns1ServerAddr, 4);
                        rxMessage->optValidFlags |=
                            GMOS_TCPIP_DHCP_MESSAGE_OPTION_FLAG_DNS1_SERVER;
                    }
                    if (optSize >= 8) {
                        gmosBufferRead (rxBuffer, optOffset + 4,
                            (uint8_t*) &rxMessage->dns2ServerAddr, 4);
                        rxMessage->optValidFlags |=
                            GMOS_TCPIP_DHCP_MESSAGE_OPTION_FLAG_DNS2_SERVER;
                    }
                } else if ((rxMessage->optValidFlags &
                    GMOS_TCPIP_DHCP_MESSAGE_OPTION_FLAG_DNS2_SERVER) == 0) {
                    if (optSize >= 4) {
                        gmosBufferRead (rxBuffer, optOffset,
                            (uint8_t*) &rxMessage->dns2ServerAddr, 4);
                        rxMessage->optValidFlags |=
                            GMOS_TCPIP_DHCP_MESSAGE_OPTION_FLAG_DNS2_SERVER;
                    }
                }
                break;
        }

        // Set the offset to the next option ID.
        optOffset += optSize;
    }
    return false;
}

/*
 * Parses a received DHCP message for the supported subset of fields.
 */
static bool gmosTcpipDhcpClientParseRxMessage (
    gmosTcpipDhcpClient_t* dhcpClient, gmosBuffer_t* rxBuffer,
    gmosTcpipDhcpRxMessage_t* rxMessage)
{
    gmosDriverTcpip_t* tcpipDriver = dhcpClient->tcpipDriver;
    uint16_t rxLength = gmosBufferGetSize (rxBuffer);
    uint8_t* ethMacAddr;
    uint8_t rxData [6];
    uint32_t rxDataU32;
    uint32_t expectedValue;
    uint16_t optOffset;
    uint16_t optLimit;
    uint8_t optSegment;

    // Set the optional fields to their default values.
    rxMessage->messageType = GMOS_TCPIP_DHCP_MESSAGE_TYPE_INVALID;
    rxMessage->optOverload = 0;
    rxMessage->optValidFlags = 0;

    // Check that that the message is at least long enough to hold the
    // standard header and the DHCP options list marker (240 octets).
    if (rxLength < 240) {
        return false;
    }

    // Check that the message is marked as a 'boot reply' with an
    // Ethernet hardware type and zero hops.
    expectedValue = GMOS_TCPIP_STACK_HTONL (0x02010600);
    gmosBufferRead (rxBuffer, 0, (uint8_t*) &rxDataU32, 4);
    if (rxDataU32 != expectedValue) {
        return false;
    }

    // Check for a valid options header magic number.
    expectedValue = GMOS_TCPIP_STACK_HTONL (0x63825363);
    gmosBufferRead (rxBuffer, 236, (uint8_t*) &rxDataU32, 4);
    if (rxDataU32 != expectedValue) {
        return false;
    }

    // Check for matching 'xid' field. This uses native byte order.
    expectedValue = dhcpClient->dhcpXid;
    gmosBufferRead (rxBuffer, 4, (uint8_t*) &rxDataU32, 4);
    if (rxDataU32 != expectedValue) {
        return false;
    }

    // Check for matching 'chaddr' field.
    ethMacAddr = gmosDriverTcpipGetMacAddr (tcpipDriver);
    gmosBufferRead (rxBuffer, 28, rxData, 6);
    if (memcmp (rxData, ethMacAddr, 6) != 0) {
        return false;
    }

    // Read the common header fields.
    gmosBufferRead (rxBuffer, 16, (uint8_t*) &rxMessage->assignedAddr, 4);

    // Process the three potential options segments in turn.
    for (optSegment = 0; optSegment < 3; optSegment++) {

        // Process the standard option extension.
        if (optSegment == 0) {
            optOffset = 240;
            optLimit = rxLength;
        }

        // Process the 'file' field options if required.
        else if ((optSegment == 1) &&
            ((rxMessage->optOverload & 1) != 0)) {
            optOffset = 108;
            optLimit = 236;
        }

        // Process the 'sname' field options if required.
        else if ((optSegment == 2) &&
            ((rxMessage->optOverload & 2) != 0)) {
            optOffset = 44;
            optLimit = 108;
        }

        // Skip unused option segments.
        else {
            optOffset = 0;
            optLimit = 0;
        }

        // Process the selected option segment.
        if (optOffset != 0) {
            if (!gmosTcpipDhcpClientParseRxMessageOptions (
                dhcpClient, rxBuffer, rxMessage, optOffset, optLimit)) {
                return false;
            }
        }
    }
    return true;
}

/*
 * Formats an option portion of a DHCP message.
 */
static bool gmosTcpipDhcpClientFormatOption (
    gmosBuffer_t* message, uint8_t optionId,
    uint8_t* optionData, uint8_t optionLength)
{
    uint8_t optionHeader [2];

    // Append the option ID and length to the message.
    optionHeader [0] = optionId;
    optionHeader [1] = optionLength;
    if (!gmosBufferAppend (message, optionHeader, 2)) {
        goto fail;
    }

    // Append the option value to the message.
    if (optionLength > 0) {
        if (!gmosBufferAppend (message, optionData, optionLength)) {
            goto fail;
        }
    }
    return true;

    // Release allocated buffer memory on failure.
fail:
    gmosBufferReset (message, 0);
    return false;
}

/*
 * Formats the header portion of a DHCP message, including the common
 * option fields.
 */
static bool gmosTcpipDhcpClientFormatHeader (
    gmosTcpipDhcpClient_t* dhcpClient, gmosBuffer_t* message,
    uint8_t messageType, bool broadcastReply, uint32_t ciaddr)
{
    gmosDriverTcpip_t* tcpipDriver = dhcpClient->tcpipDriver;
    uint32_t headerData [4];
    uint8_t zeroesData [64];
    uint8_t* ethMacAddr;
    uint8_t optionData [7];

    // Use a zero valued array for setting unused fields.
    memset (zeroesData, 0, 64);
    ethMacAddr = gmosDriverTcpipGetMacAddr (tcpipDriver);

    // Set up the common header fields.
    headerData [0] = GMOS_TCPIP_STACK_HTONL (0x01010600);

    // Set the current 'xid' value. Since this is an arbitrary token,
    // native byte order can be used.
    headerData [1] = dhcpClient->dhcpXid;

    // The optional seconds field is not used in this implementation.
    // Set flags depending on whether a broadcast reply is required.
    if (broadcastReply) {
        headerData [2] = GMOS_TCPIP_STACK_HTONL (0x00008000);
    } else {
        headerData [2] = GMOS_TCPIP_STACK_HTONL (0x00000000);
    }

    // Use the current client address if known. This should already be
    // in network byte order.
    headerData [3] = ciaddr;

    // Append the common header fields to the buffer.
    if (!gmosBufferAppend (message, (uint8_t*) headerData, 16)) {
        goto fail;
    }

    // The client always sets 'yiaddr', 'siaddr' and 'giaddr' to zero.
    if (!gmosBufferAppend (message, zeroesData, 12)) {
        goto fail;
    }

    // The first six octets of the 'chaddr' field are set to the
    // Ethernet MAC address, and the remaining ten octets are set to
    // zero.
    if ((!gmosBufferAppend (message, ethMacAddr, 6)) ||
        (!gmosBufferAppend (message, zeroesData, 10))) {
        goto fail;
    }

    // The 'sname' and 'file' fields are always set to zero. These
    // correspond to 192 zero octets in total.
    if ((!gmosBufferAppend (message, zeroesData, 64)) ||
        (!gmosBufferAppend (message, zeroesData, 64)) ||
        (!gmosBufferAppend (message, zeroesData, 64))) {
        goto fail;
    }

    // Append the 'magic cookie' values which mark the start of the
    // options list.
    headerData [0] = GMOS_TCPIP_STACK_HTONL (0x63825363);
    if (!gmosBufferAppend (message, (uint8_t*) headerData, 4)) {
        goto fail;
    }

    // Append the option for the DHCP message type.
    if (!gmosTcpipDhcpClientFormatOption (message,
        GMOS_TCPIP_DHCP_MESSAGE_OPTION_MESSAGE_TYPE, &messageType, 1)) {
        return false;
    }

    // Specify the client identifier as the Ethernet MAC address.
    optionData [0] = 1;         // Set 'htype' to 1 for Ethernet.
    memcpy (&(optionData [1]), ethMacAddr, 6);
    if (!gmosTcpipDhcpClientFormatOption (message,
        GMOS_TCPIP_DHCP_MESSAGE_OPTION_CLIENT_ID, optionData, 7)) {
        return false;
    }

    // Specify the host name.
    if (!gmosTcpipDhcpClientFormatOption (message,
        GMOS_TCPIP_DHCP_MESSAGE_OPTION_HOST_NAME,
        (uint8_t*) dhcpClient->dhcpHostName,
        strlen (dhcpClient->dhcpHostName))) {
        return false;
    }
    return true;

    // Release allocated buffer memory on failure.
fail:
    gmosBufferReset (message, 0);
    return false;
}

/*
 * Formats the DHCP discover message into the specified message buffer.
 */
static inline bool gmosTcpipDhcpClientFormatDhcpDiscover (
    gmosTcpipDhcpClient_t* dhcpClient, gmosBuffer_t* message)
{
    uint8_t optionData [3];

    // Format the common message header. For the discover message the
    // 'ciaddr' field is set to all zeros.
    if (!gmosTcpipDhcpClientFormatHeader (dhcpClient, message,
        GMOS_TCPIP_DHCP_MESSAGE_TYPE_DISCOVER, true, 0)) {
        return false;
    }

    // Set the requested parameter list.
    optionData [0] = GMOS_TCPIP_DHCP_MESSAGE_OPTION_SUBNET_MASK;
    optionData [1] = GMOS_TCPIP_DHCP_MESSAGE_OPTION_GATEWAY_ROUTERS;
    optionData [2] = GMOS_TCPIP_DHCP_MESSAGE_OPTION_DNS_SERVERS;
    if (!gmosTcpipDhcpClientFormatOption (message,
        GMOS_TCPIP_DHCP_MESSAGE_OPTION_PARAM_REQ_LIST, optionData, 3)) {
        return false;
    }

    // Append the end of options list flag. This does not include any
    // trailing data.
    if (!gmosTcpipDhcpClientFormatOption (message,
        GMOS_TCPIP_DHCP_MESSAGE_OPTION_LIST_END, NULL, 0)) {
        return false;
    }
    return true;
}

/*
 * Formats the DHCP request message into the specified message buffer.
 */
static bool gmosTcpipDhcpClientFormatDhcpRequest (
    gmosTcpipDhcpClient_t* dhcpClient, gmosBuffer_t* message,
    bool broadcastReply, bool includeClientAddr,
    bool includeRequestAddr, bool includeServerAddr)
{
    uint32_t ciaddr;

    // The client address is only included when rebinding.
    if (includeClientAddr) {
        ciaddr = dhcpClient->assignedAddr;
    } else {
        ciaddr = 0;
    }

    // Format the common message header.
    if (!gmosTcpipDhcpClientFormatHeader (dhcpClient, message,
        GMOS_TCPIP_DHCP_MESSAGE_TYPE_REQUEST, broadcastReply, ciaddr)) {
        return false;
    }

    // Include the requested IP address if required.
    if (includeRequestAddr) {
        if (!gmosTcpipDhcpClientFormatOption (message,
            GMOS_TCPIP_DHCP_MESSAGE_OPTION_REQUESTED_IP,
            (uint8_t*) &dhcpClient->assignedAddr, 4)) {
            return false;
        }
    }

    // Include the selected DHCP server address if required.
    if (includeServerAddr) {
        if (!gmosTcpipDhcpClientFormatOption (message,
            GMOS_TCPIP_DHCP_MESSAGE_OPTION_SERVER_ID,
            (uint8_t*) &dhcpClient->dhcpServerAddr, 4)) {
            return false;
        }
    }

    // Append the end of options list flag. This does not include any
    // trailing data.
    if (!gmosTcpipDhcpClientFormatOption (message,
        GMOS_TCPIP_DHCP_MESSAGE_OPTION_LIST_END, NULL, 0)) {
        return false;
    }
    return true;
}

/*
 * Formats the DHCP decline message in the specified buffer.
 */
static inline bool gmosTcpipDhcpClientFormatDhcpDecline (
    gmosTcpipDhcpClient_t* dhcpClient, gmosBuffer_t* message)
{
    // Format the common message header. For the discover message the
    // 'ciaddr' field is set to all zeros.
    if (!gmosTcpipDhcpClientFormatHeader (dhcpClient, message,
        GMOS_TCPIP_DHCP_MESSAGE_TYPE_DECLINE, true, 0)) {
        return false;
    }

    // Always include the requested IP address.
    if (!gmosTcpipDhcpClientFormatOption (message,
        GMOS_TCPIP_DHCP_MESSAGE_OPTION_REQUESTED_IP,
        (uint8_t*) &dhcpClient->assignedAddr, 4)) {
        return false;
    }

    // Always include the selected DHCP server address.
    if (!gmosTcpipDhcpClientFormatOption (message,
        GMOS_TCPIP_DHCP_MESSAGE_OPTION_SERVER_ID,
        (uint8_t*) &dhcpClient->dhcpServerAddr, 4)) {
        return false;
    }

    // Append the end of options list flag. This does not include any
    // trailing data.
    if (!gmosTcpipDhcpClientFormatOption (message,
        GMOS_TCPIP_DHCP_MESSAGE_OPTION_LIST_END, NULL, 0)) {
        return false;
    }
    return true;
}

/*
 * Parses the DHCP offer message from the specified message buffer,
 * updating the DHCP client settings as required.
 */
static inline void gmosTcpipDhcpClientParseDhcpOffer (
    gmosTcpipDhcpClient_t* dhcpClient, gmosBuffer_t* rxBuffer)
{
    gmosTcpipDhcpRxMessage_t rxMessage;
    uint8_t requiredType;
    uint8_t requiredOpts;

    // Parse the received DHCP message.
    if (!gmosTcpipDhcpClientParseRxMessage (
        dhcpClient, rxBuffer, &rxMessage)) {
        return;
    }

    // Determine if the parsed message has the required type and the
    // required options.
    requiredType = GMOS_TCPIP_DHCP_MESSAGE_TYPE_OFFER;
    requiredOpts =
        GMOS_TCPIP_DHCP_MESSAGE_OPTION_FLAG_LEASE_TIME |
        GMOS_TCPIP_DHCP_MESSAGE_OPTION_FLAG_SERVER_ID |
        GMOS_TCPIP_DHCP_MESSAGE_OPTION_FLAG_SUBNET_MASK |
        GMOS_TCPIP_DHCP_MESSAGE_OPTION_FLAG_GATEWAY_ROUTERS;
    if ((rxMessage.messageType != requiredType) ||
        ((rxMessage.optValidFlags & requiredOpts) != requiredOpts)) {
        return;
    }

    // In most cases there will only be a single DHCP server on the
    // network and this will be selected automatically. Otherwise a
    // random server is selected. Note that this simple approach has a
    // bias towards the slowest responses when applied to networks with
    // more than two DHCP servers.
    if (dhcpClient->dhcpServerAddr != 0xFFFFFFFF) {
        uint8_t randByte;
        gmosPalGetRandomBytes (&randByte, 1);
        if (randByte >= 0x80) {
            return;
        }
    }

    // Copy the required fields to the DHCP client data structure.
    dhcpClient->assignedAddr = rxMessage.assignedAddr;
    dhcpClient->dhcpServerAddr = rxMessage.dhcpServerAddr;
    dhcpClient->subnetMask = rxMessage.subnetMask;
    dhcpClient->gatewayAddr = rxMessage.gatewayAddr;

    // Select the primary DNS server.
    if ((rxMessage.optValidFlags &
        GMOS_TCPIP_DHCP_MESSAGE_OPTION_FLAG_DNS1_SERVER) != 0) {
        dhcpClient->dns1ServerAddr = rxMessage.dns1ServerAddr;
    } else {
        dhcpClient->dns1ServerAddr = GMOS_CONFIG_TCPIP_DNS_IPV4_PRIMARY;
    }

    // Select the secondary DNS server.
    if ((rxMessage.optValidFlags &
        GMOS_TCPIP_DHCP_MESSAGE_OPTION_FLAG_DNS2_SERVER) != 0) {
        dhcpClient->dns2ServerAddr = rxMessage.dns2ServerAddr;
    } else {
        dhcpClient->dns2ServerAddr = GMOS_CONFIG_TCPIP_DNS_IPV4_SECONDARY;
    }

    // Log server information for debugging. Note that addresses are
    // decoded directly from network byte ordered integers.
    GMOS_LOG_FMT (LOG_DEBUG,
        "DHCP : Server %d.%d.%d.%d offered address %d.%d.%d.%d.",
        ((uint8_t*) &rxMessage.dhcpServerAddr) [0],
        ((uint8_t*) &rxMessage.dhcpServerAddr) [1],
        ((uint8_t*) &rxMessage.dhcpServerAddr) [2],
        ((uint8_t*) &rxMessage.dhcpServerAddr) [3],
        ((uint8_t*) &rxMessage.assignedAddr) [0],
        ((uint8_t*) &rxMessage.assignedAddr) [1],
        ((uint8_t*) &rxMessage.assignedAddr) [2],
        ((uint8_t*) &rxMessage.assignedAddr) [3]);
}

/*
 * Parses a DHCP 'ACK' or 'NAK' message that has been received in
 * response to a DHCP request. This is common to the RFC2131
 * 'REQUESTING',  'RENEWING' and 'REBINDING' states.
 */
static inline bool gmosTcpipDhcpClientParseDhcpResponse (
    gmosTcpipDhcpClient_t* dhcpClient, gmosBuffer_t* rxBuffer,
    uint8_t* messageType)
{
    gmosTcpipDhcpRxMessage_t rxMessage;
    uint8_t requiredOpts;
    uint32_t leaseTime;

    // Parse the received DHCP message.
    if (!gmosTcpipDhcpClientParseRxMessage (
        dhcpClient, rxBuffer, &rxMessage)) {
        return false;
    }
    *messageType = rxMessage.messageType;

    // Process all 'NAK' responses and discard unexpected responses.
    if (*messageType == GMOS_TCPIP_DHCP_MESSAGE_TYPE_NAK) {
        return true;
    } else if (*messageType != GMOS_TCPIP_DHCP_MESSAGE_TYPE_ACK) {
        return false;
    }

    // Determine if the parsed message has the required options.
    // Silently discard malformed responses.
    requiredOpts =
        GMOS_TCPIP_DHCP_MESSAGE_OPTION_FLAG_LEASE_TIME |
        GMOS_TCPIP_DHCP_MESSAGE_OPTION_FLAG_SERVER_ID;
    if ((rxMessage.optValidFlags & requiredOpts) != requiredOpts) {
        return false;
    }

    // Only accept messages with a consistent assigned address and
    // subnet mask.
    if (dhcpClient->assignedAddr != rxMessage.assignedAddr) {
        return false;
    }
    if ((rxMessage.optValidFlags &
        GMOS_TCPIP_DHCP_MESSAGE_OPTION_FLAG_SUBNET_MASK) != 0) {
        if (dhcpClient->subnetMask != rxMessage.subnetMask) {
            return false;
        }
    }

    // Copy the server address to the DHCP client data structure. This
    // will override any values sent during the prior discovery phase,
    // but should only change during rebinding.
    dhcpClient->dhcpServerAddr = rxMessage.dhcpServerAddr;

    // Override the gateway address if required.
    if ((rxMessage.optValidFlags &
        GMOS_TCPIP_DHCP_MESSAGE_OPTION_FLAG_GATEWAY_ROUTERS) != 0) {
        dhcpClient->gatewayAddr = rxMessage.gatewayAddr;
    }

    // The DHCP lease time is limited to 604800 seconds (7 days). It is
    // converted to system ticks before storing.
    if (rxMessage.leaseTime > 604800) {
        leaseTime = GMOS_MS_TO_TICKS (1000 * 604800);
    } else {
        leaseTime = GMOS_MS_TO_TICKS (1000 * rxMessage.leaseTime);
    }
    dhcpClient->leaseTime = leaseTime;
    dhcpClient->leaseEnd = leaseTime + gmosPalGetTimer ();

    // Override the the primary DNS server if required.
    if ((rxMessage.optValidFlags &
        GMOS_TCPIP_DHCP_MESSAGE_OPTION_FLAG_DNS1_SERVER) != 0) {
        dhcpClient->dns1ServerAddr = rxMessage.dns1ServerAddr;
    }

    // Override the secondary DNS server if required.
    if ((rxMessage.optValidFlags &
        GMOS_TCPIP_DHCP_MESSAGE_OPTION_FLAG_DNS2_SERVER) != 0) {
        dhcpClient->dns2ServerAddr = rxMessage.dns2ServerAddr;
    }
    return true;
}

/*
 * Perform setup immediately after opening a new UDP socket.
 */
static gmosTaskStatus_t gmosTcpipDhcpClientSocketSetup (
    gmosTcpipDhcpClient_t* dhcpClient)
{
    uint16_t startupDelay;

    // Increment the 'xid' value each time a new UDP socket is opened.
    dhcpClient->dhcpXid += 1;

    // Set the task scheduling status to give a semi-random delay
    // between 1 and 10 seconds, as per RFC2131 section 4.4.1.
    gmosPalGetRandomBytes ((uint8_t*) &startupDelay, sizeof (startupDelay));
    while (startupDelay > GMOS_MS_TO_TICKS (9000)) {
        startupDelay /= 2;
    }
    startupDelay += GMOS_MS_TO_TICKS (1000);
    return GMOS_TASK_RUN_LATER (startupDelay);
}

/*
 * Starts the DHCP discovery process by broadcasting the DHCP discovery
 * message.
 */
static inline bool gmosTcpipDhcpClientDiscoveryStart (
    gmosTcpipDhcpClient_t* dhcpClient)
{
    gmosBuffer_t message = GMOS_BUFFER_INIT ();
    gmosNetworkStatus_t stackStatus;

    // The DHCP server address is set to all ones to indicate that a
    // server has not yet been found.
    dhcpClient->dhcpServerAddr = 0xFFFFFFFF;

    // Format the discovery message.
    if (!gmosTcpipDhcpClientFormatDhcpDiscover (dhcpClient, &message)) {
        return false;
    }

    // Attempt to broadcast the discovery message.
    stackStatus = gmosDriverTcpipUdpSendTo (dhcpClient->udpSocket,
        gmosTcpipBroadcastAddr, GMOS_TCPIP_DHCP_SERVER_PORT, &message);

    // Set the discovery window timeout on success.
    if (stackStatus == GMOS_NETWORK_STATUS_SUCCESS) {
        dhcpClient->timestamp = gmosPalGetTimer () +
            GMOS_MS_TO_TICKS (GMOS_TCPIP_DHCP_DISCOVERY_WINDOW * 1000);
        return true;
    }

    // Release resources and retry on failure.
    // TODO: should reset and restart on anything other than 'retry'.
    else {
        gmosBufferReset (&message, 0);
        return false;
    }
}

/*
 * Waits for DHCP offer responses from available DHCP servers.
 */
static inline gmosTaskStatus_t gmosTcpipDhcpClientSelectingWait (
    gmosTcpipDhcpClient_t* dhcpClient)
{
    gmosNetworkStatus_t stackStatus;
    uint8_t remoteAddr [4];
    uint16_t remotePort;
    gmosBuffer_t payload = GMOS_BUFFER_INIT ();
    int32_t timeoutDelay;

    // Set the default discovery window timeout.
    timeoutDelay = (int32_t) (dhcpClient->timestamp);
    timeoutDelay -= (int32_t) gmosPalGetTimer ();

    // Process DHCP offer messages that were received during the
    // discovery window.
    while (true) {
        stackStatus = gmosDriverTcpipUdpReceiveFrom (
            dhcpClient->udpSocket, remoteAddr, &remotePort, &payload);

        // No further response messages to process.
        if (stackStatus != GMOS_NETWORK_STATUS_SUCCESS) {
            break;
        }

        // Process all messages that were sent from the standard DHCP
        // server port.
        if (remotePort == GMOS_TCPIP_DHCP_SERVER_PORT) {
            gmosTcpipDhcpClientParseDhcpOffer (dhcpClient, &payload);
        }
    }

    // Release any residual buffer contents.
    gmosBufferReset (&payload, 0);

    // Determine whether the discovery window has now closed.
    if (timeoutDelay > 0) {
        return GMOS_TASK_RUN_LATER ((uint32_t) timeoutDelay);
    } else {
        return GMOS_TASK_RUN_IMMEDIATE;
    }
}

/*
 * Starts the DHCP requesting process by broadcasting the DHCP request
 * message.
 */
static inline bool gmosTcpipDhcpClientSelectingDone (
    gmosTcpipDhcpClient_t* dhcpClient)
{
    gmosBuffer_t message = GMOS_BUFFER_INIT ();
    gmosNetworkStatus_t stackStatus;

    // Format the selecting request message.
    if (!gmosTcpipDhcpClientFormatDhcpRequest (
        dhcpClient, &message, true, false, true, true)) {
        return false;
    }

    // Attempt to broadcast the request message.
    stackStatus = gmosDriverTcpipUdpSendTo (dhcpClient->udpSocket,
        gmosTcpipBroadcastAddr, GMOS_TCPIP_DHCP_SERVER_PORT, &message);

    // Set the requesting window timeout on success.
    if (stackStatus == GMOS_NETWORK_STATUS_SUCCESS) {
        dhcpClient->timestamp = gmosPalGetTimer () +
            GMOS_MS_TO_TICKS (GMOS_TCPIP_DHCP_RESPONSE_WINDOW * 1000);
        return true;
    }

    // Release resources and retry on failure.
    // TODO: should reset and restart on anything other than 'retry'.
    else {
        gmosBufferReset (&message, 0);
        return false;
    }
}

/*
 * Implements the DHCP response wait process by processing the DHCP
 * messages received in response to the request message.
 */
static gmosTaskStatus_t gmosTcpipDhcpClientResponseWait (
    gmosTcpipDhcpClient_t* dhcpClient, uint8_t* messageType)
{
    gmosNetworkStatus_t stackStatus;
    uint8_t remoteAddr [4];
    uint16_t remotePort;
    gmosBuffer_t payload = GMOS_BUFFER_INIT ();
    int32_t timeoutDelay;

    // Set the default request acceptance state and request window
    // timeout.
    *messageType = GMOS_TCPIP_DHCP_MESSAGE_TYPE_INVALID;
    timeoutDelay = (int32_t) (dhcpClient->timestamp);
    timeoutDelay -= (int32_t) gmosPalGetTimer ();

    // Process DHCP response messages that were received during the
    // requesting window.
    while (true) {
        stackStatus = gmosDriverTcpipUdpReceiveFrom (
            dhcpClient->udpSocket, remoteAddr, &remotePort, &payload);

        // No further response messages to process.
        if (stackStatus != GMOS_NETWORK_STATUS_SUCCESS) {
            break;
        }

        // Process the first valid message that was sent from the
        // standard DHCP server port.
        if ((remotePort == GMOS_TCPIP_DHCP_SERVER_PORT) &&
            (gmosTcpipDhcpClientParseDhcpResponse (
                dhcpClient, &payload, messageType))) {
            timeoutDelay = 0;
            break;
        }
    }

    // Release any residual buffer contents.
    gmosBufferReset (&payload, 0);

    // Determine whether the requesting window has now closed.
    if (timeoutDelay > 0) {
        return GMOS_TASK_RUN_LATER ((uint32_t) timeoutDelay);
    } else {
        return GMOS_TASK_RUN_IMMEDIATE;
    }
}

/*
 * Close the DHCP UDP socket on request completion.
 */
static bool gmosTcpipDhcpClientResponseDone (
    gmosTcpipDhcpClient_t* dhcpClient)
{
    gmosNetworkStatus_t stackStatus;
    uint32_t currentTime = gmosPalGetTimer ();
    int32_t retryDelay;
    int32_t minRetryTicks;

    // The timestamp for the first lease renewal attempt is set to 1/2
    // the lease period, as recommended by RFC2131 section 4.4.5.
    retryDelay = (int32_t) (dhcpClient->leaseEnd - currentTime);
    if (retryDelay > (dhcpClient->leaseTime / 2)) {
        retryDelay /= 2;
    }

    // The timestamp for subsequent lease renewal attempts is set to 1/4
    // the remaining lease period, which increases the number of retry
    // attempts from the conventional approach described in RFC2131
    // section 4.4.5.
    else {
        retryDelay /= 4;
    }

    // The retry delay is limited to the minimum retry interval, as
    // recommended by RFC2131 section 4.4.5.
    minRetryTicks = (int32_t) GMOS_MS_TO_TICKS (
        1000 * GMOS_TCPIP_DHCP_MIN_RETRY_INTERVAL);
    if (retryDelay < minRetryTicks) {
        retryDelay = minRetryTicks;
    }
    dhcpClient->timestamp = currentTime + retryDelay;

    // Attempt to close the UDP socket.
    stackStatus = gmosDriverTcpipUdpClose (dhcpClient->udpSocket);
    if (stackStatus == GMOS_NETWORK_STATUS_SUCCESS) {
        dhcpClient->udpSocket = NULL;
        return true;
    } else {
        return false;
    }
}

/*
 * Implement lease renewal timeouts from the DHCP 'BOUND' state.
 */
static inline gmosTaskStatus_t gmosTcpipDhcpClientBoundTimeout (
    gmosTcpipDhcpClient_t* dhcpClient, bool* leaseExpired)
{
    int32_t expiryPeriod;
    int32_t timeoutDelay;

    // If the next renewal request is scheduled for after the end of
    // the lease period, the lease is no longer valid. Note that this
    // also takes into account the time required for renewal message
    // timeouts.
    expiryPeriod = (int32_t) (dhcpClient->leaseEnd);
    expiryPeriod -= (int32_t) (dhcpClient->timestamp);
    expiryPeriod -= (int32_t) (GMOS_MS_TO_TICKS (
        1500 * GMOS_TCPIP_DHCP_RESPONSE_WINDOW));
    if (expiryPeriod <= 0) {
        *leaseExpired = true;
        return GMOS_TASK_RUN_IMMEDIATE;
    } else {
        *leaseExpired = false;
    }

    // Get the DHCP renewal window timeout delay.
    timeoutDelay = (int32_t) (dhcpClient->timestamp);
    timeoutDelay -= (int32_t) gmosPalGetTimer ();
    GMOS_LOG_FMT (LOG_VERBOSE,
        "DHCP : Timeout delay in 'BOUND' state = %ds.",
        GMOS_TICKS_TO_MS (timeoutDelay) / 1000);

    // Determine whether the timeout has expired.
    if (timeoutDelay > 0) {
        return GMOS_TASK_RUN_LATER ((uint32_t) timeoutDelay);
    } else {
        return GMOS_TASK_RUN_IMMEDIATE;
    }
}

/*
 * Starts the DHCP renewal or rebinding process by unicasting or
 * broadcasting the DHCP request message.
 */
static inline bool gmosTcpipDhcpClientRenewalInit (
    gmosTcpipDhcpClient_t* dhcpClient, bool* leaseExpired)
{
    gmosBuffer_t message = GMOS_BUFFER_INIT ();
    gmosNetworkStatus_t stackStatus;
    int32_t leaseRemaining;
    int32_t leaseExpiryTime;
    uint8_t* serverAddr;

    // Select the server address or broadcast address, depending on
    // the remaining lease time.
    *leaseExpired = false;
    leaseRemaining = (int32_t) (dhcpClient->leaseEnd - gmosPalGetTimer ());

    // The lease is treated as having already expired if it would
    // otherwise expire during the response window or shortly after.
    leaseExpiryTime = (int32_t) (GMOS_MS_TO_TICKS (
        1500 * GMOS_TCPIP_DHCP_RESPONSE_WINDOW));
    if (leaseRemaining < leaseExpiryTime) {
        *leaseExpired = true;
        return true;
    }

    // A rebinding request occurs if the remaining lease interval is
    // less than 1/8 of the original lease period, as recommended by
    // RFC2131 section 4.4.5.
    else if (leaseRemaining > (int32_t) (dhcpClient->leaseTime / 8)) {
        serverAddr = (uint8_t*) &(dhcpClient->dhcpServerAddr);
    } else {
        serverAddr = gmosTcpipBroadcastAddr;
    }

    // Format the renewal or rebinding request message.
    if (!gmosTcpipDhcpClientFormatDhcpRequest (
        dhcpClient, &message, false, true, false, false)) {
        return false;
    }

    // Attempt to transmit the request message.
    stackStatus = gmosDriverTcpipUdpSendTo (dhcpClient->udpSocket,
        serverAddr, GMOS_TCPIP_DHCP_SERVER_PORT, &message);

    // Set the requesting window timeout on success.
    if (stackStatus == GMOS_NETWORK_STATUS_SUCCESS) {
        dhcpClient->timestamp = gmosPalGetTimer () +
            GMOS_MS_TO_TICKS (GMOS_TCPIP_DHCP_RESPONSE_WINDOW * 1000);
        return true;
    }

    // Release resources and retry on failure.
    // TODO: should reset and restart on anything other than 'retry'.
    else {
        gmosBufferReset (&message, 0);
        return false;
    }
}

/*
 * Send an IP address check message. This is a unicast message to the
 * assigned IP address that is expected to time out. The UDP discard
 * protocol port is used as the destination.
 */
static inline bool gmosTcpipDhcpClientAddrCheckSend (
    gmosTcpipDhcpClient_t* dhcpClient)
{
    gmosNetworkStatus_t stackStatus;
    gmosBuffer_t message = GMOS_BUFFER_INIT ();
    uint8_t addrCheckMsg [] = "DHCP ARP Timeout Check.";

    // Create a dummy payload for the test message.
    gmosBufferAppend (&message, addrCheckMsg, sizeof (addrCheckMsg) - 1);

    // Attempt to send the test message.
    stackStatus = gmosDriverTcpipUdpSendTo (dhcpClient->udpSocket,
        (uint8_t*) &dhcpClient->assignedAddr,
        GMOS_TCPIP_DISCARD_SERVER_PORT, &message);
    if (stackStatus == GMOS_NETWORK_STATUS_SUCCESS) {
        return true;
    } else {
        gmosBufferReset (&message, 0);
        return false;
    }
}

/*
 * Send a DHCP decline messages on detecting an IP address conflict.
 */
static inline bool gmosTcpipDhcpClientAddrDecline (
    gmosTcpipDhcpClient_t* dhcpClient)
{
    gmosBuffer_t message = GMOS_BUFFER_INIT ();
    gmosNetworkStatus_t stackStatus;

    // Format the address decline message.
    if (!gmosTcpipDhcpClientFormatDhcpDecline (dhcpClient, &message)) {
        return false;
    }

    // Attempt to broadcast the address decline.
    stackStatus = gmosDriverTcpipUdpSendTo (dhcpClient->udpSocket,
        gmosTcpipBroadcastAddr, GMOS_TCPIP_DHCP_SERVER_PORT, &message);
    if (stackStatus == GMOS_NETWORK_STATUS_SUCCESS) {
        return true;
    } else {
        gmosBufferReset (&message, 0);
        return false;
    }
}

/*
 * Restart the DHCP state machine on failure to obtain or renew a lease.
 */
static inline bool gmosTcpipDhcpClientRestart (
    gmosTcpipDhcpClient_t* dhcpClient)
{
    gmosNetworkStatus_t stackStatus;

    // If the UDP socket is already closed, the DHCP state machine can
    // restart immediately.
    if (dhcpClient->udpSocket == NULL) {
        return true;
    }

    // Attempt to close the UDP socket.
    stackStatus = gmosDriverTcpipUdpClose (dhcpClient->udpSocket);
    if (stackStatus == GMOS_NETWORK_STATUS_SUCCESS) {
        dhcpClient->udpSocket = NULL;
        return true;
    } else {
        return false;
    }
}

/*
 * Implement stack notification callback handler.
 */
static void gmosTcpipDhcpClientStackNotifyHandler (
    void* notifyData, gmosTcpipStackNotify_t notification)
{
    gmosTcpipDhcpClient_t* dhcpClient =
        (gmosTcpipDhcpClient_t*) notifyData;
    bool taskResume = false;

    // Check for notifications while in the IP address checking wait
    // state.
    if (dhcpClient->dhcpState ==
        GMOS_TCPIP_DHCP_CLIENT_STATE_ADDR_CHECK_WAIT) {

        // If the test message is successfuly sent, this implies that
        // another device on the network is responding to ARP requests
        // for the assigned IP and the DHCP offer must be declined.
        if (notification == GMOS_TCPIP_STACK_NOTIFY_UDP_MESSAGE_SENT) {
            GMOS_LOG_FMT (LOG_DEBUG,
                "DHCP : IP address conflict detected for %d.%d.%d.%d.",
                ((uint8_t*) &dhcpClient->assignedAddr) [0],
                ((uint8_t*) &dhcpClient->assignedAddr) [1],
                ((uint8_t*) &dhcpClient->assignedAddr) [2],
                ((uint8_t*) &dhcpClient->assignedAddr) [3]);
            dhcpClient->dhcpState =
                GMOS_TCPIP_DHCP_CLIENT_STATE_REQUESTING_DECLINE;
            taskResume = true;
        }

        // An ARP timeout notification implies that the assigned address
        // is not in use.
        else if (notification == GMOS_TCPIP_STACK_NOTIFY_UDP_ARP_TIMEOUT) {
            GMOS_LOG_FMT (LOG_VERBOSE,
                "DHCP : IP ARP timeout detected for %d.%d.%d.%d.",
                ((uint8_t*) &dhcpClient->assignedAddr) [0],
                ((uint8_t*) &dhcpClient->assignedAddr) [1],
                ((uint8_t*) &dhcpClient->assignedAddr) [2],
                ((uint8_t*) &dhcpClient->assignedAddr) [3]);
            dhcpClient->dhcpState =
                GMOS_TCPIP_DHCP_CLIENT_STATE_REQUESTING_SUCCESS;
            taskResume = true;
        }
    }

    // Resume processing for the DHCP client task.
    if (taskResume) {
        gmosSchedulerTaskResume (&dhcpClient->dhcpWorkerTask);
    }
}

/*
 * Implement the main task loop for the DHCP client protocol processing.
 */
static gmosTaskStatus_t gmosTcpipDhcpClientWorkerTaskFn (void* taskData)
{
    gmosTcpipDhcpClient_t* dhcpClient = (gmosTcpipDhcpClient_t*) taskData;
    gmosDriverTcpip_t* tcpipDriver = dhcpClient->tcpipDriver;
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (10));
    uint8_t nextState = dhcpClient->dhcpState;
    uint8_t messageType;
    bool leaseExpired;

    // Implement the DHCP client processing state machine.
    switch (dhcpClient->dhcpState) {

        // In the unconnected state, wait for the PHY link to come up.
        case GMOS_TCPIP_DHCP_CLIENT_STATE_UNCONNECTED :
            if (gmosDriverTcpipPhyLinkIsUp (tcpipDriver)) {
                nextState = GMOS_TCPIP_DHCP_CLIENT_STATE_SET_DEFAULT_ADDR;
            }
            break;

        // Assign a default local IP address. Since this is not known,
        // an all-zero address will be used, as per RFC2131 section 4.1.
        case GMOS_TCPIP_DHCP_CLIENT_STATE_SET_DEFAULT_ADDR :
            if (gmosDriverTcpipSetNetworkInfoIpv4 (tcpipDriver,
                gmosTcpipAllZeroAddr, gmosTcpipAllZeroAddr, 0)) {
                nextState = GMOS_TCPIP_DHCP_CLIENT_STATE_DISCOVERY_OPEN;
            }
            break;

        // Open a local DHCP socket for the discovery process.
        case GMOS_TCPIP_DHCP_CLIENT_STATE_DISCOVERY_OPEN :
            dhcpClient->udpSocket = gmosDriverTcpipUdpOpen (
                tcpipDriver, false, GMOS_TCPIP_DHCP_CLIENT_PORT,
                &dhcpClient->dhcpWorkerTask,
                gmosTcpipDhcpClientStackNotifyHandler, dhcpClient);
            if (dhcpClient->udpSocket != NULL) {
                nextState = GMOS_TCPIP_DHCP_CLIENT_STATE_DISCOVERY_INIT;
                taskStatus = gmosTcpipDhcpClientSocketSetup (dhcpClient);
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (1000));
            }
            break;

        // Start the DHCP discovery process by broadcasting the DHCP
        // discovery message (the RFC2131 'INIT' state).
        case GMOS_TCPIP_DHCP_CLIENT_STATE_DISCOVERY_INIT :
            if (gmosTcpipDhcpClientDiscoveryStart (dhcpClient)) {
                nextState = GMOS_TCPIP_DHCP_CLIENT_STATE_SELECTING_WAIT;
            }
            break;

        // Wait for the discovery timeout to complete (the RFC2131
        // 'SELECTING' state while collecting DHCP offers).
        case GMOS_TCPIP_DHCP_CLIENT_STATE_SELECTING_WAIT :
            taskStatus = gmosTcpipDhcpClientSelectingWait (dhcpClient);
            if (taskStatus == GMOS_TASK_RUN_IMMEDIATE) {
                GMOS_LOG (LOG_DEBUG, "DHCP : Discovery phase complete.");
                nextState = GMOS_TCPIP_DHCP_CLIENT_STATE_SELECTING_DONE;
            }
            break;

        // Check that a valid DHCP server has been found and then send
        // the DHCP request from the 'selecting' state.
        case GMOS_TCPIP_DHCP_CLIENT_STATE_SELECTING_DONE :
            if (dhcpClient->dhcpServerAddr == 0xFFFFFFFF) {
                nextState = GMOS_TCPIP_DHCP_CLIENT_STATE_RESTARTING;
            } else if (gmosTcpipDhcpClientSelectingDone (dhcpClient)) {
                nextState = GMOS_TCPIP_DHCP_CLIENT_STATE_REQUESTING_WAIT;
            }
            break;

        // Wait for a response to the lease request.
        case GMOS_TCPIP_DHCP_CLIENT_STATE_REQUESTING_WAIT:
            taskStatus = gmosTcpipDhcpClientResponseWait (
                dhcpClient, &messageType);
            if (messageType == GMOS_TCPIP_DHCP_MESSAGE_TYPE_ACK) {
                GMOS_LOG (LOG_DEBUG, "DHCP : Lease request serviced.");
                nextState = GMOS_TCPIP_DHCP_CLIENT_STATE_ADDR_CHECK_SEND;
            } else if (messageType == GMOS_TCPIP_DHCP_MESSAGE_TYPE_NAK) {
                GMOS_LOG (LOG_DEBUG, "DHCP : Lease request rejected.");
                nextState = GMOS_TCPIP_DHCP_CLIENT_STATE_RESTARTING;
            } else if (taskStatus == GMOS_TASK_RUN_IMMEDIATE) {
                GMOS_LOG (LOG_DEBUG, "DHCP : Lease request timed out.");
                nextState = GMOS_TCPIP_DHCP_CLIENT_STATE_RESTARTING;
            }
            break;

        // Send an address check message to the assigned IP address,
        // then suspend processing until the address check is complete.
        case GMOS_TCPIP_DHCP_CLIENT_STATE_ADDR_CHECK_SEND :
            if (gmosTcpipDhcpClientAddrCheckSend (dhcpClient)) {
                nextState = GMOS_TCPIP_DHCP_CLIENT_STATE_ADDR_CHECK_WAIT;
                taskStatus = GMOS_TASK_SUSPEND;
            }
            break;

        // Send the DHCP decline message to notify the server that an
        // assigned address is already in use.
        case GMOS_TCPIP_DHCP_CLIENT_STATE_REQUESTING_DECLINE :
            if (gmosTcpipDhcpClientAddrDecline (dhcpClient)) {
                GMOS_LOG (LOG_DEBUG, "DHCP : Lease address declined.");
                nextState = GMOS_TCPIP_DHCP_CLIENT_STATE_RESTARTING;
            }
            break;


        // Close the DHCP UDP socket on request completion.
        case GMOS_TCPIP_DHCP_CLIENT_STATE_REQUESTING_SUCCESS :
            if (gmosTcpipDhcpClientResponseDone (dhcpClient)) {
                GMOS_LOG (LOG_DEBUG, "DHCP : Lease address accepted.");
                nextState = GMOS_TCPIP_DHCP_CLIENT_STATE_SET_ASSIGNED_ADDR;
            }
            break;

        // Set the local network configuration using the DHCP settings.
        case GMOS_TCPIP_DHCP_CLIENT_STATE_SET_ASSIGNED_ADDR :
            if (gmosDriverTcpipSetNetworkInfoIpv4 (tcpipDriver,
                (const uint8_t*) &(dhcpClient->assignedAddr),
                (const uint8_t*) &(dhcpClient->gatewayAddr),
                (const uint8_t*) &(dhcpClient->subnetMask))) {
                nextState = GMOS_TCPIP_DHCP_CLIENT_STATE_BOUND;
            }
            break;

        // In the bound state, wait for the lease renewal timer to
        // expire.
        case GMOS_TCPIP_DHCP_CLIENT_STATE_BOUND :
            taskStatus = gmosTcpipDhcpClientBoundTimeout (
                dhcpClient, &leaseExpired);
            if (leaseExpired) {
                GMOS_LOG (LOG_DEBUG, "DHCP : Lease expired on timeout.");
                nextState = GMOS_TCPIP_DHCP_CLIENT_STATE_RESTARTING;
            } else if (taskStatus == GMOS_TASK_RUN_IMMEDIATE) {
                nextState = GMOS_TCPIP_DHCP_CLIENT_STATE_RENEWAL_OPEN;
            }
            break;

        // Open a local DHCP socket for the renewal process.
        case GMOS_TCPIP_DHCP_CLIENT_STATE_RENEWAL_OPEN :
            dhcpClient->udpSocket = gmosDriverTcpipUdpOpen (
                tcpipDriver, false, GMOS_TCPIP_DHCP_CLIENT_PORT,
                &dhcpClient->dhcpWorkerTask,
                gmosTcpipDhcpClientStackNotifyHandler, dhcpClient);
            if (dhcpClient->udpSocket != NULL) {
                taskStatus = gmosTcpipDhcpClientSocketSetup (dhcpClient);
                nextState = GMOS_TCPIP_DHCP_CLIENT_STATE_RENEWAL_INIT;
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (1000));
            }
            break;

        // Initiate the renewal or rebinding process by sending a new
        // request message.
        case GMOS_TCPIP_DHCP_CLIENT_STATE_RENEWAL_INIT :
            if (gmosTcpipDhcpClientRenewalInit (dhcpClient, &leaseExpired)) {
                if (leaseExpired) {
                    GMOS_LOG (LOG_DEBUG, "DHCP : Lease expired on renewal.");
                    nextState = GMOS_TCPIP_DHCP_CLIENT_STATE_RESTARTING;
                } else {
                    nextState = GMOS_TCPIP_DHCP_CLIENT_STATE_RENEWAL_WAIT;
                }
            }
            break;

        // Wait for a lease renewal response.
        case GMOS_TCPIP_DHCP_CLIENT_STATE_RENEWAL_WAIT :
            taskStatus = gmosTcpipDhcpClientResponseWait (
                dhcpClient, &messageType);
            if (messageType == GMOS_TCPIP_DHCP_MESSAGE_TYPE_ACK) {
                GMOS_LOG (LOG_DEBUG, "DHCP : Lease renewal accepted.");
                nextState = GMOS_TCPIP_DHCP_CLIENT_STATE_RENEWAL_DONE;
            } else if (messageType == GMOS_TCPIP_DHCP_MESSAGE_TYPE_NAK) {
                GMOS_LOG (LOG_DEBUG, "DHCP : Lease renewal rejected.");
                nextState = GMOS_TCPIP_DHCP_CLIENT_STATE_RESTARTING;
            } else if (taskStatus == GMOS_TASK_RUN_IMMEDIATE) {
                GMOS_LOG (LOG_DEBUG, "DHCP : Lease renewal timed out.");
                nextState = GMOS_TCPIP_DHCP_CLIENT_STATE_RENEWAL_DONE;
            }
            break;

        // Close the DHCP UDP socket on request completion.
        case GMOS_TCPIP_DHCP_CLIENT_STATE_RENEWAL_DONE :
            if (gmosTcpipDhcpClientResponseDone (dhcpClient)) {
                nextState = GMOS_TCPIP_DHCP_CLIENT_STATE_BOUND;
            }
            break;

        // Restart the DHCP state machine on failure to obtain or renew
        // a lease.
        case GMOS_TCPIP_DHCP_CLIENT_STATE_RESTARTING :
            if (gmosTcpipDhcpClientRestart (dhcpClient)) {
                nextState = GMOS_TCPIP_DHCP_CLIENT_STATE_UNCONNECTED;
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (
                    1000 * GMOS_TCPIP_DHCP_MIN_RESTART_INTERVAL));
            }
            break;
    }
    dhcpClient->dhcpState = nextState;
    return taskStatus;
}

/*
 * Initialise the DHCP client on startup, using the specified TCP/IP
 * interface.
 */
bool gmosTcpipDhcpClientInit (gmosTcpipDhcpClient_t* dhcpClient,
    gmosDriverTcpip_t* tcpipDriver, const char* dhcpHostName)
{
    gmosTaskState_t* dhcpWorkerTask = &dhcpClient->dhcpWorkerTask;
    uint8_t* ethMacAddr;
    uint32_t randomValue;

    // Initialise the DHCP client state.
    dhcpClient->tcpipDriver = tcpipDriver;
    dhcpClient->dhcpHostName = dhcpHostName;
    dhcpClient->dhcpState = GMOS_TCPIP_DHCP_CLIENT_STATE_UNCONNECTED;

    // Select a random XID on startup. The local MAC address is used to
    // seed the random number generator if no source of entropy is
    // available.
    ethMacAddr = gmosDriverTcpipGetMacAddr (tcpipDriver);
    randomValue = ((uint32_t) ethMacAddr [5]) |
        (((uint32_t) ethMacAddr [4]) << 8) |
        (((uint32_t) ethMacAddr [3]) << 16) |
        (((uint32_t) ethMacAddr [2]) << 24);
    gmosPalAddRandomEntropy (randomValue);
    gmosPalGetRandomBytes ((uint8_t*) &randomValue, sizeof (randomValue));
    dhcpClient->dhcpXid = randomValue;

    // Initialise the DHCP worker task and schedule it for immediate
    // execution.
    dhcpWorkerTask->taskTickFn = gmosTcpipDhcpClientWorkerTaskFn;
    dhcpWorkerTask->taskData = dhcpClient;
    dhcpWorkerTask->taskName =
        GMOS_TASK_NAME_WRAPPER ("TCP/IP DHCP Client");
    gmosSchedulerTaskStart (dhcpWorkerTask);

    return true;
}

/*
 * Determines if the DHCP client has successfully obtained a valid IP
 * address and network configuration.
 */
bool gmosTcpipDhcpClientReady (gmosTcpipDhcpClient_t* dhcpClient)
{
    gmosDriverTcpip_t* tcpipDriver = dhcpClient->tcpipDriver;
    gmosTaskState_t* dhcpWorkerTask = &dhcpClient->dhcpWorkerTask;
    bool dhcpReady = true;

    // All states prior to the 'bound' state correspond to the DHCP
    // acquisition process.
    if (dhcpClient->dhcpState < GMOS_TCPIP_DHCP_CLIENT_STATE_BOUND) {
        dhcpReady = false;
    }

    // Loss of local network connectivity invalidates the DHCP settings.
    else if (!gmosDriverTcpipPhyLinkIsUp (tcpipDriver)) {
        dhcpClient->dhcpState = GMOS_TCPIP_DHCP_CLIENT_STATE_RESTARTING;
        gmosSchedulerTaskResume (dhcpWorkerTask);
        dhcpReady = false;
    }
    return dhcpReady;
}
