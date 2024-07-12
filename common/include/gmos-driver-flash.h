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
 * This header defines the common data structures and management
 * functions used for flash memory storage devices. This includes both
 * on-device flash memory and external flash memory such as SPI NOR
 * devices. Since an application may require access to multiple
 * different types of flash memory, the driver design supports the use
 * of jump tables for the different API functions. The common API only
 * supports uniform, fixed sized flash block/sector erasure. Fast
 * overlay block erasure requests require device specific API support.
 */

#ifndef GMOS_DRIVER_FLASH_H
#define GMOS_DRIVER_FLASH_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-scheduler.h"
#include "gmos-events.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

// Provide a forward reference to the flash device data structure.
typedef struct gmosDriverFlash_t gmosDriverFlash_t;

/**
 * This enumeration specifies the flash device status values that are
 * returned by the transaction completion function.
 */
typedef enum {
    GMOS_DRIVER_FLASH_STATUS_IDLE,
    GMOS_DRIVER_FLASH_STATUS_SUCCESS,
    GMOS_DRIVER_FLASH_STATUS_ACTIVE,
    GMOS_DRIVER_FLASH_STATUS_NOT_READY,
    GMOS_DRIVER_FLASH_STATUS_WRITE_LOCKED,
    GMOS_DRIVER_FLASH_STATUS_CALLER_ERROR,
    GMOS_DRIVER_FLASH_STATUS_DRIVER_ERROR,
} gmosDriverFlashStatus_t;

/**
 * This enumeration specifies the various flash driver operating states.
 */
typedef enum {
    GMOS_DRIVER_FLASH_STATE_RESET,
    GMOS_DRIVER_FLASH_STATE_ERROR,
    GMOS_DRIVER_FLASH_STATE_IDLE,
    GMOS_DRIVER_FLASH_STATE_ACTIVE
} gmosDriverFlashState_t;

/*
 * This set of definitions specify the event bit masks used to indicate
 * transaction completion status from the platform abstraction layer
 * driver.
 */
#define GMOS_DRIVER_FLASH_EVENT_STATUS_OFFSET       0
#define GMOS_DRIVER_FLASH_EVENT_SIZE_OFFSET         8
#define GMOS_DRIVER_FLASH_EVENT_STATUS_MASK         0x000000FF
#define GMOS_DRIVER_FLASH_EVENT_SIZE_MASK           0x00FFFF00
#define GMOS_DRIVER_FLASH_EVENT_WRITE_ENABLED_FLAG  0x20000000
#define GMOS_DRIVER_FLASH_EVENT_WRITE_DISABLED_FLAG 0x40000000
#define GMOS_DRIVER_FLASH_EVENT_COMPLETION_FLAG     0x80000000

/**
 * Defines the function prototype to be used for flash device platform
 * abstraction initialisation functions.
 * @param flash This is the flash memory device data structure that is
 *     to be initialised.
 * @return Returns a boolean value which will be set to 'true' on
 *     successful initialisation and 'false' otherwise.
 */
typedef bool (*gmosPalFlashInit_t) (gmosDriverFlash_t* flash);

/**
 * Defines the function prototype to be used for flash device write
 * enable request functions.
 * @param flash This is the flash memory device data structure that is
 *     to be accessed for the write enable request.
 * @param writeEnable This is a boolean value which should be set to
 *     'true' if erase and write operations are to be enabled for the
 *     flash memory device and 'false' if they are to be disabled.
 * @return Returns a boolean value which will be set to 'true' if the
 *     flash write enable request was initiated and is now active and
 *     'false' otherwise.
 */
typedef bool (*gmosPalFlashWriteEnable_t) (gmosDriverFlash_t* flash,
    bool writeEnable);

/**
 * Defines the function prototype to be used for flash device read
 * request functions.
 * @param flash This is the flash memory device data structure that is
 *     to be accessed for the read request.
 * @param readData This is a pointer to a byte array that will be
 *     populated with the data read back from the flash memory. It must
 *     remain valid for the duration of the transaction.
 * @param readSize This specifies the number of bytes which are to be
 *     read back from the flash memory and placed in the read data
 *     array.
 * @return Returns a boolean value which will be set to 'true' if the
 *     flash read was initiated and is now active and 'false' otherwise.
 */
typedef bool (*gmosPalFlashRead_t) (gmosDriverFlash_t* flash,
    uint32_t readAddr, uint8_t* readData, uint16_t readSize);

/**
 * Defines the function prototype to be used for flash device write
 * request functions.
 * @param flash This is the flash memory device data structure that is
 *     to be accessed for the write request.
 * @param writeAddr This is the address of the data area to be written
 *     in flash memory. It should align with the flash write word size.
 * @param writeData This is a pointer to a byte array that contains the
 *     data to be written into flash memory. It must remain valid for
 *     the duration of the transaction.
 * @param writeSize This specifies the number of bytes which are to be
 *     written into the flash memory from the write data array. It
 *     should be an integer multiple of the flash write word size.
 * @return Returns a boolean value which will be set to 'true' if the
 *     flash write was initiated and is now active and 'false'
 *     otherwise.
 */
typedef bool (*gmosPalFlashWrite_t) (gmosDriverFlash_t* flash,
    uint32_t writeAddr, uint8_t* writeData, uint16_t writeSize);

/**
 * Defines the function prototype to be used for flash device block
 * erase request functions, which are used to erase a single block of
 * flash memory.
 * @param flash This is the flash memory device data structure that is
 *     to be accessed for the erase request.
 * @param eraseAddr This is the address of the flash memory block to be
 *     erased. It should align with the start of the flash memory block.
 * @return Returns a boolean value which will be set to 'true' if the
 *     flash erase operation was initiated and is now active and 'false'
 *     otherwise.
 */
typedef bool (*gmosPalFlashErase_t) (gmosDriverFlash_t* flash,
    uint32_t eraseAddr);

/**
 * Defines the function prototype to be used for flash device bulk erase
 * request functions, which are used to erase the entire flash memory.
 * @param flash This is the flash memory device data structure that is
 *     to be accessed for the erase request.
 * @return Returns a boolean value which will be set to 'true' if the
 *     flash erase operation was initiated and is now active and 'false'
 *     otherwise.
 */
typedef bool (*gmosPalFlashEraseAll_t) (gmosDriverFlash_t* flash);

/**
 * Defines the GubbinsMOS flash memory state data structure that is used
 * for managing the low level hardware for a single flash memory device.
 */
typedef struct gmosDriverFlash_t {

    // This is an opaque pointer to the platform abstraction layer data
    // structure that is used for accessing the flash memory hardware.
    // The data structure will be specific to the selected flash memory
    // type.
    void* palData;

    // This is an opaque pointer to the platform abstraction layer
    // configuration data structure that is used for setting up the
    // flash memory hardware. The data structure will be specific to the
    // selected flash memory type.
    const void* palConfig;

    // This is a pointer to the initialisation function that will be
    // called on startup to set up the flash memory specific driver.
    gmosPalFlashInit_t palInit;

    // This is a pointer to the platform specific flash memory write
    // enable request function.
    gmosPalFlashWriteEnable_t palWriteEnable;

    // This is a pointer to the platform specific flash memory read
    // request function.
    gmosPalFlashRead_t palRead;

    // This is a pointer to the platform specific flash memory write
    // request function.
    gmosPalFlashWrite_t palWrite;

    // This is a pointer to the platform specific flash memory block
    // erase request function.
    gmosPalFlashErase_t palErase;

    // This is a pointer to the platform specific flash memory bulk
    // erase request function.
    gmosPalFlashEraseAll_t palEraseAll;

    // This is the set of event flags that are used by the platform
    // abstraction layer to signal completion of a flash memory
    // transaction.
    gmosEvent_t completionEvent;

    // This specifies the erasable flash memory block/sector size as an
    // integer number of bytes. The value must be an integer power of
    // two.
    uint32_t blockSize;

    // This specifies the number of erasable flash memory blocks/sectors
    // on the device. This is used to derive the overall flash memory
    // size.
    uint16_t blockCount;

    // This specifies the minimum number of bytes that may be read in
    // a flash memory read operation. The value must be an integer power
    // of two. All reads must be a multiple of this size and have the
    // appropriate address alignment.
    uint16_t readSize;

    // This specifies the minimum number of bytes that may be written in
    // a flash programming operation. The value must be an integer power
    // of two. All writes must be a multiple of this size and have the
    // appropriate address alignment.
    uint16_t writeSize;

    // This specifies the current operating state for the flash memory
    // device.
    uint8_t flashState;

    // This specifies the current write enable state for the flash
    // memory device.
    uint8_t writeEnable;

} gmosDriverFlash_t;

/**
 * Provides a platform configuration setup macro to be used when
 * allocating a flash memory state data structure. Assigning this macro
 * to the flash memory state data structure on declaration will
 * configure the flash memory driver to use the platform specific
 * configuration.
 * @param _palData_ This is the flash memory platform abstraction layer
 *     data structure that is to be used for accessing the platform
 *     specific hardware.
 * @param _palConfig_ This is a platform specific flash memory
 *     configuration data structure that defines a set of fixed
 *     configuration options to be used with the flash memory device.
 * @param _palInit_ This is a pointer to the platform initialisation
 *     function that will be called on startup to set up the platform
 *     specific hardware.
 */
#define GMOS_DRIVER_FLASH_PAL_CONFIG(                                  \
    _palData_, _palConfig_, _palInit_)                                 \
{                                                                      \
    .palData         = _palData_,                                      \
    .palConfig       = _palConfig_,                                    \
    .palInit         = _palInit_,                                      \
    .palWriteEnable  = NULL,                                           \
    .palRead         = NULL,                                           \
    .palWrite        = NULL,                                           \
    .palErase        = NULL,                                           \
    .completionEvent = {},                                             \
    .blockSize       = 0,                                              \
    .blockCount      = 0,                                              \
    .readSize        = 0,                                              \
    .writeSize       = 0,                                              \
    .flashState      = GMOS_DRIVER_FLASH_STATE_RESET,                  \
    .writeEnable     = 0                                               \
}

/**
 * Initialises a flash memory driver on startup. This should be called
 * for each flash memory device prior to accessing it via any of the
 * other API functions.
 * @param flash This is the flash memory device data structure that is
 *     to be initialised. It should previously have been configured
 *     using the 'GMOS_DRIVER_FLASH_PAL_CONFIG' macro.
 * @param clientTask This is the client task which is to be notified
 *     on completion of flash memory I/O transactions.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully setting up the flash memory driver and 'false' on
 *     failure.
 */
bool gmosDriverFlashInit (gmosDriverFlash_t* flash,
    gmosTaskState_t* clientTask);

/**
 * Sets the flash memory device write enable status.
 * @param flash This is the flash memory device data structure that is
 *     to be accessed for the write enable request.
 * @param writeEnable This is a boolean value which should be set to
 *     'true' if erase and write operations are to be enabled for the
 *     flash memory device and 'false' if they are to be disabled.
 * @return Returns a boolean value which will be set to 'true' if the
 *     flash write enable request was initiated and is now active and
 *     'false' otherwise.
 */
bool gmosDriverFlashWriteEnable (gmosDriverFlash_t* flash,
    bool writeEnable);

/**
 * Initiates an asynchronous flash device read request.
 * @param flash This is the flash memory device data structure that is
 *     to be accessed for the read request.
 * @param readAddr This is the address of the data to be read back from
 *     flash memory.
 * @param readData This is a pointer to a byte array that will be
 *     populated with the data read back from the flash memory. It must
 *     remain valid for the duration of the transaction.
 * @param readSize This specifies the number of bytes which are to be
 *     read back from the flash memory and placed in the read data
 *     array.
 * @return Returns a boolean value which will be set to 'true' if the
 *     flash read was initiated and is now active and 'false' otherwise.
 */
bool gmosDriverFlashRead (gmosDriverFlash_t* flash,
    uint32_t readAddr, uint8_t* readData, uint16_t readSize);

/**
 * Initiates an asynchronous flash device write request.
 * @param flash This is the flash memory device data structure that is
 *     to be accessed for the write request.
 * @param writeAddr This is the address of the data area to be written
 *     in flash memory. It should align with the flash write word size.
 * @param writeData This is a pointer to a byte array that contains the
 *     data to be written into flash memory. It must remain valid for
 *     the duration of the transaction.
 * @param writeSize This specifies the number of bytes which are to be
 *     written into the flash memory from the write data array. It
 *     should be an integer multiple of the flash write word size.
 * @return Returns a boolean value which will be set to 'true' if the
 *     flash write was initiated and is now active and 'false'
 *     otherwise.
 */
bool gmosDriverFlashWrite (gmosDriverFlash_t* flash,
    uint32_t writeAddr, uint8_t* writeData, uint16_t writeSize);

/**
 * Initiates an asynchronous flash device block erase request. This will
 * erase a single flash memory block.
 * @param flash This is the flash memory device data structure that is
 *     to be accessed for the erase request.
 * @param eraseAddr This is the address of the flash memory block to be
 *     erased. It should align with the start of the flash memory block.
 * @return Returns a boolean value which will be set to 'true' if the
 *     flash erase operation was initiated and is now active and 'false'
 *     otherwise.
 */
bool gmosDriverFlashErase (gmosDriverFlash_t* flash,
    uint32_t eraseAddr);

/**
 * Initiates an asynchronous flash device bulk erase request. This will
 * erase the entire flash memory.
 * @param flash This is the flash memory device data structure that is
 *     to be accessed for the erase request.
 * @return Returns a boolean value which will be set to 'true' if the
 *     flash erase operation was initiated and is now active and 'false'
 *     otherwise.
 */
bool gmosDriverFlashEraseAll (gmosDriverFlash_t* flash);

/**
 * Completes an asynchronous flash memory transaction.
 * @param flash This is the flash memory device data structure that is
 *     to be accessed for the asynchronous completion status.
 * @param transferSize This is a pointer to a 16-bit unsigned integer
 *     which will be populated with the number of bytes transferred
 *     during the transaction. A null reference may be used to indicate
 *     that the transfer size information is not required.
 * @return Returns a driver status value which indicates the current
 *     flash device status. The transaction will be complete when this
 *     is no longer set to 'GMOS_DRIVER_FLASH_STATUS_ACTIVE'.
 */
gmosDriverFlashStatus_t gmosDriverFlashComplete
    (gmosDriverFlash_t* flash, uint16_t* transferSize);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // GMOS_DRIVER_FLASH_H
