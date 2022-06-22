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
 * hooks that are used to invoke the GMOS scheduler from within a
 * host operating system.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-scheduler.h"
#include "gmos-mempool.h"

// These task hook functions are only used when a host operating system
// is in use.
#if GMOS_CONFIG_HOST_OS_SUPPORT

// Include the FreeRTOS headers if required and specify the expected
// task function prototype.
#if (OSAL_USE_RTOS == 9)
#include "FreeRTOS.h"
#include "task.h"
#define HARMONY_TASK_RTOS_THREAD_FN \
    void gmosPalHostOsThreadFn (void* nullPtr)
#endif

/*
 * Implement the RTOS thread function. This will be called once after
 * RTOS thread initialisation and does not exit.
 */
static HARMONY_TASK_RTOS_THREAD_FN
{
    // Initialise the common platform components.
    gmosMempoolInit ();

    // Initialise the platform abstraction layer.
    gmosPalInit ();

    // Initialise the application code.
    gmosAppInit ();

    // Indicate scheduler startup.
    gmosLifecycleNotify (SCHEDULER_STARTUP);

    // Run the scheduler loop.
    while (true) {
        uint32_t execDelay = 0;
        while (execDelay == 0) {
            execDelay = gmosSchedulerStep ();
        }
        gmosPalIdle (execDelay);
    }
}

/*
 * Run the scheduler in an independent FreeRTOS thread. This should be
 * called from the FreeRTOS startup thread to create a new thread
 * context and initiate processing.
 */
#if (OSAL_USE_RTOS == 9)
bool gmosPalHostOsInit (void)
{
    BaseType_t hostOsStatus;
    GMOS_LOG (LOG_INFO,
        "*** Using FreeRTOS as the GubbinsMOS host operating system ***");
    hostOsStatus = xTaskCreate (gmosPalHostOsThreadFn,
        "GubbinsMOS", (GMOS_CONFIG_STACK_SIZE / 4), NULL,
        (configMAX_PRIORITIES - 1), NULL);
    return (hostOsStatus == pdPASS) ? true : false;
}
#endif

#endif // GMOS_CONFIG_HOST_OS_SUPPORT
