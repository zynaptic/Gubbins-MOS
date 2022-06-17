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
 * Implements real time clock driver functionality for the STM32L0XX
 * series of microcontrollers.
 */

#include <stdint.h>
#include <stdbool.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-driver-rtc.h"
#include "stm32-device.h"
#include "stm32-driver-rtc.h"

// Use RTC software implementation instead of dedicated hardware.
#if !GMOS_CONFIG_RTC_SOFTWARE_EMULATION

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
 * Specify the backup register to be used for time zone storage.
 */
#define GMOS_DRIVER_RTC_TIME_ZONE_REG (RTC->BKP4R)

/*
 * Sets the RTC calibration register to the specified value.
 */
static int32_t gmosPalRtcSetCalibration (int32_t calibration)
{
    uint32_t regValue;
    uint32_t calmValue;

    // Restrict the calibration setting to the valid range.
    if (calibration > 512) {
        calibration = 512;
    } else if (calibration < -511) {
        calibration = -511;
    }

    // Disable RTC write protection.
    RTC->WPR = 0xCA;
    RTC->WPR = 0x53;

    // Check for recalibration register ready.
    while ((RTC->ISR & RTC_ISR_RECALPF) != 0) {};

    // Write the new calibration values to the RTC.
    regValue = RTC->CALR;
    if (calibration > 0) {
        calmValue = calibration - 512;
        regValue |= RTC_CALR_CALP;
    } else {
        calmValue = calibration;
        regValue &= ~RTC_CALR_CALP;
    }
    regValue &= ~RTC_CALR_CALM_Msk;
    regValue |= ((-calmValue) << RTC_CALR_CALM_Pos) & RTC_CALR_CALM_Msk;
    RTC->CALR = regValue;

    // Enable RTC write protection.
    RTC->WPR = 0xFF;
    return calibration;
}

/*
 * Initialises a real time clock for subsequent use. The RTC clock is
 * set up as part of the device clock initialisation process, and the
 * default configuration is correct for use with the 32.7768 kHz
 * external clock. The time zone defaults to UTC+0 on reset.
 */
bool gmosPalRtcInit (gmosDriverRtc_t* rtc, int32_t calibration)
{
    // Ensure the power control DBP bit is set to enable RTC clock
    // domain register access.
    PWR->CR |= PWR_CR_DBP;
    while ((PWR->CR & PWR_CR_DBP) == 0) {};

    // Assign the initial RTC calibration setting.
    gmosPalRtcSetCalibration (calibration);
    return true;
}

/*
 * Retrieves the current time and date from the real time clock,
 * populating the current time data structure.
 */
bool gmosDriverRtcGetTime (
    gmosDriverRtc_t* rtc, gmosDriverRtcTime_t* currentTime)
{
    uint32_t timeValue;
    uint32_t dateValue;
    uint32_t timeCheckValue;
    uint32_t dateCheckValue;

    // Ensure that the RTC shadow registers have been synchronised after
    // a clock adjustment.
    while ((RTC->ISR & RTC_ISR_RSF) == 0) {};

    // To avoid race conditions between the time and date registers,
    // the time is read first, then the date, followed by a second read
    // of the time and date registers. If there is no change in the two
    // sets of register values, the register values are consistent.
    do {
        timeValue = RTC->TR;
        dateValue = RTC->DR;
        timeCheckValue = RTC->TR;
        dateCheckValue = RTC->DR;
    } while ((timeValue != timeCheckValue) ||
        (dateValue != dateCheckValue));

    // Clear the shadow register synchronisation flag after reads.
    RTC->ISR &= ~RTC_ISR_RSF;

    // Extract the time register fields.
    currentTime->seconds = timeValue & 0x7F;
    currentTime->minutes = (timeValue >> 8) & 0x7F;
    currentTime->hours = (timeValue >> 16) & 0x3F;

    // Extract the date register fields.
    currentTime->dayOfWeek = (dateValue >> 13) & 0x07;
    currentTime->dayOfMonth = dateValue & 0x3F;
    currentTime->month = (dateValue >> 8) & 0x1F;

    // Only years 2000 to 2099 are supported by the RTC.
    currentTime->year = (dateValue >> 16) & 0xFF;

    // Set the daylight saving flag if required.
    if ((RTC->CR & RTC_CR_BKP) != 0) {
        currentTime->daylightSaving = 1;
    } else {
        currentTime->daylightSaving = 0;
    }

    // The current time zone information is stored in a backup register.
    currentTime->timeZone = GMOS_DRIVER_RTC_TIME_ZONE_REG;
    return true;
}

/*
 * Retrieves the current internal calibration setting for the real time
 * clock.
 */
int32_t gmosDriverRtcGetCalibration (gmosDriverRtc_t* rtc)
{
    uint32_t regValue;
    int32_t calibration;

    // Read the current calibration setting from the RTC, which is an
    // offset in units of parts per 2^20.
    regValue = RTC->CALR;
    calibration = -((regValue & RTC_CALR_CALM_Msk) >> RTC_CALR_CALM_Pos);
    if ((regValue & RTC_CALR_CALP) != 0) {
        calibration += 512;
    }
    return calibration;
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
    uint32_t timeValue;
    uint32_t dateValue;

    // Disable RTC write protection.
    RTC->WPR = 0xCA;
    RTC->WPR = 0x53;

    // Put the RTC into initialisation mode.
    RTC->ISR |= RTC_ISR_INIT;
    while ((RTC->ISR & RTC_ISR_INITF) == 0) {};

    // Set the time register.
    timeValue = newTime->seconds;
    timeValue |= (newTime->minutes) << 8;
    timeValue |= (newTime->hours) << 16;
    RTC->TR = timeValue;

    // Set the date register.
    dateValue = newTime->dayOfMonth;
    dateValue |= (newTime->month) << 8;
    dateValue |= (newTime->dayOfWeek) << 13;
    dateValue |= (newTime->year) << 16;
    RTC->DR = dateValue;

    // Set the daylight saving bit if required.
    if (newTime->daylightSaving != 0) {
        RTC->CR |= RTC_CR_BKP;
    } else {
        RTC->CR &= ~RTC_CR_BKP;
    }

    // Store the current time zone in a backup register.
    GMOS_DRIVER_RTC_TIME_ZONE_REG = newTime->timeZone;

    // Clear the initialisation flag, allowing the RTC to run.
    RTC->ISR &= ~RTC_ISR_INIT;
    while ((RTC->ISR & RTC_ISR_INITF) != 0) {};

    // Enable RTC write protection.
    RTC->WPR = 0xFF;
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

    // Adjust the calibration value to modify the clock frequency.
    calibration += adjustment;
    calibration = gmosPalRtcSetCalibration (calibration);

    // Log RTC updates if required.
    GMOS_LOG_FMT (LOG_VERBOSE,
        "STM32 RTC adjustment %d -> calibration %d.",
        adjustment, calibration);
    return true;
}

/*
 * Sets the current time zone for the real time clock, using platform
 * specific hardware support when available.
 */
bool gmosDriverRtcSetTimeZone (
    gmosDriverRtc_t* rtc, int8_t timeZone)
{
    // Check for valid time zone range.
    if ((timeZone < -48) || (timeZone > 56)) {
        return false;
    }

    // Store the current time zone in a backup register.
    GMOS_DRIVER_RTC_TIME_ZONE_REG = timeZone;
    return true;
}

/*
 * Sets the daylight saving time for the real time clock, using platform
 * specific hardware support when available.
 */
bool gmosDriverRtcSetDaylightSaving (
    gmosDriverRtc_t* rtc, bool daylightSaving)
{
    uint32_t regValue;
    uint32_t hoursMinsValue;

    // Make no change if the settings are consistent.
    regValue = RTC->CR;
    if (daylightSaving && ((regValue & RTC_CR_BKP) != 0)) {
        return true;
    }
    else if (!daylightSaving && ((regValue & RTC_CR_BKP) == 0)) {
        return true;
    }

    // Perform safety check for 'fall back'. This only works if the
    // current hours setting can be safely decremented without having a
    // knock-on effect on the days counter. The safe range is 1:05 to
    // 23:55 hours.
    if (!daylightSaving) {
        hoursMinsValue = (RTC->TR >> 8) & 0x3F7F;
        if ((hoursMinsValue > 0x2355) || (hoursMinsValue < 0x0105)) {
            return false;
        }
    }

    // Disable RTC write protection.
    RTC->WPR = 0xCA;
    RTC->WPR = 0x53;

    // Implement 'spring forward'. Since this increments the hours it
    // should always work, regardless of the current hours setting.
    if (daylightSaving) {
        RTC->CR = regValue | RTC_CR_ADD1H | RTC_CR_BKP;
    }

    // Implement 'fall back'. This should always work if the prior
    // safety check was successful.
    else {
        RTC->CR = (regValue | RTC_CR_SUB1H) & ~RTC_CR_BKP;
    }

    // Enable RTC write protection.
    RTC->WPR = 0xFF;
    return true;
}

#endif // GMOS_CONFIG_RTC_SOFTWARE_EMULATION
