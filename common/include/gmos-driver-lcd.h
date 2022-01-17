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
 * This header defines the portable API for accessing segment based LCD
 * controllers. The portable API assumes the use of multiple logical
 * common LCD terminals, each of which is associated with up to 64
 * segments.
 */

#ifndef GMOS_DRIVER_LCD_H
#define GMOS_DRIVER_LCD_H

#include <stdint.h>
#include <stdbool.h>

#include "gmos-scheduler.h"
#include "gmos-events.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * Defines the platform specific LCD driver state data structure. The
 * full type definition must be provided by the associated platform
 * abstraction layer.
 */
typedef struct gmosPalLcdState_t gmosPalLcdState_t;

/**
 * Defines the platform specific LCD driver configuration options. The
 * full type definition must be provided by the associated platform
 * abstraction layer.
 */
typedef struct gmosPalLcdConfig_t gmosPalLcdConfig_t;

/**
 * Defines the platform specific LCD update data structure. The full
 * type definition must be provided by the associated platform
 * abstraction layer.
 */
typedef struct gmosPalLcdUpdate_t gmosPalLcdUpdate_t;

/**
 * Defines the GubbinsMOS LCD driver state data structure that is used
 * for managing the low level hardware for a single LCD driver.
 */
typedef struct gmosDriverLcd_t {

    // This is an opaque pointer to the platform abstraction layer data
    // structure that is used for accessing the LCD hardware. The data
    // structure will be platform specific.
    gmosPalLcdState_t* palData;

    // This is an opaque pointer to the platform abstraction layer
    // configuration data structure that is used for setting up the
    // LCD hardware. The data structure will be platform specific.
    const gmosPalLcdConfig_t* palConfig;

} gmosDriverLcd_t;

/**
 * Provides a platform configuration setup macro to be used when
 * allocating an LCD driver data structure. Assigning this macro to
 * an LCD driver data structure on declaration will configure the
 * LCD driver to use the platform specific configuration.
 * @param _palData_ This is a pointer to the platform abstraction layer
 *     data structure that is to be used for accessing the platform
 *     specific hardware.
 * @param _palConfig_ This is a pointer to the platform specific LCD
 *     configuration data structure that defines a set of fixed
 *     configuration options to be used with the LCD.
 */
#define GMOS_DRIVER_LCD_PAL_CONFIG(_palData_, _palConfig_) \
    { _palData_, _palConfig_ }

/**
 * Initialises an LCD for subsequent use. This should be called for each
 * LCD instance prior to accessing it via any of the other API
 * functions.
 * @param lcd This is the LCD data structure that is to be initialised.
 *     It should previously have been configured using the
 *     'GMOS_DRIVER_LCD_PAL_CONFIG' macro.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully setting up the LCD and 'false' on failure.
 */
bool gmosDriverLcdInit (gmosDriverLcd_t* lcd);

/**
 * Requests the number of LCD common terminals supported by the logical
 * view of the underlying LCD driver.
 * @param lcd This is a pointer to the LCD driver data structure for
 *     which the number of common terminals is being requested.
 * @return Returns the number of logical common terminals supported by
 *     the platform specific LCD driver.
 */
uint8_t gmosDriverLcdNumCommons (gmosDriverLcd_t* lcd);

/**
 * Determines whether the LCD driver is ready to accept update and
 * synchronisation requests.
 * @param lcd This is a pointer to the LCD driver data structure for
 *     which the readiness check is being carried out.
 * @return Returns a boolean value which will be set to 'true' if the
 *     LCD driver is ready to accept update and synchronisation
 *     requests and 'false' if it is not yet ready.
 */
bool gmosDriverLcdReady (gmosDriverLcd_t* lcd);

/**
 * Synchronises any pending LCD update requests with the LCD display.
 * @param lcd This is a pointer to the LCD driver data structure for
 *     which the display is to be synchronised.
 * @return Returns a boolean value that will be set to 'true' if the
 *     LCD synchronisation request was successful and 'false' otherwise.
 */
bool gmosDriverLcdSync (gmosDriverLcd_t* lcd);

/**
 * Submit a portable logical format LCD update request to the LCD
 * driver. This is equivalent to formatting the platform specific update
 * and then submitting it to the LCD driver. The update will not be
 * applied to the display until an LCD synchronisation request is
 * issued.
 * @param lcd This is a pointer to the LCD driver data structure to
 *     which the LCD update is being submitted.
 * @param lcdCommon This is the LCD logical common terminal that is
 *     associated with the update request.
 * @param segmentMask This is a 64-bit LCD segment update mask. Only
 *     those segments for which the corresponding mask bit is set will
 *     be updated.
 * @param segmentData This is the 64-bit LCD segment update data.
 *     All segments selected by the segment mask will be set to the
 *     value of the corresponding bit in the segment update data.
 * @return Returns a boolean value that will be set to 'true' if the
 *     LCD update was successfully submitted and 'false' otherwise.
 */
bool gmosDriverLcdUpdate (gmosDriverLcd_t* lcd,
    uint8_t lcdCommon, uint64_t segmentMask, uint64_t segmentData);

/**
 * Map an LCD update request from the portable logical format to the
 * platform specific update format.
 * @param lcd This is a pointer to the LCD driver data structure for
 *     which the LCD update is being formatted.
 * @param lcdCommon This is the LCD logical common terminal that is
 *     associated with the update request.
 * @param segmentMask This is a 64-bit LCD segment update mask. Only
 *     those segments for which the corresponding mask bit is set will
 *     be updated.
 * @param segmentData This is the 64-bit LCD segment update data.
 *     All segments selected by the segment mask will be set to the
 *     value of the corresponding bit in the segment update data.
 * @param lcdUpdate This is a pointer to a platform specific LCD update
 *     data item which will be populated with the new update request.
 * @return Returns a boolean value that will be set to 'true' if the
 *     LCD update was successfully mapped to the platform specific
 *     format and 'false' otherwise.
 */
bool gmosDriverLcdFormatUpdate (
    gmosDriverLcd_t* lcd, uint8_t lcdCommon, uint64_t segmentMask,
    uint64_t segmentData, gmosPalLcdUpdate_t* lcdUpdate);

/**
 * Submit a platform specific formatted LCD update request to the LCD
 * driver. The update will not be applied to the display until an LCD
 * synchronisation request is issued.
 * @param lcd This is a pointer to the LCD driver data structure to
 *     which the LCD update is being submitted.
 * @param lcdUpdate This is a pointer to the platform specific LCD
 *     update data item that is to be used to update the LCD state.
 * @return Returns a boolean value that will be set to 'true' if the
 *     LCD update was successfully submitted and 'false' otherwise.
 */
bool gmosDriverLcdSubmitUpdate (
    gmosDriverLcd_t* lcd, gmosPalLcdUpdate_t* lcdUpdate);

/**
 * Writes a character to the LCD screen using a 7-segment display map.
 * The supplied display map should be an octet array containing seven
 * LCD common and segment pairs that map to the 7-segment display
 * clockwise from the upper segment, with the final element in the list
 * being the central segment. The following character set is supported:
 *     "0123456789ABCDEF".
 * @param lcd This is a pointer to the LCD driver data structure to
 *     which the LCD update is being submitted.
 * @param writeChar This is the character to be written to the LCD
 *     display.
 * @param segmentMap This is the segment map that specifies the LCD
 *     segments of the 7 segment character to be updated.
 * @return Returns a boolean value that will be set to 'true' if the
 *     LCD update was successfully submitted and 'false' otherwise.
 */
bool gmosDriverLcdWriteCharSeg7 (gmosDriverLcd_t* lcd,
    char writeChar, const uint8_t* segmentMap);

/**
 * Writes a character to the LCD screen using a 14-segment display map.
 * The supplied display map should be an octet array containing fourteen
 * LCD common and segment pairs that map to the 14-segment display
 * clockwise from the upper segment, with the 'outer' segments first,
 * followed by the 'inner' segments. The following character set is
 * supported:
 *     "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ".
 * @param lcd This is a pointer to the LCD driver data structure to
 *     which the LCD update is being submitted.
 * @param writeChar This is the character to be written to the LCD
 *     display.
 * @param segmentMap This is the segment map that specifies the LCD
 *     segments of the 14 segment character to be updated.
 * @return Returns a boolean value that will be set to 'true' if the
 *     LCD update was successfully submitted and 'false' otherwise.
 */
bool gmosDriverLcdWriteCharSeg14 (gmosDriverLcd_t* lcd,
    char writeChar, const uint8_t* segmentMap);

/**
 * Writes a bar graph level to the LCD screen using a bar graph display
 * map. The supplied display map should be an octet array containing
 * the specified number LCD common and segment pairs, representing the
 * bar graph segments in order of ascending value.
 * @param lcd This is a pointer to the LCD driver data structure to
 *     which the LCD update is being submitted.
 * @param value This is the value of the parameter to be displayed,
 *     expressed in the range from 0 to the specified scale value,
 *     inclusive.
 * @param scale This specifies the full scale range for the supplied
 *     bar graph value. The final bar graph element will only be shown
 *     if the measured value is equal to or exceeds the scale value.
 * @param segmentNum This is the number of segments in the bar graph
 *     display.
 * @param segmentMap This is the segment map that specifies the LCD
 *     segments of the bar graph to be updated.
 * @return Returns a boolean value that will be set to 'true' if the
 *     LCD update was successfully submitted and 'false' otherwise.
 */
bool gmosDriverLcdWriteBarGraph (gmosDriverLcd_t* lcd,
    uint16_t value, uint16_t scale, uint8_t segmentNum,
    const uint8_t* segmentMap);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // GMOS_DRIVER_LCD_H
