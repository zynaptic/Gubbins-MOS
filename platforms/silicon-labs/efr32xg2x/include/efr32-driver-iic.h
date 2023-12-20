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
 * This header defines the platform specific data structures used for
 * the Silicon Labs EFR32xG2x IIC (AKA I2C) driver implementation.
 */

#ifndef EFR32_DRIVER_IIC_H
#define EFR32_DRIVER_IIC_H

#include <stdint.h>

/**
 * This enumeration is a list of the I2C interfaces on the device
 * which can support IIC operation.
 */
typedef enum {
    GMOS_PAL_IIC_BUS_ID_I2C0,
    GMOS_PAL_IIC_BUS_ID_I2C1
} gmosPalIicBusId_t;

/**
 * Defines the platform specific IIC interface hardware configuration
 * settings data structure.
 */
typedef struct gmosPalIicBusConfig_t {

    // Specify the GPIO pin used for the IIC clock. The pin must support
    // the IIC SCL alternate function for the selected IIC interface.
    uint16_t sclPinId;

    // Specify the GPIO pin used for the IIC data. The pin must support
    // the IIC SDA alternate function for the selected IIC interface.
    uint16_t sdaPinId;

    // Specify the nominal bus clock frequency to be used for all
    // devices on the bus, expressed as an integer multiple of 1kHz.
    uint16_t iicBusFreq;

    // Specify the EFR32 IIC interface instance to use.
    uint8_t iicInterfaceId;

} gmosPalIicBusConfig_t;

/**
 * Defines the platform specific IIC interface dynamic data structure
 * to be used for the IIC driver.
 */
typedef struct gmosPalIicBusState_t {

} gmosPalIicBusState_t;

#endif // EFR32_DRIVER_IIC_H
