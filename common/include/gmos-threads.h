/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2023 Zynaptic Limited
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
 * This header defines a set of macros which may be used to implement a
 * threaded programming model on top of conventional GubbinsMOS state
 * machine based tasks. It is similar to the native C coroutine model
 * proposed by Simon Tatham and the protothread model described by Adam
 * Dunkels et al, but uses the 'dynamic goto' form rather than the
 * 'Duff's Device' form. This approach has some limitations compared to
 * conventional 'stackful' threads, namely:
 *   - The macros should only be used in the main task function and can
 *     not be used in nested subroutines. An exception to this general
 *     rule is the 'multi-phase' state machine design pattern.
 *   - When one of the macros is used to yield control to the scheduler
 *     any data stored as local variables on the stack will be lost.
 */

#ifndef GMOS_THREADS_H
#define GMOS_THREADS_H

#include <stdbool.h>
#include <stddef.h>
#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-scheduler.h"

/**
 * Defines the GubbinsMOS thread state data type that is used to hold
 * the execution state for a single GubbinsMOS thread.
 */
typedef void* gmosThread_t;

/**
 * Provides a compile time initialisation macro for a GubbinsMOS thread
 * state data item. Assigning this macro value to a thread state
 * variable on declaration may be used instead of a call to the
 * 'gmosThreadInit' function in order to set up the GubbinsMOS thread
 * for subsequent use.
 */
#define GMOS_THREAD_INIT() ((gmosThread_t) NULL)

/**
 * Performs a one-time initialisation of a GubbinsMOS thread state data
 * item. This should be called during initialisation in order to set up
 * the GubbinsMOS thread for subsequent use.
 * @param thread This is the GubbinsMOS thread state data item that is
 *     to be initialised.
 */
static inline void gmosThreadInit (gmosThread_t* thread) {
    *thread = (gmosThread_t) NULL;
}

/**
 * This is macro that should be use to set up a function for use as a
 * GubbinsMOS thread. It should be invoked at the start of the function
 * as part of the variable declaration region.
 * @param _thread_state_ This is the thread state data item that will be
 *     used for storing the current thread state during execution.
 */
#define GMOS_THREAD_START(_thread_state_)                              \
gmosThread_t* _gmos_thread_state_ptr_ = &(_thread_state_);             \
gmosTaskStatus_t _gmos_thread_status_ = GMOS_TASK_SUSPEND;             \
if ((*_gmos_thread_state_ptr_) != (gmosThread_t) NULL) {               \
    GMOS_PLATFORM_GOTO_LABEL_ADDRESS(*_gmos_thread_state_ptr_);        \
}

/**
 * Provides a mechanism for explicitly assigning the status of the
 * GubbinsMOS task that is responsible for executing the thread
 * function. On calling this macro, all data stored in local function
 * scope variables will be invalidated and the associated GubbinsMOS
 * task will be rescheduled using the specified task status.
 * @param _status_ This is the GubbinsMOS task status which is to be
 *     used for rescheduling the associated GubbinsMOS task.
 */
#define GMOS_THREAD_SET_STATUS(_status_) do {                          \
*_gmos_thread_state_ptr_ =                                             \
    GMOS_PLATFORM_GET_LABEL_ADDRESS(GMOS_THREAD_MAKE_LABEL(__LINE__)); \
_gmos_thread_status_ = _status_;                                       \
goto _gmos_thread_exit_;                                               \
GMOS_THREAD_MAKE_LABEL(__LINE__) : do {} while (false);                \
} while (false)

/**
 * Provides a meachanism for suspending execution of the GubbinsMOS
 * thread function until execution of the associated task is explicitly
 * resumed by the scheduler. On calling this macro, all data stored in
 * local function scope variables will be invalidated and the associated
 * GubbinsMOS task will placed in the suspended task state.
 */
#define GMOS_THREAD_SUSPEND()                                          \
    GMOS_THREAD_SET_STATUS (GMOS_TASK_SUSPEND)

/**
 * Provides a mechanism for the GubbinsMOS thread function to yield
 * control of the scheduler to other pending tasks. On calling this
 * macro, all data stored in local function scope variables will be
 * invalidated and the associated GubbinsMOS task will be rescheduled
 * for immediate execution.
 */
#define GMOS_THREAD_YIELD()                                            \
    GMOS_THREAD_SET_STATUS (GMOS_TASK_RUN_IMMEDIATE)

/**
 * Provides a mechanism for placing the GubbinsMOS thread in an idle
 * state for the specified duration. Thread execution will be resumed
 * after the specified delay or if execution of the associated task is
 * explicitly resumed by the scheduler. If the device is placed in a low
 * power sleep state it will be powered up after the specified delay
 * to resume execution of the thread. On calling this macro, all data
 * stored in local function scope variables will be invalidated and the
 * associated GubbinsMOS task will placed in the 'run later' task state.
 * @param _delay_ This is the delay after which thread execution should
 *     be resumed, expressed as an integer number of GubbinsMOS system
 *     timer ticks.
 */
#define GMOS_THREAD_IDLE(_delay_)                                      \
    GMOS_THREAD_SET_STATUS (GMOS_TASK_RUN_LATER(_delay_))

/**
 * Provides a mechanism for placing the GubbinsMOS thread in the sleep
 * state for at least the specified duration. Thread execution will be
 * resumed after the specified delay or if execution of the associated
 * task is explicitly resumed by the scheduler. However, if the device
 * is placed in a low power sleep state it will not be powered up
 * specifically to resume execution of the thread. On calling this
 * macro, all data stored in local function scope variables will be
 * invalidated and the associated GubbinsMOS task will placed in the
 * 'run after' task state.
 * @param _delay_ This is the delay after which thread execution should
 *     be resumed, expressed as an integer number of GubbinsMOS system
 *     timer ticks.
 */
#define GMOS_THREAD_SLEEP(_delay_)                                     \
    GMOS_THREAD_SET_STATUS (GMOS_TASK_RUN_AFTER(_delay_))

/**
 * This is a macro that should be placed at the end of a GubbinsMOS
 * thread function in order to stop further execution of the thread.
 * After executing the thread stop macro, further processing using the
 * thread can only be resumed by reinitialising the thread state data
 * item and explicitly resuming execution of the associated GubbinsMOS
 * task.
 */
#define GMOS_THREAD_STOP() do {                                        \
*_gmos_thread_state_ptr_ =                                             \
    GMOS_PLATFORM_GET_LABEL_ADDRESS(_gmos_thread_exit_);               \
_gmos_thread_status_ = GMOS_TASK_SUSPEND;                              \
_gmos_thread_exit_ : return (_gmos_thread_status_);                    \
} while (false)

/**
 * This is a macro that should be placed at the end of a GubbinsMOS
 * thread function in order to continue execution of the thread. After
 * executing the thread continue macro, all data stored in local
 * function scope variables will be invalidated and the thread state
 * will be reinitialised in order to restart the thread function. The
 * associated GubbinsMOS task will then be rescheduled for immediate
 * execution.
 */
#define GMOS_THREAD_CONTINUE() do {                                    \
*_gmos_thread_state_ptr_ = (gmosThread_t) NULL;                        \
_gmos_thread_status_ = GMOS_TASK_RUN_IMMEDIATE;                        \
_gmos_thread_exit_ : return (_gmos_thread_status_);                    \
} while (false)

// Defines a utility macro for concatenating statement label strings.
#define GMOS_THREAD_CONCAT_STRINGS(_a_, _b_) _a_ ## _b_

// Defines a utility macro for generating unique C statement labels.
#define GMOS_THREAD_MAKE_LABEL(_line_)                                 \
    GMOS_THREAD_CONCAT_STRINGS (_gmos_thread_label_, _line_)

#endif // GMOS_THREADS_H
