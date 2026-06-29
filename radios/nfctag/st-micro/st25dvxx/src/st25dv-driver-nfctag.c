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
 * This file implements the device specific driver for supporting the
 * ST25DVxx series of dynamic NFC tag devices.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-scheduler.h"
#include "gmos-driver-iic.h"
#include "gmos-driver-nfctag.h"
#include "st25dv-driver-nfctag.h"

/*
 * Specify the fixed IIC address for the system area.
 */
#define GMOS_DRIVER_ST25DV_IIC_SYS_ADDR 0x57

/*
 * Specify the fixed IIC address for the user data area.
 */
#define GMOS_DRIVER_ST25DV_IIC_DATA_ADDR 0x53

/*
 * This enumeration defines the operating phases used by the NFC tag
 * driver worker task.
 */
typedef enum {
    GMOS_DRIVER_ST25DV_TASK_PHASE_INIT,
    GMOS_DRIVER_ST25DV_TASK_PHASE_IDLE,
    GMOS_DRIVER_ST25DV_TASK_PHASE_READ,
    GMOS_DRIVER_ST25DV_TASK_PHASE_WRITE,
    GMOS_DRIVER_ST25DV_TASK_PHASE_FAILED,
} gmosDriverSt25dvTaskPhase_t;

/*
 * This enumeration defines the startup state space used by the NFC tag
 * driver worker task.
 */
typedef enum {
    GMOS_DRIVER_ST25DV_TASK_STATE_INIT_IDLE,
    GMOS_DRIVER_ST25DV_TASK_STATE_INIT_IIC_SSEL,
    GMOS_DRIVER_ST25DV_TASK_STATE_INIT_SYSI_REQ,
    GMOS_DRIVER_ST25DV_TASK_STATE_INIT_SYSI_POLL,
    GMOS_DRIVER_ST25DV_TASK_STATE_INIT_SYSC_REQ,
    GMOS_DRIVER_ST25DV_TASK_STATE_INIT_SYSC_POLL,
    GMOS_DRIVER_ST25DV_TASK_STATE_INIT_IIC_SREL,
    GMOS_DRIVER_ST25DV_TASK_STATE_INIT_COMPLETE,
    GMOS_DRIVER_ST25DV_TASK_STATE_INIT_FAILED
} gmosDriverSt25dvTaskStateInit_t;

/*
 * This enumeration defines the read data transaction state space used
 * by the NFC tag driver worker task.
 */
typedef enum {
    GMOS_DRIVER_ST25DV_TASK_STATE_READ_IDLE,
    GMOS_DRIVER_ST25DV_TASK_STATE_READ_START,
    GMOS_DRIVER_ST25DV_TASK_STATE_READ_IIC_DSEL,
    GMOS_DRIVER_ST25DV_TASK_STATE_READ_DATA_REQ,
    GMOS_DRIVER_ST25DV_TASK_STATE_READ_DATA_POLL,
    GMOS_DRIVER_ST25DV_TASK_STATE_READ_IIC_DREL,
    GMOS_DRIVER_ST25DV_TASK_STATE_READ_COMPLETE,
    GMOS_DRIVER_ST25DV_TASK_STATE_READ_FAILED
} gmosDriverSt25dvTaskStateRead_t;

/*
 * This enumeration defines the write data transaction state space used
 * by the NFC tag driver worker task.
 */
typedef enum {
    GMOS_DRIVER_ST25DV_TASK_STATE_WRITE_IDLE,
    GMOS_DRIVER_ST25DV_TASK_STATE_WRITE_START,
    GMOS_DRIVER_ST25DV_TASK_STATE_WRITE_IIC_DSEL,
    GMOS_DRIVER_ST25DV_TASK_STATE_WRITE_DATA_REQ,
    GMOS_DRIVER_ST25DV_TASK_STATE_WRITE_DATA_POLL,
    GMOS_DRIVER_ST25DV_TASK_STATE_WRITE_DONE_REQ,
    GMOS_DRIVER_ST25DV_TASK_STATE_WRITE_DONE_POLL,
    GMOS_DRIVER_ST25DV_TASK_STATE_WRITE_IIC_DREL,
    GMOS_DRIVER_ST25DV_TASK_STATE_WRITE_COMPLETE,
    GMOS_DRIVER_ST25DV_TASK_STATE_WRITE_FAILED
} gmosDriverSt25dvTaskStateWrite_t;

/*
 * Perform a sanity check on the contents of the system information
 * registers.
 */
static inline bool st25dvDriverCheckSystemInfo (
    gmosDriverNfcTag_t* nfcTag)
{
    gmosRalNfcTagState_t* ralData = nfcTag->ralData;
    uint8_t* sysInfo = nfcTag->transferBlock;
    uint8_t* serNum = ralData->serialNumber;
    uint_fast8_t i;
    uint_fast16_t memSize;

    // Check for a valid serial number prefix and manufacturer code.
    // Also check for a supported device ID. This range of device ID
    // values covers all currently shipping devices in the ST25DVxx
    // product range.
    if ((sysInfo [15] != 0xE0) || (sysInfo [14] != 0x02) ||
        (sysInfo [13] > 0x27) || (sysInfo [13] < 0x24)) {
        return false;
    }

    // Calculate the memory size and cache it for subsequent use.
    memSize = 1 + (uint_fast16_t) sysInfo [4] +
        (((uint_fast16_t) sysInfo [5]) << 8);
    memSize *= 1 + sysInfo [6];
    GMOS_LOG_FMT (LOG_INFO,
        "ST25DV : Detected device ST25DV%02dK", memSize / 128);
    ralData->memSize = memSize;

    // Report the DSFID and AFI settings. If used, these need to be
    // configured via the radio interface at production.
    GMOS_LOG_FMT (LOG_DEBUG,
        "ST25DV : DSFID is set to 0x%02X (Locked : %d)",
        sysInfo [2], 0x01 & sysInfo [0]);
    GMOS_LOG_FMT (LOG_DEBUG,
        "ST25DV : AFI is set to 0x%02X (Locked : %d)",
        sysInfo [3], 0x01 & sysInfo [1]);

    // Cache the serial number for subsequent use.
    for (i = 0; i < 8; i++) {
        serNum [i] = sysInfo [i + 8];
    }
    GMOS_LOG_FMT (LOG_INFO, "ST25DV : Initialised with valid UID : "
        "%02X %02X %02X %02X %02X %02X %02X %02X",
        serNum [7], serNum [6], serNum [5], serNum [4],
        serNum [3], serNum [2], serNum [1], serNum [0]);
    return true;
}

/*
 * Perform a sanity check on the contents of the system configuration
 * registers.
 */
static inline bool st25dvDriverCheckSystemConfig (
    gmosDriverNfcTag_t* nfcTag)
{
    gmosRalNfcTagState_t* ralData = nfcTag->ralData;
    uint8_t* sysConfig = nfcTag->transferBlock;
    uint_fast16_t startAddrArea2;
    uint_fast16_t startAddrArea3;
    uint_fast16_t startAddrArea4;

    // Calculate area boundaries and cache them for subsequent use.
    startAddrArea2 = (((uint_fast16_t) sysConfig [5]) + 1) * 32;
    startAddrArea3 = (((uint_fast16_t) sysConfig [7]) + 1) * 32;
    startAddrArea4 = (((uint_fast16_t) sysConfig [9]) + 1) * 32;

    GMOS_LOG_FMT (LOG_INFO, "ST25DV : Area 1 0x%04x->0x%04X",
        0, startAddrArea2);
    GMOS_LOG_FMT (LOG_INFO, "ST25DV : Area 2 0x%04x->0x%04X",
        startAddrArea2, startAddrArea3);
    GMOS_LOG_FMT (LOG_INFO, "ST25DV : Area 3 0x%04x->0x%04X",
        startAddrArea3, startAddrArea4);
    GMOS_LOG_FMT (LOG_INFO, "ST25DV : Area 4 0x%04x->0x%04X",
        startAddrArea4, ralData->memSize);

    ralData->memOffsetArea2 = startAddrArea2;
    ralData->memOffsetArea3 = startAddrArea3;
    ralData->memOffsetArea4 = startAddrArea4;
    return true;
}

/*
 * Implement the state machine for the driver startup phase.
 */
static inline gmosTaskStatus_t st25dvDriverTaskStartup (
    gmosDriverNfcTag_t* nfcTag)
{
    const gmosRalNfcTagConfig_t* ralConfig = nfcTag->ralConfig;
    gmosRalNfcTagState_t* ralData = nfcTag->ralData;
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    gmosDriverIicBus_t* iicInterface = ralConfig->iicInterface;
    gmosDriverIicDevice_t* iicDevice = &(ralData->iicDevice);
    gmosDriverIicStatus_t iicStatus;
    gmosDriverSt25dvTaskPhase_t nextPhase = ralData->driverPhase;
    gmosDriverSt25dvTaskStateInit_t nextState = ralData->driverState;
    uint8_t* txBuf = ralData->transferData;
    uint8_t* rxBuf = nfcTag->transferBlock;

    // Implement the startup state machine.
    switch (ralData->driverState) {

        // From the idle state, insert a short delay to ensure that the
        // NFC tag power supply is stable and the boot process is
        // complete (at least 600 us).
        case GMOS_DRIVER_ST25DV_TASK_STATE_INIT_IDLE :
            nextState = GMOS_DRIVER_ST25DV_TASK_STATE_INIT_IIC_SSEL;
            taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (10));
            break;

        // In the system area select state, claim the IIC bus for
        // subsequent system memory access.
        case GMOS_DRIVER_ST25DV_TASK_STATE_INIT_IIC_SSEL :
            ralData->iicDevice.iicAddr = GMOS_DRIVER_ST25DV_IIC_SYS_ADDR;
            if (gmosDriverIicDeviceSelect (iicInterface, iicDevice)) {
                nextState = GMOS_DRIVER_ST25DV_TASK_STATE_INIT_SYSI_REQ;
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (10));
            }
            break;

        // Initiate a read for the read only system information area in
        // the address range 0x10 to 0x1F.
        case GMOS_DRIVER_ST25DV_TASK_STATE_INIT_SYSI_REQ :
            txBuf [0] = 0x00;
            txBuf [1] = 0x10;
            if (gmosDriverIicIoTransfer (
                iicInterface, txBuf, rxBuf, 2, 16)) {
                nextState = GMOS_DRIVER_ST25DV_TASK_STATE_INIT_SYSI_POLL;
                taskStatus = GMOS_TASK_SUSPEND;
            }
            break;

        // Wait for completion of the system information read request
        // and process the result.
        case GMOS_DRIVER_ST25DV_TASK_STATE_INIT_SYSI_POLL :
            iicStatus = gmosDriverIicIoComplete (iicInterface, NULL);
            if (iicStatus == GMOS_DRIVER_IIC_STATUS_SUCCESS) {
                if (st25dvDriverCheckSystemInfo (nfcTag)) {
                    nextState = GMOS_DRIVER_ST25DV_TASK_STATE_INIT_SYSC_REQ;
                } else {
                    nextState = GMOS_DRIVER_ST25DV_TASK_STATE_INIT_FAILED;
                }
            } else if (iicStatus == GMOS_DRIVER_IIC_STATUS_ACTIVE) {
                taskStatus = GMOS_TASK_SUSPEND;
            } else {
                nextState = GMOS_DRIVER_ST25DV_TASK_STATE_INIT_FAILED;
            }
            break;

        // Initiate a read for the read only system configuration area
        // in the address range 0x10 to 0x1F.
        case GMOS_DRIVER_ST25DV_TASK_STATE_INIT_SYSC_REQ :
            txBuf [0] = 0x00;
            txBuf [1] = 0x00;
            if (gmosDriverIicIoTransfer (
                iicInterface, txBuf, rxBuf, 2, 16)) {
                nextState = GMOS_DRIVER_ST25DV_TASK_STATE_INIT_SYSC_POLL;
                taskStatus = GMOS_TASK_SUSPEND;
            }
            break;

        // Wait for completion of the system configuration read request
        // and process the result.
        case GMOS_DRIVER_ST25DV_TASK_STATE_INIT_SYSC_POLL :
            iicStatus = gmosDriverIicIoComplete (iicInterface, NULL);
            if (iicStatus == GMOS_DRIVER_IIC_STATUS_SUCCESS) {
                if (st25dvDriverCheckSystemConfig (nfcTag)) {
                    nextState = GMOS_DRIVER_ST25DV_TASK_STATE_INIT_IIC_SREL;
                } else {
                    nextState = GMOS_DRIVER_ST25DV_TASK_STATE_INIT_FAILED;
                }
            } else if (iicStatus == GMOS_DRIVER_IIC_STATUS_ACTIVE) {
                taskStatus = GMOS_TASK_SUSPEND;
            } else {
                nextState = GMOS_DRIVER_ST25DV_TASK_STATE_INIT_FAILED;
            }
            break;

        // Release the IIC bus after system area accesses.
        case GMOS_DRIVER_ST25DV_TASK_STATE_INIT_IIC_SREL :
            if (gmosDriverIicDeviceRelease (iicInterface, iicDevice)) {
                nextState = GMOS_DRIVER_ST25DV_TASK_STATE_INIT_COMPLETE;
            } else {
                nextState = GMOS_DRIVER_ST25DV_TASK_STATE_INIT_FAILED;
            }
            break;

        // Startup phase is complete. Enter idle phase.
        case GMOS_DRIVER_ST25DV_TASK_STATE_INIT_COMPLETE :
            GMOS_LOG (LOG_DEBUG, "ST25DV : Initialisation process complete.");
            nextState = GMOS_DRIVER_ST25DV_TASK_STATE_INIT_IDLE;
            nextPhase = GMOS_DRIVER_ST25DV_TASK_PHASE_IDLE;
            break;

        // Suspend further processing on failure.
        default :
            GMOS_LOG (LOG_ERROR, "ST25DV : Initialisation process failed.");
            nextState = GMOS_DRIVER_ST25DV_TASK_STATE_INIT_IDLE;
            nextPhase = GMOS_DRIVER_ST25DV_TASK_PHASE_FAILED;
            break;
    }
    ralData->driverPhase = nextPhase;
    ralData->driverState = nextState;
    return taskStatus;
}

/*
 * Implement the state machine for the driver read data phase.
 */
static inline gmosTaskStatus_t st25dvDriverTaskReadData (
    gmosDriverNfcTag_t* nfcTag)
{
    const gmosRalNfcTagConfig_t* ralConfig = nfcTag->ralConfig;
    gmosRalNfcTagState_t* ralData = nfcTag->ralData;
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    gmosDriverIicBus_t* iicInterface = ralConfig->iicInterface;
    gmosDriverIicDevice_t* iicDevice = &(ralData->iicDevice);
    gmosDriverIicStatus_t iicStatus;
    gmosDriverSt25dvTaskPhase_t nextPhase = ralData->driverPhase;
    gmosDriverSt25dvTaskStateRead_t nextState = ralData->driverState;
    uint8_t* txBuf = ralData->transferData;
    uint8_t* rxBuf = nfcTag->transferBlock;

    // Implement the read data state machine.
    switch (ralData->driverState) {

        // Start processing the read transaction.
        case GMOS_DRIVER_ST25DV_TASK_STATE_READ_START :
            nextState = GMOS_DRIVER_ST25DV_TASK_STATE_READ_IIC_DSEL;
            break;

        // In the data area select state, claim the IIC bus for
        // subsequent data memory access.
        case GMOS_DRIVER_ST25DV_TASK_STATE_READ_IIC_DSEL :
            ralData->iicDevice.iicAddr = GMOS_DRIVER_ST25DV_IIC_DATA_ADDR;
            if (gmosDriverIicDeviceSelect (iicInterface, iicDevice)) {
                nextState = GMOS_DRIVER_ST25DV_TASK_STATE_READ_DATA_REQ;
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (10));
            }
            break;

        // Initiate the read request for the specified data block.
        case GMOS_DRIVER_ST25DV_TASK_STATE_READ_DATA_REQ :
            txBuf [0] = ralData->transferAddr >> 8;
            txBuf [1] = ralData->transferAddr;
            if (gmosDriverIicIoTransfer (
                iicInterface, txBuf, rxBuf, 2, ralData->transferSize)) {
                nextState = GMOS_DRIVER_ST25DV_TASK_STATE_READ_DATA_POLL;
                taskStatus = GMOS_TASK_SUSPEND;
            }
            break;

        // Wait for completion of the data block read request and
        // process the result.
        case GMOS_DRIVER_ST25DV_TASK_STATE_READ_DATA_POLL :
            iicStatus = gmosDriverIicIoComplete (iicInterface, NULL);
            if (iicStatus == GMOS_DRIVER_IIC_STATUS_SUCCESS) {
                nextState = GMOS_DRIVER_ST25DV_TASK_STATE_READ_IIC_DREL;
            } else if (iicStatus == GMOS_DRIVER_IIC_STATUS_ACTIVE) {
                taskStatus = GMOS_TASK_SUSPEND;
            } else {
                nextState = GMOS_DRIVER_ST25DV_TASK_STATE_READ_FAILED;
            }
            break;

        // Release the IIC bus after data memory accesses.
        case GMOS_DRIVER_ST25DV_TASK_STATE_READ_IIC_DREL :
            if (gmosDriverIicDeviceRelease (iicInterface, iicDevice)) {
                nextState = GMOS_DRIVER_ST25DV_TASK_STATE_READ_COMPLETE;
            } else {
                nextState = GMOS_DRIVER_ST25DV_TASK_STATE_READ_FAILED;
            }
            break;

        // Complete read transaction processing with a status callback.
        case GMOS_DRIVER_ST25DV_TASK_STATE_READ_COMPLETE :
            GMOS_LOG (LOG_DEBUG, "ST25DV : Read data process complete.");
            gmosDriverNfcTagRalReadComplete (nfcTag,
                GMOS_DRIVER_NFC_TAG_STATUS_SUCCESS);
            nextState = GMOS_DRIVER_ST25DV_TASK_STATE_READ_IDLE;
            nextPhase = GMOS_DRIVER_ST25DV_TASK_PHASE_IDLE;
            break;

        // Suspend further processing on failure.
        default :
            GMOS_LOG (LOG_ERROR, "ST25DV : Read data process failed.");
            gmosDriverNfcTagRalReadComplete (nfcTag,
                GMOS_DRIVER_NFC_TAG_STATUS_FATAL_ERROR);
            nextState = GMOS_DRIVER_ST25DV_TASK_STATE_READ_IDLE;
            nextPhase = GMOS_DRIVER_ST25DV_TASK_PHASE_FAILED;
            break;
    }
    ralData->driverPhase = nextPhase;
    ralData->driverState = nextState;
    return taskStatus;
}

/*
 * Implement the state machine for the driver read data phase.
 */
static inline gmosTaskStatus_t st25dvDriverTaskWriteData (
    gmosDriverNfcTag_t* nfcTag)
{
    const gmosRalNfcTagConfig_t* ralConfig = nfcTag->ralConfig;
    gmosRalNfcTagState_t* ralData = nfcTag->ralData;
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    gmosDriverIicBus_t* iicInterface = ralConfig->iicInterface;
    gmosDriverIicDevice_t* iicDevice = &(ralData->iicDevice);
    gmosDriverIicStatus_t iicStatus;
    gmosDriverSt25dvTaskPhase_t nextPhase = ralData->driverPhase;
    gmosDriverSt25dvTaskStateWrite_t nextState = ralData->driverState;
    uint8_t* txBuf = ralData->transferData;

    // Implement the write data state machine.
    switch (ralData->driverState) {

        // Start processing the write transaction.
        case GMOS_DRIVER_ST25DV_TASK_STATE_WRITE_START :
            nextState = GMOS_DRIVER_ST25DV_TASK_STATE_WRITE_IIC_DSEL;
            break;

        // In the data area select state, claim the IIC bus for
        // subsequent data memory access.
        case GMOS_DRIVER_ST25DV_TASK_STATE_WRITE_IIC_DSEL :
            ralData->iicDevice.iicAddr = GMOS_DRIVER_ST25DV_IIC_DATA_ADDR;
            if (gmosDriverIicDeviceSelect (iicInterface, iicDevice)) {
                nextState = GMOS_DRIVER_ST25DV_TASK_STATE_WRITE_DATA_REQ;
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (10));
            }
            break;

        // Initiate the write request for the specified data block. Note
        // that the write data immediately follows the address bytes in
        // the transmit buffer.
        case GMOS_DRIVER_ST25DV_TASK_STATE_WRITE_DATA_REQ :
            txBuf [0] = ralData->transferAddr >> 8;
            txBuf [1] = ralData->transferAddr;
            if (gmosDriverIicIoWrite (
                iicInterface, txBuf, 2 + ralData->transferSize)) {
                nextState = GMOS_DRIVER_ST25DV_TASK_STATE_WRITE_DATA_POLL;
                taskStatus = GMOS_TASK_SUSPEND;
            }
            break;

        // Wait for completion of the data block write request and
        // start polling for EEPROM write completion,
        case GMOS_DRIVER_ST25DV_TASK_STATE_WRITE_DATA_POLL :
            iicStatus = gmosDriverIicIoComplete (iicInterface, NULL);
            if (iicStatus == GMOS_DRIVER_IIC_STATUS_SUCCESS) {
                nextState = GMOS_DRIVER_ST25DV_TASK_STATE_WRITE_DONE_REQ;
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (10));
            } else if (iicStatus == GMOS_DRIVER_IIC_STATUS_ACTIVE) {
                taskStatus = GMOS_TASK_SUSPEND;
            } else {
                nextState = GMOS_DRIVER_ST25DV_TASK_STATE_WRITE_FAILED;
            }
            break;

        // Issue a zero length write request which polls for EEPROM
        // write completion.
        case GMOS_DRIVER_ST25DV_TASK_STATE_WRITE_DONE_REQ :
            if (gmosDriverIicIoWrite (iicInterface, txBuf, 0)) {
                nextState = GMOS_DRIVER_ST25DV_TASK_STATE_WRITE_DONE_POLL;
                taskStatus = GMOS_TASK_SUSPEND;
            }
            break;

        // If an EEPROM write is in progress, the polling request will
        // result in an IIC NAK response.
        case GMOS_DRIVER_ST25DV_TASK_STATE_WRITE_DONE_POLL :
            iicStatus = gmosDriverIicIoComplete (iicInterface, NULL);
            if (iicStatus == GMOS_DRIVER_IIC_STATUS_SUCCESS) {
                nextState = GMOS_DRIVER_ST25DV_TASK_STATE_WRITE_IIC_DREL;
            } else if (iicStatus == GMOS_DRIVER_IIC_STATUS_ACTIVE) {
                taskStatus = GMOS_TASK_SUSPEND;
            } else if (iicStatus == GMOS_DRIVER_IIC_STATUS_NACK) {
                nextState = GMOS_DRIVER_ST25DV_TASK_STATE_WRITE_DONE_REQ;
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (10));
            } else {
                nextState = GMOS_DRIVER_ST25DV_TASK_STATE_WRITE_FAILED;
            }
            break;

        // Release the IIC bus after data memory accesses.
        case GMOS_DRIVER_ST25DV_TASK_STATE_WRITE_IIC_DREL :
            if (gmosDriverIicDeviceRelease (iicInterface, iicDevice)) {
                nextState = GMOS_DRIVER_ST25DV_TASK_STATE_WRITE_COMPLETE;
            } else {
                nextState = GMOS_DRIVER_ST25DV_TASK_STATE_WRITE_FAILED;
            }
            break;

        // Complete write transaction processing with a status callback.
        case GMOS_DRIVER_ST25DV_TASK_STATE_WRITE_COMPLETE :
            GMOS_LOG (LOG_DEBUG, "ST25DV : Write data process complete.");
            gmosDriverNfcTagRalWriteComplete (nfcTag,
                GMOS_DRIVER_NFC_TAG_STATUS_SUCCESS);
            nextState = GMOS_DRIVER_ST25DV_TASK_STATE_WRITE_IDLE;
            nextPhase = GMOS_DRIVER_ST25DV_TASK_PHASE_IDLE;
            break;

        // Suspend further processing on failure.
        default :
            GMOS_LOG (LOG_ERROR, "ST25DV : Write data process failed.");
            gmosDriverNfcTagRalWriteComplete (nfcTag,
                GMOS_DRIVER_NFC_TAG_STATUS_FATAL_ERROR);
            nextState = GMOS_DRIVER_ST25DV_TASK_STATE_WRITE_IDLE;
            nextPhase = GMOS_DRIVER_ST25DV_TASK_PHASE_FAILED;
            break;
    }
    ralData->driverPhase = nextPhase;
    ralData->driverState = nextState;
    return taskStatus;
}

/*
 * Implement the worker task for NFC tag access.
 */
static inline gmosTaskStatus_t st25dvDriverTaskFn (
    gmosDriverNfcTag_t* nfcTag)
{
    gmosRalNfcTagState_t* ralData = nfcTag->ralData;
    gmosTaskStatus_t taskStatus;

    // Select the current driver operating phase.
    switch (ralData->driverPhase) {
        case GMOS_DRIVER_ST25DV_TASK_PHASE_INIT :
            taskStatus = st25dvDriverTaskStartup (nfcTag);
            break;

        // Suspend further processing on idle.
        case GMOS_DRIVER_ST25DV_TASK_PHASE_IDLE :
            taskStatus = GMOS_TASK_SUSPEND;
            break;

        // Process read transactions.
        case GMOS_DRIVER_ST25DV_TASK_PHASE_READ :
            taskStatus = st25dvDriverTaskReadData (nfcTag);
            break;

        // Process write transactions.
        case GMOS_DRIVER_ST25DV_TASK_PHASE_WRITE :
            taskStatus = st25dvDriverTaskWriteData (nfcTag);
            break;

        // Suspend further processing on failure.
        default :
            GMOS_LOG (LOG_ERROR, "ST25DV : Driver suspended.");
            taskStatus = GMOS_TASK_SUSPEND;
            break;
    }
    return taskStatus;
}

// Add the NFC tag worker task definition.
GMOS_TASK_DEFINITION (st25dvDriverNfcTagRalTask,
    st25dvDriverTaskFn, gmosDriverNfcTag_t);

/*
 * Initialises the radio abstraction layer for a given NFC tag.
 */
bool gmosDriverNfcTagRalInit (gmosDriverNfcTag_t* nfcTag)
{
    gmosRalNfcTagState_t* ralData = nfcTag->ralData;

    // Initialise the NFC tag driver data structure. This reserves two
    // bytes at the start of the allocated data area for the address.
    nfcTag->transferBlock = &(ralData->transferData [2]);

    // Initialise the driver state variables.
    ralData->driverPhase = GMOS_DRIVER_ST25DV_TASK_PHASE_INIT;
    ralData->driverState = GMOS_DRIVER_ST25DV_TASK_STATE_INIT_IDLE;

    // Initialise the IIC device data structure.
    if (!gmosDriverIicDeviceInit (&(ralData->iicDevice),
        &(ralData->workerTask), GMOS_DRIVER_ST25DV_IIC_SYS_ADDR)) {
        return false;
    }

    // Initialise the NFC tag worker task.
    st25dvDriverNfcTagRalTask_start (&(ralData->workerTask),
        nfcTag, "ST25DVxx Driver Task");
    return true;
}

/*
 * Gets the data area information for a specific NFC tag data area.
 */
gmosDriverNfcTagStatus_t gmosDriverNfcTagGetAreaInfo (
    gmosDriverNfcTag_t* nfcTag, uint8_t dataArea, uint16_t* dataAreaSize)
{
    gmosRalNfcTagState_t* ralData = nfcTag->ralData;
    uint_fast16_t areaSize = 0;
    gmosDriverNfcTagStatus_t status = GMOS_DRIVER_NFC_TAG_STATUS_SUCCESS;

    // Wait for startup completion.
    if (ralData->driverPhase == GMOS_DRIVER_ST25DV_TASK_PHASE_INIT) {
        status = GMOS_DRIVER_NFC_TAG_STATUS_NOT_READY;
        goto out;
    }

    // Indicate failure conditions.
    if (ralData->driverPhase == GMOS_DRIVER_ST25DV_TASK_PHASE_FAILED) {
        status = GMOS_DRIVER_NFC_TAG_STATUS_FATAL_ERROR;
        goto out;
    }

    // Get the available address range for the specified area. Data
    // area 0 is the default, and maps to device area 1 in this case.
    switch (dataArea) {
        case 0 :
        case 1 :
            areaSize = ralData->memOffsetArea2;
            break;
        case 2 :
            areaSize = ralData->memOffsetArea3 - ralData->memOffsetArea2;
            break;
        case 3 :
            areaSize = ralData->memOffsetArea4 - ralData->memOffsetArea3;
            break;
        case 4 :
            areaSize = ralData->memSize - ralData->memOffsetArea4;
            break;
        default :
            status = GMOS_DRIVER_NFC_TAG_STATUS_INVALID_ADDRESS;
            goto out;
    }

    // Return the area information fields.
out :
    if (dataAreaSize != NULL) {
        *dataAreaSize = areaSize;
    }
    return status;
}

/*
 * Initiates a RAL read transaction for the specified tag data area.
 */
gmosDriverNfcTagStatus_t gmosDriverNfcTagRalReadStart (
    gmosDriverNfcTag_t* nfcTag, uint16_t readAddr, uint16_t readSize)
{
    gmosRalNfcTagState_t* ralData = nfcTag->ralData;
    gmosDriverNfcTagStatus_t status = GMOS_DRIVER_NFC_TAG_STATUS_SUCCESS;

    // Indicate failure conditions.
    if (ralData->driverPhase == GMOS_DRIVER_ST25DV_TASK_PHASE_FAILED) {
        status = GMOS_DRIVER_NFC_TAG_STATUS_FATAL_ERROR;
        goto out;
    }

    // Retry if the driver is busy.
    if (ralData->driverPhase != GMOS_DRIVER_ST25DV_TASK_PHASE_IDLE) {
        status = GMOS_DRIVER_NFC_TAG_STATUS_NOT_READY;
        goto out;
    }

    // Add memory area offsets to the specified read address.
    ralData->transferAddr = readAddr;
    switch (nfcTag->dataArea) {
        case 0 :
        case 1 :
            ralData->transferAddr += 0;
            break;
        case 2 :
            ralData->transferAddr += ralData->memOffsetArea2;
            break;
        case 3 :
            ralData->transferAddr += ralData->memOffsetArea3;
            break;
        case 4 :
            ralData->transferAddr += ralData->memOffsetArea4;
            break;
        default :
            status = GMOS_DRIVER_NFC_TAG_STATUS_INVALID_ADDRESS;
            goto out;
    }
    ralData->transferSize = readSize;

    // Start the read data process.
    ralData->driverPhase = GMOS_DRIVER_ST25DV_TASK_PHASE_READ;
    ralData->driverState = GMOS_DRIVER_ST25DV_TASK_STATE_READ_START;
    gmosSchedulerTaskResume (&(ralData->workerTask));

out:
    return status;
}

/*
 * Initiates a RAL write transaction for the specified tag data area.
 */
gmosDriverNfcTagStatus_t gmosDriverNfcTagRalWriteStart (
    gmosDriverNfcTag_t* nfcTag, uint16_t writeAddr, uint16_t writeSize)
{
    gmosRalNfcTagState_t* ralData = nfcTag->ralData;
    gmosDriverNfcTagStatus_t status = GMOS_DRIVER_NFC_TAG_STATUS_SUCCESS;

    // Indicate failure conditions.
    if (ralData->driverPhase == GMOS_DRIVER_ST25DV_TASK_PHASE_FAILED) {
        status = GMOS_DRIVER_NFC_TAG_STATUS_FATAL_ERROR;
        goto out;
    }

    // Retry if the driver is busy.
    if (ralData->driverPhase != GMOS_DRIVER_ST25DV_TASK_PHASE_IDLE) {
        status = GMOS_DRIVER_NFC_TAG_STATUS_NOT_READY;
        goto out;
    }

    // Add memory area offsets to the specified write address.
    ralData->transferAddr = writeAddr;
    switch (nfcTag->dataArea) {
        case 0 :
        case 1 :
            ralData->transferAddr += 0;
            break;
        case 2 :
            ralData->transferAddr += ralData->memOffsetArea2;
            break;
        case 3 :
            ralData->transferAddr += ralData->memOffsetArea3;
            break;
        case 4 :
            ralData->transferAddr += ralData->memOffsetArea4;
            break;
        default :
            status = GMOS_DRIVER_NFC_TAG_STATUS_INVALID_ADDRESS;
            goto out;
    }
    ralData->transferSize = writeSize;

    // Start the read data process.
    ralData->driverPhase = GMOS_DRIVER_ST25DV_TASK_PHASE_WRITE;
    ralData->driverState = GMOS_DRIVER_ST25DV_TASK_STATE_WRITE_START;
    gmosSchedulerTaskResume (&(ralData->workerTask));

out:
    return status;
}
