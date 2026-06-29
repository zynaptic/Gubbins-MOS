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
 * This file provides support for decoding NFC Forum NDEF data items
 * stored in a GubbinsMOS buffer.
 */

#include <stdint.h>
#include <stdbool.h>

#include "gmos-config.h"
#include "gmos-buffers.h"
#include "gmos-driver-nfctag-ndef.h"

/*
 * Parses a single NDEF record at the specified offset in the source
 * buffer. This extracts the raw NDEF data into a token data structure.
 */
static uint_fast16_t gmosDriverNfcTagNdefParserScanRawRecord (
    gmosDriverNfcTagNdefParser_t* parser,
    gmosDriverNfcTagNdefToken_t* token, uint_fast16_t tokenOffset)
{
    gmosBuffer_t* messageBuffer;
    uint_fast16_t newTokenOffset = 0;
    uint8_t recordHeader [7];
    uint8_t* headerPtr;
    uint_fast8_t headerByte;
    uint_fast8_t headerSize;
    uint_fast8_t typeSize;
    uint_fast8_t idSize;
    uint32_t payloadSize;
    bool hasIdSize = false;
    bool hasFullPayloadSize = false;

    // Read the minimal header from the message buffer.
    messageBuffer = parser->sourceBuffer;
    if (!gmosBufferRead (messageBuffer, tokenOffset, recordHeader, 3)) {
        goto out;
    }

    // Parse the minimal header contents.
    headerPtr = recordHeader;
    headerByte = *(headerPtr++);
    typeSize = *(headerPtr++);
    payloadSize = *(headerPtr++);

    // Calculate the full header size based on the header byte flags.
    headerSize = 3;
    if ((headerByte & GMOS_DRIVER_NFC_TAG_NDEF_FLAG_ID_PRESENT) != 0) {
        hasIdSize = true;
        headerSize += 1;
    }
    if ((headerByte & GMOS_DRIVER_NFC_TAG_NDEF_FLAG_SHORT_RECORD) == 0) {
        hasFullPayloadSize = true;
        headerSize += 3;
    }

    // Read the extended header from the message buffer.
    if (headerSize > 3) {
        if (!gmosBufferRead (messageBuffer, tokenOffset + 3,
            headerPtr, headerSize - 3)) {
            goto out;
        }
    }

    // Parse the extended header contents. Note that payload lengths are
    // restricted to 16-bit values by the implementation.
    if (hasFullPayloadSize) {
        payloadSize = (payloadSize << 8) | *(headerPtr++);
        payloadSize = (payloadSize << 8) | *(headerPtr++);
        payloadSize = (payloadSize << 8) | *(headerPtr++);
    }
    if (payloadSize > 0xFFFF) {
        goto out;
    }
    if (hasIdSize) {
        idSize = *(headerPtr++);
    } else {
        idSize = 0;
    }

    // Populate the token data structure.
    token->sourceBuffer = parser->sourceBuffer;
    token->headerByte = headerByte;
    token->typeSize = typeSize;
    token->typeOffset = tokenOffset + headerSize;
    token->idSize = idSize;
    token->idOffset = token->typeOffset + typeSize;
    token->payloadSize = payloadSize;
    token->payloadOffset = token->idOffset + idSize;
    newTokenOffset = token->payloadOffset + payloadSize;

    // Check for insufficient data in the buffer.
    if (newTokenOffset > gmosBufferGetSize (messageBuffer)) {
        newTokenOffset = 0;
    }

    // Return the new token offset, or zero on failure.
out :
    return newTokenOffset;
}

/*
 * Parses a single NDEF record as a conventional record.
 */
static inline uint_fast16_t gmosDriverNfcTagNdefParserScanRecord (
    gmosDriverNfcTagNdefParser_t* parser,
    gmosDriverNfcTagNdefToken_t* token, uint_fast16_t tokenOffset)
{
    uint_fast16_t newTokenOffset;

    // Parse the raw NDEF record.
    newTokenOffset = gmosDriverNfcTagNdefParserScanRawRecord (
        parser, token, tokenOffset);
    if (newTokenOffset == 0) {
        goto out;
    }

    // Check the various additional record format constraints.
    switch (token->headerByte & 0x07) {
        case GMOS_DRIVER_NFC_TAG_NDEF_TNF_EMPTY :
            if ((token->typeSize != 0) ||
                (token->idSize != 0) ||
                (token->payloadSize != 0)) {
                newTokenOffset = 0;
            }
            break;
        case GMOS_DRIVER_NFC_TAG_NDEF_TNF_UNKNOWN :
            if (token->typeSize != 0) {
                newTokenOffset = 0;
            }
            break;
        case GMOS_DRIVER_NFC_TAG_NDEF_TNF_UNCHANGED :
        case GMOS_DRIVER_NFC_TAG_NDEF_TNF_RESERVED :
            newTokenOffset = 0;
            break;
    }

    // Return the new token offset, or zero on failure.
out :
    return newTokenOffset;
}

/*
 * Parses a single NDEF record as a chunk record.
 */
static inline uint_fast16_t gmosDriverNfcTagNdefParserScanChunk (
    gmosDriverNfcTagNdefParser_t* parser,
    gmosDriverNfcTagNdefToken_t* token, uint_fast16_t tokenOffset)
{
    uint_fast16_t newTokenOffset;

    // Parse the raw NDEF record.
    newTokenOffset = gmosDriverNfcTagNdefParserScanRawRecord (
        parser, token, tokenOffset);

    // Check for valid chunk record. This must have the type name format
    // set to 'unchanged' and both the record type and record ID fields
    // must be empty.
    if (newTokenOffset != 0) {
        if (((token->headerByte & 0x07) !=
            GMOS_DRIVER_NFC_TAG_NDEF_TNF_UNCHANGED) ||
            (token->typeSize != 0) || (token->idSize != 0)) {
            newTokenOffset = 0;
        }
    }
    return newTokenOffset;
}

/*
 * This initialises an NDEF parser by scanning an NDEF message held in
 * the specified source buffer.
 */
bool gmosDriverNfcTagNdefParserScan (
    gmosDriverNfcTagNdefParser_t* parser,
    gmosBuffer_t* sourceBuffer, uint16_t sourceOffset)
{
    gmosDriverNfcTagNdefToken_t token;
    uint_fast16_t nextTokenOffset = sourceOffset;
    bool firstRecord = true;
    bool finalRecord = false;
    bool chunkActive = false;
    bool parsedOk = true;

    // Reset parser state.
    parser->sourceBuffer = sourceBuffer;
    gmosBufferInit (&(parser->tokenBuffer));

    // Process each record in turn.
    while (!finalRecord) {

        // Check conventional records for correct format.
        if (!chunkActive) {
            nextTokenOffset = gmosDriverNfcTagNdefParserScanRecord (
                parser, &token, nextTokenOffset);
        }

        // Check record chunks for correct format.
        else {
            nextTokenOffset = gmosDriverNfcTagNdefParserScanChunk (
                parser, &token, nextTokenOffset);
        }

        // Skip further processing on parser failure.
        if (nextTokenOffset == 0) {
            parsedOk = false;
            break;
        }

        // Check for a begin message flag on the first record only.
        if (firstRecord) {
            firstRecord = false;
            if ((token.headerByte &
                GMOS_DRIVER_NFC_TAG_NDEF_FLAG_MSG_BEGIN) == 0) {
                parsedOk = false;
                break;
            }
        } else {
            if ((token.headerByte &
                GMOS_DRIVER_NFC_TAG_NDEF_FLAG_MSG_BEGIN) != 0) {
                parsedOk = false;
                break;
            }
        }

        // Update chunk and final record flags. These should never be
        // set in the same record.
        chunkActive = ((token.headerByte &
            GMOS_DRIVER_NFC_TAG_NDEF_FLAG_CHUNKED) != 0) ? true : false;
        finalRecord = ((token.headerByte &
            GMOS_DRIVER_NFC_TAG_NDEF_FLAG_MSG_END) != 0) ? true : false;
        if (chunkActive && finalRecord) {
            parsedOk = false;
            break;
        }

        // Append the extracted token data structure to the token buffer.
        if (!gmosBufferAppend (&(parser->tokenBuffer),
            (uint8_t*) &token, sizeof (token))) {
            parsedOk = false;
            break;
        }
    }

    // Reset buffers on parsing failure.
    if (!parsedOk) {
        parser->sourceBuffer = NULL;
        gmosBufferReset (&(parser->tokenBuffer), 0);
    }
    return parsedOk;
}

/*
 * Resets the state of the parser and releases any resources allocated
 * by an NDEF parser during processing.
 */
void gmosDriverNfcTagNdefParserReset (
    gmosDriverNfcTagNdefParser_t* parser)
{
    parser->sourceBuffer = NULL;
    gmosBufferReset (&(parser->tokenBuffer), 0);
}
/*
 * Reads back the NDEF token at the specified parser token offset.
 */
uint8_t gmosDriverNfcTagNdefParserReadToken (
    gmosDriverNfcTagNdefParser_t* parser,
    uint16_t tokenIndex, gmosDriverNfcTagNdefToken_t* token)
{
    bool readOk;
    uint_fast16_t tokenBufferOffset;
    uint_fast8_t tnfValue;

    // Read back the token data structre from the token buffer.
    tokenBufferOffset = tokenIndex * sizeof (gmosDriverNfcTagNdefToken_t);
    readOk = gmosBufferRead (&(parser->tokenBuffer), tokenBufferOffset,
        (uint8_t*) token, sizeof (gmosDriverNfcTagNdefToken_t));

    // Extract the TNF value from the token.
    if (readOk) {
        tnfValue = token->headerByte & 0x07;
    } else {
        tnfValue = GMOS_DRIVER_NFC_TAG_NDEF_TNF_INVALID;
    }
    return tnfValue;
}

/*
 * Gets the type data field associated with an NDEF record, updating the
 * specified type data array value.
 */
uint8_t gmosDriverNfcTagNdefParserGetType (
    gmosDriverNfcTagNdefToken_t* token,
    uint8_t* typeData, uint8_t typeDataSize)
{
    uint_fast16_t copySize;

    // Truncate the copied data if required.
    copySize = token->typeSize;
    if (copySize > typeDataSize) {
        copySize = typeDataSize;
    }
    if (copySize > 0) {
        gmosBufferRead (token->sourceBuffer,
            token->typeOffset, typeData, copySize);
    }
    return token->typeSize;
}

/*
 * Gets the record ID data field associated with an NDEF record,
 * updating the specified record ID data array value.
 */
uint8_t gmosDriverNfcTagNdefParserGetId (
    gmosDriverNfcTagNdefToken_t* token,
    uint8_t* idData, uint8_t idDataSize)
{
    uint_fast16_t copySize;

    // Truncate the copied data if required.
    copySize = token->idSize;
    if (copySize > idDataSize) {
        copySize = idDataSize;
    }
    if (copySize > 0) {
        gmosBufferRead (token->sourceBuffer,
            token->idOffset, idData, copySize);
    }
    return token->idSize;
}

/*
 * Gets the payload data field associated with an NDEF record, updating
 * the specified payload data array value.
 */
uint16_t gmosDriverNfcTagNdefParserGetPayload (
    gmosDriverNfcTagNdefToken_t* token,
    uint8_t* payloadData, uint16_t payloadDataSize)
{
    uint_fast16_t copySize;

    // Truncate the copied data if required.
    copySize = token->payloadSize;
    if (copySize > payloadDataSize) {
        copySize = payloadDataSize;
    }
    if (copySize > 0) {
        gmosBufferRead (token->sourceBuffer,
            token->payloadOffset, payloadData, copySize);
    }
    return token->payloadSize;
}
