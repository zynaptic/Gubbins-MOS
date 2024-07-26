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
 * This file implements the common components of the GubbinsMOS flash
 * memory driver.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-scheduler.h"
#include "gmos-events.h"
#include "gmos-driver-flash.h"

/*
 * Initialises a flash memory driver on startup.
 */
bool gmosDriverFlashInit (gmosDriverFlash_t* flash,
    gmosTaskState_t* clientTask)
{
    // Initialise the completion event data structure.
    gmosEventInit (&(flash->completionEvent), clientTask);

    // Run the platform specific initialisation. The flash driver state
    // remains in 'reset' until any device specific setup is complete.
    return flash->palInit (flash);
}

/*
 * Sets the flash memory device write enable status.
 */
bool gmosDriverFlashWriteEnable (gmosDriverFlash_t* flash,
    bool writeEnable)
{
    uint32_t eventBits = 0;
    bool started;

    // Indicate a persistent driver error.
    if (flash->flashState == GMOS_DRIVER_FLASH_STATE_ERROR) {
        eventBits = GMOS_DRIVER_FLASH_EVENT_COMPLETION_FLAG |
            GMOS_DRIVER_FLASH_STATUS_DRIVER_ERROR;
        started = true;
    }

    // Wait until the flash memory is ready for access.
    else if (flash->flashState != GMOS_DRIVER_FLASH_STATE_IDLE) {
        started = false;
    }

    // Check for no change to the write enable status.
    else if ((writeEnable && (flash->writeEnable != 0)) ||
        (!writeEnable && (flash->writeEnable == 0))) {
        eventBits = GMOS_DRIVER_FLASH_EVENT_COMPLETION_FLAG |
            GMOS_DRIVER_FLASH_STATUS_SUCCESS;
        started = true;
    }

    // Issue the platform abstraction layer write enable request.
    // Note that the platform abstraction layer is responsible for
    // notifying write enable status changes on successful completion.
    else {
        started = flash->palWriteEnable (flash, writeEnable);
    }

    // Indicate completion if required.
    if (eventBits != 0) {
        gmosEventAssignBits (&(flash->completionEvent), eventBits);
    }
    if (started) {
        flash->flashState = GMOS_DRIVER_FLASH_STATE_ACTIVE;
    }
    return started;
}

/*
 * Initiates an asynchronous flash device read request.
 */
bool gmosDriverFlashRead (gmosDriverFlash_t* flash,
    uint32_t readAddr, uint8_t* readData, uint16_t readSize)
{
    uint32_t addrMask = flash->readSize - 1;
    uint32_t addrLimit = flash->blockSize * flash->blockCount;
    uint32_t eventBits = 0;
    bool started;

    // Indicate a persistent driver error.
    if (flash->flashState == GMOS_DRIVER_FLASH_STATE_ERROR) {
        eventBits = GMOS_DRIVER_FLASH_EVENT_COMPLETION_FLAG |
            GMOS_DRIVER_FLASH_STATUS_DRIVER_ERROR;
        started = true;
    }

    // Wait until the flash memory is ready for access.
    else if (flash->flashState != GMOS_DRIVER_FLASH_STATE_IDLE) {
        started = false;
    }

    // Check for valid address and size alignment.
    else if (((readAddr + readSize) > addrLimit) ||
        ((readAddr & addrMask) != 0) || ((readSize & addrMask) != 0)) {
        eventBits = GMOS_DRIVER_FLASH_EVENT_COMPLETION_FLAG |
            GMOS_DRIVER_FLASH_STATUS_CALLER_ERROR;
        started = true;
    }

    // Issue the platform abstraction layer read request.
    else {
        started = flash->palRead (flash, readAddr, readData, readSize);
    }

    // Indicate error condition if required.
    if (eventBits != 0) {
        gmosEventAssignBits (&(flash->completionEvent), eventBits);
    }
    if (started) {
        flash->flashState = GMOS_DRIVER_FLASH_STATE_ACTIVE;
    }
    return started;
}

/*
 * Initiates an asynchronous flash device write request.
 */
bool gmosDriverFlashWrite (gmosDriverFlash_t* flash,
    uint32_t writeAddr, uint8_t* writeData, uint16_t writeSize)
{
    uint32_t addrMask = flash->writeSize - 1;
    uint32_t addrLimit = flash->blockSize * flash->blockCount;
    uint32_t eventBits = 0;
    bool started;

    // Indicate a persistent driver error.
    if (flash->flashState == GMOS_DRIVER_FLASH_STATE_ERROR) {
        eventBits = GMOS_DRIVER_FLASH_EVENT_COMPLETION_FLAG |
            GMOS_DRIVER_FLASH_STATUS_DRIVER_ERROR;
        started = true;
    }

    // Wait until the flash memory is ready for access.
    else if (flash->flashState != GMOS_DRIVER_FLASH_STATE_IDLE) {
        started = false;
    }

    // Check for valid address and size alignment.
    else if (((writeAddr + writeSize) > addrLimit) ||
        ((writeAddr & addrMask) != 0) || ((writeSize & addrMask) != 0)) {
        eventBits = GMOS_DRIVER_FLASH_EVENT_COMPLETION_FLAG |
            GMOS_DRIVER_FLASH_STATUS_CALLER_ERROR;
        started = true;
    }

    // Check the write enable status.
    else if (flash->writeEnable == 0) {
        eventBits = GMOS_DRIVER_FLASH_EVENT_COMPLETION_FLAG |
            GMOS_DRIVER_FLASH_STATUS_WRITE_LOCKED;
        started = true;
    }

    // Issue the platform abstraction layer write request.
    else {
        started = flash->palWrite (flash, writeAddr, writeData, writeSize);
    }

    // Indicate error condition if required.
    if (eventBits != 0) {
        gmosEventAssignBits (&(flash->completionEvent), eventBits);
    }
    if (started) {
        flash->flashState = GMOS_DRIVER_FLASH_STATE_ACTIVE;
    }
    return started;
}

/*
 * Initiates an asynchronous flash device block erase request.
 */
bool gmosDriverFlashErase (gmosDriverFlash_t* flash,
    uint32_t eraseAddr)
{
    uint32_t addrMask = flash->blockSize - 1;
    uint32_t addrLimit = flash->blockSize * flash->blockCount;
    uint32_t eventBits = 0;
    bool started;

    // Indicate a persistent driver error.
    if (flash->flashState == GMOS_DRIVER_FLASH_STATE_ERROR) {
        eventBits = GMOS_DRIVER_FLASH_EVENT_COMPLETION_FLAG |
            GMOS_DRIVER_FLASH_STATUS_DRIVER_ERROR;
        started = true;
    }

    // Wait until the flash memory is ready for access.
    else if (flash->flashState != GMOS_DRIVER_FLASH_STATE_IDLE) {
        started = false;
    }

    // Check for valid address and size alignment.
    else if ((eraseAddr >= addrLimit) || ((eraseAddr & addrMask) != 0)) {
        eventBits = GMOS_DRIVER_FLASH_EVENT_COMPLETION_FLAG |
            GMOS_DRIVER_FLASH_STATUS_CALLER_ERROR;
        started = true;
    }

    // Check the write enable status.
    else if (flash->writeEnable == 0) {
        eventBits = GMOS_DRIVER_FLASH_EVENT_COMPLETION_FLAG |
            GMOS_DRIVER_FLASH_STATUS_WRITE_LOCKED;
        started = true;
    }

    // Issue the platform abstraction layer erase request.
    else {
        started = flash->palErase (flash, eraseAddr);
    }

    // Indicate error condition if required.
    if (eventBits != 0) {
        gmosEventAssignBits (&(flash->completionEvent), eventBits);
    }
    if (started) {
        flash->flashState = GMOS_DRIVER_FLASH_STATE_ACTIVE;
    }
    return started;
}

/*
 * Initiates an asynchronous flash device bulk erase request.
 */
bool gmosDriverFlashEraseAll (gmosDriverFlash_t* flash)
{
    uint32_t eventBits = 0;
    bool started;

    // Indicate a persistent driver error.
    if (flash->flashState == GMOS_DRIVER_FLASH_STATE_ERROR) {
        eventBits = GMOS_DRIVER_FLASH_EVENT_COMPLETION_FLAG |
            GMOS_DRIVER_FLASH_STATUS_DRIVER_ERROR;
        started = true;
    }

    // Wait until the flash memory is ready for access.
    else if (flash->flashState != GMOS_DRIVER_FLASH_STATE_IDLE) {
        started = false;
    }

    // Check the write enable status.
    else if (flash->writeEnable == 0) {
        eventBits = GMOS_DRIVER_FLASH_EVENT_COMPLETION_FLAG |
            GMOS_DRIVER_FLASH_STATUS_WRITE_LOCKED;
        started = true;
    }

    // Issue the platform abstraction layer erase request.
    else {
        started = flash->palEraseAll (flash);
    }

    // Indicate error condition if required.
    if (eventBits != 0) {
        gmosEventAssignBits (&(flash->completionEvent), eventBits);
    }
    if (started) {
        flash->flashState = GMOS_DRIVER_FLASH_STATE_ACTIVE;
    }
    return started;
}

/*
 * Completes an asynchronous flash memory transaction.
 */
gmosDriverFlashStatus_t gmosDriverFlashComplete
    (gmosDriverFlash_t* flash, uint16_t* transferSize)
{
    uint32_t eventBits;
    gmosEvent_t* completionEvent;
    gmosDriverFlashStatus_t flashStatus = GMOS_DRIVER_FLASH_STATUS_IDLE;

    // Only poll the completion event if a transaction is active.
    if (flash->flashState == GMOS_DRIVER_FLASH_STATE_ACTIVE) {
        completionEvent = &(flash->completionEvent);
        eventBits = gmosEventResetBits (completionEvent);
        if (eventBits != 0) {
            flashStatus = eventBits & GMOS_DRIVER_FLASH_EVENT_STATUS_MASK;

            // Enter the error state on a driver error condition.
            if (flashStatus == GMOS_DRIVER_FLASH_STATUS_DRIVER_ERROR) {
                flash->flashState = GMOS_DRIVER_FLASH_STATE_ERROR;
            } else {
                flash->flashState = GMOS_DRIVER_FLASH_STATE_IDLE;
            }

            // Transfer size notifications are optional.
            if (transferSize != NULL) {
                *transferSize =
                    (eventBits & GMOS_DRIVER_FLASH_EVENT_SIZE_MASK) >>
                    GMOS_DRIVER_FLASH_EVENT_SIZE_OFFSET;
            }

            // Set or clear the write enabled status if required.
            if (eventBits & GMOS_DRIVER_FLASH_EVENT_WRITE_ENABLED_FLAG) {
                flash->writeEnable = 0x01;
            }
            if (eventBits & GMOS_DRIVER_FLASH_EVENT_WRITE_DISABLED_FLAG) {
                flash->writeEnable = 0x00;
            }
        } else {
            flashStatus = GMOS_DRIVER_FLASH_STATUS_ACTIVE;
        }
    }
    return flashStatus;
}
