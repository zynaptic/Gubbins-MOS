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
 * This file provides support for encoding NFC Forum NDEF data items and
 * appending them to a GubbinsMOS buffer.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-buffers.h"
#include "gmos-driver-nfctag-ndef.h"

/*
 * Initialises an NDEF record builder structure ready for use.
 */
bool gmosDriverNfcTagNdefBuilderInit (
    gmosDriverNfcTagNdefBuilder_t* builder,
    gmosDriverNfcTagNdefTnf_t tnfOption)
{
    // Check for valid TNF option.
    bool initOk = true;
    if (tnfOption >= GMOS_DRIVER_NFC_TAG_NDEF_TNF_RESERVED) {
        initOk = false;
    }

    // Assign the default settings.
    if (initOk) {
        builder->headerByte = tnfOption;
        builder->typeData = NULL;
        builder->typeSize = 0;
        builder->idData = NULL;
        builder->idSize = 0;
        builder->payloadData = NULL;
        builder->payloadSize = 0;
    }
    return initOk;
}

/*
 * Sets the NDEF message begin, message end and record chunk flags for
 * the specified record builder.
 */
bool gmosDriverNfcTagNdefBuilderSetFlags (
   gmosDriverNfcTagNdefBuilder_t* builder,
   gmosDriverNfcTagNdefFlag_t flags)
{
    uint_fast8_t headerByte = builder->headerByte;
    uint_fast8_t headerMask =
        GMOS_DRIVER_NFC_TAG_NDEF_FLAG_MSG_BEGIN |
        GMOS_DRIVER_NFC_TAG_NDEF_FLAG_MSG_END |
        GMOS_DRIVER_NFC_TAG_NDEF_FLAG_CHUNKED;
    bool setOk;

    // The chunk flag and message end flag must not be set in the same
    // record.
    if (((flags & GMOS_DRIVER_NFC_TAG_NDEF_FLAG_CHUNKED) != 0) &&
        ((flags & GMOS_DRIVER_NFC_TAG_NDEF_FLAG_MSG_END) != 0)) {
        setOk = false;
    } else {
        headerByte &= ~headerMask;
        headerByte |= headerMask & flags;
        builder->headerByte = headerByte;
        setOk = true;
    }
    return setOk;
}

/*
 * Sets the NDEF record type for the specified record builder.
 */
bool gmosDriverNfcTagNdefBuilderSetType (
    gmosDriverNfcTagNdefBuilder_t* builder,
    uint8_t* typeData, uint8_t typeSize)
{
    uint8_t tnfOption = builder->headerByte & 0x07;
    bool setOk = true;

    // The type data and size fields must contain valid data.
    if ((typeData == NULL) || (typeSize == 0)) {
        setOk = false;
    }

    // The type field should not be present for empty, unknown and
    // continued chunk records.
    if ((tnfOption == GMOS_DRIVER_NFC_TAG_NDEF_TNF_EMPTY) ||
        (tnfOption == GMOS_DRIVER_NFC_TAG_NDEF_TNF_UNKNOWN) ||
        (tnfOption == GMOS_DRIVER_NFC_TAG_NDEF_TNF_UNCHANGED)) {
        setOk = false;
    }

    // Update type settings.
    if (setOk) {
        builder->typeData = typeData;
        builder->typeSize = typeSize;
    }
    return setOk;
}

/*
 * Sets the NDEF record ID for the specified record builder.
 */
bool gmosDriverNfcTagNdefBuilderSetId (
    gmosDriverNfcTagNdefBuilder_t* builder,
    uint8_t* idData, uint8_t idSize)
{
    uint8_t tnfOption = builder->headerByte & 0x07;
    bool setOk = true;

    // The ID data and size fields must contain valid data.
    if ((idData == NULL) || (idSize == 0)) {
        setOk = false;
    }

    // The ID field should not be present for empty and continued
    // chunk records.
    if ((tnfOption == GMOS_DRIVER_NFC_TAG_NDEF_TNF_EMPTY) ||
        (tnfOption == GMOS_DRIVER_NFC_TAG_NDEF_TNF_UNCHANGED)) {
        setOk = false;
    }

    // Update ID settings.
    if (setOk) {
        builder->idData = idData;
        builder->idSize = idSize;
    }
    return setOk;
}

/*
 * Sets the NDEF record payload for the specified record builder.
 */
bool gmosDriverNfcTagNdefBuilderSetPayload (
    gmosDriverNfcTagNdefBuilder_t* builder,
    uint8_t* payloadData, uint16_t payloadSize)
{
    uint8_t tnfOption = builder->headerByte & 0x07;
    bool setOk = true;

    // The payload data and size fields must contain valid data.
    if ((payloadData == NULL) || (payloadSize == 0)) {
        setOk = false;
    }

    // The payload field should not be present for empty records.
    if (tnfOption == GMOS_DRIVER_NFC_TAG_NDEF_TNF_EMPTY) {
        setOk = false;
    }

    // Update payload settings.
    if (setOk) {
        builder->payloadData = payloadData;
        builder->payloadSize = payloadSize;
    }
    return setOk;
}

/*
 * Encodes the contents of an NDEF record builder and appends them to
 * the end of the specified buffer.
 */
bool gmosDriverNfcTagNdefBuilderEncode (
    gmosDriverNfcTagNdefBuilder_t* builder, gmosBuffer_t* buffer)
{
    uint_fast8_t headerByte;
    uint8_t recordHeader [7];
    uint_fast8_t headerSize = 1;
    uint_fast16_t recordOffset = gmosBufferGetSize (buffer);
    bool encodeOk = true;

    // Insert the type length field.
    recordHeader [headerSize++] = builder->typeSize;

    // Insert the payload length field, setting the short record flag
    // for length values that can fit in a single byte. Note that the
    // implementation limits the payload size to a 16 bit value.
    headerByte = builder->headerByte;
    if (builder->payloadSize < 256) {
        headerByte |= GMOS_DRIVER_NFC_TAG_NDEF_FLAG_SHORT_RECORD;
    } else {
        recordHeader [headerSize++] = 0;
        recordHeader [headerSize++] = 0;
        recordHeader [headerSize++] = (uint8_t) (builder->payloadSize >> 8);
    }
    recordHeader [headerSize++] = (uint8_t) builder->payloadSize;

    // Automatically set the message begin bit if the buffer is empty.
    if (recordOffset == 0) {
        headerByte |= GMOS_DRIVER_NFC_TAG_NDEF_FLAG_MSG_BEGIN;
    }

    // Insert the optional ID length field.
    if (builder->idSize > 0) {
        headerByte |= GMOS_DRIVER_NFC_TAG_NDEF_FLAG_ID_PRESENT;
        recordHeader [headerSize++] = builder->idSize;
    }

    // Append the header to the buffer.
    recordHeader [0] = headerByte;
    encodeOk = encodeOk && gmosBufferAppend (buffer,
        recordHeader, headerSize);

    // Append the type information to the buffer.
    if (builder->typeSize > 0) {
        encodeOk = encodeOk && gmosBufferAppend (buffer,
            builder->typeData, builder->typeSize);
    }

    // Append the record ID information to the buffer.
    if (builder->idSize > 0) {
        encodeOk = encodeOk && gmosBufferAppend (buffer,
            builder->idData, builder->idSize);
    }

    // Append the payload data to the buffer.
    if (builder->payloadSize > 0) {
        encodeOk = encodeOk && gmosBufferAppend (buffer,
            builder->payloadData, builder->payloadSize);
    }

    // Discard any changes to the buffer contents on failure.
    if (!encodeOk) {
        gmosBufferResize (buffer, recordOffset);
    }
    return encodeOk;
}
