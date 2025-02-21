/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2024-2025 Zynaptic Limited
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
 * This file implements generic network link wrappers for conventional
 * TCP socket connections. This abstraction allows higher layer
 * protocols to operate across a range of network layer protcols,
 * including the TCP socket connections implemented here.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-scheduler.h"
#include "gmos-buffers.h"
#include "gmos-network.h"
#include "gmos-network-links.h"
#include "gmos-tcpip-stack.h"
#include "gmos-tcpip-links.h"
#include "gmos-tcpip-dhcp.h"
#include "gmos-tcpip-dns.h"

/*
 * Specify the internal state space for the TCP/IP link state machine.
 */
typedef enum {
    GMOS_TCPIP_LINK_STATE_INITIALISED,
    GMOS_TCPIP_LINK_STATE_CONFIGURED,
    GMOS_TCPIP_LINK_STATE_OPENING,
    GMOS_TCPIP_LINK_STATE_DNS_LOOKUP,
    GMOS_TCPIP_LINK_STATE_TCP_CONNECT,
    GMOS_TCPIP_LINK_STATE_TCP_WAIT,
    GMOS_TCPIP_LINK_STATE_CONNECTED,
    GMOS_TCPIP_LINK_STATE_CLOSING,
    GMOS_TCPIP_LINK_STATE_FAILURE
} gmosTcpipLinkState_t;

/*
 * Implement the task handler for the main TCP link state machine.
 */
static gmosTaskStatus_t gmosTcpipLinkWorkerTaskFn (void* taskData)
{
    gmosTcpipLink_t* tcpipLink = (gmosTcpipLink_t*) taskData;
    gmosTcpipStack_t* tcpipStack = tcpipLink->tcpipStack;
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    gmosNetworkStatus_t networkStatus;
    uint8_t nextLinkState = tcpipLink->linkState;
    bool useIpv6 = false;

    // Select IPv6 operation if required.
#if GMOS_CONFIG_TCPIP_IPV6_ENABLE
    useIpv6 = tcpipLink->useIpv6 ? true : false;
#endif

    // Implement task handler state machine.
    switch (tcpipLink->linkState) {

        // Perform DNS lookups. This may take some time to resolve, so
        // The DNS client is polled at 1s intervals.
        case GMOS_TCPIP_LINK_STATE_DNS_LOOKUP :
            networkStatus = gmosTcpipDnsClientQuery (
                tcpipStack->dnsClient, tcpipLink->remoteDnsName,
                useIpv6, tcpipLink->remoteIpAddr);
            if (networkStatus == GMOS_NETWORK_STATUS_SUCCESS) {
                nextLinkState = GMOS_TCPIP_LINK_STATE_TCP_CONNECT;
            } else if (networkStatus == GMOS_NETWORK_STATUS_RETRY) {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (1000));
            } else {
                // TODO: Proper error handling.
                nextLinkState = GMOS_TCPIP_LINK_STATE_FAILURE;
            }
            break;

        // Initiate the TCP connection.
        case GMOS_TCPIP_LINK_STATE_TCP_CONNECT :
            networkStatus = gmosTcpipStackTcpConnect (
                tcpipLink->tcpSocket, tcpipLink->remoteIpAddr,
                tcpipLink->remoteIpPort);
            if (networkStatus == GMOS_NETWORK_STATUS_SUCCESS) {
                nextLinkState = GMOS_TCPIP_LINK_STATE_TCP_WAIT;
            } else if (networkStatus == GMOS_NETWORK_STATUS_RETRY) {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (250));
            } else {
                // TODO: Proper error handling.
                nextLinkState = GMOS_TCPIP_LINK_STATE_FAILURE;
            }
            break;

        // Suspend further task processing in inactive states.
        default :
            taskStatus = GMOS_TASK_SUSPEND;
            break;
    }
    tcpipLink->linkState = nextLinkState;
    return taskStatus;
}

/*
 * Implement the stack status callback handler for the TCP link state
 * machine.
 */
static void gmosTcpipLinkStackNotifyCallback (
    void* notifyData, gmosTcpipStackNotify_t notification)
{
    gmosTcpipLink_t* tcpipLink = (gmosTcpipLink_t*) notifyData;
    uint8_t nextLinkState = tcpipLink->linkState;

    // Implement stack notification state machine.
    switch (tcpipLink->linkState) {

        // Wait for TCP socket opening to complete. Once open either
        // initiate a DNS lookup or use the fixed IP address.
        case GMOS_TCPIP_LINK_STATE_OPENING :
            if (notification == GMOS_TCPIP_STACK_NOTIFY_TCP_SOCKET_OPENED) {
                if (tcpipLink->remoteDnsName != NULL) {
                    nextLinkState = GMOS_TCPIP_LINK_STATE_DNS_LOOKUP;
                } else {
                    nextLinkState = GMOS_TCPIP_LINK_STATE_TCP_CONNECT;
                }
            }
            // TODO: Handle open request failures.
            break;

        // Wait for TCP connection to be established.
        case GMOS_TCPIP_LINK_STATE_TCP_WAIT :
            if (notification == GMOS_TCPIP_STACK_NOTIFY_TCP_SOCKET_CONNECTED) {
                GMOS_LOG (LOG_DEBUG, "TCP socket connected.");
                nextLinkState = GMOS_TCPIP_LINK_STATE_CONNECTED;
                if (tcpipLink->networkLink.notifyHandler != NULL) {
                    tcpipLink->networkLink.notifyHandler (
                        tcpipLink->networkLink.notifyContext,
                        GMOS_NETWORK_NOTIFY_CONNECTED);
                }
            }
            // TODO: Handle open request failures.
            break;

        // Wait for TCP socket closing to complete. This can occur as
        // result of a local request or may have been initiated by the
        // remote host.
        case GMOS_TCPIP_LINK_STATE_CONNECTED :
        case GMOS_TCPIP_LINK_STATE_CLOSING :
            if (notification == GMOS_TCPIP_STACK_NOTIFY_TCP_SOCKET_CLOSED) {
                GMOS_LOG (LOG_DEBUG, "TCP socket closed.");
                nextLinkState = GMOS_TCPIP_LINK_STATE_CONFIGURED;
                if (tcpipLink->networkLink.notifyHandler != NULL) {
                    tcpipLink->networkLink.notifyHandler (
                        tcpipLink->networkLink.notifyContext,
                        GMOS_NETWORK_NOTIFY_DISCONNECTED);
                }
            }
            // TODO: Handle open request failures.
            break;
    }

    // Run the task handler if there has been a change of state.
    if (tcpipLink->linkState != nextLinkState) {
        tcpipLink->linkState = nextLinkState;
        gmosSchedulerTaskResume (&(tcpipLink->workerTask));
    }
}

/*
 * Initiates a TCP socket connection request.
 */
static gmosNetworkStatus_t gmosTcpipLinkConnector (
    gmosNetworkLink_t* networkLink)
{
    gmosTcpipLink_t* tcpipLink = (gmosTcpipLink_t*) networkLink;
    bool useIpv6 = false;

    // Only initiate the link connection process if the link has already
    // been configured.
    if (tcpipLink->linkState != GMOS_TCPIP_LINK_STATE_CONFIGURED) {
        return GMOS_NETWORK_STATUS_NOT_VALID;
    }

    // Select IPv6 operation if required.
#if GMOS_CONFIG_TCPIP_IPV6_ENABLE
    useIpv6 = tcpipLink->useIpv6 ? true : false;
#endif

    // Open the TCP socket.
    tcpipLink->tcpSocket = gmosTcpipStackTcpOpen (
        tcpipLink->tcpipStack, useIpv6, tcpipLink->localIpPort,
        networkLink->consumerTask, gmosTcpipLinkStackNotifyCallback,
        tcpipLink);
    if (tcpipLink->tcpSocket == NULL) {
        return GMOS_NETWORK_STATUS_RETRY;
    }

    // Set the state machine to wait for the socket open callback.
    tcpipLink->linkState = GMOS_TCPIP_LINK_STATE_OPENING;
    return GMOS_NETWORK_STATUS_SUCCESS;
}

/*
 * Initiates a TCP socket disconnection request.
 */
static gmosNetworkStatus_t gmosTcpipLinkDisconnector (gmosNetworkLink_t* networkLink)
{
    gmosTcpipLink_t* tcpipLink = (gmosTcpipLink_t*) networkLink;
    gmosNetworkStatus_t status;

    // Only initiate the link disconnection process if the link is
    // currently connected.
    if (tcpipLink->linkState != GMOS_TCPIP_LINK_STATE_CONNECTED) {
        status = GMOS_NETWORK_STATUS_NOT_CONNECTED;
    }

    // Close the TCP socket.
    else {
        status = gmosTcpipStackTcpClose (tcpipLink->tcpSocket);
        if (status == GMOS_NETWORK_STATUS_SUCCESS) {
            tcpipLink->linkState = GMOS_TCPIP_LINK_STATE_CLOSING;
        }
    }
    return status;
}

/*
 * Sends the contents of a GubbinsMOS buffer over a TCP link connection.
 */
static gmosNetworkStatus_t gmosTcpipLinkSender (
    gmosNetworkLink_t* networkLink, gmosBuffer_t* payload)
{
    gmosTcpipLink_t* tcpipLink = (gmosTcpipLink_t*) networkLink;
    gmosNetworkStatus_t status;

    // Check that the link is connected.
    if (tcpipLink->linkState != GMOS_TCPIP_LINK_STATE_CONNECTED) {
        status = GMOS_NETWORK_STATUS_NOT_CONNECTED;
    }

    // Send the payload data buffer using the TCP/IP stack.
    else {
        status = gmosTcpipStackTcpSend (tcpipLink->tcpSocket, payload);
    }
    return status;
}

/*
 * Receives the contents of a GubbinsMOS buffer from a TCP link
 * connection.
 */
static gmosNetworkStatus_t gmosTcpipLinkReceiver (
    gmosNetworkLink_t* networkLink, gmosBuffer_t* payload)
{
    gmosTcpipLink_t* tcpipLink = (gmosTcpipLink_t*) networkLink;
    gmosNetworkStatus_t status;

    // Attempt the read any queued data, even if the socket has already
    // been closed by the remote side.
    status = gmosTcpipStackTcpReceive (tcpipLink->tcpSocket, payload);
    GMOS_LOG_FMT (LOG_VERBOSE, "TCP receive status is %d", status);

    // Check that the link is connected.
    if ((status != GMOS_NETWORK_STATUS_SUCCESS) &&
        (tcpipLink->linkState != GMOS_TCPIP_LINK_STATE_CONNECTED)) {
        status = GMOS_NETWORK_STATUS_NOT_CONNECTED;
        GMOS_LOG_FMT (LOG_VERBOSE, "TCP receive status replaced by %d", status);
    }
    return status;
}

/*
 * Monitors the status of a given network link.
 */
static gmosNetworkStatus_t gmosTcpipLinkMonitor (
    gmosNetworkLink_t* networkLink)
{
    gmosTcpipLink_t* tcpipLink = (gmosTcpipLink_t*) networkLink;
    gmosTcpipStack_t* tcpipStack = tcpipLink->tcpipStack;
    gmosNetworkStatus_t status;

    // Check to see if the DHCP service is in use. This is currently
    // required for normal operation.
    if (tcpipStack->dhcpClient == NULL) {
        status = GMOS_NETWORK_STATUS_UNSUPPORTED;
    }

    // Check to see if the DHCP registration is valid.
    else if (!gmosTcpipDhcpClientReady (tcpipStack->dhcpClient)) {
        status = GMOS_NETWORK_STATUS_NETWORK_DOWN;
    }

    // Determine if the TCP/IP link is connected.
    else if (tcpipLink->linkState == GMOS_TCPIP_LINK_STATE_CONNECTED) {
        status = GMOS_NETWORK_STATUS_CONNECTED;
    }

    // The connection is only reported as being disconnected if it is
    // in the configured but inactive state.
    else if (tcpipLink->linkState == GMOS_TCPIP_LINK_STATE_CONFIGURED) {
        status = GMOS_NETWORK_STATUS_NOT_CONNECTED;
    }

    // This request is not valid if the link has not been configured.
    else if (tcpipLink->linkState == GMOS_TCPIP_LINK_STATE_INITIALISED) {
        status = GMOS_NETWORK_STATUS_NOT_VALID;
    }

    // Indicate a failure condition for the network link.
    else if (tcpipLink->linkState == GMOS_TCPIP_LINK_STATE_FAILURE) {
        status = GMOS_NETWORK_STATUS_DRIVER_FAILURE;
    }

    // Remaining states are transitional states and the client should
    // retry the request at a later time.
    else {
        status = GMOS_NETWORK_STATUS_RETRY;
    }
    return status;
}

/*
 * Initialises the TCPIP link instance on startup.
 */
bool gmosTcpipLinkInit (gmosTcpipLink_t* tcpipLink,
    gmosTcpipStack_t* tcpipStack, bool useIpv6)
{
    gmosTaskState_t* workerTask = &(tcpipLink->workerTask);

    // Set up the network link function and data pointers.
    tcpipLink->networkLink.connect = gmosTcpipLinkConnector;
    tcpipLink->networkLink.disconnect = gmosTcpipLinkDisconnector;
    tcpipLink->networkLink.send = gmosTcpipLinkSender;
    tcpipLink->networkLink.receive = gmosTcpipLinkReceiver;
    tcpipLink->networkLink.monitor = gmosTcpipLinkMonitor;
    tcpipLink->networkLink.notifyHandler = NULL;
    tcpipLink->networkLink.notifyContext = NULL;
    tcpipLink->networkLink.consumerTask = NULL;

    // Initialise the TCP link specific data.
    tcpipLink->linkState = GMOS_TCPIP_LINK_STATE_INITIALISED;
    tcpipLink->tcpipStack = tcpipStack;
    tcpipLink->tcpSocket = NULL;
    tcpipLink->remoteDnsName = NULL;

    // Select IPv6 if supported.
#if GMOS_CONFIG_TCPIP_IPV6_ENABLE
    tcpipLink->useIpv6 = useIpv6 ? 1 : 0;
#else
    GMOS_ASSERT (ASSERT_FAILURE, useIpv6 == false,
        "IPv6 not supported by current configuration.");
#endif

    // Initialise the TCP link worker task.
    workerTask->taskTickFn = gmosTcpipLinkWorkerTaskFn;
    workerTask->taskData = tcpipLink;
    workerTask->taskName =
        GMOS_TASK_NAME_WRAPPER ("TCP/IP Network Link");
    gmosSchedulerTaskStart (workerTask);
    return true;
}

/*
 * Sets the DNS name based configuration for a TCPIP link instance prior
 * to initiating a connection.
 */
bool gmosTcpipLinkConfigureDnsName (gmosTcpipLink_t* tcpipLink,
    const char* remoteDnsName, uint16_t remoteIpPort,
    uint16_t localIpPort)
{
    // The link can not be reconfigured when it is in use.
    if ((tcpipLink->linkState != GMOS_TCPIP_LINK_STATE_INITIALISED) &&
        (tcpipLink->linkState != GMOS_TCPIP_LINK_STATE_CONFIGURED)) {
        return false;
    }

    // Set the TCP link parameters using DNS name lookup.
    tcpipLink->remoteDnsName = remoteDnsName;
    tcpipLink->remoteIpPort = remoteIpPort;
    tcpipLink->localIpPort = localIpPort;
    tcpipLink->linkState = GMOS_TCPIP_LINK_STATE_CONFIGURED;
    return true;
}

/*
 * Sets the fixed IP configuration for a TCPIP link instance prior to
 * initiating a connection.
 */
bool gmosTcpipLinkConfigureFixedIp (gmosTcpipLink_t* tcpipLink,
    const uint8_t* remoteIpAddr, uint16_t remoteIpPort,
    uint16_t localIpPort)
{
    size_t ipAddrSize = 4;

    // Select IPv6 address size if required.
#if GMOS_CONFIG_TCPIP_IPV6_ENABLE
    ipAddrSize = tcpipLink->useIpv6 ? 16 : 4;
#endif

    // The link can not be reconfigured when it is in use.
    if ((tcpipLink->linkState != GMOS_TCPIP_LINK_STATE_INITIALISED) &&
        (tcpipLink->linkState != GMOS_TCPIP_LINK_STATE_CONFIGURED)) {
        return false;
    }

    // Set the TCP link parameters using a fixed IP address.
    memcpy (tcpipLink->remoteIpAddr, remoteIpAddr, ipAddrSize);
    tcpipLink->remoteDnsName = NULL;
    tcpipLink->remoteIpPort = remoteIpPort;
    tcpipLink->localIpPort = localIpPort;
    tcpipLink->linkState = GMOS_TCPIP_LINK_STATE_CONFIGURED;
    return true;
}
