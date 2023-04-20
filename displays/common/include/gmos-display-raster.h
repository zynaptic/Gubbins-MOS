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
 * This header defines the portable API for accessing raster based
 * graphical displays. It is intended for use with small LCD and OLED
 * displays that may be updated on a line by line basis, such as the
 * range of Sharp Memory LCD panels or Solomon Systech OLED controllers.
 */

#ifndef GMOS_DISPLAY_RASTER_H
#define GMOS_DISPLAY_RASTER_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Specifies the colour encoding which may be used to specify
 * transparency, such that an update does not modify the existing
 * display. This is indicated by setting the upper eight bits of the
 * 32-bit colour value.
 */
#define GMOS_DISPLAY_COLOUR_TRANSPARENCY_MASK 0xFF000000

/**
 * Defines the common data structure for a raster based LCD or OLED
 * display. This will typically be allocated as the first element in a
 * driver specific data structure, with the corresponding initialisation
 * function being responsible for populating the display configuration
 * fields.
 */
typedef struct gmosDisplayRaster_t {

    // Specify a pointer to the local frame buffer which is used to
    // construct the raster image.
    uint32_t* frameBuffer;

    // Specify a pointer to the raster line dirty flags. These are used
    // to carry out selective line updates.
    uint8_t* dirtyFlags;

    // Specify the width of the raster display in pixels.
    uint16_t frameWidth;

    // Specify the height of the raster display in pixel lines.
    uint16_t frameHeight;

    // Specify the colour depth used by the frame buffer. The number of
    // bits per pixel is 2^colourDepth, so a monochrome display will
    // have a colour depth of 0 giving one bit per pixel.
    uint8_t colourDepth;

    // Store screen update pending flag.
    uint8_t updatePending;

} gmosDisplayRaster_t;

/**
 * Initiates a raster display update cycle. This sets the update pending
 * flag in the common display data structure, after which the display
 * specific driver should copy all dirty raster lines from the local
 * frame buffer to the display memory on the next screen refresh cycle.
 * @param display This is the display for which the screen update is
 *     being requested.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully setting the update pending flag and 'false' if a
 *     screen update has already been requested for the next refresh
 *     cycle.
 */
bool gmosDisplayRasterUpdate (gmosDisplayRaster_t* display);

/**
 * Clears the screen in frame buffer memory, setting all pixels to the
 * specified colour.
 * @param display This is the display for which the screen is to be
 *     cleared.
 * @param colour This is the colour to be used when clearing the screen.
 *     The number of bits that are used to set the colour will depend on
 *     the display colour depth, and they will be taken from the least
 *     significant bit positions of the specified value.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully clearing the screen in frame buffer memory and
 *     'false' if a screen update has already been requested for the
 *     next refresh cycle.
 */
bool gmosDisplayRasterClearScreen (gmosDisplayRaster_t* display,
    uint32_t colour);

/**
 * Sets a specific pixel on the raster display to the specified colour.
 * @param display This is the display on which the pixel is to be set.
 * @param xPos This is the pixel location on the raster display X axis,
 *     measured from the origin at the left side of the display.
 * @param yPos This is the pixel location on the raster display Y axis,
 *     measured downwards from the origin at the top of the display.
 * @param colour This is the colour to be used when setting the pixel.
 *     The number of bits that are used to set the colour will depend on
 *     the display colour depth, and they will be taken from the least
 *     significant bit positions of the specified value.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully setting the pixel in frame buffer memory and 'false'
 *     if a screen update has already been requested for the next
 *     refresh cycle.
 */
bool gmosDisplayRasterSetPixel (gmosDisplayRaster_t* display,
    int32_t xPos, int32_t yPos, uint32_t colour);

/**
 * Plots a straight line between two points on the raster display using
 * the specified colour.
 * @param display This is the display on which the straight line is to
 *     be plotted.
 * @param xPos1 This is the pixel location on the raster display X axis
 *     which marks the start of the line, measured from the origin at
 *     the left side of the display.
 * @param yPos1 This is the pixel location on the raster display Y axis
 *     which marks the start of the line, measured downwards from the
 *     origin at the top of the display.
 * @param xPos2 This is the pixel location on the raster display X axis
 *     which marks the end of the line, measured from the origin at the
 *     left side of the display.
 * @param yPos2 This is the pixel location on the raster display Y axis
 *     which marks the end of the line, measured downwards from the
 *     origin at the top of the display.
 * @param colour This is the colour to be used when setting the pixels
 *     which make up the line. The number of bits that are used to set
 *     the colour will depend on the display colour depth, and they will
 *     be taken from the least significant bit positions of the
 *     specified value.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully plotting the line in frame buffer memory and 'false'
 *     if a screen update has already been requested for the next
 *     refresh cycle.
 */
bool gmosDisplayRasterPlotLine (gmosDisplayRaster_t* display,
    int32_t xPos1, int32_t yPos1, int32_t xPos2, int32_t yPos2,
    uint32_t colour);

/**
 * Plots a filled rectangular box on the raster display using the
 * specified outline and fill colours. The rectangle orientation is
 * always aligned to the vertical and horizontal axes.
 * @param display This is the display on which the rectangular box is to
 *     be plotted.
 * @param boxWidth This specifies the width of the box as an integer
 *     number of pixels.
 * @param boxHeight This specifies the height of the box as an integer
 *     number of pixels.
 * @param xPos This is the pixel location on the raster display X axis
 *     at which the left hand edge of the box will be plotted.
 * @param yPos This is the pixel location on the raster display Y axis
 *     at which the upper edge of the box will be plotted.
 * @param fgColour This is the outline foreground colour that will be
 *     used when plotting the box. This colour will be used for the
 *     single pixel wide box outline.
 * @param bgColour This is the background fill colour that will be used
 *     when plotting the box. This colour will be used when filling the
 *     interior of the box outline.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully plotting the box in frame buffer memory and 'false'
 *     if a screen update has already been requested for the next
 *     refresh cycle.
 */
bool gmosDisplayRasterPlotBox (gmosDisplayRaster_t* display,
    int32_t boxWidth, int32_t boxHeight, int32_t xPos, int32_t yPos,
    uint32_t fgColour, uint32_t bgColour);

/**
 * Plots a bitmap to the raster display using the specified foreground
 * and background colours.
 * @param display This is the display on which the bitmap is to be
 *     plotted.
 * @param mapData This is a pointer to a byte array which contains the
 *     bitmap data. The first bitmap line corresponds to the top of the
 *     bitmap image and the start of each line of the bitmap is always
 *     aligned to a byte boundary.
 * @param mapWidth This specifies the number of pixels on each bitmap
 *     line. If it is not an integer multiple of 8, the end of each
 *     bitmap line must be padded to ensure byte alignment for the
 *     following line in the bitmap.
 * @param mapHeight This is the height of the bitmap, which is the
 *     number of pixel lines in the bitmap data.
 * @param xPos This is the pixel location on the raster display X axis
 *     at which the left hand edge of the bitmap will be plotted.
 * @param yPos This is the pixel location on the raster display Y axis
 *     at which the upper edge of the bitmap will be plotted.
 * @param fgColour This is the foreground colour that will be used when
 *     plotting the bitmap image. This colour will be used when the
 *     corresponding bit of the bitmap is set to '1'.
 * @param bgColour This is the background colour that will be used when
 *     plotting the bitmap image. This colour will be used when the
 *     corresponding bit of the bitmap is set to '0'.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully plotting the bitmap in frame buffer memory and
 *     'false' if a screen update has already been requested for the
 *     next refresh cycle.
 */
bool gmosDisplayRasterPlotBitmap (gmosDisplayRaster_t* display,
    uint8_t* mapData, int32_t mapWidth, int32_t mapHeight,
    int32_t xPos, int32_t yPos, uint32_t fgColour, uint32_t bgColour);

#endif
