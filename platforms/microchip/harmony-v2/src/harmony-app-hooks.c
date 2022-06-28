/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2020-2022 Zynaptic Limited
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
 * Implements the Microchip Harmony vendor framework application task
 * hooks that are used to invoke the GMOS scheduler from within the
 * main Harmony execution loop.
 */

#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-scheduler.h"
#include "gmos-mempool.h"

// These task hook functions are not used when a host operating system
// is in use.
#if !GMOS_CONFIG_HOST_OS_SUPPORT

// Specify the 'first run' initialisation flag.
static bool firstRun;

/*
 * Perform GMOS initialisation on Harmony framework startup.
 */
void GMOS_SCHEDULER_APP_Initialize (void) {
    firstRun = true;
}

/*
 * Process a single GMOS scheduler step on each Harmony framework task
 * tick.
 */
void GMOS_SCHEDULER_APP_Tasks (void) {

    // Perform application initialisation on first run. This approach
    // ensures that all the Harmony system tasks have been initialised
    // prior to setting up the GMOS infrastructure.
    if (firstRun) {
        firstRun = false;

        // Initialise the common platform components.
        gmosMempoolInit ();

        // Initialise the platform abstraction layer.
        gmosPalInit ();

        // Initialise the application code.
        gmosAppInit ();

        // Indicate scheduler startup.
        gmosLifecycleNotify (SCHEDULER_STARTUP);
    }

    // Run a single scheduler step. Do not use the idle function, since
    // other Harmony framework tasks will still need to run.
    gmosSchedulerStep ();
}

/*
 * Implement the main Harmony event loop.
 */
int main (void)
{
    // Initialize all MPLAB Harmony modules, including the GubbinsMOS
    // scheduler.
    SYS_Initialize (NULL);

    // Maintain state machines of all polled MPLAB Harmony modules.
    while (true) {
        SYS_Tasks ();
    }

    // The main function should never exit.
    return -1;
}

#endif // GMOS_CONFIG_HOST_OS_SUPPORT
