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
 * This header provides the common API for using IPv4 DHCP client setup
 * with vendor supplied and hardware accelerated TCP/IP stacks.
 */

#ifndef GMOS_TCPIP_DHCP_H
#define GMOS_TCPIP_DHCP_H

#include <stdint.h>
#include <stdbool.h>
#include "gmos-scheduler.h"
#include "gmos-driver-tcpip.h"
#include "gmos-tcpip-stack.h"

/**
 * Defines the GubbinsMOS TCP/IP stack DHCP client state that is used
 * for managing dynamic host information on a single TCP/IP interface.
 */
typedef struct gmosTcpipDhcpClient_t {

    // Specify the TCP/IP stack instance to use for the DHCP client.
    gmosDriverTcpip_t* tcpipStack;

    // Specify the DHCP host name to be used.
    const char* dhcpHostName;

    // Specify the UDP socket currently in use by the DHCP client.
    gmosTcpipStackSocket_t* udpSocket;

    // Allocate the DHCP protocol worker task data structure.
    gmosTaskState_t dhcpWorkerTask;

    // Specify the DHCP lease time using the system timer.
    uint32_t leaseTime;

    // Specify the latest DHCP lease end time using the system timer.
    uint32_t leaseEnd;

    // Specify the current DHCP timestamp using the system timer.
    uint32_t timestamp;

    // Specify the XID DHCP sequence number to be used.
    uint32_t dhcpXid;

    // Specify the current DHCP server address in network byte order.
    uint32_t dhcpServerAddr;

    // Specify the primary DNS server address in network byte order.
    uint32_t dns1ServerAddr;

    // Specify the secondary DNS server address in network byte order.
    uint32_t dns2ServerAddr;

    // Specify the current assigned address in network byte order.
    uint32_t assignedAddr;

    // Specify the current gateway router address in network byte order.
    uint32_t gatewayAddr;

    // Specify the current subnet mask in network byte order.
    uint32_t subnetMask;

    // Specify the current DHCP operating state.
    uint8_t dhcpState;

} gmosTcpipDhcpClient_t;

/**
 * Initialise the DHCP client on startup, using the specified TCP/IP
 * interface.
 * @param dhcpClient This is a pointer to the DHCP client data
 *     structure that should be used for storing the DHCP client state.
 * @param tcpipStack This is an initialised TCP/IP stack data structure
 *     that represents the TCP/IP interface to be used by the DHCP
 *     client.
 * @param dhcpHostName This should be a pointer to a unique host name
 *     string that allows the device to be identified in the DHCP server
 *     tables. It must be valid for the lifetime of the device.
 * @return Returns a boolean value which will be set to 'true' if the
 *     DHCP client was successfully initialised and 'false' otherwise.
 */
bool gmosTcpipDhcpClientInit (gmosTcpipDhcpClient_t* dhcpClient,
    gmosDriverTcpip_t* tcpipStack, const char* dhcpHostName);

/**
 * Determines if the DHCP client has successfully obtained a valid IP
 * address and network configuration.
 * @param dhcpClient This is a pointer to the DHCP client data
 *     structure that is used for storing the DHCP client state.
 * @return Returns a boolean value which will be set to 'true' if the
 *     DHCP client has obtained a valid IP address and 'false'
 *     otherwise.
 */
bool gmosTcpipDhcpClientReady (gmosTcpipDhcpClient_t* dhcpClient);

#endif // GMOS_TCPIP_DHCP_H
