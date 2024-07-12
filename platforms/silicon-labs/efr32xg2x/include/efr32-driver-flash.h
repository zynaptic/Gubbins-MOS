/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2024 Zynaptic Limited
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
 * This file defines the data structures and support functions for the
 * platform specific EFR32xG2x flash memory driver.
 */

#ifndef EFR32_DRIVER_FLASH_H
#define EFR32_DRIVER_FLASH_H

#include <stdint.h>
#include <stdbool.h>

#include "gmos-scheduler.h"
#include "gmos-driver-flash.h"

/**
 * Defines the platform specific flash memory configuration settings
 * data structure to be used for the on-chip flash memory.
 */
typedef struct gmosPalFlashConfigEfr32_t {

    // Specifies the base address of the flash memory area that is
    // available for data storage.
    uint32_t baseAddress;

    // Specifies the amount of flash memory that is available for data
    // storage.
    uint32_t memorySize;

} gmosPalFlashConfigEfr32_t;

/**
 * Defines the platform specific flash memory dynamic data structure
 * to be used for the on-chip flash memory.
 */
typedef struct gmosPalFlashStateEfr32_t {

    // Allocate bulk erase task state.
    gmosTaskState_t bulkEraseTask;

    // Specifies the address of the next flash block to be processed
    // during a bulk erase.
    uint32_t bulkEraseAddr;

} gmosPalFlashStateEfr32_t;

/**
 * Defines the platform specific initialisation function to be used for
 * the on-chip flash memory.
 * @param flash This is the flash memory device data structure that is
 *     to be initialised.
 * @return Returns a boolean value which will be set to 'true' on
 *     successful initialisation and 'false' otherwise.
 */
bool gmosPalFlashInitEfr32 (gmosDriverFlash_t* flash);

#endif // EFR32_DRIVER_FLASH_H
