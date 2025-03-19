/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2024-2025 Zynaptic Limited
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
 * This header defines the data structures and management functions for
 * generic SPI flash devices which support the JEDEC Serial Flash
 * Discoverable Parameter (SFDP) standard.
 */

#ifndef GMOS_DRIVER_FLASH_SFDP_H
#define GMOS_DRIVER_FLASH_SFDP_H

#include <stdint.h>
#include <stdbool.h>

#include "gmos-scheduler.h"
#include "gmos-driver-spi.h"
#include "gmos-driver-flash.h"

/**
 * Defines the standard reset sequence that is applicable to most
 * generic SPI flash devices. This issues the standard reset commands
 * and then waits for 8ms before further processing.
 */
#define GMOS_DRIVER_FLASH_SFDP_STANDARD_RESET_COMMANDS { \
    0x01, 0x66, 0x01, 0x99, 0x88, 0x00 }

/**
 * Defines the extended reset sequence for use with Microchip SST26XX
 * series devices that support software configurable block protection.
 * These devices default to being write protected after a power on reset
 * and require an additional global block protection unlock command to
 * be issued on startup.
 */
#define GMOS_DRIVER_FLASH_SFDP_SST26XX_RESET_COMMANDS { \
    0x01, 0x66, 0x01, 0x99, 0x88, 0x01, 0x06, 0x01, 0x98, 0x88, 0x00 }

/**
 * Defines the generic SFDP flash memory configuration settings data
 * structure to be used for the SPI flash memory device.
 */
typedef struct gmosDriverFlashConfigSfdp_t {

    // Specify a pointer to a list of SPI commands that will be executed
    // on reset. Commands are encoded as the command length followed by
    // the required number of command bytes, with a zero length command
    // being used as a terminator. Inter-command delays are encoded by
    // setting the most significant bit of the byte and using the least
    // significant bits to represent the delay in milliseconds. A null
    // reference may be used to indicate that no reset sequence is to be
    // used.
    uint8_t* resetCommands;

    // Specify the SPI bus instance to use for communicating with the
    // SPI flash device.
    gmosDriverSpiBus_t* spiInterface;

    // Specify the SPI chip selection pin to use for selecting the SPI
    // flash device.
    uint16_t spiChipSelect;

    // Specify the maximum supported SPI clock rate for the device,
    // expressed as an integer multiple of 1kHz.
    uint16_t spiClockRate;

    // Specify the SPI clock mode to be used for the device using the
    // SPI clock mode enumeration.
    uint8_t spiClockMode;

} gmosDriverFlashConfigSfdp_t;

/**
 * Defines the generic SFDP flash memory dynamic data structure to be
 * used for the SPI flash memory device.
 */
typedef struct gmosDriverFlashStateSfdp_t {

    // Allocate the main task data structure.
    gmosTaskState_t spiFlashTask;

    // Specify the SPI device data structure to be used for accessing
    // the SPI flash device.
    gmosDriverSpiDevice_t spiDevice;

    // Specify the programming page size.
    uint16_t progPageSize;

    // Specify the current operating phase for the SPI flash device.
    uint8_t spiPhase;

    // Specify the current operating state for the SPI flash device.
    uint8_t spiState;

    // Specify the 4K sector erase command used by the device.
    uint8_t cmdSectorErase;

    // Specify the number of address bytes to use for data access.
    uint8_t addressSize;

    // Define operating phase specific storage.
    union {

        // Specify startup phase data.
        struct {
            uint32_t paramBlockAddr;
            uint16_t paramBlockId;
            uint8_t  paramBlockSize;
            uint8_t  paramHeaderNum;
            uint8_t  index;
        } startup;

        // Specify erase phase data.
        struct {
            uint32_t sectorAddr;
        } erase;

        // Specify read phase data.
        struct {
            uint32_t flashAddr;
            uint8_t* dataPtr;
            uint16_t dataSize;
        } read;

        // Specify the write phase data.
        struct {
            uint32_t flashAddr;
            uint8_t* dataPtr;
            uint16_t dataSize;
            uint16_t pageDataSize;
        } write;
    } phase;

} gmosDriverFlashStateSfdp_t;

/**
 * Defines the generic SFDP flash memory initialisation function to be
 * used for the SPI flash memory device.
 * @param flash This is the flash memory device data structure that is
 *     to be initialised.
 * @return Returns a boolean value which will be set to 'true' on
 *     successful initialisation and 'false' otherwise.
 */
bool gmosDriverFlashInitSfdp (gmosDriverFlash_t* flash);

#endif // GMOS_DRIVER_FLASH_SFDP_H
