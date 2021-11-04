/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2020-2021 Zynaptic Limited
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
 * This file provides common random number generators that may be used
 * by platforms that do not provide their own random number generator
 * support.
 */

#include "gmos-config.h"
#include "gmos-platform.h"

/*
 * Select the simple XOR based random number generator proposed by
 * David Blackman and Sebastiano Vigna (https://prng.di.unimi.it/).
 * This is intended for use on simple microcontrollers without wide
 * multiplier support.
 */
#if (GMOS_CONFIG_RANDOM_SOUCE == GMOS_RANDOM_SOURCE_XOSHIRO128PP)

// Use an arbitrary seed. This should be modified before use by adding
// entropy to the random number generator.
static uint32_t s[4] = {
    0x0E466F34, 0xA2EA3931, 0xBBC1951E, 0x475D083D
};

// Perform a 32 bit rotation.
static inline uint32_t rotl(const uint32_t x, int k) {
    return (x << k) | (x >> (32 - k));
}

// Get the next 32 bit value from the random number source.
static uint32_t next(void) {
    const uint32_t result = rotl(s[0] + s[3], 7) + s[0];
    const uint32_t t = s[1] << 9;

    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];
    s[2] ^= t;
    s[3] = rotl(s[3], 11);

    return result;
}

/*
 * Provides a platform specific method of adding entropy to the random
 * number generator. Adds entropy to the random number source by XORing
 * the entropy value into the state vector.
 */
void gmosPalAddRandomEntropy (uint32_t randomEntropy)
{
    uint32_t s0 = s[0] ^ randomEntropy;
    if (s0 != 0) {
        s[0] = s0;
    }
}

/*
 * Provides a platform specific random number generator. This will
 * populate a given byte array with the specified number of random
 * bytes.
 */
void gmosPalGetRandomBytes (uint8_t* byteArray, size_t byteArraySize)
{
    size_t i;
    uint32_t randValue = 0;

    for (i = 0; i < byteArraySize; i++) {
        if ((i & 3) == 0) {
            randValue = next ();
        } else {
            randValue >>= 8;
        }
        byteArray [i] = (uint8_t) randValue;
    }
}

#endif // GMOS_RANDOM_SOURCE_XOSHIRO128PP
