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
 * This file implements the common functionality for accessing
 * integrated hardware or software emulated real time clocks.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-driver-rtc.h"

/*
 * Specify the time synchronization tracking window as an integer number
 * of seconds.
 */
#define GMOS_DRIVER_RTC_TRACKING_WINDOW 10

/*
 * Store the standard month lengths.
 */
static uint8_t monthLengths [] = {
    31, // January.
    28, // February.
    31, // March.
    30, // April.
    31, // May.
    30, // June.
    31, // July.
    31, // August.
    30, // September.
    31, // October.
    30, // November.
    31, // December.
};

/*
 * Specify the main real time clock instance that will be used for
 * storing the current system time.
 */
static gmosDriverRtc_t* mainInstance = NULL;

/*
 * This function may be used to validate a two digit BCD value to
 * ensure that it contains a valid BCD number.
 */
static bool validateBcdValue (uint8_t bcd)
{
    return (((bcd & 0x0F) <= 0x09) && ((bcd & 0xF0) <= 0x90)) ?
        true : false;
}

/*
 * Get the number of days that have elapsed since the UTC millenium
 * reference date.
 */
static uint32_t getElapsedDays (gmosDriverRtcTime_t* rtcTime)
{
    uint32_t yearCount;
    uint32_t yearDays;
    uint32_t monthCount;
    uint32_t monthDays;
    bool isLeapYear;

    // Derive the number of days that have elapsed due to an integer
    // number of leap year cycles.
    yearCount = gmosDriverRtcBcdToUint8 (rtcTime->year);
    yearDays = (yearCount / 4) * (366 + 3 * 365);
    switch (yearCount & 0x03) {
        case 0 :
            isLeapYear = true;
            break;
        case 1 :
            yearDays += 366;
            isLeapYear = false;
            break;
        case 2 :
            yearDays += 366 + 365;
            isLeapYear = false;
            break;
        default :
            yearDays += 366 + 2 * 365;
            isLeapYear = false;
            break;
    }

    // Derive the number of days that have elapsed after a set number
    // of months.
    monthCount = gmosDriverRtcBcdToUint8 (rtcTime->month);
    monthDays = 0;
    for (monthCount = monthCount - 1; monthCount > 0; monthCount --) {
        uint8_t monthLength = monthLengths [monthCount - 1];
        if (isLeapYear && (monthCount == 2)) {
            monthLength += 1;
        }
        monthDays += monthLength;
    }

    // Derive the total number of elapsed full days.
    return (yearDays + monthDays +
        gmosDriverRtcBcdToUint8 (rtcTime->dayOfMonth) - 1);
}

/*
 * This function may be used for converting the two digit BCD values
 * stored in the real time data structure into conventional 8-bit
 * integers.
 */
uint8_t gmosDriverRtcBcdToUint8 (uint8_t bcd)
{
    return ((bcd & 0x0F) + 10 * ((bcd >> 4) & 0x0F));
}

/*
 * This function may be used for converting integer values in the range
 * from 0 to 99 into a two digit BCD representation.
 */
uint8_t gmosDriverRtcBcdFromUint8 (uint8_t value)
{
    return (((value / 10) << 4) | (value % 10));
}

/*
 * This function may be used to convert from a UTC time representation
 * to a BCD encoded format suitable for use with the real time clock.
 * The UTC time value specifies the integer number of seconds since
 * 00:00:00 UTC on the 1st of January 2000.
 */
bool gmosDriverRtcConvertFromUtcTime (gmosDriverRtcTime_t* rtcTime,
    uint32_t utcTime, int8_t timeZone, bool daylightSaving)
{
    uint32_t localTime;
    uint32_t localDays;
    uint32_t localSeconds;
    uint32_t yearCount;
    uint32_t yearDays;
    uint32_t monthCount;
    uint32_t monthDays;
    uint32_t timeSeconds;
    uint32_t timeMinutes;
    uint32_t timeHours;
    bool isLeapYear;

    // Convert from UTC to local time. Time zones from -12 to +14 hours
    // are supported in quarter hour increments.
    if ((timeZone < -48) || (timeZone > 56)) {
        return false;
    }
    localTime = utcTime + ((int32_t) timeZone) * (15 * 60);
    if (daylightSaving) {
        localTime += 60 * 60;
    }

    // Derive the number of days since 1st of January 2000, and the
    // number of seconds in the day that have elapsed.
    localDays = localTime / (24 * 60 * 60);
    localSeconds = localTime - localDays * (24 * 60 * 60);

    // All years in the range 2000 to 2099 can use the basic four year
    // leap year cycle.
    yearCount = localDays / (366 + 3 * 365);
    yearDays = localDays - yearCount * (366 + 3 * 365);
    if (yearCount >= 25) {
        return false;
    }
    if (yearDays < 366) {
        yearCount = yearCount * 4;
        isLeapYear = true;
    } else {
        yearDays -= 366;
        yearCount = yearCount * 4 + 1;
        isLeapYear = false;
        while (yearDays >= 365) {
            yearDays -= 365;
            yearCount += 1;
        }
    }
    rtcTime->year = gmosDriverRtcBcdFromUint8 (yearCount);

    // Determine the month and day.
    monthDays = yearDays;
    for (monthCount = 1; monthCount <= 12; monthCount++) {
        uint8_t monthLength = monthLengths [monthCount - 1];
        if (isLeapYear && (monthCount == 2)) {
            monthLength += 1;
        }
        if (monthDays >= monthLength) {
            monthDays -= monthLength;
        } else {
            break;
        }
    }
    rtcTime->month = gmosDriverRtcBcdFromUint8 (monthCount);
    rtcTime->dayOfMonth = gmosDriverRtcBcdFromUint8 (monthDays + 1);

    // The day of the week can be derived directly from the number of
    // days since 1st of January 2000, which was a Saturday (day 6).
    rtcTime->dayOfWeek = 1 + (localDays + 5) % 7;

    // Derive the time as a 24-hour representation.
    timeHours = localSeconds / (60 * 60);
    timeSeconds = localSeconds - timeHours * (60 * 60);
    timeMinutes = timeSeconds / 60;
    timeSeconds -= timeMinutes * 60;
    rtcTime->hours = gmosDriverRtcBcdFromUint8 (timeHours);
    rtcTime->minutes = gmosDriverRtcBcdFromUint8 (timeMinutes);
    rtcTime->seconds = gmosDriverRtcBcdFromUint8 (timeSeconds);

    // Set the current time zone.
    rtcTime->timeZone = timeZone;
    rtcTime->daylightSaving = daylightSaving ? 1 : 0;
    return true;
}

/*
 * This function may be used to convert from a BCD encoded real time
 * clock time and date representation to a UTC time value. The UTC time
 * value specifies the integer number of seconds since 00:00:00 UTC on
 * the 1st of January 2000.
 */
bool gmosDriverRtcConvertToUtcTime (gmosDriverRtcTime_t* rtcTime,
    uint32_t* utcTime)
{
    uint32_t localSeconds;
    int32_t timeZoneAdjustment;

    // Derive the number of seconds that have elapsed for full days.
    localSeconds = getElapsedDays (rtcTime) * (24 * 60 * 60);

    // Derive the number of seconds in the day which have elapsed.
    localSeconds += 60 * 60 * gmosDriverRtcBcdToUint8 (rtcTime->hours);
    localSeconds += 60 * gmosDriverRtcBcdToUint8 (rtcTime->minutes);
    localSeconds += gmosDriverRtcBcdToUint8 (rtcTime->seconds);

    // Apply time zone correction.
    timeZoneAdjustment = ((int32_t) rtcTime->timeZone) * (15 * 60);
    if (rtcTime->daylightSaving != 0) {
        timeZoneAdjustment += 60 * 60;
    }
    if ((timeZoneAdjustment > 0) &&
        ((uint32_t) timeZoneAdjustment > localSeconds)) {
        return false;
    } else {
        *utcTime = localSeconds - timeZoneAdjustment;
        return true;
    }
}

/*
 * This function may be used to check that a specified RTC time data
 * structure contains a valid BCD representation of time and date. It
 * also automatically sets the day of week field to the correct value.
 */
bool gmosDriverRtcValidateRtcTime (gmosDriverRtcTime_t* rtcTime)
{
    uint8_t year;
    uint8_t month;
    uint8_t dayOfMonth;
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
    uint8_t monthLength;

    // Check for valid BCD representations.
    if ((!validateBcdValue (rtcTime->year)) ||
        (!validateBcdValue (rtcTime->month)) ||
        (!validateBcdValue (rtcTime->dayOfMonth)) ||
        (!validateBcdValue (rtcTime->hours)) ||
        (!validateBcdValue (rtcTime->minutes)) ||
        (!validateBcdValue (rtcTime->seconds))) {
        return false;
    }

    // Get integer values.
    year = gmosDriverRtcBcdToUint8 (rtcTime->year);
    month = gmosDriverRtcBcdToUint8 (rtcTime->month);
    dayOfMonth = gmosDriverRtcBcdToUint8 (rtcTime->dayOfMonth);
    hours = gmosDriverRtcBcdToUint8 (rtcTime->hours);
    minutes = gmosDriverRtcBcdToUint8 (rtcTime->minutes);
    seconds = gmosDriverRtcBcdToUint8 (rtcTime->seconds);

    // Range check fixed fields.
    if ((hours >= 24) || (minutes >= 60) || (seconds >= 60) ||
        (month < 1) || (month > 12)) {
        return false;
    }

    // Range check day of month.
    monthLength = monthLengths [month - 1];
    if (((year & 0x03) == 0) && (month == 2)) {
        monthLength += 1;
    }
    if ((dayOfMonth < 1) || (dayOfMonth > monthLength)) {
        return false;
    }

    // The day of the week can be derived directly from the number of
    // days since 1st of January 2000, which was a Saturday (day 6).
    rtcTime->dayOfWeek = 1 + (getElapsedDays (rtcTime) + 5) % 7;
    return true;
}

/*
 * Initialises a real time clock for subsequent use. This should be
 * called for each RTC instance prior to accessing it via any of the
 * other API functions.
 */
bool gmosDriverRtcInit (gmosDriverRtc_t* rtc, bool isMainInstance)
{
    // First initialise the platform abstraction layer.
    if (!gmosPalRtcInit (rtc)) {
        return false;
    }

    // Set the RTC as the main instance for storing current system time.
    if (isMainInstance) {
        mainInstance = rtc;
    }
    return true;
}

/*
 * Accesses the main real time clock instance to be used for storing
 * the current system time. For most configurations this will be the
 * only real time clock on the device.
 */
gmosDriverRtc_t* gmosDriverRtcGetInstance (void)
{
    return mainInstance;
}

/*
 * Assigns the specified time and date to the real time clock,
 * regardless of the current time and date value. The new time value
 * will be checked for a valid time and date.
 */
bool gmosDriverRtcSetTime (
    gmosDriverRtc_t* rtc, gmosDriverRtcTime_t* newTime)
{
    // Check for a valid time and date.
    if (!gmosDriverRtcValidateRtcTime (newTime)) {
        return false;
    }

    // Assign the new time and date to the platform specific RTC.
    return gmosPalRtcSetTime (rtc, newTime);
}

/*
 * Attempts to synchronize the real time clock to the specified UTC
 * time value. If there is a significant disparity between the current
 * time and date value this will be equivalent to setting the real time
 * clock value. Otherwise the local clock source may be adjusted to
 * compensate for relative clock drift.
 */
bool gmosDriverRtcSyncTime (
    gmosDriverRtc_t* rtc, uint32_t utcTime)
{
    gmosDriverRtcTime_t currentTime;
    gmosDriverRtcTime_t syncTime;
    uint32_t currentUtc;

    // Get the current RTC time settings.
    if ((!gmosDriverRtcGetTime (rtc, &currentTime)) ||
        (!gmosDriverRtcConvertToUtcTime (&currentTime, &currentUtc))) {
        return false;
    }

    // If the current time is outside the tracking window, overwrite the
    // current time. This preserves the existing time zone settings.
    if ((currentUtc > utcTime + GMOS_DRIVER_RTC_TRACKING_WINDOW) ||
        (currentUtc < utcTime - GMOS_DRIVER_RTC_TRACKING_WINDOW)) {
        if (!gmosDriverRtcConvertFromUtcTime (&syncTime, utcTime,
            currentTime.timeZone, currentTime.daylightSaving)) {
            return false;
        } else {
            return gmosPalRtcSetTime (rtc, &syncTime);
        }
    }

    // Implement fine adjustment within the tracking window.
    else {
        // Not currently implemented.
        return true;
    }
}
