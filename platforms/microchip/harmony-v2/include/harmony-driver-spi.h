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
 * This header provides device specific SPI definitions and functions
 * for the Microchip Harmony vendor framework. It supports one or more
 * standard SPI interfaces operating in master mode. The corresponding
 * SPI interfaces must be set up using the Harmony configuration tool
 * for interrupt driven operation and must not be shared with any other
 * Harmony tasks. Note that dynamic SPI clock mode configuration is not
 * supported by the Microchip Harmony framework, so all devices on the
 * same SPI bus must use the SPI clock mode specified using the Harmony
 * configuration tool.
 */

#ifndef HARMONY_DRIVER_SPI_H
#define HARMONY_DRIVER_SPI_H

// Include the Harmony system and driver headers.
#include "system/common/sys_module.h"
#include "driver/driver.h"
#include "driver/spi/drv_spi.h"

/**
 * Defines the platform specific SPI interface hardware configuration
 * settings data structure.
 */
typedef struct gmosPalSpiBusConfig_t {

    // Specifies the Harmony driver index (for example DRV_SPI_INDEX_0).
    // Note that this corresponds to the SPI driver instance specified
    // in the Harmony configuration tool, not the hardware interface ID.
    SYS_MODULE_INDEX harmonyDeviceIndex;

} gmosPalSpiBusConfig_t;

/**
 * Defines the platform specific SPI interface dynamic data structure
 * to be used for the Harmony based SPI driver.
 */
typedef struct gmosPalSpiBusState_t {

    // Identify the Harmony SPI interface to be used.
    DRV_HANDLE harmonyDriver;

    // Specify the Harmony buffer handle for the active transaction.
    DRV_SPI_BUFFER_HANDLE harmonyBuffer;

} gmosPalSpiBusState_t;

#endif // HARMONY_DRIVER_SPI_H
