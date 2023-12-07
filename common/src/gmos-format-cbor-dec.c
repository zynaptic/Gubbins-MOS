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
 * This file provides support for decoding CBOR data items held in a
 * GubbinsMOS message buffer. It does not support universal encoding
 * and relies on the application code to have an implicit model of the
 * expected data format.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-buffers.h"
#include "gmos-format-cbor.h"

/*
 * Provides a forward reference to the main token processing function.
 */
static uint_fast16_t gmosFormatCborParserScanNextToken (
    gmosFormatCborParser_t* parser, uint_fast16_t tokenOffset,
    uint_fast8_t scanDepth, bool* breakDetect);

/*
 * Selects the appropriate integer token access function to use for
 * integer map keys.
 */
#if GMOS_CONFIG_CBOR_SUPPORT_64_BIT_VALUES
#define gmosFormatCborDecodeMapIntKey gmosFormatCborDecodeInt64
#else
#define gmosFormatCborDecodeMapIntKey gmosFormatCborDecodeInt32
#endif

/*
 * This decodes the CBOR major type and associated parameter and stores
 * the result in the token data structure.
 */
static inline bool gmosFormatCborDecodeWithParameter (
    gmosBuffer_t* buffer, uint16_t bufferOffset,
    gmosFormatCborToken_t* token)
{
    uint8_t firstByte;
    uint_fast8_t additionalInfo;
    gmosFormatCborTypeParam_t typeParam;
    uint8_t dataBytes [GMOS_CONFIG_CBOR_SUPPORT_64_BIT_VALUES ? 8 : 4];

    // Read the first byte from the buffer.
    if (!gmosBufferRead (buffer, bufferOffset, &firstByte, 1)) {
        goto fail;
    }

    // Use the low order bits of the first byte as the parameter.
    additionalInfo = firstByte & 0x1F;
    if (additionalInfo < 24) {
        token->typeParam = additionalInfo;
        token->dataOffset = bufferOffset + 1;
    }

    // Use a single additional byte as the parameter.
    else if (additionalInfo == 24) {
        if (!gmosBufferRead (buffer, bufferOffset + 1, dataBytes, 1)) {
            goto fail;
        }
        token->typeParam = dataBytes [0];
        token->dataOffset = bufferOffset + 2;
    }

    // Use two additional bytes as the parameter.
    else if (additionalInfo == 25) {
        if (!gmosBufferRead (buffer, bufferOffset + 1, dataBytes, 2)) {
            goto fail;
        }
        typeParam = dataBytes [0];
        typeParam = (typeParam << 8) + dataBytes [1];
        token->typeParam = typeParam;
        token->dataOffset = bufferOffset + 3;
    }

    // Use four additional bytes as the parameter.
    else if (additionalInfo == 26) {
        if (!gmosBufferRead (buffer, bufferOffset + 1, dataBytes, 4)) {
            goto fail;
        }
        typeParam = dataBytes [0];
        typeParam = (typeParam << 8) + dataBytes [1];
        typeParam = (typeParam << 8) + dataBytes [2];
        typeParam = (typeParam << 8) + dataBytes [3];
        token->typeParam = typeParam;
        token->dataOffset = bufferOffset + 5;
    }

    // Use eight additional bytes as the parameter.
    else if (additionalInfo == 27) {
#if GMOS_CONFIG_CBOR_SUPPORT_64_BIT_VALUES
        if (!gmosBufferRead (buffer, bufferOffset + 1, dataBytes, 8)) {
            goto fail;
        }
        typeParam = dataBytes [0];
        typeParam = (typeParam << 8) + dataBytes [1];
        typeParam = (typeParam << 8) + dataBytes [2];
        typeParam = (typeParam << 8) + dataBytes [3];
        typeParam = (typeParam << 8) + dataBytes [4];
        typeParam = (typeParam << 8) + dataBytes [5];
        typeParam = (typeParam << 8) + dataBytes [6];
        typeParam = (typeParam << 8) + dataBytes [7];
        token->typeParam = typeParam;
        token->dataOffset = bufferOffset + 9;
#else
        goto fail;
#endif
    }

    // Use no derived parameter value or fail on reserved settings.
    else if (additionalInfo == 31) {
        token->typeParam = 0;
        token->dataOffset = bufferOffset + 1;
    } else {
        goto fail;
    }

    // Set the cached type specifier.
    token->typeSpecifier = firstByte;
    token->tokenCount = 1;
    return true;

    // Fail on malformed message.
fail :
    return false;
}

/*
 * This scans the contents of a fixed length array, returning the new
 * token offset on successful completion, or zero on failure.
 */
static inline uint16_t gmosFormatCborParserScanFixedArray (
    gmosFormatCborParser_t* parser, gmosFormatCborToken_t* token,
    uint_fast8_t scanDepth)
{
    uint_fast16_t newTokenOffset = 0;
    uint_fast16_t arraySize;
    uint_fast16_t tokenLocation;
    uint_fast16_t i;

    // Check the scan data structure depth limit.
    if (scanDepth > 0) {
        scanDepth -= 1;
    } else {
        goto exit;
    }

    // For a fixed length array, the number of entries will be set by
    // the token parameter.
    if (token->typeParam <= GMOS_CONFIG_CBOR_MAX_ARRAY_SIZE) {
        arraySize = (uint_fast8_t) token->typeParam;
    } else {
        goto exit;
    }

    // Append the token to the token list as a placeholder.
    tokenLocation = gmosBufferGetSize (&(parser->tokenBuffer));
    if (!gmosBufferAppend (&(parser->tokenBuffer),
        (uint8_t*) token, sizeof (gmosFormatCborToken_t))) {
        goto exit;
    }

    // The first array element is immediately after the array token.
    newTokenOffset = token->dataOffset;
    for (i = 0; i < arraySize; i++) {
        newTokenOffset = gmosFormatCborParserScanNextToken (
            parser, newTokenOffset, scanDepth, NULL);
        if (newTokenOffset == 0) {
            goto exit;
        }
    }

    // Update the token count to reflect the number of enclosed tokens.
    token->tokenCount =
        (gmosBufferGetSize (&(parser->tokenBuffer)) - tokenLocation) /
        sizeof (gmosFormatCborToken_t);
    if (!gmosBufferWrite (&(parser->tokenBuffer), tokenLocation,
        (uint8_t*) token, sizeof (gmosFormatCborToken_t))) {
        newTokenOffset = 0;
    }
exit :
    return newTokenOffset;
}

/*
 * This scans the contents of an indefinite length array, returning the
 * new token offset on successful completion or zero on failure.
 */
static inline uint16_t gmosFormatCborParserScanIndefArray (
    gmosFormatCborParser_t* parser, gmosFormatCborToken_t* token,
    uint_fast8_t scanDepth)
{
    uint_fast16_t newTokenOffset = 0;
    uint_fast16_t arraySize;
    uint_fast16_t tokenLocation;
    bool breakDetect;

    // Check the scan data structure depth limit.
    if (scanDepth > 0) {
        scanDepth -= 1;
    } else {
        goto exit;
    }

    // Append the token to the token list as a placeholder.
    tokenLocation = gmosBufferGetSize (&(parser->tokenBuffer));
    if (!gmosBufferAppend (&(parser->tokenBuffer),
        (uint8_t*) token, sizeof (gmosFormatCborToken_t))) {
        goto exit;
    }

    // The first array element is immediately after the array token.
    // The break code is used to indicate the end of the array;
    newTokenOffset = token->dataOffset;
    arraySize = 0;
    while (true) {
        newTokenOffset = gmosFormatCborParserScanNextToken (
            parser, newTokenOffset, scanDepth, &breakDetect);
        if (newTokenOffset == 0) {
            goto exit;
        } else if (breakDetect) {
            break;
        } else if (arraySize >= GMOS_CONFIG_CBOR_MAX_ARRAY_SIZE) {
            newTokenOffset = 0;
            goto exit;
        } else {
            arraySize += 1;
        }
    }

    // Update the start of array token with the detected array length
    // and the number of enclosed tokens.
    token->typeParam = arraySize;
    token->tokenCount =
        (gmosBufferGetSize (&(parser->tokenBuffer)) - tokenLocation) /
        sizeof (gmosFormatCborToken_t);
    if (!gmosBufferWrite (&(parser->tokenBuffer), tokenLocation,
        (uint8_t*) token, sizeof (gmosFormatCborToken_t))) {
        newTokenOffset = 0;
    }
exit :
    return newTokenOffset;
}

/*
 * This scans the contents of a fixed length map, returning the new
 * token offset on successful completion, or zero on failure.
 */
static inline uint16_t gmosFormatCborParserScanFixedMap (
    gmosFormatCborParser_t* parser, gmosFormatCborToken_t* token,
    uint_fast8_t scanDepth)
{
    uint_fast16_t newTokenOffset = 0;
    uint_fast16_t mapSize;
    uint_fast16_t tokenLocation;
    uint_fast16_t i;

    // Check the scan data structure depth limit.
    if (scanDepth > 0) {
        scanDepth -= 1;
    } else {
        goto exit;
    }

    // For a fixed length map, the number of entries will be set by
    // the token parameter.
    if (token->typeParam <= GMOS_CONFIG_CBOR_MAX_MAP_SIZE) {
        mapSize = (uint_fast8_t) token->typeParam;
    } else {
        goto exit;
    }

    // Append the token to the token list as a placeholder.
    tokenLocation = gmosBufferGetSize (&(parser->tokenBuffer));
    if (!gmosBufferAppend (&(parser->tokenBuffer),
        (uint8_t*) token, sizeof (gmosFormatCborToken_t))) {
        goto exit;
    }

    // The first map element is immediately after the map token and
    // each map element should consist of two tokens (key and value).
    newTokenOffset = token->dataOffset;
    for (i = 0; i < 2 * mapSize; i++) {
        newTokenOffset = gmosFormatCborParserScanNextToken (
            parser, newTokenOffset, scanDepth, NULL);
        if (newTokenOffset == 0) {
            goto exit;
        }
    }

    // Update the token count to reflect the number of enclosed tokens.
    token->tokenCount =
        (gmosBufferGetSize (&(parser->tokenBuffer)) - tokenLocation) /
        sizeof (gmosFormatCborToken_t);
    if (!gmosBufferWrite (&(parser->tokenBuffer), tokenLocation,
        (uint8_t*) token, sizeof (gmosFormatCborToken_t))) {
        newTokenOffset = 0;
    }
exit :
    return newTokenOffset;
}

/*
 * This scans the contents of an indefinite length map, returning the
 * new token offset on successful completion or zero on failure.
 */
static inline uint16_t gmosFormatCborParserScanIndefMap (
    gmosFormatCborParser_t* parser, gmosFormatCborToken_t* token,
    uint_fast8_t scanDepth)
{
    uint_fast16_t newTokenOffset = 0;
    uint_fast16_t mapSize;
    uint_fast16_t tokenLocation;
    bool breakDetect;

    // Check the scan data structure depth limit.
    if (scanDepth > 0) {
        scanDepth -= 1;
    } else {
        goto exit;
    }

    // Append the token to the token list as a placeholder.
    tokenLocation = gmosBufferGetSize (&(parser->tokenBuffer));
    if (!gmosBufferAppend (&(parser->tokenBuffer),
        (uint8_t*) token, sizeof (gmosFormatCborToken_t))) {
        goto exit;
    }

    // The first map element is immediately after the map token and each
    // map element should consist of two tokens (key and value). The
    // break code is used to indicate the end of the map.
    newTokenOffset = token->dataOffset;
    mapSize = 0;
    while (true) {

        // Check for valid map key.
        newTokenOffset = gmosFormatCborParserScanNextToken (
            parser, newTokenOffset, scanDepth, &breakDetect);
        if (newTokenOffset == 0) {
            goto exit;
        } else if (breakDetect) {
            break;
        } else if (mapSize >= GMOS_CONFIG_CBOR_MAX_MAP_SIZE) {
            newTokenOffset = 0;
            goto exit;
        } else {
            mapSize += 1;
        }

        // Check for valid map value.
        newTokenOffset = gmosFormatCborParserScanNextToken (
            parser, newTokenOffset, scanDepth, NULL);
        if (newTokenOffset == 0) {
            goto exit;
        }
    }

    // Update the start of map token with the detected map length and
    // the total number of enclosed tokens.
    token->typeParam = mapSize;
    token->tokenCount =
        (gmosBufferGetSize (&(parser->tokenBuffer)) - tokenLocation) /
        sizeof (gmosFormatCborToken_t);
    if (!gmosBufferWrite (&(parser->tokenBuffer), tokenLocation,
        (uint8_t*) token, sizeof (gmosFormatCborToken_t))) {
        newTokenOffset = 0;
    }
exit :
    return newTokenOffset;
}

/*
 * This scans the contents of a fixed character or octet string and
 * checks that the specified string size does not exceed the size of
 * the source buffer.
 */
static inline uint_fast16_t gmosFormatCborParserScanFixedString (
    gmosFormatCborParser_t* parser, gmosFormatCborToken_t* token)
{
    uint_fast16_t newTokenOffset = token->dataOffset + token->typeParam;
    if (newTokenOffset > gmosBufferGetSize (&(parser->messageBuffer))) {
        newTokenOffset = 0;
    } else if (!gmosBufferAppend (&(parser->tokenBuffer),
        (uint8_t*) token, sizeof (gmosFormatCborToken_t))) {
        newTokenOffset = 0;
    }
    return newTokenOffset;
}

/*
 * This scans the contents of a tagged data item. Recursive tags are
 * supported up to the maximum scan depth.
 */
static inline uint_fast16_t gmosFormatCborParserScanTag (
    gmosFormatCborParser_t* parser, gmosFormatCborToken_t* token,
    uint_fast8_t scanDepth)
{
    uint_fast16_t newTokenOffset = 0;
    uint_fast16_t tokenLocation;

    // Check the scan data structure depth limit.
    if (scanDepth > 0) {
        scanDepth -= 1;
    } else {
        goto exit;
    }

    // Append the token to the token list as a placeholder.
    tokenLocation = gmosBufferGetSize (&(parser->tokenBuffer));
    if (!gmosBufferAppend (&(parser->tokenBuffer),
        (uint8_t*) token, sizeof (gmosFormatCborToken_t))) {
        goto exit;
    }

    // Process a single tagged data item. This follows immediately after
    // the tag number token.
    newTokenOffset = token->dataOffset;
    newTokenOffset = gmosFormatCborParserScanNextToken (
        parser, newTokenOffset, scanDepth, NULL);

    // Update the token count to reflect the number of enclosed tokens.
    token->tokenCount =
        (gmosBufferGetSize (&(parser->tokenBuffer)) - tokenLocation) /
        sizeof (gmosFormatCborToken_t);
    if (!gmosBufferWrite (&(parser->tokenBuffer), tokenLocation,
        (uint8_t*) token, sizeof (gmosFormatCborToken_t))) {
        newTokenOffset = 0;
    }
exit :
    return newTokenOffset;
}

/*
 * This scans the next token of the source message, returning the new
 * token offset on successful completion, or zero on failure.
 */
static uint_fast16_t gmosFormatCborParserScanNextToken (
    gmosFormatCborParser_t* parser, uint_fast16_t tokenOffset,
    uint_fast8_t scanDepth, bool* breakDetect)
{
    gmosFormatCborToken_t token;
    uint_fast16_t newTokenOffset = 0;

    // Clear the break detect flag by default.
    if (breakDetect != NULL) {
        *breakDetect = false;
    }

    // Extract the major type and the parameter.
    if (!gmosFormatCborDecodeWithParameter (
        &(parser->messageBuffer), tokenOffset, &token)) {
        goto out;
    }

    // Select the processing option according to the major type.
    switch (token.typeSpecifier & 0xE0) {

        // Select between fixed length and indefinite length arrays.
        case GMOS_FORMAT_CBOR_MAJOR_TYPE_ARRAY :
            if ((token.typeSpecifier & 0x1F) == 31) {
                newTokenOffset = gmosFormatCborParserScanIndefArray (
                    parser, &token, scanDepth);
            } else {
                newTokenOffset = gmosFormatCborParserScanFixedArray (
                    parser, &token, scanDepth);
            }
            break;

        // Select between fixed length and indefinite length maps.
        case GMOS_FORMAT_CBOR_MAJOR_TYPE_MAP :
            if ((token.typeSpecifier & 0x1F) == 31) {
                newTokenOffset = gmosFormatCborParserScanIndefMap (
                    parser, &token, scanDepth);
            } else {
                newTokenOffset = gmosFormatCborParserScanFixedMap (
                    parser, &token, scanDepth);
            }
            break;

        // Process fixed length strings.
        case GMOS_FORMAT_CBOR_MAJOR_TYPE_STR_BYTE :
        case GMOS_FORMAT_CBOR_MAJOR_TYPE_STR_TEXT :
            if ((token.typeSpecifier & 0x1F) != 31) {
                newTokenOffset = gmosFormatCborParserScanFixedString (
                    parser, &token);
            }
            break;

        // Process the standard fixed size major types.
        case GMOS_FORMAT_CBOR_MAJOR_TYPE_INT_POS :
        case GMOS_FORMAT_CBOR_MAJOR_TYPE_INT_NEG :
            if (gmosBufferAppend (&(parser->tokenBuffer),
                (uint8_t*) &token, sizeof (gmosFormatCborToken_t))) {
                newTokenOffset = token.dataOffset;
            }
            break;

        // Process tagged data types.
        case GMOS_FORMAT_CBOR_MAJOR_TYPE_TAG :
            newTokenOffset = gmosFormatCborParserScanTag (
                parser, &token, scanDepth);
            break;

        // Process the simple data types. These are all appended to the
        // token buffer apart from break code tokens.
        case GMOS_FORMAT_CBOR_MAJOR_TYPE_SIMPLE :
            if (token.typeSpecifier == 0xFF) {
                if (breakDetect != NULL) {
                    newTokenOffset = token.dataOffset;
                    *breakDetect = true;
                }
            } else if (gmosBufferAppend (&(parser->tokenBuffer),
                (uint8_t*) &token, sizeof (gmosFormatCborToken_t))) {
                newTokenOffset = token.dataOffset;
            }
            break;
        default :
            break;
    }

    // Return the new token offset, or zero on failure.
out :
    return newTokenOffset;
}

/*
 * This initialises a CBOR parser by scanning a CBOR message held in the
 * specified source buffer.
 */
bool gmosFormatCborParserScan (gmosFormatCborParser_t* parser,
    gmosBuffer_t* buffer, uint8_t maxScanDepth)
{
    uint_fast16_t nextTokenOffset;

    // Reset parser state.
    gmosBufferInit (&(parser->messageBuffer));
    gmosBufferInit (&(parser->tokenBuffer));
    gmosBufferMove (buffer, &(parser->messageBuffer));

    // Parse the first token in the message.
    nextTokenOffset = gmosFormatCborParserScanNextToken (
        parser, 0, maxScanDepth, NULL);

    // On completion there should be no further data in the message
    // buffer.
    if ((nextTokenOffset != 0) && (nextTokenOffset ==
        gmosBufferGetSize (&(parser->messageBuffer)))) {
        return true;
    } else {
        gmosBufferMove (&(parser->messageBuffer), buffer);
        gmosBufferReset (&(parser->tokenBuffer), 0);
        return false;
    }
}

/*
 * Resets the state of the parser and releases any resources allocated
 * by a CBOR parser during processing.
 */
void gmosFormatCborParserReset (gmosFormatCborParser_t* parser)
{
    gmosBufferReset (&(parser->messageBuffer), 0);
    gmosBufferReset (&(parser->tokenBuffer), 0);
}

/*
 * Determines the number of CBOR tokens that make up a given CBOR data
 * item.
 */
bool gmosFormatCborDecodeTokenCount (gmosFormatCborParser_t* parser,
    uint16_t tokenIndex, uint16_t* tokenCount)
{
    bool tokenValid = false;
    gmosFormatCborToken_t token;
    uint_fast16_t tokenBufferOffset = tokenIndex * sizeof (token);

    // Get the token descriptor at the specified offset and access the
    // token count.
    if (gmosBufferRead (&(parser->tokenBuffer), tokenBufferOffset,
        (uint8_t*) &token, sizeof (token))) {
        *tokenCount = token.tokenCount;
        tokenValid = true;
    }
    return tokenValid;
}

/*
 * Checks for a CBOR null value at the specified parser token index
 * position.
 */
bool gmosFormatCborMatchNull (
    gmosFormatCborParser_t* parser, uint16_t tokenIndex)
{
    bool tokenMatch = false;
    gmosFormatCborToken_t token;
    uint_fast16_t tokenBufferOffset = tokenIndex * sizeof (token);

    // Get the token descriptor at the specified offset and check the
    // type specifier.
    if (gmosBufferRead (&(parser->tokenBuffer), tokenBufferOffset,
        (uint8_t*) &token, sizeof (token))) {
        if (token.typeSpecifier ==
            (GMOS_FORMAT_CBOR_MAJOR_TYPE_SIMPLE | 22)) {
            tokenMatch = true;
        }
    }
    return tokenMatch;
}

/*
 * Checks for a CBOR undefined value at the specified parser token index
 * position.
 */
bool gmosFormatCborMatchUndefined (gmosFormatCborParser_t* parser,
    uint16_t tokenIndex)
{
    bool tokenMatch = false;
    gmosFormatCborToken_t token;
    uint_fast16_t tokenBufferOffset = tokenIndex * sizeof (token);

    // Get the token descriptor at the specified offset and check the
    // type specifier.
    if (gmosBufferRead (&(parser->tokenBuffer), tokenBufferOffset,
        (uint8_t*) &token, sizeof (token))) {
        if (token.typeSpecifier ==
            (GMOS_FORMAT_CBOR_MAJOR_TYPE_SIMPLE | 23)) {
            tokenMatch = true;
        }
    }
    return tokenMatch;
}

/*
 * Decodes a CBOR boolean value at the specified parser token index
 * position.
 */
bool gmosFormatCborDecodeBool (gmosFormatCborParser_t* parser,
    uint16_t tokenIndex, bool* value)
{
    bool tokenValid = false;
    gmosFormatCborToken_t token;
    uint_fast16_t tokenBufferOffset = tokenIndex * sizeof (token);

    // Get the token descriptor at the specified offset and check the
    // type specifier.
    if (gmosBufferRead (&(parser->tokenBuffer), tokenBufferOffset,
        (uint8_t*) &token, sizeof (token))) {
        if (token.typeSpecifier ==
            (GMOS_FORMAT_CBOR_MAJOR_TYPE_SIMPLE | 20)) {
            *value = false;
            tokenValid = true;
        } else if (token.typeSpecifier ==
            (GMOS_FORMAT_CBOR_MAJOR_TYPE_SIMPLE | 21)) {
            *value = true;
            tokenValid = true;
        }
    }
    return tokenValid;
}

/*
 * Decodes a CBOR 32-bit unsigned integer value at the specified parser
 * token index position. The encoded value must be in the valid range
 * of the native 32-bit unsigned integer data type.
 */
bool gmosFormatCborDecodeUint32 (gmosFormatCborParser_t* parser,
    uint16_t tokenIndex, uint32_t* value)
{
    bool tokenValid = false;
    gmosFormatCborToken_t token;
    uint_fast16_t tokenBufferOffset = tokenIndex * sizeof (token);

    // Get the token descriptor at the specified offset and check the
    // type specifier and parameter range.
    if (gmosBufferRead (&(parser->tokenBuffer), tokenBufferOffset,
        (uint8_t*) &token, sizeof (token))) {
        if (((token.typeSpecifier & 0xE0) ==
            GMOS_FORMAT_CBOR_MAJOR_TYPE_INT_POS) &&
            (token.typeParam <= UINT32_MAX)) {
            *value = (uint32_t) token.typeParam;
            tokenValid = true;
        }
    }
    return tokenValid;
}

/*
 * Decodes a CBOR 32-bit signed integer value at the specified parser
 * token index position. The encoded value must be in the valid range
 * of the native 32-bit signed integer data type.
 */
bool gmosFormatCborDecodeInt32 (gmosFormatCborParser_t* parser,
    uint16_t tokenIndex, int32_t* value)
{
    bool tokenValid = false;
    gmosFormatCborToken_t token;
    uint_fast16_t tokenBufferOffset = tokenIndex * sizeof (token);

    // Get the token descriptor at the specified offset and check the
    // type specifier and parameter range.
    if (gmosBufferRead (&(parser->tokenBuffer), tokenBufferOffset,
        (uint8_t*) &token, sizeof (token))) {
        if (((token.typeSpecifier & 0xE0) ==
            GMOS_FORMAT_CBOR_MAJOR_TYPE_INT_POS) &&
            (token.typeParam <= INT32_MAX)) {
            *value = (int32_t) token.typeParam;
            tokenValid = true;
        } else if (((token.typeSpecifier & 0xE0) ==
            GMOS_FORMAT_CBOR_MAJOR_TYPE_INT_NEG) &&
            (token.typeParam <= -(INT32_MIN + 1))) {
            *value = (int32_t) ~(token.typeParam);
            tokenValid = true;
        }
    }
    return tokenValid;
}

/*
 * Decodes a CBOR 64-bit unsigned integer value at the specified parser
 * token index position. The encoded value must be in the valid range
 * of the native 64-bit unsigned integer data type.
 */
#if GMOS_CONFIG_CBOR_SUPPORT_64_BIT_VALUES
bool gmosFormatCborDecodeUint64 (gmosFormatCborParser_t* parser,
    uint16_t tokenIndex, uint64_t* value)
{
    bool tokenValid = false;
    gmosFormatCborToken_t token;
    uint_fast16_t tokenBufferOffset = tokenIndex * sizeof (token);

    // Get the token descriptor at the specified offset and check the
    // type specifier and parameter range.
    if (gmosBufferRead (&(parser->tokenBuffer), tokenBufferOffset,
        (uint8_t*) &token, sizeof (token))) {
        if (((token.typeSpecifier & 0xE0) ==
            GMOS_FORMAT_CBOR_MAJOR_TYPE_INT_POS) &&
            (token.typeParam <= UINT64_MAX)) {
            *value = (uint64_t) token.typeParam;
            tokenValid = true;
        }
    }
    return tokenValid;
}
#endif // GMOS_CONFIG_CBOR_SUPPORT_64_BIT_VALUES

/*
 * Decodes a CBOR 64-bit signed integer value at the specified parser
 * token index position. The encoded value must be in the valid range
 * of the native 64-bit signed integer data type.
 */
#if GMOS_CONFIG_CBOR_SUPPORT_64_BIT_VALUES
bool gmosFormatCborDecodeInt64 (gmosFormatCborParser_t* parser,
    uint16_t tokenIndex, int64_t* value)
{
    bool tokenValid = false;
    gmosFormatCborToken_t token;
    uint_fast16_t tokenBufferOffset = tokenIndex * sizeof (token);

    // Get the token descriptor at the specified offset and check the
    // type specifier and parameter range.
    if (gmosBufferRead (&(parser->tokenBuffer), tokenBufferOffset,
        (uint8_t*) &token, sizeof (token))) {
        if (((token.typeSpecifier & 0xE0) ==
            GMOS_FORMAT_CBOR_MAJOR_TYPE_INT_POS) &&
            (token.typeParam <= INT64_MAX)) {
            *value = (int64_t) token.typeParam;
            tokenValid = true;
        } else if (((token.typeSpecifier & 0xE0) ==
            GMOS_FORMAT_CBOR_MAJOR_TYPE_INT_NEG) &&
            (token.typeParam <= -(INT64_MIN + 1))) {
            *value = (int64_t) ~(token.typeParam);
            tokenValid = true;
        }
    }
    return tokenValid;
}
#endif // GMOS_CONFIG_CBOR_SUPPORT_64_BIT_VALUES

/*
 * Decodes a CBOR 32 bit floating point value at the specified parser
 * token index position. The encoded value must be in a valid format
 * for the IEEE 754 32 bit floating point data type.
 */
#if GMOS_CONFIG_CBOR_SUPPORT_FLOAT_VALUES
bool gmosFormatCborDecodeFloat32 (gmosFormatCborParser_t* parser,
    uint16_t tokenIndex, float* value)
{
    bool tokenValid = false;
    gmosFormatCborToken_t token;
    uint_fast16_t tokenBufferOffset = tokenIndex * sizeof (token);

    // Use a union rather than type punning to process the raw
    // representation of the floating point value.
    union { float value; uint32_t bits; } data;

    // Get the token descriptor at the specified offset and check the
    // type specifier.
    if (gmosBufferRead (&(parser->tokenBuffer), tokenBufferOffset,
        (uint8_t*) &token, sizeof (token))) {
        if (token.typeSpecifier ==
            (GMOS_FORMAT_CBOR_MAJOR_TYPE_SIMPLE | 26)) {
            data.bits = (uint32_t) token.typeParam;
            *value = data.value;
            tokenValid = true;
        }
    }
    return tokenValid;
}
#endif

/*
 * Decodes a CBOR 64 bit floating point value at the specified parser
 * token index position. The encoded value must be in a valid format
 * for the IEEE 754 64 bit floating point data type.
 */
#if GMOS_CONFIG_CBOR_SUPPORT_FLOAT_VALUES
#if GMOS_CONFIG_CBOR_SUPPORT_64_BIT_VALUES
bool gmosFormatCborDecodeFloat64 (gmosFormatCborParser_t* parser,
    uint16_t tokenIndex, double* value)
{
    bool tokenValid = false;
    gmosFormatCborToken_t token;
    uint_fast16_t tokenBufferOffset = tokenIndex * sizeof (token);

    // Use a union rather than type punning to process the raw
    // representation of the floating point value.
    union { double value; uint64_t bits; } data;

    // Get the token descriptor at the specified offset and check the
    // type specifier.
    if (gmosBufferRead (&(parser->tokenBuffer), tokenBufferOffset,
        (uint8_t*) &token, sizeof (token))) {
        if (token.typeSpecifier ==
            (GMOS_FORMAT_CBOR_MAJOR_TYPE_SIMPLE | 27)) {
            data.bits = (uint64_t) token.typeParam;
            *value = data.value;
            tokenValid = true;
        }

        // Support implicit conversion from float to double.
        else {
            float value32;
            if (gmosFormatCborDecodeFloat32 (
                parser, tokenIndex, &value32)) {
                *value = (double) value32;
                tokenValid = true;
            }
        }
    }
    return tokenValid;
}
#endif
#endif

/*
 * Checks for a CBOR text string at the specified parser token index
 * position and compares it to a conventional 'C' null terminated
 * character string.
 */
bool gmosFormatCborMatchCharString (gmosFormatCborParser_t* parser,
    uint16_t tokenIndex, const char* textString)
{
    uint_fast16_t length;

    // Check for null termination within the maximum string size.
    for (length = 0;
        length <= GMOS_CONFIG_CBOR_MAX_STRING_SIZE; length++) {
        if (textString [length] == '\0') {
            break;
        }
    }

    // Use the fixed length matching function.
    return gmosFormatCborMatchTextString (
        parser, tokenIndex, textString, length);
}

/*
 * Checks for a CBOR text string at the specified parser token index
 * position and compares it to a string of the specified length.
 */
bool gmosFormatCborMatchTextString (gmosFormatCborParser_t* parser,
    uint16_t tokenIndex, const char* textString, uint16_t length)
{
    gmosFormatCborToken_t token;
    uint_fast16_t tokenBufferOffset = tokenIndex * sizeof (token);
    uint_fast16_t matchOffset = 0;
    uint8_t blockData [16];
    uint_fast16_t blockSize;
    uint_fast8_t i;
    bool matchOk = false;

    // Get the token descriptor at the specified offset and check the
    // type specifier and string length.
    if ((length <= GMOS_CONFIG_CBOR_MAX_STRING_SIZE) &&
        (gmosBufferRead (&(parser->tokenBuffer), tokenBufferOffset,
        (uint8_t*) &token, sizeof (token)))) {
        if (((token.typeSpecifier & 0xE0) ==
            (GMOS_FORMAT_CBOR_MAJOR_TYPE_STR_TEXT)) &&
            ((token.typeSpecifier & 0x1F) != 31) &&
            (token.typeParam == length)) {
                matchOk = true;
        }
    }

    // Perform matching on blocks of data.
    while (matchOk && (matchOffset < length)) {
        blockSize = sizeof (blockData);
        if (length - matchOffset < blockSize) {
            blockSize = length - matchOffset;
        }
        if (!gmosBufferRead (&(parser->messageBuffer),
            token.dataOffset + matchOffset, blockData, blockSize)) {
            matchOk = false;
        } else {
            for (i = 0; i < blockSize; i++) {
                if (blockData [i] !=
                    ((uint8_t) (textString [matchOffset + i]))) {
                    matchOk = false;
                }
            }
        }
        matchOffset += blockSize;
    }
    return matchOk;
}

/*
 * Decodes a UTF-8 encoded text string, placing the results in a
 * pre-allocated character array with null termination. The source must
 * be a finite length CBOR text string.
 */
bool gmosFormatCborDecodeTextString (gmosFormatCborParser_t* parser,
    uint16_t tokenIndex, char* stringBuf, uint16_t stringBufLen,
    uint16_t* sourceLen)
{
    bool tokenValid = false;
    gmosFormatCborToken_t token;
    uint_fast16_t tokenBufferOffset = tokenIndex * sizeof (token);
    uint_fast16_t copySize;

    // Get the token descriptor at the specified offset and check the
    // type specifier.
    if (gmosBufferRead (&(parser->tokenBuffer), tokenBufferOffset,
        (uint8_t*) &token, sizeof (token))) {
        if (((token.typeSpecifier & 0xE0) ==
            (GMOS_FORMAT_CBOR_MAJOR_TYPE_STR_TEXT)) &&
            ((token.typeSpecifier & 0x1F) != 31)) {
            if (token.typeParam >= stringBufLen) {
                copySize = stringBufLen - 1;
            } else {
                copySize = token.typeParam;
            }

            // Attempt to read the string data from the buffer and add
            // null termination.
            if (gmosBufferRead (&(parser->messageBuffer),
                token.dataOffset, (uint8_t*) stringBuf, copySize)) {
                stringBuf [copySize] = '\0';
                if (sourceLen != NULL) {
                    *sourceLen = token.typeParam;
                }
                tokenValid = true;
            }
        }
    }
    return tokenValid;
}

/*
 * Decodes a CBOR byte string, placing the results in a pre-allocated
 * byte array. The source must be a finite length CBOR byte string.
 */
bool gmosFormatCborDecodeByteString (gmosFormatCborParser_t* parser,
    uint16_t tokenIndex, uint8_t* byteBuf, uint16_t byteBufLen,
    uint16_t* sourceLen)
{
    bool tokenValid = false;
    gmosFormatCborToken_t token;
    uint_fast16_t tokenBufferOffset = tokenIndex * sizeof (token);
    uint_fast16_t copySize;

    // Get the token descriptor at the specified offset and check the
    // type specifier.
    if (gmosBufferRead (&(parser->tokenBuffer), tokenBufferOffset,
        (uint8_t*) &token, sizeof (token))) {
        if (((token.typeSpecifier & 0xE0) ==
            (GMOS_FORMAT_CBOR_MAJOR_TYPE_STR_BYTE)) &&
            ((token.typeSpecifier & 0x1F) != 31)) {
            if (token.typeParam > byteBufLen) {
                copySize = byteBufLen;
            } else {
                copySize = token.typeParam;
            }

            // Attempt to read the string data from the buffer.
            if (gmosBufferRead (&(parser->messageBuffer),
                token.dataOffset, byteBuf, copySize)) {
                if (sourceLen != NULL) {
                    *sourceLen = token.typeParam;
                }
                tokenValid = true;
            }
        }
    }
    return tokenValid;
}

/*
 * Decodes the CBOR descriptor for a fixed or indefinite length array
 * and indicates the number of elements in the array.
 */
bool gmosFormatCborDecodeArray (gmosFormatCborParser_t* parser,
    uint16_t tokenIndex, uint16_t* length)
{
    bool tokenValid = false;
    gmosFormatCborToken_t token;
    uint_fast16_t tokenBufferOffset = tokenIndex * sizeof (token);

    // Get the token descriptor at the specified offset and check the
    // type specifier and parameter range.
    if (gmosBufferRead (&(parser->tokenBuffer), tokenBufferOffset,
        (uint8_t*) &token, sizeof (token))) {
        if (((token.typeSpecifier & 0xE0) ==
            GMOS_FORMAT_CBOR_MAJOR_TYPE_ARRAY) &&
            (token.typeParam <= GMOS_CONFIG_CBOR_MAX_ARRAY_SIZE)) {
            *length = (uint16_t) token.typeParam;
            tokenValid = true;
        }
    }
    return tokenValid;
}

/*
 * Performs an integer index lookup on a fixed or indefinite length
 * array, setting the associated value token index on success.
 */
bool gmosFormatCborLookupArrayEntry (gmosFormatCborParser_t* parser,
    uint16_t tokenIndex, uint16_t arrayIndex, uint16_t* valueIndex)
{
    bool matchOk = false;
    uint16_t arrayLength;
    uint16_t tokenCount;
    uint_fast16_t i;

    // Check that there is a valid array at the specified token index
    // and that the index value is in range.
    if ((gmosFormatCborDecodeArray (parser, tokenIndex, &arrayLength)) &&
        (arrayIndex < arrayLength)) {
        matchOk = true;
        tokenIndex += 1;
        for (i = 0; i < arrayIndex; i++) {
            if (gmosFormatCborDecodeTokenCount (
                parser, tokenIndex, &tokenCount)) {
                tokenIndex += tokenCount;
            } else {
                matchOk = false;
                break;
            }
        }
    }

    // Update the value index on successful completion.
    if (matchOk) {
        *valueIndex = tokenIndex;
    }
    return matchOk;
}

/*
 * Decodes the CBOR descriptor for a fixed or indefinite length map
 * and indicates the number of elements in the map.
 */
bool gmosFormatCborDecodeMap (gmosFormatCborParser_t* parser,
    uint16_t tokenIndex, uint16_t* length)
{
    bool tokenValid = false;
    gmosFormatCborToken_t token;
    uint_fast16_t tokenBufferOffset = tokenIndex * sizeof (token);

    // Get the token descriptor at the specified offset and check the
    // type specifier and parameter range.
    if (gmosBufferRead (&(parser->tokenBuffer), tokenBufferOffset,
        (uint8_t*) &token, sizeof (token))) {
        if (((token.typeSpecifier & 0xE0) ==
            GMOS_FORMAT_CBOR_MAJOR_TYPE_MAP) &&
            (token.typeParam <= GMOS_CONFIG_CBOR_MAX_MAP_SIZE)) {
            *length = (uint16_t) token.typeParam;
            tokenValid = true;
        }
    }
    return tokenValid;
}

/*
 * Performs an integer key lookup on a fixed or indefinite length map,
 * setting the associated value token index on success.
 */
bool gmosFormatCborLookupMapIntKey (gmosFormatCborParser_t* parser,
    uint16_t tokenIndex, gmosFormatCborMapIntKey_t key,
    uint16_t* valueIndex)
{
    bool matchOk = false;
    uint16_t mapLength;
    uint16_t tokenCount;
    uint_fast16_t i;
    gmosFormatCborMapIntKey_t matchKey;

    // Check that there is a valid map at the specified token index and
    // then search for a matching key.
    if (gmosFormatCborDecodeMap (parser, tokenIndex, &mapLength)) {
        tokenIndex += 1;
        for (i = 0; i < mapLength; i++) {
            if ((gmosFormatCborDecodeMapIntKey (
                parser, tokenIndex, &matchKey)) &&
                (key == matchKey)) {
                *valueIndex = tokenIndex + 1;
                matchOk = true;
                break;
            } else if (gmosFormatCborDecodeTokenCount (
                parser, tokenIndex + 1, &tokenCount)) {
                tokenIndex += 1 + tokenCount;
            } else {
                break;
            }
        }
    }
    return matchOk;
}

/*
 * Performs a character string key lookup on a fixed or indefinite
 * length map, using a conventional null terminated 'C' string as the
 * key and setting the associated value token index on success.
 */
bool gmosFormatCborLookupMapCharKey (
    gmosFormatCborParser_t* parser, uint16_t tokenIndex,
    const char* key, uint16_t* valueIndex)
{
    uint_fast16_t keyLength;

    // Check for null termination within the maximum string size.
    for (keyLength = 0;
        keyLength <= GMOS_CONFIG_CBOR_MAX_STRING_SIZE; keyLength++) {
        if (key [keyLength] == '\0') {
            break;
        }
    }

    // Use the fixed length key index function.
    return gmosFormatCborLookupMapTextKey (
        parser, tokenIndex, key, keyLength, valueIndex);
}

/*
 * Performs a text string key lookup on a fixed or indefinite length
 * map, using a text string of the specified length as the key and
 * setting the associated value token index on success.
 */
bool gmosFormatCborLookupMapTextKey (
    gmosFormatCborParser_t* parser, uint16_t tokenIndex,
    const char* key, uint16_t keyLength, uint16_t* valueIndex)
{
    bool matchOk = false;
    uint16_t mapLength;
    uint16_t tokenCount;
    uint_fast16_t i;

    // Check that there is a valid map at the specified token index and
    // then search for a matching key.
    if ((keyLength <= GMOS_CONFIG_CBOR_MAX_STRING_SIZE) &&
        (gmosFormatCborDecodeMap (parser, tokenIndex, &mapLength))) {
        tokenIndex += 1;
        for (i = 0; i < mapLength; i++) {
            if (gmosFormatCborMatchTextString (
                parser, tokenIndex, key, keyLength)) {
                *valueIndex = tokenIndex + 1;
                matchOk = true;
                break;
            } else if (gmosFormatCborDecodeTokenCount (
                parser, tokenIndex + 1, &tokenCount)) {
                tokenIndex += 1 + tokenCount;
            } else {
                break;
            }
        }
    }
    return matchOk;
}

/*
 * Decodes the CBOR descriptor for a tag and indicates the tag number.
 * It should then be followed by a single tag content value.
 */
bool gmosFormatCborDecodeTag (gmosFormatCborParser_t* parser,
    uint16_t tokenIndex, gmosFormatCborTypeParam_t* tagNumber)
{
    bool tokenValid = false;
    gmosFormatCborToken_t token;
    uint_fast16_t tokenBufferOffset = tokenIndex * sizeof (token);

    // Get the token descriptor at the specified offset and check the
    // type specifier and parameter range.
    if (gmosBufferRead (&(parser->tokenBuffer), tokenBufferOffset,
        (uint8_t*) &token, sizeof (token))) {
        if ((token.typeSpecifier & 0xE0) ==
            GMOS_FORMAT_CBOR_MAJOR_TYPE_TAG) {
            *tagNumber = token.typeParam;
            tokenValid = true;
        }
    }
    return tokenValid;
}
