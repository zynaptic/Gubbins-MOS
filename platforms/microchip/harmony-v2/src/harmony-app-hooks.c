/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2020 Zynaptic Limited
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

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-scheduler.h"
#include "gmos-mempool.h"

// Derive the names of the Harmony function hooks.
#define HOOK_NAME_CONCATENATE(_a_, _b_) _a_ ## _b_
#define HOOK_NAME_APPEND(_a_, _b_) HOOK_NAME_CONCATENATE(_a_, _b_)

#define HARMONY_INIT_HOOK_NAME \
    HOOK_NAME_APPEND (GMOS_CONFIG_HARMONY_GMOS_APP_NAME, _Initialize)
#define HARMONY_TASK_HOOK_NAME \
    HOOK_NAME_APPEND (GMOS_CONFIG_HARMONY_GMOS_APP_NAME, _Tasks)

// Specify the 'first run' initialisation flag.
static bool firstRun;

/*
 * Perform GMOS initialisation on Harmony framework startup.
 */
void HARMONY_INIT_HOOK_NAME (void) {
    firstRun = true;
}

/*
 * Process a single GMOS scheduler step on each Harmony framework task
 * tick.
 */
void HARMONY_TASK_HOOK_NAME (void) {
    uint32_t idlePeriod;

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

        // Enter the scheduler loop.
        gmosLifecycleNotify (SCHEDULER_STARTUP);
    }

    // Run a single scheduler step.
    idlePeriod = gmosSchedulerStep ();

    // Idle for the specified period before re-running the task.
    gmosPalIdle (idlePeriod);
}
