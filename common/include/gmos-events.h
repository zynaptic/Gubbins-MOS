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
 * Sets one or more event bits, as specified by the bit mask. For each
 * bit in the bit mask that is set to '1', the corresponding event bit
 * will be set to '1'.
 * @param event This is the event state data structure for which the
 *     specified event bits are being modified.
 * @return Returns the state of the event bits prior to modification.
 */
uint32_t gmosEventSetBits (gmosEvent_t* event, uint32_t bitMask);

/**
 * Clears one or more event bits, as specified by the bit mask. For each
 * bit in the bit mask that is set to '1', the corresponding event bit
 * will be set to '0'.
 * @param event This is the event state data structure for which the
 *     specified event bits are being modified.
 * @return Returns the state of the event bits prior to modification.
 */
uint32_t gmosEventClearBits (gmosEvent_t* event, uint32_t bitMask);

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
