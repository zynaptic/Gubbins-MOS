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

#include <stdbool.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-scheduler.h"
#include "gmos-openthread.h"
#include "platform-efr32.h"
#include "openthread-core-config.h"
#include "openthread/instance.h"
#include "pa_conversions_efr32.h"

// Specify the external EFR32 driver processing function to use.
void otSysProcessDrivers (otInstance *aInstance);

// Specify the RAIL module unlock function to use.
void RAIL_UnlockModule (uint32_t);

// Check for a configuration which is suitable for the EFR32 target.
#include "openthread-core-efr32-config-check.h"

/*
 * Initialises the OpenThread radio abstraction layer on startup.
 */
bool gmosOpenThreadRalInit (gmosOpenThreadStack_t* openThreadStack)
{
    // No thread stack configuration options are currently used.
    (void) openThreadStack;

    // This may be required to unlock radio modules on first use.
    // For Simplicity Studio builds, this is normally called via
    // a library stub function.
    RAIL_UnlockModule (0xec450369);

    // Initialise the required EFR32 platform radio components.
    sl_rail_util_pa_init ();

    // Placeholders for future radio features. FEM, PTI and RSSI offset
    // support are not required on existing boards.
    // sl_fem_util_init ();
    // sl_rail_util_pti_init ();
    // sl_rail_util_rssi_init ();

    // Initialise the EFR32 platform abstraction layer. This is
    // equivalent to calling the conventional OpenThread otSysInit call
    // without the command line arguments.
    sl_ot_sys_init ();

    return true;
}

/*
 * Implements the processing tick function for the OpenThread radio
 * abstraction layer.
 */
gmosTaskStatus_t gmosOpenThreadRalTick (
    gmosOpenThreadStack_t* openThreadStack)
{
    // Process the OpenThread supporting drivers.
    otSysProcessDrivers (openThreadStack->otInstance);

    // The EFR32 radio abstraction layer does not impose any additional
    // scheduling requirements.
    return GMOS_TASK_SUSPEND;
}

/*
 * Implement RAIL library callback function for assertion reporting.
 */
void RAILCb_AssertFailed (
    RAIL_Handle_t railHandle, RAIL_AssertErrorCodes_t errorCode)
{
    (void) railHandle;
    if (GMOS_CONFIG_LOG_LEVEL == LOG_VERBOSE) {
        static const char* railErrorMessages[] = RAIL_ASSERT_ERROR_MESSAGES;
        const char *errorMessage = "Unknown";
        if (errorCode < (sizeof(railErrorMessages) / sizeof(char*))) {
            errorMessage = railErrorMessages[errorCode];
        }
        GMOS_LOG_FMT (LOG_ERROR, "RAIL Assertion Error: %s", errorMessage);
    } else {
        GMOS_LOG_FMT (LOG_ERROR, "RAIL Assertion Error: %d", errorCode);
    }
    GMOS_ASSERT_FAIL ("RAIL Assertion Error.")
}
