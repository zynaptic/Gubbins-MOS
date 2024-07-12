/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2024 Zynaptic Limited
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
 * This file implements a GubbinsMOS flash memory driver for the
 * platform specific EFR32xG2x flash memory.
 */

#include <stdbool.h>
#include <string.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-scheduler.h"
#include "gmos-events.h"
#include "gmos-driver-flash.h"
#include "efr32-driver-flash.h"
#include "em_msc.h"

// Specify the EFR32 flash memory erase block size.
#define EFR32_FLASH_BLOCK_SIZE 8192

// Specify the minimum EFR32 flash memory read size.
#define EFR32_FLASH_READ_SIZE 1

// Specify the minimum EFR32 flash memory write size.
#define EFR32_FLASH_WRITE_SIZE 4

// Specify the bulk erase stop address.
#define EFR32_FLASH_ERASE_STOP 0xFFFFFFFF

/*
 * Implements the bulk erase task function for erasing the entire flash
 * memory contents.
 */
static inline gmosTaskStatus_t gmosPalFlashEraseAllTaskFn (
    gmosDriverFlash_t* flash)
{
    gmosPalFlashConfigEfr32_t* palConfig =
        (gmosPalFlashConfigEfr32_t*) flash->palConfig;
    gmosPalFlashStateEfr32_t* palData =
        (gmosPalFlashStateEfr32_t*) flash->palData;
    MSC_Status_TypeDef mscStatus;
    uint32_t eventBits = 0;

    // The default task tick interval is set to 10ms to allow other
    // tasks to run between the blocking erase operations.
    gmosTaskStatus_t taskStatus =
        GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (10));

    // Suspend task if the 'stop' address is present.
    if (palData->bulkEraseAddr == EFR32_FLASH_ERASE_STOP) {
        taskStatus = GMOS_TASK_SUSPEND;
        goto out;
    }

    // Initiate the blocking erase request.
    mscStatus = MSC_ErasePage ((void*) palData->bulkEraseAddr);

    // Increment block erase address on success. Complete task if the
    // final block has been erased.
    if (mscStatus == mscReturnOk) {
        palData->bulkEraseAddr += EFR32_FLASH_BLOCK_SIZE;
        if (palData->bulkEraseAddr >=
            palConfig->baseAddress + palConfig->memorySize) {
            eventBits = GMOS_DRIVER_FLASH_EVENT_COMPLETION_FLAG |
                GMOS_DRIVER_FLASH_STATUS_SUCCESS;
        }
    }

    // Map status codes on failure.
    else if (mscStatus == mscReturnLocked) {
        eventBits = GMOS_DRIVER_FLASH_EVENT_COMPLETION_FLAG |
            GMOS_DRIVER_FLASH_STATUS_WRITE_LOCKED;
    } else {
        eventBits = GMOS_DRIVER_FLASH_EVENT_COMPLETION_FLAG |
            GMOS_DRIVER_FLASH_STATUS_DRIVER_ERROR;
    }

    // Signal completion if required.
    if (eventBits != 0) {
        gmosEventAssignBits (&(flash->completionEvent), eventBits);
        palData->bulkEraseAddr = EFR32_FLASH_ERASE_STOP;
        taskStatus = GMOS_TASK_SUSPEND;
    }
out:
    return taskStatus;
}
GMOS_TASK_DEFINITION (gmosPalFlashEraseAll,
    gmosPalFlashEraseAllTaskFn, gmosDriverFlash_t);

/*
 * Implements the platform specific write enable function to be used for
 * the on-chip flash memory.
 */
static bool gmosPalFlashWriteEnableEfr32 (gmosDriverFlash_t* flash,
    bool writeEnable)
{
    uint32_t eventBits;

    // When flash writes are enabled the MSC block will be initialised,
    // which enables the clock tree and unlocks the MSC write functions.
    if (writeEnable) {
        MSC_Init ();
        eventBits = GMOS_DRIVER_FLASH_EVENT_COMPLETION_FLAG |
            GMOS_DRIVER_FLASH_EVENT_WRITE_ENABLED_FLAG |
            GMOS_DRIVER_FLASH_STATUS_SUCCESS;
    }

    // When flash writes are disabled, the MSC block will be
    // deinitialised, which locks the MSC write functions and disables
    // the clock tree.
    else {
        MSC_Deinit ();
        eventBits = GMOS_DRIVER_FLASH_EVENT_COMPLETION_FLAG |
            GMOS_DRIVER_FLASH_EVENT_WRITE_DISABLED_FLAG |
            GMOS_DRIVER_FLASH_STATUS_SUCCESS;
    }

    // Indicate successful completion.
    gmosEventAssignBits (&(flash->completionEvent), eventBits);
    return true;
}

/*
 * Implements the platform specific read function to be used for the
 * on-chip flash memory. This can just copy directly from the memory
 * mapped flash.
 */
static bool gmosPalFlashReadEfr32 (gmosDriverFlash_t* flash,
    uint32_t readAddr, uint8_t* readData, uint16_t readSize)
{
    gmosPalFlashConfigEfr32_t* palConfig =
        (gmosPalFlashConfigEfr32_t*) flash->palConfig;
    void* readPtr = (void*) (readAddr + palConfig->baseAddress);

    // Transfer the read data using a standard memory copy.
    memcpy (readData, readPtr, readSize);

    // Indicate successful completion.
    gmosEventAssignBits (&(flash->completionEvent),
        GMOS_DRIVER_FLASH_EVENT_COMPLETION_FLAG |
        GMOS_DRIVER_FLASH_STATUS_SUCCESS);
    return true;
}

/*
 * Implements the platform specific write function to be used for the
 * on-chip flash memory.
 * TODO: Support DMA based transfers for large write requests from RAM.
 */
static bool gmosPalFlashWriteEfr32 (gmosDriverFlash_t* flash,
    uint32_t writeAddr, uint8_t* writeData, uint16_t writeSize)
{
    gmosPalFlashConfigEfr32_t* palConfig =
        (gmosPalFlashConfigEfr32_t*) flash->palConfig;
    void* writePtr = (void*) (writeAddr + palConfig->baseAddress);
    MSC_Status_TypeDef mscStatus;
    uint32_t eventBits;

    // Initiate the blocking write request.
    mscStatus = MSC_WriteWord (writePtr, writeData, writeSize);

    // Map status codes on success or failure.
    if (mscStatus == mscReturnOk) {
        eventBits = GMOS_DRIVER_FLASH_EVENT_COMPLETION_FLAG |
            GMOS_DRIVER_FLASH_STATUS_SUCCESS;
    } else if (mscStatus == mscReturnLocked) {
        eventBits = GMOS_DRIVER_FLASH_EVENT_COMPLETION_FLAG |
            GMOS_DRIVER_FLASH_STATUS_WRITE_LOCKED;
    } else {
        eventBits = GMOS_DRIVER_FLASH_EVENT_COMPLETION_FLAG |
            GMOS_DRIVER_FLASH_STATUS_DRIVER_ERROR;
    }

    // Indicate completion.
    gmosEventAssignBits (&(flash->completionEvent), eventBits);
    return true;
}

/*
 * Implements the platform specific block erase function to be used for
 * the on-chip flash memory.
 */
static bool gmosPalFlashEraseEfr32 (gmosDriverFlash_t* flash,
    uint32_t eraseAddr)
{
    gmosPalFlashConfigEfr32_t* palConfig =
        (gmosPalFlashConfigEfr32_t*) flash->palConfig;
    void* erasePtr = (void*) (eraseAddr + palConfig->baseAddress);
    MSC_Status_TypeDef mscStatus;
    uint32_t eventBits;

    // Initiate the blocking erase request.
    mscStatus = MSC_ErasePage (erasePtr);

    // Map status codes on success or failure.
    if (mscStatus == mscReturnOk) {
        eventBits = GMOS_DRIVER_FLASH_EVENT_COMPLETION_FLAG |
            GMOS_DRIVER_FLASH_STATUS_SUCCESS;
    } else if (mscStatus == mscReturnLocked) {
        eventBits = GMOS_DRIVER_FLASH_EVENT_COMPLETION_FLAG |
            GMOS_DRIVER_FLASH_STATUS_WRITE_LOCKED;
    } else {
        eventBits = GMOS_DRIVER_FLASH_EVENT_COMPLETION_FLAG |
            GMOS_DRIVER_FLASH_STATUS_DRIVER_ERROR;
    }

    // Indicate completion.
    gmosEventAssignBits (&(flash->completionEvent), eventBits);
    return true;
}

/*
 * Implements the platform specific bulk erase function to be used for
 * the on-chip flash memory.
 */
static bool gmosPalFlashEraseAllEfr32 (gmosDriverFlash_t* flash)
{
    gmosPalFlashConfigEfr32_t* palConfig =
        (gmosPalFlashConfigEfr32_t*) flash->palConfig;
    gmosPalFlashStateEfr32_t* palData =
        (gmosPalFlashStateEfr32_t*) flash->palData;

    // Set the erase address to the start of flash memory.
    palData->bulkEraseAddr = palConfig->baseAddress;

    // Run the flash memory erase task.
    gmosSchedulerTaskResume (&(palData->bulkEraseTask));
    return true;
}

/*
 * Implements the platform specific initialisation function to be used
 * for the on-chip flash memory.
 */
bool gmosPalFlashInitEfr32 (gmosDriverFlash_t* flash)
{
    gmosPalFlashConfigEfr32_t* palConfig =
        (gmosPalFlashConfigEfr32_t*) flash->palConfig;
    gmosPalFlashStateEfr32_t* palData =
        (gmosPalFlashStateEfr32_t*) flash->palData;
    uint32_t addrMask;
    bool initOk = true;

    // Check for valid base address and flash memory size alignment.
    // TODO: Check against device specific flash memory address range.
    addrMask = EFR32_FLASH_BLOCK_SIZE - 1;
    if (((palConfig->baseAddress & addrMask) != 0) ||
        ((palConfig->memorySize & addrMask) != 0)) {
        initOk = false;
        goto out;
    }

    // Populate the common driver fields.
    flash->palWriteEnable = gmosPalFlashWriteEnableEfr32;
    flash->palRead = gmosPalFlashReadEfr32;
    flash->palWrite = gmosPalFlashWriteEfr32;
    flash->palErase = gmosPalFlashEraseEfr32;
    flash->palEraseAll = gmosPalFlashEraseAllEfr32;
    flash->blockSize = EFR32_FLASH_BLOCK_SIZE;
    flash->blockCount = palConfig->memorySize / EFR32_FLASH_BLOCK_SIZE;
    flash->readSize = EFR32_FLASH_READ_SIZE;
    flash->writeSize = EFR32_FLASH_WRITE_SIZE;
    flash->flashState = GMOS_DRIVER_FLASH_STATE_IDLE;

    // Initialise the platform specific fields.
    palData->bulkEraseAddr = EFR32_FLASH_ERASE_STOP;
    gmosPalFlashEraseAll_start (
        &(palData->bulkEraseTask), flash, "EFR32 flash erase");
out:
    return initOk;
}
