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
 * This file implements the common API for the TCP/IP stack. It
 * delegates most operations to the implementation specific TCP/IP
 * driver for socket management and data transfer.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-scheduler.h"
#include "gmos-buffers.h"
#include "gmos-network.h"
#include "gmos-tcpip-config.h"
#include "gmos-tcpip-stack.h"
#include "gmos-tcpip-dhcp.h"
#include "gmos-driver-tcpip.h"

/*
 * Initialises the TCP/IP stack on startup.
 */
bool gmosTcpipStackInit (gmosTcpipStack_t* tcpipStack,
    gmosDriverTcpip_t* tcpipDriver, gmosTcpipDhcpClient_t* dhcpClient,
    gmosTcpipDnsClient_t* dnsClient, const uint8_t* ethMacAddr,
    const char* dhcpHostName)
{
    // Set the TCP/IP stack component pointers.
    tcpipStack->tcpipDriver = tcpipDriver;
    tcpipStack->dhcpClient = dhcpClient;
    tcpipStack->dnsClient = dnsClient;

    // Initialise the TCP/IP driver component.
    if (!gmosDriverTcpipInit (tcpipDriver, ethMacAddr)) {
        return false;
    }

    // Initialise the DHCP client component.
    if (dhcpClient != NULL) {
        if (!gmosTcpipDhcpClientInit (
            dhcpClient, tcpipStack, dhcpHostName)) {
            return false;
        }
    }

    // Initialise the DNS client component.
    if (dnsClient != NULL) {
        if (!gmosTcpipDnsClientInit (dnsClient, tcpipStack)) {
            return false;
        }
    }
    return true;
}

/*
 * Attempts to open a new UDP socket for subsequent use.
 */
gmosTcpipStackSocket_t* gmosTcpipStackUdpOpen (
    gmosTcpipStack_t* tcpipStack, bool useIpv6,
    uint16_t localPort, gmosTaskState_t* appTask,
    gmosTcpipStackNotifyCallback_t notifyHandler, void* notifyData)
{
    gmosNalTcpipSocket_t* nalSocket;

    // Open the implementation specific socket.
    nalSocket = gmosDriverTcpipUdpOpen (tcpipStack->tcpipDriver,
        useIpv6, localPort, appTask, notifyHandler, notifyData);
    return (gmosTcpipStackSocket_t*) nalSocket;
}

/*
 * Sends a UDP datagram to the specified remote IP address using an
 * opened UDP socket.
 */
gmosNetworkStatus_t gmosTcpipStackUdpSendTo (
    gmosTcpipStackSocket_t* udpSocket, uint8_t* remoteAddr,
    uint16_t remotePort, gmosBuffer_t* payload)
{
    gmosNalTcpipSocket_t* nalSocket = (gmosNalTcpipSocket_t*) udpSocket;
    return gmosDriverTcpipUdpSendTo (
        nalSocket, remoteAddr, remotePort, payload);
}

/*
 * Receives a UDP datagram from a remote IP address using an opened UDP
 * socket.
 */
gmosNetworkStatus_t gmosTcpipStackUdpReceiveFrom (
    gmosTcpipStackSocket_t* udpSocket, uint8_t* remoteAddr,
    uint16_t* remotePort, gmosBuffer_t* payload)
{
    gmosNalTcpipSocket_t* nalSocket = (gmosNalTcpipSocket_t*) udpSocket;
    return gmosDriverTcpipUdpReceiveFrom (
        nalSocket, remoteAddr, remotePort, payload);
}

/*
 * Closes the specified UDP socket, releasing all allocated resources.
 */
gmosNetworkStatus_t gmosTcpipStackUdpClose (
    gmosTcpipStackSocket_t* udpSocket)
{
    gmosNalTcpipSocket_t* nalSocket = (gmosNalTcpipSocket_t*) udpSocket;
    return gmosDriverTcpipUdpClose (nalSocket);
}

/*
 * Attempts to open a new TCP socket for subsequent use.
 */
gmosTcpipStackSocket_t* gmosTcpipStackTcpOpen (
    gmosTcpipStack_t* tcpipStack, bool useIpv6,
    uint16_t localPort, gmosTaskState_t* appTask,
    gmosTcpipStackNotifyCallback_t notifyHandler, void* notifyData)
{
    gmosNalTcpipSocket_t* nalSocket;

    // Open the implementation specific socket.
    nalSocket = gmosDriverTcpipTcpOpen (tcpipStack->tcpipDriver,
        useIpv6, localPort, appTask, notifyHandler, notifyData);
    return (gmosTcpipStackSocket_t*) nalSocket;
}

/*
 * Initiates the TCP connection process as a TCP client, using the
 * specified server address and port.
 */
gmosNetworkStatus_t gmosTcpipStackTcpConnect (
    gmosTcpipStackSocket_t* tcpSocket,
    uint8_t* serverAddr, uint16_t serverPort)
{
    gmosNalTcpipSocket_t* nalSocket = (gmosNalTcpipSocket_t*) tcpSocket;
    return gmosDriverTcpipTcpConnect (
        nalSocket, serverAddr, serverPort);
}

/*
 * Sets up the TCP socket as a server for accepting TCP connection
 * requests, using the specified local port.
 */
gmosNetworkStatus_t gmosTcpipStackTcpBind (
    gmosTcpipStackSocket_t* tcpSocket, uint16_t serverPort)
{
    gmosNalTcpipSocket_t* nalSocket = (gmosNalTcpipSocket_t*) tcpSocket;
    return gmosDriverTcpipTcpBind (nalSocket, serverPort);
}

/*
 * Sends the contents of a GubbinsMOS buffer over an established TCP
 * connection.
 */
gmosNetworkStatus_t gmosTcpipStackTcpSend (
    gmosTcpipStackSocket_t* tcpSocket, gmosBuffer_t* payload)
{
    gmosNalTcpipSocket_t* nalSocket = (gmosNalTcpipSocket_t*) tcpSocket;
    return gmosDriverTcpipTcpSend (nalSocket, payload);
}

/*
 * Attempts to write an array of octet data to an established TCP
 * connection.
 */
gmosNetworkStatus_t gmosTcpipStackTcpWrite (
    gmosTcpipStackSocket_t* tcpSocket, const uint8_t* writeData,
    uint16_t requestSize, uint16_t* transferSize)
{
    gmosNalTcpipSocket_t* nalSocket = (gmosNalTcpipSocket_t*) tcpSocket;
    uint32_t maxTransferSize;
    gmosBuffer_t writeBuffer = GMOS_BUFFER_INIT ();
    gmosNetworkStatus_t stackStatus;

    // Determine the maximum possible transfer size. This is set at half
    // the number of free buffers in the memory pool.
    maxTransferSize = gmosMempoolSegmentsAvailable ();
    maxTransferSize *= GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE / 2;

    // Indicate that no data can be transferred at this time.
    if (maxTransferSize == 0) {
        *transferSize = 0;
        return GMOS_NETWORK_STATUS_RETRY;
    }

    // Determine the actual transfer size to use.
    if (maxTransferSize < requestSize) {
        requestSize = maxTransferSize;
    }

    // Copy the requested data to the local buffer. This should always
    // succeed due to the previous transfer size checks.
    gmosBufferAppend (&writeBuffer, writeData, requestSize);

    // Attempt to send the buffer using the TCP send API call.
    stackStatus = gmosDriverTcpipTcpSend (nalSocket, &writeBuffer);
    if (stackStatus == GMOS_NETWORK_STATUS_SUCCESS) {
        *transferSize = requestSize;
        return GMOS_NETWORK_STATUS_SUCCESS;
    }

    // Release the allocated write buffer memory on failure.
    else {
        gmosBufferReset (&writeBuffer, 0);
        *transferSize = 0;
        return stackStatus;
    }
}

/*
 * Receives a block of data over an established TCP connection.
 */
gmosNetworkStatus_t gmosTcpipStackTcpReceive (
    gmosTcpipStackSocket_t* tcpSocket, gmosBuffer_t* payload)
{
    gmosNalTcpipSocket_t* nalSocket = (gmosNalTcpipSocket_t*) tcpSocket;
    return gmosDriverTcpipTcpReceive (nalSocket, payload);
}

/*
 * Attempts to read an array of octet data from an established TCP
 * connection.
 */
gmosNetworkStatus_t gmosTcpipStackTcpRead (
    gmosTcpipStackSocket_t* tcpSocket, uint8_t* readData,
    uint16_t requestSize, uint16_t* transferSize)
{
    gmosNalTcpipSocket_t* nalSocket = (gmosNalTcpipSocket_t*) tcpSocket;
    gmosStream_t* rxStream = &(tcpSocket->rxStream);
    gmosBuffer_t payload = GMOS_BUFFER_INIT ();
    gmosNetworkStatus_t stackStatus;
    uint16_t payloadSize;
    uint16_t readSize;
    bool releaseBuffer;

    // Attempt to receive a payload buffer from the recive data
    // stream.
    stackStatus = gmosDriverTcpipTcpReceive (nalSocket, &payload);
    if (stackStatus != GMOS_NETWORK_STATUS_SUCCESS) {
        *transferSize = 0;
        return stackStatus;
    }

    // Set the read data size and determine whether the payload buffer
    // can be released on completion.
    payloadSize = gmosBufferGetSize (&payload);
    if (payloadSize > requestSize) {
        readSize = requestSize;
        releaseBuffer = false;
    } else {
        readSize = payloadSize;
        releaseBuffer = true;
    }

    // Read the data from the buffer into the read data array.
    gmosBufferRead (&payload, 0, readData, readSize);

    // Release the buffer memory or push it back onto the receive
    // buffer stream for subsequent access.
    if (releaseBuffer) {
        gmosBufferReset (&payload, 0);
    } else {
        gmosBufferRebase (&payload, payloadSize - requestSize);
        gmosStreamPushBackBuffer (rxStream, &payload);
    }
    *transferSize = readSize;
    return GMOS_NETWORK_STATUS_SUCCESS;
}

/*
 * Closes the specified TCP socket, terminating any active connection
 * and releasing all allocated resources.
 */
gmosNetworkStatus_t gmosTcpipStackTcpClose (
    gmosTcpipStackSocket_t* tcpSocket)
{
    gmosNalTcpipSocket_t* nalSocket = (gmosNalTcpipSocket_t*) tcpSocket;
    return gmosDriverTcpipTcpClose (nalSocket);
}
