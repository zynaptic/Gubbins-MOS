/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2022-2025 Zynaptic Limited
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
#include "gmos-platform.h"
#include "gmos-scheduler.h"
#include "gmos-buffers.h"
#include "gmos-network.h"
#include "gmos-tcpip-config.h"
#include "gmos-tcpip-stack.h"
#include "gmos-tcpip-dhcp.h"
#include "gmos-driver-tcpip.h"

/*
 * Allocates an ephemeral local port number for subsequent use.
 */
static uint16_t gmosTcpipPortAllocate (gmosTcpipStack_t* tcpipStack)
{
    uint16_t newPortNumber;
    uint_fast8_t i;
    uint_fast16_t activePort;
    uint_fast8_t emptySlotIndex;
    bool emptySlotFound;
    bool duplicatePort;

    // Repeat until the generated port number is not a duplicate of an
    // existing allocated port number.
    do {

        // RFC 6335 specifies an ephemeral port range from 49152 to
        // 65535 (0xC000 to 0xFFFF).
        newPortNumber = tcpipStack->portCounter++;
        newPortNumber |= 0xC000;

        // Search for empty slot and duplicate ports.
        emptySlotIndex = 0;
        emptySlotFound = false;
        duplicatePort = false;
        for (i = 0; i < GMOS_CONFIG_TCPIP_MAX_EPHEMERAL_PORTS; i++) {
            activePort = tcpipStack->activePorts [i];
            if (activePort == newPortNumber) {
                duplicatePort = true;
            } else if ((activePort == 0) && (!emptySlotFound)) {
                emptySlotFound = true;
                emptySlotIndex = i;
            }
        }
    } while (duplicatePort);

    // Add the new port number in the specified empty slot.
    if (emptySlotFound) {
        tcpipStack->activePorts [emptySlotIndex] = newPortNumber;
    } else {
        newPortNumber = 0;
    }
    return newPortNumber;
}

/*
 * Releases an ephemeral local port number from current use.
 */
static void gmosTcpipPortRelease (gmosTcpipStack_t* tcpipStack,
    uint16_t portNumber)
{
    uint_fast8_t i;

    // Search the active ports list for the matching port number.
    for (i = 0; i < GMOS_CONFIG_TCPIP_MAX_EPHEMERAL_PORTS; i++) {
        if (tcpipStack->activePorts [i] == portNumber) {
            tcpipStack->activePorts [i] = 0;
        }
    }
}

/*
 * Initialises the TCP/IP stack on startup.
 */
bool gmosTcpipStackInit (gmosTcpipStack_t* tcpipStack,
    gmosDriverTcpip_t* tcpipDriver, gmosTcpipDhcpClient_t* dhcpClient,
    gmosTcpipDnsClient_t* dnsClient, const uint8_t* ethMacAddr,
    const char* dhcpHostName)
{
    uint16_t randomPortNumber;
    uint_fast8_t i;

    // Set the TCP/IP stack component pointers.
    tcpipStack->tcpipDriver = tcpipDriver;
    tcpipStack->dhcpClient = dhcpClient;
    tcpipStack->dnsClient = dnsClient;

    // Initialise the TCP/IP driver component.
    if (!gmosDriverTcpipInit (tcpipStack, ethMacAddr)) {
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

    // Clear the allocated ephemeral local port list.
    gmosPalGetRandomBytes (
        (uint8_t*) (&randomPortNumber), sizeof (randomPortNumber));
    tcpipStack->portCounter = randomPortNumber;
    for (i = 0; i < GMOS_CONFIG_TCPIP_MAX_EPHEMERAL_PORTS; i++) {
        tcpipStack->activePorts [i] = 0;
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
    gmosNalTcpipSocket_t* nalSocket = NULL;

    // Allocate an ephemeral local port if required.
    if (localPort == 0) {
        localPort = gmosTcpipPortAllocate (tcpipStack);
    }

    // Open the implementation specific socket.
    if (localPort != 0) {
        nalSocket = gmosDriverTcpipUdpOpen (tcpipStack->tcpipDriver,
            useIpv6, localPort, appTask, notifyHandler, notifyData);
    }
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
    gmosTcpipPortRelease (udpSocket->tcpipStack, udpSocket->localPort);
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
    gmosNalTcpipSocket_t* nalSocket = NULL;

    // Allocate an ephemeral local port if required.
    if (localPort == 0) {
        localPort = gmosTcpipPortAllocate (tcpipStack);
    }

    // Open the implementation specific socket.
    if (localPort != 0) {
        nalSocket = gmosDriverTcpipTcpOpen (tcpipStack->tcpipDriver,
            useIpv6, localPort, appTask, notifyHandler, notifyData);
    }
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
    gmosTcpipPortRelease (tcpSocket->tcpipStack, tcpSocket->localPort);
    return gmosDriverTcpipTcpClose (nalSocket);
}
