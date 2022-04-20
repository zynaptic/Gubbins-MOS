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
 * Implements EEPROM driver functionality for the STM32L0XX series of
 * microcontrollers.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-platform.h"
#include "gmos-driver-eeprom.h"
#include "stm32-device.h"
#include "stm32-driver-eeprom.h"

/*
 * Unlock the EEPROM for write accesses.
 */
static inline void gmosPalEepromWriteUnlock (gmosDriverEeprom_t* eeprom)
{
    // Wait for any outstanding NVM operations to complete.
    while ((FLASH->SR & FLASH_SR_BSY) != 0) {};

    // Write the EEPROM unlock key values if not already unlocked.
    if ((FLASH->PECR & FLASH_PECR_PELOCK) != 0) {
        FLASH->PEKEYR = 0x89ABCDEF;
        FLASH->PEKEYR = 0x02030405;
    }
}

/*
 * Lock the EEPROM, preventing further write accesses.
 */
static inline void gmosPalEepromWriteLock (gmosDriverEeprom_t* eeprom)
{
    // Wait for any outstanding NVM operations to complete.
    while ((FLASH->SR & FLASH_SR_BSY) != 0) {};

    // Set the EEPROM lock bit.
    FLASH->PECR |= FLASH_PECR_PELOCK;
}

/*
 * Write the next byte or word to the EEPROM.
 */
static void gmosPalEepromWriteNextData (gmosDriverEeprom_t* eeprom)
{
    gmosPalEepromState_t* palData = eeprom->palData;
    const uint8_t* dataPtr = palData->writeData;

    // Skip if a write is already in progress.
    if ((FLASH->SR & FLASH_SR_BSY) != 0) {
        return;
    }

    // Process full word writes.
    else if ((palData->writeCount >= 4) &&
        (palData->addrOffset & 0x03) == 0) {
        uint32_t writeDataValue;
        uint32_t* writeAddr = (uint32_t*)
            (eeprom->baseAddress + palData->addrOffset);
        if (dataPtr == NULL) {
            writeDataValue = 0;
        } else {
            writeDataValue = (uint32_t) *(dataPtr++);
            writeDataValue |= ((uint32_t) *(dataPtr++)) << 8;
            writeDataValue |= ((uint32_t) *(dataPtr++)) << 16;
            writeDataValue |= ((uint32_t) *(dataPtr++)) << 24;
        }
        palData->writeData += 4;
        palData->writeCount -= 4;
        palData->addrOffset += 4;
        *writeAddr = writeDataValue;
    }

    // Process half word writes.
    else if ((palData->writeCount >= 2) &&
        (palData->addrOffset & 0x01) == 0) {
        uint16_t writeDataValue;
        uint16_t* writeAddr = (uint16_t*)
            (eeprom->baseAddress + palData->addrOffset);
        if (dataPtr == NULL) {
            writeDataValue = 0;
        } else {
            writeDataValue = (uint16_t) *(dataPtr++);
            writeDataValue |= ((uint16_t) *(dataPtr++)) << 8;
        }
        palData->writeData += 2;
        palData->writeCount -= 2;
        palData->addrOffset += 2;
        *writeAddr = writeDataValue;
    }

    // Process single byte writes.
    else if (palData->writeCount >= 1) {
        uint8_t writeDataValue;
        uint8_t* writeAddr = (uint8_t*)
            (eeprom->baseAddress + palData->addrOffset);
        if (dataPtr == NULL) {
            writeDataValue = 0;
        } else {
            writeDataValue = (uint8_t) *(dataPtr++);
        }
        palData->writeData += 1;
        palData->writeCount -= 1;
        palData->addrOffset += 1;
        *writeAddr = writeDataValue;
    }
}

/*
 * Initialise the platform abstraction layer for the EEPROM driver.
 */
bool gmosPalEepromInit (gmosDriverEeprom_t* eeprom)
{
    eeprom->baseAddress = (uint8_t*) DATA_EEPROM_BASE;
    eeprom->memSize = STM32_DRIVER_EEPROM_SIZE;
    eeprom->palData->writeData = NULL;
    eeprom->palData->writeCount = 0xFFFF;
    return true;
}

/*
 * Initiates a write operation for the EEPROM platform abstraction
 * layer, using the specified address offset within the EEPROM.
 */
bool gmosPalEepromWriteData (gmosDriverEeprom_t* eeprom,
    uint16_t addrOffset, const uint8_t* writeData, uint16_t writeSize)
{
    gmosPalEepromState_t* palData = eeprom->palData;

    // Check for a write operation already in progress.
    if (palData->writeData != NULL) {
        return false;
    }

    // Enable EEPROM writes.
    gmosPalEepromWriteUnlock (eeprom);

    // Set up the EEPROM write state.
    palData->writeData = writeData;
    palData->writeCount = writeSize;
    palData->addrOffset = addrOffset;

    // Initiate the first EEPROM write request.
    gmosPalEepromWriteNextData (eeprom);
    return true;
}

/*
 * Polls the EEPROM platform abstraction layer to determine if an EEPROM
 * write transaction is currently in progress. It should be called
 * periodically while a write transaction is active in order to progress
 * the write operation.
 */
bool gmosPalEepromWritePoll (gmosDriverEeprom_t* eeprom)
{
    gmosPalEepromState_t* palData = eeprom->palData;
    bool writeActive;

    // Check for no write operation in progress. This is indicated by a
    // write count value of 0xFFFF.
    if (palData->writeCount == 0xFFFF) {
        GMOS_LOG (LOG_VERBOSE, "STM32 EEPROM poll state IDLE.");
        writeActive = false;
    }

    // Check for low level EEPROM write in progress.
    else if ((FLASH->SR & FLASH_SR_BSY) != 0) {
        GMOS_LOG (LOG_VERBOSE, "STM32 EEPROM poll state BUSY.");
        writeActive = true;
    }

    // Check for final write completion and lock the EEPROM.
    else if (palData->writeCount == 0) {
        GMOS_LOG (LOG_VERBOSE, "STM32 EEPROM poll state COMPLETE.");
        gmosPalEepromWriteLock (eeprom);
        palData->writeData = NULL;
        palData->writeCount = 0xFFFF;
        writeActive = false;
    }

    // Start the next low level EEPROM write cycle.
    else {
        GMOS_LOG (LOG_VERBOSE, "STM32 EEPROM poll state NEXT.");
        gmosPalEepromWriteNextData (eeprom);
        writeActive = true;
    }
    return writeActive;
}
