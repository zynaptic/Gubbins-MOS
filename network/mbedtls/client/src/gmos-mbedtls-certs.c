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
 * This file implements the public API for managing MbedTLS local client
 * certificates and associated private keys.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-driver-eeprom.h"
#include "gmos-mbedtls-certs.h"
#include "gmos-mbedtls-support.h"
#include "psa/crypto.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/pk.h"
#include "mbedtls/pem.h"
#include "mbedtls/x509.h"
#include "mbedtls/x509_csr.h"

// Specify standard PEM header and footer strings.
static const char* pemCertHeader = "-----BEGIN CERTIFICATE-----";
static const char* pemCertFooter = "-----END CERTIFICATE-----";

/*
 * Perform PSA error code conversion to equivalent certificate status
 * values.
 */
static gmosMbedtlsCertStatus_t gmosMbedtlsCertConvertPsaErrors (
    psa_status_t psaStatus)
{
    gmosMbedtlsCertStatus_t certStatus;
    switch (psaStatus) {
        case PSA_ERROR_INVALID_ARGUMENT :
            certStatus = GMOS_MBEDTLS_CERT_STATUS_INVALID_ARGUMENT;
            break;
        case PSA_ERROR_BUFFER_TOO_SMALL :
            certStatus = GMOS_MBEDTLS_CERT_STATUS_BUFFER_TOO_SMALL;
            break;
        case PSA_ERROR_NOT_SUPPORTED :
            certStatus = GMOS_MBEDTLS_CERT_STATUS_NOT_SUPPORTED;
            break;
        case PSA_ERROR_NOT_PERMITTED :
            certStatus = GMOS_MBEDTLS_CERT_STATUS_NOT_PERMITTED;
            break;
        case PSA_ERROR_ALREADY_EXISTS :
            certStatus = GMOS_MBEDTLS_CERT_STATUS_ALREADY_EXISTS;
            break;
        case PSA_ERROR_INVALID_HANDLE :
            certStatus = GMOS_MBEDTLS_CERT_STATUS_INVALID_HANDLE;
            break;
        case PSA_ERROR_COMMUNICATION_FAILURE :
            certStatus = GMOS_MBEDTLS_CERT_STATUS_HSM_COMMS_ERROR;
            break;
        case PSA_ERROR_STORAGE_FAILURE :
            certStatus = GMOS_MBEDTLS_CERT_STATUS_HSM_STORAGE_ERROR;
            break;
        case PSA_ERROR_INSUFFICIENT_ENTROPY :
            certStatus = GMOS_MBEDTLS_CERT_STATUS_ENTROPY_FAILURE;
            break;
        case PSA_ERROR_INSUFFICIENT_MEMORY :
            certStatus = GMOS_MBEDTLS_CERT_STATUS_OUT_OF_MEMORY;
            break;
        case PSA_ERROR_CORRUPTION_DETECTED :
        case PSA_ERROR_DATA_INVALID :
        case PSA_ERROR_DATA_CORRUPT :
            certStatus = GMOS_MBEDTLS_CERT_STATUS_HSM_CORRUPTION;
            break;
        default :
            certStatus = GMOS_MBEDTLS_CERT_STATUS_GENERIC_ERROR;
            break;
    }
    return certStatus;
}

/*
 * Perform inline PSA status code conversion to equivalent certificate
 * status values.
 */
static inline gmosMbedtlsCertStatus_t gmosMbedtlsCertConvertPsaStatus (
    psa_status_t psaStatus)
{
    gmosMbedtlsCertStatus_t certStatus;
    if (psaStatus == PSA_SUCCESS) {
        certStatus = GMOS_MBEDTLS_CERT_STATUS_SUCCESS;
    } else {
        certStatus = gmosMbedtlsCertConvertPsaErrors (psaStatus);
    }
    return certStatus;
}

/*
 * Perform MbedTLS error code conversion to equivalent certificate
 * status values.
 */
static gmosMbedtlsCertStatus_t gmosMbedtlsCertConvertMbedErrors (
    int32_t mbedtlsStatus)
{
    gmosMbedtlsCertStatus_t certStatus;
    switch (mbedtlsStatus) {
        case MBEDTLS_ERR_PK_BAD_INPUT_DATA :
        case MBEDTLS_ERR_PEM_INVALID_DATA :
        case MBEDTLS_ERR_PEM_NO_HEADER_FOOTER_PRESENT :
        case MBEDTLS_ERR_PEM_INVALID_ENC_IV :
            certStatus = GMOS_MBEDTLS_CERT_STATUS_INVALID_DATA;
            break;
        case MBEDTLS_ERR_PK_FEATURE_UNAVAILABLE :
        case MBEDTLS_ERR_X509_FEATURE_UNAVAILABLE :
        case MBEDTLS_ERR_PEM_FEATURE_UNAVAILABLE :
        case MBEDTLS_ERR_PEM_UNKNOWN_ENC_ALG :
            certStatus = GMOS_MBEDTLS_CERT_STATUS_NOT_SUPPORTED;
            break;
        case MBEDTLS_ERR_PK_ALLOC_FAILED :
        case MBEDTLS_ERR_X509_ALLOC_FAILED :
        case MBEDTLS_ERR_PEM_ALLOC_FAILED :
            certStatus = GMOS_MBEDTLS_CERT_STATUS_OUT_OF_MEMORY;
            break;
        case MBEDTLS_ERR_X509_BUFFER_TOO_SMALL :
            certStatus = GMOS_MBEDTLS_CERT_STATUS_BUFFER_TOO_SMALL;
            break;
        case MBEDTLS_ERR_X509_FILE_IO_ERROR :
            certStatus = GMOS_MBEDTLS_CERT_STATUS_EEPROM_ACCESS_ERROR;
            break;
        case MBEDTLS_ERR_CTR_DRBG_ENTROPY_SOURCE_FAILED :
            certStatus = GMOS_MBEDTLS_CERT_STATUS_ENTROPY_FAILURE;
            break;
        default :
            certStatus = GMOS_MBEDTLS_CERT_STATUS_GENERIC_ERROR;
            break;
    }
    return certStatus;
}

/*
 * Perform inline MbedTLS status code conversion to equivalent
 * certificate status values.
 */
static inline gmosMbedtlsCertStatus_t gmosMbedtlsCertConvertMbedStatus (
    int32_t mbedtlsStatus)
{
    gmosMbedtlsCertStatus_t certStatus;
    if (mbedtlsStatus == 0) {
        certStatus = GMOS_MBEDTLS_CERT_STATUS_SUCCESS;
    } else {
        certStatus = gmosMbedtlsCertConvertMbedErrors (mbedtlsStatus);
    }
    return certStatus;
}

/*
 * Creates a new PSA key pair for subsequent use in MbedTLS client
 * authentication.
 */
gmosMbedtlsCertStatus_t gmosMbedtlsCertCreateKeyPair (
    uint32_t keyId, gmosMbedtlsCertKeyAlg_t keyAlg)
{
    psa_key_id_t psaKeyId = keyId;
    psa_status_t psaStatus = PSA_SUCCESS;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;

    // Configure the key attributes for the selected key algorithm.
    switch (keyAlg) {

        // Configure attributes for Suite B SECP256R1.
        case GMOS_MBEDTLS_CERT_KEY_ALG_SEPC256R1 :
            psa_set_key_algorithm (&attributes,
                 PSA_ALG_ECDSA (PSA_ALG_SHA_256));
            psa_set_key_type (&attributes,
                PSA_KEY_TYPE_ECC_KEY_PAIR (PSA_ECC_FAMILY_SECP_R1));
            psa_set_key_bits (&attributes, 256);
            break;

        // Unsupported algorithm.
        default :
            psaStatus = PSA_ERROR_NOT_SUPPORTED;
            goto out;
    }

    // Set common key attribute settings and generate key.
    psa_set_key_id (&attributes, psaKeyId);
    psa_set_key_lifetime (&attributes, PSA_KEY_LIFETIME_PERSISTENT);
    psa_set_key_usage_flags (&attributes, PSA_KEY_USAGE_SIGN_HASH);
    psaStatus = psa_generate_key (&attributes, &psaKeyId);
    if ((psaStatus == PSA_SUCCESS) && (psaKeyId != keyId)) {
        GMOS_LOG (LOG_ERROR, "MbedTLS key ID mismatch.");
        psaStatus = PSA_ERROR_INVALID_HANDLE;
    }

    // Clean up on exit and perform status code conversion.
out :
    psa_reset_key_attributes(&attributes);
    return gmosMbedtlsCertConvertPsaStatus (psaStatus);
}

/*
 * Destroys a PSA key pair, removing the key material from private
 * storage.
 */
gmosMbedtlsCertStatus_t gmosMbedtlsCertDestroyKeyPair (uint32_t keyId)
{
    psa_key_id_t psaKeyId = keyId;
    psa_status_t psaStatus = PSA_SUCCESS;

    // Attempt to destroy the specified key.
    psaStatus = psa_destroy_key (psaKeyId);

    // Convert PSA status to certificate status.
    return gmosMbedtlsCertConvertPsaStatus (psaStatus);
}

/*
 * Reads back the public key used for MbedTLS client authentication.
 */
gmosMbedtlsCertStatus_t gmosMbedtlsCertGetPublicKey (uint32_t keyId,
    uint8_t* keyData, size_t keyDataSize, size_t* keyMaterialSize)
{
    psa_key_id_t psaKeyId = keyId;
    psa_status_t psaStatus = PSA_SUCCESS;
    size_t psaKeySize;

    // Export the public key contents and return the key size or the
    // associated error code.
    psaStatus = psa_export_public_key (
        psaKeyId, keyData, keyDataSize, &psaKeySize);

    // Convert PSA status to certificate status.
    if (keyMaterialSize != NULL) {
        *keyMaterialSize = psaKeySize;
    }
    return gmosMbedtlsCertConvertPsaStatus (psaStatus);
}

/*
 * Creates a certificate signing request in PEM format. The request is
 * stored as text in the specified GubbinsMOS buffer.
 */
gmosMbedtlsCertStatus_t gmosMbedtlsCertCreateCsrPem (uint32_t keyId,
    const char *subjectName, gmosBuffer_t* csrBuffer)
{
    int mbedtlsStatus = 0;
    psa_key_id_t psaKeyId = keyId;
    uint8_t csrDrbgSeed [64];
    mbedtls_entropy_context* ctxEntropy;
    mbedtls_ctr_drbg_context ctxCtrDrbg;
    mbedtls_pk_context ctxKeyPair;
    mbedtls_x509write_csr csrWriter;
    size_t keySize;
    mbedtls_md_type_t mdType;
    uint8_t pemData [GMOS_CONFIG_MBEDTLS_MAX_PEM_CERT_SIZE];
    uint8_t* pemEofPtr;
    uint_fast16_t pemDataSize;

    // Initialise the MbedTLS context variables.
    ctxEntropy = gmosMbedtlsSupportGetEntropy ();
    mbedtls_ctr_drbg_init (&ctxCtrDrbg);
    mbedtls_pk_init (&ctxKeyPair);
    mbedtls_x509write_csr_init (&csrWriter);

    // Configure a random number source for use during CSR generation.
    gmosPalGetRandomBytes (csrDrbgSeed, sizeof (csrDrbgSeed));
    mbedtlsStatus = mbedtls_ctr_drbg_seed (&ctxCtrDrbg,
        mbedtls_entropy_func, ctxEntropy, csrDrbgSeed,
        sizeof (csrDrbgSeed));
    if (mbedtlsStatus != 0) {
        goto out;
    }

    // Configure the key pair to use for signing the CSR.
    mbedtlsStatus = mbedtls_pk_setup_opaque (&ctxKeyPair, psaKeyId);
    if (mbedtlsStatus != 0) {
        GMOS_LOG_FMT (LOG_DEBUG,
            "MbedTLS failed to set up key pair (status 0x%04X).",
            -mbedtlsStatus);
        goto out;
    }
    mbedtls_x509write_csr_set_key (&csrWriter, &ctxKeyPair);

    // Select the appropriate message digest to use for signing the CSR.
    keySize = mbedtls_pk_get_bitlen (&ctxKeyPair);
    switch (keySize) {
        case 256 :
            mdType = MBEDTLS_MD_SHA256;
            break;
        case 384 :
            mdType = MBEDTLS_MD_SHA384;
            break;
        default :
            mdType = MBEDTLS_MD_SHA512;
            break;
    }

    // Populate the CSR writer with the required CSR fields.
    mbedtlsStatus = mbedtls_x509write_csr_set_subject_name (
        &csrWriter, subjectName);
    if (mbedtlsStatus != 0) {
        GMOS_LOG_FMT (LOG_DEBUG,
            "MbedTLS failed to set subject name (status 0x%04X).",
            -mbedtlsStatus);
        goto out;
    }
    mbedtls_x509write_csr_set_md_alg (&csrWriter, mdType);

    // Write the certificate to a PEM buffer and check for valid null
    // terminated C string.
    mbedtlsStatus = mbedtls_x509write_csr_pem (&csrWriter,
        pemData, sizeof (pemData), mbedtls_ctr_drbg_random,
        &ctxCtrDrbg);
    if (mbedtlsStatus == 0) {
        pemEofPtr = memchr (pemData, '\0', sizeof (pemData));
        if (pemEofPtr == NULL) {
            mbedtlsStatus = MBEDTLS_ERR_X509_BUFFER_TOO_SMALL;
        } else {
            pemDataSize = (uint_fast16_t) (pemEofPtr - pemData);
        }
    }
    if (mbedtlsStatus != 0) {
        GMOS_LOG_FMT (LOG_DEBUG,
            "MbedTLS failed to write CSR PEM (status 0x%04X).",
            -mbedtlsStatus);
        goto out;
    }

    // Append certificate to the GubbinsMOS output buffer, excluding the
    // null terminator.
    if (!gmosBufferAppend (csrBuffer, pemData, pemDataSize)) {
        mbedtlsStatus = MBEDTLS_ERR_X509_ALLOC_FAILED;
    }

    // Clean up on exit and convert from MbedTLS status values.
out :
    mbedtls_x509write_csr_free (&csrWriter);
    mbedtls_pk_free (&ctxKeyPair);
    mbedtls_ctr_drbg_free (&ctxCtrDrbg);
    return gmosMbedtlsCertConvertMbedStatus (mbedtlsStatus);
}

/*
 * Converts a PEM encoded certificate to DER format for storage in local
 * EEPROM.
 */
gmosMbedtlsCertStatus_t gmosMbedtlsCertStoreCertPem (
    gmosDriverEeprom_t* eeprom, gmosDriverEepromTag_t certEepromTag,
    gmosBuffer_t* certBuffer, uint16_t certBufferOffset,
    bool padRecord, uint16_t* parsedCertSize)
{
    int mbedtlsStatus = 0;
    const uint8_t* derDataSource;
    uint8_t localData [GMOS_CONFIG_MBEDTLS_MAX_PEM_CERT_SIZE];
    size_t pemDataSize;
    size_t derDataSize;
    uint16_t eepromRecordSize;
    mbedtls_pem_context ctxPem;
    size_t parsedSize = 0;
    gmosDriverEepromStatus_t eepromStatus;

    // Initialise the MbedTLS context variables.
    mbedtls_pem_init (&ctxPem);

    // Read the maximum amount of data from the certificate buffer into
    // the local PEM data array and add a null terminator.
    pemDataSize = gmosBufferGetSize (certBuffer) - certBufferOffset;
    if (pemDataSize >= sizeof (localData)) {
        pemDataSize = sizeof (localData) - 1;
    }
    gmosBufferRead (certBuffer, certBufferOffset, localData, pemDataSize);
    localData [pemDataSize] = '\0';

    // Attempt to convert the PEM encoded data to DER format.
    mbedtlsStatus =  mbedtls_pem_read_buffer (&ctxPem, pemCertHeader,
        pemCertFooter, localData, NULL, 0, &parsedSize);
    if (mbedtlsStatus != 0) {
        GMOS_LOG_FMT (LOG_DEBUG,
            "MbedTLS failed to parse PEM certificate (status 0x%04X).",
            -mbedtlsStatus);
        goto out;
    }

    // Copy the contents of the parsed DER data to the local buffer for
    // processing.
    derDataSource = mbedtls_pem_get_buffer (&ctxPem, &derDataSize);
    if ((derDataSource == NULL) || (derDataSize > sizeof (localData)) ||
        (derDataSize > GMOS_CONFIG_MBEDTLS_MAX_DER_CERT_SIZE)) {
        mbedtlsStatus = MBEDTLS_ERR_X509_ALLOC_FAILED;
        GMOS_LOG_FMT (LOG_DEBUG,
            "MbedTLS failed to access DER certificate (status 0x%04X)",
            -mbedtlsStatus);
        goto out;
    }
    memcpy (localData, derDataSource, derDataSize);

    // Optionally pad the EEPROM record to the maximum DER storage size.
    if (padRecord) {
        memset (localData + derDataSize, 0,
            GMOS_CONFIG_MBEDTLS_MAX_DER_CERT_SIZE - derDataSize);
        eepromRecordSize = GMOS_CONFIG_MBEDTLS_MAX_DER_CERT_SIZE;
    } else {
        eepromRecordSize = derDataSize;
    }

    // Initialise the EEPROM record if required.
    eepromStatus = gmosDriverEepromRecordCreate (eeprom,
        certEepromTag, NULL, eepromRecordSize, NULL, NULL);
    if ((eepromStatus != GMOS_DRIVER_EEPROM_STATUS_SUCCESS) &&
        (eepromStatus != GMOS_DRIVER_EEPROM_STATUS_TAG_EXISTS)) {
        GMOS_LOG_FMT (LOG_DEBUG,
            "MbedTLS failed to initialise EEPROM storage (status %d).",
            eepromStatus);
        mbedtlsStatus = MBEDTLS_ERR_X509_FILE_IO_ERROR;
        goto out;
    }

    // Attempt to write the contents of the local buffer to EEPROM.
    eepromStatus = gmosDriverEepromRecordWrite (eeprom,
        certEepromTag, localData, eepromRecordSize, NULL, NULL);
    if (eepromStatus != GMOS_DRIVER_EEPROM_STATUS_SUCCESS) {
        GMOS_LOG_FMT (LOG_DEBUG,
            "MbedTLS failed to write to EEPROM storage (status %d).",
            eepromStatus);
        mbedtlsStatus = MBEDTLS_ERR_X509_FILE_IO_ERROR;
        goto out;
    }
    GMOS_LOG_FMT (LOG_VERBOSE,
        "MbedTLS PEM certificate (%d bytes) written as DER (%d bytes) "
        "to EEPROM tag 0x%X.", parsedSize, derDataSize, certEepromTag);

    // Clean up on exit. Returns the number of parsed PEM data bytes on
    // success.
out:
    mbedtls_pem_free (&ctxPem);
    if (parsedCertSize != NULL) {
        *parsedCertSize = parsedSize;
    }
    return gmosMbedtlsCertConvertMbedStatus (mbedtlsStatus);
}

/*
 * Prints the contents of a PEM encoded entity as debug data. This
 * assumes canonical PEM file formatting.
 */
void gmosMbedtlsCertPrintPemBuffer (gmosPalLogLevel_t logLevel,
    gmosBuffer_t* pemBuffer)
{
    uint8_t lineBuffer [72];
    uint_fast16_t lineOffset;
    uint_fast16_t bufferSize;
    uint_fast16_t readSize;
    uint_fast16_t residualSize;
    uint_fast8_t i;

    // Read one line at a time from the source buffer.
    bufferSize = gmosBufferGetSize (pemBuffer);
    lineOffset = 0;
    while (lineOffset < bufferSize) {
        readSize = sizeof (lineBuffer);
        residualSize = bufferSize - lineOffset;
        if (residualSize < readSize) {
            readSize = residualSize;
        }
        gmosBufferRead (pemBuffer, lineOffset, lineBuffer, readSize);

        // Scan the line, searching for the end of line character and
        // then replacing it with a null terminator.
        for (i = 0; i < sizeof (lineBuffer); i++) {
            if (lineBuffer [i] == '\n') {
                lineBuffer [i] = '\0';
                GMOS_LOG_FMT (logLevel, "%s", lineBuffer);
                break;
            }
        }
        lineOffset += i + 1;
    }
}
