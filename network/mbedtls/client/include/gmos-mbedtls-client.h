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
 * This header defines the public API for accessing an MbedTLS client
 * instance.
 */

#ifndef GMOS_MBEDTLS_CLIENT_H
#define GMOS_MBEDTLS_CLIENT_H

#include <stdint.h>
#include <stdbool.h>

#include "gmos-config.h"
#include "gmos-buffers.h"
#include "gmos-streams.h"
#include "gmos-network-links.h"
#include "gmos-mbedtls-config.h"

/**
 * Support for TLS network links requires heap based memory management.
 */
#if (GMOS_CONFIG_HEAP_SIZE == 0)
#error "TLS network link support requires heap based memory management."
#endif

/**
 * Specify the maximum size of TLS data transmit records. This should
 * usually be set so that a maximum size record fits within the MTU of
 * the underlying transport layer.
 */
#ifndef GMOS_CONFIG_MBEDTLS_MAX_TX_DATA_SIZE
#define GMOS_CONFIG_MBEDTLS_MAX_TX_DATA_SIZE 1280
#endif

/**
 * Defines the GubbinsMOS MbedTLS client state that is used for managing
 * a single MbedTLS client session.
 */
typedef struct gmosMbedtlsClient_t {

    // The MbedTLS client instance implements the network link API.
    gmosNetworkLink_t networkLink;

    // The MbedTLS client requires an underlying network link for data
    // transport.
    gmosNetworkLink_t* transportLink;

    // The MbedTLS client requires a connection configuration.
    gmosMbedtlsConfig_t* mbedtlsConfig;

    // The dynamically allocated client support data structure.
    void* clientSupport;

    // Allocate memory for in the clear transmit data stream.
    gmosStream_t txDataStream;

    // Allocate memory for in the clear receive data stream.
    gmosStream_t rxDataStream;

    // Allocate intermediate receive data buffer storage.
    gmosBuffer_t rxDataBuffer;

    // Allocate the MbedTLS client worker task data structure.
    gmosTaskState_t mbedtlsWorkerTask;

    // Specify the current MbedTLS client state.
    uint8_t clientState;

} gmosMbedtlsClient_t;

/**
 * Initialise the MbedTLS client on startup, using the specified network
 * link for the transport layer connection to the server.
 * @param mbedtlsClient This is the MbedTLS client instance that is to
 *     be initialised.
 * @param transportLink This is a pointer to a previously initialised
 *     network link data structure that should be used as the underlying
 *     MbedTLS transport layer.
 * @return Returns a boolean value which will be set to 'true' if the
 *     MbedTLS client was successfully initialised and 'false'
 *     otherwise.
 */
bool gmosMbedtlsClientInit (gmosMbedtlsClient_t* mbedtlsClient,
    gmosNetworkLink_t* transportLink);

/**
 * Perform MbedTLS client setup on startup or after a reset. This should
 * be called after all configuration options have been specified in
 * order to apply the configuration to the MbedTLS client.
 * @param mbedtlsClient This is the MbedTLS client instance that is to
 *     be configured ready for use.
 * @param mbedtlsConfig This the MbedTLS configuration which will be
 *     used to set up the client. The configuration will be locked until
 *     the client is reset.
 * @return Returns a boolean value which will be set to 'true' if the
 *     MbedTLS client was successfully set up ready for use and 'false'
 *     otherwise.
 */
bool gmosMbedtlsClientConfigure (gmosMbedtlsClient_t* mbedtlsClient,
    gmosMbedtlsConfig_t* mbedtlsConfig);

/**
 * Reset MbedTLS client after use. This will release all allocated
 * resources and allow the associated configuration to be updated if
 * required. In order to reuse the client, the client configuration
 * function should be called with the new configuration settings.
 * @param mbedtlsClient This is the MbedTLS client instance that is to
 *     be reset after use.
 * @return Returns a boolean value which will be set to 'true' if the
 *     MbedTLS client was successfully reset and 'false' otherwise.
 */
bool gmosMbedtlsClientReset (gmosMbedtlsClient_t* mbedtlsClient);

#endif // GMOS_MBEDTLS_CLIENT_H
