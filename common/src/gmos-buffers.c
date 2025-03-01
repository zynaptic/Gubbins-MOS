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
 * Discards the entire contents of a buffer.
 */
static void gmosBufferDiscardContents (gmosBuffer_t* buffer)
{
    if (buffer->segmentList != NULL) {
        gmosMempoolFreeSegments (buffer->segmentList);
        buffer->segmentList = NULL;
        buffer->bufferSize = 0;
        buffer->bufferOffset = 0;
    }
}

/*
 * Copies a block of data to a linked list of segments, starting with
 * the specified segment and segment offset. This should always be
 * successful, since the wrapper functions will have checked for
 * boundary conditions.
 */
static void gmosBufferCopyToSegments (gmosMempoolSegment_t* segment,
    uint_fast16_t segmentOffset, const uint8_t* sourceData,
    uint_fast16_t copySize)
{
    uint8_t* blockPtr;
    uint_fast16_t blockSize;

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
        BUFFER_COPY (blockPtr, sourceData, blockSize);
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
    uint_fast16_t segmentOffset, uint8_t* targetData,
    uint_fast16_t copySize)
{
    uint8_t* blockPtr;
    uint_fast16_t blockSize;

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
        BUFFER_COPY (targetData, blockPtr, blockSize);
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
    buffer->bufferOffset = 0;
}

/*
 * Gets the current allocated size of the buffer.
 */
uint16_t gmosBufferGetSize (gmosBuffer_t* buffer)
{
    return buffer->bufferSize;
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
    gmosBufferDiscardContents (buffer);

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
 * Increases the size of the buffer to the specified size, adding
 * capacity to the end of the buffer.
 */
static bool gmosBufferIncrSizeEnd (
    gmosBuffer_t* buffer, uint_fast16_t size)
{
    bool extendOk = true;
    uint_fast16_t segmentCount;
    uint_fast16_t newSegmentCount;
    gmosMempoolSegment_t** segmentPtr;
    gmosMempoolSegment_t* newSegments;

    // Count the number of segments currently in the buffer.
    segmentCount = 0;
    segmentPtr = &(buffer->segmentList);
    while (*segmentPtr != NULL) {
        segmentCount += 1;
        segmentPtr = &((*segmentPtr)->nextSegment);
    }

    // Allocate additional memory segments if required.
    newSegmentCount = 1 - segmentCount +
        ((buffer->bufferOffset + size - 1) /
        GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE);
    if (newSegmentCount != 0) {
        newSegments = gmosMempoolAllocSegments (newSegmentCount);
        if (newSegments != NULL) {
            *segmentPtr = newSegments;
        } else {
            extendOk = false;
        }
    }
    if (extendOk) {
        buffer->bufferSize = size;
    }
    return extendOk;
}

/*
 * Decreases the size of the buffer to the specified size, removing
 * capacity from the end of the buffer.
 */
static void gmosBufferDecrSizeEnd (
    gmosBuffer_t* buffer, uint_fast16_t size)
{
    uint_fast16_t byteCount;
    gmosMempoolSegment_t** segmentPtr;

    // Follow the segment list to the trim point.
    byteCount = 0;
    segmentPtr = &(buffer->segmentList);
    while (byteCount < buffer->bufferOffset + size) {
        byteCount += GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE;
        segmentPtr = &((*segmentPtr)->nextSegment);
    }

    // Return the excess segments to the memory pool.
    if (*segmentPtr != NULL) {
        gmosMempoolFreeSegments (*segmentPtr);
        *segmentPtr = NULL;
    }
    buffer->bufferSize = size;
}

/*
 * Increases the size of the buffer to the specified size, adding
 * capacity to the start of the buffer.
 */
static bool gmosBufferIncrSizeStart (
    gmosBuffer_t* buffer, uint_fast16_t size)
{
    bool extendOk = true;
    uint_fast16_t extraByteCount;
    uint_fast16_t newSegmentCount;
    uint_fast16_t newOffset;
    gmosMempoolSegment_t** segmentPtr;
    gmosMempoolSegment_t* newSegments;

    // Extend into the existing memory segment if possible.
    extraByteCount = size - buffer->bufferSize;
    if (extraByteCount <= buffer->bufferOffset) {
        buffer->bufferSize = size;
        buffer->bufferOffset -= extraByteCount;
        return true;
    };

    // Calculate the number of additional segments required and the new
    // buffer offset.
    newSegmentCount = 1 + (extraByteCount - buffer->bufferOffset - 1) /
        GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE;
    newOffset = buffer->bufferOffset - extraByteCount +
        newSegmentCount * GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE;

    // Allocate additional memory segments and link them to the start
    // of the buffer.
    newSegments = gmosMempoolAllocSegments (newSegmentCount);
    if (newSegments != NULL) {
        segmentPtr = &(newSegments->nextSegment);
        while (*segmentPtr != NULL) {
            segmentPtr = &((*segmentPtr)->nextSegment);
        }
        *segmentPtr = buffer->segmentList;
        buffer->segmentList = newSegments;
    } else {
        extendOk = false;
    }

    // Update the buffer size and offset.
    if (extendOk) {
        buffer->bufferSize = size;
        buffer->bufferOffset = newOffset;
    }
    return extendOk;
}

/*
 * Decreases the size of the buffer to the specified size, removing
 * capacity from the start of the buffer.
 */
static void gmosBufferDecrSizeStart (
    gmosBuffer_t* buffer, uint_fast16_t size)
{
    uint_fast16_t trimByteCount;
    uint_fast16_t segmentCount;
    gmosMempoolSegment_t** segmentPtr;
    gmosMempoolSegment_t* freeSegments;

    // Follow the segment list to the trim point.
    trimByteCount = buffer->bufferSize - size;
    segmentCount = 0;
    segmentPtr = &(buffer->segmentList);
    while (buffer->bufferOffset + trimByteCount >=
        (segmentCount + 1) * GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE) {
        segmentCount += 1;
        segmentPtr = &((*segmentPtr)->nextSegment);
    }

    // Return the excess segments to the memory pool.
    if (segmentCount > 0) {
        freeSegments = buffer->segmentList;
        buffer->segmentList = *segmentPtr;
        *segmentPtr = NULL;
        gmosMempoolFreeSegments (freeSegments);
    }

    // Update the buffer size and offset fields.
    buffer->bufferSize = size;
    buffer->bufferOffset += trimByteCount -
        segmentCount * GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE;
}

/*
 * Extends a GubbinsMOS data buffer. This allocates additional memory
 * segments from the memory pool, increasing the overall size of the
 * buffer by the specified amount.
 */
bool gmosBufferExtend (gmosBuffer_t* buffer, uint16_t size)
{
    bool extendOk;
    uint32_t newBufferSize =
        ((uint32_t) buffer->bufferSize) + ((uint32_t) size);

    // Extend by zero requests always succeed.
    if (size == 0) {
        extendOk = true;
    }

    // Extending beyond 2^16-1 bytes always fails.
    else if (newBufferSize > 0xFFFF) {
        extendOk = false;
    }

    // Extend to the specified size.
    else {
        extendOk = gmosBufferIncrSizeEnd (buffer, (uint16_t) newBufferSize);
    }
    return extendOk;
}

/*
 * Resizes a GubbinsMOS data buffer to the specified length.
 */
bool gmosBufferResize (gmosBuffer_t* buffer, uint16_t size)
{
    bool resizeOk;

    // Reset to zero length if required.
    if (size == 0) {
        gmosBufferDiscardContents (buffer);
        resizeOk = true;
    }

    // No resizing required.
    else if (size == buffer->bufferSize) {
        resizeOk = true;
    }

    // Extend the buffer if required.
    else if (size > buffer->bufferSize) {
        resizeOk = gmosBufferIncrSizeEnd (buffer, size);
    }

    // Truncate the buffer if required.
    else {
        gmosBufferDecrSizeEnd (buffer, size);
        resizeOk = true;
    }
    return resizeOk;
}

/*
 * Resizes a GubbinsMOS data buffer to the specified length by modifying
 * the start of the buffer.
 */
bool gmosBufferRebase (gmosBuffer_t* buffer, uint16_t size)
{
    bool resizeOk;

    // Reset to zero length if required.
    if (size == 0) {
        gmosBufferDiscardContents (buffer);
        resizeOk = true;
    }

    // No resizing required.
    else if (size == buffer->bufferSize) {
        resizeOk = true;
    }

    // Extend the buffer if required.
    else if (size > buffer->bufferSize) {
        resizeOk = gmosBufferIncrSizeStart (buffer, size);
    }

    // Truncate the buffer if required.
    else {
        gmosBufferDecrSizeStart (buffer, size);
        resizeOk = true;
    }
    return resizeOk;
}

/*
 * Writes a block of data to the buffer at the specified buffer offset.
 * The buffer must be large enough to hold all the data being written.
 */
bool gmosBufferWrite (gmosBuffer_t* buffer, uint16_t offset,
    const uint8_t* writeData, uint16_t writeSize)
{
    bool writeOk;

    // Check for valid offset and size before initiating the copy.
    if (((uint32_t) offset) + ((uint32_t) writeSize) <=
        ((uint32_t) buffer->bufferSize)) {
        gmosBufferCopyToSegments (buffer->segmentList,
            buffer->bufferOffset + offset, writeData, writeSize);
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
            buffer->bufferOffset + offset, readData, readSize);
        readOk = true;
    } else {
        readOk = false;
    }
    return readOk;
}

/*
 * Appends a block of data to the end of the buffer, increasing the
 * buffer length and automatically allocating additional memory pool
 * segments if required.
 */
bool gmosBufferAppend (gmosBuffer_t* buffer,
    const uint8_t* writeData, uint16_t writeSize)
{
    uint_fast16_t offset = buffer->bufferSize;
    bool appendOk;

    // Attempt to extend the buffer before initiating the copy.
    if (gmosBufferExtend (buffer, writeSize)) {
        gmosBufferCopyToSegments (buffer->segmentList,
            buffer->bufferOffset + offset, writeData, writeSize);
        appendOk = true;
    } else {
        appendOk = false;
    }
    return appendOk;
}

/*
 * Prepends a block of data to the start of the buffer, increasing the
 * buffer length and automatically allocating additional memory pool
 * segments if required.
 */
bool gmosBufferPrepend (gmosBuffer_t* buffer,
    const uint8_t* writeData, uint16_t writeSize)
{
    bool prependOk;

    // Attempt to extend the buffer before initiating the copy.
    if (gmosBufferRebase (buffer, buffer->bufferSize + writeSize)) {
        gmosBufferCopyToSegments (buffer->segmentList,
            buffer->bufferOffset, writeData, writeSize);
        prependOk = true;
    } else {
        prependOk = false;
    }
    return prependOk;
}

/*
 * Implements a zero copy move operation, transferring the contents of a
 * source buffer into a destination buffer.
 */
void gmosBufferMove (gmosBuffer_t* source, gmosBuffer_t* destination)
{
    // Ensure that the destination buffer is empty.
    gmosBufferDiscardContents (destination);

    // Transfer the source buffer contents to the destination.
    destination->segmentList = source->segmentList;
    destination->bufferSize = source->bufferSize;
    destination->bufferOffset = source->bufferOffset;

    // Remove source buffer references to the buffer data.
    source->segmentList = NULL;
    source->bufferSize = 0;
    source->bufferOffset = 0;
}

/*
 * Implements the common buffer copy operation after setting up or
 * checking the required copy section parameters.
 */
static bool gmosBufferCopyCommon (gmosBuffer_t* source,
    gmosBuffer_t* destination, uint16_t copyOffset, uint16_t copySize)
{
    uint_fast16_t segmentCount;
    uint_fast16_t targetOffset;
    gmosMempoolSegment_t* segmentList;
    gmosMempoolSegment_t* sourceSegment;
    gmosMempoolSegment_t* targetSegment;

    // Ensure that the destination buffer is empty. This is sufficient
    // to copy an empty source buffer section.
    gmosBufferDiscardContents (destination);
    if (copySize == 0) {
        return true;
    }

    // Allocate the required number of destination buffer segments.
    segmentCount = 1 + (source->bufferOffset + copyOffset + copySize - 1) /
        GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE;
    segmentList = gmosMempoolAllocSegments (segmentCount);
    if (segmentList == NULL) {
        return false;
    }

    // Skip source segments that are not required for the segment copy.
    targetOffset = source->bufferOffset + copyOffset;
    sourceSegment = source->segmentList;
    while (targetOffset >= GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE) {
        targetOffset -= GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE;
        sourceSegment = sourceSegment->nextSegment;
    }

    // Copy the contents of each remaining buffer segment in turn.
    targetSegment = segmentList;
    while ((sourceSegment != NULL) && (targetSegment != NULL)) {
        BUFFER_COPY (targetSegment->data.bytes,
            sourceSegment->data.bytes, GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE);
        sourceSegment = sourceSegment->nextSegment;
        targetSegment = targetSegment->nextSegment;
    }

    // Update the destination buffer state.
    destination->segmentList = segmentList;
    destination->bufferSize = copySize;
    destination->bufferOffset = targetOffset;
    return true;
}

/*
 * Implements a buffer copy operation, replicating the contents of a
 * source buffer in a destination buffer.
 */
bool gmosBufferCopy (gmosBuffer_t* source, gmosBuffer_t* destination)
{
    return gmosBufferCopyCommon (
        source, destination, 0, source->bufferSize);
}

/*
 * Implements a buffer section copy operation, replicating the contents
 * of a section of a source buffer in a destination buffer.
 */
bool gmosBufferCopySection (gmosBuffer_t* source,
    gmosBuffer_t* destination, uint16_t copyOffset, uint16_t copySize)
{
    bool copyOk = false;

    // Check for out of range copy section parameters before initiating
    // the copy operation.
    if (copyOffset + copySize <= source->bufferSize) {
        copyOk = gmosBufferCopyCommon (
            source, destination, copyOffset, copySize);
    }
    return copyOk;
}

/*
 * Perform concatenation where data from source buffer B is appended to
 * source buffer A.
 */
static inline void gmosBufferConcatenateIntoA (
    gmosBuffer_t* sourceA, gmosBuffer_t* sourceB)
{
    uint8_t copyData [2 * GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE];
    uint_fast16_t copySize;

    // Loop until all data in source B has been copied to source A.
    while (sourceB->bufferSize > 0) {
        copySize = sizeof (copyData);
        if (copySize > sourceB->bufferSize) {
            copySize = sourceB->bufferSize;
        }
        gmosBufferRead (sourceB, 0, copyData, copySize);
        gmosBufferRebase (sourceB, sourceB->bufferSize - copySize);
        gmosBufferAppend (sourceA, copyData, copySize);
    }
}

/*
 * Perform concatenation where data from source buffer A is prepended to
 * source buffer B.
 */
static inline void gmosBufferConcatenateIntoB (
    gmosBuffer_t* sourceA, gmosBuffer_t* sourceB)
{
    uint8_t copyData [2 * GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE];
    uint_fast16_t copySize;
    uint_fast16_t copyOffset;

    // Loop until all data in source A has been copied to source B.
    while (sourceA->bufferSize > 0) {
        copySize = sizeof (copyData);
        if (copySize > sourceA->bufferSize) {
            copySize = sourceA->bufferSize;
        }
        copyOffset = sourceA->bufferSize - copySize;
        gmosBufferRead (sourceA, copyOffset, copyData, copySize);
        gmosBufferResize (sourceA, sourceA->bufferSize - copySize);
        gmosBufferPrepend (sourceB, copyData, copySize);
    }
}

/*
 * Implements a buffer concatenate operation, which concatenates the
 * contents of two source buffers and places the result in a destination
 * buffer.
 */
bool gmosBufferConcatenate (gmosBuffer_t* sourceA,
    gmosBuffer_t* sourceB, gmosBuffer_t* destination)
{
    // Reset the destination buffer if both source buffers are empty.
    if ((sourceA->bufferSize == 0) && (sourceB->bufferSize == 0)) {
        gmosBufferReset (destination, 0);
    }

    // Select source B if source A is empty.
    else if (sourceA->bufferSize == 0) {
        if (destination != sourceB) {
            gmosBufferMove (sourceB, destination);
        }
    }

    // Select source A if source B is empty.
    else if (sourceB->bufferSize == 0) {
        if (destination != sourceA) {
            gmosBufferMove (sourceA, destination);
        }
    }

    // Perform concatenation when source A is the largest buffer.
    else if (sourceA->bufferSize >= sourceB->bufferSize) {
        gmosBufferConcatenateIntoA (sourceA, sourceB);
        if (destination != sourceA) {
            gmosBufferMove (sourceA, destination);
        }
    }

    // Perform concatenation when source B is the largest buffer.
    else {
        gmosBufferConcatenateIntoB (sourceA, sourceB);
        if (destination != sourceB) {
            gmosBufferMove (sourceB, destination);
        }
    }
    return true;
}

/*
 * Gets a reference to the buffer segment that contains data at the
 * specified buffer offset.
 */
gmosMempoolSegment_t* gmosBufferGetSegment (gmosBuffer_t* buffer,
    uint16_t dataOffset)
{
    uint_fast16_t byteCount;
    gmosMempoolSegment_t* segment;

    // Check for out of range requests.
    if (dataOffset >= buffer->bufferSize) {
        return NULL;
    }

    // Follow the segment list to the specified offset.
    byteCount = GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE;
    segment = buffer->segmentList;
    while (segment != NULL) {
        if (buffer->bufferOffset + dataOffset >= byteCount) {
            byteCount += GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE;
            segment = segment->nextSegment;
        } else {
            break;
        }
    }
    return segment;
}
