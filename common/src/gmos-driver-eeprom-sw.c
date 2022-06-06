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
 * This file implements a software emulated EEPROM. The software
 * emulation uses RAM data storage, so stored data will not be persisted
 * over system resets. This will typically be used for development
 * purposes only.
 */

#include <stdint.h>
#include <stdbool.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-driver-eeprom.h"

// Use EEPROM software implementation instead of dedicated hardware.
#if GMOS_CONFIG_EEPROM_SOFTWARE_EMULATION

/*
 * Initialises the EEPROM driver platform abstraction layer. This will
 * be called once on startup in order to initialise the platform
 * specific EEPROM driver state.
 */
bool gmosPalEepromInit (gmosDriverEeprom_t* eeprom)
{
    uint32_t resetData;
    uint8_t* dstPtr;
    uint8_t i;

    // Copy the configuration settings to the main data structure.
    eeprom->baseAddress = eeprom->palConfig->memAddress;
    eeprom->memSize = eeprom->palConfig->memSize;

    // Since RAM state is not persisted, the emulated EEPROM is always
    // placed in its factory reset state on startup.
    resetData = GMOS_DRIVER_EEPROM_TAG_END_MARKER;
    dstPtr = eeprom->baseAddress;
    for (i = 0; i < GMOS_CONFIG_EEPROM_TAG_SIZE; i++) {
        *(dstPtr++) = (uint8_t) resetData;
        resetData >>= 8;
    }
    for (i = 0; i < GMOS_CONFIG_EEPROM_LENGTH_SIZE; i++) {
        *(dstPtr++) = 0;
    }
    return true;
}

/*
 * Initiates a write operation for the EEPROM platform abstraction
 * layer, using the specified address offset within the EEPROM.
 */
bool gmosPalEepromWriteData (gmosDriverEeprom_t* eeprom,
    uint16_t addrOffset, const uint8_t* writeData, uint16_t writeSize)
{
    const uint8_t* srcPtr;
    uint8_t* dstPtr;
    uint16_t i;

    // Check for valid address range.
    if (addrOffset + writeSize > eeprom->memSize) {
        return false;
    }

    // Implement clear to zero.
    if (writeData == NULL) {
        dstPtr = eeprom->baseAddress + addrOffset;
        for (i = 0; i < writeSize; i++) {
            *(dstPtr++) = 0;
        }
    }

    // Implement byte based copy.
    else {
        srcPtr = writeData;
        dstPtr = eeprom->baseAddress + addrOffset;
        for (i = 0; i < writeSize; i++) {
            *(dstPtr++) = *(srcPtr++);
        }
    }
    return true;
}

/*
 * Polls the EEPROM platform abstraction layer to determine if an EEPROM
 * write transaction is currently in progress.
 */
bool gmosPalEepromWritePoll (gmosDriverEeprom_t* eeprom)
{
    return false;
}

#endif // GMOS_CONFIG_EEPROM_SOFTWARE_EMULATION
