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
 * Implements the common components of the GubbinsMOS EEPROM driver.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-driver-eeprom.h"

/*
 * Define the EEPROM end of record marker. This consists of the end of
 * EEPROM tag followed by a length of zero. This supports tag and length
 * values of
 */
static const uint8_t eepromEndTag [GMOS_DRIVER_EEPROM_HEADER_SIZE] = {
    (GMOS_DRIVER_EEPROM_TAG_END_MARKER) & 0xFF,
#if (GMOS_CONFIG_EEPROM_TAG_SIZE >= 2)
    (GMOS_DRIVER_EEPROM_TAG_END_MARKER >> 8) & 0xFF,
#endif
#if (GMOS_CONFIG_EEPROM_TAG_SIZE >= 3)
    (GMOS_DRIVER_EEPROM_TAG_END_MARKER >> 16) & 0xFF,
#endif
#if (GMOS_CONFIG_EEPROM_TAG_SIZE == 4)
    (GMOS_DRIVER_EEPROM_TAG_END_MARKER >> 23) & 0xFF,
#endif
#if (GMOS_CONFIG_EEPROM_LENGTH_SIZE == 2)
    0,
#endif
    0 };

/*
 * Specify the main EEPROM instance that will be used for storing system
 * data.
 */
static gmosDriverEeprom_t* mainInstance = NULL;

/*
 * Searches for an EEPROM record, updating the record base offset to
 * select the start of the record if a matching record tag is found.
 */
static gmosDriverEepromStatus_t gmosDriverEepromRecordSearch (
    gmosDriverEeprom_t* eeprom, uint32_t recordTag,
    uint16_t* recordBase, uint16_t* recordLength)
{
    uint16_t currentOffset;
    uint32_t currentTag;
    uint16_t currentLength;
    uint8_t* recordData;
    uint32_t i;
    gmosDriverEepromStatus_t status;

    // Determine whether the specified tag is a reserved value or
    // outside the valid range.
    if ((recordTag > ((1 << (8 * GMOS_CONFIG_EEPROM_TAG_SIZE)) - 1)) ||
        (recordTag == GMOS_DRIVER_EEPROM_TAG_END_MARKER) ||
        (recordTag == GMOS_DRIVER_EEPROM_TAG_FREE_SPACE)) {
        return GMOS_DRIVER_EEPROM_STATUS_INVALID_TAG;
    }

    // Determine whether the EEPROM is currently busy.
    if (eeprom->eepromState != GMOS_DRIVER_EEPROM_STATE_IDLE) {
        return GMOS_DRIVER_EEPROM_STATUS_NOT_READY;
    }

    // Perform a linear search on the EEPROM record list until a
    // matching tag or the end of list tag are found.
    currentOffset = 0;
    while (true) {
        if (currentOffset <=
            eeprom->memSize - GMOS_DRIVER_EEPROM_HEADER_SIZE) {
            recordData = eeprom->baseAddress + currentOffset;
        } else {
            return GMOS_DRIVER_EEPROM_STATUS_FORMATTING_ERROR;
        }

        // Derive the tag and length fields for the current record.
        currentTag = 0;
        for (i = 0; i < GMOS_CONFIG_EEPROM_TAG_SIZE; i++) {
            currentTag += ((uint32_t) *(recordData++)) << (8 * i);
        }
        currentLength = 0;
        for (i = 0; i < GMOS_CONFIG_EEPROM_LENGTH_SIZE; i++) {
            currentLength += ((uint32_t) *(recordData++)) << (8 * i);
        }

        // Check for tag matches and end of record list.
        if (currentTag == recordTag) {
            status = GMOS_DRIVER_EEPROM_STATUS_SUCCESS;
            break;
        }
        else if ((currentTag == GMOS_DRIVER_EEPROM_TAG_END_MARKER) &&
            (currentLength == 0)) {
            status = GMOS_DRIVER_EEPROM_STATUS_NO_RECORD;
            break;
        }

        // Skip to the next record in the list.
        currentOffset += GMOS_DRIVER_EEPROM_HEADER_SIZE;
        currentOffset += currentLength;
    }

    // Update the record base offset on completion.
    *recordBase = currentOffset;
    *recordLength = currentLength;
    return status;
}

/*
 * Implement the EEPROM driver task function that provides the EEPROM
 * access state machine.
 */
static gmosTaskStatus_t gmosDriverEepromTask (void* taskData)
{
    gmosDriverEeprom_t* eeprom = (gmosDriverEeprom_t*) taskData;
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    uint8_t nextState = eeprom->eepromState;

    // Implement EEPROM access state machine.
    switch (eeprom->eepromState) {

        // Write the end of record tag at the start of the EEPROM on a
        // factory reset.
        case GMOS_DRIVER_EEPROM_STATE_RESET_TAG_WRITE :
            if (gmosPalEepromWriteData (eeprom, 0,
                eepromEndTag, GMOS_DRIVER_EEPROM_HEADER_SIZE)) {
                nextState = GMOS_DRIVER_EEPROM_STATE_COMPLETION_WAIT;
            }
            break;

        // Write the end of record tag.
        case GMOS_DRIVER_EEPROM_STATE_CREATE_END_TAG_WRITE :
            if (gmosPalEepromWriteData (eeprom,
                eeprom->writeOffset + eeprom->writeSize,
                eepromEndTag, GMOS_DRIVER_EEPROM_HEADER_SIZE)) {
                nextState = GMOS_DRIVER_EEPROM_STATE_CREATE_VALUE_WRITE;
            }
            break;

        // Write a default EEPROM record value.
        case GMOS_DRIVER_EEPROM_STATE_CREATE_VALUE_WRITE :
            if (gmosPalEepromWritePoll (eeprom)) {
                taskStatus = GMOS_TASK_RUN_LATER (1);
            } else if (gmosPalEepromWriteData (eeprom,
                eeprom->writeOffset, eeprom->writeData,
                eeprom->writeSize)) {
                nextState = GMOS_DRIVER_EEPROM_STATE_CREATE_HEADER_WRITE;
            }
            break;

        // Write the EEPROM record header.
        case GMOS_DRIVER_EEPROM_STATE_CREATE_HEADER_WRITE :
            if (gmosPalEepromWritePoll (eeprom)) {
                taskStatus = GMOS_TASK_RUN_LATER (1);
            } else if (gmosPalEepromWriteData (eeprom,
                eeprom->writeOffset - GMOS_DRIVER_EEPROM_HEADER_SIZE,
                eeprom->writeHeader, GMOS_DRIVER_EEPROM_HEADER_SIZE)) {
                nextState = GMOS_DRIVER_EEPROM_STATE_COMPLETION_WAIT;
            }
            break;

        // Initiate a record value update write transaction.
        case GMOS_DRIVER_EEPROM_STATE_UPDATE_VALUE_WRITE :
            if (gmosPalEepromWriteData (eeprom, eeprom->writeOffset,
                eeprom->writeData, eeprom->writeSize)) {
                nextState = GMOS_DRIVER_EEPROM_STATE_COMPLETION_WAIT;
            }
            break;

        // Wait for the transaction to be completed before issuing the
        // callback.
        case GMOS_DRIVER_EEPROM_STATE_COMPLETION_WAIT :
            if (!gmosPalEepromWritePoll (eeprom)) {
                if (eeprom->callbackHandler != NULL) {
                    eeprom->callbackHandler (
                        GMOS_DRIVER_EEPROM_STATUS_SUCCESS,
                        eeprom->callbackData);
                    eeprom->callbackHandler = NULL;
                }
                nextState = GMOS_DRIVER_EEPROM_STATE_IDLE;
            }
            break;

        // Suspend further processing from the idle state.
        default :
            taskStatus = GMOS_TASK_SUSPEND;
            break;
    }
    eeprom->eepromState = nextState;
    return taskStatus;
}

/*
 * Run the EEPROM state machine as a blocking operation.
 */
static void gmosDriverEepromRunToIdle (gmosDriverEeprom_t* eeprom)
{
    while (eeprom->eepromState != GMOS_DRIVER_EEPROM_STATE_IDLE) {
        gmosDriverEepromTask (eeprom);
    }
}

/*
 * Initialises the EEPROM driver. This should be called once on startup
 * in order to initialise the EEPROM driver state. If required, it may
 * also perform a factory reset on the EEPROM contents, deleting all of
 * the current EEPROM records.
 */
bool gmosDriverEepromInit (gmosDriverEeprom_t* eeprom,
    bool isMainInstance, bool factoryReset, uint32_t factoryResetKey)
{
    // First initialise the platform abstraction layer.
    if (!gmosPalEepromInit (eeprom)) {
        return false;
    }

    // Initialise the EEPROM driver state machine.
    if (!factoryReset) {
        eeprom->eepromState = GMOS_DRIVER_EEPROM_STATE_IDLE;
    }

    // Attempt to perform a factory reset.
    else if (factoryResetKey == GMOS_DRIVER_EEPROM_FACTORY_RESET_KEY) {
        eeprom->callbackHandler = NULL;
        eeprom->eepromState = GMOS_DRIVER_EEPROM_STATE_RESET_TAG_WRITE;
    } else {
        return false;
    }

    // Set the EEPROM as the main instance for storing system data.
    if (isMainInstance) {
        mainInstance = eeprom;
    }

    // Start the EEPROM driver task.
    eeprom->workerTask.taskTickFn = gmosDriverEepromTask;
    eeprom->workerTask.taskData = eeprom;
    eeprom->workerTask.taskName = "EEPROM Driver";
    gmosSchedulerTaskStart (&(eeprom->workerTask));
    return true;
}

/*
 * Accesses the main EEPROM instance to be used for storing system
 * information. For most configurations this will be the only EEPROM
 * on the device.
 */
gmosDriverEeprom_t* gmosDriverEepromGetInstance (void)
{
    return mainInstance;
}

/*
 * Creates a new EEPROM data record with the specified tag, length and
 * default value. This will fail if a record with the specified tag
 * already exists.
 */
gmosDriverEepromStatus_t gmosDriverEepromRecordCreate (
    gmosDriverEeprom_t* eeprom, gmosDriverEepromTag_t recordTag,
    uint8_t* defaultValue, uint16_t recordLength,
    gmosPalEepromCallback_t callbackHandler, void* callbackData)
{
    uint16_t searchBase;
    uint16_t searchLength;
    uint8_t* headerData;
    gmosDriverEepromStatus_t status;
    uint32_t i;

    // Search for a matching EEPROM data record.
    status = gmosDriverEepromRecordSearch (
        eeprom, recordTag, &searchBase, &searchLength);
    if (status == GMOS_DRIVER_EEPROM_STATUS_SUCCESS) {
        if (recordLength == searchLength) {
            return GMOS_DRIVER_EEPROM_STATUS_TAG_EXISTS;
        } else {
            return GMOS_DRIVER_EEPROM_STATUS_INVALID_LENGTH;
        }
    } else if (status != GMOS_DRIVER_EEPROM_STATUS_NO_RECORD) {
        return status;
    }

    // Check for sufficient storage space.
    if (searchBase > eeprom->memSize -
        (recordLength + GMOS_DRIVER_EEPROM_HEADER_SIZE)) {
        return GMOS_DRIVER_EEPROM_STATUS_OUT_OF_MEMORY;
    }

    // Fill in the header for the new EEPROM record.
    headerData = eeprom->writeHeader;
    for (i = 0; i < GMOS_CONFIG_EEPROM_TAG_SIZE; i++) {
        *(headerData++) = (uint8_t) (recordTag >> (8 * i));
    }
    for (i = 0; i < GMOS_CONFIG_EEPROM_LENGTH_SIZE; i++) {
        *(headerData++) = (uint8_t) (recordLength >> (8 * i ));
    }

    // Set up the EEPROM write transaction data.
    eeprom->writeOffset = searchBase + GMOS_DRIVER_EEPROM_HEADER_SIZE;
    eeprom->writeSize = recordLength;
    eeprom->writeData = defaultValue;
    eeprom->callbackHandler = callbackHandler;
    eeprom->callbackData = callbackData;

    // Initiate the create record sequence. If a callback handler has
    // not been provided, this will block until completion.
    eeprom->eepromState = GMOS_DRIVER_EEPROM_STATE_CREATE_END_TAG_WRITE;
    if (callbackHandler != NULL) {
        gmosSchedulerTaskResume (&(eeprom->workerTask));
    } else {
        gmosDriverEepromRunToIdle (eeprom);
    }
    return GMOS_DRIVER_EEPROM_STATUS_SUCCESS;
}

/*
 * Writes data to an EEPROM data record, copying it from the specified
 * write data byte array.
 */
gmosDriverEepromStatus_t gmosDriverEepromRecordWrite (
    gmosDriverEeprom_t* eeprom, gmosDriverEepromTag_t recordTag,
    uint8_t* writeData, uint16_t writeOffset, uint16_t writeSize,
    gmosPalEepromCallback_t callbackHandler, void* callbackData)
{
    gmosDriverEepromStatus_t status;
    uint16_t recordBase;
    uint16_t recordLength;

    // Search for a matching EEPROM data record.
    status = gmosDriverEepromRecordSearch (
        eeprom, recordTag, &recordBase, &recordLength);
    if (status != GMOS_DRIVER_EEPROM_STATUS_SUCCESS) {
        return status;
    }

    // Check for valid access parameters.
    if (writeOffset + writeSize > recordLength) {
        return GMOS_DRIVER_EEPROM_STATUS_INVALID_LENGTH;
    }

    // Set up the write transaction.
    eeprom->writeOffset = recordBase +
        writeOffset + GMOS_DRIVER_EEPROM_HEADER_SIZE;
    eeprom->writeSize = writeSize;
    eeprom->writeData = writeData;
    eeprom->callbackHandler = callbackHandler;
    eeprom->callbackData = callbackData;

    // Initiate the write record sequence. If a callback handler has
    // not been provided, this will block until completion.
    eeprom->eepromState = GMOS_DRIVER_EEPROM_STATE_UPDATE_VALUE_WRITE;
    if (callbackHandler != NULL) {
        gmosSchedulerTaskResume (&(eeprom->workerTask));
    } else {
        gmosDriverEepromRunToIdle (eeprom);
    }
    return GMOS_DRIVER_EEPROM_STATUS_SUCCESS;
}

/*
 * Reads data from an EEPROM data record, storing it in the specified
 * read data octet array.
 */
gmosDriverEepromStatus_t gmosDriverEepromRecordRead (
    gmosDriverEeprom_t* eeprom, gmosDriverEepromTag_t recordTag,
    uint8_t* readData, uint16_t readOffset, uint16_t readSize)
{
    gmosDriverEepromStatus_t status;
    uint16_t recordBase;
    uint16_t recordLength;
    uint8_t* recordData;

    // Search for a matching EEPROM data record.
    status = gmosDriverEepromRecordSearch (
        eeprom, recordTag, &recordBase, &recordLength);
    if (status != GMOS_DRIVER_EEPROM_STATUS_SUCCESS) {
        return status;
    }

    // Check for valid access parameters.
    if (readOffset + readSize > recordLength) {
        return GMOS_DRIVER_EEPROM_STATUS_INVALID_LENGTH;
    }

    // Copy over the record data.
    recordData = eeprom->baseAddress + recordBase;
    recordData += readOffset + GMOS_DRIVER_EEPROM_HEADER_SIZE;
    while (readSize > 0) {
        *(readData++) = *(recordData++);
        readSize -= 1;
    }
    return GMOS_DRIVER_EEPROM_STATUS_SUCCESS;
}
