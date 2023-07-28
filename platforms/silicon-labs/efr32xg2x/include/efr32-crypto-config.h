/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2023 Zynaptic Limited
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

// The device definition file is required for setting the NVRAM driver
// memory locations.
#include "em_device.h"

// Provide entropy source using platform specific hardware.
#define MBEDTLS_ENTROPY_C
#define MBEDTLS_NO_PLATFORM_ENTROPY
#define MBEDTLS_ENTROPY_HARDWARE_ALT

// Enable standard required MbedTLS features.
#define MBEDTLS_CIPHER_C
#define MBEDTLS_CMAC_C
#define MBEDTLS_CTR_DRBG_C
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C
#define MBEDTLS_OID_C

// Enable PSA API support (medium profile with key storage).
#define MBEDTLS_PSA_CRYPTO_C
#define MBEDTLS_PSA_CRYPTO_STORAGE_C
#define MBEDTLS_PSA_CRYPTO_DRIVERS
#define MBEDTLS_PSA_CRYPTO_CONFIG
#define MBEDTLS_PSA_CRYPTO_CONFIG_FILE "efr32-crypto-config-psa.h"
#include "mbedtls/config_psa.h"

// Include the platform specific configuration settings for PSA hardware
// acceleration.
#include "sli_mbedtls_omnipresent.h"
#include "sli_psa_acceleration.h"

#endif // EFR32_CRYPTO_CONFIG_H
