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

#include "gmos-scheduler.h"
#include "gmos-openthread.h"
#include "platform-efr32.h"
#include "openthread-core-config.h"
#include "openthread/instance.h"

// Specify the external EFR32 driver processing function to use.
void otSysProcessDrivers (otInstance *aInstance);

// Check for a configuration which is suitable for the EFR32 target.
#include "openthread-core-efr32-config-check.h"

/*
 * Initialises the OpenThread radio abstraction layer on startup.
 */
bool gmosOpenThreadRalInit (gmosOpenThreadStack_t* openThreadStack)
{
    // No thread stack configuration options are currently used.
    (void) openThreadStack;

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
