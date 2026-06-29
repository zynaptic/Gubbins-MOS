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
 * This header provides various common type definitions and utility
 * functions for NFC Forum NDEF message encoding and parsing.
 */

#ifndef GMOS_DRIVER_NFC_TAG_NDEF_H
#define GMOS_DRIVER_NFC_TAG_NDEF_H

#include <stdint.h>
#include <stdbool.h>
#include "gmos-config.h"
#include "gmos-buffers.h"

/**
 * This enumeration specifies the supported NDEF type name format (TNF)
 * options.
 */
typedef enum {
    GMOS_DRIVER_NFC_TAG_NDEF_TNF_EMPTY           = 0x00,
    GMOS_DRIVER_NFC_TAG_NDEF_TNF_WELL_KNOWN_TYPE = 0x01,
    GMOS_DRIVER_NFC_TAG_NDEF_TNF_MEDIA_TYPE      = 0x02,
    GMOS_DRIVER_NFC_TAG_NDEF_TNF_ABSOLUTE_URI    = 0x03,
    GMOS_DRIVER_NFC_TAG_NDEF_TNF_EXTERNAL_TYPE   = 0x04,
    GMOS_DRIVER_NFC_TAG_NDEF_TNF_UNKNOWN         = 0x05,
    GMOS_DRIVER_NFC_TAG_NDEF_TNF_UNCHANGED       = 0x06,
    GMOS_DRIVER_NFC_TAG_NDEF_TNF_RESERVED        = 0x07,
    GMOS_DRIVER_NFC_TAG_NDEF_TNF_INVALID         = 0xFF
} gmosDriverNfcTagNdefTnf_t;

/**
 * This enumeration specifies the supported NDEF header flags.
 */
typedef enum {
    GMOS_DRIVER_NFC_TAG_NDEF_FLAG_ID_PRESENT   = 0x08,
    GMOS_DRIVER_NFC_TAG_NDEF_FLAG_SHORT_RECORD = 0x10,
    GMOS_DRIVER_NFC_TAG_NDEF_FLAG_CHUNKED      = 0x20,
    GMOS_DRIVER_NFC_TAG_NDEF_FLAG_MSG_END      = 0x40,
    GMOS_DRIVER_NFC_TAG_NDEF_FLAG_MSG_BEGIN    = 0x80
} gmosDriverNfcTagNdefFlag_t;

/**
 * This data structure is used to build NDEF records by setting the
 * various fields prior to construction.
 */
typedef struct gmosDriverNfcTagNdefBuilder_t {

    // Specifies a pointer to the payload data byte array.
    uint8_t* payloadData;

    // Specifies a pointer to the record data type byte array.
    uint8_t* typeData;

    // Specifies a pointer to the record ID byte array.
    uint8_t* idData;

    // Specifies the length of the payload data byte array.
    uint16_t payloadSize;

    // Specifies the length of the record data type byte array.
    uint8_t typeSize;

    // Specifies the length of the record ID byte array.
    uint8_t idSize;

    // Specifies the contents of the record header byte.
    uint8_t headerByte;

} gmosDriverNfcTagNdefBuilder_t;

/**
 * Defines the data structure used to implement NDEF message parsing.
 */
typedef struct gmosDriverNfcTagNdefParser_t {

    // Keep a pointer reference to the message source buffer.
    gmosBuffer_t* sourceBuffer;

    // Allocate buffer space for token storage.
    gmosBuffer_t tokenBuffer;

} gmosDriverNfcTagNdefParser_t;

/**
 * This data structure is used to store parsed NDEF record data.
 */
typedef struct gmosDriverNfcTagNdefToken_t {

    // Specifies the message buffer that contains the associated record data.
    gmosBuffer_t* sourceBuffer;

    // Specifies the offset to the start of the payload data in the
    // source buffer.
    uint16_t payloadOffset;

    // Specifies the offset to the start of the record type data in the
    // source buffer.
    uint16_t typeOffset;

    // Specifies the offset to the start of the record ID data in the
    // source buffer.
    uint16_t idOffset;

    // Specifies the length of the payload data field.
    uint16_t payloadSize;

    // Specifies the length of the record data type field.
    uint8_t typeSize;

    // Specifies the length of the record ID field.
    uint8_t idSize;

    // Specifies the contents of the record header byte.
    uint8_t headerByte;

} gmosDriverNfcTagNdefToken_t;

/**
 * Initialises an NDEF record builder structure ready for use.
 * @param builder This is the record builder structure which is to be
 *     initialised.
 * @param tnfOption This is the NDEF type name format to be used by the
 *     record.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully initialising the NDEF record builder and 'false'
 *     otherwise.
 */
bool gmosDriverNfcTagNdefBuilderInit (
    gmosDriverNfcTagNdefBuilder_t* builder,
    gmosDriverNfcTagNdefTnf_t tnfOption);

/**
 * Sets the NDEF message begin, message end and record chunk flags for
 * the specified record builder.
 * @param builder This is the record builder structure for which the
 *     NDEF record flags are being set.
 * @param flags These are the NDEF message flags that are to be used
 *     when building the new NDEF record.
* @return Returns a boolean value which will be set to 'true' on
 *     successfully setting the NDEF record flags and 'false' otherwise.
  */
bool gmosDriverNfcTagNdefBuilderSetFlags (
   gmosDriverNfcTagNdefBuilder_t* builder,
   gmosDriverNfcTagNdefFlag_t flags);

/**
 * Sets the NDEF record type for the specified record builder.
 * @param builder This is the record builder structure for which the
 *     NDEF record type is to be set.
 * @param typeData This is a pointer to the NDEF record type, stored as
 *     a byte array. It should not be set to a null value.
 * @param typeSize This is the length of the byte array which contains
 *     the NDEF record type. It should not be set to zero.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully setting the NDEF record type and 'false' otherwise.
 */
bool gmosDriverNfcTagNdefBuilderSetType (
   gmosDriverNfcTagNdefBuilder_t* builder,
   uint8_t* typeData, uint8_t typeSize);

/**
 * Sets the NDEF record ID for the specified record builder.
 * @param builder This is the record builder structure for which the
 *     NDEF record ID is to be set.
 * @param idData This is a pointer to the NDEF record ID, stored as a
 *     byte array. It should not be set to a null value.
 * @param idSize This is the length of the byte array which contains
 *     the NDEF record ID. It should not be set to zero.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully setting the NDEF record ID and 'false' otherwise.
 */
bool gmosDriverNfcTagNdefBuilderSetId (
    gmosDriverNfcTagNdefBuilder_t* builder,
    uint8_t* idData, uint8_t idSize);

/**
 * Sets the NDEF record payload for the specified record builder.
 * @param builder This is the record builder structure for which the
 *     NDEF record payload is to be set.
 * @param payloadData This is a pointer to the NDEF record payload,
 *     stored as a byte array. It should not be set to a null value.
 * @param payloadSize This is the length of the byte array which
 *     contains the NDEF record payload. It should not be set to zero.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully setting the NDEF record payload and 'false'
 *     otherwise.
 */
bool gmosDriverNfcTagNdefBuilderSetPayload (
    gmosDriverNfcTagNdefBuilder_t* builder,
    uint8_t* payloadData, uint16_t payloadSize);

/**
 * Encodes the contents of an NDEF record builder and appends them to
 * the end of the specified buffer. The message start flag for the
 * record will automatically be set when the specified buffer contains
 * no prior data.
 * @param builder This is the record builder structure which contains
 *     the NDEF record data that is to be encoded as an NDEF record and
 *     appended to the buffer.
 * @param buffer This is a pointer to the buffer which on successful
 *     completion will contain the newly encoded NDEF record.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully appending the NDEF record to the buffer and 'false'
 *     otherwise.
 */
bool gmosDriverNfcTagNdefBuilderEncode (
    gmosDriverNfcTagNdefBuilder_t* builder, gmosBuffer_t* buffer);

/**
 * Initialises an NDEF parser by scanning an NDEF message held in the
 * specified source buffer.
 * @param parser This is a pointer to the parser instance that is to
 *     be initialised using the contents of the supplied NDEF message
 *     buffer.
 * @param sourceBuffer This is a pointer to the buffer which contains
 *     the NDEF message that is to be scanned during initialisation. The
 *     contents should remain unchanged for the lifetime of the parser.
 * @param sourceOffset This is the offset in the source buffer of the
 *     start of the NDEF message.
 * @return Returns a boolean value which will be set to 'true' if the
 *     contents of the NDEF message buffer were correctly scanned and
 *     'false' otherwise.
 */
bool gmosDriverNfcTagNdefParserScan (
    gmosDriverNfcTagNdefParser_t* parser,
    gmosBuffer_t* sourceBuffer, uint16_t sourceOffset);

/**
 * Resets the state of the parser and releases any resources allocated
 * by an NDEF parser during processing.
 * @param parser This is a pointer to the parser instance that is to
 *     be reset, with all allocated resources being released.
 */
void gmosDriverNfcTagNdefParserReset (
    gmosDriverNfcTagNdefParser_t* parser);

/**
 * Reads back the NDEF token at the specified parser token offset.
 * @param parser This is a pointer to the parser instance that is to
 *     be accessed.
 * @param tokenIndex This is the token index position from which the
 *     NDEF parser token is to be read back.
 * @param token This is a pointer to a parser token data structure that
 *     will be populated with the token at the specified index position.
 * @return Returns the type name format for the parsed NDEF record, or
 *     the invalid option value on failure.
 */
uint8_t gmosDriverNfcTagNdefParserReadToken (
    gmosDriverNfcTagNdefParser_t* parser, uint16_t tokenIndex,
    gmosDriverNfcTagNdefToken_t* token);

/**
 * Gets the type data field associated with an NDEF record, updating the
 * specified type data array value.
 * @param token This is a pointer to the token instance which represents
 *     the parsed NDEF record.
 * @param typeData This is a pointer to a byte array which will be
 *     populated with the extracted NDEF record type information.
 * @param typeDataSize this is the size of the byte array which is used
 *     as the destination of the NDEF record type information. If the
 *     array is not large enough to hold the entire value it will be
 *     truncated to fit into the allocated size.
 * @return Returns the size of the type data value. This may be larger
 *     than the allocated type data size if the value has been
 *     truncated.
 */
uint8_t gmosDriverNfcTagNdefParserGetType (
    gmosDriverNfcTagNdefToken_t* token,
    uint8_t* typeData, uint8_t typeDataSize);

/**
 * Gets the record ID data field associated with an NDEF record,
 * updating the specified record ID data array value.
 * @param token This is a pointer to the token instance which represents
 *     the parsed NDEF record.
 * @param idData This is a pointer to a byte array which will be
 *     populated with the extracted NDEF record ID information.
 * @param idDataSize this is the size of the byte array which is used as
 *     the destination of the NDEF record ID information. If the array
 *     is not large enough to hold the entire value it will be truncated
 *     to fit into the allocated size.
 * @return Returns the size of the record ID data value. This may be
 *     larger than the allocated record ID data size if the value has
 *     been truncated.
 */
uint8_t gmosDriverNfcTagNdefParserGetId (
    gmosDriverNfcTagNdefToken_t* token,
    uint8_t* idData, uint8_t idDataSize);

/**
 * Gets the payload data field associated with an NDEF record, updating
 * the specified payload data array value.
 * @param token This is a pointer to the token instance which represents
 *     the parsed NDEF record.
 * @param payloadData This is a pointer to a byte array which will be
 *     populated with the extracted NDEF payload data.
 * @param payloadDataSize this is the size of the byte array which is
 *     used as the destination of the NDEF payload data. If the array is
 *     not large enough to hold the entire value it will be truncated to
 *     fit into the allocated size.
 * @return Returns the size of the payload data value. This may be
 *     larger than the allocated payload data size if the value has been
 *     truncated.
 */
uint16_t gmosDriverNfcTagNdefParserGetPayload (
    gmosDriverNfcTagNdefToken_t* token,
    uint8_t* payloadData, uint16_t payloadDataSize);

#endif // GMOS_DRIVER_NFC_TAG_NDEF_H
