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
 * This file implements the common API for controlling dynamic NFC tags.
 * The current implementation supports the reading and writing of NDEF
 * messages to one or more non volatile memory data areas on the NFC
 * tag. Mailbox based fast data transfer is not currently supported.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-scheduler.h"
#include "gmos-events.h"
#include "gmos-buffers.h"
#include "gmos-driver-nfctag.h"

/*
 * This enumeration specifies the various NFC tag operating states.
 */
typedef enum {
    GMOS_DRIVER_NFC_TAG_STATE_IDLE,
    GMOS_DRIVER_NFC_TAG_STATE_FAILED,
    GMOS_DRIVER_NFC_TAG_STATE_READ_START,
    GMOS_DRIVER_NFC_TAG_STATE_READ_WAIT,
    GMOS_DRIVER_NFC_TAG_STATE_READ_APPEND,
    GMOS_DRIVER_NFC_TAG_STATE_WRITE_START,
    GMOS_DRIVER_NFC_TAG_STATE_WRITE_QUEUE,
    GMOS_DRIVER_NFC_TAG_STATE_WRITE_WAIT,
    GMOS_DRIVER_NFC_TAG_STATE_COMPLETE
} gmosDriverNfcTagState_t;

/*
 * This set of definitions specify the event bit masks used to indicate
 * transaction completion status.
 */
#define GMOS_DRIVER_NFC_TAG_EVENT_STATUS_MASK     0x000000FF
#define GMOS_DRIVER_NFC_TAG_EVENT_COMPLETION_FLAG 0x80000000

/*
 * Issue a read transaction request to the RAL.
 */
static inline gmosTaskStatus_t gmosDriverNfcTagReadStart (
    gmosDriverNfcTag_t* nfcTag, gmosDriverNfcTagState_t* nextState)
{
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    uint_fast16_t bufferSize;
    uint_fast16_t transferAddr;
    uint_fast16_t transferSize;
    bool readComplete = false;
    gmosDriverNfcTagStatus_t status;

    // Check for completion of the full transfer.
    bufferSize = gmosBufferGetSize (nfcTag->dataBuffer);
    if (bufferSize >= nfcTag->dataSize) {
        readComplete = true;
        status = GMOS_DRIVER_NFC_TAG_STATUS_SUCCESS;
    }

    // The transfer parameters are derived from the current state of the
    // read data buffer.
    else {
        transferAddr = nfcTag->baseAddr + bufferSize;
        transferSize = nfcTag->dataSize - bufferSize;
        if (transferSize > GMOS_CONFIG_NFC_TAG_TRANSFER_BLOCK_SIZE) {
            transferSize = GMOS_CONFIG_NFC_TAG_TRANSFER_BLOCK_SIZE;
        }

        // Initiate a RAL read request with the specified transfer
        // parameters.
        status = gmosDriverNfcTagRalReadStart (
            nfcTag, transferAddr, transferSize);
        if (status == GMOS_DRIVER_NFC_TAG_STATUS_NOT_READY) {
            taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (10));
        } else if (status == GMOS_DRIVER_NFC_TAG_STATUS_SUCCESS) {
            *nextState = GMOS_DRIVER_NFC_TAG_STATE_READ_WAIT;
            taskStatus = GMOS_TASK_SUSPEND;
        } else {
            readComplete = true;
        }
    }

    // Issue an event notification on read completion.
    if (readComplete) {
        if (status != GMOS_DRIVER_NFC_TAG_STATUS_SUCCESS) {
            gmosBufferReset (nfcTag->dataBuffer, 0);
        }
        gmosEventSetBits (&(nfcTag->completionEvent),
            GMOS_DRIVER_NFC_TAG_EVENT_COMPLETION_FLAG |
            (GMOS_DRIVER_NFC_TAG_EVENT_STATUS_MASK & status));
        *nextState = GMOS_DRIVER_NFC_TAG_STATE_COMPLETE;
    }
    return taskStatus;
}

/*
 * Append read data to the read data buffer.
 */
static inline gmosTaskStatus_t gmosDriverNfcTagReadAppend (
    gmosDriverNfcTag_t* nfcTag, gmosDriverNfcTagState_t* nextState)
{
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    uint_fast16_t bufferSize;
    uint_fast16_t transferSize;

    // Determine the expected data transfer size.
    bufferSize = gmosBufferGetSize (nfcTag->dataBuffer);
    transferSize = nfcTag->dataSize - bufferSize;
    if (transferSize > GMOS_CONFIG_NFC_TAG_TRANSFER_BLOCK_SIZE) {
        transferSize = GMOS_CONFIG_NFC_TAG_TRANSFER_BLOCK_SIZE;
    }

    // Attempt to append the read data to the receive buffer.
    if (gmosBufferAppend (nfcTag->dataBuffer,
        nfcTag->transferBlock, transferSize)) {
        *nextState = GMOS_DRIVER_NFC_TAG_STATE_READ_START;
    } else {
        taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (10));
    }
    return taskStatus;
}

/*
 * Handle read transaction callbacks from the RAL.
 */
void gmosDriverNfcTagRalReadComplete (gmosDriverNfcTag_t* nfcTag,
    gmosDriverNfcTagStatus_t status)
{
    // Only process callbacks while in the read wait state.
    if (nfcTag->nfcTagState == GMOS_DRIVER_NFC_TAG_STATE_READ_WAIT) {
        if (status == GMOS_DRIVER_NFC_TAG_STATUS_SUCCESS) {
            nfcTag->nfcTagState = GMOS_DRIVER_NFC_TAG_STATE_READ_APPEND;
        } else {
            gmosBufferReset (nfcTag->dataBuffer, 0);
            gmosEventSetBits (&(nfcTag->completionEvent),
                GMOS_DRIVER_NFC_TAG_EVENT_COMPLETION_FLAG |
                (GMOS_DRIVER_NFC_TAG_EVENT_STATUS_MASK & status));
            nfcTag->nfcTagState = GMOS_DRIVER_NFC_TAG_STATE_COMPLETE;
        }
        gmosSchedulerTaskResume (&(nfcTag->workerTask));
    }
}

/*
 * Initiate a write transaction request to the RAL.
 */
static inline gmosTaskStatus_t gmosDriverNfcTagWriteStart (
    gmosDriverNfcTag_t* nfcTag, gmosDriverNfcTagState_t* nextState)
{
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    uint_fast16_t bufferSize;
    uint_fast16_t transferSize;

    // Check for completion of the full transfer.
    bufferSize = gmosBufferGetSize (nfcTag->dataBuffer);
    if (nfcTag->dataSize >= bufferSize) {
        gmosBufferReset (nfcTag->dataBuffer, 0);
        gmosEventSetBits (&(nfcTag->completionEvent),
            GMOS_DRIVER_NFC_TAG_EVENT_COMPLETION_FLAG |
            GMOS_DRIVER_NFC_TAG_STATUS_SUCCESS);
        *nextState = GMOS_DRIVER_NFC_TAG_STATE_COMPLETE;
    }

    // Copy the write data to the intermediate data array. The transfer
    // parameters are derived from the current state of the data size
    // running total.
    else {
        transferSize = bufferSize - nfcTag->dataSize;
        if (transferSize > GMOS_CONFIG_NFC_TAG_TRANSFER_BLOCK_SIZE) {
            transferSize = GMOS_CONFIG_NFC_TAG_TRANSFER_BLOCK_SIZE;
        }
        gmosBufferRead (nfcTag->dataBuffer, nfcTag->dataSize,
            nfcTag->transferBlock, transferSize);
        *nextState = GMOS_DRIVER_NFC_TAG_STATE_WRITE_QUEUE;
    }
    return taskStatus;
}

/*
 * Queue a write request for the RAL.
 */
static inline gmosTaskStatus_t gmosDriverNfcTagWriteQueue (
    gmosDriverNfcTag_t* nfcTag, gmosDriverNfcTagState_t* nextState)
{
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    uint_fast16_t bufferSize;
    uint_fast16_t transferAddr;
    uint_fast16_t transferSize;
    gmosDriverNfcTagStatus_t status;

    // The transfer parameters are derived from the current state of the
    // data size running total.
    bufferSize = gmosBufferGetSize (nfcTag->dataBuffer);
    transferAddr = nfcTag->baseAddr + nfcTag->dataSize;
    transferSize = bufferSize - nfcTag->dataSize;
    if (transferSize > GMOS_CONFIG_NFC_TAG_TRANSFER_BLOCK_SIZE) {
        transferSize = GMOS_CONFIG_NFC_TAG_TRANSFER_BLOCK_SIZE;
    }

    // Initiate a RAL read request with the specified transfer
    // parameters.
    status = gmosDriverNfcTagRalWriteStart (
        nfcTag, transferAddr, transferSize);
    if (status == GMOS_DRIVER_NFC_TAG_STATUS_NOT_READY) {
        taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (10));
    } else if (status == GMOS_DRIVER_NFC_TAG_STATUS_SUCCESS) {
        nfcTag->dataSize += transferSize;
        *nextState = GMOS_DRIVER_NFC_TAG_STATE_WRITE_WAIT;
        taskStatus = GMOS_TASK_SUSPEND;
    }

    // Indicate completion on error.
    else {
        gmosBufferReset (nfcTag->dataBuffer, 0);
        gmosEventSetBits (&(nfcTag->completionEvent),
            GMOS_DRIVER_NFC_TAG_EVENT_COMPLETION_FLAG |
            (GMOS_DRIVER_NFC_TAG_EVENT_STATUS_MASK & status));
        *nextState = GMOS_DRIVER_NFC_TAG_STATE_COMPLETE;
    }
    return taskStatus;
}

/*
 * Handle write transaction callbacks from the RAL.
 */
void gmosDriverNfcTagRalWriteComplete (gmosDriverNfcTag_t* nfcTag,
    gmosDriverNfcTagStatus_t status)
{
    // Only process callbacks while in the write wait state.
    if (nfcTag->nfcTagState == GMOS_DRIVER_NFC_TAG_STATE_WRITE_WAIT) {
        if (status == GMOS_DRIVER_NFC_TAG_STATUS_SUCCESS) {
            nfcTag->nfcTagState = GMOS_DRIVER_NFC_TAG_STATE_WRITE_START;
        } else {
            gmosBufferReset (nfcTag->dataBuffer, 0);
            gmosEventSetBits (&(nfcTag->completionEvent),
                GMOS_DRIVER_NFC_TAG_EVENT_COMPLETION_FLAG |
                (GMOS_DRIVER_NFC_TAG_EVENT_STATUS_MASK & status));
            nfcTag->nfcTagState = GMOS_DRIVER_NFC_TAG_STATE_COMPLETE;
        }
        gmosSchedulerTaskResume (&(nfcTag->workerTask));
    }
}

/*
 * Implement the NFC tag driver task function that provides the NFC tag
 * access state machine.
 */
static inline gmosTaskStatus_t gmosDriverNfcTagTaskFn (
    gmosDriverNfcTag_t* nfcTag)
{
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    gmosDriverNfcTagState_t nextState = nfcTag->nfcTagState;

    // Implement NFC tag access state machine.
    switch (nfcTag->nfcTagState) {

        // In the idle state wait for a transaction request.
        case GMOS_DRIVER_NFC_TAG_STATE_IDLE :
            taskStatus = GMOS_TASK_SUSPEND;
            break;

        // Start a new low level read transaction.
        case GMOS_DRIVER_NFC_TAG_STATE_READ_START :
            taskStatus = gmosDriverNfcTagReadStart (nfcTag, &nextState);
            break;

        // Append read data to the receive buffer.
        case GMOS_DRIVER_NFC_TAG_STATE_READ_APPEND :
            taskStatus = gmosDriverNfcTagReadAppend (nfcTag, &nextState);
            break;

        // Set up a new low level write transaction.
        case GMOS_DRIVER_NFC_TAG_STATE_WRITE_START :
            taskStatus = gmosDriverNfcTagWriteStart (nfcTag, &nextState);
            break;

        // Queue a new low level write transaction.
        case GMOS_DRIVER_NFC_TAG_STATE_WRITE_QUEUE :
            taskStatus = gmosDriverNfcTagWriteQueue (nfcTag, &nextState);
            break;

        // Suspend processing on wait conditions.
        case GMOS_DRIVER_NFC_TAG_STATE_READ_WAIT :
        case GMOS_DRIVER_NFC_TAG_STATE_WRITE_WAIT :
        case GMOS_DRIVER_NFC_TAG_STATE_COMPLETE :
            taskStatus = GMOS_TASK_SUSPEND;
            break;

        // Suspend processing on failure conditions.
        default :
            GMOS_LOG (LOG_ERROR, "NFC tag driver failed.");
            taskStatus = GMOS_TASK_SUSPEND;
            break;
    }
    nfcTag->nfcTagState = nextState;
    return taskStatus;
}

// Add the NFC tag worker task definition.
GMOS_TASK_DEFINITION (gmosDriverNfcTagTask,
    gmosDriverNfcTagTaskFn, gmosDriverNfcTag_t);

/*
 * Initialises an NFC tag using the supplied radio configuration. The
 * radio specific options should already have been populated using the
 * 'GMOS_DRIVER_NFC_TAG_RAL_CONFIG' macro.
 */
bool gmosDriverNfcTagInit (
    gmosDriverNfcTag_t* nfcTag, gmosTaskState_t* clientTask)
{
    // Initialise the radio abstraction layer.
    if (!gmosDriverNfcTagRalInit (nfcTag)) {
        return false;
    }

    // Initialise the NFC tag driver state machine.
    nfcTag->nfcTagState = GMOS_DRIVER_NFC_TAG_STATE_IDLE;
    nfcTag->dataBuffer = NULL;

    // Initialise the completion event.
    gmosEventInit (&(nfcTag->completionEvent), clientTask);

    // Start the tag access task.
    gmosDriverNfcTagTask_start (&(nfcTag->workerTask),
        nfcTag, "NFC Tag Driver Task");
    return true;
}

/*
 * Initiates an NFC tag persistent data area read request.
 */
gmosDriverNfcTagStatus_t gmosDriverNfcTagRead (
    gmosDriverNfcTag_t* nfcTag, uint8_t dataArea,
    uint16_t readAddr, uint16_t readSize, gmosBuffer_t* readBuffer)
{
    gmosDriverNfcTagStatus_t status;
    uint16_t dataAreaSize;

    // Check that there is no other transaction in progress.
    if (nfcTag->nfcTagState == GMOS_DRIVER_NFC_TAG_STATE_FAILED) {
        status = GMOS_DRIVER_NFC_TAG_STATUS_FATAL_ERROR;
        goto out;
    } else if (nfcTag->nfcTagState != GMOS_DRIVER_NFC_TAG_STATE_IDLE) {
        status = GMOS_DRIVER_NFC_TAG_STATUS_NOT_READY;
        goto out;
    }

    // Check that the target data area can service the read request.
    status = gmosDriverNfcTagGetAreaInfo (
        nfcTag, dataArea, &dataAreaSize);
    if (status != GMOS_DRIVER_NFC_TAG_STATUS_SUCCESS) {
        goto out;
    } else if (readAddr + readSize > dataAreaSize) {
        status = GMOS_DRIVER_NFC_TAG_STATUS_INVALID_ADDRESS;
        goto out;
    }

    // Store the parameter data.
    gmosBufferReset (readBuffer, 0);
    nfcTag->dataBuffer = readBuffer;
    nfcTag->dataArea = dataArea;
    nfcTag->baseAddr = readAddr;
    nfcTag->dataSize = readSize;

    // Initiate the read request.
    nfcTag->nfcTagState = GMOS_DRIVER_NFC_TAG_STATE_READ_START;
    gmosSchedulerTaskResume (&(nfcTag->workerTask));

out:
    return status;
}

/*
 * Initiates an NFC tag persistent data area write request.
 */
gmosDriverNfcTagStatus_t gmosDriverNfcTagWrite (
    gmosDriverNfcTag_t* nfcTag, uint8_t dataArea,
    uint16_t writeAddr, gmosBuffer_t* writeBuffer)
{
    gmosDriverNfcTagStatus_t status;
    uint_fast16_t writeSize;
    uint16_t dataAreaSize;

    // Check that there is no other transaction in progress.
    if (nfcTag->nfcTagState == GMOS_DRIVER_NFC_TAG_STATE_FAILED) {
        status = GMOS_DRIVER_NFC_TAG_STATUS_FATAL_ERROR;
        goto out;
    } else if (nfcTag->nfcTagState != GMOS_DRIVER_NFC_TAG_STATE_IDLE) {
        status = GMOS_DRIVER_NFC_TAG_STATUS_NOT_READY;
        goto out;
    }

    // Check that the target data area can service the write request.
    writeSize = gmosBufferGetSize (writeBuffer);
    status = gmosDriverNfcTagGetAreaInfo (
        nfcTag, dataArea, &dataAreaSize);
    if (status != GMOS_DRIVER_NFC_TAG_STATUS_SUCCESS) {
        goto out;
    } else if (writeAddr + writeSize > dataAreaSize) {
        status = GMOS_DRIVER_NFC_TAG_STATUS_INVALID_ADDRESS;
        goto out;
    }

    // Store the parameter data.
    nfcTag->dataBuffer = writeBuffer;
    nfcTag->dataArea = dataArea;
    nfcTag->baseAddr = writeAddr;
    nfcTag->dataSize = 0;

    // Initiate the write request.
    nfcTag->nfcTagState = GMOS_DRIVER_NFC_TAG_STATE_WRITE_START;
    gmosSchedulerTaskResume (&(nfcTag->workerTask));

out:
    return status;
}

/*
 * Completes an NFC tag read or write transaction, indicating the status
 * of the transaction.
 */
gmosDriverNfcTagStatus_t gmosDriverNfcTagComplete (
    gmosDriverNfcTag_t* nfcTag)
{
    gmosDriverNfcTagStatus_t status = GMOS_DRIVER_NFC_TAG_STATUS_NOT_READY;

    // Check for transaction complete state.
    if (nfcTag->nfcTagState == GMOS_DRIVER_NFC_TAG_STATE_COMPLETE) {
        uint32_t eventFlags = gmosEventResetBits (&(nfcTag->completionEvent));
        if ((eventFlags & GMOS_DRIVER_NFC_TAG_EVENT_COMPLETION_FLAG) != 0) {
            status = eventFlags & GMOS_DRIVER_NFC_TAG_EVENT_STATUS_MASK;
            if (status == GMOS_DRIVER_NFC_TAG_STATUS_SUCCESS) {
                nfcTag->nfcTagState = GMOS_DRIVER_NFC_TAG_STATE_IDLE;
            } else {
                nfcTag->nfcTagState = GMOS_DRIVER_NFC_TAG_STATE_FAILED;
            }
        }
    }
    return status;
}
