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
 * Implements the GubbinsMOS byte stream functionality.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-scheduler.h"
#include "gmos-mempool.h"
#include "gmos-streams.h"

// Use the standard 'memcpy' function for stream data transfer.
#if GMOS_CONFIG_STREAMS_USE_MEMCPY
#include <string.h>
#define STREAM_COPY(_dst_, _src_, _size_) memcpy(_dst_, _src_, _size_)

// Use an inline byte based copy for stream data transfer.
#else
#define STREAM_COPY(_dst_, _src_, _size_) do {                         \
    uint8_t* dstPtr = (uint8_t*) _dst_;                                \
    uint8_t* srcPtr = (uint8_t*) _src_;                                \
    size_t count;                                                      \
    for (count = _size_; count != 0; count--) {                        \
        *(dstPtr++) = *(srcPtr++);                                     \
    }                                                                  \
} while (false)
#endif

/*
 * Gets a pointer to the final memory pool segment in the segment list.
 * Traversing the list is preferred to storing a direct pointer since
 * the number of segments is expected to be small in most cases.
 */
static gmosMempoolSegment_t* gmosStreamSegmentListEnd (gmosStream_t* stream)
{
    gmosMempoolSegment_t* segment = stream->segmentList;
    if (segment != NULL) {
        while (segment->nextSegment != NULL) {
            segment = segment->nextSegment;
        }
    }
    return segment;
}

/*
 * Performs a one-time initialisation of a GubbinsMOS byte stream. This
 * should be called during initialisation to set up the byte stream for
 * subsequent data transfer.
 */
void gmosStreamInit (gmosStream_t* stream,
    gmosTaskState_t* consumerTask, uint16_t maxStreamSize)
{
    stream->consumerTask = consumerTask;
    stream->segmentList = NULL;
    stream->maxSize = maxStreamSize;
    stream->size = 0;
}

/*
 * Determines the maximum number of free bytes that are available for
 * stream write operations, including newly allocated segments.
 */
uint16_t gmosStreamGetWriteCapacity (gmosStream_t* stream)
{
    uint32_t maxFreeBytes;
    uint16_t maxStreamBytes;

    // There is no space in an empty segment list.
    if (stream->segmentList == NULL) {
        maxFreeBytes = 0;
    } else {
        maxFreeBytes = GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE -
            stream->writeOffset;
    }

    // The number of free bytes is increased by the number of available
    // memory pool segments.
    maxFreeBytes += GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE *
        ((uint32_t) gmosMempoolSegmentsAvailable ());

    // Limit the number of free bytes to the maximum for the stream.
    maxStreamBytes = stream->maxSize - stream->size;
    if (maxFreeBytes < maxStreamBytes) {
        maxStreamBytes = (uint16_t) maxFreeBytes;
    }
    return maxStreamBytes;
}

/*
 * Determines the maximum number of stored bytes that are available for
 * stream read operations.
 */
uint16_t gmosStreamGetReadCapacity (gmosStream_t* stream)
{
    return stream->size;
}

/*
 * Performs a stream write transaction of the specified write size.
 * This should always complete, since the wrapper functions will have
 * checked for adequate write capacity.
 */
static void gmosStreamCommonWrite (gmosStream_t* stream,
    uint8_t* writeData, uint16_t writeSize)
{
    uint16_t remainingBytes = writeSize;
    uint8_t* sourcePtr = writeData;
    uint16_t copySize;
    uint8_t* copyPtr;
    gmosMempoolSegment_t* segment;

    // Allocate a new segment if the stream is empty, otherwise select
    // the end of the segment list.
    if (stream->segmentList == NULL) {
        segment = gmosMempoolAlloc ();
        segment->nextSegment = NULL;
        stream->segmentList = segment;
        stream->size = 0;
        stream->writeOffset = 0;
        stream->readOffset = 0;
    } else {
        segment = gmosStreamSegmentListEnd (stream);
    }

    // Write data into the initial segment if there is space.
    copySize = GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE - stream->writeOffset;
    if (writeSize < copySize) {
        copySize = writeSize;
    }
    if (copySize > 0) {
        copyPtr = segment->data.bytes + stream->writeOffset;
        STREAM_COPY (copyPtr, sourcePtr, copySize);
        remainingBytes -= copySize;
        sourcePtr += copySize;
        stream->writeOffset += copySize;
        stream->size += copySize;
    }

    // Write data into subsequent newly allocated segments.
    while (remainingBytes > 0) {
        segment->nextSegment = gmosMempoolAlloc ();
        segment = segment->nextSegment;
        segment->nextSegment = NULL;
        copySize = GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE;
        if (remainingBytes < copySize) {
            copySize = remainingBytes;
        }
        copyPtr = segment->data.bytes;
        STREAM_COPY (copyPtr, sourcePtr, copySize);
        remainingBytes -= copySize;
        sourcePtr += copySize;
        stream->writeOffset = copySize;
        stream->size += copySize;
    }

    // Reschedule the suspended consumer task if required.
    if (stream->consumerTask != NULL) {
        gmosSchedulerTaskResume (stream->consumerTask);
    }
}

/*
 * Writes data from a local byte array to a GubbinsMOS byte stream. Up
 * to the specified number of bytes may be written.
 */
uint16_t gmosStreamWrite (gmosStream_t* stream,
    uint8_t* writeData, uint16_t writeSize)
{
    uint16_t transferSize;

    // Determine the maximum possible write transfer size.
    transferSize = gmosStreamGetWriteCapacity (stream);
    if (transferSize > writeSize) {
        transferSize = writeSize;
    }

    // Perform the write transaction.
    if (transferSize > 0) {
        gmosStreamCommonWrite (stream, writeData, transferSize);
    }
    return transferSize;
}

/*
 * Writes data from a local byte array to a GubbinsMOS byte stream.
 * Either the specified number of bytes will be written as a single
 * transfer or no data will be transferred.
 */
bool gmosStreamWriteAll (gmosStream_t* stream,
    uint8_t* writeData, uint16_t writeSize)
{
    // Determine if there is insufficient space for the entire transfer.
    if (gmosStreamGetWriteCapacity (stream) < writeSize) {
        return false;
    }

    // Perform the write transaction.
    if (writeSize > 0) {
        gmosStreamCommonWrite (stream, writeData, writeSize);
    }
    return true;
}

/*
 * Writes data from a local byte array to a GubbinsMOS byte stream,
 * inserting a two byte message size field as a header. Either the
 * complete message will be written as a single transfer or no data will
 * be transferred.
 */
bool gmosStreamWriteMessage (gmosStream_t* stream,
    uint8_t* writeData, uint16_t writeSize)
{
    // Determine if there is insufficient space for the entire transfer.
    if (gmosStreamGetWriteCapacity (stream) < ((uint32_t) writeSize) + 2) {
        return false;
    }

    // Write the message size header (little endian byte order) followed
    // by the message payload data.
    if (writeSize > 0) {
        gmosStreamWriteByte (stream, (uint8_t) (writeSize));
        gmosStreamWriteByte (stream, (uint8_t) (writeSize >> 8));
        gmosStreamCommonWrite (stream, writeData, writeSize);
    }
    return true;
}

/*
 * Writes a single byte to a byte stream.
 */
bool gmosStreamWriteByte (gmosStream_t* stream, uint8_t writeByte)
{
    uint8_t* writePtr;
    gmosMempoolSegment_t* segment;

    // Determine if there is insufficient space for the transfer.
    if (gmosStreamGetWriteCapacity (stream) == 0) {
        return false;
    }

    // Allocate a new segment if the stream is empty, otherwise select
    // the end of the segment list.
    if (stream->segmentList == NULL) {
        segment = gmosMempoolAlloc ();
        segment->nextSegment = NULL;
        stream->segmentList = segment;
        stream->size = 0;
        stream->writeOffset = 0;
        stream->readOffset = 0;
    } else {
        segment = gmosStreamSegmentListEnd (stream);
    }

    // Append a new segment to the segment list if required.
    if (stream->writeOffset == GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE) {
        segment->nextSegment = gmosMempoolAlloc ();
        segment = segment->nextSegment;
        segment->nextSegment = NULL;
        stream->writeOffset = 0;
    }

    // Append the data byte to the stream.
    writePtr = segment->data.bytes + stream->writeOffset;
    *writePtr = writeByte;
    stream->writeOffset += 1;
    stream->size += 1;

    // Reschedule the suspended consumer task if required.
    if (stream->consumerTask != NULL) {
        gmosSchedulerTaskResume (stream->consumerTask);
    }
    return true;
}

/*
 * Performs a stream read transaction of the specified read size. This
 * should always complete, since the wrapper functions will have checked
 * for adequate read data.
 */
static void gmosStreamCommonRead (gmosStream_t* stream,
    uint8_t* readData, uint16_t readSize)
{
    uint16_t remainingBytes = readSize;
    uint8_t* targetPtr = readData;
    uint16_t copySize;
    uint8_t* copyPtr;
    gmosMempoolSegment_t* segment = stream->segmentList;

    // Iterate from the start of the segment list.
    while (remainingBytes > 0) {
        copySize = GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE - stream->readOffset;
        if (remainingBytes < copySize) {
            copySize = remainingBytes;
        }
        copyPtr = segment->data.bytes + stream->readOffset;
        STREAM_COPY (targetPtr, copyPtr, copySize);
        remainingBytes -= copySize;
        targetPtr += copySize;
        stream->readOffset += copySize;
        stream->size -= copySize;

        // Release the current memory pool segment if required. If this
        // is the last segment, the segment list will take the null
        // reference from the next segment pointer.
        if ((stream->readOffset == GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE) ||
            (stream->size == 0)) {
            stream->segmentList = segment->nextSegment;
            stream->readOffset = 0;
            gmosMempoolFree (segment);
            segment = stream->segmentList;
        }
    }
}

/*
 * Reads data from a GubbinsMOS byte stream into a local read data byte
 * array. Up to the specified number of bytes may be transferred.
 */
uint16_t gmosStreamRead (gmosStream_t* stream,
    uint8_t* readData, uint16_t readSize)
{
    uint16_t transferSize;

    // Determine the maximum possible read transfer size.
    transferSize = stream->size;
    if (transferSize > readSize) {
        transferSize = readSize;
    }

    // Perform the read transaction.
    if (transferSize > 0) {
        gmosStreamCommonRead (stream, readData, transferSize);
    }
    return transferSize;
}

/*
 * Reads data from a GubbinsMOS byte stream into a local read data byte
 * array. Either the specified number of bytes will be read as a single
 * transfer or no data will be transferred.
 */
bool gmosStreamReadAll (gmosStream_t* stream,
    uint8_t* readData, uint16_t readSize)
{
    // Determine if there is sufficient data for the entire transfer.
    if (stream->size < readSize) {
        return false;
    }

    // Perform the read transaction.
    if (readSize > 0) {
        gmosStreamCommonRead (stream, readData, readSize);
    }
    return true;
}

/*
 * Reads data from a GubbinsMOS byte stream into a local read data byte
 * array, parsing a two byte message size field as a header. Either the
 * complete message will be read as a single transfer or no data will be
 * transferred.
 */
uint16_t gmosStreamReadMessage (gmosStream_t* stream,
    uint8_t* readData, uint16_t readSize)
{
    uint8_t msgSizeLow;
    uint8_t msgSizeHigh;
    uint16_t msgSize;

    // Attempt to access the message size bytes. This will fail if no
    // data is available.
    if (!gmosStreamPeekByte (stream, &msgSizeLow, 0) ||
        !gmosStreamPeekByte (stream, &msgSizeHigh, 1)) {
        return 0;
    }
    msgSize = (((uint16_t) msgSizeHigh) << 8) | msgSizeLow;

    // Check that all of the message is available.
    if (stream->size < msgSize + 2) {
        return 0;
    }

    // Check that all of the message can be received by the caller.
    if (msgSize > readSize) {
        return 0xFFFF;
    }

    // Discard the header bytes and then copy over the message body.
    gmosStreamReadByte (stream, &msgSizeLow);
    gmosStreamReadByte (stream, &msgSizeHigh);
    if (msgSize > 0) {
        gmosStreamCommonRead (stream, readData, msgSize);
    }
    return msgSize;
}

/*
 * Reads a single byte from a byte stream.
 */
bool gmosStreamReadByte (gmosStream_t* stream, uint8_t* readByte)
{
    uint8_t* readPtr;
    gmosMempoolSegment_t* segment = stream->segmentList;

    // Determine if there is data available.
    if (stream->size == 0) {
        return false;
    }

    // Copy the read data byte from the stream.
    readPtr = segment->data.bytes + stream->readOffset;
    *readByte = *readPtr;
    stream->readOffset += 1;
    stream->size -= 1;

    // Release the current memory pool segment if required. If this is
    // the last segment, the segment list will take the null reference
    // from the next segment pointer.
    if ((stream->readOffset == GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE) ||
        (stream->size == 0)) {
        stream->segmentList = segment->nextSegment;
        stream->readOffset = 0;
        gmosMempoolFree (segment);
    }
    return true;
}

/*
 * Peeks into the head of the byte stream, copying a byte at the
 * specified offset without removing it from the stream.
 */
bool gmosStreamPeekByte (gmosStream_t* stream,
    uint8_t* peekByte, uint16_t offset)
{
    uint16_t residualOffset;
    gmosMempoolSegment_t* segment;

    // Determine if there is data available.
    if (stream->size <= offset) {
        return false;
    }

    // Index into the initial memory segment.
    residualOffset = offset + stream->readOffset;
    segment = stream->segmentList;

    // Search for the memory segment containing the data.
    while (residualOffset >= GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE) {
        residualOffset -= GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE;
        segment = segment->nextSegment;
    }

    // Copy the data from the selected memory segment.
    *peekByte = segment->data.bytes [residualOffset];
    return true;
}
