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
 * This header defines the device specific data types and support
 * functions for the ST25DVxx series of dynamic NFC tag devices.
 */

#ifndef ST25DV_DRIVER_NFCTAG_H
#define ST25DV_DRIVER_NFCTAG_H

#include <stdint.h>
#include "gmos-driver-iic.h"
#include "gmos-driver-nfctag.h"

/**
 * Defines the NFC tag radio specific I/O state data structure. The full
 * type definition is provided here for the ST25DVXX radio abstraction
 * layer.
 */
typedef struct gmosRalNfcTagState_t {

    // Allocate the IIC device data structure.
    gmosDriverIicDevice_t iicDevice;

    // Allocate the worker task data structure.
    gmosTaskState_t workerTask;

    // Specify the total device memory size in bytes.
    uint16_t memSize;

    // Specify the starting address of memory area 2.
    uint16_t memOffsetArea2;

    // Specify the starting address of memory area 3.
    uint16_t memOffsetArea3;

    // Specify the starting address of memory area 4.
    uint16_t memOffsetArea4;

    // Specify the current transaction data transfer address.
    uint16_t transferAddr;

    // Specify the current transaction data transfer size.
    uint16_t transferSize;

    // Specify the current driver operating phase.
    uint8_t driverPhase;

    // Specify the current driver operating state.
    uint8_t driverState;

    // Allocate storage for the device serial number.
    uint8_t serialNumber [8];

    // Allocate storage for the IIC address and transfer data.
    uint8_t transferData [2 + GMOS_CONFIG_NFC_TAG_TRANSFER_BLOCK_SIZE];

} gmosRalNfcTagState_t;

/**
 * Defines the NFC tag radio specific I/O configuration options. The
 * full type definition is provided here for the ST25DVXX radio
 * abstraction layer.
 */
typedef struct gmosRalNfcTagConfig_t {

    // Specify the IIC bus interface instance to use for communicating
    // with the NFC tag device. It should have been initialised prior
    // to use.
    gmosDriverIicBus_t* iicInterface;

} gmosRalNfcTagConfig_t;

#endif // ST25DV_DRIVER_NFCTAG_H
