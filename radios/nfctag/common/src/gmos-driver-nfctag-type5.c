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
 * This file implements the common API for processing NFC Type 5 data
 * formats.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-buffers.h"
#include "gmos-driver-nfctag-type5.h"

/*
 * Appends an NFC Type 5 capability container to the specified target
 * buffer.
 */
bool gmosDriverNfcTagType5FormatCC (gmosBuffer_t* targetBuffer,
    uint32_t memorySize, bool writeEnable, bool forceExtendedCC)
{
    uint8_t ccHeader [8];
    uint_fast8_t versionByte;
    uint_fast8_t featureByte;
    uint32_t memLen;
    bool formatOk;

    // The memory size is expressed as the number of bytes allocated for
    // storing the NDEF message. It must be divisible by 8.
    if ((memorySize & 0x07) != 0) {
        formatOk = false;
        goto out;
    }

    // Set the version and access byte. The version is fixed at 1.0 and
    // the data is always readable. NFC write support is optional.
    if (writeEnable) {
        versionByte = 0x40;
    } else {
        versionByte = 0x43;
    }

    // Set the feature byte. This enables multi block reads and disables
    // all other options by default.
    featureByte = 0x01;

    // Use 4 byte encoding for memory lengths up to 2040 bytes.
    memLen = memorySize / 8;
    if (!forceExtendedCC && (memLen <= 0x00FF)) {
        ccHeader [0] = 0xE1;
        ccHeader [1] = versionByte;
        ccHeader [2] = (uint8_t) memLen;
        ccHeader [3] = featureByte;
        formatOk = gmosBufferAppend (targetBuffer, ccHeader, 4);
    }

    // Use 8 byte encoding for memory lengths of 2048 and over.
    else if (memLen <= 0xFFFF) {
        ccHeader [0] = 0xE2;
        ccHeader [1] = versionByte;
        ccHeader [2] = 0x00;
        ccHeader [3] = featureByte;
        ccHeader [4] = 0x00;
        ccHeader [5] = 0x00;
        ccHeader [6] = (uint8_t) (memLen >> 8);
        ccHeader [7] = (uint8_t) memLen;
        formatOk = gmosBufferAppend (targetBuffer, ccHeader, 8);
    } else {
        formatOk = false;
    }
out :
    return formatOk;
}

/*
 * Appends an NFC Type 5 TLV record to the specified target buffer.
 */
bool gmosDriverNfcTagType5FormatTLV (gmosBuffer_t* targetBuffer,
    uint8_t type, gmosBuffer_t* value)
{
    uint8_t tlvHeader [4];
    uint_fast8_t headerSize;
    uint_fast16_t length;
    bool formatOk;

    // If no value is supplied or a known short record type as been
    // specified, only the tag is used in the TLV record.
    if ((value == NULL) ||
        (type == GMOS_DRIVER_NFC_TAG_TYPE5_NULL_TYPE) ||
        (type == GMOS_DRIVER_NFC_TAG_TYPE5_TERM_TYPE))
     {
        tlvHeader [0] = type;
        headerSize = 1;
    }

    // Select the appropriate length encoding.
    else {
        length = gmosBufferGetSize (value);
        if (length <= 0xFE) {
            tlvHeader [0] = type;
            tlvHeader [1] = (uint8_t) length;
            headerSize = 2;
        } else if (length <= 0xFFFE) {
            tlvHeader [0] = type;
            tlvHeader [1] = 0xFF;
            tlvHeader [2] = (uint8_t) (length >> 8);
            tlvHeader [3] = (uint8_t) length;
            headerSize = 4;
        } else {
            formatOk = false;
            goto out;
        }
    }

    // Append the TLV header to the target buffer.
    formatOk = gmosBufferAppend (targetBuffer, tlvHeader, headerSize);

    // Attempt to append the value after the TLV header. On failure
    // reinstate the original target buffer state.
    if (formatOk && (headerSize > 1)) {
        formatOk = gmosBufferConcatenate (targetBuffer, value, targetBuffer);
        if (!formatOk) {
            gmosBufferResize (targetBuffer,
                gmosBufferGetSize (targetBuffer) - headerSize);
        }
    }
out :
    return formatOk;
}

/*
 * Verifies and parses an NFC Type 5 capability container (CC) from the
 * start of the specified source buffer.
 */
uint16_t gmosDriverNfcTagType5ParseCC (gmosBuffer_t* sourceBuffer,
    uint32_t* memorySize, bool* writeEnable)
{
    uint8_t ccHeader [8];
    uint_fast8_t versionByte;
    uint32_t memLen;
    uint_fast16_t parsedLength;

    // Attempt to read back the first 4 bytes of the source buffer.
    if (!gmosBufferRead (sourceBuffer, 0, ccHeader, 4)) {
        parsedLength = 0;
        goto out;
    }

    // Extract the common capability container fields.
    versionByte = ccHeader [1];

    // Extract the size from the standard 4-byte capability container.
    if (ccHeader [0] == 0xE1) {
        memLen = ccHeader [2];
        parsedLength = 4;
    }

    // Extract the size from the extended 8-byte capability container.
    else if (ccHeader [0] == 0xE2) {
        if (!gmosBufferRead (sourceBuffer, 4, &(ccHeader [4]), 4)) {
            parsedLength = 0;
            goto out;
        }
        memLen = ccHeader [6];
        memLen = (memLen << 8) | ccHeader [7];
        parsedLength = 8;
    } else {
        parsedLength = 0;
        goto out;
    }

    // Check for a supported capability container version.
    if ((versionByte & 0xF0) != 0x40) {
        parsedLength = 0;
        goto out;
    }

    // Update the return values.
    if (memorySize != NULL) {
        *memorySize = memLen * 8;
    }
    if (writeEnable != NULL) {
        *writeEnable = ((versionByte & 0x03) == 0) ? true : false;
    }
out:
    return parsedLength;
}

/*
 * Verifies and parses an NFC Type 5 TLV record from a given offset in
 * the specified source buffer.
 */
uint16_t gmosDriverNfcTagType5ParseTLV (gmosBuffer_t* sourceBuffer,
    uint16_t sourceOffset, uint8_t* type, uint16_t* valueOffset,
    uint16_t* valueLength)
{
    uint8_t tlvHeader [4];
    uint_fast16_t readSize;
    uint_fast8_t recordType;
    uint_fast16_t dataOffset;
    uint_fast16_t dataLength;
    uint_fast16_t parsedLength;

    // Read as much data as possible, up to the end of the buffer.
    readSize = gmosBufferGetSize (sourceBuffer);
    if (readSize <= sourceOffset) {
        parsedLength = 0;
        goto out;
    }
    readSize -= sourceOffset;
    if (readSize > 4) {
        readSize = 4;
    }
    gmosBufferRead (sourceBuffer, sourceOffset, tlvHeader, readSize);
    recordType = tlvHeader [0];

    // Check for known single byte TLV records.
    if ((recordType == GMOS_DRIVER_NFC_TAG_TYPE5_NULL_TYPE) ||
        (recordType == GMOS_DRIVER_NFC_TAG_TYPE5_TERM_TYPE)) {
        dataOffset = 0;
        dataLength = 0;
        parsedLength = 1;
    }

    // All remaining record types are assumed to be full sized records,
    // so the length field is assumed to be valid.
    else if (readSize < 2) {
        parsedLength = 0;
        goto out;
    } else if (tlvHeader [1] < 0xFF) {
        dataOffset = sourceOffset + 2;
        dataLength = tlvHeader [1];
        parsedLength = dataLength + 2;
    } else if (readSize < 4) {
        parsedLength = 0;
        goto out;
    } else {
        dataOffset = sourceOffset + 4;
        dataLength = tlvHeader [2];
        dataLength = (dataLength << 8) | tlvHeader [3];
        parsedLength = dataLength + 4;
    }

    // Update the return values.
    if (type != NULL) {
        *type = recordType;
    }
    if (valueOffset != NULL) {
        *valueOffset = dataOffset;
    }
    if (valueLength != NULL) {
        *valueLength = dataLength;
    }
out:
    return parsedLength;
}
