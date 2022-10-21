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
 * This header provides the common API for issuing IPv4 (A record) DNS
 * client requests for vendor supplied and hardware accelerated TCP/IP
 * stacks. It relies on the IPv4 DHCP client for DNS server address
 * assignment. Only recursive requests are supported.
 */

#ifndef GMOS_TCPIP_DNS_H
#define GMOS_TCPIP_DNS_H

#include <stdint.h>
#include <stdbool.h>
#include "gmos-config.h"
#include "gmos-tcpip-dhcp.h"

/**
 * Specifies the number of DNS cache table entries.
 */
#ifndef GMOS_CONFIG_TCPIP_DNS_CACHE_SIZE
#define GMOS_CONFIG_TCPIP_DNS_CACHE_SIZE 2
#endif

/**
 * Specifies the DNS cache entry retention period as an integer number
 * of seconds.
 */
#ifndef GMOS_CONFIG_TCPIP_DNS_RETENTION_TIME
#define GMOS_CONFIG_TCPIP_DNS_RETENTION_TIME 60
#endif

/**
 * Specifies the number of DNS retry attempts. The DNS client will issue
 * a first request to the primary DNS server followed by the specified
 * number of retry attempts which alternate between the secondary and
 * primary servers.
 */
#ifndef GMOS_CONFIG_TCPIP_DNS_RETRY_COUNT
#define GMOS_CONFIG_TCPIP_DNS_RETRY_COUNT 3
#endif

/**
 * Specifies the timeout interval between DNS retry attempts as an
 * integer number of seconds.
 */
#ifndef GMOS_CONFIG_TCPIP_DNS_RETRY_INTERVAL
#define GMOS_CONFIG_TCPIP_DNS_RETRY_INTERVAL 4
#endif

/**
 * Defines the GubbinsMOS TCP/IP stack DNS client state that is used
 * for managing DNS lookups on a single TCP/IP interface.
 */
typedef struct gmosTcpipDnsClient_t {

    // Specify the DHCP client instance to use for the DNS client.
    gmosTcpipDhcpClient_t* dhcpClient;

    // Specify the UDP socket currently in use by the DNS client.
    gmosTcpipStackSocket_t* udpSocket;

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
 * @param dhcpClient This is a pointer to an initialised DHCP client
 *     instance that holds the current TCP/IP stack and DNS server
 *     information.
 * @return Returns a boolean value which will be set to 'true' if the
 *     DNS client was successfully initialised and 'false' otherwise.
 */
bool gmosTcpipDnsClientInit (gmosTcpipDnsClient_t* dnsClient,
    gmosTcpipDhcpClient_t* dhcpClient);

/**
 * Performs a DNS query for resolving a given DNS name to an IPv4
 * address.
 * @param dnsClient This is the DNS client which is to be used for
 *     performing the lookup.
 * @param dnsName This is the DNS name for which the corresponding IPv4
 *     address is to be resolved. Note that GubbinsMOS treats DNS names
 *     as being case sensitive, so capitalisation of individual DNS
 *     names should be consistent across calls to this function.
 * @param dnsAddress This is a pointer to a four octet array which will
 *     be populated with the resolved IPv4 address on successful
 *     resolution.
 * @return Returns a TCP/IP stack status value which will be set to
 *     'success' if the DNS entry was in the local cache and has been
 *     used to update the resolved IPv4 address, 'retry' if the DNS
 *     lookup is in progress or other status values to indicate failure.
 */
gmosTcpipStackStatus_t gmosTcpipDnsClientQuery (
    gmosTcpipDnsClient_t* dnsClient, const char* dnsName,
    uint8_t* dnsAddress);

#endif // GMOS_TCPIP_DNS_H
