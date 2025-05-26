/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2023-2025 Zynaptic Limited
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
 * This header provides the device specific cryptography library
 * configuration for the EFR32xG2x range of devices.
 */

#ifndef EFR32_CRYPTO_CONFIG_H
#define EFR32_CRYPTO_CONFIG_H

// Allow application configuration options to override the default
// settings.
#include "gmos-config.h"

/**
 * This configuration option may be used to enable the required MbedTLS
 * cryptography libraries for OpenThread networking.
 */
#ifndef GMOS_CONFIG_MBEDTLS_OPENTHREAD_SUPPORT
#define GMOS_CONFIG_MBEDTLS_OPENTHREAD_SUPPORT false
#endif

// Provide entropy source using platform specific hardware.
#define MBEDTLS_ENTROPY_C
#define MBEDTLS_NO_PLATFORM_ENTROPY
#define MBEDTLS_ENTROPY_HARDWARE_ALT

// Enable commonly required MbedTLS features.
#define MBEDTLS_BIGNUM_C
#define MBEDTLS_AES_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_CIPHER_C
#define MBEDTLS_CMAC_C
#define MBEDTLS_CTR_DRBG_C

// Enable PSA API support (medium profile with key storage).
#define MBEDTLS_USE_PSA_CRYPTO
#define MBEDTLS_PSA_CRYPTO_C
#define MBEDTLS_PSA_CRYPTO_STORAGE_C
#define MBEDTLS_PSA_CRYPTO_DRIVERS

// Enable PAKE support and associated libraries for the OpenThread
// joining protocol using MBEDTLS_TLS_ECJPAKE_WITH_AES_128_CCM_8.
#if GMOS_CONFIG_MBEDTLS_OPENTHREAD_SUPPORT
#define MBEDTLS_PK_C
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_PK_PARSE_C
#define MBEDTLS_OID_C
#define MBEDTLS_CCM_C
#define MBEDTLS_ECP_C
#define MBEDTLS_MD_C
#define MBEDTLS_HMAC_DRBG_C
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_ECJPAKE_C
#define MBEDTLS_PK_HAVE_JPAKE
#define MBEDTLS_PK_HAVE_CURVE_SECP256R1
#define MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED
#endif

// Enable DTLS support (with TLS 1.2 as a prerequisite) for OpenThread.
#if GMOS_CONFIG_MBEDTLS_OPENTHREAD_SUPPORT
#define MBEDTLS_SSL_TLS_C
#define MBEDTLS_SSL_CLI_C
#define MBEDTLS_SSL_EXPORT_KEYS
#define MBEDTLS_SSL_PROTO_TLS1_2
#define MBEDTLS_SSL_PROTO_DTLS
#endif

// Memory management functions should be explicitly provided by the
// platform.
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_MEMORY
#define MBEDTLS_PLATFORM_CALLOC_MACRO gmosPalCalloc
#define MBEDTLS_PLATFORM_FREE_MACRO   gmosPalFree

// Include the platform specific configuration settings for PSA hardware
// acceleration.
#include "sli_mbedtls_omnipresent.h"
#include "sli_psa_acceleration.h"

// Adjust mismatch between Gecko SDK PSA accelerator settings and new
// MbedTLS options.
#ifdef MBEDTLS_PSA_ACCEL_KEY_TYPE_ECC_KEY_PAIR
#define MBEDTLS_PSA_ACCEL_KEY_TYPE_ECC_KEY_PAIR_BASIC
#define MBEDTLS_PSA_ACCEL_KEY_TYPE_ECC_KEY_PAIR_IMPORT
#define MBEDTLS_PSA_ACCEL_KEY_TYPE_ECC_KEY_PAIR_EXPORT
#endif

#endif // EFR32_CRYPTO_CONFIG_H
