/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2023-2025 Zynaptic Limited
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
 * This file implements a GubbinsMOS EEPROM driver wrapper for the
 * platform specific EFR32xG2x NVM3 non-volatile memory library. It
 * only supports the single default NVM3 area at the top of flash
 * memory.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "gmos-config.h"

// Build the platform specific library implementation if required.
#if GMOS_CONFIG_EEPROM_PLATFORM_LIBRARY
#include "gmos-platform.h"
#include "gmos-driver-eeprom.h"
#include "efr32-driver-eeprom.h"
#include "nvm3_default.h"

/*
 * This enumeration specifies the various EEPROM operating states.
 */
typedef enum {
    GMOS_DRIVER_EEPROM_STATE_IDLE,
    GMOS_DRIVER_EEPROM_STATE_WRITE,
} gmosDriverEepromState_t;

/*
 * Specify the main EEPROM instance that will be used for storing system
 * data.
 */
static gmosDriverEeprom_t* mainInstance = NULL;

/*
 * Map NVM error codes to the common EEPROM API error codes.
 */
static gmosDriverEepromStatus_t gmosDriverEepromMapError (
    Ecode_t nvmStatus)
{
    gmosDriverEepromStatus_t eepromStatus;
    switch (nvmStatus) {
        case ECODE_NVM3_ERR_STORAGE_FULL :
            eepromStatus = GMOS_DRIVER_EEPROM_STATUS_OUT_OF_MEMORY;
            break;
        case ECODE_NVM3_ERR_KEY_INVALID :
            eepromStatus = GMOS_DRIVER_EEPROM_STATUS_INVALID_TAG;
            break;
        case ECODE_NVM3_ERR_KEY_NOT_FOUND :
            eepromStatus = GMOS_DRIVER_EEPROM_STATUS_NO_RECORD;
            break;
        case ECODE_NVM3_ERR_WRITE_DATA_SIZE :
        case ECODE_NVM3_ERR_READ_DATA_SIZE :
            eepromStatus = GMOS_DRIVER_EEPROM_STATUS_INVALID_LENGTH;
            break;
        default :
            eepromStatus = GMOS_DRIVER_EEPROM_STATUS_FATAL_ERROR;
            break;
    }
    return eepromStatus;
}

/*
 * Implement inline status code mapping.
 */
static inline gmosDriverEepromStatus_t gmosDriverEepromMapStatus (
    Ecode_t nvmStatus)
{
    gmosDriverEepromStatus_t eepromStatus;
    if (nvmStatus == ECODE_NVM3_OK) {
        eepromStatus = GMOS_DRIVER_EEPROM_STATUS_SUCCESS;
    } else {
        eepromStatus = gmosDriverEepromMapError (nvmStatus);
    }
    return eepromStatus;
}

/*
 * Check the current information for an EEPROM record.
 */
static gmosDriverEepromStatus_t gmosDriverEepromRecordCheck (
    gmosDriverEeprom_t* eeprom, gmosDriverEepromTag_t recordTag)
{
    Ecode_t nvmStatus;
    uint32_t nvmType;

    // Defer processing if a transaction is in progress.
    if (eeprom->eepromState != GMOS_DRIVER_EEPROM_STATE_IDLE) {
        return GMOS_DRIVER_EEPROM_STATUS_NOT_READY;
    }

    // Derive the NVM3 object key from the specified EEPROM tag.
    eeprom->recordTag = (((nvm3_ObjectKey_t) recordTag) & 0xFFFF) |
        (GMOS_CONFIG_EFR32_EEPROM_NVM3_KEY_SPACE & 0xF0000);

    // Check for existing tag values.
    nvmStatus = nvm3_getObjectInfo (eeprom->platformNvm,
        eeprom->recordTag, &nvmType, &(eeprom->recordSize));
    if (nvmStatus == ECODE_NVM3_OK) {
        if (nvmType == NVM3_OBJECTTYPE_DATA) {
            return GMOS_DRIVER_EEPROM_STATUS_TAG_EXISTS;
        } else {
            return GMOS_DRIVER_EEPROM_STATUS_FORMATTING_ERROR;
        }
    } else  {
        return gmosDriverEepromMapStatus (nvmStatus);
    }
}

/*
 * Write an initial value using all zeros.
 */
static inline Ecode_t gmosDriverEepromWriteZero (nvm3_Handle_t* nvm3,
    nvm3_ObjectKey_t tag, size_t length)
{
    uint8_t zerosData [length];
    memset (zerosData, 0, length);
    return nvm3_writeData (nvm3, tag, zerosData, length);
}

/*
 * Implement the EEPROM driver task function that provides the EEPROM
 * access state machine.
 */
static gmosTaskStatus_t gmosDriverEepromTask (void* taskData)
{
    gmosDriverEeprom_t* eeprom = (gmosDriverEeprom_t*) taskData;
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    Ecode_t nvmStatus;
    uint8_t nextState = eeprom->eepromState;

    // Implement EEPROM access state machine.
    switch (eeprom->eepromState) {

        // In the idle state issue periodic repacking requests. The
        // repacking state is checked every second while the scheduler
        // is running.
        case GMOS_DRIVER_EEPROM_STATE_IDLE :
            taskStatus = GMOS_TASK_RUN_AFTER (GMOS_MS_TO_TICKS (1000));
            if (nvm3_repackNeeded (eeprom->platformNvm)) {
                nvmStatus = nvm3_repack (eeprom->platformNvm);
                if (nvmStatus == ECODE_NVM3_OK) {
                    taskStatus = GMOS_TASK_RUN_BACKGROUND;
                } else {
                    GMOS_LOG_FMT (LOG_DEBUG,
                        "NVM3 failed to repack with status code 0x%X.",
                        nvmStatus);
                }
            }
            break;

        // Perform a NVM3 write transaction. The NVM3 API uses blocking
        // writes, so this may be a relatively long running operation.
        case GMOS_DRIVER_EEPROM_STATE_WRITE :
            if (eeprom->writeData == NULL) {
                nvmStatus = gmosDriverEepromWriteZero (eeprom->platformNvm,
                    eeprom->recordTag, eeprom->recordSize);
            } else {
                nvmStatus = nvm3_writeData (eeprom->platformNvm,
                    eeprom->recordTag, eeprom->writeData, eeprom->recordSize);
            }
            eeprom->eepromStatus = gmosDriverEepromMapStatus (nvmStatus);
            if (eeprom->callbackHandler != NULL) {
                eeprom->callbackHandler (
                    eeprom->eepromStatus, eeprom->callbackData);
            }
            nextState = GMOS_DRIVER_EEPROM_STATE_IDLE;
            taskStatus = GMOS_TASK_RUN_BACKGROUND;
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
 * also perform a factory reset on the EEPROM contents, invalidating all
 * of the current EEPROM records. The main instance flag must be set in
 * order to indicate that the default NVM3 instance is to be used.
 */
bool gmosDriverEepromInit (gmosDriverEeprom_t* eeprom,
    bool isMainInstance, bool factoryReset, uint32_t factoryResetKey)
{
    Ecode_t nvmStatus;

    // Only the main NVM3 instance is supported.
    if ((eeprom == NULL) || (!isMainInstance)) {
        return false;
    }

    // Initialise the default NVM3 instance.
    nvmStatus = nvm3_initDefault ();
    if (nvmStatus != ECODE_NVM3_OK) {
        GMOS_LOG_FMT (LOG_DEBUG,
            "NVM3 failed to initialise with status code 0x%X.",
            nvmStatus);
        return false;
    }

    // Perform a factory reset if required.
    if (factoryReset) {
        if (factoryResetKey == GMOS_DRIVER_EEPROM_FACTORY_RESET_KEY) {
            nvmStatus = nvm3_eraseAll (nvm3_defaultHandle);
            if (nvmStatus != ECODE_NVM3_OK) {
                GMOS_LOG_FMT (LOG_DEBUG,
                    "NVM3 failed to clear with status code 0x%X.",
                    nvmStatus);
                return false;
            }
        } else {
            GMOS_LOG (LOG_DEBUG,
                "NVM3 failed to clear with invalid reset key.");
            return false;
        }
    }

    // Set up the main EEPROM instance using the default NVM3 instance.
    eeprom->platformNvm = nvm3_defaultHandle;
    mainInstance = eeprom;
    GMOS_LOG_FMT (LOG_INFO,
        "NVM3 main EEPROM data size %d, max item size %d.",
        nvm3_defaultInit->nvmSize, nvm3_defaultInit->maxObjectSize);

    // Start the EEPROM driver task.
    eeprom->eepromState = GMOS_DRIVER_EEPROM_STATE_IDLE;
    eeprom->workerTask.taskTickFn = gmosDriverEepromTask;
    eeprom->workerTask.taskData = eeprom;
    eeprom->workerTask.taskName = "NVM3 EEPROM Driver";
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
    gmosDriverEepromStatus_t eepromStatus;

    // Check the current record status and populate the common record
    // fields.
    eepromStatus = gmosDriverEepromRecordCheck (eeprom, recordTag);
    if ((eepromStatus == GMOS_DRIVER_EEPROM_STATUS_TAG_EXISTS) &&
        (eeprom->recordSize != recordLength)) {
        GMOS_LOG_FMT (LOG_DEBUG,
            "Requested record length = %d, current record length = %d.",
            recordLength, eeprom->recordSize);
        return GMOS_DRIVER_EEPROM_STATUS_INVALID_LENGTH;
    } else if (eepromStatus != GMOS_DRIVER_EEPROM_STATUS_NO_RECORD) {
        return eepromStatus;
    }

    // Set up a write transaction to create the new record.
    eeprom->eepromState = GMOS_DRIVER_EEPROM_STATE_WRITE;
    eeprom->writeData = defaultValue;
    eeprom->recordSize = recordLength;
    eeprom->eepromStatus = GMOS_DRIVER_EEPROM_STATUS_SUCCESS;
    eeprom->callbackHandler = callbackHandler;
    eeprom->callbackData = callbackData;

    // Resume the processing task or run as a blocking transaction if
    // no callback is specified.
    if (callbackHandler != NULL) {
        gmosSchedulerTaskResume (&(eeprom->workerTask));
    } else {
        gmosDriverEepromRunToIdle (eeprom);
    }
    return eeprom->eepromStatus;
}

/*
 * Writes data to an EEPROM data record, copying it from the specified
 * write data byte array.
 */
gmosDriverEepromStatus_t gmosDriverEepromRecordWrite (
    gmosDriverEeprom_t* eeprom, gmosDriverEepromTag_t recordTag,
    uint8_t* writeData, uint16_t writeSize,
    gmosPalEepromCallback_t callbackHandler, void* callbackData)
{
    gmosDriverEepromStatus_t eepromStatus;

    // Check the current record status and populate the common record
    // fields.
    eepromStatus = gmosDriverEepromRecordCheck (eeprom, recordTag);
    if (eepromStatus != GMOS_DRIVER_EEPROM_STATUS_TAG_EXISTS) {
        return eepromStatus;
    } else if (eeprom->recordSize != writeSize) {
        return GMOS_DRIVER_EEPROM_STATUS_INVALID_LENGTH;
    }

    // Set up a write transaction to write the new data.
    eeprom->eepromState = GMOS_DRIVER_EEPROM_STATE_WRITE;
    eeprom->writeData = writeData;
    eeprom->eepromStatus = GMOS_DRIVER_EEPROM_STATUS_SUCCESS;
    eeprom->callbackHandler = callbackHandler;
    eeprom->callbackData = callbackData;

    // Resume the processing task or run as a blocking transaction if
    // no callback is specified.
    if (callbackHandler != NULL) {
        gmosSchedulerTaskResume (&(eeprom->workerTask));
    } else {
        gmosDriverEepromRunToIdle (eeprom);
    }
    return eeprom->eepromStatus;
}

/*
 * Reads data from an EEPROM data record, storing it in the specified
 * read data byte array.
 */
gmosDriverEepromStatus_t gmosDriverEepromRecordRead (
    gmosDriverEeprom_t* eeprom, gmosDriverEepromTag_t recordTag,
    uint8_t* readData, uint16_t readOffset, uint16_t readSize)
{
    Ecode_t nvmStatus;
    gmosDriverEepromStatus_t eepromStatus;

    // Check the current record status and populate the common record
    // fields.
    eepromStatus = gmosDriverEepromRecordCheck (eeprom, recordTag);
    if (eepromStatus != GMOS_DRIVER_EEPROM_STATUS_TAG_EXISTS) {
        return eepromStatus;
    } else if (readOffset + readSize > eeprom->recordSize) {
        return GMOS_DRIVER_EEPROM_STATUS_INVALID_LENGTH;
    }

    // Issue the partial read request to the NVM3 library.
    nvmStatus = nvm3_readPartialData (eeprom->platformNvm,
        eeprom->recordTag, readData, readOffset, readSize);
    return gmosDriverEepromMapStatus (nvmStatus);
}

/*
 * Reads all the data from an EEPROM data record, storing it in the
 * specified read data byte array.
 */
gmosDriverEepromStatus_t gmosDriverEepromRecordReadAll (
    gmosDriverEeprom_t* eeprom, gmosDriverEepromTag_t recordTag,
    uint8_t* readData, uint16_t readMaxSize, uint16_t* readSize)
{
    Ecode_t nvmStatus;
    gmosDriverEepromStatus_t eepromStatus;

    // Check the current record status and populate the common record
    // fields.
    eepromStatus = gmosDriverEepromRecordCheck (eeprom, recordTag);
    if (eepromStatus != GMOS_DRIVER_EEPROM_STATUS_TAG_EXISTS) {
        return eepromStatus;
    } else if (readMaxSize < eeprom->recordSize) {
        return GMOS_DRIVER_EEPROM_STATUS_INVALID_LENGTH;
    } else if (readSize != NULL) {
        *readSize = eeprom->recordSize;
    }

    // Issue the full read request to the NVM3 library.
    nvmStatus = nvm3_readData (eeprom->platformNvm,
        eeprom->recordTag, readData, eeprom->recordSize);
    return gmosDriverEepromMapStatus (nvmStatus);
}

#endif // GMOS_CONFIG_EEPROM_PLATFORM_LIBRARY
