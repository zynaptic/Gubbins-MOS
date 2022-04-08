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
 * This header implements the common functionality for accessing
 * integrated real time clock peripherals.
 */

#include <stdint.h>
#include <stdbool.h>

#include "gmos-driver-rtc.h"

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
    bool monthFound;
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
    monthFound = false;
    for (monthCount = 1; monthCount <= 12; monthCount++) {
        switch (monthCount) {
            case 1 :  // January.
            case 3 :  // March.
            case 5 :  // May.
            case 7 :  // July.
            case 8 :  // August.
            case 10 : // October.
            case 12 : // December.
                if (monthDays < 31) {
                    monthFound = true;
                } else {
                    monthDays -= 31;
                }
                break;
            case 4 :  // April.
            case 6 :  // June.
            case 9 :  // September.
            case 11 : // November.
                if (monthDays < 30) {
                    monthFound = true;
                } else {
                    monthDays -= 30;
                }
                break;
            default : // February.
                if (isLeapYear) {
                    if (monthDays < 29) {
                        monthFound = true;
                    } else {
                        monthDays -= 29;
                    }
                } else {
                    if (monthDays < 28) {
                        monthFound = true;
                    } else {
                        monthDays -= 28;
                    }
                }
                break;
        }
        if (monthFound) {
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
    rtcTime->timeZone = 0x7F & ((uint8_t) timeZone);
    if (daylightSaving) {
        rtcTime->timeZone |= 0x80;
    }
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
    uint32_t localDays;
    uint32_t localSeconds;
    uint32_t yearCount;
    uint32_t yearDays;
    uint32_t monthCount;
    uint32_t monthDays;
    int8_t timeZone;
    int32_t timeZoneAdjustment;
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
        switch (monthCount) {
            case 1 :  // January.
            case 3 :  // March.
            case 5 :  // May.
            case 7 :  // July.
            case 8 :  // August.
            case 10 : // October.
            case 12 : // December.
                monthDays += 31;
                break;
            case 4 :  // April.
            case 6 :  // June.
            case 9 :  // September.
            case 11 : // November.
                monthDays += 30;
                break;
            default : // February.
                if (isLeapYear) {
                    monthDays += 29;
                } else {
                    monthDays += 28;
                }
                break;
        }
    }

    // Derive the number of seconds that have elapsed for full days.
    localDays = yearDays + monthDays +
        gmosDriverRtcBcdToUint8 (rtcTime->dayOfMonth) - 1;
    localSeconds = localDays * (24 * 60 * 60);

    // Derive the number of seconds in the day which have elapsed.
    localSeconds += 60 * 60 * gmosDriverRtcBcdToUint8 (rtcTime->hours);
    localSeconds += 60 * gmosDriverRtcBcdToUint8 (rtcTime->minutes);
    localSeconds += gmosDriverRtcBcdToUint8 (rtcTime->seconds);

    // Apply time zone correction.
    timeZone = 0x7F & rtcTime->timeZone;
    timeZone |= (timeZone & 40) << 1;
    timeZoneAdjustment = ((int32_t) timeZone) * (15 * 60);
    if ((rtcTime->timeZone & 0x80) != 0) {
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
