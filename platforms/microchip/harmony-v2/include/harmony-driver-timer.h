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
 * This header provides platform specific hardware timer definitions and
 * functions for the Microchip Harmony vendor framework.
 */

#ifndef HARMONY_DRIVER_TIMER_H
#define HARMONY_DRIVER_TIMER_H

// Include the Harmony system and driver headers.
#include "system/common/sys_module.h"
#include "driver/driver.h"
#include "driver/tmr/drv_tmr.h"

/**
 * Defines the platform specific hardware timer configuration settings
 * data structure.
 */
typedef struct gmosPalTimerConfig_t {

    // Specifies the Harmony driver index (for example DRV_TMR_INDEX_1).
    // Note that this corresponds to the SPI driver instance specified
    // in the Harmony configuration tool, not the hardware timer ID.
    SYS_MODULE_INDEX harmonyDeviceIndex;

} gmosPalTimerConfig_t;

/**
 * Defines the platform specific hardware timer dynamic data structure.
 */
typedef struct gmosPalTimerState_t {

    // Identify the Harmony timer interface to be used.
    DRV_HANDLE harmonyDriver;

} gmosPalTimerState_t;

// Select the timer source clock frequency for the target device.
#if defined(__PIC32MZ__)
#define HARMONY_DRIVER_TIMER_CLOCK SYS_CLK_BUS_PERIPHERAL_3

// Device not currently supported.
#else
#error ("Microchip Harmony Target Device Not Supported By Timer Driver");
#endif

#endif // HARMONY_DRIVER_TIMER_H
