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
 * This header provides device specific LCD driver definitions and
 * functions for the STM32L1XX range of devices.
 */

#ifndef STM32_DRIVER_LCD_H
#define STM32_DRIVER_LCD_H

#include "gmos-config.h"

/**
 * Defines the platform specific LCD driver configuration settings
 * data structure.
 */
typedef struct gmosPalLcdConfig_t {

    // This is a pointer to a list of GPIO pin IDs that need to be
    // configured for use by the LCD controller. The list should be
    // terminated with the invalid pin ID of 0xFFFF.
    const uint16_t* lcdPinList;

    // Include a pointer to the segment mapping table if segment
    // remapping has been enabled in the configuration options. The
    // segment mapping table should be a 64 entry array that is indexed
    // by logical segment IDs and which contains the corresponding
    // driver level segment IDs.
#if GMOS_CONFIG_STM32_LCD_REMAP_SEGMENTS
    const uint8_t* segmentMap;
#endif

    // This is a bit vector that specifies the logical segments that
    // are supported by the LCD panel. Valid segments are indicated by
    // a bit value of 1 and unused segments are indicated by a bit value
    // of 0.
    uint64_t validSegmentMask;

    // Specify the number of common terminals supported by the LCD
    // panel.
    uint8_t numCommonTerminals;

} gmosPalLcdConfig_t;

/**
 * Defines the platform specific LCD driver dynamic data structure.
 */
typedef struct gmosPalLcdState_t {

} gmosPalLcdState_t;

/**
 * Defines the platform specific LCD driver update data structure.
 */
typedef struct gmosPalLcdUpdate_t {

    // This is the segment mask indicating the segments to be modified.
    uint32_t segmentMaskL;
    uint32_t segmentMaskH;

    // This is the segment data which is to be updated.
    uint32_t segmentDataL;
    uint32_t segmentDataH;

    // This is the LCD common terminal to be used in the update.
    uint8_t lcdCommon;

} gmosPalLcdUpdate_t;

#endif // STM32_DRIVER_LCD_H
