/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2025 Zynaptic Limited
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
 * This file implements the public API for accessing an MbedTLS client
 * instance.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-buffers.h"
#include "gmos-streams.h"
#include "gmos-network-links.h"
#include "gmos-mbedtls-client.h"
#include "gmos-mbedtls-config.h"
#include "gmos-mbedtls-support.h"
#include "mbedtls/ssl.h"

/*
 * Specify the state space for the MbedTLS client state machine.
 */
typedef enum {
    GMOS_MBEDTLS_CLIENT_STATE_INITIALISED,
    GMOS_MBEDTLS_CLIENT_STATE_CONFIGURED,
    GMOS_MBEDTLS_CLIENT_STATE_TRANSPORT_CONNECT,
    GMOS_MBEDTLS_CLIENT_STATE_TLS_HANDSHAKE_POLL,
    GMOS_MBEDTLS_CLIENT_STATE_TLS_HANDSHAKE_STEP,
    GMOS_MBEDTLS_CLIENT_STATE_TLS_CONNECTED,
    GMOS_MBEDTLS_CLIENT_STATE_TLS_CLOSE_REQUEST,
    GMOS_MBEDTLS_CLIENT_STATE_TLS_CLOSE_POLL,
    GMOS_MBEDTLS_CLIENT_STATE_TRANSPORT_DISCONNECT,
    GMOS_MBEDTLS_CLIENT_STATE_SESSION_RESET,
    GMOS_MBEDTLS_CLIENT_STATE_DISCONNECTED,
    GMOS_MBEDTLS_CLIENT_STATE_FAILURE
} gmosMbedtlsClientState_t;

/*
 * Process network link connection requests.
 */
static gmosNetworkStatus_t gmosMbedtlsLinkConnector (
    gmosNetworkLink_t* networkLink)
{
    gmosMbedtlsClient_t* mbedtlsClient = (gmosMbedtlsClient_t*) networkLink;
    gmosNetworkStatus_t networkStatus = GMOS_NETWORK_STATUS_UNSUPPORTED;

    // Only initiate the link connection process if the link has already
    // been configured.
    if (mbedtlsClient->clientState !=
        GMOS_MBEDTLS_CLIENT_STATE_CONFIGURED) {
        networkStatus = GMOS_NETWORK_STATUS_NOT_VALID;
    }

    // Attempt to open the underlying transport link.
    else {
        networkStatus = gmosNetworkLinkConnect (mbedtlsClient->transportLink);
        if (networkStatus == GMOS_NETWORK_STATUS_SUCCESS) {
            mbedtlsClient->clientState =
                GMOS_MBEDTLS_CLIENT_STATE_TRANSPORT_CONNECT;
            gmosSchedulerTaskResume (&(mbedtlsClient->mbedtlsWorkerTask));
        }
    }
    return networkStatus;
}

/*
 * Process network link disconnection requests.
 */
static gmosNetworkStatus_t gmosMbedtlsLinkDisconnector (
    gmosNetworkLink_t* networkLink)
{
    gmosMbedtlsClient_t* mbedtlsClient = (gmosMbedtlsClient_t*) networkLink;
    gmosNetworkStatus_t networkStatus = GMOS_NETWORK_STATUS_UNSUPPORTED;

    // Allow the contents of the transmit queue to clear before
    // attempting to initiate a close request.
    if (gmosStreamGetReadCapacity (
        &(mbedtlsClient->txDataStream)) > 0) {
        networkStatus = GMOS_NETWORK_STATUS_RETRY;
    }

    // Indicate disconnection shutdown progress.
    else switch (mbedtlsClient->clientState) {

        // Only initiate the link disconnection process if the link is
        // currently connected.
        case GMOS_MBEDTLS_CLIENT_STATE_TLS_CONNECTED :
            mbedtlsClient->clientState =
                GMOS_MBEDTLS_CLIENT_STATE_TLS_CLOSE_REQUEST;
            gmosSchedulerTaskResume (&(mbedtlsClient->mbedtlsWorkerTask));
            networkStatus = GMOS_NETWORK_STATUS_RETRY;
            break;

        // Indicate success once the disconnected state is reached.
        case GMOS_MBEDTLS_CLIENT_STATE_DISCONNECTED :
            mbedtlsClient->clientState =
                GMOS_MBEDTLS_CLIENT_STATE_CONFIGURED;
            networkStatus = GMOS_NETWORK_STATUS_SUCCESS;
            break;

        // Request a retry attempt while in transitional shutdown states.
        case GMOS_MBEDTLS_CLIENT_STATE_TLS_CLOSE_REQUEST :
        case GMOS_MBEDTLS_CLIENT_STATE_TLS_CLOSE_POLL :
        case GMOS_MBEDTLS_CLIENT_STATE_TRANSPORT_DISCONNECT :
        case GMOS_MBEDTLS_CLIENT_STATE_SESSION_RESET :
            networkStatus = GMOS_NETWORK_STATUS_RETRY;
            break;

        // Other states imply that the MbedTLS client is not connected.
        default :
            networkStatus = GMOS_NETWORK_STATUS_NOT_CONNECTED;
            break;
    }
    return networkStatus;
}

/*
 * Send data held in a GubbinsMOS buffer over the network link.
 */
static gmosNetworkStatus_t gmosMbedtlsLinkSender (
    gmosNetworkLink_t* networkLink, gmosBuffer_t* payload)
{
    gmosMbedtlsClient_t* mbedtlsClient = (gmosMbedtlsClient_t*) networkLink;
    gmosStream_t* txDataStream = &(mbedtlsClient->txDataStream);
    gmosNetworkStatus_t status;

    // Check that the TLS client is connected.
    if (mbedtlsClient->clientState !=
        GMOS_MBEDTLS_CLIENT_STATE_TLS_CONNECTED) {
        status = GMOS_NETWORK_STATUS_NOT_CONNECTED;
    }

    // Append the buffer to the transmit stream.
    else if (gmosStreamSendBuffer (txDataStream, payload)) {
        status = GMOS_NETWORK_STATUS_SUCCESS;
    } else {
        status = GMOS_NETWORK_STATUS_RETRY;
    }
    return status;
}

/*
 * Receive data from the network link and transfer it to a local
 * GubbinsMOS buffer.
 */
static gmosNetworkStatus_t gmosMbedtlsLinkReceiver (
    gmosNetworkLink_t* networkLink, gmosBuffer_t* payload)
{
    gmosMbedtlsClient_t* mbedtlsClient = (gmosMbedtlsClient_t*) networkLink;
    gmosStream_t* rxDataStream = &(mbedtlsClient->rxDataStream);
    gmosNetworkStatus_t status;

    // Check that the TLS client is connected.
    if (mbedtlsClient->clientState !=
        GMOS_MBEDTLS_CLIENT_STATE_TLS_CONNECTED) {
        status = GMOS_NETWORK_STATUS_NOT_CONNECTED;
    }

    // Attempt to pop a buffer from the receive stream.
    else if (gmosStreamAcceptBuffer (rxDataStream, payload)) {
        status = GMOS_NETWORK_STATUS_SUCCESS;
    } else {
        status = GMOS_NETWORK_STATUS_RETRY;
    }
    return status;
}

/*
 * Monitor the status of the network link.
 */
static gmosNetworkStatus_t gmosMbedtlsLinkMonitor (
    gmosNetworkLink_t* networkLink)
{
    gmosMbedtlsClient_t* mbedtlsClient = (gmosMbedtlsClient_t*) networkLink;
    gmosNetworkStatus_t status;

    // Map current client state to appropriate status values.
    switch (mbedtlsClient->clientState) {

        // Determine if the TCP/IP link is connected.
        case GMOS_MBEDTLS_CLIENT_STATE_TLS_CONNECTED :
            status = GMOS_NETWORK_STATUS_CONNECTED;
            break;

        // The connection is only reported as being disconnected if it
        // is in the configured but inactive state.
        case GMOS_MBEDTLS_CLIENT_STATE_CONFIGURED :
            status = GMOS_NETWORK_STATUS_NOT_CONNECTED;
            break;

        // This request is not valid for an unconfigured link.
        case GMOS_MBEDTLS_CLIENT_STATE_INITIALISED :
            status = GMOS_NETWORK_STATUS_NOT_VALID;
            break;

        // Indicate a failure condition for the network link.
        case GMOS_MBEDTLS_CLIENT_STATE_FAILURE :
            status = GMOS_NETWORK_STATUS_DRIVER_FAILURE;
            break;

        // Remaining states are transitional states and the client
        // should retry the request at a later time.
        default :
            status = GMOS_NETWORK_STATUS_RETRY;
            break;
    }
    return status;
}

/*
 * Implement data forwarding from the transmit data stream. This copies
 * as much data as possible from the input data stream into the MbedTLS
 * internal buffer.
 */
static gmosTaskStatus_t gmosMbedtlsClientStreamTxData (
    gmosMbedtlsClient_t* mbedtlsClient, bool* sessionReset)
{
    gmosMbedtlsClientSupport_t* clientSupport =
        (gmosMbedtlsClientSupport_t*) mbedtlsClient->clientSupport;
    gmosStream_t* txDataStream = &(mbedtlsClient->txDataStream);
    gmosBuffer_t txDataBuffer = GMOS_BUFFER_INIT ();
    gmosTaskStatus_t taskStatus = GMOS_TASK_SUSPEND;
    uint_fast16_t txDataBufferSize;
    uint8_t txDataArray [GMOS_CONFIG_MBEDTLS_MAX_TX_DATA_SIZE];
    uint_fast16_t txDataSize;
    int mbedtlsStatus;
    bool txSessionReset = false;

    // Check that there is a data buffer available for transmission.
    if (!gmosStreamAcceptBuffer (txDataStream, &txDataBuffer)) {
        taskStatus = GMOS_TASK_SUSPEND;
        goto out;
    }

    // Attempt to send the entire contents of the current buffer.
    txDataBufferSize = gmosBufferGetSize (&txDataBuffer);
    while (txDataBufferSize > 0) {

        // Copy data from the buffer into an intermediate data array.
        GMOS_LOG_FMT (LOG_VERBOSE,
            "MbedTLS transmit data buffer length %d.",
            txDataBufferSize);
        txDataSize = (txDataBufferSize > sizeof (txDataArray)) ?
            sizeof (txDataArray) : txDataBufferSize;
        gmosBufferRead (&txDataBuffer, 0, txDataArray, txDataSize);

        // Attempt to send the data over the TLS connection.
        mbedtlsStatus = mbedtls_ssl_write (
            &(clientSupport->ctxSsl), txDataArray, txDataSize);

        // Remove transmitted data from the buffer.
        if (mbedtlsStatus > 0) {
            GMOS_LOG_FMT (LOG_VERBOSE,
                "MbedTLS completed data write length %d.",
                mbedtlsStatus);
            txDataBufferSize -= mbedtlsStatus;
            gmosBufferRebase (&txDataBuffer, txDataBufferSize);
        }

        // Handle status code processing. For continuation status codes
        // push the residual buffer contents back into the transmit
        // stream for subsequent use.
        else switch (mbedtlsStatus) {
            case 0 :
            case MBEDTLS_ERR_SSL_WANT_WRITE :
            case MBEDTLS_ERR_SSL_ASYNC_IN_PROGRESS :
            case MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS :
                gmosStreamPushBackBuffer (txDataStream, &txDataBuffer);
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (10));
                goto out;
            case MBEDTLS_ERR_SSL_WANT_READ :
                gmosStreamPushBackBuffer (txDataStream, &txDataBuffer);
                taskStatus = GMOS_TASK_SUSPEND;
                goto out;
            default :
                GMOS_LOG_FMT (LOG_DEBUG,
                    "MbedTLS transmit data fault (status 0x%04X).",
                    -mbedtlsStatus);
                txSessionReset = true;
                taskStatus = GMOS_TASK_SUSPEND;
                goto out;
        }
    }
out:
    *sessionReset = txSessionReset;
    return taskStatus;
}

/*
 * Implement data forwarding to the receive data stream. This copies
 * as much data as possible from the the MbedTLS internal buffer to the
 * receive data stream.
 */
static gmosTaskStatus_t gmosMbedtlsClientStreamRxData (
    gmosMbedtlsClient_t* mbedtlsClient, bool *sessionReset)
{
    gmosMbedtlsClientSupport_t* clientSupport =
        (gmosMbedtlsClientSupport_t*) mbedtlsClient->clientSupport;
    gmosStream_t* rxDataStream = &(mbedtlsClient->rxDataStream);
    gmosBuffer_t rxDataBuffer = GMOS_BUFFER_INIT ();
    gmosTaskStatus_t taskStatus = GMOS_TASK_SUSPEND;
    uint_fast16_t rxDataBufferSize = 0;
    uint8_t rxDataArray [256];
    uint_fast16_t rxDataSize;
    int mbedtlsStatus;
    bool rxSessionReset = false;

    // Check that there is sufficient space in the receive data stream
    // to queue a new buffer entry.
    if (gmosStreamGetWriteCapacity (rxDataStream) < sizeof (gmosBuffer_t)) {
        taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (10));
        goto out;
    }

    // Place an arbitrary limit on the amount of data that can be stored
    // in a single receive buffer.
    while (rxDataBufferSize < 2048) {

        // Attempt to extend the receive data buffer in order to
        // allocate memory for the received data.
        rxDataSize = sizeof (rxDataArray);
        if (!gmosBufferExtend (&rxDataBuffer, rxDataSize)) {
            taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (10));
            goto out;
        }

        // Attempt to receive data over the TLS connection.
        mbedtlsStatus = mbedtls_ssl_read (
            &(clientSupport->ctxSsl), rxDataArray, rxDataSize);

        // On receiving new data, copy it to the receive data buffer and
        // update the buffer size if required.
        if (mbedtlsStatus > 0) {
            GMOS_LOG_FMT (LOG_VERBOSE,
                "MbedTLS completed data read length %d.",
                mbedtlsStatus);
            gmosBufferWrite (&rxDataBuffer,
                rxDataBufferSize, rxDataArray, mbedtlsStatus);
            rxDataBufferSize += mbedtlsStatus;
            if ((unsigned int) mbedtlsStatus < rxDataSize) {
                gmosBufferResize (&rxDataBuffer, rxDataBufferSize);
            }
        }

        // Handle status code processing after trimming the buffer back
        // to its original size.
        else {
            gmosBufferResize (&rxDataBuffer, rxDataBufferSize);
            switch (mbedtlsStatus) {
                case 0 :
                case MBEDTLS_ERR_SSL_WANT_WRITE :
                case MBEDTLS_ERR_SSL_ASYNC_IN_PROGRESS :
                case MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS :
                    taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (10));
                    goto out;
                case MBEDTLS_ERR_SSL_WANT_READ :
                    taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (1000));
                    goto out;
                default :
                    GMOS_LOG_FMT (LOG_DEBUG,
                        "MbedTLS receive data fault (status 0x%04X).",
                        -mbedtlsStatus);
                    rxSessionReset = true;
                    taskStatus = GMOS_TASK_SUSPEND;
                    goto out;
            }
        }
    }

    // Append the receive data buffer to the stream if it contains any
    // data.
out :
    if (rxDataBufferSize > 0) {
        gmosStreamSendBuffer (rxDataStream, &rxDataBuffer);
    }
    *sessionReset = rxSessionReset;
    return taskStatus;
}

/*
 * Implement the main MbedTLS client state machine task.
 */
static inline gmosTaskStatus_t gmosMbedtlsClientWorkerTaskFn (
    gmosMbedtlsClient_t* mbedtlsClient)
{
    gmosMbedtlsClientSupport_t* clientSupport =
        (gmosMbedtlsClientSupport_t*) mbedtlsClient->clientSupport;
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    gmosTaskStatus_t txTaskStatus;
    gmosTaskStatus_t rxTaskStatus;
    gmosMbedtlsClientState_t nextState = mbedtlsClient->clientState;
    gmosNetworkStatus_t networkStatus;
    int mbedtlsStatus;
    bool sessionReset;

    // Implement the main MbedTLS client state machine.
    switch (mbedtlsClient->clientState) {

        // Poll the underlying transport link state until it is ready
        // for use or indicates a connection timeout.
        case GMOS_MBEDTLS_CLIENT_STATE_TRANSPORT_CONNECT :
            networkStatus = gmosNetworkLinkMonitor (
                mbedtlsClient->transportLink);
            if (networkStatus == GMOS_NETWORK_STATUS_RETRY) {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (50));
            } else if (networkStatus == GMOS_NETWORK_STATUS_CONNECTED) {
                GMOS_LOG (LOG_DEBUG, "MbedTLS transport link connected.");
                nextState = GMOS_MBEDTLS_CLIENT_STATE_TLS_HANDSHAKE_STEP;
            } else {
                GMOS_LOG_FMT (LOG_DEBUG,
                    "MbedTLS transport link connection failed (status %d).",
                    networkStatus);
                nextState = GMOS_MBEDTLS_CLIENT_STATE_FAILURE;
            }
            break;

        // After the transport layer has connected, attempt to perform
        // the TLS handshake.
        case GMOS_MBEDTLS_CLIENT_STATE_TLS_HANDSHAKE_STEP :
            mbedtlsStatus = mbedtls_ssl_handshake (&(clientSupport->ctxSsl));
            switch (mbedtlsStatus) {
                case 0 :
                    nextState = GMOS_MBEDTLS_CLIENT_STATE_TLS_HANDSHAKE_POLL;
                    break;
                case MBEDTLS_ERR_SSL_WANT_READ :
                case MBEDTLS_ERR_SSL_WANT_WRITE :
                case MBEDTLS_ERR_SSL_ASYNC_IN_PROGRESS :
                case MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS :
                    nextState = GMOS_MBEDTLS_CLIENT_STATE_TLS_HANDSHAKE_POLL;
                    taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (10));
                    break;
                default :
                    GMOS_LOG_FMT (LOG_DEBUG,
                        "MbedTLS handshake failed (status 0x%04X).",
                        -mbedtlsStatus);
                    nextState = GMOS_MBEDTLS_CLIENT_STATE_FAILURE;
                    break;
            }
            break;

        // Check the handshake state to determine whether the handshake
        // has completed.
        case GMOS_MBEDTLS_CLIENT_STATE_TLS_HANDSHAKE_POLL :
            if (mbedtls_ssl_is_handshake_over (&(clientSupport->ctxSsl))) {
                GMOS_LOG (LOG_DEBUG, "MbedTLS handshake complete.");
                nextState = GMOS_MBEDTLS_CLIENT_STATE_TLS_CONNECTED;
                if (mbedtlsClient->networkLink.notifyHandler != 0) {
                    mbedtlsClient->networkLink.notifyHandler (
                        mbedtlsClient->networkLink.notifyContext,
                        GMOS_NETWORK_NOTIFY_CONNECTED);
                }
            } else {
                nextState = GMOS_MBEDTLS_CLIENT_STATE_TLS_HANDSHAKE_STEP;
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (1000));
            }
            break;

        // Forward transmit and receive data in the connected state.
        case GMOS_MBEDTLS_CLIENT_STATE_TLS_CONNECTED :
            txTaskStatus = gmosMbedtlsClientStreamTxData (
                mbedtlsClient, &sessionReset);
            if (!sessionReset) {
                rxTaskStatus = gmosMbedtlsClientStreamRxData (
                    mbedtlsClient, &sessionReset);
            }
            if (!sessionReset) {
                taskStatus = gmosSchedulerPrioritise (
                    txTaskStatus, rxTaskStatus);
            } else {
                nextState = GMOS_MBEDTLS_CLIENT_STATE_TRANSPORT_DISCONNECT;
            }
            break;

        // Handle TLS close request transmission.
        case GMOS_MBEDTLS_CLIENT_STATE_TLS_CLOSE_REQUEST :
            mbedtlsStatus = mbedtls_ssl_close_notify (
                &(clientSupport->ctxSsl));
            switch (mbedtlsStatus) {
                case 0 :
                    GMOS_LOG (LOG_DEBUG, "MbedTLS close request sent.");
                    nextState = GMOS_MBEDTLS_CLIENT_STATE_TLS_CLOSE_POLL;
                    break;
                case MBEDTLS_ERR_SSL_WANT_READ :
                case MBEDTLS_ERR_SSL_WANT_WRITE :
                    taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (10));
                    break;
                default :
                    GMOS_LOG_FMT (LOG_DEBUG,
                        "MbedTLS close request failed (status 0x%04X).",
                        -mbedtlsStatus);
                    nextState = GMOS_MBEDTLS_CLIENT_STATE_FAILURE;
                    break;
            }
            break;

        // Process the receive queue while waiting for the TLS close
        // response.
        case GMOS_MBEDTLS_CLIENT_STATE_TLS_CLOSE_POLL :
            rxTaskStatus = gmosMbedtlsClientStreamRxData (
                mbedtlsClient, &sessionReset);
            if (!sessionReset) {
                taskStatus = rxTaskStatus;
            } else {
                nextState = GMOS_MBEDTLS_CLIENT_STATE_TRANSPORT_DISCONNECT;
            }
            break;

        // Close the underlying transport link if required. This will
        // continue to reset the MbedTLS session regardless of the
        // underlying transport status.
        case GMOS_MBEDTLS_CLIENT_STATE_TRANSPORT_DISCONNECT :
            networkStatus = gmosNetworkLinkDisconnect (
                mbedtlsClient->transportLink);
            if (networkStatus == GMOS_NETWORK_STATUS_RETRY) {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (10));
            } else {
                GMOS_LOG_FMT (LOG_DEBUG,
                    "MbedTLS closed transport link (status %d).",
                    networkStatus);
                nextState = GMOS_MBEDTLS_CLIENT_STATE_SESSION_RESET;
            }
            break;

        // Handle MbedTLS session data resets. This resets the MbedTLS
        // session context ready for reuse, leaving the client in it's
        // configured state.
        case GMOS_MBEDTLS_CLIENT_STATE_SESSION_RESET :
            mbedtlsStatus = mbedtls_ssl_session_reset (
                &(clientSupport->ctxSsl));
            if (mbedtlsStatus == 0) {
                nextState = GMOS_MBEDTLS_CLIENT_STATE_DISCONNECTED;
            } else {
                GMOS_LOG_FMT (LOG_DEBUG,
                    "MbedTLS session reset failed (status 0x%04X).",
                    -mbedtlsStatus);
                nextState = GMOS_MBEDTLS_CLIENT_STATE_FAILURE;
            }
            break;

        // Suspend further task processing in inactive states.
        default :
            taskStatus = GMOS_TASK_SUSPEND;
            break;
    }
    mbedtlsClient->clientState = nextState;
    return taskStatus;
}

/*
 * Provide the MbedTLS client worker task definition.
 */
GMOS_TASK_DEFINITION (gmosMbedtlsClientWorkerTask,
    gmosMbedtlsClientWorkerTaskFn, gmosMbedtlsClient_t);

/*
 * Initialise the MbedTLS client on startup, using the specified network
 * link for the transport layer connection to the server.
 */
bool gmosMbedtlsClientInit (gmosMbedtlsClient_t* mbedtlsClient,
    gmosNetworkLink_t* transportLink)
{
    // Initialise local state.
    mbedtlsClient->transportLink = transportLink;
    mbedtlsClient->clientState = GMOS_MBEDTLS_CLIENT_STATE_INITIALISED;
    gmosBufferInit (&(mbedtlsClient->rxDataBuffer));
    gmosStreamInit (&(mbedtlsClient->txDataStream),
        &(mbedtlsClient->mbedtlsWorkerTask),
        GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE);
    gmosStreamInit (&(mbedtlsClient->rxDataStream), NULL,
        GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE);

    // Set up the network link function and data pointers.
    mbedtlsClient->networkLink.connect = gmosMbedtlsLinkConnector;
    mbedtlsClient->networkLink.disconnect = gmosMbedtlsLinkDisconnector;
    mbedtlsClient->networkLink.send = gmosMbedtlsLinkSender;
    mbedtlsClient->networkLink.receive = gmosMbedtlsLinkReceiver;
    mbedtlsClient->networkLink.monitor = gmosMbedtlsLinkMonitor;
    mbedtlsClient->networkLink.notifyHandler = NULL;
    mbedtlsClient->networkLink.notifyContext = NULL;
    mbedtlsClient->networkLink.consumerTask = NULL;
    mbedtlsClient->mbedtlsConfig = NULL;
    mbedtlsClient->clientSupport = NULL;

    // Set the worker task as the consumer task to wake on transport
    // layer received data.
    gmosNetworkLinkSetConsumerTask (transportLink,
        &(mbedtlsClient->mbedtlsWorkerTask));

    // Run the MbedTLS client task.
    gmosMbedtlsClientWorkerTask_start (
        &(mbedtlsClient->mbedtlsWorkerTask), mbedtlsClient,
        GMOS_TASK_NAME_WRAPPER ("MbedTLS Client"));
    return true;
}

/*
 * Configure the MbedTLS client on startup. This should be called after
 * all setup options have been specified for the client in order to
 * apply the configuration to the MbedTLS client.
 */
bool gmosMbedtlsClientConfigure (gmosMbedtlsClient_t* mbedtlsClient,
    gmosMbedtlsConfig_t* mbedtlsConfig)
{
    bool configuredOk = true;

    // Only run client configuration while in the initialisation state.
    if (mbedtlsClient->clientState !=
        GMOS_MBEDTLS_CLIENT_STATE_INITIALISED) {
        configuredOk = false;
        goto out;
    }

    // Allocate client support memory.
    if (mbedtlsClient->clientSupport == NULL) {
        mbedtlsClient->clientSupport =
            GMOS_MALLOC (sizeof (gmosMbedtlsClientSupport_t));
        if (mbedtlsClient->clientSupport == NULL) {
            configuredOk = false;
            goto out;
        }
    }

    // Lock the new client configuration prior to use.
    if (gmosMbedtlsConfigLock (mbedtlsConfig)) {
        mbedtlsClient->mbedtlsConfig = mbedtlsConfig;
    } else {
        configuredOk = false;
        goto out;
    }

    // Run the configuration setup support routines.
    configuredOk = gmosMbedtlsSupportConfigure (mbedtlsClient);

    // Update the client state or clean up on failure.
out:
    if (configuredOk) {
        mbedtlsClient->clientState =
            GMOS_MBEDTLS_CLIENT_STATE_CONFIGURED;
    } else if (mbedtlsClient->mbedtlsConfig != NULL) {
        gmosMbedtlsConfigUnlock (mbedtlsClient->mbedtlsConfig);
        mbedtlsClient->mbedtlsConfig = NULL;
    }
    return configuredOk;
}

/*
 * Reset the client, removing the current configuration settings.
 */
bool gmosMbedtlsClientReset (gmosMbedtlsClient_t* mbedtlsClient)
{
    bool resetOk = true;
    gmosBuffer_t discardBuffer = GMOS_BUFFER_INIT ();
    gmosStream_t* discardStream;

    // Only run client reset while in the configured state.
    if (mbedtlsClient->clientState !=
        GMOS_MBEDTLS_CLIENT_STATE_CONFIGURED) {
        resetOk = false;
        goto out;
    }

    // Unlock the client configuration prior to reset.
    if (gmosMbedtlsConfigUnlock (mbedtlsClient->mbedtlsConfig)) {
        mbedtlsClient->mbedtlsConfig = NULL;
    } else {
        resetOk = false;
        goto out;
    }

    // Run the client reset support routines.
    gmosMbedtlsSupportReset (mbedtlsClient);

    // Ensure that all buffer and stream memory is released.
    gmosBufferReset (&(mbedtlsClient->rxDataBuffer), 0);
    discardStream = &(mbedtlsClient->txDataStream);
    while (gmosStreamGetReadCapacity (discardStream) > 0) {
        gmosStreamAcceptBuffer (discardStream, &discardBuffer);
        gmosBufferReset (&discardBuffer, 0);
    }
    discardStream = &(mbedtlsClient->rxDataStream);
    while (gmosStreamGetReadCapacity (discardStream) > 0) {
        gmosStreamAcceptBuffer (discardStream, &discardBuffer);
        gmosBufferReset (&discardBuffer, 0);
    }

    // Release the client support memory.
    if (mbedtlsClient->clientSupport != NULL) {
        GMOS_FREE (mbedtlsClient->clientSupport);
        mbedtlsClient->clientSupport = NULL;
    }

    // Update the client state on exit.
out:
    if (resetOk) {
        mbedtlsClient->clientState =
            GMOS_MBEDTLS_CLIENT_STATE_INITIALISED;
    }
    return resetOk;
}
