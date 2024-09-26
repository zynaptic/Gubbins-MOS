/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2024 Zynaptic Limited
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
 * This header provides generic network link wrappers for conventional
 * TCP socket connections. This abstraction allows higher layer
 * protocols to operate across a range of network layer protcols,
 * including the TCP socket connections implemented here.
 */

#ifndef GMOS_TCPIP_LINKS_H
#define GMOS_TCPIP_LINKS_H

#include <stdint.h>
#include <stdbool.h>
#include "gmos-network.h"
#include "gmos-network-links.h"
#include "gmos-tcpip-config.h"
#include "gmos-tcpip-stack.h"

/**
 * Define the protocol specific data structure for TCP based network
 * links.
 */
typedef struct gmosTcpipLink_t {

    // Include the common network link data structure.
    gmosNetworkLink_t networkLink;

    // Specify the TCP/IP stack to use for the link.
    gmosTcpipStack_t* tcpipStack;

    // Specify the TCP socket used for the link connection.
    gmosTcpipStackSocket_t* tcpSocket;

    // Specify the remote DNS name used for the link connection.
    const char* remoteDnsName;

    // Implement the TCP link worker task that is used for opening and
    // closing TCP connections.
    gmosTaskState_t workerTask;

    // Specify the remote IP address used for the link connection.
    uint8_t remoteIpAddr [GMOS_CONFIG_TCPIP_DNS_MAX_ADDR_SIZE];

    // Specify the remote IP port to use for the connection.
    uint16_t remoteIpPort;

    // Specify the local IP port to use for the connection.
    uint16_t localIpPort;

    // Specify whether the link should use IPv6.
#if GMOS_CONFIG_TCPIP_IPV6_ENABLE
    uint8_t useIpv6;
#endif

    // Specify the current TCP/IP link state.
    uint8_t linkState;

} gmosTcpipLink_t;

/**
 * Initialises a TCP link instance on startup.
 * @param tcpipLink This is the TCP link instance that is to be
 *     initialised.
 * @param tcpipStack This is the TCP/IP stack which is to be used as
 *     the link transport.
 * @param useIpv6 This is a boolean flag which when set to 'true'
 *     selects IPv6 operation and when set to 'false' selects IPv4
 *     operation.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully initialising the TCP link state and 'false'
 *     otherwise.
 */
bool gmosTcpipLinkInit (gmosTcpipLink_t* tcpipLink,
    gmosTcpipStack_t* tcpipStack, bool useIpv6);

/**
 * Configures a TCP link using a DNS host name to identify the remote
 * server.
 * @param tcpipLink This is the TCP link instance that is to be
 *     configured.
 * @param remoteDnsName This is a pointer to the DNS host name for the
 *     remote server. The referenced name should remain valid for the
 *     lifetime of the link connection or until a new link configuration
 *     is assigned.
 * @param remoteIpPort This is the IP port on the remote server which
 *     is to be used when establishing the TCP link connection.
 * @param localIpPort This is the IP port on the local interface which
 *     is to be used when establishing the TCP link connection.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully configuring the TCP link and 'false' otherwise.
 */
bool gmosTcpipLinkConfigureDnsName (gmosTcpipLink_t* tcpipLink,
    const char* remoteDnsName, uint16_t remoteIpPort,
    uint16_t localIpPort);

/**
 * Configures a TCP link using a fixed IP address to identify the remote
 * server.
 * @param tcpipLink This is the TCP link instance that is to be
 *     configured.
 * @param remoteIpAddr This is a pointer to a four octet IPv4 address or
 *     a sixteen octet IPv6 address, depending on whether IPv6 operation
 *     was selected for the link during initialisation. The referenced
 *     address will be copied to local storage and does not need to
 *     remain valid after this function returns.
 * @param remoteIpPort This is the IP port on the remote server which
 *     is to be used when establishing the TCP link connection.
 * @param localIpPort This is the IP port on the local interface which
 *     is to be used when establishing the TCP link connection.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully configuring the TCP link and 'false' otherwise.
 */
bool gmosTcpipLinkConfigureFixedIp (gmosTcpipLink_t* tcpipLink,
    const uint8_t* remoteIpAddr, uint16_t remoteIpPort,
    uint16_t localIpPort);

#endif // GMOS_TCPIP_LINKS_H
