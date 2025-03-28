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
 * This header defines the API for GubbinsMOS byte stream support.
 */

#ifndef GMOS_STREAMS_H
#define GMOS_STREAMS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "gmos-scheduler.h"
#include "gmos-mempool.h"
#include "gmos-buffers.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * Defines the GubbinsMOS byte stream data structure which is used for
 * managing an individual byte stream.
 */
typedef struct gmosStream_t {

    // This is a pointer to the task state data structure for the stream
    // consumer task.
    gmosTaskState_t* consumerTask;

    // This is a pointer to the start of the stream segment list.
    gmosMempoolSegment_t* segmentList;

    // This specifies the upper limit of the stream size.
    uint16_t maxSize;

    // This specifies the current size of the stream contents.
    uint16_t size;

    // This specifies the current offset for the write pointer.
    uint16_t writeOffset;

    // This specifies the current offset for the read pointer.
    uint16_t readOffset;

} gmosStream_t;

/**
 * Provides a stream definition macro that can be used to enforce static
 * type checking when implementing GubbinsMOS streams. The macro defines
 * the following functions:
 *
 * void <_id_>_init (gmosStream_t* stream,
 *     gmosTaskState_t* consumerTask, uint16_t maxDataItems)
 *
 * The initialisation function should be used for the one time setup of
 * a new stream data structure. It specifies the optional consumer task
 * and the maximum number of data items that can be queued using the
 * stream.
 *
 * bool <id>_write (gmosStream_t* stream, <_data_type_>* data)
 *
 * The write function may be used for writing a new data item of the
 * specified data type to the stream.
 *
 * bool <id>_read (gmosStream_t* stream, <_data_type_>* data)
 *
 * The read function may be used for reading a queued data item of the
 * specified data type from the stream.
 *
 * @param _id_ This is the identifier to be used when referring to the
 *     GubbinsMOS stream type defined here.
 * @param _data_type_ This is the data type of items that may be
 *     transferred using the stream.
 */
#define GMOS_STREAM_DEFINITION(_id_, _data_type_)                      \
                                                                       \
static inline void _id_ ## _init (gmosStream_t* stream,                \
    gmosTaskState_t* consumerTask, uint16_t maxDataItems)              \
{                                                                      \
    uint16_t maxStreamSize = maxDataItems * sizeof (_data_type_);      \
    gmosStreamInit (stream, consumerTask, maxStreamSize);              \
}                                                                      \
                                                                       \
static inline bool _id_ ## _write (                                    \
    gmosStream_t* stream, _data_type_ * data)                          \
{                                                                      \
    return gmosStreamWriteAll (stream,                                 \
        (uint8_t*) data, sizeof (_data_type_));                        \
}                                                                      \
                                                                       \
static inline bool _id_ ## _read (                                     \
    gmosStream_t* stream, _data_type_ * data)                          \
{                                                                      \
    return gmosStreamReadAll (stream,                                  \
        (uint8_t*) data, sizeof (_data_type_));                        \
}

/**
 * Provides a compile time initialisation macro for a GubbinsMOS byte
 * stream. Assigning this macro value to a byte stream variable on
 * declaration may be used instead of a call to the 'gmosStreamInit'
 * function to set up the byte stream for subsequent data transfer.
 * @param _consumer_task_ This is the consumer task to which the stream
 *     is to forward data. It is used to automatically make the consumer
 *     task ready to run when new data is written to the stream. A null
 *     reference will disable this functionality.
 * @param _max_stream_size_ This is the maximum number of bytes that may
 *     be queued by the stream at any given time. It must be greater
 *     than zero.
 */
#define GMOS_STREAM_INIT(_consumer_task_, _max_stream_size_)           \
    { _consumer_task_, NULL, _max_stream_size_, 0, 0, 0 }

/**
 * Performs a one-time initialisation of a GubbinsMOS byte stream. This
 * should be called during initialisation to set up the byte stream for
 * subsequent data transfer.
 * @param stream This is the stream state data structure that is to
 *     be initialised.
 * @param consumerTask This is the consumer task to which the stream is
 *     to forward data. It is used to automatically make the consumer
 *     task ready to run when new data is written to the stream. A null
 *     reference will disable this functionality.
 * @param maxStreamSize This is the maximum number of bytes that may be
 *     queued by the stream at any given time. It must be greater than
 *     zero.
 */
void gmosStreamInit (gmosStream_t* stream,
    gmosTaskState_t* consumerTask, uint16_t maxStreamSize);

/**
 * Resets a GubbinsMOS byte stream, discarding all the contents of the
 * stream and releasing all allocated memory.
 * @param stream This is the stream state data structure that is to
 *     be reset.
 */
void gmosStreamReset (gmosStream_t* stream);

/**
 * Dynamically set the consumer task associated with a given stream,
 * resuming consumer task execution if stream data is available.
 * @param stream This is the stream state data structure that is to
 *     be associated with a new consumer task.
 * @param consumerTask This is the new consumer task that is to be
 *     assigned to the stream, or a null reference if no consumer task
 *     is to be used.
 */
void gmosStreamSetConsumerTask (
    gmosStream_t* stream, gmosTaskState_t* consumerTask);

/**
 * Determines the maximum number of free bytes that are available for
 * stream write operations, including any newly allocated segments.
 * @param stream This is the stream state data structure which is
 *     associated with the capacity request.
 * @return Returns the number of bytes that may be written to the stream
 *     without exceeding the available capacity.
 */
uint16_t gmosStreamGetWriteCapacity (gmosStream_t* stream);

/**
 * Determines the maximum number of stored bytes that are available for
 * stream read operations.
 * @param stream This is the stream state data structure which is
 *     associated with the capacity request.
 * @return Returns the number of bytes that may be safely read from the
 *     stream.
 */
uint16_t gmosStreamGetReadCapacity (gmosStream_t* stream);

/**
 * Determines the maximum number of free bytes that are available for
 * stream push back operations, including any newly allocated segments.
 * @param stream This is the stream state data structure which is
 *     associated with the capacity request.
 * @return Returns the number of bytes that may be pushed back to the
 *     stream without exceeding the available capacity.
 */
uint16_t gmosStreamGetPushBackCapacity (gmosStream_t* stream);

/**
 * Writes data from a local byte array to a GubbinsMOS byte stream. Up
 * to the specified number of bytes may be written.
 * @param stream This is the stream state data structure which is
 *     associated with the write data stream.
 * @param writeData This is a pointer to the start of the byte array
 *     that is to be written to the byte stream.
 * @param writeSize This is the number of bytes that are ready to be
 *     written to the byte stream.
 * @return Returns the number of bytes that were written to the byte
 *     stream. This may be zero if no data could be written or any
 *     number of bytes up to the maximum specified by the write size
 *     parameter.
 */
uint16_t gmosStreamWrite (gmosStream_t* stream,
    const uint8_t* writeData, uint16_t writeSize);

/**
 * Writes data from a local byte array to a GubbinsMOS byte stream.
 * Either the specified number of bytes will be written as a single
 * transfer or no data will be transferred.
 * @param stream This is the stream state data structure which is
 *     associated with the write data stream.
 * @param writeData This is a pointer to the start of the byte array
 *     that is to be written to the byte stream.
 * @param writeSize This is the number of bytes that are to be written
 *     to the byte stream.
 * @return Returns a boolean value which will be set to 'true' if all
 *     the write data was transferred to the byte stream and 'false' if
 *     no write data was transferred to the byte stream.
 */
bool gmosStreamWriteAll (gmosStream_t* stream,
    const uint8_t* writeData, uint16_t writeSize);

/**
 * Writes data from a local byte array to a GubbinsMOS byte stream,
 * inserting a two byte message size field as a header. Either the
 * complete message will be written as a single transfer or no data will
 * be transferred.
 * @param stream This is the stream state data structure which is
 *     associated with the write data stream.
 * @param writeData This is a pointer to the start of the byte array
 *     that is to be written to the byte stream.
 * @param writeSize This is the number of bytes that are to be written
 *     to the byte stream. A message length of 0xFFFE or greater is
 *     invalid.
 * @return Returns a boolean value which will be set to 'true' if all
 *     the write data was transferred to the byte stream and 'false' if
 *     no write data was transferred to the byte stream.
 */
bool gmosStreamWriteMessage (gmosStream_t* stream,
    const uint8_t* writeData, uint16_t writeSize);

/**
 * Writes a single byte to a byte stream.
 * @param stream This is the stream state data structure which is
 *     associated with the write data stream.
 * @param writeByte This is the byte value that is to be written to the
 *     byte stream.
 * @return Returns a boolean value which will be set to 'true' if the
 *     data byte was written to the byte stream and 'false' if no write
 *     data was transferred to the byte stream.
 */
bool gmosStreamWriteByte (gmosStream_t* stream, uint8_t writeByte);

/**
 * Reads data from a GubbinsMOS byte stream into a local read data byte
 * array. Up to the specified number of bytes may be transferred.
 * @param stream This is the stream state data structure which is
 *     associated with the read data stream.
 * @param readData This is a pointer to the start of the local byte
 *     array into which the read data is to be transferred.
 * @param readSize This is the size of the read data byte array, and
 *     indicates the maximum number of bytes that may be transferred.
 * @return Returns the number of bytes that were transferred to the
 *     local read data byte array. A return value of zero indicates that
 *     no read data was available.
 */
uint16_t gmosStreamRead (gmosStream_t* stream,
    uint8_t* readData, uint16_t readSize);

/**
 * Reads data from a GubbinsMOS byte stream into a local read data byte
 * array. Either the specified number of bytes will be read as a single
 * transfer or no data will be transferred.
 * @param stream This is the stream state data structure which is
 *     associated with the read data stream.
 * @param readData This is a pointer to the start of the local byte
 *     array into which the read data is to be transferred.
 * @param readSize This is the size of the read data byte array, and
 *     indicates the number of bytes that must be transferred.
 * @return Returns a boolean value which will be set to 'true' if the
 *     specified number of bytes were transferred from the byte stream
 *     and 'false' if no read data was transferred from the byte stream.
 */
bool gmosStreamReadAll (gmosStream_t* stream,
    uint8_t* readData, uint16_t readSize);

/**
 * Reads data from a GubbinsMOS byte stream into a local read data byte
 * array, parsing a two byte message size field as a header. Either the
 * complete message will be read as a single transfer or no data will be
 * transferred.
 * @param stream This is the stream state data structure which is
 *     associated with the read data stream.
 * @param readData This is a pointer to the start of the local byte
 *     array into which the read data is to be transferred.
 * @param readSize This is the size of the read data byte array, and
 *     indicates the maximum number of bytes that can be transferred.
 * @return Returns the number of bytes that were transferred to the
 *     local read data byte array. A return value of zero indicates that
 *     no read data was available and a value of 0xFFFF indicates that
 *     the message is too large to be stored in the local byte array.
 */
uint16_t gmosStreamReadMessage (gmosStream_t* stream,
    uint8_t* readData, uint16_t readSize);

/**
 * Reads a single byte from a byte stream.
 * @param stream This is the stream state data structure which is
 *     associated with the read data stream.
 * @param readByte This is a pointer to the byte value location that
 *     will be updated with the read data value.
 * @return Returns a boolean value which will be set to 'true' if the
 *     data byte was read from the byte stream and 'false' if no read
 *     data was transferred from the byte stream.
 */
bool gmosStreamReadByte (gmosStream_t* stream, uint8_t* readByte);

/**
 * Peeks into the head of the byte stream, copying a byte at the
 * specified offset without removing it from the stream.
 * @param stream This is the stream state data structure which is
 *     associated with the read data stream.
 * @param peekByte This is a pointer to the byte value location that
 *     will be updated with the copied data value.
 * @param offset This is the offset from the head of the stream from
 *     which the stream data byte will be copied.
 * @return Returns a boolean value which will be set to 'true' if the
 *     data byte was copied from the byte stream and 'false' if no data
 *     was copied from the byte stream.
 */
bool gmosStreamPeekByte (gmosStream_t* stream,
    uint8_t* peekByte, uint16_t offset);

/**
 * Pushes data from a local byte array back to the head of a GubbinsMOS
 * byte stream. Either the specified number of bytes will be pushed back
 * as a single transfer or no data will be transferred. The first byte
 * of the local byte array will become the next byte that may be read
 * from the stream.
 * @param stream This is the stream state data structure which is
 *     associated with the push back data stream.
 * @param pushBackData This is a pointer to the start of the byte array
 *     that is to be pushed back to the byte stream.
 * @param pushBackSize This is the number of bytes that are to be pushed
 *     back to the byte stream.
 * @return Returns a boolean value which will be set to 'true' if all
 *     the push back data was transferred to the byte stream and 'false'
 *     if no push back data was transferred to the byte stream.
 */
bool gmosStreamPushBack (gmosStream_t* stream,
    uint8_t* pushBackData, uint16_t pushBackSize);

/**
 * Sends the contents of a data buffer over a GubbinsMOS stream using
 * 'pass by reference' semantics to avoid excessive data copying.
 * @param stream This is the stream state data structure which is
 *     associated with the write data stream.
 * @param buffer This is the buffer containing the data which is to be
 *     transferred via the stream. On successful completion the buffer
 *     instance will automatically be reset to a length of zero, with
 *     ownership of the buffer data having been transferred to the
 *     stream.
 * @return Returns a boolean value which will be set to 'true' if
 *     ownership of the buffer data was transferred to the stream and
 *     'false' on failure.
 */
bool gmosStreamSendBuffer (gmosStream_t* stream, gmosBuffer_t* buffer);

/**
 * Accepts the contents of a data buffer from a GubbinsMOS stream using
 * 'pass by reference' semantics to avoid excessive data copying.
 * @param stream This is the stream state data structure which is
 *     associated with the read data stream.
 * @param buffer This is a buffer instance which on successful
 *     completion will be updated to contain the buffer data transferred
 *     via the stream. Any existing buffer contents will automatically
 *     be discarded, regardless of outcome.
 * @return Returns a boolean value which will be set to 'true' if data
 *     was transferred to the buffer and 'false' if no data is currently
 *     available.
 */
bool gmosStreamAcceptBuffer (gmosStream_t* stream, gmosBuffer_t* buffer);

/**
 * Pushes a data buffer back to the head of a GubbinsMOS stream using
 * 'pass by reference' semantics to avoid excessive data copying. This
 * is useful for situations where a buffer is accepted from the stream,
 * but not all of the buffer contents can be immediately processed.
 * @param stream This is the stream state data structure which is
 *     associated with the push back data stream.
 * @param buffer This is the buffer containing the data which is to be
 *     pushed back to the stream. On successful completion the buffer
 *     instance will automatically be reset to a length of zero, with
 *     ownership of the buffer data having been transferred to the
 *     stream.
 * @return Returns a boolean value which will be set to 'true' if
 *     ownership of the buffer data was transferred to the stream and
 *     'false' on failure.
 */
bool gmosStreamPushBackBuffer (gmosStream_t* stream, gmosBuffer_t* buffer);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // GMOS_STREAMS_H
