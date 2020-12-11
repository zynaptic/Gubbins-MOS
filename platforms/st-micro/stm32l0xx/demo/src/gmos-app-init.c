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
 * Implements the main entry point for the STM32L0XX demo application.
 */

#include "gmos-app-config.h"
#include "gmos-platform.h"
#include "gmos-scheduler.h"
#include "gmos-mempool.h"

/*
 * Implements a scheduler lifecycle handler that just prints lifecycle
 * status information to the console.
 */
static bool lifecycleHandler (gmosLifecycleStatus_t lifecycleStatus)
{
    switch (lifecycleStatus) {
        case SCHEDULER_STARTUP :
            GMOS_LOG (LOG_DEBUG, "%08X : SCHEDULER_STARTUP", gmosPalGetTimer());
            break;
        case SCHEDULER_SHUTDOWN :
            GMOS_LOG (LOG_DEBUG, "%08X : SCHEDULER_SHUTDOWN", gmosPalGetTimer());
            break;
        case SCHEDULER_ENTER_POWER_SAVE :
            GMOS_LOG (LOG_DEBUG, "%08X : SCHEDULER_ENTER_POWER_SAVE", gmosPalGetTimer());
            break;
        case SCHEDULER_EXIT_POWER_SAVE :
            GMOS_LOG (LOG_DEBUG, "%08X : SCHEDULER_EXIT_POWER_SAVE", gmosPalGetTimer());
            break;
        case SCHEDULER_ENTER_DEEP_SLEEP :
            GMOS_LOG (LOG_DEBUG, "%08X : SCHEDULER_ENTER_DEEP_SLEEP", gmosPalGetTimer());
            break;
        case SCHEDULER_EXIT_DEEP_SLEEP :
            GMOS_LOG (LOG_DEBUG, "%08X : SCHEDULER_EXIT_DEEP_SLEEP", gmosPalGetTimer());
            break;
    }
    return true;
}

// Allocate and initialise the lifecycle monitor data structure that is
// used to register the lifecycle handler.
static gmosLifecycleMonitor_t lifecycleMonitor = {lifecycleHandler};

/*
 * Sets up the demo application. The main scheduler loop will
 * automatically be started on returning from this function.
 */
void gmosAppInit (void) {

    // Print some information to the debug log.
    GMOS_LOG (LOG_INFO,
        "Initialising GubbinsMOS demo application for STM32L0XX devices");

    // Add callbacks for monitoring the scheduler lifecycle.
    if (GMOS_DEMO_APP_LOG_LIFECYCLE_INFO) {
        gmosLifecycleAddMonitor (&lifecycleMonitor);
    }
}
