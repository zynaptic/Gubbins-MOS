/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2020 Zynaptic Limited
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
 * Implements the GubbinsMOS data buffer functionality.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-mempool.h"
#include "gmos-buffers.h"

// Use the standard 'memcpy' function for buffer data transfer.
#if GMOS_CONFIG_BUFFERS_USE_MEMCPY
#include <string.h>
#define BUFFER_COPY(_dst_, _src_, _size_) memcpy(_dst_, _src_, _size_)

// Use an inline byte based copy for buffer data transfer.
#else
#define BUFFER_COPY(_dst_, _src_, _size_) do {                         \
    uint8_t* dstPtr = (uint8_t*) _dst_;                                \
    uint8_t* srcPtr = (uint8_t*) _src_;                                \
    size_t count;                                                      \
    for (count = _size_; count != 0; count--) {                        \
        *(dstPtr++) = *(srcPtr++);                                     \
    }                                                                  \
} while (false)
#endif

/*
 * Copies a block of data to a linked list of segments, starting with
 * the specified segment and segment offset. This should always be
 * successful, since the wrapper functions will have checked for
 * boundary conditions.
 */
static void gmosBufferCopyToSegments (gmosMempoolSegment_t* segment,
    uint16_t segmentOffset, uint8_t* sourceData, uint16_t copySize)
{
    uint8_t* blockPtr;
    uint16_t blockSize;

    // Skip to the segment containing the start of the data block.
    while (segmentOffset >= GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE) {
        segment = segment->nextSegment;
        segmentOffset -= GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE;
    }

    // Copy the data to successive segments.
    while (true) {
        blockPtr = &(segment->data.bytes [segmentOffset]);
        blockSize = GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE - segmentOffset;
        if (blockSize > copySize) {
            blockSize = copySize;
        }
        BUFFER_COPY (&blockPtr, sourceData, blockSize);
        copySize -= blockSize;

        // Break out of the loop on completion.
        if (copySize == 0) {
            return;
        } else {
            segment = segment->nextSegment;
            segmentOffset = 0;
            sourceData += blockSize;
        }
    }
}

/*
 * Copies a block of data from a linked list of segments, starting with
 * the specified segment and segment offset. This should always be
 * successful, since the wrapper functions will have checked for
 * boundary conditions.
 */
static void gmosBufferCopyFromSegments (gmosMempoolSegment_t* segment,
    uint16_t segmentOffset, uint8_t* targetData, uint16_t copySize)
{
    uint8_t* blockPtr;
    uint16_t blockSize;

    // Skip to the segment containing the start of the data block.
    while (segmentOffset >= GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE) {
        segment = segment->nextSegment;
        segmentOffset -= GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE;
    }

    // Copy the data to successive segments.
    while (true) {
        blockPtr = &(segment->data.bytes [segmentOffset]);
        blockSize = GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE - segmentOffset;
        if (blockSize > copySize) {
            blockSize = copySize;
        }
        BUFFER_COPY (targetData, &blockPtr, blockSize);
        copySize -= blockSize;

        // Break out of the loop on completion.
        if (copySize == 0) {
            return;
        } else {
            segment = segment->nextSegment;
            segmentOffset = 0;
            targetData += blockSize;
        }
    }
}

/*
 * Performs a one-time initialisation of a GubbinsMOS data buffer. This
 * should be called during initialisation to set up the data buffer for
 * subsequent use.
 */
void gmosBufferInit (gmosBuffer_t* buffer)
{
    buffer->segmentList = NULL;
    buffer->bufferSize = 0;
}

/*
 * Resets a GubbinsMOS data buffer. All current data in the buffer is
 * discarded and then sufficient memory will be allocated to store the
 * requested number of bytes.
 */
bool gmosBufferReset (gmosBuffer_t* buffer, uint16_t size)
{
    bool resetOk = true;

    // Return all the current data segments to the free list.
    if (buffer->segmentList != NULL) {
        gmosMempoolFreeSegments (buffer->segmentList);
        buffer->segmentList = NULL;
        buffer->bufferSize = 0;
    }

    // Attempt to allocate the specified amount of memory.
    if (size > 0) {
        buffer->segmentList = gmosMempoolAllocSegments
            (1 + ((size - 1) / GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE));
        if (buffer->segmentList != NULL) {
            buffer->bufferSize = size;
        } else {
            resetOk = false;
        }
    }
    return resetOk;
}

/*
 * Extends a GubbinsMOS data buffer. This allocates additional memory
 * segments from the memory pool, increasing the overall size of the
 * buffer by the specified amount.
 */
bool gmosBufferExtend (gmosBuffer_t* buffer, uint16_t size)
{
    bool extendOk = true;
    uint16_t newBufferSize;
    uint16_t segmentCount;
    uint16_t newSegmentCount;
    gmosMempoolSegment_t** segmentPtr;
    gmosMempoolSegment_t* newSegments;

    // Extend by zero requests always succeed.
    if (size == 0) {
        return true;
    }

    // Extending beyond 2^16-1 bytes always fails.
    if (((uint32_t) buffer->bufferSize) + ((uint32_t) size) > 0xFFFF) {
        return false;
    }

    // Count the number of segments currently in the buffer.
    segmentCount = 0;
    segmentPtr = &(buffer->segmentList);
    while (*segmentPtr != NULL) {
        segmentCount += 1;
        segmentPtr = &((*segmentPtr)->nextSegment);
    }

    // Allocate additional memory segments if required.
    newBufferSize = buffer->bufferSize + size;
    newSegmentCount = 1 - segmentCount +
        ((newBufferSize - 1) / GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE);
    if (newSegmentCount != 0) {
        newSegments = gmosMempoolAllocSegments (newSegmentCount);
        if (newSegments != NULL) {
            *segmentPtr = newSegments;
        } else {
            extendOk = false;
        }
    }
    if (extendOk) {
        buffer->bufferSize = newBufferSize;
    }
    return extendOk;
}

/*
 * Writes a block of data to the buffer at the specified buffer offset.
 * The buffer must be large enough to hold all the data being written.
 */
bool gmosBufferWrite (gmosBuffer_t* buffer, uint16_t offset,
    uint8_t* writeData, uint16_t writeSize)
{
    bool writeOk;

    // Check for valid offset and size before initiating the copy.
    if (((uint32_t) offset) + ((uint32_t) writeSize) <=
        ((uint32_t) buffer->bufferSize)) {
        gmosBufferCopyToSegments (buffer->segmentList,
            offset, writeData, writeSize);
        writeOk = true;
    } else {
        writeOk = false;
    }
    return writeOk;
}

/*
 * Reads a block of data from the buffer at the specified buffer offset.
 * The buffer must be large enough to service the entire read request.
 */
bool gmosBufferRead (gmosBuffer_t* buffer, uint16_t offset,
    uint8_t* readData, uint16_t readSize)
{
    bool readOk;

    // Check for valid offset and size before initating the copy.
    if (((uint32_t) offset) + ((uint32_t) readSize) <=
        ((uint32_t) buffer->bufferSize)) {
        gmosBufferCopyFromSegments (buffer->segmentList,
            offset, readData, readSize);
        readOk = true;
    } else {
        readOk = false;
    }
    return readOk;
}

/*
 * Appends a block of data to the end of the buffer, increasing the
 * buffer length and automatically allocating additional memory pool
 */
bool gmosBufferAppend (gmosBuffer_t* buffer,
    uint8_t* writeData, uint16_t writeSize)
{
    uint16_t offset = buffer->bufferSize;
    bool appendOk;

    // Attempt to extend the buffer before initiating the copy.
    if (gmosBufferExtend (buffer, writeSize)) {
        gmosBufferCopyToSegments (buffer->segmentList,
            offset, writeData, writeSize);
        appendOk = true;
    } else {
        appendOk = false;
    }
    return appendOk;
}
