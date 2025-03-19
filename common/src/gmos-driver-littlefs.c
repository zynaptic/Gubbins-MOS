/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2025 Zynaptic Limited
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
 * This file implements the API wrapper for the Little FS filing system,
 * which integrates the standard Little FS library into the GubbinsMOS
 * runtime framework.
 */

#include "gmos-config.h"

// The LittleFS code is only compiled if configured.
#if GMOS_CONFIG_LITTLEFS_ENABLE

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "gmos-platform.h"
#include "gmos-scheduler.h"
#include "gmos-driver-littlefs.h"

// Implement LittleFS tracing to log verbose debug messages.
#if (GMOS_CONFIG_LOG_LEVEL <= LOG_VERBOSE)
#define _LFS_TRACE_(_fmt_, ...) \
    GMOS_LOG_FMT (LOG_VERBOSE, _fmt_, __VA_ARGS__)
#define LFS_TRACE(...) _LFS_TRACE_(__VA_ARGS__, "")
#else
#define LFS_TRACE(...)
#endif

// Implement LittleFS debug to log standard debug messages.
#if (GMOS_CONFIG_LOG_LEVEL <= LOG_DEBUG)
#define _LFS_DEBUG_(_fmt_, ...) \
    GMOS_LOG_FMT (LOG_DEBUG, _fmt_, __VA_ARGS__)
#define LFS_DEBUG(...) _LFS_DEBUG_(__VA_ARGS__, "")
#else
#define LFS_DEBUG(...)
#endif

// Implement LittleFS warnings to log warning messages.
#if (GMOS_CONFIG_LOG_LEVEL <= LOG_WARNING)
#define _LFS_WARN_(_fmt_, ...) \
    GMOS_LOG_FMT (LOG_WARNING, _fmt_, __VA_ARGS__)
#define LFS_WARN(...) _LFS_WARN_(__VA_ARGS__, "")
#else
#define LFS_WARN(...)
#endif

// Implement LittleFS errors to log error messages.
#if (GMOS_CONFIG_LOG_LEVEL <= LOG_WARNING)
#define _LFS_ERROR_(_fmt_, ...) \
    GMOS_LOG_FMT (LOG_ERROR, _fmt_, __VA_ARGS__)
#define LFS_ERROR(...) _LFS_ERROR_(__VA_ARGS__, "")
#else
#define LFS_ERROR(...)
#endif

// Implement LittleFS assertions for assertion errors.
#if (GMOS_CONFIG_ASSERT_LEVEL <= ASSERT_ERROR)
#define LFS_ASSERT(_test_) \
    GMOS_ASSERT (ASSERT_ERROR, _test_, \
    "Assertion in LittleFS imported library.")
#else
#define LFS_ASSERT(test)
#endif

// Implement LittleFS malloc and free if heap is enabled.
#if (GMOS_CONFIG_HEAP_SIZE > 0)
#define LFS_MALLOC(_size_) GMOS_MALLOC (_size_)
#define LFS_FREE(_mem_) GMOS_FREE (_mem_)
#else
#define LFS_NO_MALLOC true
#error "The GubbinsMOS LittleFS driver requires dynamic memory support."
#endif

// The main LittleFS code is included for inline compilation.
#include "lfs.c"
#include "lfs_util.c"

/*
 * Define the set of file system operating states.
 */
typedef enum {
    GMOS_DRIVER_LITTLEFS_STATE_INIT,
    GMOS_DRIVER_LITTLEFS_STATE_UNMOUNTED,
    GMOS_DRIVER_LITTLEFS_STATE_MOUNTED,
    GMOS_DRIVER_LITTLEFS_STATE_RUNNING_GC,
    GMOS_DRIVER_LITTLEFS_STATE_FAILED
} gmosDriverLittlefsState_t;

/*
 * Implement the LittleFS flash memory reader function.
 */
static int gmosDriverLittlefsRead (const struct lfs_config* lfsConfig,
    lfs_block_t block, lfs_off_t offset, void *buffer, lfs_size_t size)
{
    gmosDriverLittlefs_t* littlefs =
        (gmosDriverLittlefs_t*) lfsConfig->context;
    gmosDriverFlash_t* flash = littlefs->flashDevice;
    gmosDriverFlashStatus_t flashStatus;
    uint32_t readAddr;
    uint16_t readSize;
    int lfsStatus;

    // Attempt to initiate a flash memory read request.
    readAddr = block * lfsConfig->block_size + offset;
    while (!gmosDriverFlashRead (
        flash, readAddr, (uint8_t*) buffer, size)) {
        gmosSchedulerTaskBusyWait ();
    }

    // Wait until the flash memory transaction is complete.
    do {
        gmosSchedulerTaskBusyWait ();
        flashStatus = gmosDriverFlashComplete (flash, &readSize);
    } while (flashStatus == GMOS_DRIVER_FLASH_STATUS_ACTIVE);

    // Indicate successful completion or an I/O error.
    if (flashStatus == GMOS_DRIVER_FLASH_STATUS_SUCCESS) {
        lfsStatus = readSize;
    } else {
        lfsStatus = LFS_ERR_IO;
    }

    // Optionally include verbose logging.
    if (GMOS_CONFIG_LITTLEFS_LOG_FLASH_IO) {
        GMOS_LOG_FMT (LOG_VERBOSE, "gmosDriverLittlefsRead  "
            "(block = %4d, offset = %4d, size = %4d) -> %d",
            block, offset, size, lfsStatus);
    }
    return lfsStatus;
}

/*
 * Implement the LittleFS flash memory writer function.
 */
static int gmosDriverLittlefsWrite (const struct lfs_config* lfsConfig,
    lfs_block_t block, lfs_off_t offset, const void *buffer, lfs_size_t size)
{
    gmosDriverLittlefs_t* littlefs =
        (gmosDriverLittlefs_t*) lfsConfig->context;
    gmosDriverFlash_t* flash = littlefs->flashDevice;
    gmosDriverFlashStatus_t flashStatus;
    uint32_t writeAddr;
    uint16_t writeSize;
    int lfsStatus;

    // Attempt to initiate a flash memory write request.
    writeAddr = block * lfsConfig->block_size + offset;
    while (!gmosDriverFlashWrite (
        flash, writeAddr, (uint8_t*) buffer, size)) {
        gmosSchedulerTaskBusyWait ();
    }

    // Wait until the flash memory transaction is complete.
    do {
        gmosSchedulerTaskBusyWait ();
        flashStatus = gmosDriverFlashComplete (flash, &writeSize);
    } while (flashStatus == GMOS_DRIVER_FLASH_STATUS_ACTIVE);

    // Indicate successful completion or an I/O error.
    if (flashStatus == GMOS_DRIVER_FLASH_STATUS_SUCCESS) {
        lfsStatus = writeSize;
    } else if (flashStatus == GMOS_DRIVER_FLASH_STATUS_WRITE_LOCKED) {
        lfsStatus = LFS_ERR_ROFS;
    } else {
        lfsStatus = LFS_ERR_IO;
    }

    // Optionally include verbose logging.
    if (GMOS_CONFIG_LITTLEFS_LOG_FLASH_IO) {
        GMOS_LOG_FMT (LOG_VERBOSE,
            "gmosDriverLittlefsWrite "
            "(block = %4d, offset = %4d, size = %4d) -> %d",
            block, offset, size, lfsStatus);
    }
    return lfsStatus;
}

/*
 * Implement the LittleFS flash memory block erase function.
 */
static int gmosDriverLittlefsErase (const struct lfs_config* lfsConfig,
    lfs_block_t block)
{
    gmosDriverLittlefs_t* littlefs =
        (gmosDriverLittlefs_t*) lfsConfig->context;
    gmosDriverFlash_t* flash = littlefs->flashDevice;
    gmosDriverFlashStatus_t flashStatus;
    uint32_t eraseAddr;
    int lfsStatus;

    // Attempt to initiate a flash memory erase request.
    eraseAddr = block * lfsConfig->block_size;
    while (!gmosDriverFlashErase (flash, eraseAddr)) {
        gmosSchedulerTaskBusyWait ();
    }

    // Wait until the flash memory transaction is complete.
    do {
        gmosSchedulerTaskBusyWait ();
        flashStatus = gmosDriverFlashComplete (flash, NULL);
    } while (flashStatus == GMOS_DRIVER_FLASH_STATUS_ACTIVE);

    // Indicate successful completion or an I/O error.
    if (flashStatus == GMOS_DRIVER_FLASH_STATUS_SUCCESS) {
        lfsStatus = LFS_ERR_OK;
    } else if (flashStatus == GMOS_DRIVER_FLASH_STATUS_WRITE_LOCKED) {
        lfsStatus = LFS_ERR_ROFS;
    } else {
        lfsStatus = LFS_ERR_IO;
    }

    // Optionally include verbose logging.
    if (GMOS_CONFIG_LITTLEFS_LOG_FLASH_IO) {
        GMOS_LOG_FMT (LOG_VERBOSE,
            "gmosDriverLittlefsErase (block = %4d) -> %d",
            block, lfsStatus);
    }
    return lfsStatus;
}

/*
 * Implement the LittleFS flash memory data sync function.
 */
static int gmosDriverLittlefsSync (const struct lfs_config* lfsConfig)
{
    (void) lfsConfig;
    return LFS_ERR_OK;
}

/*
 * Implement the LittleFS file system lock function.
 */
static int gmosDriverLittlefsLock (const struct lfs_config* lfsConfig)
{
    gmosDriverLittlefs_t* littlefs =
        (gmosDriverLittlefs_t*) lfsConfig->context;
    int lfsStatus;

    // File system locking must wait for initialisation to complete.
    if (littlefs->lfsState == GMOS_DRIVER_LITTLEFS_STATE_FAILED) {
        lfsStatus = LFS_ERR_IO;
    } else if (littlefs->lfsState == GMOS_DRIVER_LITTLEFS_STATE_INIT) {
        lfsStatus = LFS_ERR_AGAIN;
    }

    // Lock the file system for the duration of the transaction.
    else if (littlefs->lfsLocked == 0) {
        littlefs->lfsLocked = 1;
        lfsStatus = LFS_ERR_OK;
    } else {
        lfsStatus = LFS_ERR_AGAIN;
    }

    // Optionally include verbose logging.
    if (GMOS_CONFIG_LITTLEFS_LOG_FLASH_IO) {
        GMOS_LOG_FMT (LOG_VERBOSE,
            "gmosDriverLittlefsLock () -> %d", lfsStatus);
    }
    return lfsStatus;
}

/*
 * Implement the LittleFS file system unlock function.
 */
static int gmosDriverLittlefsUnlock (const struct lfs_config* lfsConfig)
{
    gmosDriverLittlefs_t* littlefs =
        (gmosDriverLittlefs_t*) lfsConfig->context;
    int lfsStatus;

    // Unlock the file system on completion of the transaction.
    if (littlefs->lfsLocked != 0) {
        littlefs->lfsLocked = 0;
        lfsStatus = LFS_ERR_OK;
    } else {
        littlefs->lfsState = GMOS_DRIVER_LITTLEFS_STATE_FAILED;
        lfsStatus = LFS_ERR_IO;
    }

    // Optionally include verbose logging.
    if (GMOS_CONFIG_LITTLEFS_LOG_FLASH_IO) {
        GMOS_LOG_FMT (LOG_VERBOSE,
            "gmosDriverLittlefsUnlock () -> %d", lfsStatus);
    }
    return lfsStatus;
}

/*
 * Perform flash device parameter extraction on startup.
 */
static inline bool gmosDriverLittlefsSetup (
    gmosDriverLittlefs_t* littlefs)
{
    struct lfs_config* lfsConfig = &(littlefs->lfsConfig);
    gmosDriverFlash_t* flashDevice = littlefs->flashDevice;
    bool initOk = false;

    // Wait for the flash device to complete its initialisation and then
    // set up the flash memory access parameters.
    if (flashDevice->flashState == GMOS_DRIVER_FLASH_STATE_IDLE) {
        lfsConfig->read_size = flashDevice->readSize;
        lfsConfig->prog_size = flashDevice->writeSize;
        lfsConfig->block_size = flashDevice->blockSize;
        lfsConfig->block_count = flashDevice->blockCount;
        initOk = true;
        GMOS_LOG_FMT (LOG_INFO,
            "LittleFS setup complete for %dK flash device.",
            lfsConfig->block_count * lfsConfig->block_size / 1024);
        GMOS_LOG_FMT (LOG_DEBUG,
            "LittleFS flash uses %d x %d byte blocks.",
            lfsConfig->block_count, lfsConfig->block_size);
        GMOS_LOG_FMT (LOG_DEBUG,
            "LittleFS miniumum read size %d, programming size %d.",
            lfsConfig->read_size, lfsConfig->prog_size);
    }
    return initOk;
}

/*
 * Implement the main LittleFS state machine task.
 */
static inline gmosTaskStatus_t gmosDriverLittlefsTaskFn (
    gmosDriverLittlefs_t* littlefs)
{
    uint8_t nextState = littlefs->lfsState;
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    int lfsStatus;
    int32_t delay;

    // Implement the main LittleFS state machine.
    switch (littlefs->lfsState) {

        // In the initialisation state, wait for the underlying flash
        // memory device to be ready before running file system
        // initialisation. This enables dynamic configuration of flash
        // memory parameters.
        case GMOS_DRIVER_LITTLEFS_STATE_INIT :
            if (gmosDriverLittlefsSetup (littlefs)) {
                nextState = GMOS_DRIVER_LITTLEFS_STATE_UNMOUNTED;
                taskStatus = GMOS_TASK_SUSPEND;
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (10));
            }
            break;

        // In the mounted state wait until it is time for the scheduled
        // garbage collection. This is skipped if the flash memory is
        // currently in read only mode.
        case GMOS_DRIVER_LITTLEFS_STATE_MOUNTED :
            delay = (int32_t)
                (littlefs->lfsGcTimestamp - gmosPalGetTimer ());
            GMOS_LOG_FMT (LOG_VERBOSE,
                "LittleFS in mounted state (delay %d).", delay);
            if (littlefs->lfsGcInterval == 0) {
                taskStatus = GMOS_TASK_SUSPEND;
            } else if (delay > 0) {
                taskStatus = GMOS_TASK_RUN_LATER ((uint32_t) delay);
            } else if (littlefs->flashDevice->writeEnable) {
                nextState = GMOS_DRIVER_LITTLEFS_STATE_RUNNING_GC;
            } else {
                littlefs->lfsGcTimestamp += GMOS_MS_TO_TICKS (
                    1000 * (uint32_t) (littlefs->lfsGcInterval));
            }
            break;

        // Attempt to run periodic garbage collection.
        case GMOS_DRIVER_LITTLEFS_STATE_RUNNING_GC :
            GMOS_LOG (LOG_DEBUG, "LittleFS running garbage collection.");
            lfsStatus = lfs_fs_gc (&(littlefs->lfsInstance));
            if ((lfsStatus == LFS_ERR_OK) || (lfsStatus == LFS_ERR_ROFS)) {
                littlefs->lfsGcTimestamp += GMOS_MS_TO_TICKS (
                    1000 * (uint32_t) (littlefs->lfsGcInterval));
                nextState = GMOS_DRIVER_LITTLEFS_STATE_MOUNTED;
            } else if (lfsStatus == LFS_ERR_AGAIN) {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (10));
            } else {
                nextState = GMOS_DRIVER_LITTLEFS_STATE_FAILED;
                GMOS_LOG_FMT (LOG_ERROR,
                    "LittleFS garbage collection failed with status %d.",
                    lfsStatus);
            }
            break;

        // Suspend processing in remaining states.
        default :
            taskStatus = GMOS_TASK_SUSPEND;
            break;
    }
    littlefs->lfsState = nextState;
    return taskStatus;
}
GMOS_TASK_DEFINITION (gmosDriverLittlefsTask,
    gmosDriverLittlefsTaskFn, gmosDriverLittlefs_t);

/*
 * Initialises a LittleFS file system driver on startup. This should be
 * called for each file system prior to accessing it via any of the
 * other API functions.
 */
bool gmosDriverLittlefsInit (gmosDriverLittlefs_t* littlefs,
    gmosDriverFlash_t* flashDevice, uint16_t gcInterval)
{
    struct lfs_config* lfsConfig = &(littlefs->lfsConfig);

    // Initialise the common file system fields.
    littlefs->flashDevice = flashDevice;
    littlefs->lfsState = GMOS_DRIVER_LITTLEFS_STATE_INIT;
    littlefs->lfsLocked = 0;
    littlefs->lfsGcInterval = gcInterval;

    // Zero out the configuration settings for defaults and backwards
    // compatibility.
    memset (lfsConfig, 0, sizeof (struct lfs_config));

    // Reference the main driver data structure as the context.
    lfsConfig->context = littlefs;

    // Set up the flash memory access functions.
    lfsConfig->read = gmosDriverLittlefsRead;
    lfsConfig->prog = gmosDriverLittlefsWrite;
    lfsConfig->erase = gmosDriverLittlefsErase;
    lfsConfig->sync = gmosDriverLittlefsSync;

    // Set up the file system lock functions.
    lfsConfig->lock = gmosDriverLittlefsLock;
    lfsConfig->unlock = gmosDriverLittlefsUnlock;

    // Static memory allocation is not currently supported.
    lfsConfig->read_buffer = NULL;
    lfsConfig->prog_buffer = NULL;
    lfsConfig->lookahead_buffer = NULL;

    // Select appropriate parameter settings.
    lfsConfig->cache_size = 64;
    lfsConfig->lookahead_size = 16;
    lfsConfig->block_cycles = 500;

    // Initialise the state machine task.
    gmosDriverLittlefsTask_start (&(littlefs->lfsTask), littlefs,
        GMOS_TASK_NAME_WRAPPER ("LittleFS Driver Task"));
    return true;
}

/*
 * Formats a LittleFS file system for subsequent use. This function
 * should be used instead of the standard LittleFS API format function.
 */
int gmosDriverLittlefsFormat (gmosDriverLittlefs_t* littlefs,
    uint32_t factoryResetKey)
{
    int lfsStatus;

    // Check the factory reset key before attempting to format the file
    // system.
    if (factoryResetKey != GMOS_CONFIG_LITTLEFS_FACTORY_RESET_KEY) {
        lfsStatus = LFS_ERR_INVAL;
    }

    // Only run formatting if the file system is unmounted.
    else switch (littlefs->lfsState) {
        case GMOS_DRIVER_LITTLEFS_STATE_UNMOUNTED :
            lfsStatus = lfs_format (
                &(littlefs->lfsInstance), &(littlefs->lfsConfig));
            break;

        // Generate appropriate status codes for other states.
        case GMOS_DRIVER_LITTLEFS_STATE_INIT :
            lfsStatus = LFS_ERR_AGAIN;
            break;
        case GMOS_DRIVER_LITTLEFS_STATE_FAILED :
            lfsStatus = LFS_ERR_IO;
            break;
        default :
            lfsStatus = LFS_ERR_INVAL;
            break;
    }
    return lfsStatus;
}

/*
 * Mounts a LittleFS file system for subsequent use. This function
 * should be used instead of the standard LittleFS API mount function.
 */
int gmosDriverLittlefsMount (gmosDriverLittlefs_t* littlefs)
{
    int lfsStatus;

    // Only run the mount request if the file system is unmounted.
    // After mounting the file system, the first garbage collection
    // cycle is scheduled after half the normal interval has elapsed.
    switch (littlefs->lfsState) {
        case GMOS_DRIVER_LITTLEFS_STATE_UNMOUNTED :
            lfsStatus = lfs_mount (
                &(littlefs->lfsInstance), &(littlefs->lfsConfig));
            if (lfsStatus == LFS_ERR_OK) {
                littlefs->lfsState = GMOS_DRIVER_LITTLEFS_STATE_MOUNTED;
                littlefs->lfsGcTimestamp =
                    gmosPalGetTimer () + GMOS_MS_TO_TICKS (
                        500 * (uint32_t) (littlefs->lfsGcInterval));
                gmosSchedulerTaskResume (&(littlefs->lfsTask));
            }
            break;

        // Generate appropriate status codes for other states.
        case GMOS_DRIVER_LITTLEFS_STATE_INIT :
            lfsStatus = LFS_ERR_AGAIN;
            break;
        case GMOS_DRIVER_LITTLEFS_STATE_FAILED :
            lfsStatus = LFS_ERR_IO;
            break;
        default :
            lfsStatus = LFS_ERR_INVAL;
            break;
    }
    return lfsStatus;
}

/*
 * Unmounts a LittleFS file system after use. This function should be
 * used instead of the standard LittleFS unmount function.
 */
int gmosDriverLittlefsUnmount (gmosDriverLittlefs_t* littlefs)
{
    int lfsStatus;

    // Only run the unmount request if the file system is mounted.
    switch (littlefs->lfsState) {
        case GMOS_DRIVER_LITTLEFS_STATE_MOUNTED :
            lfsStatus = lfs_unmount (&(littlefs->lfsInstance));
            if (lfsStatus == LFS_ERR_OK) {
                littlefs->lfsState = GMOS_DRIVER_LITTLEFS_STATE_UNMOUNTED;
            } else if (lfsStatus != LFS_ERR_AGAIN) {
                littlefs->lfsState = GMOS_DRIVER_LITTLEFS_STATE_FAILED;
            }
            break;

        // Generate appropriate status codes for other states.
        case GMOS_DRIVER_LITTLEFS_STATE_RUNNING_GC :
            lfsStatus = LFS_ERR_AGAIN;
            break;
        case GMOS_DRIVER_LITTLEFS_STATE_FAILED :
            lfsStatus = LFS_ERR_IO;
            break;
        default :
            lfsStatus = LFS_ERR_INVAL;
            break;
    }
    return lfsStatus;
}

/*
 * Sets the write enable status for the underlying file system flash
 * memory.
 */
int gmosDriverLittlefsWriteEnable (gmosDriverLittlefs_t* littlefs,
    bool writeEnable)
{
    gmosDriverFlash_t* flash = littlefs->flashDevice;
    gmosDriverFlashStatus_t flashStatus;
    int lfsStatus;

    // Ensure that the underlying flash memory is initialised.
    if (littlefs->lfsState == GMOS_DRIVER_LITTLEFS_STATE_INIT) {
        lfsStatus = LFS_ERR_AGAIN;
    }

    // Attempt to initiate a flash memory write enable request.
    else if (!gmosDriverFlashWriteEnable (flash, writeEnable)) {
        lfsStatus = LFS_ERR_AGAIN;
    }

    // Implement busy waiting on the transaction request.
    else {
        do {
            gmosSchedulerTaskBusyWait ();
            flashStatus = gmosDriverFlashComplete (flash, NULL);
        } while (flashStatus == GMOS_DRIVER_FLASH_STATUS_ACTIVE);
        if (flashStatus == GMOS_DRIVER_FLASH_STATUS_SUCCESS) {
            lfsStatus = LFS_ERR_OK;
        } else {
            lfsStatus = LFS_ERR_IO;
        }
    }
    return lfsStatus;
}

#endif // GMOS_CONFIG_LITTLEFS_ENABLE
