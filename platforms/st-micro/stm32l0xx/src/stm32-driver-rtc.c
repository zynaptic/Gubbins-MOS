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
#include "gmos-driver-rtc.h"
#include "stm32-device.h"
#include "stm32-driver-rtc.h"

// Use RTC software implementation instead of dedicated hardware.
#if !GMOS_CONFIG_RTC_SOFTWARE_EMULATION

/*
 * Initialises a real time clock for subsequent use. The RTC clock is
 * set up as part of the device clock initialisation process, and the
 * default configuration is correct for use with the 32.7768 kHz
 * external clock. The time zone defaults to UTC+0 on reset.
 */
bool gmosPalRtcInit (gmosDriverRtc_t* rtc)
{
    gmosPalRtcState_t* palData = rtc->palData;
    palData->timeZone = 0;
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
    uint32_t timeValue;
    uint32_t dateValue;
    uint32_t checkValue;

    // To avoid race conditions between the time and date registers,
    // the time is read first, then the date, followed by a second read
    // of the time register. If there is no change in the two time
    // register values, the register values are consistent.
    do {
        timeValue = RTC->TR;
        dateValue = RTC->DR;
        checkValue = RTC->TR;
    } while (timeValue != checkValue);

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

    // The current time zone information is stored in RAM.
    currentTime->timeZone = palData->timeZone;
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
    uint32_t timeValue;
    uint32_t dateValue;

    // Ensure the power control DBP bit is set to enable RTC clock
    // domain register access.
    PWR->CR |= PWR_CR_DBP;
    while ((PWR->CR & PWR_CR_DBP) == 0) {};

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

    // Store the current time zone in RAM.
    palData->timeZone = newTime->timeZone;

    // Clear the initialisation flag, allowing the RTC to run.
    RTC->ISR &= ~RTC_ISR_INIT;

    // Enable RTC write protection.
    RTC->WPR = 0xFF;
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

    // Store the current time zone in RAM.
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
    uint32_t regValue;
    uint32_t hoursValue;

    // Make no change if the settings are consistent.
    regValue = RTC->CR;
    if (daylightSaving && ((regValue & RTC_CR_BKP) != 0)) {
        return true;
    }
    else if (!daylightSaving && ((regValue & RTC_CR_BKP) == 0)) {
        return true;
    }

    // Implement 'spring forward'. Since this increments the hours it
    // should always work, regardless of the current hours setting.
    if (daylightSaving) {
        RTC->CR = regValue | RTC_CR_ADD1H | RTC_CR_BKP;
        return true;
    }

    // Implement 'fall back'. This only works if the current hours
    // setting can be safely decremented without having a knock-on
    // effect on the days counter. The safe range is 1 to 22 hours.
    hoursValue = (RTC->TR >> 16) & 0x3F;
    if ((hoursValue > 0x00) && (hoursValue < 0x23)) {
        RTC->CR = (regValue | RTC_CR_SUB1H) & ~RTC_CR_BKP;
        return true;
    } else {
        return false;
    }
}

#endif // GMOS_CONFIG_RTC_SOFTWARE_EMULATION
