/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2023 Zynaptic Limited
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
 * This header provides various common type definitions and utility
 * functions for CBOR encoding and parsing. This implements a basic
 * subset of RFC 8949 with some additional restrictions to reduce code
 * complexity. It does not support universal encoding and relies on the
 * application code to have an implicit model of the expected data
 * format.
 */

#ifndef GMOS_FORMAT_CBOR_H
#define GMOS_FORMAT_CBOR_H

#include <stdint.h>
#include <stdbool.h>
#include "gmos-config.h"
#include "gmos-buffers.h"

/**
 * This configuration option provides the ability to disable encoding
 * and decoding of 64-bit wide data items on highly constrained devices.
 */
#ifndef GMOS_CONFIG_CBOR_SUPPORT_64_BIT_VALUES
#define GMOS_CONFIG_CBOR_SUPPORT_64_BIT_VALUES true
#endif

/**
 * This configuration option provides the ability to disable encoding
 * and decoding of floating point data items on highly constrained
 * devices.
 */
#ifndef GMOS_CONFIG_CBOR_SUPPORT_FLOAT_VALUES
#define GMOS_CONFIG_CBOR_SUPPORT_FLOAT_VALUES true
#endif

/**
 * This configuration option provides the ability to restrict the size
 * of CBOR strings.
 */
#ifndef GMOS_CONFIG_CBOR_MAX_STRING_SIZE
#define GMOS_CONFIG_CBOR_MAX_STRING_SIZE 1024
#endif

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * This enumeration specifies the major data types used by the CBOR
 * data encoding.
 */
typedef enum {
    GMOS_FORMAT_CBOR_MAJOR_TYPE_INT_POS  = 0x00,
    GMOS_FORMAT_CBOR_MAJOR_TYPE_INT_NEG  = 0x20,
    GMOS_FORMAT_CBOR_MAJOR_TYPE_STR_BYTE = 0x40,
    GMOS_FORMAT_CBOR_MAJOR_TYPE_STR_TEXT = 0x60,
    GMOS_FORMAT_CBOR_MAJOR_TYPE_ARRAY    = 0x80,
    GMOS_FORMAT_CBOR_MAJOR_TYPE_MAP      = 0xA0,
    GMOS_FORMAT_CBOR_MAJOR_TYPE_TAG      = 0xC0,
    GMOS_FORMAT_CBOR_MAJOR_TYPE_SIMPLE   = 0xE0
} gmosFormatCborMajorType_t;

/**
 * Defines the data type used for CBOR type parameter storage.
 */
#if GMOS_CONFIG_CBOR_SUPPORT_64_BIT_VALUES
typedef uint64_t gmosFormatCborTypeParam_t;
#else
typedef uint32_t gmosFormatCborTypeParam_t;
#endif

/**
 * Defines the data structure used to implement CBOR message parsing.
 */
typedef struct gmosFormatCborParser_t {

    // Allocate buffer space for the message buffer.
    gmosBuffer_t messageBuffer;

    // Allocate buffer space for token storage.
    gmosBuffer_t tokenBuffer;

} gmosFormatCborParser_t;

/**
 * Defines the data structure used to encapsulate a single parsed CBOR
 * message token.
 */
typedef struct gmosFormatCborToken_t {

    // Cache the parsed type parameter value as a native data type.
    gmosFormatCborTypeParam_t typeParam;

    // Specify the offset of the associated data in the message buffer.
    uint16_t dataOffset;

    // Cache the data type specifier byte.
    uint8_t typeSpecifier;

} gmosFormatCborToken_t;

/**
 * Encodes a CBOR null value and appends it to the specified GubbinsMOS
 * buffer.
 * @param buffer This is the buffer to which the new CBOR value will be
 *     appended.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully appending the new value and 'false' if there is
 *     insufficient buffer memory available.
 */
bool gmosFormatCborEncodeNull (gmosBuffer_t* buffer);

/**
 * Encodes a CBOR undefined value and appends it to the specified
 * GubbinsMOS buffer.
 * @param buffer This is the buffer to which the new CBOR value will be
 *     appended.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully appending the new value and 'false' if there is
 *     insufficient buffer memory available.
 */
bool gmosFormatCborEncodeUndefined (gmosBuffer_t* buffer);

/**
 * Encodes a CBOR boolean value and appends it to the specified
 * GubbinsMOS buffer.
 * @param buffer This is the buffer to which the new CBOR value will be
 *     appended.
 * @param value This is the boolean value which is to be appended to
 *     the GubbinsMOS buffer.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully appending the new value and 'false' if there is
 *     insufficient buffer memory available.
 */
bool gmosFormatCborEncodeBool (gmosBuffer_t* buffer, bool value);

/**
 * Encodes a CBOR 32-bit unsigned integer value and appends it to the
 * specified GubbinsMOS buffer.
 * @param buffer This is the buffer to which the new CBOR value will be
 *     appended.
 * @param value This is the 32-bit unsigned integer value which is to be
 *     appended to the GubbinsMOS buffer.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully appending the new value and 'false' if there is
 *     insufficient buffer memory available.
 */
bool gmosFormatCborEncodeUint32 (gmosBuffer_t* buffer, uint32_t value);

/**
 * Encodes a CBOR 32-bit signed integer value and appends it to the
 * specified GubbinsMOS buffer.
 * @param buffer This is the buffer to which the new CBOR value will be
 *     appended.
 * @param value This is the 32-bit signed integer value which is to be
 *     appended to the GubbinsMOS buffer.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully appending the new value and 'false' if there is
 *     insufficient buffer memory available.
 */
bool gmosFormatCborEncodeInt32 (gmosBuffer_t* buffer, int32_t value);

/**
 * Encodes a CBOR 64-bit unsigned integer value and appends it to the
 * specified GubbinsMOS buffer.
 * @param buffer This is the buffer to which the new CBOR value will be
 *     appended.
 * @param value This is the 64-bit unsigned integer value which is to be
 *     appended to the GubbinsMOS buffer.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully appending the new value and 'false' if there is
 *     insufficient buffer memory available.
 */
#if GMOS_CONFIG_CBOR_SUPPORT_64_BIT_VALUES
bool gmosFormatCborEncodeUint64 (gmosBuffer_t* buffer, uint64_t value);
#endif

/**
 * Encodes a CBOR 64-bit signed integer value and appends it to the
 * specified GubbinsMOS buffer.
 * @param buffer This is the buffer to which the new CBOR value will be
 *     appended.
 * @param value This is the 64-bit signed integer value which is to be
 *     appended to the GubbinsMOS buffer.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully appending the new value and 'false' if there is
 *     insufficient buffer memory available.
 */
#if GMOS_CONFIG_CBOR_SUPPORT_64_BIT_VALUES
bool gmosFormatCborEncodeInt64 (gmosBuffer_t* buffer, int64_t value);
#endif

/**
 * Encodes a CBOR 32-bit floating point value and appends it to the
 * specified GubbinsMOS buffer.
 * @param buffer This is the buffer to which the new CBOR value will be
 *     appended.
 * @param value This is the 32-bit floating point value which is to be
 *     appended to the GubbinsMOS buffer.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully appending the new value and 'false' if there is
 *     insufficient buffer memory available.
 */
#if GMOS_CONFIG_CBOR_SUPPORT_FLOAT_VALUES
bool gmosFormatCborEncodeFloat32 (gmosBuffer_t* buffer, float value);
#endif

/**
 * Encodes a CBOR 64-bit floating point value and appends it to the
 * specified GubbinsMOS buffer.
 * @param buffer This is the buffer to which the new CBOR value will be
 *     appended.
 * @param value This is the 64-bit floating point value which is to be
 *     appended to the GubbinsMOS buffer.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully appending the new value and 'false' if there is
 *     insufficient buffer memory available.
 */
#if GMOS_CONFIG_CBOR_SUPPORT_FLOAT_VALUES
#if GMOS_CONFIG_CBOR_SUPPORT_64_BIT_VALUES
bool gmosFormatCborEncodeFloat64 (gmosBuffer_t* buffer, double value);
#endif
#endif

/**
 * Encodes a conventional 'C' null terminated character string as a CBOR
 * text string and appends it to the specified GubbinsMOS buffer.
 * @param buffer This is the buffer to which the new CBOR text string
 *     will be appended.
 * @param textString This is a pointer to the null terminated character
 *     string which is to be appended to the GubbinsMOS buffer.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully appending the new value and 'false' if there is
 *     insufficient buffer memory available.
 */
bool gmosFormatCborEncodeCharString (
    gmosBuffer_t* buffer, const char* textString);

/**
 * Encodes a UTF-8 encoded text string of a specified length as a CBOR
 * text string and appends it to the specified GubbinsMOS buffer.
 * @param buffer This is the buffer to which the new CBOR text string
 *     will be appended.
 * @param textString This is a pointer to the UTF-8 encoded string which
 *     is to be appended to the GubbinsMOS buffer.
 * @param length This is the length of the text string which is to be
 *     appended to the GubbinsMOS buffer.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully appending the new value and 'false' if there is
 *     insufficient buffer memory available.
 */
bool gmosFormatCborEncodeTextString (
    gmosBuffer_t* buffer, const char* textString, uint16_t length);

/**
 * Encodes an arbitrary byte array of a specified length as a CBOR byte
 * string and appends it to the specified GubbinsMOS buffer.
 * @param buffer This is the buffer to which the new CBOR byte string
 *     will be appended.
 * @param byteString This is a pointer to the arbitrary byte array which
 *     is to be appended to the GubbinsMOS buffer.
 * @param length This is the length of the arbitrary byte array which is
 *     to be appended to the GubbinsMOS buffer.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully appending the new value and 'false' if there is
 *     insufficient buffer memory available.
 */
bool gmosFormatCborEncodeByteString (
    gmosBuffer_t* buffer, const uint8_t* byteString, uint16_t length);

/**
 * Encodes the CBOR descriptor for a fixed length array and appends it
 * to the specified GubbinsMOS buffer. It should then be followed by the
 * specified number of array data items.
 * @param buffer This is the buffer to which the new CBOR array
 *     descriptor will be appended.
 * @param length This is the number of data items in the array that will
 *     follow the descriptor.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully appending the new descriptor and 'false' if there is
 *     insufficient buffer memory available.
 */
bool gmosFormatCborEncodeArray (gmosBuffer_t* buffer, uint16_t length);

/**
 * Encodes the CBOR descriptor for a fixed length map and appends it to
 * the specified GubbinsMOS buffer. It should then be followed by the
 * specified number of key/value data item pairs.
 * @param buffer This is the buffer to which the new CBOR array
 *     descriptor will be appended.
 * @param length This is the number of key/value data item pairs that
 *     will follow the descriptor.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully appending the new descriptor and 'false' if there is
 *     insufficient buffer memory available.
 */
bool gmosFormatCborEncodeMap (gmosBuffer_t* buffer, uint16_t length);

/**
 * Encodes the CBOR descriptor for an indefinite length array and
 * appends it to the specified GubbinsMOS buffer. It should then be
 * followed by an arbitrary number of array data items and then the
 * indefinite length break descriptor.
 * @param buffer This is the buffer to which the new CBOR array
 *     descriptor will be appended.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully appending the new descriptor and 'false' if there is
 *     insufficient buffer memory available.
 */
bool gmosFormatCborEncodeIndefArray (gmosBuffer_t* buffer);

/**
 * Encodes the CBOR descriptor for an indefinite length map and appends
 * it to the specified GubbinsMOS buffer. It should then be followed by
 * an arbitrary number of key/value pair data items and then the
 * indefinite length break indicator.
 * @param buffer This is the buffer to which the new CBOR map descriptor
 *     will be appended.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully appending the new descriptor and 'false' if there is
 *     insufficient buffer memory available.
 */
bool gmosFormatCborEncodeIndefMap (gmosBuffer_t* buffer);

/**
 * Encodes the CBOR descriptor for an indefinite length break indicator
 * and appends it to the specified GubbinsMOS buffer.
 * @param buffer This is the buffer to which the new CBOR break
 *     descriptor will be appended.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully appending the new descriptor and 'false' if there is
 *     insufficient buffer memory available.
 */
bool gmosFormatCborEncodeIndefBreak (gmosBuffer_t* buffer);

/**
 * Encodes the CBOR descriptor for a data tag and appends it to the
 * specified GubbinsMOS buffer. It should then be followed by a single
 * data item to which the tag applies.
 * @param buffer This is the buffer to which the new CBOR data tag
 *     descriptor will be appended.
 * @param tagId This is the data tag identifier which should be used
 *     to tag the subsequent data item.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully appending the new descriptor and 'false' if there is
 *     insufficient buffer memory available.
 */
bool gmosFormatCborEncodeTag (gmosBuffer_t* buffer, uint32_t tagId);

/**
 * Initialises a CBOR parser by scanning a CBOR message held in the
 * specified source buffer.
 * @param parser This is a pointer to the parser instance that is to
 *     be initialised using the contents of the supplied CBOR message
 *     buffer.
 * @param buffer This is a pointer to the buffer which contains the
 *     CBOR message that is to be scanned during initialisation. On
 *     successful completion the contents of the buffer will be reset.
 * @param maxScanDepth This sets the maximum number of hierarchical
 *     levels that will be recursively scanned. Setting this to the
 *     correct expected value will prevent malformed CBOR messages being
 *     used in stack overflow attackes.
 * @return Returns a boolean value which will be set to 'true' if the
 *     contents of the CBOR message buffer were correctly scanned and
 *     'false' otherwise.
 */
bool gmosFormatCborParserScan (gmosFormatCborParser_t* parser,
    gmosBuffer_t* buffer, uint8_t maxScanDepth);

/**
 * Resets the state of the parser and releases any resources allocated
 * by a CBOR parser during processing.
 * @param parser This is a pointer to the parser instance that is to
 *     be reset, with all allocated resources being released.
 */
void gmosFormatCborParserReset (gmosFormatCborParser_t* parser);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // GMOS_FORMAT_CBOR_H
