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
 * Provides device configuration and setup routines for Microchip/Atmel
 * ATMEGA family devices.
 */

#include <stdint.h>
#include <stdbool.h>

#include "gmos-platform.h"
#include "gmos-mempool.h"
#include "gmos-scheduler.h"
#include "atmega-device.h"

/*
 * The device setup and scheduler loop are all implemented from the
 * main application entry point.
 */
int main(void)
{
    // Initialise the common platform components.
    gmosMempoolInit ();

    // Initialise the platform abstraction layer.
    gmosPalInit ();

    // Initialise the application code.
    gmosAppInit ();

    // Enter the scheduler loop. This is implemented in the 'main'
    // function to avoid adding an extra stack frame.
    gmosLifecycleNotify (SCHEDULER_STARTUP);
    while (true) {
        uint32_t execDelay = 0;
        while (execDelay == 0) {
            execDelay = gmosSchedulerStep ();
        }
        gmosPalIdle (execDelay);
    }
}
