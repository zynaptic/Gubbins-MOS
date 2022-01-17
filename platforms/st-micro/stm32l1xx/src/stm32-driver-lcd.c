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
 * Implements LCD driver functionality for the STM32L1XX series of
 * microcontrollers.
 */

#include <stdint.h>
#include <stdbool.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-driver-gpio.h"
#include "gmos-driver-lcd.h"
#include "stm32-device.h"
#include "stm32-driver-gpio.h"
#include "stm32-driver-lcd.h"

/*
 * Initialises the LCD controller for subsequent use.
 */
bool gmosDriverLcdInit (gmosDriverLcd_t* lcd)
{
    const gmosPalLcdConfig_t* palConfig = lcd->palConfig;
    const uint16_t* lcdPinPtr;
    uint32_t crBiasDuty;
    uint32_t clockDivisor;
    uint32_t frameRate;
    uint32_t fcrPs = 0;
    uint32_t fcrDiv = 0;

    // Enable the main peripheral clock for the LCD controller for both
    // normal and power saving modes. The refresh clock uses the 32kHz
    // external clock source. This is assumed to be already enabled,
    // since it is used as the source clock for the system timer and
    // real time clock.
    RCC->APB1ENR |= RCC_APB1ENR_LCDEN;
    RCC->APB1LPENR |= RCC_APB1LPENR_LCDLPEN;

    // Set all the LCD GPIO pins to LCD alternate function mode.
    lcdPinPtr = palConfig->lcdPinList;
    while (*lcdPinPtr != 0xFFFF) {
        if (!gmosDriverGpioAltModeInit (*lcdPinPtr,
            GMOS_DRIVER_GPIO_OUTPUT_PUSH_PULL,
            STM32_GPIO_DRIVER_SLEW_FAST,
            GMOS_DRIVER_GPIO_INPUT_PULL_NONE, 11)) {
            return false;
        }
        lcdPinPtr ++;
    }

    // Select the appropriate bias voltage for the selected duty cycle
    // ratio.
    switch (GMOS_CONFIG_STM32_LCD_DUTY_CYCLE_RATIO) {
        case 1 :        // Bias unused in static operation.
            crBiasDuty = (0 << LCD_CR_BIAS_Pos) | (0 << LCD_CR_DUTY_Pos);
            break;
        case 2 :        // Bias set to 1/2 for two phase mux.
            crBiasDuty = (1 << LCD_CR_BIAS_Pos) | (1 << LCD_CR_DUTY_Pos);
            break;
        case 3 :        // Bias set to 1/3 for three phase mux.
            crBiasDuty = (2 << LCD_CR_BIAS_Pos) | (2 << LCD_CR_DUTY_Pos);
            break;
        case 4 :        // Bias set to 1/3 for four phase mux.
            crBiasDuty = (2 << LCD_CR_BIAS_Pos) | (3 << LCD_CR_DUTY_Pos);
            break;
        case 8 :        // Bias set to 1/4 for eight phase mux.
            crBiasDuty = (0 << LCD_CR_BIAS_Pos) | (4 << LCD_CR_DUTY_Pos);
            break;
        default :
            return false;
    }

    // Apply the alternative segment pin mapping if required.
    if (GMOS_CONFIG_STM32_LCD_REMAP_DEVICE_PINS) {
        crBiasDuty |= LCD_CR_MUX_SEG;
    }

    // Set up the configuration register. Note that the external LCD
    // power option is not currently supported.
    LCD->CR |= crBiasDuty;

    // Calculate the frame rate settings from the configuration options.
    clockDivisor = 32768 / (GMOS_CONFIG_STM32_LCD_FRAME_RATE *
        GMOS_CONFIG_STM32_LCD_DUTY_CYCLE_RATIO);
    while ((31 * (1 << fcrPs)) < clockDivisor) {
        fcrPs += 1;
    }
    while (((16 + fcrDiv) * (1 << fcrPs)) < clockDivisor) {
        fcrDiv += 1;
    }
    frameRate = 32768 / ((16 + fcrDiv) * (1 << fcrPs) *
        GMOS_CONFIG_STM32_LCD_DUTY_CYCLE_RATIO);
    GMOS_LOG_FMT (LOG_DEBUG,
        "LCD frame rate %dHz, duty 1/%d (FCR:PS = %d, FCR:DIV = %d).",
        frameRate, GMOS_CONFIG_STM32_LCD_DUTY_CYCLE_RATIO, fcrPs, fcrDiv);

    // Set up the frame control register. Note that LCD blink and high
    // drive options are not currently supported.
    LCD->FCR |= (fcrPs << LCD_FCR_PS_Pos) | (fcrDiv << LCD_FCR_DIV_Pos) |
        (GMOS_CONFIG_STM32_LCD_DEFAULT_VOLTAGE_LEVEL << LCD_FCR_CC_Pos) |
        (4 << LCD_FCR_PON_Pos);

    // Wait for the frame control register to be updated in the frame
    // clock domain.
    while ((LCD->SR & LCD_SR_FCRSR) == 0) {};

    // Enable the LCD controller.
    LCD->CR |= LCD_CR_LCDEN;

    return true;
}

/*
 * Requests the number of LCD common terminals supported by the logical
 * view of the underlying LCD driver.
 */
uint8_t gmosDriverLcdNumCommons (gmosDriverLcd_t* lcd)
{
    const gmosPalLcdConfig_t* palConfig = lcd->palConfig;
    return palConfig->numCommonTerminals;
}

/*
 * Check that the LCD controller is ready to accept an update.
 */
bool gmosDriverLcdReady (gmosDriverLcd_t* lcd)
{
    // Check that a prior update request is not currently in progress.
    return ((LCD->SR & LCD_SR_UDR) == 0) ? true : false;
}

/*
 * Synchronise the contents of the updated write buffer to the display.
 */
bool gmosDriverLcdSync (gmosDriverLcd_t* lcd)
{
    // Only issue an update if a prior update is not active.
    if ((LCD->SR & LCD_SR_UDR) == 0) {
        LCD->SR |= LCD_SR_UDR;
        return true;
    } else {
        return false;
    }
}

/*
 * Submit a portable logical format LCD update request to the LCD
 * driver.
 */
bool gmosDriverLcdUpdate (gmosDriverLcd_t* lcd,
    uint8_t lcdCommon, uint64_t segmentMask, uint64_t segmentData)
{
    gmosPalLcdUpdate_t lcdUpdate;

    // Attempt to format the update request.
    if (!gmosDriverLcdFormatUpdate (lcd, lcdCommon,
        segmentMask, segmentData, &lcdUpdate)) {
        return false;
    }

    // Submit the update request.
    return gmosDriverLcdSubmitUpdate (lcd, &lcdUpdate);
}

/*
 * Map an LCD update request from the portable logical format to the
 * platform specific update format (segment remapping version).
 */
#if GMOS_CONFIG_STM32_LCD_REMAP_SEGMENTS
bool gmosDriverLcdFormatUpdate (
    gmosDriverLcd_t* lcd, uint8_t lcdCommon, uint64_t segmentMask,
    uint64_t segmentData, gmosPalLcdUpdate_t* lcdUpdate)
{
    const gmosPalLcdConfig_t* palConfig = lcd->palConfig;
    uint32_t i;
    uint32_t remappedMaskL = 0;
    uint32_t remappedMaskH = 0;
    uint32_t remappedDataL = 0;
    uint32_t remappedDataH = 0;

    // Check for valid update mask and LCD common terminal.
    if ((lcdCommon >= palConfig->numCommonTerminals) ||
        ((segmentMask & ~(palConfig->validSegmentMask)) != 0)) {
        return false;
    }

    // Iterate over all entries in the logical format segment mask,
    // terminating when none remain. There is no quick way of doing
    // this, which is why direct mapping is usually preferred.
    for (i = 0; segmentMask != 0; i++) {
        if ((segmentMask & 1) != 0) {
            uint32_t segmentDataBit = (uint32_t) (segmentData & 1);
            uint8_t mappedSegment = palConfig->segmentMap [i];
            if (mappedSegment < 32) {
                remappedMaskL |= (1 << mappedSegment);
                remappedDataL |= (segmentDataBit << mappedSegment);
            } else {
                mappedSegment -= 32;
                remappedMaskH |= (1 << mappedSegment);
                remappedDataH |= (segmentDataBit << mappedSegment);
            }
        }
        segmentMask >>= 1;
        segmentData >>= 1;
    }

    // Copy over the LCD update parameters.
    lcdUpdate->lcdCommon = lcdCommon;
    lcdUpdate->segmentMaskL = remappedMaskL;
    lcdUpdate->segmentMaskH = remappedMaskH;
    lcdUpdate->segmentDataL = remappedDataL;
    lcdUpdate->segmentDataH = remappedDataH;
    return true;
}

/*
 * Map an LCD update request from the portable logical format to the
 * platform specific update format (direct mapped version).
 */
#else
bool gmosDriverLcdFormatUpdate (
    gmosDriverLcd_t* lcd, uint8_t lcdCommon, uint64_t segmentMask,
    uint64_t segmentData, gmosPalLcdUpdate_t* lcdUpdate)
{
    const gmosPalLcdConfig_t* palConfig = lcd->palConfig;

    // Check for valid update mask and LCD common terminal.
    if ((lcdCommon >= palConfig->numCommonTerminals) ||
        ((segmentMask & ~(palConfig->validSegmentMask)) != 0)) {
        return false;
    }

    // Copy over the LCD update parameters.
    lcdUpdate->lcdCommon = lcdCommon;
    lcdUpdate->segmentMaskL = (uint32_t) segmentMask;
    lcdUpdate->segmentMaskH = (uint32_t) (segmentMask >> 32);
    lcdUpdate->segmentDataL = (uint32_t) segmentData;
    lcdUpdate->segmentDataH = (uint32_t) (segmentData >> 32);
    return true;
}
#endif

/*
 * Submit a platform specific formatted LCD update request to the LCD
 * driver. The update will not be applied to the display until an LCD
 * synchronisation request is issued.
 */
bool gmosDriverLcdSubmitUpdate (
    gmosDriverLcd_t* lcd, gmosPalLcdUpdate_t* lcdUpdate)
{
    uint32_t lcdRamAddr;
    uint32_t lcdData;

    // Fail if the LCD RAM is write protected.
    if ((LCD->SR & LCD_SR_UDR) != 0) {
        return false;
    }

    // Get the base LCD RAM address, given the common terminal.
    lcdRamAddr = 2 * lcdUpdate->lcdCommon;

    // Update the LCD RAM for segments 0 to 31.
    lcdData = LCD->RAM [lcdRamAddr];
    lcdData &= ~(lcdUpdate->segmentMaskL);
    lcdData |= lcdUpdate->segmentMaskL & lcdUpdate->segmentDataL;
    LCD->RAM [lcdRamAddr] = lcdData;

    // Update the LCD RAM for segments 32 to 63.
    lcdRamAddr += 1;
    lcdData = LCD->RAM [lcdRamAddr];
    lcdData &= ~(lcdUpdate->segmentMaskH);
    lcdData |= lcdUpdate->segmentMaskH & lcdUpdate->segmentDataH;
    LCD->RAM [lcdRamAddr] = lcdData;
    return true;
}
