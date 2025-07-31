/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2025 Zynaptic Limited
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
 * This file implements the GubbinsMOS hash map data structure for
 * devices that support conventional heap based memory management.
 */

// The contents of this file are only compiled if heap based memory
// management is enabled.
#include "gmos-config.h"
#if (GMOS_CONFIG_HEAP_SIZE > 0)

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-platform.h"
#include "gmos-scheduler.h"
#include "gmos-hashmap.h"

/**
 * Specify a configuration option for the minimum size of a hash map
 * subarray. This must be no less that 8, but the default setting of 16
 * is a sensible option for most applications.
 */
#ifndef GMOS_CONFIG_HASHMAP_HEAP_SUBARRAY_MIN_SIZE_LOG2
#define GMOS_CONFIG_HASHMAP_HEAP_SUBARRAY_MIN_SIZE_LOG2 4
#endif

/**
 * Specify a configuration option for the maximum size of a hash map
 * subarray. The default setting of 256 is a sensible option for most
 * applications.
 */
#ifndef GMOS_CONFIG_HASHMAP_HEAP_SUBARRAY_MAX_SIZE_LOG2
#define GMOS_CONFIG_HASHMAP_HEAP_SUBARRAY_MAX_SIZE_LOG2 8
#endif

/**
 * Specify the scaling factor to use when calculating subarray sizes.
 * Requested subarray sizes are a multiple of the current number of
 * table entries and will fall in the range SF/16 to SF/8. So a value of
 * 4 would correspond to the range of 0.25 to 0.5 times the current
 * number of table entries. Lower values will result in finer grained
 * memory allocation, which improves the memory efficiency of the table
 * at the expense of increased average probe times.
 */
#ifndef GMOS_CONFIG_HASHMAP_HEAP_SUBARRAY_SIZE_SCALING
#define GMOS_CONFIG_HASHMAP_HEAP_SUBARRAY_SIZE_SCALING 7
#endif

/**
 * Specify a configuration option for the hash map standard subarrray
 * linear probe depth. The default setting of 2 seems to be a sensible
 * tradeoff between search time and memory requirements for most
 * applications.
 */
#ifndef GMOS_CONFIG_HASHMAP_HEAP_PROBE_DEPTH
#define GMOS_CONFIG_HASHMAP_HEAP_PROBE_DEPTH 2
#endif

/**
 * Specify a configuration option for the hash map final subarrray
 * linear probe depth. This is an extended probe depth to deal with
 * hash values that have significant contention.
 */
#ifndef GMOS_CONFIG_HASHMAP_HEAP_FINAL_PROBE_DEPTH
#define GMOS_CONFIG_HASHMAP_HEAP_FINAL_PROBE_DEPTH 8
#endif

/**
 * Enable additional hash table debug tracing.
 */
#ifndef GMOS_CONFIG_HASHMAP_HEAP_DEBUG_TRACE
#define GMOS_CONFIG_HASHMAP_HEAP_DEBUG_TRACE false
#endif

/*
 * Use the standard 'memcpy' and 'memcmp' functions for bytewise data
 * transfer and comparison.
 */
#if GMOS_CONFIG_BUFFERS_USE_MEMCPY
#include <string.h>
static inline void byteCopy (uint8_t* dst, uint8_t* src, size_t size)
{
    memcpy (dst, src, size);
}
static inline bool byteMatch (uint8_t* dst, uint8_t* src, size_t size)
{
    return ((memcmp (dst, src, size) == 0) ? true : false);
}

/*
 * Use inlined bytewise copy and comparison functions for buffer data
 * transfer and comparison.
 */
#else
static inline void byteCopy (uint8_t* dst, uint8_t* src, size_t size)
{
    size_t count;
    for (count = size; count != 0; count--) {
        *(dst++) = *(src++);
    }
}
static inline bool byteMatch (uint8_t* dst, uint8_t* src, size_t size)
{
    size_t count;
    bool match = true;
    for (count = size; count != 0; count--) {
        if (*(dst++) != *(src++)) {
            match = false;
            break;
        }
    }
    return match;
}
#endif

/*
 * Define the state values and flags for the background processing state
 * machine.
 */
typedef enum {
    GMOS_HASHMAP_BACKGROUND_STATE_RELOCATE     = 0x0000,
    GMOS_HASHMAP_BACKGROUND_STATE_IDLE         = 0x1000,
    GMOS_HASHMAP_BACKGROUND_STATE_DEALLOCATE   = 0x2000,
    GMOS_HASHMAP_BACKGROUND_STATE_LOAD_FACTOR  = 0x3000,
    GMOS_HASHMAP_BACKGROUND_STATE_TRIGGER_FLAG = 0x8000
} GmosHashMapBackgroundState_t;

/*
 * Defines the structure of a single subarray for the hash map.
 */
typedef struct gmosHashMapSubarray_t {

    // This is a pointer to the next subarray in the list.
    struct gmosHashMapSubarray_t* nextSubarray;

    // This is the mask used to convert the hash value to an index.
    uint16_t hashMask;

    // This is the shift used to convert the hash value to an index.
    uint8_t hashShift;

    // Specify the first byte of the occupancy flags array.
    uint8_t occupancyFlags;

} gmosHashMapSubarray_t;

/*
 * Implement the default hashing function using a simple algorithm that
 * should be good for most applications.
 */
static uint16_t gmosHashMapDefaultHashingFunction (
    uint8_t* keyData, uint8_t keySize)
{
    uint_fast16_t hashValue;
    uint_fast8_t i;
    uint_fast8_t initByte;
    uint8_t* keyDataPtr = keyData;

    // The hash value is initialised by replicating the first byte.
    initByte = *(keyDataPtr++);
    hashValue = initByte;
    hashValue = (hashValue << 8) | initByte;

    // Subsequent bytes are incorporated by left rotating the existing
    // value by 7 bits and XORing the new byte into the lower 8 bits.
    for (i = keySize - 1; i > 0; i--) {
        hashValue = (hashValue << 7) | (hashValue >> 9);
        hashValue ^= (uint_fast16_t) *(keyDataPtr++);
    }
    return hashValue;
}

/*
 * Select either the default or custom hashing function.
 */
static inline uint_fast16_t gmosHashMapHashingFunction (
    gmosHashMap_t* hashMap, uint8_t* keyData)
{
    gmosHashMapHashingFunction_t hashingFunction;
    if (hashMap->hashingFunction != NULL) {
        hashingFunction = hashMap->hashingFunction;
    } else {
        hashingFunction = gmosHashMapDefaultHashingFunction;
    }
    return hashingFunction (keyData, hashMap->keySize);
}

/*
 * For debugging purposes print out subarray information.
 */
#if GMOS_CONFIG_HASHMAP_HEAP_DEBUG_TRACE
static void gmosHashMapPrintSubarrayStats (gmosHashMap_t* hashMap)
{
    gmosHashMapSubarray_t* subarray;
    uint32_t entryCount;
    uint32_t capacity;
    uint8_t* occupancyFlagPtr;
    uint_fast8_t occupancyFlags;
    uint_fast8_t i, j;

    // Loop over each subarray evaluating the load factor.
    GMOS_LOG (LOG_DEBUG, "HashMap: Current subarray information:");
    subarray = hashMap->subarrayList;
    while (subarray != NULL) {
        entryCount = 0;
        capacity = 1UL + subarray->hashMask;
        occupancyFlagPtr = &(subarray->occupancyFlags);
        for (i = 0; i < capacity / 8; i++) {
            occupancyFlags = occupancyFlagPtr [i];
            for (j = 0; j < 8; j++) {
                if ((occupancyFlags & 0x01) != 0) {
                    entryCount += 1;
                }
                occupancyFlags >>= 1;
            }
        }
        GMOS_LOG_FMT (LOG_DEBUG,
            "    0x%08X has mask 0x%04X, shift %2d, load factor %d/%d (%d%%).",
            (uintptr_t) subarray, subarray->hashMask, subarray->hashShift,
            entryCount, capacity, (100UL * entryCount) / capacity);
        subarray = subarray->nextSubarray;
    }
}
#else
static inline void gmosHashMapPrintSubarrayStats (gmosHashMap_t* hashMap)
{
    (void) hashMap;
}
#endif

/*
 * Allocate and initialise a new subarray.
 */
static gmosHashMapSubarray_t* gmosHashMapAllocSubarray (uint8_t keySize,
    uint8_t valueSize, uint8_t sizeLog2, uint8_t baseShift)
{
    uint32_t allocSize;
    uint_fast16_t i;
    uint8_t* occupancyFlagPtr;
    gmosHashMapSubarray_t* subarray = NULL;

    // Do some initial sanity checking on the requested size. It must be
    // between the configured minimum and maximum size.
    if (sizeLog2 > GMOS_CONFIG_HASHMAP_HEAP_SUBARRAY_MAX_SIZE_LOG2) {
        sizeLog2 = GMOS_CONFIG_HASHMAP_HEAP_SUBARRAY_MAX_SIZE_LOG2;
    }
    if (sizeLog2 < GMOS_CONFIG_HASHMAP_HEAP_SUBARRAY_MIN_SIZE_LOG2) {
        sizeLog2 = GMOS_CONFIG_HASHMAP_HEAP_SUBARRAY_MIN_SIZE_LOG2;
    }

    // Attempt to allocate the maximum size subarray, but reduce the
    // size if there is insufficient contiguous memory. A single
    // subarray should also be no larger than 1/16 of the heap.
    while (true) {

        // The allocation size is the size of the header plus N/8 bytes
        // for occupany flags plus 'N' times the key and value size.
        allocSize = sizeof (gmosHashMapSubarray_t) - 1;
        allocSize += 1UL << (sizeLog2 - 3);
        allocSize +=
            (((uint32_t) keySize) + ((uint32_t) valueSize)) << sizeLog2;

        // Attempt the allocation.
        if (allocSize <= (GMOS_CONFIG_HEAP_SIZE / 16)) {
            subarray = GMOS_MALLOC (allocSize);
            if (subarray != NULL) {
                break;
            }
        }

        // Reduce the allocation size on failure. Give up if there is
        // not enough space for an 8 entry subarray.
        sizeLog2 -= 1;
        if (sizeLog2 < 3) {
            break;
        }
    }

    // Initialise the newly allocated subarray, including clearing all
    // the occupancy flags.
    if (subarray != NULL) {
        occupancyFlagPtr = &(subarray->occupancyFlags);
        for (i = 1UL << (sizeLog2 - 3); i > 0; i--) {
            *(occupancyFlagPtr++) = 0;
        }
        subarray->nextSubarray = NULL;
        subarray->hashMask = (1UL << sizeLog2) -  1;
        subarray->hashShift = (baseShift + 3) & 0x0F;
        if (GMOS_CONFIG_HASHMAP_HEAP_DEBUG_TRACE) {
            GMOS_LOG_FMT (LOG_DEBUG,
                "HashMap : Allocated subarray 0x%08X (mask 0x%04X, shift %d).",
                (uintptr_t) subarray, subarray->hashMask, subarray->hashShift);
        }
    }
    return subarray;
}

/*
 * Calculates the index into a given subarray for a given hash value.
 */
static inline uint_fast16_t gmosHashMapSubarrayIndex (
    gmosHashMapSubarray_t* subarray, uint_fast16_t hashValue)
{
    hashValue = (hashValue << subarray->hashShift) |
        (hashValue >> (16 - subarray->hashShift));
    return hashValue & subarray->hashMask;
}

/*
 * Gets the address of a subarray entry with the specified index.
 */
static uint8_t* gmosHashMapSubarrayEntryAddress (
    gmosHashMap_t* hashMap, gmosHashMapSubarray_t* subarray,
    uint_fast16_t index)
{
    uint_fast8_t occupancyFlagsSize = (1UL + subarray->hashMask) / 8;
    return &(subarray->occupancyFlags) + occupancyFlagsSize + index *
        ((uintptr_t) hashMap->keySize + (uintptr_t) hashMap->valueSize);
}

/*
 * Check whether the subarray location corresponding to a given index
 * is empty.
 */
static bool gmosHashMapSubarrayIsEmpty (
    gmosHashMapSubarray_t* subarray, uint_fast16_t index)
{
    uint8_t* occupancyArray = &(subarray->occupancyFlags);
    uint8_t occupancyByte;

    // Check the occupancy flags for the specified subarray index.
    occupancyByte = *(occupancyArray + (index / 8));
    occupancyByte &= (1 << (index & 7));
    return (occupancyByte == 0) ? true : false;
}

/*
 * Check whether the subarray location corresponding to a given index
 * has a matching key.
 */
static bool gmosHashMapSubarrayMatchesKey (gmosHashMap_t* hashMap,
    gmosHashMapSubarray_t* subarray, uint_fast16_t index, uint8_t* key)
{
    uint8_t* storedKey = gmosHashMapSubarrayEntryAddress (
        hashMap, subarray, index);
    return byteMatch (key, storedKey, hashMap->keySize);
}

/*
 * Search for a subarray that contains a specified key.
 */
static gmosHashMapSubarray_t* gmosHashMapSubarraySearch (
    gmosHashMap_t* hashMap, uint8_t* key, uint_fast16_t* matchedIndex,
    gmosHashMapSubarray_t** emptySubarray, uint_fast16_t* emptyIndex)
{
    gmosHashMapSubarray_t* searchSubarray;
    gmosHashMapSubarray_t* matchedSubarray;
    uint_fast16_t hashValue;
    uint_fast16_t searchIndex;
    uint_fast8_t i;
    uint_fast8_t probeDepth;
    bool findEmptySubarray;

    // Get the hash value.
    hashValue = gmosHashMapHashingFunction (hashMap, key);

    // Determine if the search should include empty subarrays.
    if ((emptySubarray != NULL) && (emptyIndex != NULL)) {
        *emptySubarray = NULL;
        findEmptySubarray = true;
    } else {
        findEmptySubarray = false;
    }

    // Search over all the subarrays, looking for a matching entry using
    // a linear probe sequence. The probe depth is only set to extended
    // probing for the final subarray in the list.
    searchSubarray = hashMap->subarrayList;
    matchedSubarray = NULL;
    probeDepth = GMOS_CONFIG_HASHMAP_HEAP_PROBE_DEPTH;
    while ((matchedSubarray == NULL) && (searchSubarray != NULL)) {
        if (searchSubarray->nextSubarray == NULL) {
            probeDepth = GMOS_CONFIG_HASHMAP_HEAP_FINAL_PROBE_DEPTH;
        }
        searchIndex = gmosHashMapSubarrayIndex (searchSubarray, hashValue);
        for (i = probeDepth; i > 0; i--) {

            // Include the first empty subarray if required.
            if (gmosHashMapSubarrayIsEmpty (searchSubarray, searchIndex)) {
                if (GMOS_CONFIG_HASHMAP_HEAP_DEBUG_TRACE) {
                    GMOS_LOG_FMT (LOG_VERBOSE,
                        "HashMap : Probe entry %08X:%03d (E)",
                        (uintptr_t) searchSubarray, searchIndex);
                }
                if (findEmptySubarray) {
                    *emptySubarray = searchSubarray;
                    *emptyIndex = searchIndex;
                    findEmptySubarray = false;
                }
            }

            // Check for a matching entry.
            else if (gmosHashMapSubarrayMatchesKey (hashMap,
                searchSubarray, searchIndex, key)) {
                if (GMOS_CONFIG_HASHMAP_HEAP_DEBUG_TRACE) {
                    GMOS_LOG_FMT (LOG_VERBOSE,
                        "HashMap : Probe entry %08X:%03d (M)",
                        (uintptr_t) searchSubarray, searchIndex);
                }
                matchedSubarray = searchSubarray;
                *matchedIndex = searchIndex;
                break;
            }

            // Table contains a different entry.
            else if (GMOS_CONFIG_HASHMAP_HEAP_DEBUG_TRACE) {
                GMOS_LOG_FMT (LOG_VERBOSE,
                    "HashMap : Probe entry %08X:%03d (X)",
                    (uintptr_t) searchSubarray, searchIndex);
            }
            searchIndex = (searchIndex + 1) & searchSubarray->hashMask;
        }
        searchSubarray = searchSubarray->nextSubarray;
    }
    return matchedSubarray;
}

/*
 * Sets the occupancy flag for a given subarray entry to the specified
 * value.
 */
static void gmosHashMapSubarraySetOccupancy (
    gmosHashMapSubarray_t* subarray, uint_fast16_t index, bool setFlag)
{
    uint8_t* occupancyArray = &(subarray->occupancyFlags);
    uint8_t* occupancyBytePtr = occupancyArray + (index / 8);
    uint8_t occupancyFlag = 1 << (index & 7);
    if (setFlag) {
        *occupancyBytePtr |= occupancyFlag;
    } else {
        *occupancyBytePtr &= ~occupancyFlag;
    }
}

/*
 * Writes the key and value to a given subarray entry.
 */
static void gmosHashMapSubarraySetKeyValue (gmosHashMap_t* hashMap,
    gmosHashMapSubarray_t* subarray, uint_fast16_t index,
    uint8_t* key, uint8_t* value)
{
    uint8_t* storedEntry = gmosHashMapSubarrayEntryAddress (
        hashMap, subarray, index);
    byteCopy (storedEntry, key, hashMap->keySize);
    byteCopy (storedEntry + hashMap->keySize, value, hashMap->valueSize);
}

/*
 * Attempt to relocate an entry in the last subarray.
 */
static inline bool gmosHashMapRelocateEntry (gmosHashMap_t* hashMap)
{
    gmosHashMapSubarray_t* sourceSubarray;
    gmosHashMapSubarray_t* searchSubarray;
    gmosHashMapSubarray_t* targetSubarray;
    uint8_t* sourcePtr;
    uint8_t* targetPtr;
    uint_fast16_t hashValue;
    uint_fast16_t sourceIndex;
    uint_fast16_t searchIndex;
    uint_fast16_t targetIndex;
    uint_fast8_t i;
    uint_fast8_t probeDepth;
    bool relocationValid;

    // Get the last subarray in the list. No relocation can be carried
    // out if there is only one subarray in the list.
    sourceSubarray = hashMap->subarrayList;
    relocationValid = false;
    while (sourceSubarray->nextSubarray != NULL) {
        sourceSubarray = sourceSubarray->nextSubarray;
        relocationValid = true;
    }
    if (!relocationValid) {
        goto out;
    }

    // Check for a valid entry count. This is encoded in the background
    // processing state variable.
    sourceIndex = hashMap->backgroundState &
        ~GMOS_HASHMAP_BACKGROUND_STATE_TRIGGER_FLAG;
    if (sourceIndex > sourceSubarray->hashMask) {
        relocationValid = false;
        goto out;
    }

    // Check for an active entry in the subarray.
    if (gmosHashMapSubarrayIsEmpty (sourceSubarray, sourceIndex)) {
        goto out;
    }

    // Get the hash value for the active entry.
    sourcePtr = gmosHashMapSubarrayEntryAddress (
        hashMap, sourceSubarray, sourceIndex);
    hashValue = gmosHashMapHashingFunction (hashMap, sourcePtr);

    // Search for a subarray that has an empty location.
    searchSubarray = hashMap->subarrayList;
    targetSubarray = NULL;
    probeDepth = GMOS_CONFIG_HASHMAP_HEAP_PROBE_DEPTH;
    while ((targetSubarray == NULL) && (searchSubarray != sourceSubarray)) {
        searchIndex = gmosHashMapSubarrayIndex (searchSubarray, hashValue);
        for (i = probeDepth; i > 0; i--) {
            if (gmosHashMapSubarrayIsEmpty (searchSubarray, searchIndex)) {
                targetSubarray = searchSubarray;
                targetIndex = searchIndex;
                break;
            }
            searchIndex = (searchIndex + 1) & searchSubarray->hashMask;
        }
        searchSubarray = searchSubarray->nextSubarray;
    }
    if (targetSubarray == NULL) {
        goto out;
    }

    // Copy the key and value to the empty location.
    targetPtr = gmosHashMapSubarrayEntryAddress (
        hashMap, targetSubarray, targetIndex);
    byteCopy (targetPtr, sourcePtr,
        hashMap->keySize + hashMap->valueSize);

    // Set and clear the appropriate occupancy flags to move the entry.
    gmosHashMapSubarraySetOccupancy (targetSubarray, targetIndex, true);
    gmosHashMapSubarraySetOccupancy (sourceSubarray, sourceIndex, false);
    if (GMOS_CONFIG_HASHMAP_HEAP_DEBUG_TRACE) {
        GMOS_LOG_FMT (LOG_VERBOSE,
            "HashMap : Relocated entry %08X:%03d to %08X:%03d.",
            (uintptr_t) sourceSubarray, sourceIndex,
            (uintptr_t) targetSubarray, targetIndex);
    }

out:
    return relocationValid;
}

/*
 * Attempt to deallocate the last subarray in the list.
 */
static inline bool gmosHashMapDeallocateSubarray (gmosHashMap_t* hashMap)
{
    gmosHashMapSubarray_t* lastSubarray;
    gmosHashMapSubarray_t* priorSubarray;
    uint_fast16_t i;
    uint8_t* occupancyFlagPtr;
    bool deallocationValid;

    // Get the last subarray in the list. No relocation can be carried
    // out if there is only one subarray in the list.
    priorSubarray = NULL;
    lastSubarray = hashMap->subarrayList;
    deallocationValid = false;
    while (lastSubarray->nextSubarray != NULL) {
        priorSubarray = lastSubarray;
        lastSubarray = lastSubarray->nextSubarray;
        deallocationValid = true;
    }
    if (!deallocationValid) {
        goto out;
    }

    // Check to see if all the entries in the subarray are empty.
    occupancyFlagPtr = &(lastSubarray->occupancyFlags);
    for (i = (1UL + lastSubarray->hashMask) >> 3; i > 0; i--) {
        if (*(occupancyFlagPtr++) != 0) {
            deallocationValid = false;
            goto out;
        }
    }

    // Deallocate the last subarray in the list.
    if (GMOS_CONFIG_HASHMAP_HEAP_DEBUG_TRACE) {
        GMOS_LOG_FMT (LOG_DEBUG,
            "HashMap : Removed subarray 0x%08X (mask 0x%04X, shift %d).",
            (uintptr_t) lastSubarray, lastSubarray->hashMask,
            lastSubarray->hashShift);
    }
    priorSubarray->nextSubarray = NULL;
    GMOS_FREE (lastSubarray);

out:
    return deallocationValid;
}

/*
 * Perform load factor adjustment. New reduced sized subarrays will be
 * added for maps with a low load factor so that existing sparsely
 * populated subarrays can be deallocated.
 */
static inline bool gmosHashMapLoadFactorAdjust (gmosHashMap_t* hashMap)
{
    gmosHashMapSubarray_t* emptySubarray;
    uint32_t capacity;
    uint32_t entryCount;
    uint_fast16_t hashMaskMin;
    uint_fast16_t targetSize;
    uint_fast8_t sizeLog2;
    bool adjustmentValid;

    // Load factor adjustment occurs if the load factor drops to 1/4.
    entryCount = gmosHashMapLoadFactor (hashMap, &capacity);
    if (entryCount > capacity / 4) {
        if (GMOS_CONFIG_HASHMAP_HEAP_DEBUG_TRACE) {
            GMOS_LOG (LOG_DEBUG,
                "HashMap : Load factor adjustment not required.");
        }
        adjustmentValid = false;
        goto out;
    }

    // Load factor adjustment is skipped for minimum size hash maps.
    hashMaskMin =
        (1UL << GMOS_CONFIG_HASHMAP_HEAP_SUBARRAY_MIN_SIZE_LOG2) - 1;
    if ((hashMap->subarrayList->nextSubarray == NULL) &&
        (hashMap->subarrayList->hashMask == hashMaskMin)) {
        if (GMOS_CONFIG_HASHMAP_HEAP_DEBUG_TRACE) {
            GMOS_LOG (LOG_DEBUG,
                "HashMap : Load factor adjustment at minimum size.");
        }
        adjustmentValid = false;
        goto out;
    }

    // Attempt to allocate a new subarray. The target size for the new
    // subarray is determined by the specified scaling factor.
    targetSize = ((uint32_t) hashMap->entryCount *
        GMOS_CONFIG_HASHMAP_HEAP_SUBARRAY_SIZE_SCALING) / 16;
    for (sizeLog2 = 3; sizeLog2 <=
        GMOS_CONFIG_HASHMAP_HEAP_SUBARRAY_MAX_SIZE_LOG2; sizeLog2++) {
        if ((1UL << sizeLog2) >= targetSize) {
            break;
        }
    }
    emptySubarray = gmosHashMapAllocSubarray (
        hashMap->keySize, hashMap->valueSize,
        sizeLog2, hashMap->subarrayList->hashShift);
    if (emptySubarray == NULL) {
        if (GMOS_CONFIG_HASHMAP_HEAP_DEBUG_TRACE) {
            GMOS_LOG (LOG_DEBUG,
                "HashMap : Load factor adjustment alloc failed.");
        }
        adjustmentValid = false;
        goto out;
    }

    // Add the new subarray to the start of the subarray list to allow
    // relocation of existing entries.
    if (GMOS_CONFIG_HASHMAP_HEAP_DEBUG_TRACE) {
        GMOS_LOG (LOG_DEBUG,
            "HashMap : Load factor adjustment in progress.");
    }
    emptySubarray->nextSubarray = hashMap->subarrayList;
    hashMap->subarrayList = emptySubarray;
    adjustmentValid = true;

out:
    return adjustmentValid;
}

/*
 * Implement the garbage collection background task.
 */
static gmosTaskStatus_t gmosHashMapBackgroundTaskFn (
    gmosHashMap_t* hashMap)
{
    uint_fast16_t nextState;
    uint_fast16_t nextTriggerFlag;
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_BACKGROUND;

    // Extract the next state and the next trigger flag values.
    nextState = hashMap->backgroundState &
        ~GMOS_HASHMAP_BACKGROUND_STATE_TRIGGER_FLAG;
    nextTriggerFlag = hashMap->backgroundState &
        GMOS_HASHMAP_BACKGROUND_STATE_TRIGGER_FLAG;

    // Implement background processing state machine.
    switch (nextState) {

        // From the idle condition, resume processing if the background
        // task has been triggered.
        case GMOS_HASHMAP_BACKGROUND_STATE_IDLE :
            if (nextTriggerFlag != 0) {
                if (GMOS_CONFIG_HASHMAP_HEAP_DEBUG_TRACE) {
                    GMOS_LOG (LOG_DEBUG,
                        "HashMap : Triggered background task.");
                }
                nextState = GMOS_HASHMAP_BACKGROUND_STATE_RELOCATE;
                nextTriggerFlag = 0;
            } else {
                taskStatus = GMOS_TASK_SUSPEND;
            }
            break;

        // Attempt to deallocate the last subarray in the list.
        case GMOS_HASHMAP_BACKGROUND_STATE_DEALLOCATE :
            if (gmosHashMapDeallocateSubarray (hashMap)) {
                nextState = GMOS_HASHMAP_BACKGROUND_STATE_RELOCATE;
            } else {
                nextState = GMOS_HASHMAP_BACKGROUND_STATE_LOAD_FACTOR;
            }
            break;

        // Perform load factor adjustment if required.
        case GMOS_HASHMAP_BACKGROUND_STATE_LOAD_FACTOR :
            if (gmosHashMapLoadFactorAdjust (hashMap)) {
                nextState = GMOS_HASHMAP_BACKGROUND_STATE_RELOCATE;
            } else {
                nextState = GMOS_HASHMAP_BACKGROUND_STATE_IDLE;
                gmosHashMapPrintSubarrayStats (hashMap);
            }
            break;

        // Remaining states use the state variable as an index counter.
        default :
            if (gmosHashMapRelocateEntry (hashMap)) {
                nextState += 1;
            } else {
                nextState = GMOS_HASHMAP_BACKGROUND_STATE_DEALLOCATE;
            }
            break;
    }

    // Update the background state, preserving the trigger flag state.
    hashMap->backgroundState = nextState | nextTriggerFlag;
    if ((GMOS_CONFIG_HASHMAP_HEAP_DEBUG_TRACE) && (nextState > 0x0FFF)) {
        GMOS_LOG_FMT (LOG_DEBUG,
            "HashMap : Background task state = 0x%04X.",
            hashMap->backgroundState);
    }
    return taskStatus;
}
GMOS_TASK_DEFINITION (gmosHashMapBackgroundTask,
    gmosHashMapBackgroundTaskFn, gmosHashMap_t);

/*
 * Performs a one-time initialisation of a GubbinsMOS hash map. This
 * should be called during initialisation to set up the hash map for
 * subsequent use.
 */
bool gmosHashMapInit (gmosHashMap_t* hashMap, uint8_t keySize,
    uint8_t valueSize, gmosHashMapHashingFunction_t hashingFunction)
{
    bool initOk = true;
    gmosHashMapSubarray_t* subarray;

    // Attempt to allocate the first hash map subarray.
    subarray = gmosHashMapAllocSubarray (keySize, valueSize, 0, 0);
    if (subarray == NULL) {
        initOk = false;
        goto out;
    }

    // Initialise the main data structure.
    hashMap->subarrayList = subarray;
    hashMap->keySize = keySize;
    hashMap->valueSize = valueSize;
    hashMap->entryCount = 0;
    hashMap->hashingFunction = hashingFunction;

    // Run the background task.
    hashMap->backgroundState = GMOS_HASHMAP_BACKGROUND_STATE_IDLE;
    gmosHashMapBackgroundTask_start (&(hashMap->backgroundTask),
        hashMap, "Hash Map Background Task");

    // Clean up on exit.
out :
    if ((!initOk) && (subarray != NULL)) {
        GMOS_FREE (subarray);
    }
    return initOk;
}

/*
 * Puts a new entry into the hash map. This will either create a new
 * entry with the specified key and value or update an existing entry
 * with the same key.
 */
bool gmosHashMapPut (gmosHashMap_t* hashMap, uint8_t* key, uint8_t* value)
{
    gmosHashMapSubarray_t* matchedSubarray;
    gmosHashMapSubarray_t* emptySubarray;
    uint_fast16_t matchedIndex;
    uint_fast16_t emptyIndex;
    bool triggerBackgroundTask = false;
    bool putOk;

    // Search for a matching subarray or an empty alternative.
restart:
    matchedSubarray = gmosHashMapSubarraySearch (
        hashMap, key, &matchedIndex, &emptySubarray, &emptyIndex);

    // Overwrite an existing entry if found.
    if (matchedSubarray != NULL) {
        gmosHashMapSubarraySetKeyValue (hashMap,
            matchedSubarray, matchedIndex, key, value);
        putOk = true;
    }

    // Populate an empty subarray location if available.
    else if (emptySubarray != NULL) {
        gmosHashMapSubarraySetKeyValue (hashMap,
            emptySubarray, emptyIndex, key, value);
        gmosHashMapSubarraySetOccupancy (
            emptySubarray, emptyIndex, true);
        hashMap->entryCount += 1;
        putOk = true;
        gmosHashMapPrintSubarrayStats (hashMap);
    }

    // Attempt to allocate a new subarray and restart the process. The
    // target size for the new subarray is determined by the specified
    // scaling factor.
    else {
        uint32_t targetSize = ((uint32_t) hashMap->entryCount *
            GMOS_CONFIG_HASHMAP_HEAP_SUBARRAY_SIZE_SCALING) / 16;
        uint_fast8_t sizeLog2;
        for (sizeLog2 = 3; sizeLog2 <=
            GMOS_CONFIG_HASHMAP_HEAP_SUBARRAY_MAX_SIZE_LOG2; sizeLog2++) {
            if ((1UL << sizeLog2) >= targetSize) {
                break;
            }
        }
        emptySubarray = gmosHashMapAllocSubarray (
            hashMap->keySize, hashMap->valueSize,
            sizeLog2, hashMap->subarrayList->hashShift);
        if (emptySubarray != NULL) {
            emptySubarray->nextSubarray = hashMap->subarrayList;
            hashMap->subarrayList = emptySubarray;
            triggerBackgroundTask = true;
            goto restart;
        }
        putOk = false;
    }

    // Trigger execution of the background task.
    if (triggerBackgroundTask) {
        hashMap->backgroundState |=
            GMOS_HASHMAP_BACKGROUND_STATE_TRIGGER_FLAG;
        gmosSchedulerTaskResume (&hashMap->backgroundTask);
    }
    return putOk;
}

/*
 * Gets a entry from the hash map. If a matching entry exists, this will
 * read the entry value that corresponds to the specified key.
 */
bool gmosHashMapGet (gmosHashMap_t* hashMap, uint8_t* key, uint8_t* value)
{
    gmosHashMapSubarray_t* matchedSubarray;
    uint_fast16_t matchedIndex;

    // Search for a matching subarray.
    matchedSubarray = gmosHashMapSubarraySearch (
        hashMap, key, &matchedIndex, NULL, NULL);

    // Copy over the stored value if required.
    if ((matchedSubarray != NULL) && (value != NULL)) {
        uint8_t* entryPtr = gmosHashMapSubarrayEntryAddress (
            hashMap, matchedSubarray, matchedIndex);
        byteCopy (value, entryPtr + hashMap->keySize,
            hashMap->valueSize);
    }
    return (matchedSubarray != NULL) ? true : false;
}

/*
 * Deletes an entry from the hash map. If a matching entry exists, this
 * will remove the entry that corresponds to the specified key.
 */
bool gmosHashMapDelete (gmosHashMap_t* hashMap, uint8_t* key)
{
    gmosHashMapSubarray_t* matchedSubarray;
    uint_fast16_t matchedIndex;
    bool triggerBackgroundTask = false;

    // Search for a matching subarray.
    matchedSubarray = gmosHashMapSubarraySearch (
        hashMap, key, &matchedIndex, NULL, NULL);

    // Clear the occupancy flag to delete the entry.
    if (matchedSubarray != NULL) {
        gmosHashMapSubarraySetOccupancy (
            matchedSubarray, matchedIndex, false);
        hashMap->entryCount -= 1;
        triggerBackgroundTask = true;
    }

    // Trigger execution of the background task.
    if (triggerBackgroundTask) {
        hashMap->backgroundState |=
            GMOS_HASHMAP_BACKGROUND_STATE_TRIGGER_FLAG;
        gmosSchedulerTaskResume (&hashMap->backgroundTask);
    }
    return (matchedSubarray != NULL) ? true : false;
}

/*
 * Gets the load factor parameters for the hash map.
 */
uint32_t gmosHashMapLoadFactor (gmosHashMap_t* hashMap, uint32_t* capacity)
{
    gmosHashMapSubarray_t* subarray;
    uint32_t capacitySum;

    // Calculate the capacity from the sum of all the subarrays.
    if (capacity != NULL) {
        capacitySum = 0;
        subarray = hashMap->subarrayList;
        while (subarray != NULL) {
            capacitySum += 1UL + subarray->hashMask;
            subarray = subarray->nextSubarray;
        }
        *capacity = capacitySum;
    }
    return hashMap->entryCount;
}

#endif // GMOS_CONFIG_HEAP_SIZE
