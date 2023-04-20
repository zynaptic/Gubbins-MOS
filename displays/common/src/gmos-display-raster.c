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
 * This file implements the common API functions for accessing raster
 * based graphical displays.
 */

#include <stdint.h>
#include <stdbool.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-display-raster.h"

/*
 * Write a single pixel value to the raster display, marking the
 * appropriate raster line as dirty.
 */
static void gmosDisplayRasterPixelWrite (gmosDisplayRaster_t* display,
    int32_t xPos, int32_t yPos, uint32_t colour)
{
    uint32_t pixelOffset;
    uint32_t pixelShift;
    uint32_t pixelMask;
    uint32_t pixelUpdate;
    uint32_t frameIndex;
    uint32_t pixelWord;

    // Off screen pixel writes are discarded.
    if ((xPos < 0) || (xPos >= display->frameWidth) ||
        (yPos < 0) || (yPos >= display->frameHeight)) {
        return;
    }

    // Transparent pixel writes are discarded.
    if ((colour & GMOS_DISPLAY_COLOUR_TRANSPARENCY_MASK) ==
        GMOS_DISPLAY_COLOUR_TRANSPARENCY_MASK) {
        return;
    }

    // Determine the pixel offset in the frame buffer.
    pixelOffset = (uint32_t) (xPos + yPos * display->frameWidth);
    pixelOffset <<= display->colourDepth;
    pixelShift = pixelOffset & 0x1F;
    frameIndex = pixelOffset / 32;

    // Generate the pixel update value and mask.
    pixelMask = (((uint32_t) 1) << (display->colourDepth + 1)) - 1;
    pixelMask = pixelMask << pixelShift;
    pixelUpdate = colour << pixelShift;

    // Apply the pixel update.
    pixelWord = display->frameBuffer [frameIndex];
    pixelWord &= ~pixelMask;
    pixelWord |= pixelUpdate & pixelMask;
    display->frameBuffer [frameIndex] = pixelWord;

    // Mark the pixel line as dirty.
    display->dirtyFlags [yPos / 8] |= ((uint8_t) 1) << (yPos & 0x07);
}

/*
 * Implement pixel based line plotting using Bresenham's integer
 * algorithm.
 */
static inline void gmosDisplayRasterLineWrite (
    gmosDisplayRaster_t* display, int32_t xPos1, int32_t yPos1,
    int32_t xPos2, int32_t yPos2, uint32_t colour)
{
    bool xAxisIsDriver;
    int32_t xDelta;
    int32_t yDelta;
    int32_t dDelta;
    int32_t pDelta;
    int32_t dPos;
    int32_t dPosStart;
    int32_t dPosEnd;
    int32_t pPos;
    int32_t pPosStart;
    int32_t pPosEnd;
    int32_t pPosIncr;
    int32_t err;

    // Select the driver axis as the one with the largest delta.
    xDelta = (xPos2 > xPos1) ? (xPos2 - xPos1) : (xPos1 - xPos2);
    yDelta = (yPos2 > yPos1) ? (yPos2 - yPos1) : (yPos1 - yPos2);
    xAxisIsDriver = (xDelta >= yDelta) ? true : false;

    // Select the initial parameters when the X axis is the driver.
    if (xAxisIsDriver) {
        err = -xDelta;
        dDelta = 2 * xDelta;
        pDelta = 2 * yDelta;
        if (xPos1 <= xPos2) {
            dPosStart = xPos1;
            dPosEnd = xPos2;
            pPosStart = yPos1;
            pPosEnd = yPos2;
        } else {
            dPosStart = xPos2;
            dPosEnd = xPos1;
            pPosStart = yPos2;
            pPosEnd = yPos1;
        }
    }

    // Select the initial parameters when the Y axis is the driver.
    else {
        err = -yDelta;
        dDelta = 2 * yDelta;
        pDelta = 2 * xDelta;
        if (yPos1 <= yPos2) {
            dPosStart = yPos1;
            dPosEnd = yPos2;
            pPosStart = xPos1;
            pPosEnd = xPos2;
        } else {
            dPosStart = yPos2;
            dPosEnd = yPos1;
            pPosStart = xPos2;
            pPosEnd = xPos1;
        }
    }

    // Select the sign to use for passive axis increments.
    pPos = pPosStart;
    pPosIncr = (pPosEnd >= pPosStart) ? 1 : -1;

    // Loop over the required number of pixels on the driving axis.
    for (dPos = dPosStart; dPos <= dPosEnd; dPos++) {
        int32_t xPos;
        int32_t yPos;

        // Write the next pixel.
        if (xAxisIsDriver) {
            xPos = dPos;
            yPos = pPos;
        } else {
            xPos = pPos;
            yPos = dPos;
        }
        gmosDisplayRasterPixelWrite (display, xPos, yPos, colour);

        // Perform the error update for the next pixel.
        if (dPos != dPosEnd) {
            err += pDelta;
            if (err > 0) {
                pPos += pPosIncr;
                err -= dDelta;
            }
        }
    }
}

/*
 * Write a simple filled box to the raster display frame buffer.
 */
static inline void gmosDisplayRasterBoxWrite (
    gmosDisplayRaster_t* display, int32_t boxWidth, int32_t boxHeight,
    int32_t xPos, int32_t yPos, uint32_t fgColour, uint32_t bgColour)
{
    int32_t boxLine;
    int32_t boxPixel;
    uint32_t colour;

    // Iterate over the box lines.
    for (boxLine = 0; boxLine < boxHeight; boxLine ++) {
        for (boxPixel = 0; boxPixel < boxWidth; boxPixel++) {
            if ((boxLine == 0) || (boxLine == boxHeight - 1) ||
                (boxPixel == 0) || (boxPixel == boxWidth - 1)) {
                colour = fgColour;
            } else {
                colour = bgColour;
            }
            gmosDisplayRasterPixelWrite (display,
                xPos + boxPixel, yPos + boxLine, colour);
        }
    }
}

/*
 * Write a bitmap image to the raster display frame buffer.
 */
static inline void gmosDisplayRasterBitmapWrite (
    gmosDisplayRaster_t* display, uint8_t* mapData, int32_t mapWidth,
    int32_t mapHeight, int32_t xPos, int32_t yPos, uint32_t fgColour,
    uint32_t bgColour)
{
    int32_t mapLine;
    int32_t mapPixel;
    uint8_t* mapPtr;
    uint8_t mapByte;
    uint32_t colour;

    // Iterate over the bitmap lines. Note that each line is always
    // aligned to the start of a new map data byte.
    mapPtr = mapData;
    mapByte = 0;
    for (mapLine = 0; mapLine < mapHeight; mapLine++) {
        for (mapPixel = 0; mapPixel < mapWidth; mapPixel++) {
            if ((mapPixel & 0x07) == 0) {
                mapByte = *(mapPtr++);
            } else {
                mapByte >>= 1;
            }
            colour = ((mapByte & 0x01) != 0) ? fgColour : bgColour;
            gmosDisplayRasterPixelWrite (display,
                xPos + mapPixel, yPos + mapLine, colour);
        }
    }
}

/*
 * Set the display update pending flag for the raster display. This will
 * be cleared by the raster display driver once the update is complete.
 */
bool gmosDisplayRasterUpdate (gmosDisplayRaster_t* display)
{
    bool updateOk = false;
    if (display->updatePending == 0) {
        display->updatePending = 1;
        updateOk = true;
    }
    return updateOk;
}

/*
 * Clears the screen in frame buffer memory, setting all pixels to the
 * specified colour.
 */
bool gmosDisplayRasterClearScreen (gmosDisplayRaster_t* display,
    uint32_t colour)
{
    uint32_t pixelWord;
    uint32_t frameBitSize;
    uint32_t i;

    // Check for updates enabled.
    if (display->updatePending != 0) {
        return false;
    }

    // Replicate the selected colour over the pixel word.
    pixelWord = (((uint32_t) 1) << (display->colourDepth + 1)) - 1;
    pixelWord &= colour;
    switch (display->colourDepth) {
        case 0 :
            pixelWord |= pixelWord << 1;
            // Falls through.
        case 1 :
            pixelWord |= pixelWord << 2;
            // Falls through.
        case 2 :
            pixelWord |= pixelWord << 4;
            // Falls through.
        case 3 :
            pixelWord |= pixelWord << 8;
            // Falls through.
        case 4 :
            pixelWord |= pixelWord << 16;
        break;
    }

    // Replicate the pixel word over the frame buffer.
    frameBitSize = (((uint32_t) 1) << (display->colourDepth)) *
        (uint32_t) display->frameWidth * (uint32_t) display->frameHeight;
    for (i = 0; i < frameBitSize / 32; i++) {
        display->frameBuffer [i] = pixelWord;
    }

    // Mark all the frame buffer lines as dirty.
    for (i = 0; i < display->frameHeight / 8; i++) {
        display->dirtyFlags [i] = 0xFF;
    }
    return true;
}

/*
 * Sets a specific pixel on the raster display to the specified colour.
 */
bool gmosDisplayRasterSetPixel (gmosDisplayRaster_t* display,
    int32_t xPos, int32_t yPos, uint32_t colour)
{
    bool updateOk = false;
    if (display->updatePending == 0) {
        gmosDisplayRasterPixelWrite (display, xPos, yPos, colour);
        updateOk = true;
    }
    return updateOk;
}

/*
 * Plots a straight line between two points on the raster display using
 * the specified colour.
 */
bool gmosDisplayRasterPlotLine (gmosDisplayRaster_t* display,
    int32_t xPos1, int32_t yPos1, int32_t xPos2, int32_t yPos2,
    uint32_t colour)
{
    bool updateOk = false;
    if (display->updatePending == 0) {
        gmosDisplayRasterLineWrite (display,
            xPos1, yPos1, xPos2, yPos2, colour);
        updateOk = true;
    }
    return updateOk;
}

/*
 * Plots a filled rectangular box on the raster display using the
 * specified outline and fill colours.
 */
bool gmosDisplayRasterPlotBox (gmosDisplayRaster_t* display,
    int32_t boxWidth, int32_t boxHeight, int32_t xPos, int32_t yPos,
    uint32_t fgColour, uint32_t bgColour)
{
    bool updateOk = false;
    if (display->updatePending == 0) {
        gmosDisplayRasterBoxWrite (display, boxWidth,
            boxHeight, xPos, yPos, fgColour, bgColour);
        updateOk = true;
    }
    return updateOk;
}

/*
 * Plots a bitmap to the raster display using the specified foreground
 * and background colours.
 */
bool gmosDisplayRasterPlotBitmap (gmosDisplayRaster_t* display,
    uint8_t* mapData, int32_t mapWidth, int32_t mapHeight,
    int32_t xPos, int32_t yPos, uint32_t fgColour, uint32_t bgColour)
{
    bool updateOk = false;
    if (display->updatePending == 0) {
        gmosDisplayRasterBitmapWrite (display, mapData, mapWidth,
            mapHeight, xPos, yPos, fgColour, bgColour);
        updateOk = true;
    }
    return updateOk;
}
