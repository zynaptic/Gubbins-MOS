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
 * This file implements the common components of the GubbinsMOS firmware
 * management and bootloader support.
 */

#include <stdint.h>
#include <stdbool.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-scheduler.h"
#include "gmos-buffers.h"
#include "gmos-bootloader.h"
#include "gmos-driver-flash.h"

/*
 * This enumeration specifies the various bootloader operating states.
 */
typedef enum {
    GMOS_BOOTLOADER_STATE_IDLE,
    GMOS_BOOTLOADER_STATE_ERASING,
    GMOS_BOOTLOADER_STATE_WRITE_WAIT,
    GMOS_BOOTLOADER_STATE_WRITE_REQ,
    GMOS_BOOTLOADER_STATE_WRITE_POLL,
    GMOS_BOOTLOADER_STATE_CLOSE_REQ,
    GMOS_BOOTLOADER_STATE_CLOSE_POLL,
    GMOS_BOOTLOADER_STATE_VERIFYING,
    GMOS_BOOTLOADER_STATE_VERIFIED_OK,
    GMOS_BOOTLOADER_STATE_VERIFIED_FAIL,
    GMOS_BOOTLOADER_STATE_FAILED,
} gmosBootloaderState_t;

// Specify the bootloader polling interval.
#define GMOS_BOOTLOADER_POLL_INTERVAL \
    (GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (10)))

// Allocate memory for bootloader task handling.
static gmosTaskState_t gmosBootloaderTask;

// Allocate memory for bootloader state machine.
static uint8_t btlTaskState;
static uint32_t btlWriteOffset;
static uint16_t btlWriteSize;

// Allocate static memory for write data array.
#if GMOS_CONFIG_BOOTLOADER_STATIC_WRITE_ARRAY
static uint32_t btlWriteArrayAligned
    [GMOS_CONFIG_BOOTLOADER_MAX_WRITE_SIZE / 4];
#endif

// Allocate memory for the local write data buffer.
static gmosBuffer_t btlWriteBuffer;

/*
 * Write a block of data from the local write data buffer.
 */
static gmosBootloaderStatus_t gmosBootloaderWriteRequest (void)
{
    // Allocate stack memory for write data array.
#if !GMOS_CONFIG_BOOTLOADER_STATIC_WRITE_ARRAY
    uint32_t btlWriteArrayAligned
        [GMOS_CONFIG_BOOTLOADER_MAX_WRITE_SIZE / 4];
#endif
    uint8_t* btlWriteArray = (uint8_t*) btlWriteArrayAligned;

    // Determine the size of the block write request.
    btlWriteSize = gmosBufferGetSize (&btlWriteBuffer);
    if (btlWriteSize > GMOS_CONFIG_BOOTLOADER_MAX_WRITE_SIZE) {
        btlWriteSize = GMOS_CONFIG_BOOTLOADER_MAX_WRITE_SIZE;
    }

    // Force word alignment.
    btlWriteSize /= GMOS_CONFIG_BOOTLOADER_WRITE_WORD_SIZE;
    btlWriteSize *= GMOS_CONFIG_BOOTLOADER_WRITE_WORD_SIZE;

    // Extract the block data from the buffer.
    gmosBufferRead (&btlWriteBuffer, 0, btlWriteArray, btlWriteSize);

    // Initiate the flash memory write request using the platform
    // abstraction layer.
    return gmosBootloaderPalImageWrite (
        btlWriteOffset, btlWriteArray, btlWriteSize);
}

/*
 * Complete a write transaction.
 */
static bool gmosBootloaderWriteComplete (void)
{
    uint_fast16_t bufferSize;
    uint_fast16_t residualSize;
    bool writeComplete;

    // Remove the written data from the current write buffer.
    bufferSize = gmosBufferGetSize (&btlWriteBuffer);
    if (bufferSize > btlWriteSize) {
        gmosBufferRebase (&btlWriteBuffer, bufferSize - btlWriteSize);
    } else {
        gmosBufferReset (&btlWriteBuffer, 0);
    }

    // The write operation is complete if it no longer possible to carry
    // out further word aligned write operations.
    residualSize = gmosBufferGetSize (&btlWriteBuffer);
    if (residualSize < GMOS_CONFIG_BOOTLOADER_WRITE_WORD_SIZE) {
        writeComplete = true;
    } else {
        writeComplete = false;
    }

    // Update the write data offset.
    btlWriteOffset += btlWriteSize;
    return writeComplete;
}

/*
 * Implement the bootloader processing task.
 */
static gmosTaskStatus_t gmosBootloaderTaskFn (void* nullData)
{
    gmosBootloaderState_t nextState = btlTaskState;
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    gmosBootloaderStatus_t btlStatus;
    (void) nullData;

    // Implement the bootloader processing state machine.
    switch (btlTaskState) {

        // From the idle state wait for a new firmware update.
        case GMOS_BOOTLOADER_STATE_IDLE :
            taskStatus = GMOS_TASK_SUSPEND;
            break;

        // Poll the platform abstraction layer for completion of the
        // image erase request.
        case GMOS_BOOTLOADER_STATE_ERASING :
            btlStatus = gmosBootloaderPalStatusPoll ();
            if (btlStatus == GMOS_BOOTLOADER_STATUS_SUCCESS) {
                btlWriteOffset = 0;
                gmosBufferReset (&btlWriteBuffer, 0);
                nextState = GMOS_BOOTLOADER_STATE_WRITE_WAIT;
            } else if (btlStatus == GMOS_BOOTLOADER_STATUS_RETRY) {
                taskStatus = GMOS_BOOTLOADER_POLL_INTERVAL;
            } else {
                nextState = GMOS_BOOTLOADER_STATE_FAILED;
            }
            break;

        // Wait for a firmware write request.
        case GMOS_BOOTLOADER_STATE_WRITE_WAIT :
            taskStatus = GMOS_TASK_SUSPEND;
            break;

        // Initiate a write request if there is write data available.
        case GMOS_BOOTLOADER_STATE_WRITE_REQ :
            btlStatus = gmosBootloaderWriteRequest ();
            if (btlStatus == GMOS_BOOTLOADER_STATUS_SUCCESS) {
                nextState = GMOS_BOOTLOADER_STATE_WRITE_POLL;
            } else if (btlStatus == GMOS_BOOTLOADER_STATUS_RETRY) {
                taskStatus = GMOS_BOOTLOADER_POLL_INTERVAL;
            } else {
                nextState = GMOS_BOOTLOADER_STATE_FAILED;
            }
            break;

        // Poll for write request completion.
        case GMOS_BOOTLOADER_STATE_WRITE_POLL :
            btlStatus = gmosBootloaderPalStatusPoll ();
            if (btlStatus == GMOS_BOOTLOADER_STATUS_SUCCESS) {
                if (gmosBootloaderWriteComplete ()) {
                    nextState = GMOS_BOOTLOADER_STATE_WRITE_WAIT;
                } else {
                    nextState = GMOS_BOOTLOADER_STATE_WRITE_REQ;
                }
            } else if (btlStatus == GMOS_BOOTLOADER_STATUS_RETRY) {
                taskStatus = GMOS_BOOTLOADER_POLL_INTERVAL;
            } else {
                nextState = GMOS_BOOTLOADER_STATE_FAILED;
            }
            break;

        // Initiate a close request.
        case GMOS_BOOTLOADER_STATE_CLOSE_REQ :
            btlStatus = gmosBootloaderWriteRequest ();
            if (btlStatus == GMOS_BOOTLOADER_STATUS_SUCCESS) {
                nextState = GMOS_BOOTLOADER_STATE_CLOSE_POLL;
            } else if (btlStatus == GMOS_BOOTLOADER_STATUS_RETRY) {
                taskStatus = GMOS_BOOTLOADER_POLL_INTERVAL;
            } else {
                nextState = GMOS_BOOTLOADER_STATE_FAILED;
            }
            break;

        // Poll for close request completion.
        case GMOS_BOOTLOADER_STATE_CLOSE_POLL :
            btlStatus = gmosBootloaderPalStatusPoll ();
            if (btlStatus == GMOS_BOOTLOADER_STATUS_SUCCESS) {
                nextState = GMOS_BOOTLOADER_STATE_IDLE;
            } else if (btlStatus == GMOS_BOOTLOADER_STATUS_RETRY) {
                taskStatus = GMOS_BOOTLOADER_POLL_INTERVAL;
            } else {
                nextState = GMOS_BOOTLOADER_STATE_FAILED;
            }
            break;

        // Initiate an upgrade image verification request.
        case GMOS_BOOTLOADER_STATE_VERIFYING :
            btlStatus = gmosBootloaderPalStatusPoll ();
            if (btlStatus == GMOS_BOOTLOADER_STATUS_SUCCESS) {
                nextState = GMOS_BOOTLOADER_STATE_VERIFIED_OK;
            } else if (btlStatus == GMOS_BOOTLOADER_STATUS_RETRY) {
                taskStatus = GMOS_BOOTLOADER_POLL_INTERVAL;
            } else {
                nextState = GMOS_BOOTLOADER_STATE_VERIFIED_FAIL;
            }
            break;

        // Suspend further processing on failure.
        case GMOS_BOOTLOADER_STATE_FAILED :
            taskStatus = GMOS_TASK_SUSPEND;
            break;
    }
    btlTaskState = nextState;
    return taskStatus;
}
GMOS_TASK_DEFINITION (gmosBootloaderTask, gmosBootloaderTaskFn, void);

/*
 * Initialises the bootloader support service on startup.
 */
bool gmosBootloaderInit (void)
{
    bool initOk = true;

    // Initialise the platform abstraction layer.
    initOk = initOk && gmosBootloaderPalInit ();

    // Run the bootloader processing task.
    if (initOk) {
        btlTaskState = GMOS_BOOTLOADER_STATE_IDLE;
        gmosBufferInit (&btlWriteBuffer);
        gmosBootloaderTask_start (&gmosBootloaderTask, NULL,
            GMOS_TASK_NAME_WRAPPER ("Bootloader Support Task"));
    }
    return initOk;
}

/*
 * Opens the stored bootloader application upgrade image file and
 * deletes any existing data ready for a new upgrade image to be stored.
 */
gmosBootloaderStatus_t gmosBootloaderImageOpen (void)
{
    gmosBootloaderStatus_t btlStatus;

    // Check the current state machine state.
    switch (btlTaskState) {

        // The bootloader state machine must be in an idle state prior
        // to opening the upgrade image file.
        case GMOS_BOOTLOADER_STATE_IDLE :
        case GMOS_BOOTLOADER_STATE_VERIFIED_OK :
        case GMOS_BOOTLOADER_STATE_VERIFIED_FAIL :
            btlStatus = gmosBootloaderPalImageErase ();
            if (btlStatus == GMOS_BOOTLOADER_STATUS_SUCCESS) {
                btlTaskState = GMOS_BOOTLOADER_STATE_ERASING;
                gmosSchedulerTaskResume (&gmosBootloaderTask);
            }
            break;

        // Indicate failure.
        case GMOS_BOOTLOADER_STATE_FAILED :
            btlStatus = GMOS_BOOTLOADER_STATUS_FATAL_ERROR;
            break;

        // Indicate not ready.
        default :
            btlStatus = GMOS_BOOTLOADER_STATUS_NOT_READY;
            break;
    }
    return btlStatus;
}

/*
 * Appends the contents of a data buffer to the stored bootloader
 * application upgrade image.
 */
gmosBootloaderStatus_t gmosBootloaderImageAppend (gmosBuffer_t* imageData)
{
    gmosBootloaderStatus_t btlStatus;

    // Check the current state machine state.
    switch (btlTaskState) {

        // The bootloader state machine must be in the write wait state
        // before the write request can be accepted. Note that any
        // residual data is concatenated with the new data buffer
        // contents.
        case GMOS_BOOTLOADER_STATE_WRITE_WAIT :
            if (gmosBufferConcatenate (
                &btlWriteBuffer, imageData, &btlWriteBuffer)) {
                if (gmosBufferGetSize (&btlWriteBuffer) >=
                    GMOS_CONFIG_BOOTLOADER_WRITE_WORD_SIZE) {
                    btlTaskState = GMOS_BOOTLOADER_STATE_WRITE_REQ;
                    gmosSchedulerTaskResume (&gmosBootloaderTask);
                }
                btlStatus = GMOS_BOOTLOADER_STATUS_SUCCESS;
            } else {
                btlStatus = GMOS_BOOTLOADER_STATUS_RETRY;
            }
            break;

        // Indicate operation already in progress.
        case GMOS_BOOTLOADER_STATE_ERASING :
        case GMOS_BOOTLOADER_STATE_WRITE_REQ :
        case GMOS_BOOTLOADER_STATE_WRITE_POLL :
            btlStatus = GMOS_BOOTLOADER_STATUS_RETRY;
            break;

        // Indicate failure.
        case GMOS_BOOTLOADER_STATE_FAILED :
            btlStatus = GMOS_BOOTLOADER_STATUS_FATAL_ERROR;
            break;

        // Indicate not ready.
        default :
            btlStatus = GMOS_BOOTLOADER_STATUS_NOT_READY;
            break;
    }
    return btlStatus;
}

/*
 * Closes the stored bootloader application upgrade image file after all
 * the image data has been written.
 */
gmosBootloaderStatus_t gmosBootloaderImageClose (void)
{
    gmosBootloaderStatus_t btlStatus;
    uint_fast16_t residualSize;
    uint8_t padding [GMOS_CONFIG_BOOTLOADER_WRITE_WORD_SIZE] = { 0 };

    // Check the current state machine state.
    switch (btlTaskState) {

        // The bootloader state machine must be in the write wait state
        // before the close request can be accepted. Note that any
        // residual data is padded to a complete write word.
        case GMOS_BOOTLOADER_STATE_WRITE_WAIT :
            residualSize = gmosBufferGetSize (&btlWriteBuffer);
            if (residualSize == 0) {
                btlTaskState = GMOS_BOOTLOADER_STATE_IDLE;
                btlStatus = GMOS_BOOTLOADER_STATUS_SUCCESS;
            } else if (gmosBufferAppend (&btlWriteBuffer,
                padding, sizeof (padding) - residualSize)) {
                btlTaskState = GMOS_BOOTLOADER_STATE_CLOSE_REQ;
                gmosSchedulerTaskResume (&gmosBootloaderTask);
                btlStatus = GMOS_BOOTLOADER_STATUS_SUCCESS;
            } else {
                btlStatus = GMOS_BOOTLOADER_STATUS_RETRY;
            }
            break;

        // Indicate operation already in progress.
        case GMOS_BOOTLOADER_STATE_ERASING :
        case GMOS_BOOTLOADER_STATE_WRITE_REQ :
        case GMOS_BOOTLOADER_STATE_WRITE_POLL :
            btlStatus = GMOS_BOOTLOADER_STATUS_RETRY;
            break;

        // Indicate failure.
        case GMOS_BOOTLOADER_STATE_FAILED :
            btlStatus = GMOS_BOOTLOADER_STATUS_FATAL_ERROR;
            break;

        // Indicate not ready.
        default :
            btlStatus = GMOS_BOOTLOADER_STATUS_NOT_READY;
            break;
    }
    return btlStatus;
}

/*
 * Verifies the contents of the stored bootloader application upgrade
 * image once the complete upgrade image has been stored.
 */
gmosBootloaderStatus_t gmosBootloaderImageVerify (void)
{
    gmosBootloaderStatus_t btlStatus;

    // Check the current state machine state.
    switch (btlTaskState) {

        // Indicate the result of a prior verification request.
        case GMOS_BOOTLOADER_STATE_VERIFIED_OK :
            btlStatus = GMOS_BOOTLOADER_STATUS_SUCCESS;
            break;
        case GMOS_BOOTLOADER_STATE_VERIFIED_FAIL :
            btlStatus = GMOS_BOOTLOADER_STATUS_VERIFY_FAILED;
            break;

        // The bootloader state machine must be in the idle state
        // before verification can start.
        case GMOS_BOOTLOADER_STATE_IDLE :
            btlStatus = gmosBootloaderPalImageVerify ();
            if (btlStatus == GMOS_BOOTLOADER_STATUS_SUCCESS) {
                btlTaskState = GMOS_BOOTLOADER_STATE_VERIFYING;
                gmosSchedulerTaskResume (&gmosBootloaderTask);
                btlStatus = GMOS_BOOTLOADER_STATUS_RETRY;
            }
            break;

        // Indicate file close or verification operation in progress.
        case GMOS_BOOTLOADER_STATE_CLOSE_REQ :
        case GMOS_BOOTLOADER_STATE_CLOSE_POLL :
        case GMOS_BOOTLOADER_STATE_VERIFYING :
            btlStatus = GMOS_BOOTLOADER_STATUS_RETRY;
            break;

        // Indicate failure.
        case GMOS_BOOTLOADER_STATE_FAILED :
            btlStatus = GMOS_BOOTLOADER_STATUS_FATAL_ERROR;
            break;

        // Indicate not ready.
        default :
            btlStatus = GMOS_BOOTLOADER_STATUS_NOT_READY;
            break;
    }
    return btlStatus;
}

/*
 * Deploys the stored bootloader application upgrade image. This will
 * only proceed once a valid image has been loaded into local storage
 * and verified.
 */
gmosBootloaderStatus_t gmosBootloaderImageDeploy (void)
{
    gmosBootloaderStatus_t btlStatus;

    // Check the current state machine state.
    switch (btlTaskState) {

        // Deploy the verified image.
        case GMOS_BOOTLOADER_STATE_VERIFIED_OK :
            btlStatus = gmosBootloaderPalImageDeploy ();
            break;

        // Indicate failure.
        case GMOS_BOOTLOADER_STATE_VERIFIED_FAIL :
            btlStatus = GMOS_BOOTLOADER_STATUS_VERIFY_FAILED;
            break;
        case GMOS_BOOTLOADER_STATE_FAILED :
            btlStatus = GMOS_BOOTLOADER_STATUS_FATAL_ERROR;
            break;

        // Indicate not ready.
        default :
            btlStatus = GMOS_BOOTLOADER_STATUS_NOT_READY;
            break;
    }
    return btlStatus;
}
