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
 * This header provides the common API for supporting vendor supplied
 * and hardware accelerated TCP/IP stacks. The underlying TCP/IP stack
 * is assumed to support the following protocols: IPv4 and/or IPv6, TCP,
 * UDP, ARP and ICMP.
 */

#ifndef GMOS_TCPIP_STACK_H
#define GMOS_TCPIP_STACK_H

#include <stdint.h>
#include <stdbool.h>
#include "gmos-config.h"
#include "gmos-buffers.h"
#include "gmos-scheduler.h"
#include "gmos-driver-tcpip.h"

/**
 * This enumeration specifies the various status values which may be
 * reported by the TCP/IP stack.
 */
typedef enum {

    // Indicates successful completion of a TCP/IP stack operation.
    GMOS_TCPIP_STACK_STATUS_SUCCESS,

    // Indicates that a TCP socket is already connected.
    GMOS_TCPIP_STACK_STATUS_CONNECTED,

    // Indicates that a TCP socket is not connected.
    GMOS_TCPIP_STACK_STATUS_NOT_CONNECTED,

    // Indicates that a request can not be completed because the
    // specified socket was not open for the required protocol.
    GMOS_TCPIP_STACK_STATUS_NOT_OPEN,

    // Indicates that a request is not valid, usually due to invalid or
    // malformed parameters.
    GMOS_TCPIP_STACK_STATUS_NOT_VALID,

    // Indicate that an operation can not be completed at this time,
    // but may be retried later.
    GMOS_TCPIP_STACK_STATUS_RETRY,

    // Indicates that a UDP datagram or TCP data buffer is too large for
    // transmission by the stack. This will normally be as a result of
    // hardware buffer size limitations rather than exceeding a protocol
    // imposed limit.
    GMOS_TCPIP_STACK_STATUS_OVERSIZED,

    // Indicates that the TCP/IP network connection is down. This may
    // be due to a loss of local connectivity or lack of valid DHCP
    // settings.
    GMOS_TCPIP_STACK_STATUS_NETWORK_DOWN,

    // Indicates that the TCP/IP network transaction timed out.
    GMOS_TCPIP_STACK_STATUS_TIMEOUT

}
gmosTcpipStackStatus_t;

/**
 * This enumeration specifies the various TCP/IP notifications that may
 * be sent via the notification callback handlers.
 */
typedef enum {

    // Indicates that the local PHY link has been reconnected.
    GMOS_TCPIP_STACK_NOTIFY_PHY_LINK_UP,

    // Indicates that the local PHY link has been disconnected.
    GMOS_TCPIP_STACK_NOTIFY_PHY_LINK_DOWN,

    // Indicates that the UDP socket opening process has completed.
    GMOS_TCPIP_STACK_NOTIFY_UDP_SOCKET_OPENED,

    // Indicates that the UDP socket has been closed and it may no
    // longer be used.
    GMOS_TCPIP_STACK_NOTIFY_UDP_SOCKET_CLOSED,

    // Indicates that transmission of a UDP datagram has completed.
    GMOS_TCPIP_STACK_NOTIFY_UDP_MESSAGE_SENT,

    // Indicates that an ARP request timeout occured when attempting
    // to send a UDP datagram.
    GMOS_TCPIP_STACK_NOTIFY_UDP_ARP_TIMEOUT,

    // Indicates that the TCP socket opening process has completed.
    GMOS_TCPIP_STACK_NOTIFY_TCP_SOCKET_OPENED,

    // Indicates that the TCP socket has been closed and it may no
    // longer be used.
    GMOS_TCPIP_STACK_NOTIFY_TCP_SOCKET_CLOSED,

    // Indicates that an ARP request timeout or a TCP request timeout
    // occurred when attempting to establish a TCP connection.
    GMOS_TCPIP_STACK_NOTIFY_TCP_CONNECT_TIMEOUT

}
gmosTcpipStackNotify_t;

/**
 * Defines the stack specific TCP/IP socket data structure. The full
 * type definition must be provided by the associated TCP/IP network
 * abstraction layer.
 */
typedef struct gmosTcpipStackSocket_t gmosTcpipStackSocket_t;

/**
 * Specifies the function prototype used for asynchronous TCP/IP stack
 * notification callbacks.
 * @param notifyData This is the opaque notification data item which
 *     was registered with the notification callback handler.
 * @param notification This is the the TCP/IP notification that is being
 *     sent via the callback.
 */
typedef void (*gmosTcpipStackNotifyCallback_t) (
    void* notifyData, gmosTcpipStackNotify_t notification);

/**
 * Attempts to open a new UDP socket for subsequent use.
 * @param tcpipStack This is the TCP/IP stack driver instance for which
 *     the UDP socket is being opened.
 * @param useIpv6 If this flag is set and the TCP/IP stack supports
 *     IPv6, the UDP socket will be opened as an IPv6 socket. Otherwise
 *     an IPv4 socket will be opened.
 * @param localPort This is the local port to be used when sending and
 *     receiving UDP datagrams.
 * @param appTask This is the application layer task which will be used
 *     to process received data and stack status updates.
 * @param notifyHandler This is a notification callback handler that
 *     will be used to notify the socket client of any TCP/IP stack
 *     events associated with the socket. A null reference may be used
 *     to indicate that no notification callbacks are required.
 * @param notifyData This is a pointer to an opaque data item that will
 *     be included in all stack notification callbacks.
 * @return Returns a pointer to the UDP socket data structure or a null
 *     reference if no UDP socket instance is currently available.
 */
gmosTcpipStackSocket_t* gmosTcpipStackUdpOpen (
    gmosDriverTcpip_t* tcpipStack, bool useIpv6,
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
 * @return Returns a status value which will indicate that the UDP
 *     datagram was successfully queued for transmission or will specify
 *     the reason for failure.
 */
gmosTcpipStackStatus_t gmosTcpipStackUdpSendTo (
    gmosTcpipStackSocket_t* udpSocket, uint8_t* remoteAddr,
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
 * @return Returns a status value which will indicate that the UDP
 *     datagram was successfully received or will specify the reason
 *     for failure.
 */
gmosTcpipStackStatus_t gmosTcpipStackUdpReceiveFrom (
    gmosTcpipStackSocket_t* udpSocket, uint8_t* remoteAddr,
    uint16_t* remotePort, gmosBuffer_t* payload);

/**
 * Closes the specified UDP socket, releasing all allocated resources.
 * @param udpSocket This is the UDP socket for which the socket close
 *     request is being initiated.
 * @return Returns a TCP/IP stack status value indicating success or
 *     the reason for failure.
 */
gmosTcpipStackStatus_t gmosTcpipStackUdpClose (
    gmosTcpipStackSocket_t* udpSocket);

/**
 * Attempts to open a new TCP socket for subsequent use.
 * @param tcpipStack This is the TCP/IP stack driver instance for which
 *     the TCP socket is being opened.
 * @param useIpv6 If this flag is set and the TCP/IP stack supports
 *     IPv6, the TCP socket will be opened as an IPv6 socket. Otherwise
 *     an IPv4 socket will be opened.
 * @param localPort This is the local port to be used when establishing
 *     a TCP connection.
 * @param appTask This is the application layer task which will be used
 *     to process received data and stack status updates.
 * @param notifyHandler This is a notification callback handler that
 *     will be used to notify the socket client of any TCP/IP stack
 *     events associated with the socket. A null reference may be used
 *     to indicate that no notification callbacks are required.
 * @param notifyData This is a pointer to an opaque data item that will
 *     be included in all stack notification callbacks.
 * @return Returns a pointer to the TCP socket data structure or a null
 *     reference if no TCP socket instance is currently available.
 */
gmosTcpipStackSocket_t* gmosTcpipStackTcpOpen (
    gmosDriverTcpip_t* tcpipStack, bool useIpv6,
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
 * @return Returns a TCP/IP stack status value indicating success or
 *     the reason for failure.
 */
gmosTcpipStackStatus_t gmosTcpipStackTcpConnect (
    gmosTcpipStackSocket_t* tcpSocket,
    uint8_t* serverAddr, uint16_t serverPort);

/**
 * Sets up the TCP socket as a server for accepting TCP connection
 * requests, using the specified local port.
 * @param tcpSocket This is the TCP socket for which the server bind
 *     request is being initiated.
 * @param serverPort This is the server port number to be used for the
 *     inbound connections.
 * @return Returns a TCP/IP stack status value indicating success or
 *     the reason for failure.
 */
gmosTcpipStackStatus_t gmosTcpipStackTcpBind (
    gmosTcpipStackSocket_t* tcpSocket, uint16_t serverPort);

/**
 * Sends the contents of a GubbinsMOS buffer over an established TCP
 * connection.
 * @param tcpSocket This is the TCP socket which is to be used to send
 *     the payload data.
 * @param payload This is a pointer to a GubbinsMOS buffer that contains
 *     the data to be transmitted over the TCP connection. On successful
 *     completion, the buffer contents will automatically be released.
 * @return Returns a TCP/IP stack status value indicating success or
 *     the reason for failure.
 */
gmosTcpipStackStatus_t gmosTcpipStackTcpSend (
    gmosTcpipStackSocket_t* tcpSocket, gmosBuffer_t* payload);

/**
 * Attempts to write an array of octet data to an established TCP
 * connection.
 * @param tcpSocket This is the TCP socket which is to be used to send
 *     the payload data.
 * @param writeData This is a pointer to the octet array that is to be
 *     written to the TCP connection.
 * @param requestSize This is the length of the octet array that should
 *     be written to the TCP connection.
 * @param transferSize This is a pointer to the transfer size value
 *     which will be updated with the actual number of octets written
 *     to the TCP connection.
 * @return Returns a TCP/IP stack status value indicating success or
 *     the reason for failure.
 */
gmosTcpipStackStatus_t gmosTcpipStackTcpWrite (
    gmosTcpipStackSocket_t* tcpSocket, uint8_t* writeData,
    uint16_t requestSize, uint16_t* transferSize);

/**
 * Receives a block of data over an established TCP connection.
 * @param tcpSocket This is the TCP socket which is to be used to
 *     receive the payload data.
 * @param payload This is a pointer to a GubbinsMOS buffer that will
 *     be populated with data received over the TCP connection. On
 *     successful completion, any existing buffer contents will be
 *     discarded and replaced with the TCP payload data.
 * @return Returns a TCP/IP stack status value indicating success or
 *     the reason for failure.
 */
gmosTcpipStackStatus_t gmosTcpipStackTcpReceive (
    gmosTcpipStackSocket_t* tcpSocket, gmosBuffer_t* payload);

/**
 * Attempts to read an array of octet data from an established TCP
 * connection.
 * @param tcpSocket This is the TCP socket which is to be used to
 *     receive the payload data.
 * @param readData This is a pointer to the octet array that is to be
 *     updated with the data read from the TCP connection.
 * @param requestSize This is the length of the octet array that may be
 *     used to store received data from the TCP connection.
 * @param transferSize This is a pointer to the transfer size value
 *     which will be updated with the actual number of octets read
 *     to the TCP connection.
 * @return Returns a TCP/IP stack status value indicating success or
 *     the reason for failure.
 */
gmosTcpipStackStatus_t gmosTcpipStackTcpRead (
    gmosTcpipStackSocket_t* tcpSocket, uint8_t* readData,
    uint16_t requestSize, uint16_t* transferSize);

/**
 * Closes the specified TCP socket, terminating any active connection
 * and releasing all allocated resources.
 * @param tcpSocket This is the TCP socket for which the socket close
 *     request is being initiated.
 * @return Returns a TCP/IP stack status value indicating success or
 *     the reason for failure.
 */
gmosTcpipStackStatus_t gmosTcpipStackTcpClose (
    gmosTcpipStackSocket_t* tcpSocket);

/**
 * Provides support for 32-bit byte reversal.
 * @param value This is the 32-bit value that is to be byte reversed.
 * @return result This is the integer value after performing byte
 *    reversal.
 */
static inline uint32_t gmosTcpipStackByteReverseU32 (uint32_t value)
{
    return ((value >> 24) & 0x000000FF) | ((value >> 8) & 0x0000FF00) |
        ((value << 8) & 0x00FF0000) | ((value << 24) & 0xFF000000);
}

/**
 * Provides support for 16-bit byte reversal.
 * @param value This is the 16-bit value that is to be byte reversed.
 * @return result This is the integer value after performing byte
 *    reversal.
 */
static inline uint16_t gmosTcpipStackByteReverseU16 (uint16_t value)
{
    return ((value >> 8) & 0x00FF) | ((value << 8) & 0xFF00);
}

/**
 * Provides a macro for converting 32-bit integer values from native
 * host representation to network byte order.
 * @param _hostLong_ This is the 32-bit host native integer value.
 * @return Returns a 32-bit integer value in network byte order.
 */
#if GMOS_CONFIG_HOST_BIG_ENDIAN
#define GMOS_TCPIP_STACK_HTONL(_hostLong_) \
    (_hostLong_)
#else
#define GMOS_TCPIP_STACK_HTONL(_hostLong_) \
    (gmosTcpipStackByteReverseU32 (_hostLong_))
#endif

/**
 * Provides a macro for converting 16-bit integer values from native
 * host representation to network byte order.
 * @param _hostShort_ This is the 16-bit host native integer value.
 * @return Returns a 32-bit integer value in network byte order.
 */
#if GMOS_CONFIG_HOST_BIG_ENDIAN
#define GMOS_TCPIP_STACK_HTONS(_hostShort_) \
    (_hostShort_)
#else
#define GMOS_TCPIP_STACK_HTONS(_hostShort_) \
    (gmosTcpipStackByteReverseU16 (_hostShort_))
#endif

/**
 * Provides a macro for converting 32-bit integer values from network
 * byte order to native host representation.
 * @param _netLong_ This is the 32-bit network byte order integer value.
 * @return Returns a 32-bit integer value in native host representation.
 */
#if GMOS_CONFIG_HOST_BIG_ENDIAN
#define GMOS_TCPIP_STACK_NTOHL(_netLong_) \
    (_netLong_)
#else
#define GMOS_TCPIP_STACK_NTOHL(_netLong_) \
    (gmosTcpipStackByteReverseU32 (_netLong_))
#endif

/**
 * Provides a macro for converting 16-bit integer values from network
 * byte order to native host representation.
 * @param _netShort_ This is the 16-bit network byte order integer value.
 * @return Returns a 16-bit integer value in native host representation.
 */
#if GMOS_CONFIG_HOST_BIG_ENDIAN
#define GMOS_TCPIP_STACK_NTOHS(_netShort_) \
    (_netShort_)
#else
#define GMOS_TCPIP_STACK_NTOHS(_netShort_) \
    (gmosTcpipStackByteReverseU16 (_netShort_))
#endif

#endif // GMOS_TCPIP_STACK_H
