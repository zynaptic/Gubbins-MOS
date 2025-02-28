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
 * This header defines the internal API for providing MbedTLS library
 * configuration and support.
 */

#ifndef GMOS_MBEDTLS_SUPPORT_H
#define GMOS_MBEDTLS_SUPPORT_H

#include "gmos-config.h"
#include "gmos-mbedtls-client.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"

/**
 * Specify the dynamically allocated data structure for MbedTLS clients.
 * This includes the MbedTLS data structures which are highly dependent
 * on the MbedTLS library configuration. Wrapping them as a dynamically
 * allocated data structure avoids the risk of using misconfigured
 * static data structures allocated by the application code.
 */
typedef struct gmosMbedtlsClientSupport_t {

    // Allocate the required MbedTLS session context.
    mbedtls_ssl_context ctxSsl;

} gmosMbedtlsClientSupport_t;

/**
 * Specify the dynamically allocated data structure for MbedTLS client
 * configurations. This includes the MbedTLS data structures which are
 * highly dependent on the MbedTLS library configuration. Wrapping them
 * as a dynamically allocated data structure avoids the risk of using
 * misconfigured static data structures allocated by the application
 * code.
 */
typedef struct gmosMbedtlsConfigSupport_t {

    // Allocate the required MbedTLS library components.
    mbedtls_ssl_config       cfgSsl;
    mbedtls_ctr_drbg_context ctxCtrDrbg;
    mbedtls_x509_crt         caCertChain;
    mbedtls_x509_crt         ownCertChain;
    mbedtls_pk_context       ownKeyPair;

} gmosMbedtlsConfigSupport_t;

/**
 * Configures the MbedTLS library support on client setup. This is an
 * internal function that will automatically be called on setup by the
 * main MbedTLS client configuration function.
 * @param mbedtlsClient This is the MbedTLS client instance that is to
 *     be configured.
 * @return Returns a boolean value which will be set to 'true' if the
 *     MbedTLS library support was successfully configured and 'false'
 *     otherwise.
 */
bool gmosMbedtlsSupportConfigure (gmosMbedtlsClient_t* mbedtlsClient);

/**
 * Resets the MbedTLS library support on client state reset. This is an
 * internal function that will automatically be called by the main
 * MbedTLS client reset function.
 * @param mbedtlsClient This is the MbedTLS client instance that is to
 *     be reset.
 */
void gmosMbedtlsSupportReset (gmosMbedtlsClient_t* mbedtlsClient);

/**
 * Gets the common entropy source to use for the MbedTLS client
 * components.
 * @return Returns a pointer to the common MbedTLS entropy source.
 */
mbedtls_entropy_context* gmosMbedtlsSupportGetEntropy (void);

#endif // GMOS_MBEDTLS_SUPPORT_H
