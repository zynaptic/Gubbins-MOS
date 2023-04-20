/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2023 Zynaptic Limited
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
 * This header defines the display specific structures and function
 * definitions for the Sharp Memory LCD range of products.
 */

#ifndef GMOS_DISPLAY_MEMLCD_H
#define GMOS_DISPLAY_MEMLCD_H

#include <stdint.h>
#include <stdbool.h>
#include "gmos-scheduler.h"
#include "gmos-driver-spi.h"
#include "gmos-display-raster.h"

// Provide an enumerated list of the supported Memory LCD devices.
// Only the monochrome LCD panels are currently supported.
#define GMOS_DISPLAY_MEMLCD_LS013B7DH03 0

/**
 * Specify the Memory LCD device type to be supported by the driver.
 */
#ifndef GMOS_CONFIG_DISPLAY_MEMLCD
#define GMOS_CONFIG_DISPLAY_MEMLCD GMOS_DISPLAY_MEMLCD_LS013B7DH03
#endif

// Derive the Memory LCD parameter settings for the selected device.
#if (GMOS_CONFIG_DISPLAY_MEMLCD == GMOS_DISPLAY_MEMLCD_LS013B7DH03)
#define GMOS_CONFIG_DISPLAY_MEMLCD_WIDTH        128
#define GMOS_CONFIG_DISPLAY_MEMLCD_HEIGHT       128
#define GMOS_CONFIG_DISPLAY_MEMLCD_SCLK_FREQ    1000000
#define GMOS_CONFIG_DISPLAY_MEMLCD_COM_INV_FREQ 60
#else
#error "Sharp Memory LCD Device Not Supported."
#endif

/**
 * Defines the display specific data structure for a Sharp Memory LCD
 * display. This includes the generic raster display data structure as
 * the first element in the data structure.
 */
typedef struct gmosDisplayMemLcd_t {

    // Allocate memory for the common raster display data structure.
    gmosDisplayRaster_t raster;

    // Allocate memory for the memory LCD processing task.
    gmosTaskState_t displayTask;

    // Allocate memory for the SPI device data structure.
    gmosDriverSpiDevice_t spiDevice;

    // Specify the SPI bus to be used to access the display.
    gmosDriverSpiBus_t* spiInterface;

    // Store timestamp for the next common inversion request.
    uint32_t comInvSetTimestamp;

    // Store timestamp for the common inversion clear.
    uint32_t comInvClrTimestamp;

    // Allocate local frame buffer memory for the selected device.
    uint32_t frameBuffer [GMOS_CONFIG_DISPLAY_MEMLCD_WIDTH *
        GMOS_CONFIG_DISPLAY_MEMLCD_HEIGHT / 32];

    // Store the selected common terminal inversion pin ID.
    uint16_t commonInvPin;

    // Allocate local memory for the dirty line flags.
    uint8_t dirtyFlags [GMOS_CONFIG_DISPLAY_MEMLCD_HEIGHT / 8];

    // Allocate local memory for the SPI write buffer.
    uint8_t spiWriteBuffer [2 + (GMOS_CONFIG_DISPLAY_MEMLCD_WIDTH / 8)];

    // Store current display state.
    uint8_t displayState;

} gmosDisplayMemLcd_t;

/**
 * Initialises a Sharp Memory LCD display on startup. On successful
 * completion the standard raster display API may be used to write to
 * the display.
 * @param display This is the Sharp Memory LCD data structure that is to
 *     be initialised for use.
 * @param spiBus This is the SPI bus data structure which corresponds to
 *     the SPI bus controller that is being used to write to the
 *     display memory.
 * @param spiChipSelPin This is the active high SPI chip select pin that
 *     should be used to select the Sharp Memory LCD device.
 * @param commonInvPin This is the GPIO output pin that should be used
 *     for periodically inverting the LCD common terminal polarity. See
 *     the Sharp Memory LCD documentation for details of this feature.
 * @return Returns a boolean status value which will be set to 'true' on
 *     successful initialisation and 'false' on failure.
 */
bool gmosDisplayMemLcdInit (gmosDisplayMemLcd_t* display,
    gmosDriverSpiBus_t* spiBus, uint16_t spiChipSelPin,
    uint16_t commonInvPin);

#endif // GMOS_DISPLAY_MEMLCD_H
