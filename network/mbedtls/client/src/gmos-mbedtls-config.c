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
 * This header implements the public API for managing MbedTLS client
 * configuration instances.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-driver-eeprom.h"
#include "gmos-mbedtls-config.h"
#include "gmos-mbedtls-support.h"
#include "gmos-mbedtls-certs.h"
#include "mbedtls/ssl.h"
#include "mbedtls/pk.h"
#include "mbedtls/debug.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/x509_crt.h"

// Specify special encodings for the configuration lock counter.
#define CONFIG_LOCK_UNALLOCATED   0xFF
#define CONFIG_LOCK_ALLOCATED     0xFE
#define CONFIG_LOCK_MAX_COUNT     0xFD

/*
 * Specify a map of MbedTLS debug levels to their equivalent GubbinsMOS
 * debug levels.
 */
#ifdef MBEDTLS_DEBUG_C
static const uint8_t gmosMbedtlsDebugLevels [] = {
    LOG_ERROR, LOG_WARNING, LOG_DEBUG, LOG_INFO, LOG_VERBOSE };

/*
 * Implement debugging message callback for MbedTLS library.
 */
static void gmosMbedtlsDebug (void *debugContext, int debugLevel,
    const char *sourceFile, int sourceLine, const char *message)
{
    (void) debugContext;
    (void) sourceFile;
    (void) sourceLine;
    uint_fast8_t gmosDebugLevel = 0;

    // Map the MbedTLS debug levels to equivalent GubbinsMOS debug
    // levels.
    if ((debugLevel < 0) ||
        (debugLevel >= (int) sizeof (gmosMbedtlsDebugLevels))) {
        gmosDebugLevel = LOG_ERROR;
    } else {
        gmosDebugLevel = gmosMbedtlsDebugLevels [debugLevel];
    }

    // Log the debug message.
    GMOS_LOG_FMT (gmosDebugLevel, "MbedTLS Debug: %s", message);
}
#endif // MBEDTLS_DEBUG_C

/*
 * Perform MbedTLS SSL configuration setup when the configuration is
 * first locked.
 */
static inline bool gmosMbedtlsConfigSetup (
    gmosMbedtlsConfig_t* mbedtlsConfig)
{
    gmosMbedtlsConfigSupport_t* configSupport =
        (gmosMbedtlsConfigSupport_t*) mbedtlsConfig->configSupport;
    uint8_t seedString [64];
    int32_t mbedtlsStatus;
    bool setupOk = true;

    // Assign default SSL configuration options.
    mbedtlsStatus = mbedtls_ssl_config_defaults (
        &(configSupport->cfgSsl), MBEDTLS_SSL_IS_CLIENT,
        MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
    if (mbedtlsStatus < 0) {
        GMOS_LOG_FMT (LOG_DEBUG,
            "MbedTLS configure defaults failed (status 0x%04X).",
            -mbedtlsStatus);
        setupOk = false;
        goto out;
    }

    // Seed the pseudo random binary sequence generator and add it to
    // the configuration.
    gmosPalGetRandomBytes (seedString, sizeof (seedString));
    mbedtlsStatus = mbedtls_ctr_drbg_seed (&(configSupport->ctxCtrDrbg),
        mbedtls_entropy_func, gmosMbedtlsSupportGetEntropy (), seedString,
        sizeof (seedString));
    if (mbedtlsStatus < 0) {
        GMOS_LOG_FMT (LOG_DEBUG,
            "MbedTLS DRBG seeding failed (status 0x%04X).",
            -mbedtlsStatus);
        setupOk = false;
        goto out;
    }

    // Assign SSL configuration callback for pseudo random number
    // generation.
    mbedtls_ssl_conf_rng (&(configSupport->cfgSsl),
        mbedtls_ctr_drbg_random, &(configSupport->ctxCtrDrbg));

    // Assign SSL configuration callback for debug support.
#ifdef MBEDTLS_DEBUG_C
    mbedtls_ssl_conf_dbg (&(mbedtlsConfig->cfgSsl),
        gmosMbedtlsDebug, NULL);
    mbedtls_debug_set_threshold (MBEDTLS_DEBUG_LEVEL);
    GMOS_LOG_FMT (LOG_DEBUG,
        "MbedTLS set debug level to %d.", MBEDTLS_DEBUG_LEVEL);
#endif

    // Set up server authentication. Always requires verification if a
    // CA certificate chain has been specified during initialisation.
    // A certificate version of 0 is used by the MBedTLS library to
    // indicate that the certificate chain is empty.
    if (configSupport->caCertChain.version == 0) {
        mbedtls_ssl_conf_authmode (&(configSupport->cfgSsl),
            MBEDTLS_SSL_VERIFY_NONE);
        GMOS_LOG (LOG_WARNING, "MbedTLS CA verification is disabled.");
    } else {
        mbedtls_ssl_conf_ca_chain (&(configSupport->cfgSsl),
            &(configSupport->caCertChain), NULL);
        mbedtls_ssl_conf_authmode (&(configSupport->cfgSsl),
            MBEDTLS_SSL_VERIFY_REQUIRED);
        GMOS_LOG (LOG_DEBUG, "MbedTLS CA verification is enabled.");
    }

    // Set up client authentication if required.
    if (configSupport->caCertChain.version == 0) {
        GMOS_LOG (LOG_WARNING, "MbedTLS client verification is disabled.");
    } else {
        mbedtlsStatus = mbedtls_ssl_conf_own_cert (&(configSupport->cfgSsl),
            &(configSupport->ownCertChain), &(configSupport->ownKeyPair));
        if (mbedtlsStatus != 0) {
            GMOS_LOG_FMT (LOG_DEBUG,
                "MbedTLS own certificate setup failed (status 0x%04X).",
                -mbedtlsStatus);
            setupOk = false;
            goto out;
        }
        GMOS_LOG (LOG_DEBUG, "MbedTLS client verification is enabled.");
    }
out:
    return setupOk;
}

/*
 * Initialises an MbedTLS configuration state structure ready for use.
 */
bool gmosMbedtlsConfigInit (gmosMbedtlsConfig_t* mbedtlsConfig)
{
    bool initOk = true;
    psa_status_t psaStatus;

    // Ensure that the PSA cryptography library is initialised.
    psaStatus = psa_crypto_init ();
    if (psaStatus != PSA_SUCCESS) {
        GMOS_LOG_FMT (LOG_DEBUG,
            "MbedTLS PSA initialisation failed (status %d).",
            psaStatus);
        initOk = false;
    }

    // The configuration data structure is unallocated on startup
    mbedtlsConfig->lockCount = CONFIG_LOCK_UNALLOCATED;
    mbedtlsConfig->configSupport = NULL;
    return initOk;
}

/*
 * Creates a new MbedTLS configuration, allocating the required data
 * structure memory.
 */
bool gmosMbedtlsConfigCreate (gmosMbedtlsConfig_t* mbedtlsConfig)
{
    gmosMbedtlsConfigSupport_t* configSupport;
    bool createdOk = true;

    // Allocate memory for configuration data storage.
    if (mbedtlsConfig->configSupport == NULL) {
        mbedtlsConfig->configSupport =
            GMOS_MALLOC (sizeof (gmosMbedtlsConfigSupport_t));
        if (mbedtlsConfig->configSupport == NULL) {
            createdOk = false;
            goto out;
        }
    }
    configSupport =
        (gmosMbedtlsConfigSupport_t*) mbedtlsConfig->configSupport;

    // Initialise MbedTLS components.
    mbedtls_ssl_config_init (&(configSupport->cfgSsl));
    mbedtls_ctr_drbg_init (&(configSupport->ctxCtrDrbg));
    mbedtls_x509_crt_init (&(configSupport->caCertChain));
    mbedtls_x509_crt_init (&(configSupport->ownCertChain));
    mbedtls_pk_init (&(configSupport->ownKeyPair));

    // Reset the configuration lock counter on exit.
out:
    if (createdOk) {
        mbedtlsConfig->lockCount = CONFIG_LOCK_ALLOCATED;
    } else {
        mbedtlsConfig->lockCount = CONFIG_LOCK_UNALLOCATED;
    }
    return createdOk;
}

/*
 * Discards an MbedTLS configuration after use, releasing all allocated
 * resources.
 */
bool gmosMbedtlsConfigFree (gmosMbedtlsConfig_t* mbedtlsConfig)
{
    gmosMbedtlsConfigSupport_t* configSupport =
        (gmosMbedtlsConfigSupport_t*) mbedtlsConfig->configSupport;
    bool freedOk = false;

    // Free resources associated with the MbedTLS components.
    if (mbedtlsConfig->lockCount == 0) {
        if (mbedtlsConfig->configSupport != NULL) {
            mbedtls_ssl_config_free (&(configSupport->cfgSsl));
            mbedtls_ctr_drbg_free (&(configSupport->ctxCtrDrbg));
            mbedtls_x509_crt_free (&(configSupport->caCertChain));
            mbedtls_x509_crt_free (&(configSupport->ownCertChain));
            mbedtls_pk_free (&(configSupport->ownKeyPair));
            GMOS_FREE (mbedtlsConfig->configSupport);
            mbedtlsConfig->configSupport = NULL;
        }
        mbedtlsConfig->lockCount = CONFIG_LOCK_UNALLOCATED;
        freedOk = true;
    }
    return freedOk;
}

/*
 * Locks an MbedTLS configuration, preventing further configuration
 * changes and preventing the allocated configuration resources from
 * being released.
 */
bool gmosMbedtlsConfigLock (gmosMbedtlsConfig_t* mbedtlsConfig)
{
    bool lockedOk = false;

    // Implement configuration setup the first time the configuration is
    // locked after initialisation.
    if (mbedtlsConfig->lockCount == CONFIG_LOCK_ALLOCATED) {
        lockedOk = gmosMbedtlsConfigSetup (mbedtlsConfig);
        if (lockedOk) {
            mbedtlsConfig->lockCount = 1;
        }
    }

    // Increment the lock count if the configuration is used by multiple
    // client instances, up to a maximum of CONFIG_LOCK_MAX_COUNT.
    else if (mbedtlsConfig->lockCount < CONFIG_LOCK_MAX_COUNT) {
        lockedOk = true;
        mbedtlsConfig->lockCount += 1;
    }
    return lockedOk;
}

/*
 * Unlocks an MbedTLS configuration, which will allow the allocated
 * configuration resources to be released.
 */
bool gmosMbedtlsConfigUnlock (gmosMbedtlsConfig_t* mbedtlsConfig)
{
    bool unlockedOk = false;

    // Decrement the configuration lock counter to zero.
    if ((mbedtlsConfig->lockCount > 0) &&
        (mbedtlsConfig->lockCount <= CONFIG_LOCK_MAX_COUNT)) {
        mbedtlsConfig->lockCount -= 1;
        unlockedOk = true;
    }
    return unlockedOk;
}

/*
 * Add a DER encoded CA certificate stored in EEPROM memory to the chain
 * of server certificate authorities that can be trusted by the client.
 */
bool gmosMbedtlsConfigAddCaCert (gmosMbedtlsConfig_t* mbedtlsConfig,
    gmosDriverEeprom_t* eeprom, gmosDriverEepromTag_t certEepromTag)
{
    gmosMbedtlsConfigSupport_t* configSupport =
        (gmosMbedtlsConfigSupport_t*) mbedtlsConfig->configSupport;
    uint8_t localData [GMOS_CONFIG_MBEDTLS_MAX_DER_CERT_SIZE];
    uint16_t recordSize;
    gmosDriverEepromStatus_t eepromStatus;
    int mbedtlsStatus;
    bool addedOk = true;

    // Only support configuration changes in the 'allocated' state.
    if (mbedtlsConfig->lockCount != CONFIG_LOCK_ALLOCATED) {
        addedOk = false;
        goto out;
    }

    // Attempt to copy the DER encoded certificate from EEPROM memory.
    eepromStatus = gmosDriverEepromRecordReadAll (eeprom,
        certEepromTag, localData, sizeof (localData), &recordSize);
    if (eepromStatus != GMOS_DRIVER_EEPROM_STATUS_SUCCESS) {
        GMOS_LOG_FMT (LOG_DEBUG,
            "MbedTLS failed to read EEPROM certificate (status %d).",
            eepromStatus);
        addedOk = false;
        goto out;
    }

    // Parse the DER certificate data.
    mbedtlsStatus = mbedtls_x509_crt_parse_der (
        &(configSupport->caCertChain), localData, recordSize);
    if (mbedtlsStatus < 0) {
        GMOS_LOG_FMT (LOG_WARNING,
            "MbedTLS failed to parse DER certificate (status 0x%04X).",
            -mbedtlsStatus);
        addedOk = false;
    }
out :
    return addedOk;
}

/*
 * Add a PEM encoded CA certificate to the chain of server certificate
 * authorities that can be trusted by the client.
 */
bool gmosMbedtlsConfigAddCaCertPem (gmosMbedtlsConfig_t* mbedtlsConfig,
    const char* certPemData)
{
    gmosMbedtlsConfigSupport_t* configSupport =
        (gmosMbedtlsConfigSupport_t*) mbedtlsConfig->configSupport;
    int mbedtlsStatus;
    size_t pemDataLen = strlen (certPemData);
    bool addedOk = true;

    // Only support configuration changes in the 'allocated' state.
    if (mbedtlsConfig->lockCount != CONFIG_LOCK_ALLOCATED) {
        addedOk = false;
        goto out;
    }

    // Parse the PEM certificate data.
    mbedtlsStatus = mbedtls_x509_crt_parse (&(configSupport->caCertChain),
        (uint8_t*) certPemData, pemDataLen + 1);
    if (mbedtlsStatus < 0) {
        GMOS_LOG_FMT (LOG_WARNING,
            "MbedTLS failed to parse PEM certificate (status 0x%04X).",
            -mbedtlsStatus);
        addedOk = false;
    }
out:
    return addedOk;
}

/*
 * Add a DER encoded CA certificate stored in EEPROM memory to the chain
 * of client certificates that are used to authenticate the client with
 * the server.
 */
bool gmosMbedtlsConfigAddOwnCert (gmosMbedtlsConfig_t* mbedtlsConfig,
    gmosDriverEeprom_t* eeprom, gmosDriverEepromTag_t certEepromTag,
    uint32_t keyPairId)
{
    gmosMbedtlsConfigSupport_t* configSupport =
        (gmosMbedtlsConfigSupport_t*) mbedtlsConfig->configSupport;
    psa_key_id_t psaKeyId = keyPairId;
    uint8_t localData [GMOS_CONFIG_MBEDTLS_MAX_DER_CERT_SIZE];
    uint16_t recordSize;
    gmosDriverEepromStatus_t eepromStatus;
    int mbedtlsStatus;
    bool addedOk = true;

    // Only support configuration changes in the 'allocated' state.
    if (mbedtlsConfig->lockCount != CONFIG_LOCK_ALLOCATED) {
        addedOk = false;
        goto out;
    }

    // A certificate version of 0 is used by the MBedTLS library to
    // indicate that the certificate chain is empty. This means that the
    // specified key pair needs to be added to the configuration.
    if (configSupport->ownCertChain.version == 0) {
        mbedtlsStatus = mbedtls_pk_setup_opaque (
            &(configSupport->ownKeyPair), psaKeyId);
        if (mbedtlsStatus < 0) {
            GMOS_LOG_FMT (LOG_DEBUG,
                "MbedTLS opaque key pair setup failed (status 0x%04X).",
                -mbedtlsStatus);
            addedOk = false;
            goto out;
        }
    }

    // Attempt to copy the DER encoded certificate from EEPROM memory.
    eepromStatus = gmosDriverEepromRecordReadAll (eeprom,
        certEepromTag, localData, sizeof (localData), &recordSize);
    if (eepromStatus != GMOS_DRIVER_EEPROM_STATUS_SUCCESS) {
        GMOS_LOG_FMT (LOG_DEBUG,
            "MbedTLS failed to read EEPROM certificate (status %d).",
            eepromStatus);
        addedOk = false;
        goto out;
    }

    // Parse the DER certificate data.
    mbedtlsStatus = mbedtls_x509_crt_parse_der (
        &(configSupport->ownCertChain), localData, recordSize);
    if (mbedtlsStatus < 0) {
        GMOS_LOG_FMT (LOG_WARNING,
            "MbedTLS failed to parse DER certificate (status 0x%04X).",
            -mbedtlsStatus);
        addedOk = false;
    }
out:
    return addedOk;
}
