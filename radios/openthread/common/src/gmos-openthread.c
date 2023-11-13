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
 * This file implements the main functions for integrating the
 * OpenThread stack into the GubbinsMOS runtime framework.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-scheduler.h"
#include "gmos-openthread.h"
#include "openthread-core-config.h"
#include "openthread/instance.h"
#include "openthread/tasklet.h"
#include "openthread/random_crypto.h"
#include "openthread/platform/logging.h"

/*
 * Implement the OpenThread stack processing task.
 */
static inline gmosTaskStatus_t gmosOpenThreadTaskFn (
    gmosOpenThreadStack_t* openThreadStack)
{
    gmosTaskStatus_t otTaskStatus;
    gmosTaskStatus_t ralTaskStatus;

    // Process the OpenThread stack tasklets.
    otTaskletsProcess (openThreadStack->otInstance);

    // Process the OpenThread supporting drivers.
    ralTaskStatus = gmosOpenThreadRalTick (openThreadStack);

    // Idle in the background for a short period if no more processing
    // is required. Using background scheduling will allow the device to
    // sleep when possible.
    if (otTaskletsArePending (openThreadStack->otInstance)) {
        otTaskStatus = GMOS_TASK_RUN_IMMEDIATE;
    } else {
        otTaskStatus = GMOS_TASK_RUN_BACKGROUND;
    }
    return gmosSchedulerPrioritise (ralTaskStatus, otTaskStatus);
}

// Define the OpenThread stack processing task.
GMOS_TASK_DEFINITION (gmosOpenThreadTask,
    gmosOpenThreadTaskFn, gmosOpenThreadStack_t);

/*
 * Initialise the OpenThread stack on startup.
 */
bool gmosOpenThreadInit (gmosOpenThreadStack_t* openThreadStack)
{
    uint32_t entropy;
    otError otStatus;

    // Initialise the platform specific OpenThread RAL.
    if (!gmosOpenThreadRalInit (openThreadStack)) {
        return false;
    }

    // Only a single OpenThread stack instance is supported.
    openThreadStack->otInstance = otInstanceInitSingle ();

    // Initialise the OpenThread CLI using the debug console.
    if (!gmosOpenThreadCliInit (openThreadStack)) {
        return false;
    }

    // Initialise the OpenThread network control task.
    if (!gmosOpenThreadNetInit (openThreadStack)) {
        return false;
    }

    // Seed the platform random number generator, which is used for
    // random delay insertion. Use the high quality OpenThread random
    // number generator to add entropy.
    otStatus = otRandomCryptoFillBuffer (
        (uint8_t*) &entropy, sizeof (entropy));
    if (otStatus == OT_ERROR_NONE) {
        gmosPalAddRandomEntropy (entropy);
    }

    // Run the OpenThread processing task.
    gmosOpenThreadTask_start (&(openThreadStack->openThreadTask),
        openThreadStack, "OpenThread Stack");
    return true;
}

/*
 * Hook the OpenThread logging calls into the GubbinsMOS platform
 * logging API.
 */
void otPlatLog (otLogLevel aLogLevel,
    otLogRegion aLogRegion, const char *aFormat, ...)
{
    (void) aLogLevel;
    (void) aLogRegion;
    GMOS_LOG (LOG_INFO, aFormat);
}

/*
 * Hook the OpenThread assertion handling into the GubbinsMOS platform
 * assertion API.
 */
void otPlatAssertFail(const char *aFilename, int aLineNumber)
{
    gmosPalAssertFail (aFilename, aLineNumber,
        "OpenThread Internal Error.");
}

/*
 * Use the standard platform heap for memory allocations.
 */
void *otPlatCAlloc (size_t aNum, size_t aSize)
{
    return GMOS_CALLOC (aNum, aSize);
}

/*
 * Use the standard platform heap for memory free operations.
 */
void otPlatFree (void *aPtr)
{
    GMOS_FREE (aPtr);
}
