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
 * This header defines the public API for managing MbedTLS client
 * configuration instances.
 */

#ifndef GMOS_MBEDTLS_CONFIG_H
#define GMOS_MBEDTLS_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

#include "gmos-config.h"
#include "gmos-driver-eeprom.h"

/**
 * Defines the GubbinsMOS MbedTLS configuration state structure that is
 * used for managing a single MbedTLS client configuration.
 */
typedef struct gmosMbedtlsConfig_t {

    // Reference the configuration support data structure.
    void* configSupport;

    // Implement a configuration lock counter.
    uint8_t lockCount;

} gmosMbedtlsConfig_t;

/**
 * Initialises an MbedTLS configuration data structure ready for use.
 * This should be called once on startup, after which the data structure
 * can be used to create a new MbedTLS configuration.
 * @param mbedtlsConfig This is the MbedTLS configuration instance that
 *     is to be initialised for subsequent use.
 * @return Returns a boolean value which will be set to 'true' on
 *     successful initialisation and 'false' otherwise.
 */
bool gmosMbedtlsConfigInit (gmosMbedtlsConfig_t* mbedtlsConfig);

/**
 * Creates a new MbedTLS configuration, allocating any required
 * resources. On successful completion a new set of MbedTLS
 * configuration settings may be applied for configuring MbedTLS
 * client connections.
 * @param mbedtlsConfig This is the MbedTLS configuration instance for
 *     which the stored configuration is to be created, allocating all
 *     required resources.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully allocating all required resources and 'false'
 *     otherwise.
 */
bool gmosMbedtlsConfigCreate (gmosMbedtlsConfig_t* mbedtlsConfig);

/**
 * Discards an MbedTLS configuration after use, releasing all allocated
 * resources. This can only be called once the configuration has been
 * unlocked, indicating that the configuration is no longer required.
 * @param mbedtlsConfig This is the MbedTLS configuration instance for
 *     which the stored configuration is to be discarded, releasing all
 *     allocated resources.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully releasing all allocated resources and 'false'
 *     otherwise.
 */
bool gmosMbedtlsConfigFree (gmosMbedtlsConfig_t* mbedtlsConfig);

/**
 * Locks an MbedTLS configuration, preventing further configuration
 * changes and preventing the allocated configuration resources from
 * being released. This may be called multiple times, since a lock
 * counter is used to protect the configuration contents.
 * @param mbedtlsConfig This is the MbedTLS configuration instance for
 *     which the stored configuration is to be locked, preventing any
 *     further changes.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully locking the configuration settings and 'false'
 *     otherwise.
 */
bool gmosMbedtlsConfigLock (gmosMbedtlsConfig_t* mbedtlsConfig);

/**
 * Unlocks an MbedTLS configuration, which will allow the allocated
 * configuration resources to be released. This may be called multiple
 * times, since a lock counter is used to protect the configuration
 * contents. This must be called the same number of times as previous
 * calls to gmosMbedtlsConfigLock before the configuration resources can
 * be released using gmosMbedtlsConfigFree.
 * @param mbedtlsConfig This is the MbedTLS configuration instance for
 *     which the stored configuration is to be unlocked, allowing
 *     allocated configuration resources to be released.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully unlocking the configuration settings and 'false'
 *     otherwise.
 */
bool gmosMbedtlsConfigUnlock (gmosMbedtlsConfig_t* mbedtlsConfig);

/**
 * Add a DER encoded CA certificate stored in EEPROM memory to the chain
 * of server certificate authorities that can be trusted by the client.
 * @param mbedtlsConfig This is the MbedTLS configuration instance to
 *     which the server CA certificate is to be added.
 * @param eeprom This is the EEPROM instance which holds the DER encoded
 *     CA certificate.
 * @param certEepromTag This is the EEPROM tag that is used to locate
 *     the DER encoded CA certificate in EEPROM storage.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully adding the server CA certificate and 'false'
 *     otherwise.
 */
bool gmosMbedtlsConfigAddCaCert (gmosMbedtlsConfig_t* mbedtlsConfig,
    gmosDriverEeprom_t* eeprom, gmosDriverEepromTag_t certEepromTag);

/**
 * Adds a PEM encoded CA certificate to the chain of server certificate
 * authorities that can be trusted by the client.
 * @param mbedtlsConfig This is the MbedTLS configuration instance to
 *     which the server CA certificate is to be added.
 * @param certPemData This is a pointer to a string which contains the
 *     PEM encoded server CA certificate.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully adding the server CA certificate and 'false'
 *     otherwise.
 */
bool gmosMbedtlsConfigAddCaCertPem (gmosMbedtlsConfig_t* mbedtlsConfig,
    const char* certPemData);

/**
 * Add a DER encoded certificate stored in EEPROM memory to the chain
 * of client certificates that are used to authenticate the client with
 * the server. The first certificate added should be the device specific
 * certificate and then additional certificates in the chain should be
 * added if required.
 * @param mbedtlsConfig This is the MbedTLS configuration instance to
 *     which the client certificate is to be added.
 * @param eeprom This is the EEPROM instance which holds the DER encoded
 *     client certificate.
 * @param certEepromTag This is the EEPROM tag that is used to locate
 *     the DER encoded client certificate in EEPROM storage.
 * @param keyPairId This is the PSA key pair identifier which is used to
 *     select the local key pair for device certificate authentication.
 *     It is only used when adding the first certificate to the chain.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully adding the client certificate and 'false' otherwise.
 */
bool gmosMbedtlsConfigAddOwnCert (gmosMbedtlsConfig_t* mbedtlsConfig,
    gmosDriverEeprom_t* eeprom, gmosDriverEepromTag_t certEepromTag,
    uint32_t keyPairId);

#endif // GMOS_MBEDTLS_CONFIG_H
