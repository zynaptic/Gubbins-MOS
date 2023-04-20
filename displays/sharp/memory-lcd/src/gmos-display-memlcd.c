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
 * This file implements the display specific functions for driving the
 * Sharp Memory LCD range of products.
 */

#include <stdbool.h>

#include "gmos-platform.h"
#include "gmos-scheduler.h"
#include "gmos-driver-gpio.h"
#include "gmos-driver-spi.h"
#include "gmos-display-raster.h"
#include "gmos-display-memlcd.h"

/*
 * Define the state space for the display driver state machine.
 */
typedef enum {
    GMOS_DRIVER_MEMLCD_TASK_STATE_INIT,
    GMOS_DRIVER_MEMLCD_TASK_STATE_CLEAR,
    GMOS_DRIVER_MEMLCD_TASK_STATE_IDLE,
    GMOS_DRIVER_MEMLCD_TASK_STATE_COM_INV,
    GMOS_DRIVER_MEMLCD_TASK_STATE_UPDATE_START,
    GMOS_DRIVER_MEMLCD_TASK_STATE_UPDATE_FORMAT,
    GMOS_DRIVER_MEMLCD_TASK_STATE_UPDATE_WRITE,
    GMOS_DRIVER_MEMLCD_TASK_STATE_UPDATE_WAIT,
    GMOS_DRIVER_MEMLCD_TASK_STATE_UPDATE_END,
    GMOS_DRIVER_MEMLCD_TASK_STATE_FAILED,
} gmosDriverMemLcdTaskState_t;

/*
 * Perform bit order reversal on a single byte.
 */
static inline uint8_t gmosDisplayMemLcdReverseBits (uint8_t value)
{
    value = ((value & 0xF0) >> 4) | ((value & 0x0F) << 4);
    value = ((value & 0xCC) >> 2) | ((value & 0x33) << 2);
    value = ((value & 0xAA) >> 1) | ((value & 0x55) << 1);
    return value;
}

/*
 * Clear the LCD screen on startup. This uses an inline SPI transaction
 * to simplify the initialisation state machine.
 */
static inline bool gmosDisplayMemLcdClearScreen
    (gmosDisplayMemLcd_t* display)
{
    gmosDriverSpiBus_t* spiInterface = display->spiInterface;
    gmosDriverSpiDevice_t* spiDevice = &(display->spiDevice);
    gmosDriverSpiStatus_t spiStatus;
    uint8_t writeData [2];

    // Attempt to claim the SPI bus for the clear screen command.
    if (!gmosDriverSpiDeviceSelect (spiInterface, spiDevice)) {
        return false;
    }

    // Attempt to send the clear screen command.
    writeData [0] = 0x20;
    writeData [1] = 0x00;
    spiStatus = gmosDriverSpiIoInlineWrite (spiInterface, writeData, 2);
    gmosDriverSpiDeviceRelease (spiInterface, spiDevice);

    // Indicate the success or failure of the transaction.
    return (spiStatus == GMOS_DRIVER_SPI_STATUS_SUCCESS) ? true : false;
}

/*
 * Start a multi-line LCD write operation. This selects the LCD device
 * and sends the initial command byte.
 */
static inline bool gmosDisplayMemLcdStartWrite
    (gmosDisplayMemLcd_t* display)
{
    gmosDriverSpiBus_t* spiInterface = display->spiInterface;
    gmosDriverSpiDevice_t* spiDevice = &(display->spiDevice);
    gmosDriverSpiStatus_t spiStatus;
    uint8_t writeData [1];

    // Attempt to claim the SPI bus for the start write command.
    if (!gmosDriverSpiDeviceSelect (spiInterface, spiDevice)) {
        return false;
    }

    // Attempt to send the start write command.
    writeData [0] = 0x80;
    spiStatus = gmosDriverSpiIoInlineWrite (spiInterface, writeData, 1);
    if (spiStatus == GMOS_DRIVER_SPI_STATUS_SUCCESS) {
        return true;
    } else {
        gmosDriverSpiDeviceRelease (spiInterface, spiDevice);
        return false;
    }
}

/*
 * Complete a multi-line LCD write operation. This sends the final
 * trailer byte and releases the device.
 */
static inline bool gmosDisplayMemLcdCompleteWrite
    (gmosDisplayMemLcd_t* display)
{
    gmosDriverSpiBus_t* spiInterface = display->spiInterface;
    gmosDriverSpiDevice_t* spiDevice = &(display->spiDevice);
    gmosDriverSpiStatus_t spiStatus;
    uint8_t writeData [1];

    // Attempt to send the trailer byte.
    writeData [0] = 0x00;
    spiStatus = gmosDriverSpiIoInlineWrite (spiInterface, writeData, 1);
    gmosDriverSpiDeviceRelease (spiInterface, spiDevice);

    // Indicate the success or failure of the transaction.
    return (spiStatus == GMOS_DRIVER_SPI_STATUS_SUCCESS) ? true : false;
}

/*
 * Format a single line write into the SPI transmit buffer.
 */
static inline bool gmosDisplayMemLcdFormatWrite
    (gmosDisplayMemLcd_t* display)
{
    int32_t lineIndex;
    uint32_t i, j;
    uint8_t* spiWritePtr = &(display->spiWriteBuffer [0]);
    uint8_t writeData;

    // Find the next dirty line to be written.
    lineIndex = -1;
    for (i = 0; i < GMOS_CONFIG_DISPLAY_MEMLCD_HEIGHT / 8; i++) {
        uint8_t dirtyFlags = display->dirtyFlags [i];
        if (dirtyFlags != 0) {
            uint8_t dirtyMask = 1;
            for (j = 0; j < 8; j++) {
                if ((dirtyFlags & dirtyMask) != 0) {
                    lineIndex = 8 * i + j;
                    display->dirtyFlags [i] &= ~dirtyMask;
                    break;
                }
                dirtyMask <<= 1;
            }
            break;
        }
    }

    // Set the line index or indicate that no more dirty lines are
    // available for writing. Note that the display lines are indexed
    // from the top left corner instead of standard cartesian layout
    // and are indexed from 1.
    if (lineIndex >= 0) {
        writeData = gmosDisplayMemLcdReverseBits (lineIndex + 1);
        *(spiWritePtr++) = writeData;
    } else {
        return false;
    }

    // Copy the line data into the SPI write buffer. This involves
    // reversing the bit order in each byte for MSB transmission.
    lineIndex *= GMOS_CONFIG_DISPLAY_MEMLCD_WIDTH / 32;
    for (i = 0; i < GMOS_CONFIG_DISPLAY_MEMLCD_WIDTH / 32; i++) {
        uint32_t pixelData = display->frameBuffer [lineIndex + i];
        for (j = 0; j < 4; j++) {
            writeData = gmosDisplayMemLcdReverseBits (pixelData);
            *(spiWritePtr++) = writeData;
            pixelData >>= 8;
        }
    }

    // Append the trailer byte to the end of the SPI transmit buffer.
    *(spiWritePtr) = 0x00;
    return true;
}

/*
 * Implement the Sharp Memory LCD update state machine.
 */
static gmosTaskStatus_t gmosDisplayMemLcdTask (void* taskData)
{
    gmosDisplayMemLcd_t* display = (gmosDisplayMemLcd_t*) taskData;
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    gmosDriverMemLcdTaskState_t nextState = display->displayState;
    gmosDriverSpiStatus_t spiStatus;
    uint32_t timerVal = gmosPalGetTimer ();
    int32_t delay;

    // Implement the main state machine.
    switch (display->displayState) {

        // Insert a short delay on startup to ensure the display is
        // powered up.
        case GMOS_DRIVER_MEMLCD_TASK_STATE_INIT :
            nextState = GMOS_DRIVER_MEMLCD_TASK_STATE_CLEAR;
            taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (10));
            break;

        // Clear the screen on startup.
        case GMOS_DRIVER_MEMLCD_TASK_STATE_CLEAR :
            if (gmosDisplayMemLcdClearScreen (display)) {
                GMOS_LOG (LOG_DEBUG, "Memory LCD Clear Screen Complete.");
                nextState = GMOS_DRIVER_MEMLCD_TASK_STATE_IDLE;
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (10));
            }
            break;

        // In the idle state, wait for a timeout to generate the common
        // terminal inversion strobe or initiate a screen update on
        // request.
        case GMOS_DRIVER_MEMLCD_TASK_STATE_IDLE :
            delay = (int32_t) (display->comInvSetTimestamp - timerVal);
            if (delay <= 0) {
                gmosDriverGpioSetPinState (display->commonInvPin, true);
                display->comInvClrTimestamp =
                    timerVal + GMOS_MS_TO_TICKS (5);
                nextState = GMOS_DRIVER_MEMLCD_TASK_STATE_COM_INV;
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (5));
            } else if (display->raster.updatePending != 0) {
                nextState = GMOS_DRIVER_MEMLCD_TASK_STATE_UPDATE_START;
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (delay);
            }
            break;

        // During the common terminal inversion strobe, wait for the
        // short delay before reverting the strobe.
        case GMOS_DRIVER_MEMLCD_TASK_STATE_COM_INV :
            delay = (int32_t) (display->comInvClrTimestamp - timerVal);
            if (delay <= 0) {
                gmosDriverGpioSetPinState (display->commonInvPin, false);
                display->comInvSetTimestamp += GMOS_MS_TO_TICKS (
                    1000 / GMOS_CONFIG_DISPLAY_MEMLCD_COM_INV_FREQ);
                nextState = GMOS_DRIVER_MEMLCD_TASK_STATE_IDLE;
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (delay);
            }
            break;

        // Attempt to start a display update cycle.
        case GMOS_DRIVER_MEMLCD_TASK_STATE_UPDATE_START :
            if (gmosDisplayMemLcdStartWrite (display)) {
                GMOS_LOG (LOG_DEBUG, "Memory LCD Update Started.");
                nextState = GMOS_DRIVER_MEMLCD_TASK_STATE_UPDATE_FORMAT;
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (10));
            }
            break;

        // Format the next line data into the SPI write buffer.
        case GMOS_DRIVER_MEMLCD_TASK_STATE_UPDATE_FORMAT :
            if (gmosDisplayMemLcdFormatWrite (display)) {
                nextState = GMOS_DRIVER_MEMLCD_TASK_STATE_UPDATE_WRITE;
            } else {
                nextState = GMOS_DRIVER_MEMLCD_TASK_STATE_UPDATE_END;
            }
            break;

        // Initiate the SPI write transaction.
        case GMOS_DRIVER_MEMLCD_TASK_STATE_UPDATE_WRITE :
            if (gmosDriverSpiIoWrite (
                display->spiInterface, display->spiWriteBuffer,
                2 + (GMOS_CONFIG_DISPLAY_MEMLCD_WIDTH / 8))) {
                nextState = GMOS_DRIVER_MEMLCD_TASK_STATE_UPDATE_WAIT;
                taskStatus = GMOS_TASK_SUSPEND;
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (10));
            }
            break;

        // Wait for the line write to complete.
        case GMOS_DRIVER_MEMLCD_TASK_STATE_UPDATE_WAIT :
            spiStatus = gmosDriverSpiIoComplete (
                display->spiInterface, NULL);
            if (spiStatus == GMOS_DRIVER_SPI_STATUS_ACTIVE) {
                taskStatus = GMOS_TASK_SUSPEND;
            } else if (spiStatus == GMOS_DRIVER_SPI_STATUS_SUCCESS) {
                nextState = GMOS_DRIVER_MEMLCD_TASK_STATE_UPDATE_FORMAT;
            } else {
                nextState = GMOS_DRIVER_MEMLCD_TASK_STATE_FAILED;
            }
            break;

        // Complete processing after all dirty lines have been written.
        case GMOS_DRIVER_MEMLCD_TASK_STATE_UPDATE_END :
            if (gmosDisplayMemLcdCompleteWrite (display)) {
                GMOS_LOG (LOG_DEBUG, "Memory LCD Update Completed.");
                display->raster.updatePending = 0;
                nextState = GMOS_DRIVER_MEMLCD_TASK_STATE_IDLE;
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (10));
            }
            break;

        // Suspend further processing on failure.
        default :
            taskStatus = GMOS_TASK_SUSPEND;
            break;
    }
    display->displayState = nextState;
    return taskStatus;
}

/*
 * Initialises a Sharp Memory LCD display on startup.
 */
bool gmosDisplayMemLcdInit (gmosDisplayMemLcd_t* display,
    gmosDriverSpiBus_t* spiInterface,
    uint16_t spiChipSelPin,
    uint16_t commonInvPin)
{
    gmosTaskState_t* displayTask = &(display->displayTask);
    gmosDriverSpiDevice_t* spiDevice = &(display->spiDevice);
    uint32_t i;

    // Clear the frame buffer memory. All lines are marked as dirty so
    // that they will be updated on reset.
    for (i = 0; i < GMOS_CONFIG_DISPLAY_MEMLCD_WIDTH *
        GMOS_CONFIG_DISPLAY_MEMLCD_HEIGHT / 32; i++) {
        display->frameBuffer [i] = 0x00;
    }
    for (i = 0; i < GMOS_CONFIG_DISPLAY_MEMLCD_HEIGHT / 8; i++) {
        display->dirtyFlags [i] = 0xFF;
    }

    // Initialise the common display driver fields.
    display->raster.frameBuffer = display->frameBuffer;
    display->raster.dirtyFlags = display->dirtyFlags;
    display->raster.frameWidth = GMOS_CONFIG_DISPLAY_MEMLCD_WIDTH;
    display->raster.frameHeight = GMOS_CONFIG_DISPLAY_MEMLCD_HEIGHT;
    display->raster.colourDepth = 0;

    // The update pending flag is set so that the initial state will
    // always be written on a restart.
    display->raster.updatePending = 1;

    // Initialise the common terminal inversion timestamps.
    display->comInvSetTimestamp = gmosPalGetTimer ();
    display->comInvClrTimestamp = 0;

    // Initialise the common terminal inversion pin.
    display->commonInvPin = commonInvPin;
    if (!gmosDriverGpioPinInit (commonInvPin,
        GMOS_DRIVER_GPIO_OUTPUT_PUSH_PULL,
        GMOS_DRIVER_GPIO_SLEW_MINIMUM,
        GMOS_DRIVER_GPIO_INPUT_PULL_NONE)) {
        return false;
    }
    gmosDriverGpioSetAsOutput (commonInvPin);
    gmosDriverGpioSetPinState (commonInvPin, false);

    // Initialise the SPI interface.
    display->spiInterface = spiInterface;
    if (!gmosDriverSpiDeviceInit (spiDevice, displayTask, spiChipSelPin,
        GMOS_DRIVER_SPI_CHIP_SELECT_OPTION_ACTIVE_HIGH,
        GMOS_CONFIG_DISPLAY_MEMLCD_SCLK_FREQ / 1000,
        GMOS_DRIVER_SPI_CLOCK_MODE_0)) {
        return false;
    }

    // Initialise the memory LCD task state.
    display->displayState = GMOS_DRIVER_MEMLCD_TASK_STATE_INIT;
    displayTask->taskTickFn = gmosDisplayMemLcdTask;
    displayTask->taskData = display;
    displayTask->taskName =
        GMOS_TASK_NAME_WRAPPER ("Memory LCD Driver Task");
    gmosSchedulerTaskStart (displayTask);
    return true;
}
