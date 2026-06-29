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
 * This header defines the common API for processing NFC Type 5 data
 * formats.
 */

#ifndef GMOS_DRIVER_NFC_TAG_TYPE5_H
#define GMOS_DRIVER_NFC_TAG_TYPE5_H

/**
 * This enumeration specifies the supported type values for inclusion in
 * type length and value (TLV) records for NFC Type 5 devices.
 */
typedef enum {
    GMOS_DRIVER_NFC_TAG_TYPE5_NULL_TYPE = 0x00,
    GMOS_DRIVER_NFC_TAG_TYPE5_NDEF_TYPE = 0x03,
    GMOS_DRIVER_NFC_TAG_TYPE5_PROP_TYPE = 0xFD,
    GMOS_DRIVER_NFC_TAG_TYPE5_TERM_TYPE = 0xFE
} gmosDriverNfcTagType5Types_t;

/**
 * Appends an NFC Type 5 capability container (CC) to the specified
 * target buffer.
 * @param targetBuffer This is a pointer to the buffer which on
 *     successful completion will contain the NFC Type 5 capability
 *     container.
 * @param memorySize This specifies the size of the memory area
 *     allocated for NFC Type 5 data storage, specified as the total
 *     number of allocated bytes. It must be an integer multiple of 8.
 * @param writeEnable This specifies whether the memory area allocated
 *     for NFC Type 5 data storage is writeable via the NFC interface.
 * @param forceExtendedCC This is a boolean value, which if set to
 *     'true' will force the use of the extended 8-byte format, even for
 *     smaller memory areas.
 * @return Returns a boolean value which will be set to 'true' if the
 *     capability container was successfully formatted and appended to
 *     the target buffer and 'false' otherwise.
 */
bool gmosDriverNfcTagType5FormatCC (gmosBuffer_t* targetBuffer,
    uint32_t memorySize, bool writeEnable, bool forceExtendedCC);

/**
 * Appends an NFC Type 5 type, length and value (TLV) record to the
 * specified target buffer.
 * @param targetBuffer This is a pointer to the buffer which on
 *     successful completion will contain the NFC Type 5 TLV record.
 * @param type This is the type byte which is used to identify the
 *     TLV record type.
 * @param value This is a buffer which contains the value that is to be
 *     included in the TLV record. A null reference may be passed in
 *     order to append tag only records. On successful completion the
 *     contents of the value buffer will automatically be reset.
 * @return Returns a boolean value which will be set to 'true' if the
 *     TLV record was successfully formatted and appended to the target
 *     buffer and 'false' otherwise.
 */
bool gmosDriverNfcTagType5FormatTLV (gmosBuffer_t* targetBuffer,
    uint8_t type, gmosBuffer_t* value);

/**
 * Verifies and parses an NFC Type 5 capability container (CC) from the
 * start of the specified source buffer.
 * @param sourceBuffer This is a pointer to the buffer which contains
 *     the NFC Type 5 capability container.
 * @param memorySize This is a pointer to an integer value, which on
 *     successful completion will be updated with the size of the memory
 *     area allocated for NFC Type 5 data storage, specified as the
 *     total number of allocated bytes.
 * @param writeEnable This is a pointer to a boolean value, which on
 *     successful completion will be updated to indicate whether the
 *     memory area allocated for NFC Type 5 data storage is writeable
 *     via the NFC interface.
 * @return Returns the length of the parsed capability container, or
 *     zero if the capability container could not be parsed.
 */
uint16_t gmosDriverNfcTagType5ParseCC (gmosBuffer_t* sourceBuffer,
    uint32_t* memorySize, bool* writeEnable);

/**
 * Verifies and parses an NFC Type 5 TLV record from a given offset in
 * the specified source buffer.
 * @param sourceBuffer This is a pointer to the buffer which contains
 *     the NFC Type 5 TLV record.
 * @param sourceOffset This is the offset within the source buffer at
 *     which the TLV record is located.
 * @param type This is a pointer to the a byte value, which on
 *     successful completion will be updated with the TLV record type.
 * @param valueOffset This is a pointer to a 16-bit unsigned integer,
 *     which on successful completion will be updated with the offset of
 *     the start of the TLV value field in the source buffer.
 * @param valueLength This is a pointer to a 16-bit unsigned integer,
 *     which on successful completion will be updated with the length of
 *     the TLV value field in the source buffer.
 * @return Returns the length of the parsed TLV record, or zero if the
 *     TLV record could not be parsed.
 */
uint16_t gmosDriverNfcTagType5ParseTLV (gmosBuffer_t* sourceBuffer,
    uint16_t sourceOffset, uint8_t* type, uint16_t* valueOffset,
    uint16_t* valueLength);

#endif // GMOS_DRIVER_NFC_TAG_TYPE5_H
