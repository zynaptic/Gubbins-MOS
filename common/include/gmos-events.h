/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2020-2024 Zynaptic Limited
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
 * This header defines the API for GubbinsMOS asynchronous event flag
 * support.
 */

#ifndef GMOS_EVENTS_H
#define GMOS_EVENTS_H

#include <stdint.h>
#include <stddef.h>
#include "gmos-scheduler.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * Defines the GubbinsMOS event state data structure which is used for
 * managing a set of asynchronous event flags.
 */
typedef struct gmosEvent_t {

    // This is a pointer to the task state data structure for the event
    // consumer task.
    gmosTaskState_t* consumerTask;

    // This is a pointer to the next event in the pending event queue.
    struct gmosEvent_t* nextEvent;

    // This is a 32-bit integer which is used to store the 32 individual
    // event flags as a bit field. To ensure safe operation this should
    // only be accessed via the get, set and clear functions.
    uint32_t eventBits;

} gmosEvent_t;

/**
 * Provides a compile time initialisation macro for a set of GubbinsMOS
 * event flags. Assigning this macro value to an event state variable on
 * declaration may be used instead of a call to the 'gmosEventInit'
 * function to set up the event flags for subsequent asynchronous
 * notifications.
 * @param _consumer_task_ This is the consumer task which is to be
 *     notified of any changes to the event flags. A null reference will
 *     disable this functionality.
 */
#define GMOS_EVENT_INIT(_consumer_task_)                               \
    { _consumer_task_, NULL, 0 }

/**
 * Performs a one-time initialisation of a set of GubbinsMOS event
 * flags. This should be called during initialisation to set up the
 * event flags for subsequent asynchronous notifications.
 * @param event This is the event state data structure which is to be
 *     initialised.
 * @param consumerTask This is the consumer task which is to be notified
 *     of any changes to the event flags. A null reference will disable
 *     this functionality.
 */
void gmosEventInit (gmosEvent_t* event, gmosTaskState_t* consumerTask);

/**
 * Accesses the current state of the event bits, each of which will
 * normally be treated as an individual event flag.
 * @param event This is the event state data structure for which the
 *     associated event bits are being accessed.
 * @return Returns the current state of the event bits.
 */
uint32_t gmosEventGetBits (gmosEvent_t* event);

/**
 * Tests the state of the event bits against a specified bit mask.
 * Returns true if all the bits identified by the bit mask are set.
 * @param event This is the event state data structure for which the
 *     associated event bits are being accessed.
 * @param bitMask This is a bit vector specifying the event bits that
 *     are to be tested.
 * @return Returns a boolean value which will be set to 'true' if all
 *     the bits specified by the bit mask are set and 'false' otherwise.
 */
bool gmosEventTestAllBits (gmosEvent_t* event, uint32_t bitMask);

/**
 * Tests the state of the event bits against a specified bit mask.
 * Returns true if any of the bits identified by the bit mask are set.
 * @param event This is the event state data structure for which the
 *     associated event bits are being accessed.
 * @param bitMask This is a bit vector specifying the event bits that
 *     are to be tested.
 * @return Returns a boolean value which will be set to 'true' if any of
 *     the bits specified by the bit mask are set and 'false' otherwise.
 */
bool gmosEventTestAnyBits (gmosEvent_t* event, uint32_t bitMask);

/**
 * Assigns the full set of event bits, as specified by the bit values.
 * Assigning event bits will always queue the consumer task for
 * subsequent event processing.
 * @param event This is the event state data structure for which the
 *     specified event bits are being modified.
 * @param bitValues This is a bit vector specifying the new values of
 *     all the event bits.
 * @return Returns the state of the event bits prior to modification.
 */
uint32_t gmosEventAssignBits (gmosEvent_t* event, uint32_t bitValues);

/**
 * Assigns a masked set of event bits, as specified by the bit values
 * and the associated bit mask. For each bit in the bit mask that is set
 * to '1', the corresponding event bit will be set to the bit value at
 * the same position in the bit values parameter. Assigning masked event
 * bits will always queue the consumer task for subsequent event
 * processing.
 * @param event This is the event state data structure for which the
 *     specified event bits are being modified.
 * @param bitMask This is a bit vector which is used to select the bits
 *     that are to be assigned to a new value.
 * @param bitValues This is a bit vector specifying the new values of
 *     all the event bits.
 * @return Returns the state of the event bits prior to modification.
 */
uint32_t gmosEventAssignMaskedBits (gmosEvent_t* event,
    uint32_t bitMask, uint32_t bitValues);

/**
 * Sets one or more event bits, as specified by the bit mask. For each
 * bit in the bit mask that is set to '1', the corresponding event bit
 * will be set to '1'. Setting event bits will always queue the consumer
 * task for subsequent event processing.
 * @param event This is the event state data structure for which the
 *     specified event bits are being modified.
 * @param bitMask This is a bit vector specifying the event bits that
 *     are to be set to '1'.
 * @return Returns the state of the event bits prior to modification.
 */
uint32_t gmosEventSetBits (gmosEvent_t* event, uint32_t bitMask);

/**
 * Clears one or more event bits, as specified by the bit mask. For each
 * bit in the bit mask that is set to '1', the corresponding event bit
 * will be cleared to '0'. Clearing event bits will always queue the
 * consumer task for subsequent event processing.
 * @param event This is the event state data structure for which the
 *     specified event bits are being modified.
 * @param bitMask This is a bit vector specifying the event bits that
 *     are to be cleared to '0'.
 * @return Returns the state of the event bits prior to modification.
 */
uint32_t gmosEventClearBits (gmosEvent_t* event, uint32_t bitMask);

/**
 * Resets all the event bits to zero, returning the state of the event
 * bits immediately prior to reset. Resetting event bits will not queue
 * the consumer task for subsequent event processing. This is the normal
 * method of processing events in the consumer task.
 * @param event This is the event state data structure for which the
 *     event bits are being reset.
 * @return Returns the state of the event bits prior to modification.
 */
uint32_t gmosEventResetBits (gmosEvent_t* event);

/**
 * If one or more events have occurred, this function will return the
 * associated consumer tasks in the order in which the events occured.
 * @return Returns a reference to the next event consumer task or a null
 *     reference if no event consumer tasks are waiting to be processed.
 */
gmosTaskState_t* gmosEventGetNextConsumer (void);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // GMOS_EVENTS_H
