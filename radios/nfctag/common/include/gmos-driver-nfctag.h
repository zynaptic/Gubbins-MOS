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
 * This header defines the common API for controlling dynamic NFC tags.
 * The current implementation supports the reading and writing of NDEF
 * messages to one or more non volatile memory data areas on the NFC
 * tag. Mailbox based fast data transfer is not currently supported.
 */

#ifndef GMOS_DRIVER_NFC_TAG_H
#define GMOS_DRIVER_NFC_TAG_H

#include <stdint.h>
#include <stdbool.h>
#include "gmos-config.h"
#include "gmos-scheduler.h"
#include "gmos-events.h"
#include "gmos-buffers.h"

/**
 * Specify the size of low level driver NFC data transfer blocks.
 */
#ifndef GMOS_CONFIG_NFC_TAG_TRANSFER_BLOCK_SIZE
#define GMOS_CONFIG_NFC_TAG_TRANSFER_BLOCK_SIZE 32
#endif

/**
 * This enumeration specifies the NFC tag status values that may be
 * returned by NFC tag access functions.
 */
typedef enum {
    GMOS_DRIVER_NFC_TAG_STATUS_SUCCESS,
    GMOS_DRIVER_NFC_TAG_STATUS_FATAL_ERROR,
    GMOS_DRIVER_NFC_TAG_STATUS_NOT_READY,
    GMOS_DRIVER_NFC_TAG_STATUS_ACCESS_PROTECTED,
    GMOS_DRIVER_NFC_TAG_STATUS_INVALID_ADDRESS
} gmosDriverNfcTagStatus_t;

/**
 * Defines the NFC tag radio specific I/O state data structure. The full
 * type definition must be provided by the associated radio abstraction
 * layer.
 */
typedef struct gmosRalNfcTagState_t gmosRalNfcTagState_t;

/**
 * Defines the NFC tag radio specific I/O configuration options. The
 * full type definition must be provided by the associated radio
 * abstraction layer.
 */
typedef struct gmosRalNfcTagConfig_t gmosRalNfcTagConfig_t;

/**
 * Defines the GubbinsMOS NFC tag state data structure that is used for
 * managing the low level I/O for a single dynamic NFC tag.
 */
typedef struct gmosDriverNfcTag_t {

    // This is an opaque pointer to the NFC tag radio abstraction layer
    // data structure that is used for accessing the NFC tag radio
    // hardware. The data structure will be device specific.
    gmosRalNfcTagState_t* ralData;

    // This is an opaque pointer to the NFC tag radio abstraction layer
    // configuration data structure that is used for setting up the
    // NFC tag hardware. The data structure will be device specific.
    const gmosRalNfcTagConfig_t* ralConfig;

    // Specify the data buffer used for read and write transfers.
    gmosBuffer_t* dataBuffer;

    // This is a pointer to the intermediate data transfer block
    // provided by the radio abstraction layer.
    uint8_t* transferBlock;

    // This is the NFC tag driver worker task that implements the NFC
    // tag access state machine.
    gmosTaskState_t workerTask;

    // This is a completion event which is used for waking an associated
    // application task on transaction completion.
    gmosEvent_t completionEvent;

    // This is the base address of the current NFC data transfer.
    uint16_t baseAddr;

    // This is the size of the current NFC data transfer.
    uint16_t dataSize;

    // This is the NFC device data area used for the NFC data transfer.
    uint8_t dataArea;

    // This is the current NFC tag driver state.
    uint8_t nfcTagState;

} gmosDriverNfcTag_t;

/**
 * Provides a radio hardware configuration setup macro to be used when
 * allocating an NFC tag I/O data structure. Assigning this macro to
 * an NFC tag I/O data structure on declaration will configure the NFC
 * tag driver to use the radio specific configuration. Refer to the
 * device specific NFC tag implementation for full details of the radio
 * data area and the NFC tag configuration options.
 * @param _ralData_ This is the NFC tag radio abstraction layer data
 *     structure that is to be used for accessing the radio specific
 *     hardware.
 * @param _ralConfig_ This is a hardware specific NFC tag configuration
 *     data structure that defines a set of fixed configuration options
 *     to be used with the NFC tag.
 */
#define GMOS_DRIVER_NFC_TAG_RAL_CONFIG(_ralData_, _ralConfig_) {       \
    .ralData = _ralData_, .ralConfig = _ralConfig_ }

/**
 * Initialises an NFC tag using the supplied radio configuration. The
 * radio specific options should already have been populated using the
 * 'GMOS_DRIVER_NFC_TAG_RAL_CONFIG' macro.
 * @param nfcTag This is the NFC tag data structure that will be used
 *     for managing the low level I/O for a single NFC tag.
 * @param clientTask This is the client task which will automatically
 *     be resuumed on completion of an NFC transaction.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully completing the initialisation process and 'false'
 *     otherwise.
 */
bool gmosDriverNfcTagInit (
    gmosDriverNfcTag_t* nfcTag, gmosTaskState_t* clientTask);

/**
 * Gets the data area information for a specific NFC tag data area.
 * @param nfcTag This is the NFC tag data structure that will be used
 *     to access the associated data area information.
 * @param dataArea This is the NFC tag data area for which the
 *     associated data area information is being accessed. A value of
 *     zero selects the default data area and other values are RAL
 *     specific.
 * @param dataAreaSize This is a pointer to a 16-bit integer value,
 *     which on successful completion will contain the size of the data
 *     area, expressed as an integer number of bytes.
 * @return Returns a status value which will indicate successful
 *     completion of the request or the reason for failure.
 */
gmosDriverNfcTagStatus_t gmosDriverNfcTagGetAreaInfo (
    gmosDriverNfcTag_t* nfcTag, uint8_t dataArea,
    uint16_t* dataAreaSize);

/**
 * Initiates an NFC tag persistent data area read request.
 * @param nfcTag This is the NFC tag data structure for which the NFC
 *     tag persistent data area read request is being initiated.
 * @param dataArea This is the NFC tag data area for which the
 *     read request is being initiated. A value of zero selects the
 *     default data area and other values are RAL specific.
 * @param readAddr This is the address of the start of the data area
 *     memory that is being requested, offset from the start of the data
 *     area.
 * @param readSize This specifies the size of the read data request,
 *     expressed as an integer number of bytes.
 * @param readBuffer This is a pointer to a read data buffer, which on
 *     successful completion will contain a copy of the data read back
 *     from the data area.
 * @return Returns a status value which will indicate successful
 *     initiation of the request or the reason for failure.
 */
gmosDriverNfcTagStatus_t gmosDriverNfcTagRead (
    gmosDriverNfcTag_t* nfcTag, uint8_t dataArea,
    uint16_t readAddr, uint16_t readSize, gmosBuffer_t* readBuffer);

/**
 * Initiates an NFC tag persistent data area write request.
 * @param nfcTag This is the NFC tag data structure for which the NFC
 *     tag persistent data area write request is being initiated.
 * @param dataArea This is the NFC tag data area for which the
 *     write request is being initiated. A value of zero selects the
 *     default data area and other values are RAL specific.
 * @param writeAddr This is the address of the start of the data area
 *     memory that is being updated, offset from the start of the data
 *     area.
 * @param writeBuffer This is a pointer to a write data buffer, which
 *     contains the data to be written to the data area. On completion
 *     the contents of the buffer will be reset.
 * @return Returns a status value which will indicate successful
 *     initiation of the request or the reason for failure.
 */
gmosDriverNfcTagStatus_t gmosDriverNfcTagWrite (
    gmosDriverNfcTag_t* nfcTag, uint8_t dataArea,
    uint16_t writeAddr, gmosBuffer_t* writeBuffer);

/**
 * Completes an NFC tag read or write transaction, indicating the status
 * of the transaction. This may indicate successful completion, the
 * reason for failure or a 'not ready' status which implies that the
 * transaction is still active and completion should be requested again
 * at a later time.
 * @param nfcTag This is the NFC tag data structure for which the NFC
 *     tag transaction completion is being requested.
 * @return Returns a status value which will indicate successful
 *     completion of the transaction or the reason for failure.
 */
gmosDriverNfcTagStatus_t gmosDriverNfcTagComplete (
    gmosDriverNfcTag_t* nfcTag);

/**
 * Initialises the radio abstraction layer for a given NFC tag. Refer
 * to the radio specific NFC tag implementation for details of the radio
 * data area and the NFC tag configuration options. This function is
 * called automatically by the 'gmosDriverNfcTagInit' function.
 * @param nfcTag This is the NFC tag data structure that will be used
 *     for managing the low level I/O for a single NFC tag.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully completing the initialisation process and 'false'
 *     otherwise.
 */
bool gmosDriverNfcTagRalInit (gmosDriverNfcTag_t* nfcTag);

/**
 * Initiates a RAL read transaction for the specified tag data area. The
 * resulting data will be written to the transfer block memory allocated
 * by the RAL.
 * @param nfcTag This is the NFC tag data structure for which the NFC
 *     tag persistent data area read request is being initiated. This
 *     includes valid settings for the associated data area and transfer
 *     block memory.
 * @param readAddr This is the address of the start of the data area
 *     memory that is being requested, offset from the start of the data
 *     area.
 * @param readSize This specifies the size of the read data request,
 *     expressed as an integer number of bytes.
 * @return Returns a status value which will indicate successful
 *     initiation of the request or the reason for failure.
 */
gmosDriverNfcTagStatus_t gmosDriverNfcTagRalReadStart (
    gmosDriverNfcTag_t* nfcTag, uint16_t readAddr, uint16_t readSize);

/**
 * Provides a callback handler for notification of RAL read transaction
 * completion.
 * @param nfcTag This is the NFC tag data structure for which the NFC
 *     tag persistent data area read request is being completed.
 * @param status This specifies the completion status for the RAL read
 *     transaction.
 */
void gmosDriverNfcTagRalReadComplete (gmosDriverNfcTag_t* nfcTag,
    gmosDriverNfcTagStatus_t status);

/**
 * Initiates a RAL write transaction for the specified tag data area.
 * The written data will be sourced from the transfer block memory
 * allocated by the RAL.
 * @param nfcTag This is the NFC tag data structure for which the NFC
 *     tag persistent data area write request is being initiated. This
 *     includes valid settings for the associated data area and transfer
 *     block memory.
 * @param writeAddr This is the address of the start of the data area
 *     memory that is being written, offset from the start of the data
 *     area.
 * @param writeSize This specifies the size of the write data request,
 *     expressed as an integer number of bytes.
 * @return Returns a status value which will indicate successful
 *     initiation of the request or the reason for failure.
 */
gmosDriverNfcTagStatus_t gmosDriverNfcTagRalWriteStart (
    gmosDriverNfcTag_t* nfcTag, uint16_t writeAddr, uint16_t writeSize);

/**
 * Provides a callback handler for notification of RAL write transaction
 * completion.
 * @param nfcTag This is the NFC tag data structure for which the NFC
 *     tag persistent data area write request is being completed.
 * @param status This specifies the completion status for the RAL write
 *     transaction.
 */
void gmosDriverNfcTagRalWriteComplete (gmosDriverNfcTag_t* nfcTag,
    gmosDriverNfcTagStatus_t status);

#endif // GMOS_DRIVER_NFC_TAG_H
