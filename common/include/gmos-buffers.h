/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2020-2021 Zynaptic Limited
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
 * This header defines the API for GubbinsMOS data buffer support.
 */

#ifndef GMOS_BUFFERS_H
#define GMOS_BUFFERS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "gmos-mempool.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * Defines the GubbinsMOS data buffer structure which is used for
 * managing an individual data buffer.
 */
typedef struct gmosBuffer_t {

    // This is a pointer to the start of the data buffer segment list.
    gmosMempoolSegment_t* segmentList;

    // This specifies the current size of the data buffer.
    uint16_t bufferSize;

} gmosBuffer_t;

/**
 * Provides a compile time initialisation macro for a GubbinsMOS data
 * buffer. Assigning this macro value to a data buffer variable on
 * declaration may be used instead of a call to the 'gmosBufferInit'
 * function to set up a data buffer for subsequent use.
 */
#define GMOS_BUFFER_INIT()                                             \
    { NULL, 0 }

/**
 * Performs a one-time initialisation of a GubbinsMOS data buffer. This
 * should be called during initialisation to set up the data buffer for
 * subsequent use.
 * @param buffer This is the data buffer structure that is to be
 *     initialised.
 */
void gmosBufferInit (gmosBuffer_t* buffer);

/**
 * Resets a GubbinsMOS data buffer. All current data in the buffer is
 * discarded and then sufficient memory will be allocated to store the
 * requested number of bytes.
 * @param buffer This is the data buffer that is to be reset.
 * @param size This is the number of bytes which should be allocated for
 *     storage in the data buffer. A value of zero may be used to
 *     release all the allocated memory.
 * @return Returns a boolean value which will be set to 'true' if the
 *     requested amount of memory was allocated to the buffer and
 *     'false' if there was insufficient memory available.
 */
bool gmosBufferReset (gmosBuffer_t* buffer, uint16_t size);

/**
 * Extends a GubbinsMOS data buffer. This allocates additional memory
 * segments from the memory pool, increasing the overall size of the
 * buffer by the specified amount.
 * @param buffer This is the data buffer that is to be extended.
 * @param size This is the number of additional bytes which should be
 *     allocated for storage in the data buffer.
 * @return Returns a boolean value which will be set to 'true' if the
 *     requested amount of memory was allocated to the buffer and
 *     'false' if there was insufficient memory available.
 */
bool gmosBufferExtend (gmosBuffer_t* buffer, uint16_t size);

/**
 * Resizes a GubbinsMOS data buffer to the specified length. If the
 * effect of the resizing operation is to increase the buffer length,
 * additional memory segments will be allocated from the memory pool as
 * required. If the effect of the resizing operation is to decrease the
 * buffer length, all data at the end of the buffer will be discarded
 * and memory segments will be returned to the memory pool as required.
 * @param buffer This is the data buffer that is to be resized.
 * @param size This is the number of bytes which should be available for
 *     storage in the data buffer after resizing. A value of zero may be
 *     used to release all the allocated memory.
 * @return Returns a boolean value which will be set to 'true' if the
 *     requested amount of memory was allocated to the buffer and
 *     'false' if there was insufficient memory available.
 */
bool gmosBufferResize (gmosBuffer_t* buffer, uint16_t size);

/**
 * Writes a block of data to the buffer at the specified buffer offset.
 * The buffer must be large enough to hold all the data being written.
 * @param buffer This is the buffer which is to be updated.
 * @param offset This is the offset within the buffer at which the new
 *     data is to be written.
 * @param writeData This is a pointer to the block of data that is to
 *     be written to the data buffer.
 * @param writeSize This specifies the number of bytes that are to be
 *     written to the data buffer.
 * @return Returns a boolean value which will be set to 'true' if the
 *     data was written to the buffer and 'false' if the buffer was
 *     not large enough to hold the new data.
 */
bool gmosBufferWrite (gmosBuffer_t* buffer, uint16_t offset,
    uint8_t* writeData, uint16_t writeSize);

/**
 * Reads a block of data from the buffer at the specified buffer offset.
 * The buffer must be large enough to service the entire read request.
 * @param buffer This is the buffer which is to be accessed.
 * @param offset This is the offset within the buffer at which the data
 *     is to be accessed.
 * @param readData This is a pointer to a block of memory that is to be
 *     updated with the data read from the buffer.
 * @param readSize This specifies the number of bytes that are to be
 *     copied from the data buffer to the read data area.
 * @return Returns a boolean value which will be set to 'true' if the
 *     data was read from the buffer and 'false' if the buffer was
 *     not large enough to service the entire read request.
 */
bool gmosBufferRead (gmosBuffer_t* buffer, uint16_t offset,
    uint8_t* readData, uint16_t readSize);

/**
 * Appends a block of data to the end of the buffer, increasing the
 * buffer length and automatically allocating additional memory pool
 * segments if required.
 * @param buffer This is the buffer which is to be updated.
 * @param writeData This is a pointer to the block of data that is to
 *     be appended to the data buffer.
 * @param writeSize This specifies the number of bytes that are to be
 *     appended to the data buffer.
 * @return Returns a boolean value which will be set to 'true' if the
 *     data was appended to the buffer and 'false' if an attempt to
 *     allocate additional memory to the buffer failed.
 */
bool gmosBufferAppend (gmosBuffer_t* buffer,
    uint8_t* writeData, uint16_t writeSize);

/**
 * Gets a reference to the buffer segment that contains data at the
 * specified buffer offset.
 * @param buffer This is the buffer which is to be accessed.
 * @param dataOffset This is the offset within the buffer for which the
 *     associated memory segment is being accessed.
 * @return Returns a memory pool segment pointer to the buffer segment
 *     that contains data at the specified offset, or a null reference
 *     if the specified offset is out of range.
 */
gmosMempoolSegment_t* gmosBufferGetSegment (gmosBuffer_t* buffer,
    uint16_t dataOffset);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // GMOS_BUFFERS_H
