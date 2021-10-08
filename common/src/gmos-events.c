/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2020-2021 Zynaptic Limited
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
 * Implements the GubbinsMOS asynchronous event flag support.
 */

#include <stdint.h>
#include <stddef.h>

#include "gmos-platform.h"
#include "gmos-events.h"

// Specifies the head of the pending event queue.
static gmosEvent_t* pendingEvents = NULL;

// Specifies a volatile flag that can be used to avoid disabling
// interrupts to check for queued events.
static volatile bool pendingEventsReady = false;

/*
 * Appends an event to the end of the pending event queue if not already
 * present in the queue.
 */
static void gmosEventAppendToQueue (gmosEvent_t* event)
{
    gmosEvent_t** nextEventPtr;

    // Only append the event to the event queue if it has an associated
    // consumer task.
    if (event->consumerTask == NULL) {
        return;
    }

    // Loop until the end of the queue, exiting if the queue already
    // contains the event.
    nextEventPtr = &pendingEvents;
    while (*nextEventPtr != NULL) {
        if (*nextEventPtr == event) {
            return;
        }
        nextEventPtr = &((*nextEventPtr)->nextEvent);
    }

    // Append the event to the end of the queue.
    event->nextEvent = NULL;
    *nextEventPtr = event;
    pendingEventsReady = true;
}

/*
 * Performs a one-time initialisation of a set of GubbinsMOS event
 * flags. This should be called during initialisation to set up the
 * event flags for subsequent asynchronous notifications.
 */
void gmosEventInit (gmosEvent_t* event, gmosTaskState_t* consumerTask)
{
    event->consumerTask = consumerTask;
    event->nextEvent = NULL;
    event->eventBits = 0;
}

/*
 * Accesses the current state of the event bits, each of which will
 * normally be treated as an individual event flag.
 */
uint32_t gmosEventGetBits (gmosEvent_t* event)
{
    uint32_t eventBits = 0;

    // Read the event bits with interrupts disabled.
    gmosPalMutexLock ();
    eventBits = event->eventBits;
    gmosPalMutexUnlock ();
    return eventBits;
}

/*
 * Sets one or more event bits, as specified by the bit mask.
 */
uint32_t gmosEventSetBits (gmosEvent_t* event, uint32_t bitMask)
{
    uint32_t eventBits = 0;

    // Set the event bits with interrupts disabled and then add the
    // event to the pending queue.
    gmosPalMutexLock ();
    eventBits = event->eventBits;
    event->eventBits |= bitMask;
    gmosEventAppendToQueue (event);
    gmosPalMutexUnlock ();
    return eventBits;
}

/*
 * Clears one or more event bits, as specified by the bit mask.
 */
uint32_t gmosEventClearBits (gmosEvent_t* event, uint32_t bitMask)
{
    uint32_t eventBits = 0;

    // Clear the event bits with interrupts disabled and then add the
    // event to the pending queue.
    gmosPalMutexLock ();
    eventBits = event->eventBits;
    event->eventBits &= ~bitMask;
    gmosEventAppendToQueue (event);
    gmosPalMutexUnlock ();
    return eventBits;
}

/*
 * If one or more events have occurred, this function will return the
 * associated consumer tasks in the order in which the events occured.
 */
gmosTaskState_t* gmosEventGetNextConsumer (void)
{
    gmosEvent_t* event = NULL;

    // Avoid queue processing if there are no pending events.
    if (!pendingEventsReady) {
        return NULL;
    }

    // Pop the next event from the queue with interrupts disabled.
    gmosPalMutexLock ();
    if (pendingEvents != NULL) {
        event = pendingEvents;
        pendingEvents = event->nextEvent;
    }
    if (pendingEvents == NULL) {
        pendingEventsReady = false;
    }
    gmosPalMutexUnlock ();

    // Return the consumer task associated with the event.
    return (event == NULL) ? NULL : event->consumerTask;
}
