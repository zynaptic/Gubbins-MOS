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
 * time clock peripherals. It supports times and dates from the year
 * 2000 through to 2099.
 */

#ifndef GMOS_DRIVER_RTC_H
#define GMOS_DRIVER_RTC_H

#include <stdint.h>
#include <stdbool.h>
#include "gmos-config.h"
#include "gmos-scheduler.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * This constant defines the offset between UNIX time values based on
 * the UNIX epoch starting in the year 1970 and the UTC time values
 * based on the millenial epoch starting in year 2000, expressed as an
 * integer number of seconds.
 */
#define GMOS_PAL_RTC_UNIX_UTC_TIME_OFFSET 946684800

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

    // This is the reference UTC timestamp from the last RTC
    // synchronisation cycle.
    uint32_t lastRefTimestamp;

    // The is the local RTC UTC timestamp from the last RTC
    // synchronisation cycle.
    uint32_t lastRtcTimestamp;

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

    // This is the two digit year, expressed as a BCD value from 0 to 99
    // and representing years 2000 to 2099.
    uint8_t year;

    // This is the month of the year, as a BCD value from 1 to 12.
    uint8_t month;

    // This is the day of the month, as a BCD value from 1 to 31.
    uint8_t dayOfMonth;

    // This is the day of the week, where 1 represents Monday and 7
    // represents Sunday.
    uint8_t dayOfWeek;

    // This is the hours field, as a BCD value from 0 to 23.
    uint8_t hours;

    // This is the minutes field, as a BCD value from 0 to 59.
    uint8_t minutes;

    // This is the seconds field, as a BCD value from 0 to 59.
    uint8_t seconds;

    // This is the local time zone indicator. It represents the UTC
    // timezone offset as a signed number of quarter hours, from
    // -12 hours (ie, -48) up to +14 hours (ie, +56).
    int8_t timeZone;

    // This is the daylight saving flag. It is set to zero if daylight
    // saving is not in effect and a non-zero value if daylight saving
    // is active.
    uint8_t daylightSaving;

} gmosDriverRtcTime_t;

/**
 * This function may be used for converting the two digit BCD values
 * stored in the real time data structure into conventional 8-bit
 * integers.
 * @param bcd This is the two digit BCD value to be converted into a
 *     conventional integer.
 * @return Returns a conventional unsigned integer representation of the
 *     two digit BCD value.
 */
uint8_t gmosDriverRtcBcdToUint8 (uint8_t bcd);

/**
 * This function may be used for converting integer values in the range
 * from 0 to 99 into a two digit BCD representation.
 * @param value This is the integer value which is to be converted into
 *     two digit BCD notation. It must be in the range from 0 to 99.
 * @return Returns the two digit BCD notation for the integer value.
 */
uint8_t gmosDriverRtcBcdFromUint8 (uint8_t value);

/**
 * This function may be used to convert from a UTC time representation
 * to a BCD encoded format suitable for use with the real time clock.
 * The UTC time value specifies the integer number of seconds since
 * 00:00:00 UTC on the 1st of January 2000.
 * @param rtcTime This is a pointer to an RTC time data structure which
 *     will be populated to match the specified UTC time value.
 * @param utcTime This is the UTC time value that is to be converted to
 *     BCD encoded format.
 * @param timeZone This is the time zone to be used for the RTC time,
 *     represented as a signed offset from UTC in quarter hour
 *     increments in the valid range from -48 to +56.
 * @param daylightSaving This is a boolean flag which should be set to
 *     indicate that daylight saving time is in use (this adds an extra
 *     hour to the time indicated by the base time zone).
 * @return Returns a boolean value which will be set to 'true' if the
 *     specified UTC time value can be represented as a valid RTC time
 *     and 'false' otherwise.
 */
bool gmosDriverRtcConvertFromUtcTime (gmosDriverRtcTime_t* rtcTime,
    uint32_t utcTime, int8_t timeZone, bool daylightSaving);

/**
 * This function may be used to convert from a BCD encoded real time
 * clock time and date representation to a UTC time value. The UTC time
 * value specifies the integer number of seconds since 00:00:00 UTC on
 * the 1st of January 2000.
 * @param rtcTime This is a pointer to an RTC time data structure which
 *     contains the BCD encoded time and date representation.
 * @param utcTime This is a pointer to an unsigned 32-bit integer which
 *     will be populated with the calculated UTC time value.
 * @return Returns a boolean value which will be set to 'true' if the
 *     specified UTC time value can be calculated from the RTC time
 *     and 'false' otherwise.
 */
bool gmosDriverRtcConvertToUtcTime (gmosDriverRtcTime_t* rtcTime,
    uint32_t* utcTime);

/**
 * This function may be used to check that a specified RTC time data
 * structure contains a valid BCD representation of time and date. It
 * also automatically sets the day of week field to the correct value.
 * @param rtcTime This is the real time clock data structure that is to
 *     be checked for a valid time and date representation.
 * @return Returns a boolean value which will be set to 'true' if the
 *     data structure contains a valid BCD encoded time and date and
 *     'false' otherwise.
 */
bool gmosDriverRtcValidateRtcTime (gmosDriverRtcTime_t* rtcTime);

/**
 * Initialises a real time clock for subsequent use. This should be
 * called for each RTC instance prior to accessing it via any of the
 * other API functions.
 * @param rtc This is the RTC data structure that is to be initialised.
 *     It should previously have been configured using the
 *     'GMOS_DRIVER_RTC_PAL_CONFIG' macro.
 * @param calibration This is the initial calibration setting for the
 *     RTC, expressed as parts per 2^20 (about the same as parts per
 *     million). A positive value indicates that the RTC should run
 *     faster than its nominal frequency, and a negative value
 *     indiciates that it should run slower.
 * @param isMainInstance This is a boolean flag, which when set to
 *     'true' indicates that this is the main real time clock instance
 *     that will be used for storing the current system time.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully setting up the real time clock and 'false' on
 *     failure.
 */
bool gmosDriverRtcInit (gmosDriverRtc_t* rtc,
    int32_t calibration, bool isMainInstance);

/**
 * Accesses the main real time clock instance to be used for storing
 * the current system time. For most configurations this will be the
 * only real time clock on the device.
 * @return Returns the main real time clock instance that is to be used
 *     for storing the current system time, or a null reference if no
 *     main real time clock instance has been specified.
 */
gmosDriverRtc_t* gmosDriverRtcGetInstance (void);

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

/**
 * Retrieves the current internal calibration setting for the real time
 * clock.
 * @param rtc This is the RTC data structure which is associated with
 *     the real time clock to be accessed.
 * @return Returns the internal calibration setting for the RTC,
 *     expressed as parts per 2^20 (about the same as parts per
 *     million). A positive value indicates that the RTC is running
 *     faster than its nominal frequency, and a negative value
 *     indiciates that it is running slower.
 */
int32_t gmosDriverRtcGetCalibration (gmosDriverRtc_t* rtc);

/**
 * Assigns the specified time and date to the real time clock,
 * regardless of the current time and date value. The new time value
 * will be checked for a valid time and date.
 * @param rtc This is the RTC data structure which is associated with
 *     the real time clock to be accessed.
 * @param newTime This is an RTC time data structure which is populated
 *     with the time and date that are to be assigned to the real time
 *     clock. The various time and date fields must be valid.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully setting the new time and 'false' on failure.
 */
bool gmosDriverRtcSetTime (
    gmosDriverRtc_t* rtc, gmosDriverRtcTime_t* newTime);

/**
 * Sets the current time zone for the real time clock, using platform
 * specific hardware support when available.
 * @param rtc This is the RTC data structure which is associated with
 *     the real time clock to be accessed.
 * @param timeZone This is the time zone to be used for the RTC time,
 *     represented as a signed offset from UTC in quarter hour
 *     increments in the valid range from -48 to +56.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully setting the time zone and 'false' if the time zone
 *     setting can not be safely updated at this time.
 */
bool gmosDriverRtcSetTimeZone (
    gmosDriverRtc_t* rtc, int8_t timeZone);

/**
 * Sets the daylight saving time for the real time clock, using platform
 * specific hardware support when available.
 * @param rtc This is the RTC data structure which is associated with
 *     the real time clock to be accessed.
 * @param daylightSaving This is a boolean value which when set to
 *     'true' will select dalight saving time and when set to 'false'
 *     will select standard local time.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully setting daylight saving time and 'false' if the
 *     daylight savings setting can not be safely updated at this time.
 */
bool gmosDriverRtcSetDaylightSaving (
    gmosDriverRtc_t* rtc, bool daylightSaving);

/**
 * Attempts to synchronize the real time clock to the specified UTC
 * time value. If there is a significant disparity between the current
 * time and date value this will be equivalent to setting the real time
 * clock value. Otherwise the local clock source may be adjusted to
 * compensate for relative clock drift.
 * @param rtc This is the RTC data structure which is associated with
 *     the real time clock to be accessed.
 * @param utcTime This is a the UTC time value which specifies the
 *     number of seconds that have elapsed since the millenial epoch.
 */
bool gmosDriverRtcSyncTime (
    gmosDriverRtc_t* rtc, uint32_t utcTime);

/**
 * Initialises the real time clock driver platform abstraction layer.
 * This will be called once on startup in order to initialise the
 * platform specific real time clock driver state.
 * @param rtc This is a pointer to the real time clock driver data
 *     structure for the real time clock that is to be initialised.
 * @param calibration This is the initial calibration setting for the
 *     RTC, expressed as parts per 2^20 (about the same as parts per
 *     million). A positive value indicates that the RTC should run
 *     faster than its nominal frequency, and a negative value
 *     indiciates that it should run slower.
 * @return Returns a boolean value which will be set to 'true' on
 *     successful initialisation and 'false' otherwise.
 */
bool gmosPalRtcInit (gmosDriverRtc_t* rtc, int32_t calibration);

/**
 * Assigns the specified time and date to the platform specific real
 * time clock, regardless of the current time and date value. The new
 * time value must specify a valid time and date.
 * @param rtc This is the RTC data structure which is associated with
 *     the real time clock to be accessed.
 * @param newTime This is an RTC time data structure which is populated
 *     with the time and date that are to be assigned to the real time
 *     clock. The various time and date fields must be valid.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully setting the new time and 'false' on failure.
 */
bool gmosPalRtcSetTime (
    gmosDriverRtc_t* rtc, gmosDriverRtcTime_t* newTime);

/**
 * Requests a clock source adjustment from the platform specific real
 * time clock, given the current clock offset and drift relative to the
 * reference clock.
 * @param rtc This is the RTC data structure which is associated with
 *     the real time clock to be adjusted.
 * @param clockOffset This is the number of seconds difference between
 *     the current real time clock and the reference clock. A positive
 *     value implies that the local real time clock is running ahead of
 *     the reference clock.
 * @param clockDrift This is the relative frequency drift between the
 *     local real time clock and the reference clock, expressed in parts
 *     per 2^20 (approximately parts per million). A positive value
 *     implies that the local real time clock is running fast.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully adjusting the local real time clock and 'false' on
 *     failure.
 */
bool gmosPalRtcAdjustClock (
    gmosDriverRtc_t* rtc, int8_t clockOffset, int32_t clockDrift);

// Define the platform abstraction layer data structures for RTC
// software emulation when a dedicated RTC peripheral is not available.
#if GMOS_CONFIG_RTC_SOFTWARE_EMULATION

/**
 * Defines the platform specific real time clock driver configuration
 * settings data structure for software emulation.
 */
typedef struct gmosPalRtcConfig_t {

} gmosPalRtcConfig_t;

/**
 * Defines the platform specific real time clock driver dynamic data
 * structure for software emulation.
 */
typedef struct gmosPalRtcState_t {

    // Allocate memory for the counter update task.
    gmosTaskState_t timerTask;

    // Specify the timestamp of the last sub-second increment, taken
    // from the platform timer.
    uint32_t subSecTimestamp;

    // Specify the sub-second counter. This will wrap once per second,
    // incrementing the RTC seconds counter.
    uint32_t subSecCounter;

    // Specify the per-tick sub-second calibration adjustment. This
    // will be adjusted to track the external reference clock.
    int32_t subSecCalibration;

    // This is the two digit year, expressed as an integer value from
    // 0 to 99 and representing years 2000 to 2099.
    uint8_t year;

    // This is the two digit year, expressed as a BCD value from 0 to
    // 99 and representing years 2000 to 2099.
    uint8_t yearBcd;

    // This is the month of the year, as a BCD value from 1 to 12.
    uint8_t monthBcd;

    // This is the day of the month, as a BCD value from 1 to 31.
    uint8_t dayOfMonthBcd;

    // This is the day of the week, where 1 represents Monday and 7
    // represents Sunday.
    uint8_t dayOfWeek;

    // This is the hours field, as a BCD value from 0 to 23.
    uint8_t hoursBcd;

    // This is the minutes field, as a BCD value from 0 to 59.
    uint8_t minutesBcd;

    // This is the seconds field, as a BCD value from 0 to 59.
    uint8_t secondsBcd;

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

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // GMOS_DRIVER_RTC_H
