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
 * vendor supplied and hardware accelerated TCP/IP implementations. The
 * underlying TCP/IP hardware or vendor library is assumed to support
 * the following protocols: IPv4 and/or IPv6, TCP, UDP, ARP and ICMP.
 */

#ifndef GMOS_DRIVER_TCPIP_H
#define GMOS_DRIVER_TCPIP_H

#include <stdint.h>
#include <stdbool.h>
#include "gmos-buffers.h"
#include "gmos-network.h"
#include "gmos-tcpip-stack.h"

/**
 * Defines the TCP/IP implementation specific I/O state data structure.
 * The full type definition must be provided by the associated TCP/IP
 * network abstraction layer.
 */
typedef struct gmosNalTcpipState_t gmosNalTcpipState_t;

/**
 * Defines the TCP/IP implementation specific I/O configuration options.
 * The full type definition must be provided by the associated TCP/IP
 * network abstraction layer.
 */
typedef struct gmosNalTcpipConfig_t gmosNalTcpipConfig_t;

/**
 * Defines the TCP/IP implementation specific socket data structure. The
 * full type definition must be provided by the associated TCP/IP
 * network abstraction layer. The first item in the data structure must
 * always be the common TCP/IP socket data structure, allowing socket
 * pointers to be freely cast between standard and implementation
 * specific socket types.
 */
typedef struct gmosNalTcpipSocket_t gmosNalTcpipSocket_t;

/**
 * Defines the GubbinsMOS TCP/IP implementation state and networking
 * driver data structure that is used for managing the low level TCP/IP
 * networking protocols.
 */
typedef struct gmosDriverTcpip_t {

    // This is an opaque pointer to the TCP/IP network abstraction layer
    // data structure that is used for accessing the vendor specific
    // TCP/IP implementation and associated network driver hardware. The
    // data structure will be implementation specific.
    gmosNalTcpipState_t* nalData;

    // This is an opaque pointer to the TCP/IP network abstraction layer
    // configuration data structure that is used for setting up the
    // TCP/IP implementation. The data structure will be implementation
    // specific.
    const gmosNalTcpipConfig_t* nalConfig;

} gmosDriverTcpip_t;

/**
 * Provides a network hardware configuration setup macro to be used when
 * allocating a TCP/IP driver I/O data structure. Assigning this macro
 * to a TCP/IP driver I/O data structure on declaration will configure
 * the driver to use the network specific configuration. Refer to the
 * vendor specific TCP/IP implementation for full details of the
 * configuration options.
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
 * @param tcpipDriver This is the TCP/IP driver data structure that
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
    gmosDriverTcpip_t* tcpipDriver, const uint8_t* ethMacAddr);

/**
 * Resets the TCP/IP driver, forcing all sockets to close and clearing
 * all previously configured network settings. This is typically called
 * whenever the lease expires on a DHCP address allocation, after which
 * the local IP address is no longer valid.
 * @param tcpipDriver This is the TCP/IP driver instance that is to be
 *     reset.
 * @return Returns a boolean value which will be set to 'true' if the
 *     TCP/IP driver was successfully reset and 'false' if the reset
 *     process has not yet completed.
 */
bool gmosDriverTcpipReset (gmosDriverTcpip_t* tcpipDriver);

/**
 * Update the IPv4 network address and associated network parameters
 * that are to be used by the TCP/IP network abstraction layer.
 * @param tcpipDriver This is the TCP/IP driver data structure that
 *     represents the TCP/IP driver being configured.
 * @param interfaceAddr This is the four octet IPv4 address of the local
 *     interface, encoded as a 32-bit integer in network byte order.
 * @param gatewayAddr This is the four octet address of the IPv4 gateway
 *     on the local subnet, encoded as a 32-bit integer in network byte
 *     order.
 * @param subnetMask This is the IPv4 subnet mask to be used, encoded
 *     in bit mask form as a 32-bit integer in network byte order.
 * @return Returns a boolean value which will be set to 'true' if the
 *     network information was successfully updated and 'false'
 *     otherwise.
 */
bool gmosDriverTcpipSetNetworkInfoIpv4 (
    gmosDriverTcpip_t* tcpipDriver, uint32_t interfaceAddr,
    uint32_t gatewayAddr, uint32_t subnetMask);

/**
 * Update the IPv6 network address and associated network parameters
 * that are to be used by the TCP/IP network abstraction layer.
 * @param tcpipDriver This is the TCP/IP driver data structure that
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
    gmosDriverTcpip_t* tcpipDriver, const uint8_t* interfaceAddr,
    const uint8_t* gatewayAddr, const uint8_t subnetMask);

/**
 * Attempts to open a new UDP socket for subsequent use.
 * @param tcpipDriver This is the TCP/IP driver instance for which the
 *     UDP socket is being opened.
 * @param useIpv6 If this flag is set and the TCP/IP driver supports
 *     IPv6, the UDP socket will be opened as an IPv6 socket. Otherwise
 *     an IPv4 socket will be opened.
 * @param localPort This is the local port to be used when sending and
 *     receiving UDP datagrams.
 * @param appTask This is the application layer task which will be used
 *     to process received data and driver status updates.
 * @param notifyHandler This is a notification callback handler that
 *     will be used to notify the socket client of any TCP/IP driver
 *     events associated with the socket. A null reference may be used
 *     to indicate that no notification callbacks are required.
 * @param notifyData This is a pointer to an opaque data item that will
 *     be included in all driver notification callbacks.
 * @return Returns a pointer to the UDP socket data structure or a null
 *     reference if no UDP socket instance is currently available.
 */
gmosNalTcpipSocket_t* gmosDriverTcpipUdpOpen (
    gmosDriverTcpip_t* tcpipDriver, bool useIpv6,
    uint16_t localPort, gmosTaskState_t* appTask,
    gmosTcpipStackNotifyCallback_t notifyHandler, void* notifyData);

/**
 * Sends a UDP datagram to the specified remote IP address using an
 * opened UDP socket.
 * @param udpSocket This is the opened UDP socket that should be used
 *     for sending the UDP datagram.
 * @param remoteAddr This is a pointer to the address of the remote
 *     device to which the UDP datagram will be sent, expressed in
 *     network byte order. For IPv4 sockets this will be a four octet
 *     address and for IPv6 sockets this will be a sixteen octet
 *     address.
 * @param remotePort This is the port on the remote device to which the
 *     UDP datagram will be sent.
 * @param payload This is a pointer to a GubbinsMOS buffer that contains
 *     the UDP datagram payload. On successful completion, the buffer
 *     contents will automatically be released.
 * @return Returns a network status value which will indicate that the
 *     UDP datagram was successfully queued for transmission or which
 *     will specify the reason for failure.
 */
gmosNetworkStatus_t gmosDriverTcpipUdpSendTo (
    gmosNalTcpipSocket_t* udpSocket, uint8_t* remoteAddr,
    uint16_t remotePort, gmosBuffer_t* payload);

/**
 * Receives a UDP datagram from a remote IP address using an opened UDP
 * socket.
 * @param udpSocket This is the opened UDP socket that should be used
 *     for receiving the UDP datagram.
 * @param remoteAddr This is a pointer to the address of the remote
 *     device from which the UDP datagram will be sent, expressed in
 *     network byte order. On successful packet reception, it will be
 *     populated with the source address of the received UDP datagram.
 *     For IPv4 sockets this must be a four octet array and for IPv6
 *     sockets this must be a sixteen octet array.
 * @param remotePort This is a pointer to the port on the remote device
 *     from which the UDP datagram was received. On successful packet
 *     reception, it will be populated with the source port of the
 *     received UDP datagram.
 * @param payload This is a pointer to a GubbinsMOS buffer that will
 *     receive the UDP datagram payload. On successful completion,
 *     any existing buffer contents will be discarded and replaced with
 *     the UDP datagram payload contents.
 * @return Returns a network status value which will indicate that the
 *     UDP datagram was successfully received or which will specify the
 *     reason for failure.
 */
gmosNetworkStatus_t gmosDriverTcpipUdpReceiveFrom (
    gmosNalTcpipSocket_t* udpSocket, uint8_t* remoteAddr,
    uint16_t* remotePort, gmosBuffer_t* payload);

/**
 * Closes the specified UDP socket, releasing all allocated resources.
 * @param udpSocket This is the UDP socket for which the socket close
 *     request is being initiated.
 * @return Returns a network status value indicating success or the
 *     reason for failure.
 */
gmosNetworkStatus_t gmosDriverTcpipUdpClose (
    gmosNalTcpipSocket_t* udpSocket);

/**
 * Attempts to open a new TCP socket for subsequent use.
 * @param tcpipDriver This is the TCP/IP driver instance for which the
 *     TCP socket is being opened.
 * @param useIpv6 If this flag is set and the TCP/IP implementation
 *     supports IPv6, the TCP socket will be opened as an IPv6 socket.
 *     Otherwise an IPv4 socket will be opened.
 * @param localPort This is the local port to be used when establishing
 *     a TCP connection.
 * @param appTask This is the application layer task which will be used
 *     to process received data and driver status updates.
 * @param notifyHandler This is a notification callback handler that
 *     will be used to notify the socket client of any TCP/IP driver
 *     events associated with the socket. A null reference may be used
 *     to indicate that no notification callbacks are required.
 * @param notifyData This is a pointer to an opaque data item that will
 *     be included in all driver notification callbacks.
 * @return Returns a pointer to the TCP socket data structure or a null
 *     reference if no TCP socket instance is currently available.
 */
gmosNalTcpipSocket_t* gmosDriverTcpipTcpOpen (
    gmosDriverTcpip_t* tcpipDriver, bool useIpv6,
    uint16_t localPort, gmosTaskState_t* appTask,
    gmosTcpipStackNotifyCallback_t notifyHandler, void* notifyData);

/**
 * Initiates the TCP connection process as a TCP client, using the
 * specified server address and port.
 * @param tcpSocket This is the TCP socket for which the connection
 *     request is being initiated.
 * @param serverAddr This is a pointer to the server address to which
 *     the client should connect, expressed in network byte order. For
 *     IPv4 sockets this will be a four octet address and for IPv6
 *     sockets this will be a sixteen octet address.
 * @param serverPort This is the server port number to be used for the
 *     connection.
 * @return Returns a network status value indicating success or the
 *     reason for failure.
 */
gmosNetworkStatus_t gmosDriverTcpipTcpConnect (
    gmosNalTcpipSocket_t* tcpSocket,
    uint8_t* serverAddr, uint16_t serverPort);

/**
 * Sets up the TCP socket as a server for accepting TCP connection
 * requests, using the specified local port.
 * @param tcpSocket This is the TCP socket for which the server bind
 *     request is being initiated.
 * @param serverPort This is the server port number to be used for the
 *     inbound connections.
 * @return Returns a network status value indicating success or the
 *     reason for failure.
 */
gmosNetworkStatus_t gmosDriverTcpipTcpBind (
    gmosNalTcpipSocket_t* tcpSocket, uint16_t serverPort);

/**
 * Sends the contents of a GubbinsMOS buffer over an established TCP
 * connection.
 * @param tcpSocket This is the TCP socket which is to be used to send
 *     the payload data.
 * @param payload This is a pointer to a GubbinsMOS buffer that contains
 *     the data to be transmitted over the TCP connection. On successful
 *     completion, the buffer contents will automatically be released.
 * @return Returns a network status value indicating success or the
 *     reason for failure.
 */
gmosNetworkStatus_t gmosDriverTcpipTcpSend (
    gmosNalTcpipSocket_t* tcpSocket, gmosBuffer_t* payload);

/**
 * Receives a block of data over an established TCP connection.
 * @param tcpSocket This is the TCP socket which is to be used to
 *     receive the payload data.
 * @param payload This is a pointer to a GubbinsMOS buffer that will
 *     be populated with data received over the TCP connection. On
 *     successful completion, any existing buffer contents will be
 *     discarded and replaced with the TCP payload data.
 * @return Returns a network status value indicating success or the
 *     reason for failure.
 */
gmosNetworkStatus_t gmosDriverTcpipTcpReceive (
    gmosNalTcpipSocket_t* tcpSocket, gmosBuffer_t* payload);

/**
 * Closes the specified TCP socket, terminating any active connection
 * and releasing all allocated resources.
 * @param tcpSocket This is the TCP socket for which the socket close
 *     request is being initiated.
 * @return Returns a network status value indicating success or the
 *     reason for failure.
 */
gmosNetworkStatus_t gmosDriverTcpipTcpClose (
    gmosNalTcpipSocket_t* tcpSocket);

/**
 * Determines if the underlying physical layer link is ready to
 * transport TCP/IP traffic.
 * @param tcpipDriver This is the TCP/IP driver data structure that
 *     represents the TCP/IP driver being interrogated.
 * @return Returns a boolean value which will be set to 'true' if the
 *     physical layer link is ready to transport TCP/IP traffic and
 *     'false' otherwise.
 */
bool gmosDriverTcpipPhyLinkIsUp (gmosDriverTcpip_t* tcpipDriver);

/**
 * Accesses the 48-bit Ethernet MAC address for the TCP/IP driver.
 * @param tcpipDriver This is the TCP/IP driver data structure that
 *     represents the TCP/IP driver being accessed.
 * @return Returns a pointer to a six octet array that contains the
 *     48-bit Ethernet MAC address in network byte order.
 */
uint8_t* gmosDriverTcpipGetMacAddr (gmosDriverTcpip_t* tcpipDriver);

#endif // GMOS_DRIVER_TCPIP_H
