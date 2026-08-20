/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2020-2026 Zynaptic Limited
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
 * This header provides a common platform independent API for a
 * selection of widely used cryptographic primitives.
 */

#ifndef GMOS_CRYPTO_H
#define GMOS_CRYPTO_H

#include <stdint.h>
#include <stdbool.h>

#include "gmos-config.h"

// Include PSA API support if required.
#if GMOS_CONFIG_CRYPTO_PSA_ENABLED
#include "psa/crypto.h"
#endif

/**
 * Initialises the cryptography platform abstraction layer on startup.
 * @return Returns a boolean value which will be set to 'true' if the
 *     cryptography platform abstraction layer was successfully
 *     initialised and 'false' otherwise.
 */
bool gmosCryptoPalInit (void);

/**
 * Requests a block of entropy data from the cryptography platform
 * abstraction layer.
 * @param entropy This is a pointer to a byte array which will be
 *     populated with entropy data on successful completion.
 * @param requestSize This is the size of the byte array which may be
 *     populated with entropy data.
 * @param entropySize This is a pointer to a size value which will be
 *     set to the number of consecutive bytes of entropy returned in the
 *     byte array.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully generating the entropy data and 'false' if no new
 *     entropy data is available at this time.
 */
bool gmosCryptoPalEntropyPoll (
    uint8_t* entropy, size_t requestSize, size_t* entropySize);

#endif // GMOS_CRYPTO_H
