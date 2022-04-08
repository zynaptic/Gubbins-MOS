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

#include "gmos-driver-rtc.h"
#include "stm32-device.h"
#include "stm32-driver-rtc.h"

/*
 * Initialises a real time clock for subsequent use. The RTC clock is
 * set up as part of the device clock initialisation process, and the
 * default configuration is correct for use with the 32.7768 kHz
 * external clock. No further initialisation is required.
 */
bool gmosDriverRtcInit (gmosDriverRtc_t* rtc)
{
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

    // Set the daylight saving bit if required. No other time zone
    // information is stored by the RTC.
    if ((RTC->CR & RTC_CR_BKP) != 0) {
        currentTime->timeZone = 0x80;
    } else {
        currentTime->timeZone = 0x00;
    }
    return true;
}

/*
 * Assigns the specified time and date to the real time clock,
 * regardless of the current time and date value.
 */
bool gmosDriverRtcSetTime (
    gmosDriverRtc_t* rtc, gmosDriverRtcTime_t* newTime)
{
    uint32_t timeValue;
    uint32_t dateValue;

    // TODO: Add time validation check.

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

    // Set the daylight saving bit if required. No other time zone
    // information is stored by the RTC.
    if ((newTime->timeZone & 0x80) != 0) {
        RTC->CR |= RTC_CR_BKP;
    } else {
        RTC->CR &= ~RTC_CR_BKP;
    }

    // Clear the initialisation flag, allowing the RTC to run.
    RTC->ISR &= ~RTC_ISR_INIT;

    // Enable RTC write protection.
    RTC->WPR = 0xFF;
    return true;
}

