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
 * This file implements a software emulated real time clock, using the
 * system timer as a clock source.
 */

#include <stdint.h>
#include <stdbool.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-scheduler.h"
#include "gmos-driver-rtc.h"

// Use RTC software implementation instead of dedicated hardware.
#if GMOS_CONFIG_RTC_SOFTWARE_EMULATION

/*
 * Specify the gain for the time offset error, expressed as parts per
 * 2^20. The reciprocal of this can be interpreted as the time taken to
 * correct for a one second offset error given no other adjustments.
 */
#define GMOS_DRIVER_RTC_OFFSET_GAIN 12        // 24 hour correction.

/*
 * Specify the gain for the clock drift error, which is implemented as
 * a right shift operation such that the gain is 1/2^N.
 */
#define GMOS_DRIVER_RTC_DRIFT_GAIN_SHIFT 3    // 1/8 gain correction.

/*
 * Specify the limiting factor for the clock calibration corrections.
 * All calibration corrections will be saturated at this level.
 */
#define GMOS_DRIVER_RTC_CORRECTION_LIMIT 64   // No more than 64 ppm.

/*
 * Derive the default sub-second increment value.
 */
#define GMOS_DRIVER_RTC_SUBSECOND_INCREMENT \
    (0x100000000 / GMOS_CONFIG_SYSTEM_TIMER_FREQUENCY)

/*
 * Store the standard month lengths in BCD format, indexed by BCD
 * month values.
 */
static uint8_t monthLengthsBcd [] = {
       0, // 0x00
    0x31, // January.
    0x28, // February.
    0x31, // March.
    0x30, // April.
    0x31, // May.
    0x30, // June.
    0x31, // July.
    0x31, // August.
    0x30, // September.
       0, // 0x0A,
       0, // 0x0B,
       0, // 0x0C,
       0, // 0x0D,
       0, // 0x0E,
       0, // 0x0F,
    0x31, // October.
    0x30, // November.
    0x31, // December.
};

/*
 * Implement two digit BCD increments.
 */
static uint8_t incrementBcdValue (uint8_t bcd)
{
    bcd += 1;
    if ((bcd & 0x0F) >= 0x0A) {
        bcd &= 0xF0;
        bcd += 0x10;
    }
    return bcd;
}

/*
 * Implement two digit BCD decrements.
 */
static uint8_t decrementBcdValue (uint8_t bcd)
{
    if ((bcd & 0x0F) > 0) {
        bcd -= 1;
    } else {
        bcd -= (0x10 - 0x09);
    }
    return bcd;
}

/*
 * Increment the RTC date fields, starting with the day counters.
 */
static void incrementDate (gmosPalRtcState_t* palData)
{
    uint8_t monthLengthBcd;

    // Determine the length of the current month, taking into account
    // leap years from 2000 to 2099.
    monthLengthBcd = monthLengthsBcd [palData->monthBcd];
    if (((palData->year & 0x03) == 0) && (palData->monthBcd == 0x02)) {
        monthLengthBcd += 1;
    }

    // Count the days of the month.
    palData->dayOfMonthBcd = incrementBcdValue (palData->dayOfMonthBcd);
    if (palData->dayOfMonthBcd > monthLengthBcd) {
        palData->dayOfMonthBcd = 1;
        palData->monthBcd = incrementBcdValue (palData->monthBcd);
    }

    // Count the number of months.
    if (palData->monthBcd > 0x12) {
        palData->monthBcd = 1;
        palData->year += 1;
        palData->yearBcd = incrementBcdValue (palData->yearBcd);
    }

    // Calculate the number of years, wrapping after 2099.
    if (palData->year > 99) {
        palData->year = 0;
        palData->yearBcd = 0;
    }

    // Count the days of the week.
    palData->dayOfWeek += 1;
    if (palData->dayOfWeek > 7) {
        palData->dayOfWeek = 1;
    }
}

/*
 * Increment the RTC time fields in 24 hour format.
 */
static void incrementTime (gmosPalRtcState_t* palData)
{
    // Count the seconds.
    palData->secondsBcd = incrementBcdValue (palData->secondsBcd);
    if (palData->secondsBcd > 0x59) {
        palData->secondsBcd = 0;
        palData->minutesBcd = incrementBcdValue (palData->minutesBcd);
    }

    // Count the minutes.
    if (palData->minutesBcd > 0x59) {
        palData->minutesBcd = 0;
        palData->hoursBcd = incrementBcdValue (palData->hoursBcd);
    }

    // Count the hours.
    if (palData->hoursBcd > 0x23) {
        palData->hoursBcd = 0;
        incrementDate (palData);
    }
}

/*
 * Performs a periodic update of the RTC timer counter.
 */
static void updateTimerCounter (gmosPalRtcState_t* palData)
{
    uint32_t currentTicks;
    uint32_t incrementTicks;
    uint32_t subSecIncrement;
    uint32_t nextSubSecCounter;

    // Increment the sub-second counter.
    currentTicks = gmosPalGetTimer ();
    incrementTicks = currentTicks - palData->subSecTimestamp;
    subSecIncrement = GMOS_DRIVER_RTC_SUBSECOND_INCREMENT
        + palData->subSecCalibration;
    nextSubSecCounter = palData->subSecCounter +
        (incrementTicks * subSecIncrement);

    // Update the counters on rollover of the sub-second counter.
    if (nextSubSecCounter < palData->subSecCounter) {
        incrementTime (palData);
    }

    // Update the sub-second counter state.
    palData->subSecTimestamp = currentTicks;
    palData->subSecCounter = nextSubSecCounter;
}

/*
 * Implement the timer counter update task.
 */
static gmosTaskStatus_t timerUpdateTask (void* taskData)
{
    gmosPalRtcState_t* palData = (gmosPalRtcState_t*) taskData;

    // Update the timer counter at 300ms intervals.
    updateTimerCounter (palData);
    return GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (300));
}

/*
 * Initialises the real time clock driver platform abstraction layer.
 */
bool gmosPalRtcInit (gmosDriverRtc_t* rtc, int32_t calibration)
{
    gmosPalRtcState_t* palData = rtc->palData;
    int64_t scaledCalibration;

    // Reset the RTC to 00:00:00 UTC on Saturday 1/1/2000.
    palData->year = 0;
    palData->yearBcd = 0;
    palData->monthBcd = 1;
    palData->dayOfMonthBcd = 1;
    palData->dayOfWeek = 6;
    palData->hoursBcd = 0;
    palData->minutesBcd = 0;
    palData->secondsBcd = 0;
    palData->timeZone = 0;
    palData->daylightSaving = 0;

    // Scale the calibration value to the subsecond increment,
    // including rounding.
    scaledCalibration = ((int64_t) calibration) *
        ((int64_t) GMOS_DRIVER_RTC_SUBSECOND_INCREMENT);
    scaledCalibration += 1 << 19;
    scaledCalibration >>= 20;

    // Set the sub-second timer counter state. The initial sub-second
    // increment value is derived from the nominal system timer
    // frequency.
    palData->subSecTimestamp = gmosPalGetTimer ();
    palData->subSecCounter = 0;
    palData->subSecCalibration = (int32_t) scaledCalibration;
    GMOS_LOG_FMT (LOG_VERBOSE,
        "Software RTC initial calibration %d.",
        palData->subSecCalibration);

    // Initialise the timer counter update task.
    palData->timerTask.taskTickFn = timerUpdateTask;
    palData->timerTask.taskData = palData;
    palData->timerTask.taskName = "RTC Software Emulation";
    gmosSchedulerTaskStart (&(palData->timerTask));

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

    // Force a timer counter update if required.
    updateTimerCounter (palData);

    // Copy the timer counters to the current time data structure.
    currentTime->year = palData->yearBcd;
    currentTime->month = palData->monthBcd;
    currentTime->dayOfMonth = palData->dayOfMonthBcd;
    currentTime->dayOfWeek = palData->dayOfWeek;
    currentTime->hours = palData->hoursBcd;
    currentTime->minutes = palData->minutesBcd;
    currentTime->seconds = palData->secondsBcd;
    currentTime->timeZone = palData->timeZone;
    currentTime->daylightSaving = palData->daylightSaving;

    return true;
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

    // Copy the timer counters from the new time data structure.
    palData->year = gmosDriverRtcBcdToUint8 (newTime->year);
    palData->yearBcd = newTime->year;
    palData->monthBcd = newTime->month;
    palData->dayOfMonthBcd = newTime->dayOfMonth;
    palData->dayOfWeek = newTime->dayOfWeek;
    palData->hoursBcd = newTime->hours;
    palData->minutesBcd = newTime->minutes;
    palData->secondsBcd = newTime->seconds;
    palData->timeZone = newTime->timeZone;
    palData->daylightSaving = newTime->daylightSaving;

    // Reset the sub-second timer counter state. Do not change the
    // adjusted sub-second increment value.
    palData->subSecTimestamp = gmosPalGetTimer ();
    palData->subSecCounter = 0;

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
    gmosPalRtcState_t* palData = rtc->palData;
    int32_t adjustment;
    int64_t scaledAdjustment;

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

    // Scale the adjustment by the sub-second increment value.
    scaledAdjustment = ((int64_t) adjustment) *
        ((int64_t) GMOS_DRIVER_RTC_SUBSECOND_INCREMENT);
    scaledAdjustment += 1 << 19;
    scaledAdjustment >>= 20;

    // Adjust the sub-second increment value to modify the clock
    // frequency.
    palData->subSecCalibration += (int32_t) (scaledAdjustment);
    GMOS_LOG_FMT (LOG_VERBOSE,
        "Software RTC adjustment %d -> calibration %d.",
        (int32_t) scaledAdjustment, palData->subSecCalibration);
    return true;
}

/*
 * Sets the current time zone for the real time clock.
 */
bool gmosDriverRtcSetTimeZone (
    gmosDriverRtc_t* rtc, int8_t timeZone)
{
    gmosPalRtcState_t* palData = rtc->palData;

    // Check for valid time zone range.
    if ((timeZone < -48) || (timeZone > 56)) {
        return false;
    }

    // Store the current time zone in RAM.
    palData->timeZone = timeZone;
    return true;
}

/*
 * Sets the daylight saving time for the real time clock.
 */
bool gmosDriverRtcSetDaylightSaving (
    gmosDriverRtc_t* rtc, bool daylightSaving)
{
    gmosPalRtcState_t* palData = rtc->palData;

    // Make no change if the settings are consistent.
    if (daylightSaving && (palData->daylightSaving != 0)) {
        return true;
    }
    else if (!daylightSaving && (palData->daylightSaving == 0)) {
        return true;
    }

    // Implement 'spring forward'. Since this increments the hours it
    // should always work, regardless of the current hours setting.
    if (daylightSaving) {
        palData->hoursBcd = incrementBcdValue (palData->hoursBcd);
        if (palData->hoursBcd > 0x23) {
            palData->hoursBcd = 0;
            incrementDate (palData);
        }
        palData->daylightSaving = true;
        return true;
    }

    // Implement 'fall back'. This only works if the current hours
    // setting can be safely decremented without having a knock-on
    // effect on the days counter. The safe range is 1 to 23 hours.
    if ((palData->hoursBcd > 0x00) && (palData->hoursBcd <= 0x23)) {
        palData->hoursBcd = decrementBcdValue (palData->hoursBcd);
        palData->daylightSaving = false;
        return true;
    } else {
        return false;
    }
}

#endif // GMOS_CONFIG_RTC_SOFTWARE_EMULATION
