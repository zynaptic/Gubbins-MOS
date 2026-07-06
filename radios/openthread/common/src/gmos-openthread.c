/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2023-2026 Zynaptic Limited
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
#include "gmos-openthread-gpio.h"
#include "gmos-openthread-config.h"
#include "openthread-core-config.h"
#include "openthread/instance.h"
#include "openthread/tasklet.h"
#include "openthread/link.h"
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

    // Initialise the GPIO support if required.
    gmosOpenThreadGpioInit (openThreadStack);

    // Initialise the platform specific OpenThread RAL.
    if (!gmosOpenThreadRalInit (openThreadStack)) {
        return false;
    }

    // Only a single OpenThread stack instance is supported.
    openThreadStack->otInstance = otInstanceInitSingle ();

    // Initialise the OpenThread CLI using the debug console.
#if GMOS_CONFIG_OPENTHREAD_ENABLE_INTERACTIVE_CLI
    if (!gmosOpenThreadCliInit (openThreadStack)) {
        return false;
    }
#endif

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
 * Resets the OpenThread stack. This will also force a device reset via
 * the OpenThread platform abstraction layer, so it is not expected to
 * return.
 */
void gmosOpenThreadReset (gmosOpenThreadStack_t* openThreadStack,
    gmosOpenThreadResetType_t resetType)
{
    otInstance* instance = (otInstance*) openThreadStack->otInstance;

    switch (resetType) {
        case GMOS_OPENTHREAD_RESET_TYPE_FACTORY :
            otInstanceFactoryReset (instance);
            break;
        default :
            otInstanceReset (instance);
            break;
    }
}

/*
 * Gets the factory assigned IEEE EUI-64 value for the Thread network
 * interface.
 */
uint64_t gmosOpenThreadGetDeviceEui64 (
    gmosOpenThreadStack_t* openThreadStack)
{
    otInstance* instance = (otInstance*) openThreadStack->otInstance;
    otExtAddress otEui64;
    uint_fast8_t i;
    uint64_t eui64;

    // Convert to a 64-bit integer using network byte order.
    otLinkGetFactoryAssignedIeeeEui64 (instance, &otEui64);
    eui64 = 0;
    for (i = 0; i < 8; i++) {
        eui64 = (eui64 << 8) | otEui64.m8 [i];
    }
    return eui64;
}

/*
 * Gets the currently active Thread network key. This will only return
 * a valid result if the network is currently active.
 */
bool gmosOpenThreadGetNetworkKey (
    gmosOpenThreadStack_t* openThreadStack, uint8_t* networkKey)
{
    otInstance* instance = (otInstance*) openThreadStack->otInstance;
    otOperationalDataset activeDataset;
    otError status;
    uint_fast8_t i;
    bool accessedOk = false;

    // Attempt to access the full active dataset.
    status = otDatasetGetActive (instance, &activeDataset);
    if ((status == OT_ERROR_NONE) &&
        (activeDataset.mComponents.mIsNetworkKeyPresent)) {
        for (i = 0; i < 16; i++) {
            networkKey [i] = activeDataset.mNetworkKey.m8 [i];
        }
        accessedOk = true;
    }
    return accessedOk;
}

/*
 * Gets the currently active Thread network name. This will only return
 * a valid result if the network is currently active.
 */
bool gmosOpenThreadGetNetworkName (
    gmosOpenThreadStack_t* openThreadStack, char* networkName,
    uint8_t networkNameSize)
{
    otInstance* instance = (otInstance*) openThreadStack->otInstance;
    otOperationalDataset activeDataset;
    otError status;
    char nextChar;
    uint_fast8_t i;
    bool accessedOk = false;

    // Attempt to access the full active dataset.
    status = otDatasetGetActive (instance, &activeDataset);
    if ((status == OT_ERROR_NONE) &&
        (activeDataset.mComponents.mIsNetworkNamePresent)) {
        for (i = 0; i < networkNameSize; i++) {
            if (i + 1 == networkNameSize) {
                nextChar = '\0';
            } else {
                nextChar = activeDataset.mNetworkName.m8 [i];
            }
            networkName [i] = nextChar;
            if (nextChar == '\0') {
                break;
            }
        }
        accessedOk = true;
    }
    return accessedOk;
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
