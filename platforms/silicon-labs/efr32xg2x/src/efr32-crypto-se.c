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
 * This file implements the platform independent API for a selection of
 * widely used cryptographic primitives.
 */

#include <stdint.h>
#include <stdbool.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-crypto.h"
#include "sl_se_manager.h"
#include "sl_se_manager_util.h"
#include "sl_se_manager_entropy.h"
#include "psa/crypto.h"

/*
 * Initialises the cryptography platform abstraction layer.
 */
bool gmosCryptoPalInit (void)
{
    psa_status_t psaStatus;
    sl_se_command_context_t seContext;
    sl_status_t seStatus;
    uint32_t seVersion;

    // Support multiple calls to this initialisation function.
    static bool initOk = false;
    if (!initOk) {

        // Attempt to initialise the secure element and read back the
        // firmware version.
        seStatus = sl_se_init ();
        if (seStatus == SL_STATUS_OK) {
            seStatus = sl_se_get_se_version (&seContext, &seVersion);
        }
        if (seStatus != SL_STATUS_OK) {
            GMOS_LOG_FMT (LOG_WARNING,
                "Failed to initialise EFR32 secure element (status %d).",
                seStatus);
            goto out;
        }

        // Report the firmware version for the secure element. The high
        // order byte is the die ID, which is not reported.
        GMOS_LOG_FMT (LOG_INFO,
            "Initialised EFR32 secure element (firmware version %d.%d.%d).",
            0xFF & (seVersion >> 16), 0xFF & (seVersion >> 8), 0xFF & seVersion);

        // Attempt to initialise the PSA cryptography library.
        psaStatus = psa_crypto_init ();
        if (psaStatus != PSA_SUCCESS) {
            GMOS_LOG_FMT (LOG_WARNING,
                "Failed to initialise PSA crypto library (status %d).",
                psaStatus);
            goto out;
        }
        initOk = true;
    }
out:
    return initOk;
}

/*
 * Requests a block of entropy data from the cryptography platform
 * abstraction layer.
 */
bool gmosCryptoPalEntropyPoll (
    uint8_t* entropy, size_t requestSize, size_t* entropySize)
{
    sl_se_command_context_t seContext;
    sl_status_t seStatus;

    // Attempt to read back the requested number of entropy bytes.
    seStatus = sl_se_get_random (&seContext, entropy, requestSize);
    if (seStatus != SL_STATUS_OK) {
        GMOS_LOG_FMT (LOG_WARNING,
            "Failed to get EFR32 secure element entropy (status %d).",
            seStatus);
        *entropySize = 0;
        return false;
    }

    // The secure element always returns the requested number of entropy
    // bytes.
    *entropySize = requestSize;
    return true;
}
