/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2020 Zynaptic Limited
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
 * This header defines the API for the GubbinsMOS memory pool, which
 * supports fixed sized dynamic memory allocation.
 */

#ifndef GMOS_MEMPOOL_H
#define GMOS_MEMPOOL_H

#include <stdint.h>
#include "gmos-config.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * Defines the GubbinsMOS memory pool segment data structure which is
 * used for allocating a single memory pool segment.
 */
typedef struct gmosMempoolSegment_t {

    // Specifies the location of the next segment in the list.
    struct gmosMempoolSegment_t* nextSegment;

    // Allocates a block of word aligned segment data which may be
    // accessed as either a byte array or a 32-bit word array.
    union {
        uint32_t words [GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE / 4];
        uint8_t  bytes [GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE];
    } data;

} gmosMempoolSegment_t;

/**
 * Initialises the memory pool. This is called automatically during
 * system initialisation to set up the memory pool.
 */
void gmosMempoolInit (void);

/**
 * Determines the number of free memory pool segments currently
 * available for allocation.
 * @return Returns the number of memory pool segments currently
 *     available for allocation.
 */
uint16_t gmosMempoolSegmentsAvailable (void);

/**
 * Allocates a new memory pool segment from the memory pool and returns
 * a pointer to it.
 * @return Returns a pointer to an allocated memory pool segment.
 *    Returns 'NULL' if all memory pool segments have already been
 *    allocated.
 */
gmosMempoolSegment_t* gmosMempoolAlloc (void);

/**
 * Returns a memory pool segment to the memory pool free list after use.
 * @param freeSegment This is a pointer to a memory pool segment
 *     previously allocated using 'gmosMempoolAlloc' that is to be
 *     returned to the memory pool free list.
 */
void gmosMempoolFree (gmosMempoolSegment_t* freeSegment);

/**
 * Allocates a number of memory pool segments from the memory pool and
 * returns a pointer to a linked list containing the allocated segments.
 * @param segmentCount This is the number of memory pool segments that
 *     are to be allocated.
 * @return Returns a pointer to a linked list that contains the
 *     specified number of segments, or a null reference if the
 *     requested number of segments are not available.
 */
gmosMempoolSegment_t* gmosMempoolAllocSegments (uint16_t segmentCount);

/**
 * Returns a number of memory pool segments to the memory pool.
 * @param freeSegments This is a pointer to a linked list of memory pool
 *     segments that are to be returned to the memory pool.
 */
void gmosMempoolFreeSegments (gmosMempoolSegment_t* freeSegments);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // GMOS_MEMPOOL_H
