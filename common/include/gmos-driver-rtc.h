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
 * This header defines the portable API for accessing integrated real
 * time clock peripherals.
 */

#ifndef GMOS_DRIVER_RTC_H
#define GMOS_DRIVER_RTC_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * Defines the platform specific RTC driver state data structure. The
 * full type definition must be provided by the associated platform
 * abstraction layer.
 */
typedef struct gmosPalRtcState_t gmosPalRtcState_t;

/**
 * Defines the platform specific RTC driver configuration options. The
 * full type definition must be provided by the associated platform
 * abstraction layer.
 */
typedef struct gmosPalRtcConfig_t gmosPalRtcConfig_t;

/**
 * Defines the GubbinsMOS RTC driver state data structure that is used
 * for managing the low level hardware for a single RTC driver.
 */
typedef struct gmosDriverRtc_t {

    // This is an opaque pointer to the platform abstraction layer data
    // structure that is used for accessing the RTC hardware. The data
    // structure will be platform specific.
    gmosPalRtcState_t* palData;

    // This is an opaque pointer to the platform abstraction layer
    // configuration data structure that is used for setting up the
    // RTC hardware. The data structure will be platform specific.
    const gmosPalRtcConfig_t* palConfig;

} gmosDriverRtc_t;

/**
 * Provides a platform configuration setup macro to be used when
 * allocating an RTC driver data structure. Assigning this macro to
 * an RTC driver data structure on declaration will configure the
 * RTC driver to use the platform specific configuration.
 * @param _palData_ This is a pointer to the platform abstraction layer
 *     data structure that is to be used for accessing the platform
 *     specific hardware.
 * @param _palConfig_ This is a pointer to the platform specific RTC
 *     configuration data structure that defines a set of fixed
 *     configuration options to be used with the RTC.
 */
#define GMOS_DRIVER_RTC_PAL_CONFIG(_palData_, _palConfig_) \
    { _palData_, _palConfig_ }

/**
 * Defines the real time clock time and date data structure that is
 * used for accessing the current time. Most fields use two digit BCD
 * encoded values expressed as 8-bit integers, where the upper nibble
 * represents the most significant BCD digit.
 */
typedef struct gmosDriverRtcTime_t {

    // This is the seconds field, as a BCD value from 0 to 59.
    uint8_t seconds;

    // This is the minutes field, as a BCD value from 0 to 59.
    uint8_t minutes;

    // This is the hours field, as a BCD value from 0 to 23.
    uint8_t hours;

    // This is the day of the week, where 1 represents Monday and 7
    // represents Sunday.
    uint8_t dayOfWeek;

    // This is the day of the month, as a BCD value from 1 to 31.
    uint8_t dayOfMonth;

    // This is the month of the year, as a BCD value from 1 to 12.
    uint8_t month;

    // This is the two digit year, as a BCD value from 0 to 99.
    uint8_t year;

    // This is the local time zone and daylight saving indicator. Bit
    // 7 is the daylight saving flag. Bit 6 is the timezone valid flag.
    // If the timezone valid flag is set, bits 0 to 5 represent the
    // UTC timezone offset as a signed number of quarter hours.
    uint8_t timeZone;

} gmosDriverRtcTime_t;

/**
 * This macro may be used for converting the BCD values stored in the
 * real time data structure into conventional 8-bit integers.
 * @param _bcd_ This is the two digit BCD value to be converted into a
 *     conventional integer.
 * @return Returns a conventional unsigned integer representation of the
 *     two digit BCD value.
 */
#define GMOS_DRIVER_RTC_BCD_TO_INT(_bcd_) \
    ((uint8_t) ((_bcd_ & 0x0F) + 10 * ((_bcd_ >> 4) & 0x0F)))

/**
 * This macro may be used for converting integer values in the range
 * from 0 to 99 into a two digit BCD representation.
 * @param _int_ This is the integer value which is to be converted into
 *     two digit BCD notation. It must be in the range from 0 to 99.
 * @return Returns the two digit BCD notation for the integer value.
 */
#define GMOS_DRIVER_RTC_INT_TO_BCD(_int_) \
    ((uint8_t) ((_int_ / 10) << 4) | (_int_ % 10))

/**
 * Initialises a real time clock for subsequent use. This should be
 * called for each RTC instance prior to accessing it via any of the
 * other API functions.
 * @param rtc This is the RTC data structure that is to be initialised.
 *     It should previously have been configured using the
 *     'GMOS_DRIVER_RTC_PAL_CONFIG' macro.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully setting up the real time clock and 'false' on
 *     failure.
 */
bool gmosDriverRtcInit (gmosDriverRtc_t* rtc);

/**
 * Retrieves the current time and date from the real time clock,
 * populating the current time data structure.
 * @param rtc This is the RTC data structure which is associated with
 *     the real time clock to be accessed.
 * @param currentTime This is an RTC time data structure which will be
 *     populated with the current time and date.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully reading the current time and 'false' if the current
 *     time is not valid - for example, if the real time clock has not
 *     yet been set.
 */
bool gmosDriverRtcGetTime (
    gmosDriverRtc_t* rtc, gmosDriverRtcTime_t* currentTime);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // GMOS_DRIVER_RTC_H
