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
 * This header provides the common driver framework for supporting
 * vendor supplied and hardware accelerated TCP/IP stacks. The
 * underlying TCP/IP stack is assumed to support the following
 * protocols: IPv4 and/or IPv6, TCP, UDP, ARP and ICMP.
 */

#ifndef GMOS_DRIVER_TCPIP_H
#define GMOS_DRIVER_TCPIP_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Defines the TCP/IP stack specific I/O state data structure. The full
 * type definition must be provided by the associated TCP/IP network
 * abstraction layer.
 */
typedef struct gmosNalTcpipState_t gmosNalTcpipState_t;

/**
 * Defines the TCP/IP stack specific I/O configuration options. The full
 * type definition must be provided by the associated TCP/IP network
 * abstraction layer.
 */
typedef struct gmosNalTcpipConfig_t gmosNalTcpipConfig_t;

/**
 * Defines the GubbinsMOS TCP/IP stack state and networking driver data
 * structure that is used for managing the low level TCP/IP networking
 * protocols.
 */
typedef struct gmosDriverTcpip_t {

    // This is an opaque pointer to the TCP/IP network abstraction layer
    // data structure that is used for accessing the vendor specific
    // TCP/IP stack and associated network driver hardware. The data
    // structure will be TCP/IP stack vendor specific.
    gmosNalTcpipState_t* nalData;

    // This is an opaque pointer to the TCP/IP network abstraction layer
    // configuration data structure that is used for setting up the
    // TCP/IP stack. The data structure will be TCP/IP stack vendor
    // specific.
    const gmosNalTcpipConfig_t* nalConfig;

} gmosDriverTcpip_t;

/**
 * Provides a network hardware configuration setup macro to be used when
 * allocating a TCP/IP stack driver I/O data structure. Assigning this
 * macro to a TCP/IP driver I/O data structure on declaration will
 * configure the driver to use the network specific configuration.
 * Refer to the hardware specific TCP/IP implementation for full details
 * of the configuration options.
 * @param _nalData_ This is the network interface abstraction layer
 *     data structure that is to be used for accessing the interface
 *     specific hardware.
 * @param _ralConfig_ This is a network interface specific configuration
 *     data structure that defines a set of fixed configuration options
 *     to be used with the network interface.
 */
#define GMOS_DRIVER_TCPIP_NAL_CONFIG(_nalData_, _nalConfig_)           \
    { _nalData_, _nalConfig_ }

/**
 * Initialise the TCP/IP driver on startup, using the supplied network
 * settings.
 * @param tcpipStack This is the TCP/IP stack data structure that
 *     represents the TCP/IP driver being initialised.
 * @param ethMacAddr This is a pointer to the 48-bit Ethernet MAC
 *     address which is to be assigned to the network interface, stored
 *     as an array of six octets in network byte order. A null reference
 *     may be passed for low level interfaces that include their own
 *     hardcoded Ethernet MAC addresses.
 * @return Returns a boolean value which will be set to 'true' if the
 *     network abstraction layer was successfully initialised and
 *     'false' otherwise.
 */
bool gmosDriverTcpipInit (
    gmosDriverTcpip_t* tcpipStack, const uint8_t* ethMacAddr);

/**
 * Update the IPv4 network address and associated network parameters
 * that are to be used by the TCP/IP network abstraction layer.
 * @param tcpipStack This is the TCP/IP stack data structure that
 *     represents the TCP/IP driver being configured.
 * @param interfaceAddr This is a pointer to the four octet IPv4 address
 *     of the local interface, stored in network byte order.
 * @param gatewayAddr This is a pointer to the four octet address of the
 *     IPv4 gateway on the local subnet, stored in network byte order.
 * @param subnetMask This is the IPv4 subnet mask to be used, expressed
 *     in network byte order bit mask form.
 * @return Returns a boolean value which will be set to 'true' if the
 *     network information was successfully updated and 'false'
 *     otherwise.
 */
bool gmosDriverTcpipSetNetworkInfoIpv4 (
    gmosDriverTcpip_t* tcpipStack, const uint8_t* interfaceAddr,
    const uint8_t* gatewayAddr, const uint8_t* subnetMask);

/**
 * Update the IPv6 network address and associated network parameters
 * that are to be used by the TCP/IP network abstraction layer.
 * @param tcpipStack This is the TCP/IP stack data structure that
 *     represents the TCP/IP driver being configured.
 * @param interfaceAddr This is a pointer to the sixteen octet IPv6
 *     address of the local interface, stored in network byte order.
 * @param gatewayAddr This is a pointer to the sixteen octet address of
 *     the IPv6 gateway on the local subnet, stored in network byte
 *     order.
 * @param subnetMask This is the IPv6 subnet mask to be used, expressed
 *     in prefix form.
 * @return Returns a boolean value which will be set to 'true' if the
 *     network information was successfully updated and 'false'
 *     otherwise.
 */
bool gmosDriverTcpipSetNetworkInfoIpv6 (
    gmosDriverTcpip_t* tcpipStack, const uint8_t* interfaceAddr,
    const uint8_t* gatewayAddr, const uint8_t subnetMask);

/**
 * Determines if the underlying physical layer link is ready to
 * transport TCP/IP traffic.
 * @param tcpipStack This is the TCP/IP stack data structure that
 *     represents the TCP/IP driver being interrogated.
 * @return Returns a boolean value which will be set to 'true' if the
 *     physical layer link is ready to transport TCP/IP traffic and
 *     'false' otherwise.
 */
bool gmosDriverTcpipPhyLinkIsUp (gmosDriverTcpip_t* tcpipStack);

/**
 * Accesses the 48-bit Ethernet MAC address for the TCP/IP driver.
 * @param tcpipStack This is the TCP/IP stack data structure that
 *     represents the TCP/IP driver being accessed.
 * @return Returns a pointer to a six octet array that contains the
 *     48-bit Ethernet MAC address in network byte order.
 */
uint8_t* gmosDriverTcpipGetMacAddr (gmosDriverTcpip_t* tcpipStack);

#endif // GMOS_DRIVER_TCPIP_H
