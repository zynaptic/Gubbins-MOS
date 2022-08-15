/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2022 Zynaptic Limited
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
 * This header provides device specific real time clock driver
 * definitions and functions for the Raspberry Pi RP2040 range of
 * devices.
 */

#ifndef PICO_DRIVER_RTC_H
#define PICO_DRIVER_RTC_H

#include <stdint.h>
#include "gmos-config.h"

// Use RTC software implementation instead of dedicated hardware.
#if !GMOS_CONFIG_RTC_SOFTWARE_EMULATION

/**
 * Defines the platform specific real time clock driver configuration
 * settings data structure.
 */
typedef struct gmosPalRtcConfig_t {

} gmosPalRtcConfig_t;

/**
 * Defines the platform specific real time clock driver dynamic data
 * structure.
 */
typedef struct gmosPalRtcState_t {

    // This is the local time zone indicator. It represents the UTC
    // timezone offset as a signed number of quarter hours, from
    // -12 hours (ie, -48) up to +14 hours (ie, +56).
    int8_t timeZone;

    // This is the daylight saving flag. It is set to zero if daylight
    // saving is not in effect and a non-zero value if daylight saving
    // is active.
    uint8_t daylightSaving;

} gmosPalRtcState_t;

#endif // GMOS_CONFIG_RTC_SOFTWARE_EMULATION
#endif // PICO_DRIVER_RTC_H
