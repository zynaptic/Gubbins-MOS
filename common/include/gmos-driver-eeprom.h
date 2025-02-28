/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2020-2025 Zynaptic Limited
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
 * This header defines the common API for accessing the standard EEPROM
 * memory on a target platform. The model assumes support for fast, non
 * blocking reads with slow asynchronous writes. This maps directly to
 * most on-chip memory mapped EEPROM, or SPI and I2C based EEPROM where
 * the entire EEPROM contents are cached locally in RAM. EEPROM records
 * are stored in tag, length and value form and use a linear search for
 * access, which trades access time for compact representation. The
 * initial implementation does not support record deletion, so all
 * created EEPROM records will persist until a factory reset occurs.
 */

#ifndef GMOS_DRIVER_EEPROM_H
#define GMOS_DRIVER_EEPROM_H

#include <stdint.h>
#include "gmos-config.h"
#include "gmos-scheduler.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * Define the overall size of the EEPROM record header.
 */
#define GMOS_DRIVER_EEPROM_HEADER_SIZE \
    (GMOS_CONFIG_EEPROM_TAG_SIZE + GMOS_CONFIG_EEPROM_LENGTH_SIZE)

/**
 * Define the EEPROM tag value that is used to indicate the end of the
 * EEPROM record list. This should always be paired with a length field
 * of zero.
 */
#define GMOS_DRIVER_EEPROM_TAG_END_MARKER \
    ((1 << (8 * GMOS_CONFIG_EEPROM_TAG_SIZE)) - 1)

/**
 * Define a reserved value that may subsequently be used to indicate a
 * deleted record in the EEPROM record list.
 */
#define GMOS_DRIVER_EEPROM_TAG_FREE_SPACE \
    ((1 << (8 * GMOS_CONFIG_EEPROM_TAG_SIZE)) - 2)

/**
 * Define an invalid EEPROM tag value for use in situations where EEPROM
 * access is disabled.
 */
#define GMOS_DRIVER_EEPROM_TAG_INVALID \
    GMOS_DRIVER_EEPROM_TAG_END_MARKER

/**
 * Define the EEPROM factory reset key value.
 */
#define GMOS_DRIVER_EEPROM_FACTORY_RESET_KEY 0x706E6DF1

/**
 * This selects the appropriate EEPROM tag type based on the configured
 * tag size.
 */
#if (GMOS_CONFIG_EEPROM_TAG_SIZE == 1)
typedef uint8_t gmosDriverEepromTag_t;
#elif (GMOS_CONFIG_EEPROM_TAG_SIZE == 2)
typedef uint16_t gmosDriverEepromTag_t;
#elif (GMOS_CONFIG_EEPROM_TAG_SIZE == 3)
typedef uint32_t gmosDriverEepromTag_t;
#elif (GMOS_CONFIG_EEPROM_TAG_SIZE == 4)
typedef uint32_t gmosDriverEepromTag_t;
#else
#error "Unsupported EEPROM tag size."
#endif

/**
 * This enumeration specifies the EEPROM status values that may be
 * returned by EEPROM access functions.
 */
typedef enum {
    GMOS_DRIVER_EEPROM_STATUS_SUCCESS,
    GMOS_DRIVER_EEPROM_STATUS_FATAL_ERROR,
    GMOS_DRIVER_EEPROM_STATUS_NOT_READY,
    GMOS_DRIVER_EEPROM_STATUS_NO_RECORD,
    GMOS_DRIVER_EEPROM_STATUS_OUT_OF_MEMORY,
    GMOS_DRIVER_EEPROM_STATUS_TAG_EXISTS,
    GMOS_DRIVER_EEPROM_STATUS_FORMATTING_ERROR,
    GMOS_DRIVER_EEPROM_STATUS_INVALID_TAG,
    GMOS_DRIVER_EEPROM_STATUS_INVALID_LENGTH,
    GMOS_DRIVER_EEPROM_STATUS_INVALID_RESET_KEY
} gmosDriverEepromStatus_t;

/**
 * Defines the function prototype to be used for EEPROM transaction
 * complete callbacks.
 * @param status This is the completion status for the EEPROM
 *     transaction.
 * @param callbackData This is an opaque pointer to the data item that
 *     was passed as the callback data parameter when initiating the
 *     transaction.
 */
typedef void (*gmosPalEepromCallback_t) (
    gmosDriverEepromStatus_t status, void* callbackData);

/**
 * Defines the platform specific EEPROM state data structure. The full
 * type definition must be provided by the associated platform
 * abstraction layer.
 */
typedef struct gmosPalEepromState_t gmosPalEepromState_t;

/**
 * Defines the platform specific EEPROM configuration options. The full
 * type definition must be provided by the associated platform
 * abstraction layer.
 */
typedef struct gmosPalEepromConfig_t gmosPalEepromConfig_t;

/**
 * Defines the GubbinsMOS EEPROM driver state data structure that is
 * used for managing a platform specific EEPROM driver implementation.
 * The full type definition must be provided by the associated platform
 * specific library.
 */
#if GMOS_CONFIG_EEPROM_PLATFORM_LIBRARY
typedef struct gmosDriverEeprom_t gmosDriverEeprom_t;

/**
 * Defines the GubbinsMOS EEPROM driver state data structure that is
 * used for managing the low level hardware for a single EEPROM driver.
 */
#else // GMOS_CONFIG_EEPROM_PLATFORM_LIBRARY
typedef struct gmosDriverEeprom_t {

    // This is an opaque pointer to the platform abstraction layer data
    // structure that is used for accessing the EEPROM hardware. The
    // data structure will be platform specific.
    gmosPalEepromState_t* palData;

    // This is an opaque pointer to the platform abstraction layer
    // configuration data structure that is used for setting up the
    // EEPROM hardware. The data structure will be platform specific.
    const gmosPalEepromConfig_t* palConfig;

    // This is the memory mapped EEPROM base address used for EEPROM
    // read accesses. It is set by the platform abstraction layer during
    // initialisation.
    uint8_t* baseAddress;

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

    // This is the EEPROM size as an integer number of bytes not
    // exceeding 64K. It is set by the platform abstraction layer during
    // initialisation.
    uint16_t memSize;

    // This is the current EEPROM write transaction offset.
    uint16_t writeOffset;

    // This is the current EEPROM write transaction size.
    uint16_t writeSize;

    // This is the current EEPROM driver state.
    uint8_t eepromState;

    // This is the current EEPROM record write header value.
    uint8_t writeHeader [GMOS_DRIVER_EEPROM_HEADER_SIZE];

} gmosDriverEeprom_t;
#endif // GMOS_CONFIG_EEPROM_PLATFORM_LIBRARY

/**
 * Provides a platform configuration setup macro to be used when
 * allocating an EEPROM driver data structure. Assigning this macro to
 * an EEPROM driver data structure on declaration will configure the
 * EEPROM driver to use the platform specific configuration.
 * @param _palData_ This is the EEPROM platform abstraction layer data
 *     structure that is to be used for accessing the platform specific
 *     hardware.
 * @param _palConfig_ This is a platform specific EEPROM configuration
 *     data structure that defines a set of fixed configuration options
 *     to be used with the EEPROM driver.
 */
#define GMOS_DRIVER_EEPROM_PAL_CONFIG(_palData_, _palConfig_)          \
    { _palData_, _palConfig_, NULL, NULL, NULL, NULL, {0}, 0, 0, 0, 0, {0} }

/**
 * Initialises the EEPROM driver. This should be called once on startup
 * in order to initialise the EEPROM driver state. If required, it may
 * also perform a factory reset on the EEPROM contents, invalidating all
 * of the current EEPROM records.
 * @param eeprom This is a pointer to the EEPROM driver data structure
 *     for the EEPROM that is to be initialised.
 * @param isMainInstance This is a boolean flag, which when set to
 *     'true' indicates that this is the main EEPROM instance that will
 *     be used for storing system information.
 * @param factoryReset This is a boolean flag, which when set to 'true'
 *     will initialise the EEPROM to its factory reset state,
 *     invalidating all of the current EEPROM records.
 * @param factoryResetKey This is the factory reset key. If performing a
 *     factory reset, this must be set to the correct key value.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully initialising the EEPROM and 'false' otherwise.
 */
bool gmosDriverEepromInit (gmosDriverEeprom_t* eeprom,
    bool isMainInstance, bool factoryReset, uint32_t factoryResetKey);

/**
 * Accesses the main EEPROM instance to be used for storing system
 * information. For most configurations this will be the only EEPROM
 * on the device.
 * @return Returns the main EEPROM instance that is to be used for
 *     storing system information, or a null reference if no main EEPROM
 *     instance has been specified.
 */
gmosDriverEeprom_t* gmosDriverEepromGetInstance (void);

/**
 * Creates a new EEPROM data record with the specified tag, length and
 * default value. This will fail if a record with the specified tag
 * already exists.
 * @param eeprom This is the EEPROM device for which the new data record
 *     is being created.
 * @param recordTag This is the tag which will be used to uniquely
 *     identify the EEPROM data record.
 * @param defaultValue This is a pointer to a byte array that contains
 *     the default value to be used when creating the EEPROM record. It
 *     must remain valid until the record creation process is complete.
 *     If this is set to a null reference, the record data area will be
 *     initialised to an all zero value.
 * @param recordLength This is the length of the EEPROM data record to
 *     be created.
 * @param callbackHandler This is the callback handler that will be
 *     called on transaction completion. If a null callback handler is
 *     specified, this call will block until completion.
 * @param callbackData This is a pointer to an opaque data item that
 *     will be passed back as a callback handler parameter.
 * @return Returns the status of the transaction request. On returning
 *     success, the EEPROM driver will process the transaction and call
 *     the callback handler on completion.
 */
gmosDriverEepromStatus_t gmosDriverEepromRecordCreate (
    gmosDriverEeprom_t* eeprom, gmosDriverEepromTag_t recordTag,
    uint8_t* defaultValue, uint16_t recordLength,
    gmosPalEepromCallback_t callbackHandler, void* callbackData);

/**
 * Writes data to an EEPROM data record, copying it from the specified
 * write data byte array.
 * @param eeprom This is a pointer to the EEPROM driver data structure
 *     for the EEPROM being accessed.
 * @param recordTag This is the unique tag which is used to identify the
 *     EEPROM record which is to be updated.
 * @param writeData This is a pointer to the write data array which is
 *     to be copied into the EEPROM. It must remain valid until the
 *     write transaction has completed.
 * @param writeSize This specifies the number of bytes which are to be
 *     written to the EEPROM record. This must match the stored record
 *     length.
 * @param callbackHandler This is the callback handler that will be
 *     called on transaction completion. If a null callback handler is
 *     specified, this call will block until completion.
 * @param callbackData This is a pointer to an opaque data item that
 *     will be passed back as a callback handler parameter.
 * @return Returns the status of the transaction request. On returning
 *     success, the EEPROM driver will process the transaction and call
 *     the callback handler on completion.
 */
gmosDriverEepromStatus_t gmosDriverEepromRecordWrite (
    gmosDriverEeprom_t* eeprom, gmosDriverEepromTag_t recordTag,
    uint8_t* writeData, uint16_t writeSize,
    gmosPalEepromCallback_t callbackHandler, void* callbackData);

/**
 * Reads data from an EEPROM data record, storing it in the specified
 * read data byte array.
 * @param eeprom This is a pointer to the EEPROM driver data structure
 *     for the EEPROM being accessed.
 * @param recordTag This is the unique tag which is used to identify the
 *     EEPROM record which is to be read back.
 * @param readData This is a pointer to the read data array which is to
 *     be populated with the data read back from the EEPROM.
 * @param readOffset This is the offset within the EEPROM record from
 *     which the EEPROM data is to be read back.
 * @param readSize This specifies the number of bytes which are to be
 *     read back from the EEPROM record.
 * @return Returns the status of the read request. There is no delay
 *     when reading from the EEPROM, so the read data array will be
 *     updated prior to returning a successful status value.
 */
gmosDriverEepromStatus_t gmosDriverEepromRecordRead (
    gmosDriverEeprom_t* eeprom, gmosDriverEepromTag_t recordTag,
    uint8_t* readData, uint16_t readOffset, uint16_t readSize);

/**
 * Reads all the data from an EEPROM data record, storing it in the
 * specified read data byte array.
 * @param eeprom This is a pointer to the EEPROM driver data structure
 *     for the EEPROM being accessed.
 * @param recordTag This is the unique tag which is used to identify the
 *     EEPROM record which is to be read back.
 * @param readData This is a pointer to the read data array which is to
 *     be populated with the data read back from the EEPROM.
 * @param readMaxSize This specifies the size of the target read data
 *     array. It must be large enough to hold the entire EEPROM record.
 * @param readSize This is a pointer to a read data size variable which
 *     will be populated with the EEPROM record size on successful
 *     completion.
 * @return Returns the status of the read request. There is no delay
 *     when reading from the EEPROM, so the read data array will be
 *     updated prior to returning a successful status value.
 */
gmosDriverEepromStatus_t gmosDriverEepromRecordReadAll (
    gmosDriverEeprom_t* eeprom, gmosDriverEepromTag_t recordTag,
    uint8_t* readData, uint16_t readMaxSize, uint16_t* readSize);

/**
 * Initialises the EEPROM driver platform abstraction layer. This will
 * be called once on startup in order to initialise the platform
 * specific EEPROM driver state.
 * @param eeprom This is a pointer to the EEPROM driver data structure
 *     for the EEPROM that is to be initialised.
 * @return Returns a boolean value which will be set to 'true' on
 *     successful initialisation and 'false' otherwise.
 */
bool gmosPalEepromInit (gmosDriverEeprom_t* eeprom);

/**
 * Initiates a write operation for the EEPROM platform abstraction
 * layer, using the specified address offset within the EEPROM.
 * @param eeprom This is a pointer to the EEPROM driver data structure
 *     for the EEPROM being accessed.
 * @param addrOffset This is the offset within the EEPROM at which the
 *     first bytes of the write data should be written.
 * @param writeData This is a pointer to a byte array which contains the
 *     data to be written to the EEPROM. It must remain valid for the
 *     duration of the write operation. If set to a null reference, the
 *     corresponding EEPROM bytes will be set to zero instead.
 * @param writeSize This specifies the number of bytes which are to be
 *     written to the EEPROM.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully initiating the write request and 'false' if a write
 *     transaction can not be initiated at this time.
 */
bool gmosPalEepromWriteData (gmosDriverEeprom_t* eeprom,
    uint16_t addrOffset, const uint8_t* writeData, uint16_t writeSize);

/**
 * Polls the EEPROM platform abstraction layer to determine if an EEPROM
 * write transaction is currently in progress. It should be called
 * periodically while a write transaction is active in order to progress
 * the write operation.
 * @param eeprom This is a pointer to the EEPROM driver data structure
 *     for the EEPROM being accessed.
 * @return Returns a boolean value which will be set to 'true' if an
 *     EEPROM write transaction is currently in progress and 'false'
 *     otherwise.
 */
bool gmosPalEepromWritePoll (gmosDriverEeprom_t* eeprom);

// Define the platform abstraction layer data structures for EEPROM
// software emulation when a dedicated EEPROM is not available.
#if GMOS_CONFIG_EEPROM_SOFTWARE_EMULATION

/**
 * Defines the platform specific EEPROM driver configuration settings
 * data structure for software emulation.
 */
typedef struct gmosPalEepromConfig_t {

    // This is the memory mapped base address used for emulated EEPROM
    // read accesses.
    uint8_t* memAddress;

    // This is the emulated EEPROM size as an integer number of bytes
    // not exceeding 64K.
    uint16_t memSize;

} gmosPalEepromConfig_t;

/**
 * Defines the platform specific EEPROM driver dynamic data structure
 * for software emulation.
 */
typedef struct gmosPalEepromState_t {

} gmosPalEepromState_t;

#endif // GMOS_CONFIG_EEPROM_SOFTWARE_EMULATION

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // GMOS_DRIVER_EEPROM_H
