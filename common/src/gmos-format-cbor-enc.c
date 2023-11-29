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
 * This file provides support for encoding CBOR data items and appending
 * them to a GubbinsMOS buffer. It does not support universal encoding
 * and relies on the application code to have an implicit model of the
 * expected data format.
 */

#include <stdint.h>
#include <stdbool.h>

#include "gmos-config.h"
#include "gmos-buffers.h"
#include "gmos-format-cbor.h"

/*
 * This encodes the specified CBOR major type with a numeric parameter
 * and appends it to the target buffer.
 */
static bool gmosFormatCborEncodeWithParameter (gmosBuffer_t* buffer,
    uint_fast8_t majorType, gmosFormatCborTypeParam_t parameter)
{
    uint8_t dataBytes [GMOS_CONFIG_CBOR_SUPPORT_64_BIT_VALUES ? 9 : 5];
    uint_fast8_t dataSize;

    // Implement single byte encoding for small parameter values.
    if (parameter < 24) {
        dataBytes [0] = majorType | (uint8_t) parameter;
        dataSize = 1;
    }

    // Implement one or two byte encoding for other parameter values,
    // using network byte order.
    else if (parameter <= 0xFF) {
        dataBytes [0] = majorType | 24;
        dataBytes [1] = (uint8_t) parameter;
        dataSize = 2;
    } else if (parameter <= 0xFFFF) {
        dataBytes [0] = majorType | 25;
        dataBytes [1] = (uint8_t) (parameter >> 8);
        dataBytes [2] = (uint8_t) parameter;
        dataSize = 3;
    }

    // Optionally support eight byte encoding of 64-bit integers.
#if GMOS_CONFIG_CBOR_SUPPORT_64_BIT_VALUES
    else if (parameter >= 0x100000000) {
        dataBytes [0] = majorType | 27;
        dataBytes [1] = (uint8_t) (parameter >> 56);
        dataBytes [2] = (uint8_t) (parameter >> 48);
        dataBytes [3] = (uint8_t) (parameter >> 40);
        dataBytes [4] = (uint8_t) (parameter >> 32);
        dataBytes [5] = (uint8_t) (parameter >> 24);
        dataBytes [6] = (uint8_t) (parameter >> 16);
        dataBytes [7] = (uint8_t) (parameter >> 8);
        dataBytes [8] = (uint8_t) parameter;
        dataSize = 9;
    }
#endif

    // Support four byte encoding of 32-bit integers.
    else {
        dataBytes [0] = majorType | 26;
        dataBytes [1] = (uint8_t) (parameter >> 24);
        dataBytes [2] = (uint8_t) (parameter >> 16);
        dataBytes [3] = (uint8_t) (parameter >> 8);
        dataBytes [4] = (uint8_t) parameter;
        dataSize = 5;
    }

    // Append the result to the data buffer.
    return gmosBufferAppend (buffer, dataBytes, dataSize);
}

/*
 * This encodes the specified CBOR string type with an associated byte
 * array.
 */
bool gmosFormatCborEncodeWithByteArray (gmosBuffer_t* buffer,
    uint_fast8_t majorType, const uint8_t* byteArray, uint16_t length)
{
    bool appendOk;
    uint_fast16_t rollbackSize;

    // Check the maximum string length limit.
    if (length > GMOS_CONFIG_CBOR_MAX_STRING_SIZE) {
        return false;
    }

    // Get the current buffer length to support rollbacks on failure.
    rollbackSize = gmosBufferGetSize (buffer);
    appendOk = gmosFormatCborEncodeWithParameter (
        buffer, majorType, (gmosFormatCborTypeParam_t) length);

    // Append the byte array contents to the data buffer.
    if (appendOk) {
        appendOk = gmosBufferAppend (buffer, byteArray, length);
        if (!appendOk) {
            gmosBufferResize (buffer, rollbackSize);
        }
    }
    return appendOk;
}

/*
 * This encodes a null value using the simple value major type.
 */
bool gmosFormatCborEncodeNull (gmosBuffer_t* buffer)
{
    uint8_t encoding = GMOS_FORMAT_CBOR_MAJOR_TYPE_SIMPLE | 22;
    return gmosBufferAppend (buffer, &encoding, 1);
}

/*
 * This encodes an undefined value using the simple value major type.
 */
bool gmosFormatCborEncodeUndefined (gmosBuffer_t* buffer)
{
    uint8_t encoding = GMOS_FORMAT_CBOR_MAJOR_TYPE_SIMPLE | 23;
    return gmosBufferAppend (buffer, &encoding, 1);
}

/*
 * This encodes a CBOR break code using the simple value major type.
 */
bool gmosFormatCborEncodeBreakCode (gmosBuffer_t* buffer)
{
    uint8_t encoding = GMOS_FORMAT_CBOR_MAJOR_TYPE_SIMPLE | 31;
    return gmosBufferAppend (buffer, &encoding, 1);
}

/*
 * This encodes a boolean value using the simple value major type.
 */
bool gmosFormatCborEncodeBool (gmosBuffer_t* buffer, bool value)
{
    uint8_t encoding;
    if (value) {
        encoding = GMOS_FORMAT_CBOR_MAJOR_TYPE_SIMPLE | 21;
    } else {
        encoding = GMOS_FORMAT_CBOR_MAJOR_TYPE_SIMPLE | 20;
    }
    return gmosBufferAppend (buffer, &encoding, 1);
}

/*
 * This encodes an unsigned integer of up to 32 bits and appends it to
 * the data buffer.
 */
bool gmosFormatCborEncodeUint32 (gmosBuffer_t* buffer, uint32_t value)
{
    return gmosFormatCborEncodeWithParameter (buffer,
        GMOS_FORMAT_CBOR_MAJOR_TYPE_INT_POS,
        (gmosFormatCborTypeParam_t) value);
}

/*
 * This encodes a signed integer of up to 32 bits and appends it to the
 * data buffer.
 */
bool gmosFormatCborEncodeInt32 (gmosBuffer_t* buffer, int32_t value)
{
    uint_fast8_t type;
    gmosFormatCborTypeParam_t param;

    // Select positive or negative integer encoding.
    if (value >= 0) {
        type = GMOS_FORMAT_CBOR_MAJOR_TYPE_INT_POS;
        param = (gmosFormatCborTypeParam_t) value;
    } else {
        type = GMOS_FORMAT_CBOR_MAJOR_TYPE_INT_NEG;
        param = (gmosFormatCborTypeParam_t) -(value + 1);
    }
    return gmosFormatCborEncodeWithParameter (buffer, type, param);
}

/*
 * This encodes an unsigned integer of up to 64 bits and appends it to
 * the data buffer.
 */
#if GMOS_CONFIG_CBOR_SUPPORT_64_BIT_VALUES
bool gmosFormatCborEncodeUint64 (gmosBuffer_t* buffer, uint64_t value)
{
    return gmosFormatCborEncodeWithParameter (buffer,
        GMOS_FORMAT_CBOR_MAJOR_TYPE_INT_POS,
        (gmosFormatCborTypeParam_t) value);
}
#endif

/*
 * This encodes a signed integer of up to 64 bits and appends it to the
 * data buffer.
 */
#if GMOS_CONFIG_CBOR_SUPPORT_64_BIT_VALUES
bool gmosFormatCborEncodeInt64 (gmosBuffer_t* buffer, int64_t value)
{
    uint_fast8_t type;
    gmosFormatCborTypeParam_t param;

    // Select positive or negative integer encoding.
    if (value >= 0) {
        type = GMOS_FORMAT_CBOR_MAJOR_TYPE_INT_POS;
        param = (gmosFormatCborTypeParam_t) value;
    } else {
        type = GMOS_FORMAT_CBOR_MAJOR_TYPE_INT_NEG;
        param = (gmosFormatCborTypeParam_t) -(value + 1);
    }
    return gmosFormatCborEncodeWithParameter (buffer, type, param);
}
#endif

/*
 * This encodes a single precision floating point value and appends it
 * to the data buffer.
 */
#if GMOS_CONFIG_CBOR_SUPPORT_FLOAT_VALUES
bool gmosFormatCborEncodeFloat32 (gmosBuffer_t* buffer, float value)
{
    uint8_t dataBytes [5];

    // Use a union rather than type punning to extract the raw
    // representation of the floating point value.
    union { float value; uint32_t bits; } data;
    data.value = value;

    // This encoding always has a fixed length of five bytes.
    dataBytes [0] = GMOS_FORMAT_CBOR_MAJOR_TYPE_SIMPLE | 26;
    dataBytes [1] = (uint8_t) (data.bits >> 24);
    dataBytes [2] = (uint8_t) (data.bits >> 16);
    dataBytes [3] = (uint8_t) (data.bits >> 8);
    dataBytes [4] = (uint8_t) data.bits;
    return gmosBufferAppend (buffer, dataBytes, 5);
}
#endif

/*
 * This encodes a double precision floating point value and appends it
 * to the data buffer.
 */
#if GMOS_CONFIG_CBOR_SUPPORT_FLOAT_VALUES
#if GMOS_CONFIG_CBOR_SUPPORT_64_BIT_VALUES
bool gmosFormatCborEncodeFloat64 (gmosBuffer_t* buffer, double value)
{
    uint8_t dataBytes [9];

    // Use a union rather than type punning to extract the raw
    // representation of the floating point value.
    union { double value; uint64_t bits; } data;
    data.value = value;

    // This encoding always has a fixed length of nine bytes.
    dataBytes [0] = GMOS_FORMAT_CBOR_MAJOR_TYPE_SIMPLE | 27;
    dataBytes [1] = (uint8_t) (data.bits >> 56);
    dataBytes [2] = (uint8_t) (data.bits >> 48);
    dataBytes [3] = (uint8_t) (data.bits >> 40);
    dataBytes [4] = (uint8_t) (data.bits >> 32);
    dataBytes [5] = (uint8_t) (data.bits >> 24);
    dataBytes [6] = (uint8_t) (data.bits >> 16);
    dataBytes [7] = (uint8_t) (data.bits >> 8);
    dataBytes [8] = (uint8_t) data.bits;
    return gmosBufferAppend (buffer, dataBytes, 9);
}
#endif
#endif

/*
 * This encodes a conventional null terminated string and appends it
 * to the data buffer.
 */
bool gmosFormatCborEncodeCharString (
    gmosBuffer_t* buffer, const char* textString)
{
    uint_fast16_t length;

    // Check for null termination within the maximum string size.
    for (length = 0;
        length <= GMOS_CONFIG_CBOR_MAX_STRING_SIZE; length++) {
        if (textString [length] == '\0') {
            break;
        }
    }

    // Append as a fixed length string.
    return gmosFormatCborEncodeWithByteArray (buffer,
        GMOS_FORMAT_CBOR_MAJOR_TYPE_STR_TEXT,
        (const uint8_t*) textString, length);
}

/*
 * This encodes a UTF-8 encoded string of a specified length and appends
 * it to the data buffer.
 */
bool gmosFormatCborEncodeTextString (
    gmosBuffer_t* buffer, const char* textString, uint16_t length)
{
    return gmosFormatCborEncodeWithByteArray (buffer,
        GMOS_FORMAT_CBOR_MAJOR_TYPE_STR_TEXT,
        (const uint8_t*) textString, length);
}

/*
 * This encodes a fixed size byte array as a defined length CBOR byte
 * string.
 */
bool gmosFormatCborEncodeByteString (gmosBuffer_t* buffer,
    const uint8_t* byteString, uint16_t length)
{
    return gmosFormatCborEncodeWithByteArray (buffer,
        GMOS_FORMAT_CBOR_MAJOR_TYPE_STR_BYTE, byteString, length);
}

/*
 * This encodes the CBOR descriptor for a fixed length array.
 */
bool gmosFormatCborEncodeArray (gmosBuffer_t* buffer, uint16_t length)
{
    return gmosFormatCborEncodeWithParameter (buffer,
        GMOS_FORMAT_CBOR_MAJOR_TYPE_ARRAY,
        (gmosFormatCborTypeParam_t) length);
}

/*
 * This encodes the CBOR descriptor for a fixed length map.
 */
bool gmosFormatCborEncodeMap (gmosBuffer_t* buffer, uint16_t length)
{
    return gmosFormatCborEncodeWithParameter (buffer,
        GMOS_FORMAT_CBOR_MAJOR_TYPE_MAP,
        (gmosFormatCborTypeParam_t) length);
}

/*
 * This encodes the CBOR descriptor for an indefinite length array.
 */
bool gmosFormatCborEncodeIndefArray (gmosBuffer_t* buffer)
{
    uint8_t encoding = GMOS_FORMAT_CBOR_MAJOR_TYPE_ARRAY | 31;
    return gmosBufferAppend (buffer, &encoding, 1);
}

/*
 * This encodes the CBOR descriptor for an indefinite length map.
 */
bool gmosFormatCborEncodeIndefMap (gmosBuffer_t* buffer)
{
    uint8_t encoding = GMOS_FORMAT_CBOR_MAJOR_TYPE_MAP | 31;
    return gmosBufferAppend (buffer, &encoding, 1);
}

/*
 * This encodes the CBOR descriptor for an indefinite length break
 * indicator.
 */
bool gmosFormatCborEncodeIndefBreak (gmosBuffer_t* buffer)
{
    uint8_t encoding = GMOS_FORMAT_CBOR_MAJOR_TYPE_SIMPLE | 31;
    return gmosBufferAppend (buffer, &encoding, 1);
}

/*
 * This encodes the CBOR descriptor for a data tag.
 */
bool gmosFormatCborEncodeTag (gmosBuffer_t* buffer,
    gmosFormatCborTypeParam_t tagNumber)
{
    return gmosFormatCborEncodeWithParameter (buffer,
        GMOS_FORMAT_CBOR_MAJOR_TYPE_TAG, tagNumber);
}
