/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2026 Zynaptic Limited
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
 * This header defines the API for GubbinsMOS bootloader support.
 */

#ifndef GMOS_BOOTLOADER_H
#define GMOS_BOOTLOADER_H

#include <stdbool.h>
#include <stdint.h>
#include "gmos-buffers.h"

/**
 * This enumeration specifies the bootloader status values that may be
 * returned by bootloader access functions.
 */
typedef enum {
    GMOS_BOOTLOADER_STATUS_SUCCESS,
    GMOS_BOOTLOADER_STATUS_OVERSIZED_IMAGE,
    GMOS_BOOTLOADER_STATUS_VERIFY_FAILED,
    GMOS_BOOTLOADER_STATUS_FATAL_ERROR,
    GMOS_BOOTLOADER_STATUS_NOT_READY,
    GMOS_BOOTLOADER_STATUS_RETRY
} gmosBootloaderStatus_t;

/**
 * Initialises the bootloader support service on startup.
 * @return Returns a boolean value which will be set to 'true' on
 *     successful initialisation and 'false' otherwise.
 */
bool gmosBootloaderInit (void);

/**
 * Opens the stored bootloader application upgrade image file and
 * deletes any existing data ready for a new upgrade image to be stored.
 * @return Returns a bootloader status value which indicates success or
 *     the reason for failure.
 */
gmosBootloaderStatus_t gmosBootloaderImageOpen (void);

/**
 * Appends the contents of a data buffer to the stored bootloader
 * application upgrade image file.
 * @param imageData This is a data buffer which contains the data which
 *     is to be appended to the stored bootloader application upgrade
 *     image file.
 * @return Returns a bootloader status value which indicates success or
 *     the reason for failure.
 */
gmosBootloaderStatus_t gmosBootloaderImageAppend (gmosBuffer_t* imageData);

/**
 * Closes the stored bootloader application upgrade image file after all
 * the image data has been written.
 * @return Returns a bootloader status value which indicates success or
 *     the reason for failure.
 */
gmosBootloaderStatus_t gmosBootloaderImageClose (void);

/**
 * Verifies the contents of the stored bootloader application upgrade
 * image once the complete upgrade image has been stored.
 * @return Returns a bootloader status value which indicates success or
 *     the reason for failure.
 */
gmosBootloaderStatus_t gmosBootloaderImageVerify (void);

/**
 * Deploys the contents of the stored bootloader application upgrade
 * image once the complete upgrade image has been verified.
 * @return Returns a bootloader status value which indicates success or
 *     the reason for failure.
 */
gmosBootloaderStatus_t gmosBootloaderImageDeploy (void);

/**
 * Initialises the bootloader platform abstraction layer on startup.
 * This function is called automatically by the 'gmosBootloaderInit'
 * function.
 * @return Returns a boolean value which will be set to 'true' on
 *     successful initialisation and 'false' otherwise.
 */
bool gmosBootloaderPalInit (void);

/**
 * Polls the status of the bootloader platform abstraction layer during
 * long running operations.
 * @return Returns a retry status code to indicate that the platform
 *     abstraction layer is currently busy or any other status value to
 *     indicate success or the reason for failure.
 */
gmosBootloaderStatus_t gmosBootloaderPalStatusPoll (void);

/**
 * Initiates an erase request for the contents of the stored bootloader
 * application upgrade image in the platform abstraction layer.
 * @return Returns a bootloader status value which indicates success or
 *     the reason for failure.
 */
gmosBootloaderStatus_t gmosBootloaderPalImageErase (void);

/**
 * Initiates a write request for the stored bootloader application
 * upgrade image in the platform abstraction layer.
 * @param writeOffset This is the offset within the upgrade image at
 *     which the new block of data is to be written.
 * @param writeData This is a pointer to a byte array which contains the
 *     data to be written to the upgrade image.
 * @param writeSize This specifies the number of bytes from the byte
 *     array which are to be written to the upgrade image.
 * @return Returns a bootloader status value which indicates success or
 *     the reason for failure.
 */
gmosBootloaderStatus_t gmosBootloaderPalImageWrite (
    uint32_t writeOffset, uint8_t* writeData, uint16_t writeSize);

/**
 * Initiates a verification request for the stored bootloader
 * application upgrade image in the platform abstraction layer.
 * @return Returns a bootloader status value which indicates success or
 *     the reason for failure.
 */
gmosBootloaderStatus_t gmosBootloaderPalImageVerify (void);

/**
 * Deploys the stored bootloader application upgrade image. This will
 * only proceed once a valid image has been loaded into local storage
 * and verified.
 * @return Returns a bootloader status value which indicates success or
 *     the reason for failure.
 */
gmosBootloaderStatus_t gmosBootloaderPalImageDeploy (void);

#endif // GMOS_BOOTLOADER_H
