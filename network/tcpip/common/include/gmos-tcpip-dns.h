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
 * This header provides the common API for issuing IPv4 (A record) and
 * optionally IPv6 (AAAA record) DNS client requests for vendor supplied
 * and hardware accelerated TCP/IP stacks. Only recursive requests are
 * supported.
 */

#ifndef GMOS_TCPIP_DNS_H
#define GMOS_TCPIP_DNS_H

#include <stdint.h>
#include <stdbool.h>
#include "gmos-scheduler.h"
#include "gmos-buffers.h"
#include "gmos-network.h"
#include "gmos-tcpip-config.h"
#include "gmos-tcpip-stack.h"

/**
 * Defines the GubbinsMOS TCP/IP stack DNS server information structures
 * that are used to configure DNS lookups for a specific server.
 */
typedef struct gmosTcpipDnsServerInfo_t {

    // Specify a pointer to the next DNS server list entry.
    struct gmosTcpipDnsServerInfo_t* nextServer;

    // Specify the DNS server address. This may be a four octet IPv4
    // address or a sixteen octet IPv6 address.
    uint8_t address [GMOS_CONFIG_TCPIP_DNS_MAX_ADDR_SIZE];

    // Specify the server priority level.
    uint8_t priority;

    // Deremine if the server is reachable on an IPv6 address.
#if (GMOS_CONFIG_TCPIP_DNS_SUPPORT_IPV6)
    uint8_t addressIsIpv6;
#endif

} gmosTcpipDnsServerInfo_t;

/**
 * Defines the GubbinsMOS TCP/IP stack DNS client state that is used
 * for managing DNS lookups on a single TCP/IP interface.
 */
typedef struct gmosTcpipDnsClient_t {

    // Specify the TCP/IP stack instance to use for the DNS client.
    gmosTcpipStack_t* tcpipStack;

    // Specify the start of the DNS server list.
    gmosTcpipDnsServerInfo_t* dnsServerList;

    // Specify the IPv4 UDP socket currently in use by the DNS client.
    gmosTcpipStackSocket_t* udpSocketIpv4;

    // Specify the IPv6 UDP socket currently in use by the DNS client.
#if (GMOS_CONFIG_TCPIP_DNS_SUPPORT_IPV6)
    gmosTcpipStackSocket_t* udpSocketIpv6;
#endif

    // Allocate the DNS protocol worker task data structure.
    gmosTaskState_t dnsWorkerTask;

    // Allocate the array of data buffers that are used to store cache
    // table entries.
    gmosBuffer_t dnsCache [GMOS_CONFIG_TCPIP_DNS_CACHE_SIZE];

    // Specify the DNS transaction ID sequence number to be used.
    uint16_t dnsXid;

} gmosTcpipDnsClient_t;

/**
 * Initialise the DNS client on startup, using the specified DHCP client
 * for accessing the TCP/IP interface and DNS server information.
 * @param dnsClient This is a pointer to the DNS client data structure
 *     that should be used for storing the DNS client state.
 * @param tcpipStack This is an initialised TCP/IP stack data structure
 *     that represents the TCP/IP interface to be used by the DNS
 *     client.
 * @return Returns a boolean value which will be set to 'true' if the
 *     DNS client was successfully initialised and 'false' otherwise.
 */
bool gmosTcpipDnsClientInit (gmosTcpipDnsClient_t* dnsClient,
    gmosTcpipStack_t* tcpipStack);

/**
 * Adds a new DNS server to the list of available servers.
 * @param dnsClient This is a pointer to the DNS client data structure
 *     to which the new DNS server information is to be added.
 * @param dnsServerInfo This is a pointer to the new DNS server
 *     information data structure that is to be used for adding the
 *     new server.
 * @param useIpv6 This is a boolean value which should be set to 'true'
 *     id the server address is an IPv6 address and 'false' otherwise.
 * @param serverAddr This is a pointer to the new DNS server address.
 *     It may be a four octet IPv4 address or a sixteeen octet IPv6
 *     address and will be copied to local storage.
 * @param priority This is the server access priority. Servers will be
 *     accessed in order of decreasing priority.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully adding the new DNS server configuration and 'false'
 *     otherwise.
 */
bool gmosTcpipDnsClientAddServer (gmosTcpipDnsClient_t* dnsClient,
    gmosTcpipDnsServerInfo_t* dnsServerInfo, bool useIpv6,
    uint8_t* serverAddr, uint8_t priority);

/**
 * Removes a DNS server from the list of available servers.
 * @param dnsClient This is a pointer to the DNS client data structure
 *     from which the new DNS server information is to be removed.
 * @param dnsServerInfo This is a pointer to the DNS server information
 *     data structure that is to be removed.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully removing the DNS server information and 'false' if
 *     the specified DNS server information was not currently in use.
 */
bool gmosTcpipDnsClientRemoveServer (gmosTcpipDnsClient_t* dnsClient,
    gmosTcpipDnsServerInfo_t* dnsServerInfo);

/**
 * Performs a DNS query for resolving a given DNS name to an IPv4
 * address.
 * @param dnsClient This is the DNS client which is to be used for
 *     performing the lookup.
 * @param dnsName This is the DNS name for which the corresponding IPv4
 *     address is to be resolved. Note that GubbinsMOS treats DNS names
 *     as being case sensitive, so capitalisation of individual DNS
 *     names should be consistent across calls to this function.
 * @param useIpv6 This is a boolean flag which should be set to 'true'
 *     to request an IPv6 address and 'false' to request an IPv4
 *     address.
 * @param dnsAddress This is a pointer to a four or sixteen octet array
 *     which will be populated with the resolved IPv4 or IPv6 address on
 *     successful resolution.
 * @return Returns a TCP/IP stack status value which will be set to
 *     'success' if the DNS entry was in the local cache and has been
 *     used to update the resolved address, 'retry' if the DNS lookup
 *     is still in progress or other status values to indicate failure.
 */
gmosNetworkStatus_t gmosTcpipDnsClientQuery (
    gmosTcpipDnsClient_t* dnsClient, const char* dnsName,
    bool useIpv6, uint8_t* dnsAddress);

#endif // GMOS_TCPIP_DNS_H
