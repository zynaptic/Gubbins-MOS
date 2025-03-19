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
 * This file implements the GubbinsMOS driver for generic SPI flash
 * devices which support the Serial Flash Discoverable Parameter (SFDP)
 * standard.
 */

#include <stdint.h>
#include <stdbool.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-scheduler.h"
#include "gmos-events.h"
#include "gmos-driver-spi.h"
#include "gmos-driver-flash.h"
#include "gmos-driver-flash-sfdp.h"

/*
 * Define the set of SPI flash operating phases.
 */
typedef enum {
    GMOS_SPI_FLASH_TASK_PHASE_FAILED,
    GMOS_SPI_FLASH_TASK_PHASE_INIT,
    GMOS_SPI_FLASH_TASK_PHASE_IDLE,
    GMOS_SPI_FLASH_TASK_PHASE_READ,
    GMOS_SPI_FLASH_TASK_PHASE_WRITE,
    GMOS_SPI_FLASH_TASK_PHASE_ERASE
} gmosSpiFlashTaskPhase_t;

/*
 * Define the state space for the initialisation state machine.
 */
typedef enum {
    GMOS_SPI_FLASH_TASK_STATE_INIT_IDLE,
    GMOS_SPI_FLASH_TASK_STATE_INIT_RESET,
    GMOS_SPI_FLASH_TASK_STATE_INIT_START,
    GMOS_SPI_FLASH_TASK_STATE_INIT_SFDP_PH_READ,
    GMOS_SPI_FLASH_TASK_STATE_INIT_SFDP_PH_CHECK,
    GMOS_SPI_FLASH_TASK_STATE_INIT_BASIC_READ_A,
    GMOS_SPI_FLASH_TASK_STATE_INIT_BASIC_READ_B,
    GMOS_SPI_FLASH_TASK_STATE_INIT_COMPLETE,
    GMOS_SPI_FLASH_TASK_STATE_INIT_FAILED
} gmosSpiFlashTaskStateInit_t;

/*
 * Define the state space for the read request state machine.
 */
typedef enum {
    GMOS_SPI_FLASH_TASK_STATE_READ_IDLE,
    GMOS_SPI_FLASH_TASK_STATE_READ_START,
    GMOS_SPI_FLASH_TASK_STATE_READ_SYNC_REQ,
    GMOS_SPI_FLASH_TASK_STATE_READ_ASYNC_REQ,
    GMOS_SPI_FLASH_TASK_STATE_READ_ASYNC_POLL,
    GMOS_SPI_FLASH_TASK_STATE_READ_COMPLETE,
    GMOS_SPI_FLASH_TASK_STATE_READ_FAILED
} gmosSpiFlashTaskStateRead_t;

/*
 * Define the state space for the write request state machine.
 */
typedef enum {
    GMOS_SPI_FLASH_TASK_STATE_WRITE_IDLE,
    GMOS_SPI_FLASH_TASK_STATE_WRITE_START,
    GMOS_SPI_FLASH_TASK_STATE_WRITE_COMMAND,
    GMOS_SPI_FLASH_TASK_STATE_WRITE_SYNC_REQ,
    GMOS_SPI_FLASH_TASK_STATE_WRITE_ASYNC_REQ,
    GMOS_SPI_FLASH_TASK_STATE_WRITE_ASYNC_POLL,
    GMOS_SPI_FLASH_TASK_STATE_WRITE_POLL_STATUS,
    GMOS_SPI_FLASH_TASK_STATE_WRITE_COMPLETE,
    GMOS_SPI_FLASH_TASK_STATE_WRITE_FAILED
} gmosSpiFlashTaskStateWrite_t;

/*
 * Define the state space for the erase request state machine.
 */
typedef enum {
    GMOS_SPI_FLASH_TASK_STATE_ERASE_IDLE,
    GMOS_SPI_FLASH_TASK_STATE_ERASE_SECTOR,
    GMOS_SPI_FLASH_TASK_STATE_ERASE_SECTOR_REQ,
    GMOS_SPI_FLASH_TASK_STATE_ERASE_ALL,
    GMOS_SPI_FLASH_TASK_STATE_ERASE_ALL_REQ,
    GMOS_SPI_FLASH_TASK_STATE_ERASE_POLL_STATUS,
    GMOS_SPI_FLASH_TASK_STATE_ERASE_COMPLETE,
    GMOS_SPI_FLASH_TASK_STATE_ERASE_FAILED
} gmosSpiFlashTaskStateErase_t;

// Specify the state machine 'tick' interval.
#define GMOS_SPI_FLASH_TICK_INTERVAL GMOS_MS_TO_TICKS (10)

// Specify the erase status polling interval. In future this could be
// derived from the SFDP data, but a 5ms polling interval will be
// suitable for most use cases.
#define GMOS_SPI_FLASH_ERASE_POLL_INTERVAL GMOS_MS_TO_TICKS (5)

// Specify the programming status polling interval. In future this could
// be derived from the SFDP data, but a 1ms polling interval will be
// suitable for most use cases.
#define GMOS_SPI_FLASH_WRITE_POLL_INTERVAL GMOS_MS_TO_TICKS (1)

/*
 * Implement the SPI flash write enable request as a blocking I/O
 * operation. This is required prior to any erase or page write
 * requests, since the write enable flag is always reset on completion.
 */
static bool gmosDriverFlashSfdpSetWriteEnableLatch (
    gmosDriverFlash_t* flash, gmosDriverSpiStatus_t* spiStatus)
{
    gmosDriverFlashConfigSfdp_t* palConfig =
        (gmosDriverFlashConfigSfdp_t*) flash->palConfig;
    gmosDriverFlashStateSfdp_t* palData =
        (gmosDriverFlashStateSfdp_t*) flash->palData;
    gmosDriverSpiBus_t* spiInterface = palConfig->spiInterface;
    gmosDriverSpiDevice_t* spiDevice = &(palData->spiDevice);
    uint8_t txBuf [1] = { 0x06 };
    gmosDriverSpiStatus_t status = GMOS_DRIVER_SPI_STATUS_ACTIVE;
    bool transactionDone = false;

    // Select the device to initiate the transaction.
    if (gmosDriverSpiDeviceSelect (spiInterface, spiDevice)) {
        status = gmosDriverSpiIoInlineWrite (
            spiInterface, txBuf, sizeof (txBuf));
        gmosDriverSpiDeviceRelease (spiInterface, spiDevice);
        transactionDone = true;
    }
    *spiStatus = status;
    return transactionDone;
}

/*
 * Implement the SPI flash status register read request as a blocking
 * I/O operation in order to determine if a flash update operation is
 * currently in progress.
 */
static bool gmosDriverFlashSfdpGetWriteInProgress (
    gmosDriverFlash_t* flash, gmosDriverSpiStatus_t* spiStatus,
    bool* writeInProgress)
{
    gmosDriverFlashConfigSfdp_t* palConfig =
        (gmosDriverFlashConfigSfdp_t*) flash->palConfig;
    gmosDriverFlashStateSfdp_t* palData =
        (gmosDriverFlashStateSfdp_t*) flash->palData;
    gmosDriverSpiBus_t* spiInterface = palConfig->spiInterface;
    gmosDriverSpiDevice_t* spiDevice = &(palData->spiDevice);
    uint8_t txBuf [2] = { 0x05, 0x00 };
    uint8_t rxBuf [2];
    gmosDriverSpiStatus_t status = GMOS_DRIVER_SPI_STATUS_ACTIVE;
    bool transactionDone = false;

    // Select the device to initiate the transaction.
    if (gmosDriverSpiDeviceSelect (spiInterface, spiDevice)) {
        status = gmosDriverSpiIoInlineTransfer (
            spiInterface, txBuf, rxBuf, sizeof (rxBuf));
        gmosDriverSpiDeviceRelease (spiInterface, spiDevice);

        // Check both the write enable latch and the write in progress
        // bit of the status register.
        *writeInProgress = ((rxBuf [1] & 0x03) == 0) ? false : true;
        transactionDone = true;
    }
    *spiStatus = status;
    return transactionDone;
}

/*
 * Issue a SPI flash reset command as a blocking I/O operation.
 */
static inline bool gmosDriverFlashSfdpSendReset (
    gmosDriverFlash_t* flash, gmosDriverSpiStatus_t* spiStatus)
{
    gmosDriverFlashConfigSfdp_t* palConfig =
        (gmosDriverFlashConfigSfdp_t*) flash->palConfig;
    gmosDriverFlashStateSfdp_t* palData =
        (gmosDriverFlashStateSfdp_t*) flash->palData;
    uint8_t* resetCommands = palConfig->resetCommands;
    gmosDriverSpiBus_t* spiInterface = palConfig->spiInterface;
    gmosDriverSpiDevice_t* spiDevice = &(palData->spiDevice);
    uint8_t* txPtr;
    uint_fast8_t txSize;
    gmosDriverSpiStatus_t status = GMOS_DRIVER_SPI_STATUS_ACTIVE;
    bool transactionDone = false;

    // Select the command from the reset commands array.
    txSize = resetCommands [palData->phase.startup.index];
    txPtr = &(resetCommands [palData->phase.startup.index + 1]);

    // Select the device to initiate the transaction.
    if (gmosDriverSpiDeviceSelect (spiInterface, spiDevice)) {
        status = gmosDriverSpiIoInlineWrite (
            spiInterface, txPtr, txSize);
        gmosDriverSpiDeviceRelease (spiInterface, spiDevice);
        transactionDone = true;
    }
    *spiStatus = status;
    return transactionDone;
}

/*
 * Implement the SPI flash SFDP main header read request as a blocking
 * I/O operation.
 */
static inline bool gmosDriverFlashSfdpReadMainHeader (
    gmosDriverFlash_t* flash, gmosDriverSpiStatus_t* spiStatus)
{
    gmosDriverFlashConfigSfdp_t* palConfig =
        (gmosDriverFlashConfigSfdp_t*) flash->palConfig;
    gmosDriverFlashStateSfdp_t* palData =
        (gmosDriverFlashStateSfdp_t*) flash->palData;
    gmosDriverSpiBus_t* spiInterface = palConfig->spiInterface;
    gmosDriverSpiDevice_t* spiDevice = &(palData->spiDevice);
    uint8_t txBuf [13] = { 0x5A, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    uint8_t rxBuf [13];
    gmosDriverSpiStatus_t status = GMOS_DRIVER_SPI_STATUS_ACTIVE;
    bool transactionDone = false;

    // Select the device to initiate the transaction.
    if (gmosDriverSpiDeviceSelect (spiInterface, spiDevice)) {
        status = gmosDriverSpiIoInlineTransfer (
            spiInterface, txBuf, rxBuf, sizeof (rxBuf));
        gmosDriverSpiDeviceRelease (spiInterface, spiDevice);
        transactionDone = true;
    }
    if (status != GMOS_DRIVER_SPI_STATUS_SUCCESS) {
        goto out;
    }

    // Check for SFDP magic number.
    if ((rxBuf [5] != 'S') || (rxBuf [6] != 'F') ||
        (rxBuf [7] != 'D') || (rxBuf [8] != 'P')) {
        GMOS_LOG (LOG_ERROR, "SPI Flash SFDP : No valid SFDP header found.");
        status = GMOS_DRIVER_SPI_STATUS_DRIVER_ERROR;
        goto out;
    }

    // Current support is restricted to legacy JESD216B access devices.
    if (rxBuf [12] != 0xFF) {
        GMOS_LOG (LOG_ERROR, "SPI Flash SFDP : Legacy JESD216B access support only.");
        status = GMOS_DRIVER_SPI_STATUS_DRIVER_ERROR;
        goto out;
    }

    // Report the SFDP header version and store the number of parameter
    // headers that are available.
    GMOS_LOG_FMT (LOG_INFO, "SPI Flash SFDP : Detected SFDP v%d.%d",
        rxBuf [10], rxBuf [9]);
    palData->phase.startup.index = 0;
    palData->phase.startup.paramHeaderNum = rxBuf [11] + 1;
out:
    *spiStatus = status;
    return transactionDone;
}

/*
 * Implement the SPI flash SFDP parameter header read request as a
 * blocking I/O operation.
 */
static inline bool gmosDriverFlashSfdpReadParamHeader (
    gmosDriverFlash_t* flash, gmosDriverSpiStatus_t* spiStatus)
{
    gmosDriverFlashConfigSfdp_t* palConfig =
        (gmosDriverFlashConfigSfdp_t*) flash->palConfig;
    gmosDriverFlashStateSfdp_t* palData =
        (gmosDriverFlashStateSfdp_t*) flash->palData;
    gmosDriverSpiBus_t* spiInterface = palConfig->spiInterface;
    gmosDriverSpiDevice_t* spiDevice = &(palData->spiDevice);
    uint8_t txBuf [13] = { 0x5A, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    uint8_t rxBuf [13];
    uint32_t txAddr;
    uint32_t paramBlockAddr;
    uint_fast16_t paramBlockId;
    gmosDriverSpiStatus_t status = GMOS_DRIVER_SPI_STATUS_ACTIVE;
    bool transactionDone = false;

    // Select the offset for the parameter table read.
    txAddr = 0x08 * (1 + palData->phase.startup.index);
    txBuf [1] = 0xFF & (txAddr >> 16);
    txBuf [2] = 0xFF & (txAddr >> 8);
    txBuf [3] = 0xFF & (txAddr);

    // Select the device to initiate the transaction.
    if (gmosDriverSpiDeviceSelect (spiInterface, spiDevice)) {
        status = gmosDriverSpiIoInlineTransfer (
            spiInterface, txBuf, rxBuf, sizeof (rxBuf));
        gmosDriverSpiDeviceRelease (spiInterface, spiDevice);
        transactionDone = true;
    }
    if (status != GMOS_DRIVER_SPI_STATUS_SUCCESS) {
        goto out;
    }

    // Derive the parameter block start address.
    paramBlockAddr = (uint32_t) (rxBuf [11]);
    paramBlockAddr = (paramBlockAddr << 8) | (uint32_t) (rxBuf [10]);
    paramBlockAddr = (paramBlockAddr << 8) | (uint32_t) (rxBuf [9]);

    // Derive the parameter block ID.
    paramBlockId = (uint_fast16_t) (rxBuf [12]);
    paramBlockId = (paramBlockId << 8) | (uint_fast16_t) rxBuf [5];

    // Report and update the parameter table details.
    GMOS_LOG_FMT (LOG_INFO,
        "SPI Flash SFDP : Found SFDP table ID 0x%04X, v%d.%d (0x%06X->0x%06X)",
        paramBlockId, rxBuf [7], rxBuf [6], paramBlockAddr,
        paramBlockAddr + 4 * rxBuf [8] - 1);
    palData->phase.startup.paramBlockId = paramBlockId;
    palData->phase.startup.paramBlockSize = rxBuf [8];
    palData->phase.startup.paramBlockAddr = paramBlockAddr;
out:
    *spiStatus = status;
    return transactionDone;
}

/*
 * Implement the first SPI flash basic information read request as a
 * blocking I/O operation. This requests the first 8 bytes of the basic
 * parameter table in order to determine the size of the device and
 * confirm that standard 4K erase sectors are supported.
 */
static inline bool gmosDriverFlashSfdpReadBasicParamsA (
    gmosDriverFlash_t* flash, gmosDriverSpiStatus_t* spiStatus)
{
    gmosDriverFlashConfigSfdp_t* palConfig =
        (gmosDriverFlashConfigSfdp_t*) flash->palConfig;
    gmosDriverFlashStateSfdp_t* palData =
        (gmosDriverFlashStateSfdp_t*) flash->palData;
    gmosDriverSpiBus_t* spiInterface = palConfig->spiInterface;
    gmosDriverSpiDevice_t* spiDevice = &(palData->spiDevice);
    uint8_t txBuf [13] = { 0x5A, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    uint8_t rxBuf [13];
    uint32_t txAddr;
    uint32_t memSize;
    gmosDriverSpiStatus_t status = GMOS_DRIVER_SPI_STATUS_ACTIVE;
    bool transactionDone = false;

    // Select the offset for the parameter table read.
    txAddr = palData->phase.startup.paramBlockAddr;
    txBuf [1] = 0xFF & (txAddr >> 16);
    txBuf [2] = 0xFF & (txAddr >> 8);
    txBuf [3] = 0xFF & (txAddr);

    // Select the device to initiate the transaction.
    if (gmosDriverSpiDeviceSelect (spiInterface, spiDevice)) {
        status = gmosDriverSpiIoInlineTransfer (
            spiInterface, txBuf, rxBuf, sizeof (rxBuf));
        gmosDriverSpiDeviceRelease (spiInterface, spiDevice);
        transactionDone = true;
    }
    if (status != GMOS_DRIVER_SPI_STATUS_SUCCESS) {
        goto out;
    }

    // Check for uniform 4KByte erase sector support.
    if ((rxBuf [5] & 0x03) != 0x01) {
        GMOS_LOG (LOG_ERROR,
            "SPI Flash SFDP : Uniform 4K erase segments not supported.");
        status = GMOS_DRIVER_SPI_STATUS_DRIVER_ERROR;
        goto out;
    }
    flash->blockSize = 4096;
    palData->cmdSectorErase = rxBuf [6];
    GMOS_LOG_FMT (LOG_DEBUG,
        "SPI Flash SFDP : Sector erase command  : 0x%02X",
        palData->cmdSectorErase);

    // Determine whether fixed size 3 byte or 4 byte addresses are to be
    // used for data access.
    if ((rxBuf [7] & 0x06) == 0x00) {
        palData->addressSize = 3;
    } else if ((rxBuf [7] & 0x06) == 0x04) {
        palData->addressSize = 4;
    } else {
        GMOS_LOG (LOG_ERROR,
            "SPI Flash SFDP : Unsupported address size option.");
        status = GMOS_DRIVER_SPI_STATUS_DRIVER_ERROR;
        goto out;
    }
    GMOS_LOG_FMT (LOG_DEBUG,
        "SPI Flash SFDP : Command address size  : %d",
        palData->addressSize);

    // Determine the flash memory size. Encodings for device sizes of
    // 4GBit and over are not currently supported.
    if ((rxBuf [12] & 0x80) != 0x00) {
        GMOS_LOG (LOG_ERROR,
            "SPI Flash SFDP : High capacity devices not supported.");
        status = GMOS_DRIVER_SPI_STATUS_DRIVER_ERROR;
        goto out;
    }
    memSize = (uint32_t) rxBuf [12];
    memSize = (memSize << 8) | (uint32_t) rxBuf [11];
    memSize = (memSize << 8) | (uint32_t) rxBuf [10];
    memSize = (memSize << 8) | (uint32_t) rxBuf [9];
    memSize += 1;
    flash->blockCount = memSize / (8 * 4096);
    GMOS_LOG_FMT (LOG_DEBUG,
        "SPI Flash SFDP : Device sector count   : %d",
        flash->blockCount);
out:
    *spiStatus = status;
    return transactionDone;
}

/*
 * Implement the second SPI flash basic information read request as a
 * blocking I/O operation. This requests the flash programming parameter
 * information.
 */
static inline bool gmosDriverFlashSfdpReadBasicParamsB (
    gmosDriverFlash_t* flash, gmosDriverSpiStatus_t* spiStatus)
{
    gmosDriverFlashConfigSfdp_t* palConfig =
        (gmosDriverFlashConfigSfdp_t*) flash->palConfig;
    gmosDriverFlashStateSfdp_t* palData =
        (gmosDriverFlashStateSfdp_t*) flash->palData;
    gmosDriverSpiBus_t* spiInterface = palConfig->spiInterface;
    gmosDriverSpiDevice_t* spiDevice = &(palData->spiDevice);
    uint8_t txBuf [13] = { 0x5A, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00 };
    uint8_t rxBuf [9];
    uint32_t txAddr;
    uint_fast16_t progPageSize;
    gmosDriverSpiStatus_t status = GMOS_DRIVER_SPI_STATUS_ACTIVE;
    bool transactionDone = false;

    // Select the offset for the parameter table read.
    txAddr = palData->phase.startup.paramBlockAddr + (10 * 4);
    txBuf [1] = 0xFF & (txAddr >> 16);
    txBuf [2] = 0xFF & (txAddr >> 8);
    txBuf [3] = 0xFF & (txAddr);

    // Select the device to initiate the transaction.
    if (gmosDriverSpiDeviceSelect (spiInterface, spiDevice)) {
        status = gmosDriverSpiIoInlineTransfer (
            spiInterface, txBuf, rxBuf, sizeof (rxBuf));
        gmosDriverSpiDeviceRelease (spiInterface, spiDevice);
        transactionDone = true;
    }
    if (status != GMOS_DRIVER_SPI_STATUS_SUCCESS) {
        goto out;
    }

    // Determine the programming page size for the device.
    progPageSize = (rxBuf [5] >> 4) & 0x0F;
    progPageSize = 1L << progPageSize;
    palData->progPageSize = progPageSize;
    GMOS_LOG_FMT (LOG_DEBUG,
        "SPI Flash SFDP : Programming page size : %d",
        palData->progPageSize);
out:
    *spiStatus = status;
    return transactionDone;
}

/*
 * Implement the SPI Flash startup state machine.
 */
static inline gmosTaskStatus_t gmosDriverFlashSfdpStartup (
    gmosDriverFlash_t* flash)
{
    gmosDriverFlashConfigSfdp_t* palConfig =
        (gmosDriverFlashConfigSfdp_t*) flash->palConfig;
    gmosDriverFlashStateSfdp_t* palData =
        (gmosDriverFlashStateSfdp_t*) flash->palData;
    uint8_t* resetCommands = palConfig->resetCommands;
    uint8_t index = palData->phase.startup.index;
    gmosDriverSpiStatus_t spiStatus;
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    uint8_t nextPhase = palData->spiPhase;
    uint8_t nextState = palData->spiState;

    // Select the current SPI Flash initialisation state.
    switch (palData->spiState) {

        // Insert a short delay after reset before attempting to access
        // the device.
        case GMOS_SPI_FLASH_TASK_STATE_INIT_IDLE :
            if (resetCommands == NULL) {
                nextState = GMOS_SPI_FLASH_TASK_STATE_INIT_START;
            } else {
                nextState = GMOS_SPI_FLASH_TASK_STATE_INIT_RESET;
            }
            taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (250));
            break;

        // Issue the device specific reset sequence.
        case GMOS_SPI_FLASH_TASK_STATE_INIT_RESET :
            if (resetCommands [index] == 0) {
                nextState = GMOS_SPI_FLASH_TASK_STATE_INIT_START;
            } else if ((resetCommands [index] & 0x80) != 0) {
                palData->phase.startup.index += 1;
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (
                    resetCommands [index] & 0x7F));
            } else if (gmosDriverFlashSfdpSendReset (flash, &spiStatus)) {
                palData->phase.startup.index += 1 + resetCommands [index];
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_SPI_FLASH_TICK_INTERVAL);
            }
            break;

        // Process the initial SFDP header request.
        case GMOS_SPI_FLASH_TASK_STATE_INIT_START :
            if (gmosDriverFlashSfdpReadMainHeader (flash, &spiStatus)) {
                if (spiStatus == GMOS_DRIVER_SPI_STATUS_SUCCESS) {
                    nextState = GMOS_SPI_FLASH_TASK_STATE_INIT_SFDP_PH_READ;
                } else {
                    nextState = GMOS_SPI_FLASH_TASK_STATE_INIT_FAILED;
                }
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_SPI_FLASH_TICK_INTERVAL);
            };
            break;

        // Start processing the next parameter header.
        case GMOS_SPI_FLASH_TASK_STATE_INIT_SFDP_PH_READ :
            if (index == palData->phase.startup.paramHeaderNum) {
                nextState = GMOS_SPI_FLASH_TASK_STATE_INIT_COMPLETE;
            } else if (gmosDriverFlashSfdpReadParamHeader (flash, &spiStatus)) {
                if (spiStatus == GMOS_DRIVER_SPI_STATUS_SUCCESS) {
                    nextState = GMOS_SPI_FLASH_TASK_STATE_INIT_SFDP_PH_CHECK;
                } else {
                    nextState = GMOS_SPI_FLASH_TASK_STATE_INIT_FAILED;
                }
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_SPI_FLASH_TICK_INTERVAL);
            };
            break;

        // Check the contents of the parameter header and initiate
        // parameter block processing if required.
        case GMOS_SPI_FLASH_TASK_STATE_INIT_SFDP_PH_CHECK :
            if (palData->phase.startup.paramBlockId == 0xFF00) {
                nextState = GMOS_SPI_FLASH_TASK_STATE_INIT_BASIC_READ_A;
            } else {
                palData->phase.startup.index += 1;
                nextState = GMOS_SPI_FLASH_TASK_STATE_INIT_SFDP_PH_READ;
            }
            break;

        // Read the first set of parameters from the JEDEC basic
        // parameter block.
        case GMOS_SPI_FLASH_TASK_STATE_INIT_BASIC_READ_A :
            if (gmosDriverFlashSfdpReadBasicParamsA (flash, &spiStatus)) {
                if (spiStatus == GMOS_DRIVER_SPI_STATUS_SUCCESS) {
                    nextState = GMOS_SPI_FLASH_TASK_STATE_INIT_BASIC_READ_B;
                } else {
                    nextState = GMOS_SPI_FLASH_TASK_STATE_INIT_FAILED;
                }
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_SPI_FLASH_TICK_INTERVAL);
            }
            break;

        // Read the second set of parameters from the JEDEC basic
        // parameter block.
        case GMOS_SPI_FLASH_TASK_STATE_INIT_BASIC_READ_B :
            if (gmosDriverFlashSfdpReadBasicParamsB (flash, &spiStatus)) {
                if (spiStatus == GMOS_DRIVER_SPI_STATUS_SUCCESS) {
                    palData->phase.startup.index += 1;
                    nextState = GMOS_SPI_FLASH_TASK_STATE_INIT_SFDP_PH_READ;
                } else {
                    nextState = GMOS_SPI_FLASH_TASK_STATE_INIT_FAILED;
                }
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_SPI_FLASH_TICK_INTERVAL);
            }
            break;

        // Complete the initialisation process.
        case GMOS_SPI_FLASH_TASK_STATE_INIT_COMPLETE :
            nextPhase = GMOS_SPI_FLASH_TASK_PHASE_IDLE;
            nextState = GMOS_SPI_FLASH_TASK_STATE_INIT_IDLE;
            break;

        // Suspend further processing on failure.
        default :
            nextPhase = GMOS_SPI_FLASH_TASK_PHASE_FAILED;
            nextState = GMOS_SPI_FLASH_TASK_STATE_INIT_IDLE;
            break;
    }
    palData->spiPhase = nextPhase;
    palData->spiState = nextState;
    return taskStatus;
}

/*
 * Implement the initial read request as a blocking I/O operation. This
 * uses the fast read command and adds the required dummy cycle.
 */
static inline bool gmosDriverFlashSfdpReadRequest (
    gmosDriverFlash_t* flash, gmosDriverSpiStatus_t* spiStatus)
{
    gmosDriverFlashConfigSfdp_t* palConfig =
        (gmosDriverFlashConfigSfdp_t*) flash->palConfig;
    gmosDriverFlashStateSfdp_t* palData =
        (gmosDriverFlashStateSfdp_t*) flash->palData;
    gmosDriverSpiBus_t* spiInterface = palConfig->spiInterface;
    gmosDriverSpiDevice_t* spiDevice = &(palData->spiDevice);
    uint8_t txBuf [6];
    uint32_t txAddr;
    gmosDriverSpiStatus_t status = GMOS_DRIVER_SPI_STATUS_ACTIVE;
    bool transactionDone = false;

    // Set the fast read command byte.
    txBuf [0] = 0x0B;

    // Set the appropriate number of address bytes.
    txAddr = palData->phase.read.flashAddr;
    if (palData->addressSize == 3) {
        txBuf [1] = 0xFF & (txAddr >> 16);
        txBuf [2] = 0xFF & (txAddr >> 8);
        txBuf [3] = 0xFF & (txAddr);
        txBuf [4] = 0xFF;
    } else if (palData->addressSize == 4) {
        txBuf [1] = 0xFF & (txAddr >> 24);
        txBuf [2] = 0xFF & (txAddr >> 16);
        txBuf [3] = 0xFF & (txAddr >> 8);
        txBuf [4] = 0xFF & (txAddr);
        txBuf [5] = 0xFF;
    } else {
        status = GMOS_DRIVER_SPI_STATUS_DRIVER_ERROR;
        transactionDone = true;
        goto out;
    }

    // Select the device to initiate the transaction.
    if (gmosDriverSpiDeviceSelect (spiInterface, spiDevice)) {
        status = gmosDriverSpiIoInlineWrite (
            spiInterface, txBuf, palData->addressSize + 2);
        if (status != GMOS_DRIVER_SPI_STATUS_SUCCESS) {
            gmosDriverSpiDeviceRelease (spiInterface, spiDevice);
        }
        transactionDone = true;
    }
out:
    *spiStatus = status;
    return transactionDone;
}

/*
 * Implement the read response handling as a blocking I/O operation.
 */
static inline void gmosDriverFlashSfdpReadInlineData (
    gmosDriverFlash_t* flash, gmosDriverSpiStatus_t* spiStatus)
{
    gmosDriverFlashConfigSfdp_t* palConfig =
        (gmosDriverFlashConfigSfdp_t*) flash->palConfig;
    gmosDriverFlashStateSfdp_t* palData =
        (gmosDriverFlashStateSfdp_t*) flash->palData;
    gmosDriverSpiBus_t* spiInterface = palConfig->spiInterface;
    gmosDriverSpiDevice_t* spiDevice = &(palData->spiDevice);

    // Initiate the read response transfer request.
    *spiStatus = gmosDriverSpiIoInlineRead (spiInterface,
        palData->phase.read.dataPtr, palData->phase.read.dataSize);

    // Release the device on completion of the transaction.
    gmosDriverSpiDeviceRelease (spiInterface, spiDevice);
}

/*
 * Initiate the read response handling as a asynchronous I/O operation.
 */
static inline void gmosDriverFlashSfdpReadAsyncData (
    gmosDriverFlash_t* flash, gmosDriverSpiStatus_t* spiStatus)
{
    gmosDriverFlashConfigSfdp_t* palConfig =
        (gmosDriverFlashConfigSfdp_t*) flash->palConfig;
    gmosDriverFlashStateSfdp_t* palData =
        (gmosDriverFlashStateSfdp_t*) flash->palData;
    gmosDriverSpiBus_t* spiInterface = palConfig->spiInterface;

    // Initiate the read response transfer request.
    *spiStatus = gmosDriverSpiIoRead (spiInterface,
        palData->phase.read.dataPtr, palData->phase.read.dataSize);
}

/*
 * Complete the read response handling as an asynchronous I/O operation.
 */
static inline void gmosDriverFlashSfdpReadAsyncComplete (
    gmosDriverFlash_t* flash, gmosDriverSpiStatus_t* spiStatus)
{
    gmosDriverFlashConfigSfdp_t* palConfig =
        (gmosDriverFlashConfigSfdp_t*) flash->palConfig;
    gmosDriverFlashStateSfdp_t* palData =
        (gmosDriverFlashStateSfdp_t*) flash->palData;
    gmosDriverSpiBus_t* spiInterface = palConfig->spiInterface;
    gmosDriverSpiDevice_t* spiDevice = &(palData->spiDevice);
    gmosDriverSpiStatus_t status;

    // Poll for asynchronous transaction completion.
    status = gmosDriverSpiIoComplete (spiInterface, NULL);

    // Release the device on completion of the transaction.
    if (status != GMOS_DRIVER_SPI_STATUS_ACTIVE) {
        gmosDriverSpiDeviceRelease (spiInterface, spiDevice);
    }
    *spiStatus = status;
}

/*
 * Implement the SPI Flash read data state machine.
 */
static inline gmosTaskStatus_t gmosDriverFlashSfdpDoRead (
    gmosDriverFlash_t* flash)
{
    gmosDriverFlashStateSfdp_t* palData =
        (gmosDriverFlashStateSfdp_t*) flash->palData;
    gmosDriverSpiStatus_t spiStatus;
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    uint8_t nextPhase = palData->spiPhase;
    uint8_t nextState = palData->spiState;

    // Select the current SPI Flash read data state.
    switch (palData->spiState) {

        // Attempt to initiate the read request.
        case GMOS_SPI_FLASH_TASK_STATE_READ_START :
            if (gmosDriverFlashSfdpReadRequest (flash, &spiStatus)) {
                if (spiStatus != GMOS_DRIVER_SPI_STATUS_SUCCESS) {
                    nextState = GMOS_SPI_FLASH_TASK_STATE_READ_FAILED;
                } else if (palData->phase.read.dataSize <= 8) {
                    nextState = GMOS_SPI_FLASH_TASK_STATE_READ_SYNC_REQ;
                } else {
                    nextState = GMOS_SPI_FLASH_TASK_STATE_READ_ASYNC_REQ;
                }
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_SPI_FLASH_TICK_INTERVAL);
            }
            break;

        // Attempt to initiate an inline data transfer for small reads.
        case GMOS_SPI_FLASH_TASK_STATE_READ_SYNC_REQ :
            gmosDriverFlashSfdpReadInlineData (flash, &spiStatus);
            if (spiStatus == GMOS_DRIVER_SPI_STATUS_SUCCESS) {
                nextState = GMOS_SPI_FLASH_TASK_STATE_READ_COMPLETE;
            } else {
                nextState = GMOS_SPI_FLASH_TASK_STATE_READ_FAILED;
            }
            break;

        // Initiate an asynchronous data transfer for larger reads.
        case GMOS_SPI_FLASH_TASK_STATE_READ_ASYNC_REQ :
            gmosDriverFlashSfdpReadAsyncData (flash, &spiStatus);
            if (spiStatus == GMOS_DRIVER_SPI_STATUS_SUCCESS) {
                nextState = GMOS_SPI_FLASH_TASK_STATE_READ_ASYNC_POLL;
                taskStatus = GMOS_TASK_SUSPEND;
            } else {
                nextState = GMOS_SPI_FLASH_TASK_STATE_READ_FAILED;
            }
            break;

        // Complete an asynchronous data transfer for larger reads.
        case GMOS_SPI_FLASH_TASK_STATE_READ_ASYNC_POLL :
            gmosDriverFlashSfdpReadAsyncComplete (flash, &spiStatus);
            if (spiStatus == GMOS_DRIVER_SPI_STATUS_SUCCESS) {
                nextState = GMOS_SPI_FLASH_TASK_STATE_READ_COMPLETE;
            } else if (spiStatus == GMOS_DRIVER_SPI_STATUS_ACTIVE) {
                taskStatus = GMOS_TASK_SUSPEND;
            } else {
                nextState = GMOS_SPI_FLASH_TASK_STATE_READ_FAILED;
            }
            break;

        // Complete the SPI device read data process by signalling
        // successful completion.
        case GMOS_SPI_FLASH_TASK_STATE_READ_COMPLETE :
            gmosEventAssignBits (&(flash->completionEvent),
                GMOS_DRIVER_FLASH_EVENT_COMPLETION_FLAG |
                GMOS_DRIVER_FLASH_STATUS_SUCCESS);
            nextPhase = GMOS_SPI_FLASH_TASK_PHASE_IDLE;
            nextState = GMOS_SPI_FLASH_TASK_STATE_READ_IDLE;
            break;

        // Indicate driver error on failure.
        default :
            gmosEventAssignBits (&(flash->completionEvent),
                GMOS_DRIVER_FLASH_EVENT_COMPLETION_FLAG |
                GMOS_DRIVER_FLASH_STATUS_DRIVER_ERROR);
            nextPhase = GMOS_SPI_FLASH_TASK_PHASE_IDLE;
            nextState = GMOS_SPI_FLASH_TASK_STATE_READ_IDLE;
            break;
    }
    palData->spiPhase = nextPhase;
    palData->spiState = nextState;
    return taskStatus;
}

/*
 * Implement the initial write request as a blocking I/O operation. This
 * uses the page write command and calculates the amount of data to be
 * written to the current page.
 */
static inline bool gmosDriverFlashSfdpWriteRequest (
    gmosDriverFlash_t* flash, gmosDriverSpiStatus_t* spiStatus)
{
    gmosDriverFlashConfigSfdp_t* palConfig =
        (gmosDriverFlashConfigSfdp_t*) flash->palConfig;
    gmosDriverFlashStateSfdp_t* palData =
        (gmosDriverFlashStateSfdp_t*) flash->palData;
    gmosDriverSpiBus_t* spiInterface = palConfig->spiInterface;
    gmosDriverSpiDevice_t* spiDevice = &(palData->spiDevice);
    uint8_t txBuf [5];
    uint32_t txAddr;
    uint32_t pageOffset;
    uint_fast16_t pageDataSize;
    gmosDriverSpiStatus_t status = GMOS_DRIVER_SPI_STATUS_ACTIVE;
    bool transactionDone = false;

    // Set the page write command byte.
    txBuf [0] = 0x02;

    // Set the appropriate number of address bytes.
    txAddr = palData->phase.write.flashAddr;
    if (palData->addressSize == 3) {
        txBuf [1] = 0xFF & (txAddr >> 16);
        txBuf [2] = 0xFF & (txAddr >> 8);
        txBuf [3] = 0xFF & (txAddr);
    } else if (palData->addressSize == 4) {
        txBuf [1] = 0xFF & (txAddr >> 24);
        txBuf [2] = 0xFF & (txAddr >> 16);
        txBuf [3] = 0xFF & (txAddr >> 8);
        txBuf [4] = 0xFF & (txAddr);
    } else {
        status = GMOS_DRIVER_SPI_STATUS_DRIVER_ERROR;
        transactionDone = true;
        goto out;
    }

    // Calculate the amount of data to be written to this page. Writes
    // must be split at page boundaries for correct operation.
    pageDataSize = palData->phase.write.dataSize;
    pageOffset = txAddr & (palData->progPageSize - 1);
    if ((palData->progPageSize - pageOffset) < pageDataSize) {
        pageDataSize = palData->progPageSize - pageOffset;
    }
    palData->phase.write.pageDataSize = pageDataSize;

    // Select the device to initiate the transaction.
    if (gmosDriverSpiDeviceSelect (spiInterface, spiDevice)) {
        status = gmosDriverSpiIoInlineWrite (
            spiInterface, txBuf, palData->addressSize + 1);
        if (status != GMOS_DRIVER_SPI_STATUS_SUCCESS) {
            gmosDriverSpiDeviceRelease (spiInterface, spiDevice);
        }
        transactionDone = true;
    }
out:
    *spiStatus = status;
    return transactionDone;
}

/*
 * Implement the write data handling as a blocking I/O operation.
 */
static inline void gmosDriverFlashSfdpWriteInlineData (
    gmosDriverFlash_t* flash, gmosDriverSpiStatus_t* spiStatus)
{
    gmosDriverFlashConfigSfdp_t* palConfig =
        (gmosDriverFlashConfigSfdp_t*) flash->palConfig;
    gmosDriverFlashStateSfdp_t* palData =
        (gmosDriverFlashStateSfdp_t*) flash->palData;
    gmosDriverSpiBus_t* spiInterface = palConfig->spiInterface;
    gmosDriverSpiDevice_t* spiDevice = &(palData->spiDevice);

    // Initiate the write data transfer request.
    *spiStatus = gmosDriverSpiIoInlineWrite (spiInterface,
        palData->phase.write.dataPtr, palData->phase.write.pageDataSize);

    // Release the device on completion of the transaction.
    gmosDriverSpiDeviceRelease (spiInterface, spiDevice);
}

/*
 * Initiate the write data handling as a asynchronous I/O operation.
 */
static inline void gmosDriverFlashSfdpWriteAsyncData (
    gmosDriverFlash_t* flash, gmosDriverSpiStatus_t* spiStatus)
{
    gmosDriverFlashConfigSfdp_t* palConfig =
        (gmosDriverFlashConfigSfdp_t*) flash->palConfig;
    gmosDriverFlashStateSfdp_t* palData =
        (gmosDriverFlashStateSfdp_t*) flash->palData;
    gmosDriverSpiBus_t* spiInterface = palConfig->spiInterface;

    // Initiate the write data transfer request.
    *spiStatus = gmosDriverSpiIoWrite (spiInterface,
        palData->phase.write.dataPtr, palData->phase.write.pageDataSize);
}

/*
 * Complete the write data handling as an asynchronous I/O operation.
 */
static inline void gmosDriverFlashSfdpWriteAsyncComplete (
    gmosDriverFlash_t* flash, gmosDriverSpiStatus_t* spiStatus)
{
    gmosDriverFlashConfigSfdp_t* palConfig =
        (gmosDriverFlashConfigSfdp_t*) flash->palConfig;
    gmosDriverFlashStateSfdp_t* palData =
        (gmosDriverFlashStateSfdp_t*) flash->palData;
    gmosDriverSpiBus_t* spiInterface = palConfig->spiInterface;
    gmosDriverSpiDevice_t* spiDevice = &(palData->spiDevice);
    gmosDriverSpiStatus_t status;

    // Poll for asynchronous transaction completion.
    status = gmosDriverSpiIoComplete (spiInterface, NULL);

    // Release the device on completion of the transaction.
    if (status != GMOS_DRIVER_SPI_STATUS_ACTIVE) {
        gmosDriverSpiDeviceRelease (spiInterface, spiDevice);
    }
    *spiStatus = status;
}

/*
 * Implement the SPI Flash write data state machine.
 */
static inline gmosTaskStatus_t gmosDriverFlashSfdpDoWrite (
    gmosDriverFlash_t* flash)
{
    gmosDriverFlashStateSfdp_t* palData =
        (gmosDriverFlashStateSfdp_t*) flash->palData;
    gmosDriverSpiStatus_t spiStatus;
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    uint8_t nextPhase = palData->spiPhase;
    uint8_t nextState = palData->spiState;
    uint_fast16_t pageDataSize;
    bool writeInProgress;

    // Select the current SPI Flash write data state.
    switch (palData->spiState) {

        // Attempt to set the write enable latch for page writes.
        case GMOS_SPI_FLASH_TASK_STATE_WRITE_START :
            if (gmosDriverFlashSfdpSetWriteEnableLatch (flash, &spiStatus)) {
                if (spiStatus == GMOS_DRIVER_SPI_STATUS_SUCCESS) {
                    nextState = GMOS_SPI_FLASH_TASK_STATE_WRITE_COMMAND;
                } else {
                    nextState = GMOS_SPI_FLASH_TASK_STATE_WRITE_FAILED;
                }
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_SPI_FLASH_TICK_INTERVAL);
            }
            break;

        // Attempt to initiate the write request.
        case GMOS_SPI_FLASH_TASK_STATE_WRITE_COMMAND :
            if (gmosDriverFlashSfdpWriteRequest (flash, &spiStatus)) {
                if (spiStatus != GMOS_DRIVER_SPI_STATUS_SUCCESS) {
                    nextState = GMOS_SPI_FLASH_TASK_STATE_WRITE_FAILED;
                } else if (palData->phase.write.pageDataSize <= 8) {
                    nextState = GMOS_SPI_FLASH_TASK_STATE_WRITE_SYNC_REQ;
                } else {
                    nextState = GMOS_SPI_FLASH_TASK_STATE_WRITE_ASYNC_REQ;
                }
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_SPI_FLASH_TICK_INTERVAL);
            }
            break;

        // Attempt to initiate an inline data transfer for small writes.
        case GMOS_SPI_FLASH_TASK_STATE_WRITE_SYNC_REQ :
            gmosDriverFlashSfdpWriteInlineData (flash, &spiStatus);
            if (spiStatus == GMOS_DRIVER_SPI_STATUS_SUCCESS) {
                nextState = GMOS_SPI_FLASH_TASK_STATE_WRITE_POLL_STATUS;
                taskStatus = GMOS_TASK_RUN_LATER (
                    GMOS_SPI_FLASH_WRITE_POLL_INTERVAL);
            } else {
                nextState = GMOS_SPI_FLASH_TASK_STATE_WRITE_FAILED;
            }
            break;

        // Initiate an asynchronous data transfer for larger writes.
        case GMOS_SPI_FLASH_TASK_STATE_WRITE_ASYNC_REQ :
            gmosDriverFlashSfdpWriteAsyncData (flash, &spiStatus);
            if (spiStatus == GMOS_DRIVER_SPI_STATUS_SUCCESS) {
                nextState = GMOS_SPI_FLASH_TASK_STATE_WRITE_ASYNC_POLL;
                taskStatus = GMOS_TASK_SUSPEND;
            } else {
                nextState = GMOS_SPI_FLASH_TASK_STATE_WRITE_FAILED;
            }
            break;

        // Complete an asynchronous data transfer for larger writes.
        case GMOS_SPI_FLASH_TASK_STATE_WRITE_ASYNC_POLL :
            gmosDriverFlashSfdpWriteAsyncComplete (flash, &spiStatus);
            if (spiStatus == GMOS_DRIVER_SPI_STATUS_SUCCESS) {
                nextState = GMOS_SPI_FLASH_TASK_STATE_WRITE_POLL_STATUS;
                taskStatus = GMOS_TASK_RUN_LATER (
                    GMOS_SPI_FLASH_WRITE_POLL_INTERVAL);
            } else if (spiStatus == GMOS_DRIVER_SPI_STATUS_ACTIVE) {
                taskStatus = GMOS_TASK_SUSPEND;
            } else {
                nextState = GMOS_SPI_FLASH_TASK_STATE_WRITE_FAILED;
            }
            break;

        // Poll the SPI device status register for completion of the
        // page write request.
        case GMOS_SPI_FLASH_TASK_STATE_WRITE_POLL_STATUS :
            if (gmosDriverFlashSfdpGetWriteInProgress (flash,
                &spiStatus, &writeInProgress)) {
                if (spiStatus != GMOS_DRIVER_SPI_STATUS_SUCCESS) {
                    nextState = GMOS_SPI_FLASH_TASK_STATE_WRITE_FAILED;
                } else if (writeInProgress) {
                    taskStatus = GMOS_TASK_RUN_LATER (
                        GMOS_SPI_FLASH_WRITE_POLL_INTERVAL);
                } else {
                    nextState = GMOS_SPI_FLASH_TASK_STATE_WRITE_COMPLETE;
                }
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_SPI_FLASH_TICK_INTERVAL);
            }
            break;

        // Complete the SPI device write data process by checking for
        // further page writes then signalling successful completion.
        case GMOS_SPI_FLASH_TASK_STATE_WRITE_COMPLETE :
            pageDataSize = palData->phase.write.pageDataSize;
            if (pageDataSize >= palData->phase.write.dataSize) {
                gmosEventAssignBits (&(flash->completionEvent),
                    GMOS_DRIVER_FLASH_EVENT_COMPLETION_FLAG |
                    GMOS_DRIVER_FLASH_STATUS_SUCCESS);
                nextPhase = GMOS_SPI_FLASH_TASK_PHASE_IDLE;
                nextState = GMOS_SPI_FLASH_TASK_STATE_WRITE_IDLE;
            } else {
                palData->phase.write.flashAddr += pageDataSize;
                palData->phase.write.dataPtr += pageDataSize;
                palData->phase.write.dataSize -= pageDataSize;
                nextState = GMOS_SPI_FLASH_TASK_STATE_WRITE_START;
            }
            break;

        // Indicate driver error on failure.
        default :
            gmosEventAssignBits (&(flash->completionEvent),
                GMOS_DRIVER_FLASH_EVENT_COMPLETION_FLAG |
                GMOS_DRIVER_FLASH_STATUS_DRIVER_ERROR);
            nextPhase = GMOS_SPI_FLASH_TASK_PHASE_IDLE;
            nextState = GMOS_SPI_FLASH_TASK_STATE_WRITE_IDLE;
            break;
    }
    palData->spiPhase = nextPhase;
    palData->spiState = nextState;
    return taskStatus;
}

/*
 * Implement the sector erase request as a blocking I/O operation.
 */
static inline bool gmosDriverFlashSfdpEraseSector (
    gmosDriverFlash_t* flash, gmosDriverSpiStatus_t* spiStatus)
{
    gmosDriverFlashConfigSfdp_t* palConfig =
        (gmosDriverFlashConfigSfdp_t*) flash->palConfig;
    gmosDriverFlashStateSfdp_t* palData =
        (gmosDriverFlashStateSfdp_t*) flash->palData;
    gmosDriverSpiBus_t* spiInterface = palConfig->spiInterface;
    gmosDriverSpiDevice_t* spiDevice = &(palData->spiDevice);
    uint8_t txBuf [5];
    uint32_t txAddr;
    gmosDriverSpiStatus_t status = GMOS_DRIVER_SPI_STATUS_ACTIVE;
    bool transactionDone = false;

    // Set the erase command byte.
    txBuf [0] = palData->cmdSectorErase;

    // Set the appropriate number of address bytes.
    txAddr = palData->phase.erase.sectorAddr;
    if (palData->addressSize == 3) {
        txBuf [1] = 0xFF & (txAddr >> 16);
        txBuf [2] = 0xFF & (txAddr >> 8);
        txBuf [3] = 0xFF & (txAddr);
    } else if (palData->addressSize == 4) {
        txBuf [1] = 0xFF & (txAddr >> 24);
        txBuf [2] = 0xFF & (txAddr >> 16);
        txBuf [3] = 0xFF & (txAddr >> 8);
        txBuf [4] = 0xFF & (txAddr);
    } else {
        status = GMOS_DRIVER_SPI_STATUS_DRIVER_ERROR;
        transactionDone = true;
        goto out;
    }

    // Select the device to initiate the transaction.
    if (gmosDriverSpiDeviceSelect (spiInterface, spiDevice)) {
        status = gmosDriverSpiIoInlineWrite (
            spiInterface, txBuf, palData->addressSize + 1);
        gmosDriverSpiDeviceRelease (spiInterface, spiDevice);
        transactionDone = true;
    }
out:
    *spiStatus = status;
    return transactionDone;
}

/*
 * Implement the full device erase request as a blocking I/O operation.
 */
static inline bool gmosDriverFlashSfdpEraseAll (
    gmosDriverFlash_t* flash, gmosDriverSpiStatus_t* spiStatus)
{
    gmosDriverFlashConfigSfdp_t* palConfig =
        (gmosDriverFlashConfigSfdp_t*) flash->palConfig;
    gmosDriverFlashStateSfdp_t* palData =
        (gmosDriverFlashStateSfdp_t*) flash->palData;
    gmosDriverSpiBus_t* spiInterface = palConfig->spiInterface;
    gmosDriverSpiDevice_t* spiDevice = &(palData->spiDevice);
    uint8_t txBuf [1] = { 0xC7 };
    gmosDriverSpiStatus_t status = GMOS_DRIVER_SPI_STATUS_ACTIVE;
    bool transactionDone = false;

    // Select the device to initiate the transaction.
    if (gmosDriverSpiDeviceSelect (spiInterface, spiDevice)) {
        status = gmosDriverSpiIoInlineWrite (
            spiInterface, txBuf, sizeof (txBuf));
        gmosDriverSpiDeviceRelease (spiInterface, spiDevice);
        transactionDone = true;
    }
    *spiStatus = status;
    return transactionDone;
}

/*
 * Implement the SPI Flash startup state machine.
 */
static inline gmosTaskStatus_t gmosDriverFlashSfdpDoErase (
    gmosDriverFlash_t* flash)
{
    gmosDriverFlashStateSfdp_t* palData =
        (gmosDriverFlashStateSfdp_t*) flash->palData;
    gmosDriverSpiStatus_t spiStatus;
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    uint8_t nextPhase = palData->spiPhase;
    uint8_t nextState = palData->spiState;
    bool writeInProgress;

    // Select the current SPI Flash erase state.
    switch (palData->spiState) {

        // Attempt to set the write enable latch for sector erase.
        case GMOS_SPI_FLASH_TASK_STATE_ERASE_SECTOR :
            if (gmosDriverFlashSfdpSetWriteEnableLatch (flash, &spiStatus)) {
                if (spiStatus == GMOS_DRIVER_SPI_STATUS_SUCCESS) {
                    nextState = GMOS_SPI_FLASH_TASK_STATE_ERASE_SECTOR_REQ;
                } else {
                    nextState = GMOS_SPI_FLASH_TASK_STATE_ERASE_FAILED;
                }
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_SPI_FLASH_TICK_INTERVAL);
            }
            break;

        // Attempt to send the erase request for sector erase.
        case GMOS_SPI_FLASH_TASK_STATE_ERASE_SECTOR_REQ :
            if (gmosDriverFlashSfdpEraseSector (flash, &spiStatus)) {
                if (spiStatus == GMOS_DRIVER_SPI_STATUS_SUCCESS) {
                    nextState = GMOS_SPI_FLASH_TASK_STATE_ERASE_POLL_STATUS;
                    taskStatus = GMOS_TASK_RUN_LATER (
                        GMOS_SPI_FLASH_ERASE_POLL_INTERVAL);
                } else {
                    nextState = GMOS_SPI_FLASH_TASK_STATE_ERASE_FAILED;
                }
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_SPI_FLASH_TICK_INTERVAL);
            }
            break;

        // Attempt to set the write enable latch for chip erase.
        case GMOS_SPI_FLASH_TASK_STATE_ERASE_ALL :
            if (gmosDriverFlashSfdpSetWriteEnableLatch (flash, &spiStatus)) {
                if (spiStatus == GMOS_DRIVER_SPI_STATUS_SUCCESS) {
                    nextState = GMOS_SPI_FLASH_TASK_STATE_ERASE_ALL_REQ;
                } else {
                    nextState = GMOS_SPI_FLASH_TASK_STATE_ERASE_FAILED;
                }
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_SPI_FLASH_TICK_INTERVAL);
            }
            break;

        // Attempt to send the erase request for the full device.
        case GMOS_SPI_FLASH_TASK_STATE_ERASE_ALL_REQ :
            if (gmosDriverFlashSfdpEraseAll (flash, &spiStatus)) {
                if (spiStatus == GMOS_DRIVER_SPI_STATUS_SUCCESS) {
                    nextState = GMOS_SPI_FLASH_TASK_STATE_ERASE_POLL_STATUS;
                    taskStatus = GMOS_TASK_RUN_LATER (
                        GMOS_SPI_FLASH_ERASE_POLL_INTERVAL);
                } else {
                    nextState = GMOS_SPI_FLASH_TASK_STATE_ERASE_FAILED;
                }
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_SPI_FLASH_TICK_INTERVAL);
            }
            break;

        // Poll the SPI device status register for completion of the
        // erase request.
        case GMOS_SPI_FLASH_TASK_STATE_ERASE_POLL_STATUS :
            if (gmosDriverFlashSfdpGetWriteInProgress (flash,
                &spiStatus, &writeInProgress)) {
                if (spiStatus != GMOS_DRIVER_SPI_STATUS_SUCCESS) {
                    nextState = GMOS_SPI_FLASH_TASK_STATE_ERASE_FAILED;
                } else if (writeInProgress) {
                    taskStatus = GMOS_TASK_RUN_LATER (
                        GMOS_SPI_FLASH_ERASE_POLL_INTERVAL);
                } else {
                    nextState = GMOS_SPI_FLASH_TASK_STATE_ERASE_COMPLETE;
                }
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_SPI_FLASH_TICK_INTERVAL);
            }
            break;

        // Complete the SPI device erase process by signalling
        // successful completion.
        case GMOS_SPI_FLASH_TASK_STATE_ERASE_COMPLETE :
            gmosEventAssignBits (&(flash->completionEvent),
                GMOS_DRIVER_FLASH_EVENT_COMPLETION_FLAG |
                GMOS_DRIVER_FLASH_STATUS_SUCCESS);
            nextPhase = GMOS_SPI_FLASH_TASK_PHASE_IDLE;
            nextState = GMOS_SPI_FLASH_TASK_STATE_ERASE_IDLE;
            break;

        // Indicate driver error on failure.
        default :
            gmosEventAssignBits (&(flash->completionEvent),
                GMOS_DRIVER_FLASH_EVENT_COMPLETION_FLAG |
                GMOS_DRIVER_FLASH_STATUS_DRIVER_ERROR);
            nextPhase = GMOS_SPI_FLASH_TASK_PHASE_IDLE;
            nextState = GMOS_SPI_FLASH_TASK_STATE_ERASE_IDLE;
            break;
    }
    palData->spiPhase = nextPhase;
    palData->spiState = nextState;
    return taskStatus;
}

/*
 * Implement the main SPI flash state machine task.
 */
static inline gmosTaskStatus_t gmosDriverFlashSfdpTaskFn (
    gmosDriverFlash_t* flash)
{
    gmosDriverFlashStateSfdp_t* palData =
        (gmosDriverFlashStateSfdp_t*) flash->palData;
    gmosTaskStatus_t taskStatus;

    // Select the current SPI flash operating phase.
    switch (palData->spiPhase) {

        // Perform device initialisation on startup.
        case GMOS_SPI_FLASH_TASK_PHASE_INIT :
            taskStatus = gmosDriverFlashSfdpStartup (flash);
            break;

        // Suspend processing in the idle state.
        case GMOS_SPI_FLASH_TASK_PHASE_IDLE :
            flash->flashState = GMOS_DRIVER_FLASH_STATE_IDLE;
            taskStatus = GMOS_TASK_SUSPEND;
            break;

        // Implement read data request state machine.
        case GMOS_SPI_FLASH_TASK_PHASE_READ :
            taskStatus = gmosDriverFlashSfdpDoRead (flash);
            break;

        // Implement write data request state machine.
        case GMOS_SPI_FLASH_TASK_PHASE_WRITE :
            taskStatus = gmosDriverFlashSfdpDoWrite (flash);
            break;

        // Implement erase request state machine.
        case GMOS_SPI_FLASH_TASK_PHASE_ERASE :
            taskStatus = gmosDriverFlashSfdpDoErase (flash);
            break;

        // Suspend operation on failure.
        default :
            GMOS_LOG (LOG_ERROR, "SPI Flash SFDP Driver Failed.");
            taskStatus = GMOS_TASK_SUSPEND;
            break;
    }
    return taskStatus;
}
GMOS_TASK_DEFINITION (gmosDriverFlashSfdpTask,
    gmosDriverFlashSfdpTaskFn, gmosDriverFlash_t);

/*
 * Sets the flash memory device write enable status. No hardware write
 * enable support is currently implemented.
 */
static bool gmosDriverFlashWriteEnableSfdp (gmosDriverFlash_t* flash,
    bool writeEnable)
{
    uint32_t eventBits;

    // Set write enabled flag in completed event code.
    if (writeEnable) {
        eventBits = GMOS_DRIVER_FLASH_EVENT_COMPLETION_FLAG |
            GMOS_DRIVER_FLASH_EVENT_WRITE_ENABLED_FLAG |
            GMOS_DRIVER_FLASH_STATUS_SUCCESS;
    }

    // Set write disabled flag in completed event code.
    else {
        eventBits = GMOS_DRIVER_FLASH_EVENT_COMPLETION_FLAG |
            GMOS_DRIVER_FLASH_EVENT_WRITE_DISABLED_FLAG |
            GMOS_DRIVER_FLASH_STATUS_SUCCESS;
    }

    // Indicate successful completion.
    gmosEventAssignBits (&(flash->completionEvent), eventBits);
    return true;
}

/*
 * Initiates an asynchronous flash device read request.
 */
static bool gmosDriverFlashReadSfdp (gmosDriverFlash_t* flash,
    uint32_t readAddr, uint8_t* readData, uint16_t readSize)
{
    gmosDriverFlashStateSfdp_t* palData =
        (gmosDriverFlashStateSfdp_t*) flash->palData;

    // Set the flash read request parameters.
    palData->spiPhase = GMOS_SPI_FLASH_TASK_PHASE_READ;
    palData->spiState = GMOS_SPI_FLASH_TASK_STATE_READ_START;
    palData->phase.read.flashAddr = readAddr;
    palData->phase.read.dataPtr = readData;
    palData->phase.read.dataSize = readSize;

    // Resume processing.
    gmosSchedulerTaskResume (&(palData->spiFlashTask));
    return true;
}

/*
 * Initiates an asynchronous flash device write request.
 */
static bool gmosDriverFlashWriteSfdp (gmosDriverFlash_t* flash,
    uint32_t writeAddr, uint8_t* writeData, uint16_t writeSize)
{
    gmosDriverFlashStateSfdp_t* palData =
        (gmosDriverFlashStateSfdp_t*) flash->palData;

    // Set the flash read request parameters.
    palData->spiPhase = GMOS_SPI_FLASH_TASK_PHASE_WRITE;
    palData->spiState = GMOS_SPI_FLASH_TASK_STATE_WRITE_START;
    palData->phase.write.flashAddr = writeAddr;
    palData->phase.write.dataPtr = writeData;
    palData->phase.write.dataSize = writeSize;

    // Resume processing.
    gmosSchedulerTaskResume (&(palData->spiFlashTask));
    return true;
}

/*
 * Initiates an asynchronous flash device block erase request. This will
 * erase a single flash memory block.
 */
static bool gmosDriverFlashEraseSfdp (gmosDriverFlash_t* flash,
    uint32_t eraseAddr)
{
    gmosDriverFlashStateSfdp_t* palData =
        (gmosDriverFlashStateSfdp_t*) flash->palData;

    // Set the flash erase parameters.
    palData->spiPhase = GMOS_SPI_FLASH_TASK_PHASE_ERASE;
    palData->spiState = GMOS_SPI_FLASH_TASK_STATE_ERASE_SECTOR;
    palData->phase.erase.sectorAddr = eraseAddr;

    // Resume processing.
    gmosSchedulerTaskResume (&(palData->spiFlashTask));
    return true;
}

/*
 * Initiates an asynchronous flash device bulk erase request. This will
 * erase the entire flash memory.
 */
static bool gmosDriverFlashEraseAllSfdp (gmosDriverFlash_t* flash)
{
    gmosDriverFlashStateSfdp_t* palData =
        (gmosDriverFlashStateSfdp_t*) flash->palData;

    // Set the flash erase parameters.
    palData->spiPhase = GMOS_SPI_FLASH_TASK_PHASE_ERASE;
    palData->spiState = GMOS_SPI_FLASH_TASK_STATE_ERASE_ALL;

    // Resume processing.
    gmosSchedulerTaskResume (&(palData->spiFlashTask));
    return true;
}

/*
 * Implements the generic SFDP flash memory initialisation function to
 * be used for the SPI flash memory device.
 */
bool gmosDriverFlashInitSfdp (gmosDriverFlash_t* flash)
{
    gmosDriverFlashConfigSfdp_t* palConfig =
        (gmosDriverFlashConfigSfdp_t*) flash->palConfig;
    gmosDriverFlashStateSfdp_t* palData =
        (gmosDriverFlashStateSfdp_t*) flash->palData;
    gmosTaskState_t* spiFlashTask = &(palData->spiFlashTask);
    gmosDriverSpiDevice_t* spiDevice = &(palData->spiDevice);
    bool initOk = true;

    // Populate the common driver fields.
    flash->palWriteEnable = gmosDriverFlashWriteEnableSfdp;
    flash->palRead = gmosDriverFlashReadSfdp;
    flash->palWrite = gmosDriverFlashWriteSfdp;
    flash->palErase = gmosDriverFlashEraseSfdp;
    flash->palEraseAll = gmosDriverFlashEraseAllSfdp;
    flash->blockSize = 0;
    flash->blockCount = 0;
    flash->readSize = 1;
    flash->writeSize = 1;
    flash->flashState = GMOS_DRIVER_FLASH_STATE_INIT;

    // Initialise SPI flash device data structure.
    if (!gmosDriverSpiDeviceInit (
        spiDevice, spiFlashTask, palConfig->spiChipSelect,
        GMOS_DRIVER_SPI_CHIP_SELECT_OPTION_ACTIVE_LOW,
        palConfig->spiClockRate, palConfig->spiClockMode)) {
        initOk = false;
        goto out;
    }

    // Initialise the SPI flash data structure.
    palData->phase.startup.index = 0;
    palData->spiPhase = GMOS_SPI_FLASH_TASK_PHASE_INIT;
    palData->spiState = GMOS_SPI_FLASH_TASK_STATE_INIT_IDLE;

    // Initialise the state machine task.
    gmosDriverFlashSfdpTask_start (
        &(palData->spiFlashTask), flash, "SPI Flash SFDP Driver Task");
out:
    return initOk;
}
