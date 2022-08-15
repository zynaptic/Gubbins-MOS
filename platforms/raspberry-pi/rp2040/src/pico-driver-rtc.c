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
 * Implements real time clock driver functionality for the Raspberry Pi
 * RP2040 series of microcontrollers.
 */

#include <stdint.h>
#include <stdbool.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-driver-rtc.h"
#include "pico-device.h"
#include "pico-driver-rtc.h"
#include "hardware/rtc.h"
#include "hardware/clocks.h"

// Use RTC software implementation instead of dedicated hardware.
#if !GMOS_CONFIG_RTC_SOFTWARE_EMULATION

/*
 * Specify the gain for the time offset error, expressed as parts per
 * 2^20. The reciprocal of this can be interpreted as the time taken to
 * correct for a one second offset error given no other adjustments.
 * Since the default RTC clock is 46875 Hz, the minimum gain is 23 to
 * ensure that an offset of 1 results in a suitable adjustment to the
 * calibration register.
 */
#define GMOS_DRIVER_RTC_OFFSET_GAIN 24        // 12 hour correction.

/*
 * Specify the gain for the clock drift error, which is implemented as
 * a right shift operation such that the gain is 1/2^N.
 */
#define GMOS_DRIVER_RTC_DRIFT_GAIN_SHIFT 4    // 1/16 gain correction.

/*
 * Specify the limiting factor for the clock calibration corrections.
 * All calibration corrections will be saturated at this level.
 */
#define GMOS_DRIVER_RTC_CORRECTION_LIMIT 128  // No more than 128 ppm.

/*
 * Sets the RTC calibration register to the specified value.
 */
static void gmosPalRtcSetCalibration (int32_t calibration)
{
    int32_t rtcClockFreq;
    int32_t rtcClockCalFreq;
    int32_t rtcBaseCalibration;

    // Get the base frequency of the RTC clock.
    rtcClockFreq = clock_get_hz (clk_rtc);

    // Derive the base calibration by scaling from parts per 2^20 (about
    // the same as parts per million) to RTC source clock ticks.
    rtcBaseCalibration = ((calibration * rtcClockFreq) + (1 << 19)) >> 20;
    rtcClockCalFreq = rtcClockFreq - rtcBaseCalibration;

    // Update the RTC clock scaling register. Changing this value while
    // the RTC is running is not recommended by the datasheet, but it
    // is the only option available for runtime calibration.
    rtc_hw->clkdiv_m1 = rtcClockCalFreq - 1;

    // Report calibration settings for debug purposes.
    GMOS_LOG_FMT (LOG_VERBOSE,
        "RTC Pico : Calibration %d maps to clock divider %d.",
        calibration, rtcClockCalFreq);
}

/*
 * Initialises the real time clock driver platform abstraction layer.
 * This will be called once on startup in order to initialise the
 * platform specific real time clock driver state.
 */
bool gmosPalRtcInit (gmosDriverRtc_t* rtc, int32_t calibration)
{
    gmosPalRtcState_t* palData = rtc->palData;
    uint32_t rtcClockFreq;
    datetime_t picoRtcTime;

    // Perform SDK RTC initialisation.
    rtc_init ();

    // Get the configured RTC clock frequency for calibration purposes.
    rtcClockFreq = clock_get_hz (clk_rtc);
    GMOS_LOG_FMT (LOG_DEBUG,
        "RTC Pico : Initialising with source clock %d Hz.", rtcClockFreq);

    // Set initial calibration.
    gmosPalRtcSetCalibration (calibration);

    // Always reset the initial time and date to the start of Saturday
    // 1/1/2000, even if the previous setting persisted over a soft
    // reset. This is because the time zone and daylight saving settings
    // are not preserved, so any persisted state would be inconsistent.
    picoRtcTime.year = 2000;
    picoRtcTime.month = 1;
    picoRtcTime.day = 1;
    picoRtcTime.dotw = 6;
    picoRtcTime.hour = 0;
    picoRtcTime.min = 0;
    picoRtcTime.sec = 0;
    rtc_set_datetime (&picoRtcTime);

    // Initialise the local state.
    palData->timeZone = 0;
    palData->daylightSaving = 0;
    return true;
}

/*
 * Retrieves the current time and date from the real time clock,
 * populating the current time data structure.
 */
bool gmosDriverRtcGetTime (
    gmosDriverRtc_t* rtc, gmosDriverRtcTime_t* currentTime)
{
    gmosPalRtcState_t* palData = rtc->palData;
    datetime_t picoRtcTime;

    // Read back the date and time in Pico SDK format.
    if (!rtc_get_datetime (&picoRtcTime)) {
        return false;
    }

    // Only years 2000 to 2099 are currently supported by the RTC API.
    if ((picoRtcTime.year < 2000) || (picoRtcTime.year > 2099)) {
        return false;
    }

    // Extract the time fields.
    currentTime->seconds = gmosDriverRtcBcdFromUint8 (picoRtcTime.sec);
    currentTime->minutes = gmosDriverRtcBcdFromUint8 (picoRtcTime.min);
    currentTime->hours = gmosDriverRtcBcdFromUint8 (picoRtcTime.hour);

    // Extract the date fields.
    currentTime->dayOfMonth = gmosDriverRtcBcdFromUint8 (picoRtcTime.day);
    currentTime->month = gmosDriverRtcBcdFromUint8 (picoRtcTime.month);
    currentTime->year = gmosDriverRtcBcdFromUint8 (picoRtcTime.year - 2000);

    // Modify the day of week representation from 0..6 to 1..7.
    // Apart from Sunday, the other days use the same encoding.
    currentTime->dayOfWeek = picoRtcTime.dotw;
    if (currentTime->dayOfWeek == 0) {
        currentTime->dayOfWeek = 7;
    }

    // Populate the time zone and daylight saving settings from local
    // storage.
    currentTime->timeZone = palData->timeZone;
    currentTime->daylightSaving = palData->daylightSaving;
    return true;
}

/*
 * Retrieves the current internal calibration setting for the real time
 * clock.
 */
int32_t gmosDriverRtcGetCalibration (gmosDriverRtc_t* rtc)
{
    int32_t rtcClockFreq;
    int32_t rtcClockCalFreq;
    int32_t rtcBaseCalibration;
    int32_t rtcScaledCalibration;

    // Get the base frequency of the RTC clock.
    rtcClockFreq = clock_get_hz (clk_rtc);

    // Get the adjusted frequency for the RTC clock.
    rtcClockCalFreq = rtc_hw->clkdiv_m1 + 1;

    // Returns the internal calibration setting for the RTC, expressed
    // as source clock tick periods. A positive value indicates that the
    // RTC is running faster than its nominal frequency, and a negative
    // value indiciates that it is running slower.
    rtcBaseCalibration = rtcClockFreq - rtcClockCalFreq;

    // Approximate the base calibration as parts per 2^20 (about the
    // same as parts per million). Use conventional rounding.
    rtcScaledCalibration = (rtcBaseCalibration << 21) / rtcClockFreq;
    rtcScaledCalibration = (rtcScaledCalibration + 1) / 2;

    // Report calibration settings for debug purposes.
    GMOS_LOG_FMT (LOG_VERBOSE,
        "RTC Pico : Clock divider %d maps to calibration %d.",
        rtcClockCalFreq, rtcScaledCalibration);
    return rtcScaledCalibration;
}

/*
 * Assigns the specified time and date to the real time clock,
 * regardless of the current time and date value. The new time value
 * must specify a valid time and date. If necessary, this can be checked
 * by using the time validation function prior to calling this function.
 */
bool gmosPalRtcSetTime (
    gmosDriverRtc_t* rtc, gmosDriverRtcTime_t* newTime)
{
    gmosPalRtcState_t* palData = rtc->palData;
    datetime_t picoRtcTime;

    // Format the time fields.
    picoRtcTime.sec = gmosDriverRtcBcdToUint8 (newTime->seconds);
    picoRtcTime.min = gmosDriverRtcBcdToUint8 (newTime->minutes);
    picoRtcTime.hour = gmosDriverRtcBcdToUint8 (newTime->hours);

    // Format the date fields.
    picoRtcTime.day = gmosDriverRtcBcdToUint8 (newTime->dayOfMonth);
    picoRtcTime.month = gmosDriverRtcBcdToUint8 (newTime->month);
    picoRtcTime.year =
        2000 + (uint16_t) gmosDriverRtcBcdToUint8 (newTime->year);

    // Modify the day of week representation from 1..7 to 0..6.
    // Apart from Sunday, the other days use the same encoding.
    picoRtcTime.dotw = newTime->dayOfWeek;
    if (picoRtcTime.dotw > 6) {
        picoRtcTime.dotw = 0;
    }

    // Set the RTC statue using the SDK API.
    if (!rtc_set_datetime (&picoRtcTime)) {
        return false;
    }

    // Populate the time zone and daylight saving settings in local
    // storage.
    palData->timeZone = newTime->timeZone;
    palData->daylightSaving = newTime->daylightSaving;
    return true;
}

/*
 * Requests a clock source adjustment from the platform specific real
 * time clock, given the current clock offset and drift relative to the
 * reference clock.
 */
bool gmosPalRtcAdjustClock (
    gmosDriverRtc_t* rtc, int8_t clockOffset, int32_t clockDrift)
{
    int32_t calibration;
    int32_t adjustment;

    // Read the current calibration setting from the RTC, which is an
    // offset in units of parts per 2^20.
    calibration = gmosDriverRtcGetCalibration (rtc);

    // Calculate the adjustment required to compensate for clock drift,
    // with rounding.
    adjustment = (-clockDrift) +
        (1 << (GMOS_DRIVER_RTC_DRIFT_GAIN_SHIFT - 1));
    adjustment >>= GMOS_DRIVER_RTC_DRIFT_GAIN_SHIFT;

    // Calculate the scaled adjustment derived from the clock offset.
    adjustment += (-clockOffset) * GMOS_DRIVER_RTC_OFFSET_GAIN;
    if (adjustment > GMOS_DRIVER_RTC_CORRECTION_LIMIT) {
        adjustment = GMOS_DRIVER_RTC_CORRECTION_LIMIT;
    } else if (adjustment < -GMOS_DRIVER_RTC_CORRECTION_LIMIT) {
        adjustment = -GMOS_DRIVER_RTC_CORRECTION_LIMIT;
    }
    GMOS_LOG_FMT (LOG_VERBOSE,
        "RTC Pico : Applying calibration adjustment %d.", adjustment);

    // Adjust the calibration value to modify the clock frequency.
    calibration += adjustment;
    gmosPalRtcSetCalibration (calibration);
    return true;
}

/*
 * Sets the current time zone for the real time clock, using platform
 * specific hardware support when available.
 */
bool gmosDriverRtcSetTimeZone (
    gmosDriverRtc_t* rtc, int8_t timeZone)
{
    gmosPalRtcState_t* palData = rtc->palData;

    // Check for valid time zone range.
    if ((timeZone < -48) || (timeZone > 56)) {
        return false;
    }

    // Store the current time zone as local data.
    palData->timeZone = timeZone;
    return true;
}

/*
 * Sets the daylight saving time for the real time clock, using platform
 * specific hardware support when available.
 */
bool gmosDriverRtcSetDaylightSaving (
    gmosDriverRtc_t* rtc, bool daylightSaving)
{
    gmosPalRtcState_t* palData = rtc->palData;
    datetime_t picoRtcTime;

    // Make no change if the settings are consistent.
    if (daylightSaving && (palData->daylightSaving != 0)) {
        return true;
    }
    else if (!daylightSaving && (palData->daylightSaving == 0)) {
        return true;
    }

    // Read the current Pico SDK time.
    if (!rtc_get_datetime (&picoRtcTime)) {
        return false;
    }

    // Perform safety check for 'fall back'. This only works if the
    // current hours setting can be safely decremented without having a
    // knock-on effect on the days counter.
    if (!daylightSaving) {
        if (picoRtcTime.hour <= 0) {
            return false;
        } else {
            picoRtcTime.hour -= 1;
        }
    }

    // Perform safety check for 'spring forwards'. This only works if
    // the current hours setting can be safely incremented without
    // having a knock-on effect on the days counter.
    if (daylightSaving) {
        if (picoRtcTime.hour >= 23) {
            return false;
        } else {
            picoRtcTime.hour += 1;
        }
    }

    // Write the modified time back to the RTC using the Pico SDK.
    if (!rtc_set_datetime (&picoRtcTime)) {
        return false;
    }

    // Update the local daylight saving state.
    palData->daylightSaving = daylightSaving ? 1 : 0;
    return true;
}

#endif // GMOS_CONFIG_RTC_SOFTWARE_EMULATION
