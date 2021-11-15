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
 * This common source file implements the GubbinsMOS memory pool.
 */

#include <stdint.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-mempool.h"

// Statically allocate the memory pool area.
#if (GMOS_CONFIG_MEMPOOL_USE_HEAP == false)
static gmosMempoolSegment_t gmosMempool [GMOS_CONFIG_MEMPOOL_SEGMENT_NUMBER];
#else
static gmosMempoolSegment_t gmosMempool [0];
#endif

// Specifies the head of the free segment list.
static gmosMempoolSegment_t* gmosMempoolFreeList;

// Specifies the number of available free segments.
static uint16_t gmosMempoolFreeSegmentCount;

/*
 * Initialises the memory pool. This should be called exactly once on
 * system initialisation to set up the memory pool prior to using any
 * other memory pool functions.
 */
void gmosMempoolInit (void)
{
    uint16_t i;
    gmosMempoolSegment_t** nextSegmentPtr;
    gmosMempoolSegment_t* currentSegment;

    // Link the memory pool free segment list.
    nextSegmentPtr = &gmosMempoolFreeList;
    for (i = 0; i < GMOS_CONFIG_MEMPOOL_SEGMENT_NUMBER; i++) {
        if (GMOS_CONFIG_MEMPOOL_USE_HEAP) {
            currentSegment = (gmosMempoolSegment_t*)
                GMOS_MALLOC (sizeof (gmosMempoolSegment_t));
            GMOS_ASSERT (ASSERT_FAILURE, (currentSegment != NULL),
                "Out of heap memory when creating memory pool.");
        } else {
            currentSegment = &gmosMempool [i];
        }
        *nextSegmentPtr = currentSegment;
        nextSegmentPtr = &(currentSegment->nextSegment);
    }

    // Add null terminator to the list.
    *nextSegmentPtr = NULL;
    gmosMempoolFreeSegmentCount = GMOS_CONFIG_MEMPOOL_SEGMENT_NUMBER;
}

/*
 * Determines the number of free memory pool segments currently
 * available for allocation.
 */
uint16_t gmosMempoolSegmentsAvailable (void)
{
    return gmosMempoolFreeSegmentCount;
}

/*
 * Allocates a new memory pool segment from the memory pool.
 */
gmosMempoolSegment_t* gmosMempoolAlloc (void)
{
    gmosMempoolSegment_t* segment = gmosMempoolFreeList;
    if (segment != NULL) {
        gmosMempoolFreeList = segment->nextSegment;
        segment->nextSegment = NULL;
        gmosMempoolFreeSegmentCount -= 1;
    }
    return segment;
}

/*
 * Returns a memory pool segment to the memory pool free list after use.
 */
void gmosMempoolFree (gmosMempoolSegment_t* freeSegment)
{
    if (freeSegment != NULL) {
        freeSegment->nextSegment = gmosMempoolFreeList;
        gmosMempoolFreeList = freeSegment;
        gmosMempoolFreeSegmentCount += 1;
    }
}

/*
 * Allocates a number of memory pool segments from the memory pool and
 * returns a pointer to a linked list containing the allocated segments.
 */
gmosMempoolSegment_t* gmosMempoolAllocSegments (uint16_t segmentCount)
{
    uint16_t i;
    gmosMempoolSegment_t* segment;
    gmosMempoolSegment_t* result = NULL;

    // Remove the required number of segments from the free list and
    // null terminate the return list.
    if (segmentCount <= gmosMempoolFreeSegmentCount) {
        segment = gmosMempoolFreeList;
        for (i = 1; i < segmentCount; i++) {
            segment = segment->nextSegment;
        }
        result = gmosMempoolFreeList;
        gmosMempoolFreeList = segment->nextSegment;
        segment->nextSegment = NULL;
        gmosMempoolFreeSegmentCount -= segmentCount;
    }
    return result;
}

/*
 * Returns a number of memory pool segments to the memory pool.
 */
void gmosMempoolFreeSegments (gmosMempoolSegment_t* freeSegments)
{
    uint16_t segmentCount = 0;
    gmosMempoolSegment_t* segment;

    // Count the number of free segments and return them to the free
    // list.
    if (freeSegments != NULL) {
        segmentCount = 1;
        segment = freeSegments;
        while (segment->nextSegment != NULL) {
            segmentCount += 1;
            segment = segment->nextSegment;
        }
        segment->nextSegment = gmosMempoolFreeList;
        gmosMempoolFreeList = freeSegments;
    }
    gmosMempoolFreeSegmentCount += segmentCount;
}
