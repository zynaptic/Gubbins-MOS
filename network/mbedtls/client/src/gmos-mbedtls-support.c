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
 * This file implements the required support functions for the MbedTLS
 * library.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-mempool.h"
#include "gmos-buffers.h"
#include "gmos-network-links.h"
#include "gmos-mbedtls-client.h"
#include "gmos-mbedtls-support.h"
#include "mbedtls/ssl.h"
#include "mbedtls/net_sockets.h"

/*
 * Implement basic I/O non-blocking transmit request using the
 * underlying transport link.
 */
static int gmosMbedtlsLinkSend (
    void *context, const unsigned char *txData, size_t txDataLen)
{
    gmosMbedtlsClient_t* mbedtlsClient = (gmosMbedtlsClient_t*) context;
    gmosBuffer_t txBuffer = GMOS_BUFFER_INIT ();
    uint_fast16_t txNumSegments;
    gmosNetworkStatus_t networkStatus;
    int retVal = 0;

    // Limit the transmitted data to 1/2 the remaining buffer memory.
    txNumSegments = gmosMempoolSegmentsAvailable () / 2;
    if (txDataLen > txNumSegments * GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE) {
        txDataLen = txNumSegments * GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE;
    }

    // Indicate blocking if no buffer memory is available.
    if (txDataLen == 0) {
        retVal = MBEDTLS_ERR_SSL_WANT_WRITE;
        goto out;
    }

    // Attempt to copy the transmit data to a GubbinsMOS buffer. This
    // should always work due to the prior capacity check.
    gmosBufferAppend (&txBuffer, txData, txDataLen);

    // Attempt to queue the transmit data on the transport link.
    networkStatus = gmosNetworkLinkSend (
        mbedtlsClient->transportLink, &txBuffer);

    // Report successful data transmission or map error conditions to
    // MbedTLS error codes. All error codes are currently used to
    // indicate that the connection has been reset.
    switch (networkStatus) {
        case GMOS_NETWORK_STATUS_SUCCESS :
            retVal = txDataLen;
            break;
        case GMOS_NETWORK_STATUS_RETRY :
            retVal = MBEDTLS_ERR_SSL_WANT_WRITE;
            break;
        default :
            retVal = MBEDTLS_ERR_NET_CONN_RESET;
            break;
    }

    // Ensure that the transmit buffer is empty on exit.
out :
    gmosBufferReset (&txBuffer, 0);
    if (retVal == MBEDTLS_ERR_SSL_WANT_WRITE) {
        GMOS_LOG (LOG_VERBOSE, "MbedTLS link send retry.");
    } else if (retVal < 0) {
        GMOS_LOG_FMT (LOG_DEBUG,
            "MbedTLS link send failed (status 0x%04X).", -retVal);
    } else {
        GMOS_LOG_FMT (LOG_VERBOSE,
            "MbedTLS link send requested %d bytes, accepted %d bytes.",
            txDataLen, retVal);
    }
    return retVal;
}

/*
 * Implement basic I/O non-blocking receive request using the
 * underlying transport link.
 */
static int gmosMbedtlsLinkRecv (
    void *context, unsigned char *rxData, size_t rxDataLen)
{
    gmosMbedtlsClient_t* mbedtlsClient = (gmosMbedtlsClient_t*) context;
    gmosBuffer_t* rxBuffer = &(mbedtlsClient->rxDataBuffer);
    uint_fast16_t rxBufferSize;
    gmosNetworkStatus_t networkStatus;
    size_t copyDataLen;
    int retVal = 0;

    // Either use the existing contents of the receive buffer or attempt
    // to receive new data from the transport layer.
    if (gmosBufferGetSize (rxBuffer) > 0) {
        networkStatus = GMOS_NETWORK_STATUS_SUCCESS;
    } else {
        networkStatus = gmosNetworkLinkReceive (
            mbedtlsClient->transportLink, rxBuffer);
    }

    // If available, copy data from the start of the receive buffer.
    if (networkStatus == GMOS_NETWORK_STATUS_SUCCESS) {
        rxBufferSize = gmosBufferGetSize (rxBuffer);
        copyDataLen = rxDataLen;
        if (copyDataLen > rxBufferSize) {
            copyDataLen = rxBufferSize;
        }
        gmosBufferRead (rxBuffer, 0, rxData, copyDataLen);
        gmosBufferRebase (rxBuffer, rxBufferSize - copyDataLen);
        retVal = copyDataLen;
    }

    // Map error conditions to MbedTLS error codes. All error codes are
    // currently used to indicate that the connection has been reset.
    else switch (networkStatus) {
        case GMOS_NETWORK_STATUS_RETRY :
            retVal = MBEDTLS_ERR_SSL_WANT_READ;
            break;
        default :
            retVal = MBEDTLS_ERR_NET_CONN_RESET;
            break;
    }

    // Add debug tracing for low level transactions.
    if (retVal == MBEDTLS_ERR_SSL_WANT_READ) {
        GMOS_LOG (LOG_VERBOSE, "MbedTLS link receive retry.");
    } else if (retVal < 0) {
        GMOS_LOG_FMT (LOG_DEBUG,
            "MbedTLS link receive failed (status 0x%04X)", -retVal);
    } else {
        GMOS_LOG_FMT (LOG_VERBOSE,
            "MbedTLS link receive requested %d bytes, returned %d bytes.",
            rxDataLen, retVal);
    }
    return retVal;
}

/*
 * Configures the MbedTLS library support on client setup.
 */
bool gmosMbedtlsSupportConfigure (gmosMbedtlsClient_t* mbedtlsClient)
{
    gmosMbedtlsConfig_t* mbedtlsConfig = mbedtlsClient->mbedtlsConfig;
    gmosMbedtlsClientSupport_t* clientSupport =
        (gmosMbedtlsClientSupport_t*) mbedtlsClient->clientSupport;
    gmosMbedtlsConfigSupport_t* configSupport =
        (gmosMbedtlsConfigSupport_t*) mbedtlsConfig->configSupport;
    int32_t mbedtlsStatus;
    bool configuredOk = true;

    // Initialise MbedTLS context.
    mbedtls_ssl_init (&(clientSupport->ctxSsl));

    // Perform SSL context setup using the specified configuration.
    mbedtlsStatus = mbedtls_ssl_setup (&(clientSupport->ctxSsl),
        &(configSupport->cfgSsl));
    if (mbedtlsStatus < 0) {
        GMOS_LOG_FMT (LOG_DEBUG,
            "MbedTLS  SSL setup failed (status 0x%04X).",
            -mbedtlsStatus);
        configuredOk = false;
        goto out;
    }

    // Set up the transport layer non-blocking IO functions.
    mbedtls_ssl_set_bio (&(clientSupport->ctxSsl), mbedtlsClient,
        gmosMbedtlsLinkSend, gmosMbedtlsLinkRecv, NULL);

out:
    return configuredOk;
}

/*
 * Resets the MbedTLS library support on client state reset.
 */
void gmosMbedtlsSupportReset (gmosMbedtlsClient_t* mbedtlsClient)
{
    gmosMbedtlsClientSupport_t* clientSupport =
        (gmosMbedtlsClientSupport_t*) mbedtlsClient->clientSupport;

    // Release the allocated SSL context memory.
    mbedtls_ssl_free (&(clientSupport->ctxSsl));

    // Disable transport layer IO functions.
    mbedtls_ssl_set_bio (&(clientSupport->ctxSsl),
        NULL, NULL, NULL, NULL);
}

/*
 * Gets the entropy source to use for the MbedTLS client components.
 */
mbedtls_entropy_context* gmosMbedtlsSupportGetEntropy (void)
{
    static uint8_t entropyReady = 0;
    static mbedtls_entropy_context ctxEntropy;

    // Implement lazy initialisation of the common entropy component.
    if (!entropyReady) {
        mbedtls_entropy_init (&ctxEntropy);
        entropyReady = 1;
    }
    return &ctxEntropy;
}
