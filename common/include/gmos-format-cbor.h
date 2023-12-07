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

/**
 * This configuration option provides the ability to restrict the size
 * of CBOR arrays.
 */
#ifndef GMOS_CONFIG_CBOR_MAX_ARRAY_SIZE
#define GMOS_CONFIG_CBOR_MAX_ARRAY_SIZE 256
#endif

/**
 * This configuration option provides the ability to restrict the size
 * of CBOR maps.
 */
#ifndef GMOS_CONFIG_CBOR_MAX_MAP_SIZE
#define GMOS_CONFIG_CBOR_MAX_MAP_SIZE 256
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
typedef int64_t  gmosFormatCborMapIntKey_t;
#else
typedef uint32_t gmosFormatCborTypeParam_t;
typedef int32_t  gmosFormatCborMapIntKey_t;
#endif

/**
 * Defines the data structure used to encapsulate a single parsed CBOR
 * message token.
 */
typedef struct gmosFormatCborToken_t {

    // Cache the parsed type parameter value as a native data type.
    gmosFormatCborTypeParam_t typeParam;

    // Specify the offset of the associated data in the message buffer.
    uint16_t dataOffset;

    // Specify the total number of tokens required to represent the
    // complete data item, including hierarchically nested tokens.
    uint16_t tokenCount;

    // Cache the data type specifier byte.
    uint8_t typeSpecifier;

} gmosFormatCborToken_t;

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
 * Provides a compile time initialisation macro for a CBOR parser
 * instance. Assigning this macro value to a parser instance on
 * declaration may be used to ensure that the parser data structure
 * is in a valid state prior to subsequent processing.
 */
#define GMOS_FORMAT_CBOR_PARSER_INIT()                                 \
    { GMOS_BUFFER_INIT(), GMOS_BUFFER_INIT() }

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
 * Encodes a CBOR indefinite length break code and appends it to the
 * specified GubbinsMOS buffer.
 * @param buffer This is the buffer to which the new CBOR break code
 *     will be appended.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully appending the break code and 'false' if there is
 *     insufficient buffer memory available.
 */
bool gmosFormatCborEncodeBreakCode (gmosBuffer_t* buffer);

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
 * @param tagNumber This is the data tag identifier which should be used
 *     to tag the subsequent data item.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully appending the new descriptor and 'false' if there is
 *     insufficient buffer memory available.
 */
bool gmosFormatCborEncodeTag (gmosBuffer_t* buffer,
    gmosFormatCborTypeParam_t tagNumber);

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

/**
 * Determines the number of CBOR tokens that make up a given CBOR data
 * item. Complex CBOR data structures such as nested arrays and maps
 * consist of multiple tokens, and this function may be used to
 * determine the total number of tokens that make up the data structure.
 * @param parser This is a pointer to the parser instance that is to
 *     be accessed.
 * @param tokenIndex This is the token index position which is to be
 *     checked for the total number of tokens that make up the
 *     associated CBOR data item.
 * @param tokenCount This is a pointer to a variable that will be
 *     updated with the total number of tokens that make up the
 *     associated data item, including the token specified by the
 *     token index parameter.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully determining the data item token count and 'false'
 *     otherwise.
 */
bool gmosFormatCborDecodeTokenCount (gmosFormatCborParser_t* parser,
    uint16_t tokenIndex, uint16_t* tokenCount);

/**
 * Checks for a CBOR null value at the specified parser token index
 * position.
 * @param parser This is a pointer to the parser instance that is to
 *     be accessed.
 * @param tokenIndex This is the token index position which is to be
 *     checked for the presence of a null value token.
 * @return Returns a boolean value which will be set to 'true' if a
 *     valid null value token is present at the specified index position
 *     and 'false' otherwise.
 */
bool gmosFormatCborMatchNull (gmosFormatCborParser_t* parser,
    uint16_t tokenIndex);

/**
 * Checks for a CBOR undefined value at the specified parser token index
 * position.
 * @param parser This is a pointer to the parser instance that is to
 *     be accessed.
 * @param tokenIndex This is the token index position which is to be
 *     checked for the presence of an undefined value token.
 * @return Returns a boolean value which will be set to 'true' if a
 *     valid undefined value token is present at the specified index
 *     position and 'false' otherwise.
 */
bool gmosFormatCborMatchUndefined (gmosFormatCborParser_t* parser,
    uint16_t tokenIndex);

/**
 * Decodes a CBOR boolean value at the specified parser token index
 * position.
 * @param parser This is a pointer to the parser instance that is to
 *     be accessed.
 * @param tokenIndex This is the token index position which is to be
 *     checked for the presence of a CBOR boolean value token.
 * @param value This is a pointer to the variable which is to be updated
 *     with the decoded CBOR boolean value.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully decoding the CBOR boolean value and 'false'
 *     otherwise.
 */
bool gmosFormatCborDecodeBool (gmosFormatCborParser_t* parser,
    uint16_t tokenIndex, bool* value);

/**
 * Decodes a CBOR 32-bit unsigned integer value at the specified parser
 * token index position. The encoded value must be in the valid range
 * of the native 32-bit unsigned integer data type.
 * @param parser This is a pointer to the parser instance that is to
 *     be accessed.
 * @param tokenIndex This is the token index position which is to be
 *     checked for the presence of a CBOR unsigned integer value token.
 * @param value This is a pointer to the variable which is to be updated
 *     with the decoded 32-bit unsigned integer value.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully decoding the 32-bit unsigned integer value and
 *     'false' otherwise.
 */
bool gmosFormatCborDecodeUint32 (gmosFormatCborParser_t* parser,
    uint16_t tokenIndex, uint32_t* value);

/**
 * Decodes a CBOR 32-bit signed integer value at the specified parser
 * token index position. The encoded value must be in the valid range
 * of the native 32-bit signed integer data type.
 * @param parser This is a pointer to the parser instance that is to
 *     be accessed.
 * @param tokenIndex This is the token index position which is to be
 *     checked for the presence of a CBOR signed integer value token.
 * @param value This is a pointer to the variable which is to be updated
 *     with the decoded 32-bit signed integer value.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully decoding the 32-bit signed integer value and 'false'
 *     otherwise.
 */
bool gmosFormatCborDecodeInt32 (gmosFormatCborParser_t* parser,
    uint16_t tokenIndex, int32_t* value);

/**
 * Decodes a CBOR 64-bit unsigned integer value at the specified parser
 * token index position. The encoded value must be in the valid range
 * of the native 64-bit unsigned integer data type.
 * @param parser This is a pointer to the parser instance that is to
 *     be accessed.
 * @param tokenIndex This is the token index position which is to be
 *     checked for the presence of a CBOR unsigned integer value token.
 * @param value This is a pointer to the variable which is to be updated
 *     with the decoded 64-bit unsigned integer value.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully decoding the 64-bit unsigned integer value and
 *     'false' otherwise.
 */
#if GMOS_CONFIG_CBOR_SUPPORT_64_BIT_VALUES
bool gmosFormatCborDecodeUint64 (gmosFormatCborParser_t* parser,
    uint16_t tokenIndex, uint64_t* value);
#endif

/**
 * Decodes a CBOR 64-bit signed integer value at the specified parser
 * token index position. The encoded value must be in the valid range
 * of the native 64-bit signed integer data type.
 * @param parser This is a pointer to the parser instance that is to
 *     be accessed.
 * @param tokenIndex This is the token index position which is to be
 *     checked for the presence of a CBOR signed integer value token.
 * @param value This is a pointer to the variable which is to be updated
 *     with the decoded 64-bit signed integer value.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully decoding the 64-bit signed integer value and 'false'
 *     otherwise.
 */
#if GMOS_CONFIG_CBOR_SUPPORT_64_BIT_VALUES
bool gmosFormatCborDecodeInt64 (gmosFormatCborParser_t* parser,
    uint16_t tokenIndex, int64_t* value);
#endif

/**
 * Decodes a CBOR 32-bit floating point value at the specified parser
 * token index position. The encoded value must be in a valid format
 * for the IEEE 754 32-bit floating point data type.
 * @param parser This is a pointer to the parser instance that is to
 *     be accessed.
 * @param tokenIndex This is the token index position which is to be
 *     checked for the presence of a CBOR 32-bit floating point token.
 * @param value This is a pointer to the variable which is to be updated
 *     with the decoded 32-bit floating point value.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully decoding the 32-bit floating point value and 'false'
 *     otherwise.
 */
#if GMOS_CONFIG_CBOR_SUPPORT_FLOAT_VALUES
bool gmosFormatCborDecodeFloat32 (gmosFormatCborParser_t* parser,
    uint16_t tokenIndex, float* value);
#endif

/**
 * Decodes a CBOR 64-bit floating point value at the specified parser
 * token index position. The encoded value must be in a valid format
 * for the IEEE 754 32-bit or 64-bit floating point data type.
 * @param parser This is a pointer to the parser instance that is to
 *     be accessed.
 * @param tokenIndex This is the token index position which is to be
 *     checked for the presence of a CBOR 32-bit or 64-bit floating
 *     point token.
 * @param value This is a pointer to the variable which is to be updated
 *     with the decoded 64-bit floating point value.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully decoding the 64-bit floating point value and 'false'
 *     otherwise.
 */
#if GMOS_CONFIG_CBOR_SUPPORT_FLOAT_VALUES
#if GMOS_CONFIG_CBOR_SUPPORT_64_BIT_VALUES
bool gmosFormatCborDecodeFloat64 (gmosFormatCborParser_t* parser,
    uint16_t tokenIndex, double* value);
#endif
#endif

/**
 * Checks for a CBOR text string at the specified parser token index
 * position and compares it to a conventional 'C' null terminated
 * character string.
 * @param parser This is a pointer to the parser instance that is to
 *     be accessed.
 * @param tokenIndex This is the token index position which is to be
 *     checked for the presence of a CBOR finite length text string.
 * @param textString This is a pointer to the null terminated character
 *     string which is to be matched to the CBOR encoded value.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully matching the string contents and 'false' otherwise.
 */
bool gmosFormatCborMatchCharString (gmosFormatCborParser_t* parser,
    uint16_t tokenIndex, const char* textString);

/**
 * Checks for a CBOR text string at the specified parser token index
 * position and compares it to a string of the specified length.
 * @param parser This is a pointer to the parser instance that is to
 *     be accessed.
 * @param tokenIndex This is the token index position which is to be
 *     checked for the presence of a CBOR finite length text string.
 * @param textString This is a pointer to the UTF-8 encoded string which
 *     is to be compared to the CBOR encoded value.
 * @param length This is the length of the text string which is to be
 *     compared to the CBOR encoded value.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully matching the string contents and 'false' otherwise.
 */
bool gmosFormatCborMatchTextString (gmosFormatCborParser_t* parser,
    uint16_t tokenIndex, const char* textString, uint16_t length);

/**
 * Decodes a UTF-8 encoded text string, placing the results in a
 * pre-allocated character array with null termination. The source must
 * be a finite length CBOR text string.
 * @param parser This is a pointer to the parser instance that is to
 *     be accessed.
 * @param tokenIndex This is the token index position which is to be
 *     checked for the presence of a CBOR finite length text string.
 * @param stringBuf This is the character array into which the CBOR text
 *     string is to be copied as a null terminated C string. If the
 *     source string is larger than this array it will be truncated
 *     with null termination.
 * @param stringBufLen This is the length of the allocated character
 *     array into which the CBOR text string is to be copied.
 * @param sourceLen This is a pointer to a variable which will be set
 *     to the length of the original CBOR text string, which may be
 *     larger than the length of the destination character array.
 *     This may be set to a null reference if the caller does not need
 *     to check the original source length.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully decoding the CBOR text string and 'false' otherwise.
 */
bool gmosFormatCborDecodeTextString (gmosFormatCborParser_t* parser,
    uint16_t tokenIndex, char* stringBuf, uint16_t stringBufLen,
    uint16_t* sourceLen);

/**
 * Decodes a CBOR byte string, placing the results in a pre-allocated
 * byte array. The source must be a finite length CBOR byte string.
 * @param parser This is a pointer to the parser instance that is to
 *     be accessed.
 * @param tokenIndex This is the token index position which is to be
 *     checked for the presence of a CBOR finite length byte string.
 * @param byteBuf This is the byte array into which the CBOR byte string
 *     is to be copied. If the source string is larger than this array
 *     it will be truncated.
 * @param byteBufLen This is the length of the allocated byte array into
 *     which the CBOR byte string is to be copied.
 * @param sourceLen This is a pointer to a variable which will be set
 *     to the length of the original CBOR byte string, which may be
 *     larger than the length of the destination byte array. This may be
 *     set to a null reference if the caller does not need to check the
 *     original source length.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully decoding the CBOR byte string and 'false' otherwise.
 */
bool gmosFormatCborDecodeByteString (gmosFormatCborParser_t* parser,
    uint16_t tokenIndex, uint8_t* byteBuf, uint16_t byteBufLen,
    uint16_t* sourceLen);

/**
 * Decodes the CBOR descriptor for a fixed or indefinite length array
 * and indicates the number of elements in the array. It should then be
 * followed by the specified number of array data items.
 * @param parser This is a pointer to the parser instance that is to
 *     be accessed.
 * @param tokenIndex This is the token index position which is to be
 *     checked for the presence of a CBOR array.
 * @param length This is a pointer to a variable which will be set
 *     to the length of the encoded CBOR array.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully decoding the CBOR array and 'false' otherwise.
 */
bool gmosFormatCborDecodeArray (gmosFormatCborParser_t* parser,
    uint16_t tokenIndex, uint16_t* length);

/**
 * Performs an integer index lookup on a fixed or indefinite length
 * array, setting the associated value token index on success.
 * @param parser This is a pointer to the parser instance that is to
 *     be accessed.
 * @param tokenIndex This is the token index position which is to be
 *     checked for the presence of a CBOR array.
 * @param arrayIndex This is the zero based array index value which is
 *     to be used during the array entry lookup.
 * @param valueIndex This is a pointer to a variable which on success
 *     will be set to the token index for the indexed value.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully finding the array entry and 'false' otherwise.
 */
bool gmosFormatCborLookupArrayEntry (gmosFormatCborParser_t* parser,
    uint16_t tokenIndex, uint16_t arrayIndex, uint16_t* valueIndex);

/**
 * Decodes the CBOR descriptor for a fixed or indefinite length map
 * and indicates the number of elements in the map. It should then be
 * followed by the specified number of key/value data item pairs.
 * @param parser This is a pointer to the parser instance that is to
 *     be accessed.
 * @param tokenIndex This is the token index position which is to be
 *     checked for the presence of a CBOR map.
 * @param length This is a pointer to a variable which will be set
 *     to the length of the encoded CBOR map.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully decoding the CBOR map and 'false' otherwise.
 */
bool gmosFormatCborDecodeMap (gmosFormatCborParser_t* parser,
    uint16_t tokenIndex, uint16_t* length);

/**
 * Performs an integer key lookup on a fixed or indefinite length map,
 * setting the associated value token index on success.
 * @param parser This is a pointer to the parser instance that is to
 *     be accessed.
 * @param tokenIndex This is the token index position which is to be
 *     checked for the presence of a CBOR map.
 * @param key This is the key which is to be used during the map entry
 *     lookup. For duplicate keys, the first instance of the duplicate
 *     key will be matched.
 * @param valueIndex This is a pointer to a variable which on success
 *     will be set to the token index for the lookup value.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully finding the map entry and 'false' otherwise.
 */
bool gmosFormatCborLookupMapIntKey (gmosFormatCborParser_t* parser,
    uint16_t tokenIndex, gmosFormatCborMapIntKey_t key,
    uint16_t* valueIndex);

/**
 * Performs a character string key lookup on a fixed or indefinite
 * length map, using a conventional null terminated 'C' string as the
 * key and setting the associated value token index on success.
 * @param parser This is a pointer to the parser instance that is to
 *     be accessed.
 * @param tokenIndex This is the token index position which is to be
 *     checked for the presence of a CBOR map.
 * @param key This is the key which is to be used during the map entry
 *     lookup. For duplicate keys, the first instance of the duplicate
 *     key will be matched.
 * @param valueIndex This is a pointer to a variable which on success
 *     will be set to the token index for the lookup value.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully finding the map entry and 'false' otherwise.
 */
bool gmosFormatCborLookupMapCharKey (
    gmosFormatCborParser_t* parser, uint16_t tokenIndex,
    const char* key, uint16_t* valueIndex);

/**
 * Performs a text string key lookup on a fixed or indefinite length
 * map, using a text string of the specified length as the key and
 * setting the associated value token index on success.
 * @param parser This is a pointer to the parser instance that is to
 *     be accessed.
 * @param tokenIndex This is the token index position which is to be
 *     checked for the presence of a CBOR map.
 * @param key This is the key which is to be used during the map entry
 *     lookup. For duplicate keys, the first instance of the duplicate
 *     key will be matched.
 * @param keyLength This is the length of the map key string.
 * @param valueIndex This is a pointer to a variable which on success
 *     will be set to the token index for the lookup value.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully finding the map entry and 'false' otherwise.
 */
bool gmosFormatCborLookupMapTextKey (
    gmosFormatCborParser_t* parser, uint16_t tokenIndex,
    const char* key, uint16_t keyLength, uint16_t* valueIndex);

/**
 * Decodes the CBOR descriptor for a tag and indicates the tag number.
 * It should then be followed by a single tag content value.
 * @param parser This is a pointer to the parser instance that is to
 *     be accessed.
 * @param tokenIndex This is the token index position which is to be
 *     checked for the presence of a CBOR tag.
 * @param tagNumber This is a pointer to a variable which will be set
 *     to the tag number.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully decoding the CBOR tag and 'false' otherwise.
 */
bool gmosFormatCborDecodeTag (gmosFormatCborParser_t* parser,
    uint16_t tokenIndex, gmosFormatCborTypeParam_t* tagNumber);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // GMOS_FORMAT_CBOR_H
