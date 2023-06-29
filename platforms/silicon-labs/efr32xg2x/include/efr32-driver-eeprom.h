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
 * This header provides a GubbinsMOS EEPROM driver wrapper for the
 * platform specific EFR32xG2x NVM3 non-volatile memory library.
 */

#ifndef EFR32_DRIVER_EEPROM_H
#define EFR32_DRIVER_EEPROM_H

#include "gmos-config.h"

// Only use the platform specific definitions if required.
#if GMOS_CONFIG_EEPROM_PLATFORM_LIBRARY
#include "gmos-driver-eeprom.h"

/**
 * Defines the GubbinsMOS EEPROM driver state data structure that is
 * used for managing the platform specific EEPROM driver implementation.
 * Note that since this does not require a platform abstraction layer,
 * the GMOS_DRIVER_EEPROM_PAL_CONFIG macro should not be used during
 * allocation.
 */
typedef struct gmosDriverEeprom_t {

    // Hold a pointer to the associated platform NVM instance.
    void* platformNvm;

    // This is a pointer to the current record data used during write
    // transactions.
    uint8_t* writeData;

    // This is the callback handler to be used on completion of the
    // current transaction.
    gmosPalEepromCallback_t callbackHandler;

    // This is the opaque data item that will be passed back as the
    // callback handler parameter.
    void* callbackData;

    // This is the EEPROM driver worker task that implements the EEPROM
    // access state machine.
    gmosTaskState_t workerTask;

    // This is the most recent EEPROM transaction status.
    gmosDriverEepromStatus_t eepromStatus;

    // This is the current EEPROM transaction size.
    size_t recordSize;

    // This is the current EEPROM transaction tag.
    uint32_t recordTag;

    // This is the current EEPROM driver state.
    uint8_t eepromState;

} gmosDriverEeprom_t;

#endif // GMOS_CONFIG_EEPROM_PLATFORM_LIBRARY
#endif // EFR32_DRIVER_EEPROM_H
