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
 * This header provides the device specific PSA cryptography library
 * configuration for the EFR32xG2x range of devices. It enables the
 * standard set of NIST Suite B algorithms that are typically used in
 * embedded applications.
 */

#ifndef EFR32_CRYPTO_CONFIG_PSA_H
#define EFR32_CRYPTO_CONFIG_PSA_H

#define PSA_WANT_ALG_CCM                        1
#define PSA_WANT_ALG_CTR                        1
#define PSA_WANT_ALG_ECB_NO_PADDING             1
#define PSA_WANT_ALG_HKDF                       1
#define PSA_WANT_ALG_CMAC                       1
#define PSA_WANT_ALG_HMAC                       1

#define PSA_WANT_ALG_SHA_224                    1
#define PSA_WANT_ALG_SHA_256                    1

#define PSA_WANT_ALG_ECDH                       1
#define PSA_WANT_ALG_ECDSA                      1
#define PSA_WANT_ALG_DETERMINISTIC_ECDSA        1
#define PSA_WANT_ECC_SECP_R1_256                1

#define PSA_WANT_KEY_TYPE_DERIVE                1
#define PSA_WANT_KEY_TYPE_HMAC                  1
#define PSA_WANT_KEY_TYPE_AES                   1
#define PSA_WANT_KEY_TYPE_ECC_KEY_PAIR          1
#define PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY        1
#define PSA_WANT_KEY_TYPE_RAW_DATA              1

#endif // EFR32_CRYPTO_CONFIG_PSA_H
