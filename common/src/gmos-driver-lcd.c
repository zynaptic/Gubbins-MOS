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
 * This header implements the portable API for accessing segment based
 * LCD controllers.
 */

#include <stdint.h>
#include <stdbool.h>

#include "gmos-platform.h"
#include "gmos-driver-lcd.h"

/*
 * Define the active segments for the supported 7-segment characters.
 */
static uint8_t characterEncodings7Seg [] = {
    0x3F, 0x06, 0x5B, 0x4F,     // 0, 1, 2, 3
    0x66, 0x6D, 0x7D, 0x07,     // 4, 5, 6, 7
    0x7F, 0x6F, 0x77, 0x7C,     // 8, 9, A, b
    0x39, 0x5E, 0x79, 0x71      // C, d, E, F
};

/*
 * Define the active segments for the supported 14-segment characters.
 */
static uint16_t characterEncodings14Seg [] = {
    0x08BF, 0x0006, 0x111B, 0x010F,     // 0, 1, 2, 3
    0x1126, 0x112D, 0x113D, 0x0007,     // 4, 5, 6, 7
    0x113F, 0x112F, 0x1137, 0x054F,     // 8, 9, A, B
    0x0039, 0x044F, 0x1039, 0x1031,     // C, D, E, F
    0x013D, 0x1136, 0x0449, 0x001E,     // G, H, I, J
    0x12B0, 0x0038, 0x20B6, 0x2236,     // K, L, M, N
    0x003F, 0x1133, 0x023F, 0x1333,     // O, P, Q, R
    0x1229, 0x0441, 0x003E, 0x08B0,     // S, T, U, V
    0x0A36, 0x2A80, 0x2480, 0x0889      // W, X, Y, Z
};

/*
 * Writes a character to the LCD screen using a 7-segment display map.
 */
bool gmosDriverLcdWriteCharSeg7 (gmosDriverLcd_t* lcd,
    char writeChar, const uint8_t* segmentMap)
{
    uint8_t numCommons;
    uint8_t common;
    uint8_t i;
    uint8_t encodedChar;
    uint8_t encShiftReg;
    uint64_t segmentMask;
    uint64_t segmentData;
    const uint8_t* segmentMapPtr;

    // Select the character encoding to be used.
    if ((writeChar >= '0') && (writeChar <= '9')) {
        encodedChar = characterEncodings7Seg [writeChar - '0'];
    } else if ((writeChar >= 'A') && (writeChar <= 'F')) {
        encodedChar = characterEncodings7Seg [writeChar - 'A' + 10];
    } else {
        return false;
    }

    // Loop over the LCD common terminals, assembling the corresponding
    // segment fields.
    numCommons = gmosDriverLcdNumCommons (lcd);
    for (common = 0; common < numCommons; common++) {
        encShiftReg = encodedChar;
        segmentMask = 0;
        segmentData = 0;
        segmentMapPtr = segmentMap;

        // Prepare an LCD update for all segments which use the same
        // common terminal.
        for (i = 0; i < 7; i ++) {
            uint8_t nextCommon = *(segmentMapPtr++);
            uint8_t nextSegment = *(segmentMapPtr++);
            if (nextCommon == common) {
                segmentMask |= (1 << nextSegment);
                segmentData |= ((encShiftReg & 1) << nextSegment);
            }
            encShiftReg >>= 1;
        }

        // Attempt to update the LCD controller segment RAM.
        if (segmentMask != 0) {
            if (!gmosDriverLcdUpdate (
                lcd, common, segmentMask, segmentData)) {
                return false;
            }
        }
    }
    return true;
}

/*
 * Writes a character to the LCD screen using a 14-segment display map.
 */
bool gmosDriverLcdWriteCharSeg14 (gmosDriverLcd_t* lcd,
    char writeChar, const uint8_t* segmentMap)
{
    uint8_t numCommons;
    uint8_t common;
    uint8_t i;
    uint16_t encodedChar;
    uint16_t encShiftReg;
    uint64_t segmentMask;
    uint64_t segmentData;
    const uint8_t* segmentMapPtr;

    // Select the character encoding to be used.
    if ((writeChar >= '0') && (writeChar <= '9')) {
        encodedChar = characterEncodings14Seg [writeChar - '0'];
    } else if ((writeChar >= 'A') && (writeChar <= 'Z')) {
        encodedChar = characterEncodings14Seg [writeChar - 'A' + 10];
    } else {
        return false;
    }

    // Loop over the LCD common terminals, assembling the corresponding
    // segment fields.
    numCommons = gmosDriverLcdNumCommons (lcd);
    for (common = 0; common < numCommons; common++) {
        encShiftReg = encodedChar;
        segmentMask = 0;
        segmentData = 0;
        segmentMapPtr = segmentMap;

        // Prepare an LCD update for all segments which use the same
        // common terminal.
        for (i = 0; i < 14; i ++) {
            uint8_t nextCommon = *(segmentMapPtr++);
            uint8_t nextSegment = *(segmentMapPtr++);
            if (nextCommon == common) {
                segmentMask |= (1 << nextSegment);
                segmentData |= ((encShiftReg & 1) << nextSegment);
            }
            encShiftReg >>= 1;
        }

        // Attempt to update the LCD controller segment RAM.
        if (segmentMask != 0) {
            if (!gmosDriverLcdUpdate (
                lcd, common, segmentMask, segmentData)) {
                return false;
            }
        }
    }
    return true;
}

/*
 * Writes a bar graph level to the LCD screen using a bar graph display
 * map.
 */
bool gmosDriverLcdWriteBarGraph (gmosDriverLcd_t* lcd,
    uint16_t value, uint16_t scale, uint8_t segmentNum,
    const uint8_t* segmentMap)
{
    uint32_t scaledValue;
    uint8_t numCommons;
    uint8_t common;
    uint8_t i;
    uint64_t segmentMask;
    uint64_t segmentData;
    const uint8_t* segmentMapPtr;

    // Scale the specified value to the number of bar graph segments.
    scaledValue = ((((uint32_t) value) * segmentNum) / scale);

    // Loop over the LCD common terminals, assembling the corresponding
    // segment fields.
    numCommons = gmosDriverLcdNumCommons (lcd);
    for (common = 0; common < numCommons; common++) {
        segmentMask = 0;
        segmentData = 0;
        segmentMapPtr = segmentMap;
        for (i = 0; i < segmentNum; i++) {
            uint8_t nextCommon = *(segmentMapPtr++);
            uint8_t nextSegment = *(segmentMapPtr++);
            if (nextCommon == common) {
                segmentMask |= (1 << nextSegment);
                if (scaledValue > i) {
                    segmentData |= (1 << nextSegment);
                }
            }
        }

        // Attempt to update the LCD controller segment RAM.
        if (segmentMask != 0) {
            if (!gmosDriverLcdUpdate (
                lcd, common, segmentMask, segmentData)) {
                return false;
            }
        }
    }
    return true;
}
