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
 * This header provides the common data structures and API for accessing
 * generic network links from next higher layer protocol components.
 * Network links are abstract point to point connections which hide the
 * low level implementation details of the connection.
 */

#ifndef GMOS_NETWORK_LINKS_H
#define GMOS_NETWORK_LINKS_H

#include "gmos-scheduler.h"
#include "gmos-buffers.h"
#include "gmos-network.h"

// Provide forward reference type definitions.
typedef struct gmosNetworkLink_t gmosNetworkLink_t;

/**
 * Specifies the function prototype to be used for network link
 * connection requests.
 * @param networkLink This is the network link for which the connection
 *     is to be established.
 * @return Returns a network status value which indicates whether the
 *     network link connection process was successfully initiated or if
 *     it needs to be retried at a future time. Alternatively, it will
 *     indicate the reason for failure.
 */
typedef gmosNetworkStatus_t (*gmosNetworkLinkConnecter_t) (
    gmosNetworkLink_t* networkLink);

/**
 * Specifies the function prototype to be used for network link
 * disconnection requests.
 * @param networkLink This is the network link for which the connection
 *     is to be terminated.
 * @return Returns a network status value which indicates whether the
 *     network link disconnection process was successfully initiated or
 *     if it needs to be retried at a future time. Alternatively, it
 *     will indicate the reason for failure.
 */
typedef gmosNetworkStatus_t (*gmosNetworkLinkDisconnecter_t) (
    gmosNetworkLink_t* networkLink);

/**
 * Specifies the function prototype to be used for sending data held in
 * a GubbinsMOS buffer over a network link.
 * @param networkLink This is a pointer to the network link instance
 *     that is to be used for sending the data.
 * @param payload This is a pointer to a GubbinsMOS buffer that contains
 *     the data to be transmitted over the network link. On successful
 *     completion, the buffer contents will automatically be released.
 * @return Returns a network status value indicating whether the buffer
 *     send request was successful or if it needs to be retried at a
 *     future time. Alternatively, it will indicate the reason for
 *     failure.
 */
typedef gmosNetworkStatus_t (*gmosNetworkLinkSender_t) (
    gmosNetworkLink_t* networkLink, gmosBuffer_t* payload);

/**
 * Specifies the function prototype to be used for receiving data from
 * a network link and transferring it to a local GubbinsMOS buffer.
 * @param networkLink This is a pointer to the network link instance
 *     that is to be used for receiving the data.
 * @param payload This is a pointer to a GubbinsMOS buffer that will
 *     be populated with data received over the network link. On
 *     successful completion, any existing buffer contents will be
 *     discarded and replaced with the network link payload data.
 * @return Returns a network status value indicating whether the buffer
 *     receive request was successful or if it needs to be retried at a
 *     future time. Alternatively, it will indicate the reason for
 *     failure.
 */
typedef gmosNetworkStatus_t (*gmosNetworkLinkReceiver_t) (
    gmosNetworkLink_t* networkLink, gmosBuffer_t* payload);

/**
 * Specifies the function prototype to be used for monitoring the status
 * of a network link.
 * @param networkLink This is a pointer to the network link instance
 *     that is to be used for accessing the link status.
 * @return Returns a network status value indicating the current
 *     network link status. This may indicate that the network is down,
 *     the network is available but the link is not connected or that
 *     a link is currently connected. A retry status is used to indicate
 *     that the link is in a transient state.
 */
typedef gmosNetworkStatus_t (*gmosNetworkLinkMonitor_t) (
    gmosNetworkLink_t* networkLink);

/**
 * Defines the data structure for a generic network link, which mainly
 * consists of a function pointer table for selecting the appropriate
 * network link access functions. This will typically be placed at the
 * start of each implementation specific network link data structure.
 */
typedef struct gmosNetworkLink_t {

    // Specify a pointer to the network link connect function.
    gmosNetworkLinkConnecter_t connect;

    // Specify a pointer to the network link disconnect function.
    gmosNetworkLinkDisconnecter_t disconnect;

    // Specify a pointer to the network link buffer send function.
    gmosNetworkLinkSender_t send;

    // Specify a pointer to the network link buffer receive function.
    gmosNetworkLinkReceiver_t receive;

    // Specify a pointer to the network link monitor function.
    gmosNetworkLinkMonitor_t monitor;

    // Specify a pointer to the network link notification handler.
    gmosNetworkNotifyHandler_t notifyHandler;

    // Specify an opaque pointer to the associated notification handler
    // context data.
    void* notifyContext;

    // Specify the consumer task which will be automatically resumed
    // when new receive data is available.
    gmosTaskState_t* consumerTask;

} gmosNetworkLink_t;

/**
 * Assigns a notification callback handler to a given network link
 * instance.
 * @param networkLink This is the network link for which the
 *     notification handler is to be assigned.
 * @param notifyHandler This is the network status notification handler
 *     which is to be registered for the network link. A null reference
 *     may be passed in order to disable status notification callbacks.
 * @param notifyContext This is an opaque pointer to the network context
 *     data which will be passed to the notification handler on each
 *     callback.
 */
static inline void gmosNetworkLinkSetNotifyHandler (
    gmosNetworkLink_t* networkLink,
    gmosNetworkNotifyHandler_t notifyHandler, void* notifyContext)
{
    networkLink->notifyHandler = notifyHandler;
    networkLink->notifyContext = notifyContext;
}

/**
 * Assigns a consumer task reference to a given network link instance.
 * The consumer task will be automatically resumed whenever new data is
 * received over the network link.
 * @param networkLink This is the network link for which the consumer
 *     task is to be assigned.
 * @param consumerTask This is the consumer task which will be resumed
 *     on receiving new data. A null reference may be passed in order to
 *     disable automatic task resumption.
 */
static inline void gmosNetworkLinkSetConsumerTask (
    gmosNetworkLink_t* networkLink, gmosTaskState_t* consumerTask)
{
    networkLink->consumerTask = consumerTask;
}

/**
 * Issues a connection request for the specified network link instance.
 * @param networkLink This is the network link for which the connection
 *     is to be established.
 * @return Returns a network status value which indicates whether the
 *     network link connection process was successfully initiated or if
 *     it needs to be retried at a future time. Alternatively, it will
 *     indicate the reason for failure.
 */
static inline gmosNetworkStatus_t gmosNetworkLinkConnect (
    gmosNetworkLink_t* networkLink)
{
    return networkLink->connect (networkLink);
}

/**
 * Issues a disconnection request for the specified network link
 * instance.
 * @param networkLink This is the network link for which the connection
 *     is to be terminated.
 * @return Returns a network status value which indicates whether the
 *     network link disconnection process was successfully initiated or
 *     if it needs to be retried at a future time. Alternatively, it
 *     will indicate the reason for failure.
 */
static inline gmosNetworkStatus_t gmosNetworkLinkDisconnect (
    gmosNetworkLink_t* networkLink)
{
    return networkLink->disconnect (networkLink);
}

/**
 * Sends data held in a GubbinsMOS buffer over the specified network
 * link instance.
 * @param networkLink This is a pointer to the network link instance
 *     that is to be used for sending the data.
 * @param payload This is a pointer to a GubbinsMOS buffer that contains
 *     the data to be transmitted over the network link. On successful
 *     completion, the buffer contents will automatically be released.
 * @return Returns a network status value indicating whether the buffer
 *     send request was successful or if it needs to be retried at a
 *     future time. Alternatively, it will indicate the reason for
 *     failure.
 */
static inline gmosNetworkStatus_t gmosNetworkLinkSend (
    gmosNetworkLink_t* networkLink, gmosBuffer_t* payload)
{
    return networkLink->send (networkLink, payload);
}

/**
 * Receives data from the specified network link instance and transfers
 * it to a local GubbinsMOS buffer.
 * @param networkLink This is a pointer to the network link instance
 *     that is to be used for receiving the data.
 * @param payload This is a pointer to a GubbinsMOS buffer that will
 *     be populated with data received over the network link. On
 *     successful completion, any existing buffer contents will be
 *     discarded and replaced with the network link payload data.
 * @return Returns a network status value indicating whether the buffer
 *     receive request was successful or if it needs to be retried at a
 *     future time. Alternatively, it will indicate the reason for
 *     failure.
 */
static inline gmosNetworkStatus_t gmosNetworkLinkReceive (
    gmosNetworkLink_t* networkLink, gmosBuffer_t* payload)
{
    return networkLink->receive (networkLink, payload);
}

/**
 * Monitors the status of a given network link.
 * @param networkLink This is a pointer to the network link instance
 *     that is to be used for accessing the link status.
 * @return Returns a network status value indicating the current
 *     network link status. This may indicate that the network is down,
 *     the network is available but the link is not connected or that
 *     a link is currently connected. A retry status is used to indicate
 *     that the link is in a transient state.
 */
static inline gmosNetworkStatus_t gmosNetworkLinkMonitor (
    gmosNetworkLink_t* networkLink)
{
    return networkLink->monitor (networkLink);
}

#endif // GMOS_NETWORK_LINKS_H
