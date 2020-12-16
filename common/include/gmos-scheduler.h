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
 * This header defines the API for the GubbinsMOS task scheduler.
 */

#ifndef GMOS_SCHEDULER_H
#define GMOS_SCHEDULER_H

#include <stdint.h>
#include <stdbool.h>
#include "gmos-config.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * Defines the GubbinsMOS task status type that is used to indicate the
 * required task scheduling behaviour.
 */
typedef uint32_t gmosTaskStatus_t;

/**
 * Defines the GubbinsMOS task state data structure which is used for
 * managing an individual task.
 */
typedef struct gmosTaskState_t {

    // This is a pointer to the task execution function. It will be
    // passed a pointer to the task data item and return an encoded
    // task status that is used to indicate the required task
    // rescheduling behaviour.
    gmosTaskStatus_t (*taskTickFn) (void*);

    // This is a pointer to an arbitrary task data item that is used to
    // hold persistent task specific data.
    void* taskData;

    // This is a pointer to a task name string, which must remain valid
    // for the lifetime of the task. A NULL reference may be used if no
    // task name is assigned.
    const char* taskName;

    // This is a pointer to the next task in the task queue.
    struct gmosTaskState_t* nextTask;

    // This is a timestamp that is used to indicate the next platform
    // timer value at which the task is to be run.
    int32_t timestamp;

    // This is the current task status, which indicates whether the
    // task is initialising, running, suspended or queued.
    uint8_t taskState;

} gmosTaskState_t;

/**
 * Defines the set of scheduler lifecycle status notifications that may
 * be passed to the lifecycle notification handlers.
 */
typedef enum {
    SCHEDULER_STARTUP,
    SCHEDULER_SHUTDOWN,
    SCHEDULER_ENTER_POWER_SAVE,
    SCHEDULER_EXIT_POWER_SAVE,
    SCHEDULER_ENTER_DEEP_SLEEP,
    SCHEDULER_EXIT_DEEP_SLEEP
} gmosLifecycleStatus_t;

/**
 * Defines the GubbinsMOS task lifecycle monitor type that is used to
 * processes scheduler lifecycle events.
 */
typedef struct gmosLifecycleMonitor_t {

    // This is a pointer to the lifecycle handler function. It will be
    // passed a lifecycle status value and return a boolean value that
    // should be set to 'true' on successful completion.
    bool (*handlerFn) (gmosLifecycleStatus_t);

    // This is a pointer to the next lifecycle monitor in the list.
    struct gmosLifecycleMonitor_t* nextMonitor;

} gmosLifecycleMonitor_t;


/**
 * Provides a task definition macro that can be used to enforce static
 * type checking when implementing GubbinsMOS tasks. The macro defines
 * the following functions:
 *
 * void <_id_>_start (gmosTaskState_t* taskState,
 *     <_data_type_>* taskData, const char* taskName)
 *
 * This function may be used to start a new instance of the defined task
 * type using the referenced task state and task data structures. A
 * constant task name string may also be provided for task monitoring.
 *
 * @param _id_ This is the identifier to be used when referring to the
 *     GubbinsMOS task type defined here.
 * @param _exec_fn_ This is the name of the GubbinsMOS task execution
 *     function that is to be associated with the task. It should
 *     accept a single pointer of type _data_type_ that encapsulates
 *     the task state. It should return a task status value which will
 *     be used to control task rescheduling.
 * @param _data_type_ This is the data type of the task data structure
 *     which encapsulates the task state.
 */
#define GMOS_TASK_DEFINITION(_id_, _exec_fn_, _data_type_)             \
                                                                       \
static gmosTaskStatus_t _ ## _id_ ## _exec_ (void* taskData)           \
{                                                                      \
    return _exec_fn_ ((_data_type_ *) taskData);                       \
}                                                                      \
                                                                       \
static inline void _id_ ## _start (gmosTaskState_t* taskState,         \
    _data_type_ * taskData, const char* taskName)                      \
{                                                                      \
    taskState->taskTickFn = _ ## _id_ ## _exec_;                       \
    taskState->taskData = taskData;                                    \
    taskState->taskName = taskName;                                    \
    gmosSchedulerTaskStart (taskState);                                \
}

/**
 * Defines the task function return value that is used to indicate that
 * the task should be re-run immediately by the scheduler.
 */
#define GMOS_TASK_RUN_IMMEDIATE ((gmosTaskStatus_t) 0)

/**
 * Defines the task function return value that is used to indicate that
 * the task should be re-run in the background by the scheduler.
 */
#define GMOS_TASK_RUN_BACKGROUND ((gmosTaskStatus_t) \
    (0x80000000 | GMOS_CONFIG_BACKGROUND_TASK_INTERVAL))

/**
 * Defines the task function return value that is used to indicate that
 * the task can be suspended for an indefinite period.
 */
#define GMOS_TASK_SUSPEND ((gmosTaskStatus_t) 0x80000000)

/**
 * Defines the task function return macro that is used to indicate that
 * the task should be re-run after a specified number of platform timer
 * ticks. If the device is subsequently placed in idle mode it will be
 * reactivated at the appropriate time to re-run the task.
 * @param _delay_ This is the delay after which the task will be re-run.
 *     It should be an integer number of system timer ticks in the range
 *     from 1 to 2^31-1.
 */
#define GMOS_TASK_RUN_LATER(_delay_) ((gmosTaskStatus_t) \
    (((_delay_) < 1) ? 1 : ((_delay_) > 0x7FFFFFFF) ? 0x7FFFFFFF : (_delay_)))

/**
 * Defines the task function return macro that is used to indicate that
 * the task should be re-run at the first opportunity after a specified
 * number of platform timer ticks. If the device is subsequently placed
 * in idle mode it will not be reactivated specifically to run this
 * task.
 * @param _delay_ This is the minimum delay after which the task will be
 *     re-run. It should be an integer number of system timer ticks in
 *     the range from 1 to 2^31-1.
 */
#define GMOS_TASK_RUN_AFTER(_delay_) ((gmosTaskStatus_t) (0x80000000 | \
    (((_delay_) < 1) ? 1 : ((_delay_) > 0x7FFFFFFF) ? 0x7FFFFFFF : (_delay_))))

/**
 * Implements the core GubbinsMOS scheduler loop, which repeatedly
 * calls the 'gmosSchedulerTick' function. This should be called from
 * the 'main' function after setting up the initial task set and is not
 * expected to return.
 */
void gmosSchedulerStart (void);

/**
 * Performs a single GubbinsMOS scheduler iteration and then returns to
 * the caller. This may be used instead of 'gmosSchedulerStart' for
 * environments which already have an outer event loop, such as the
 * Arduino platform.
 * @return Returns the number of system timer ticks that may elapse
 *     before another call to this function is required.
 */
uint32_t gmosSchedulerStep (void);

/**
 * Starts a new task, making it ready for scheduler execution. This
 * function should be called exactly once for each newly created task.
 * @param newTask This is a pointer to the task state for a newly
 *     created task.
 */
void gmosSchedulerTaskStart (gmosTaskState_t* newTask);

/**
 * Immediately resumes processing of a suspended or delayed task, making
 * it ready for scheduler execution.
 * @param resumedTask This is a pointer to the task state for the task
 *     that is to be resumed.
 */
void gmosSchedulerTaskResume (gmosTaskState_t* resumedTask);

/**
 * Requests that the scheduler avoids powering down the device. This
 * will typically be called when an interrupt driven I/O process is
 * running in order to allow it to run to completion. Calling this
 * function increments an internal 'stay awake' counter, so each call
 * must be paired with a subsequent call to the 'gmosSchedulerCanSleep'
 * function.
 */
void gmosSchedulerStayAwake (void);

/**
 * Requests that the scheduler allows the device to sleep when possible.
 * Calling this function decrements an internal 'stay awake' counter and
 * only allows the device to sleep once the counter is decremented to
 * zero. This means that each call must be paired with a previous call
 * to the 'gmosSchedulerStayAwake' function.
 */
void gmosSchedulerCanSleep (void);

/**
 * Accesses the task state data for the currently executing task.
 * @return Returns a pointer to the task state data for the currently
 *     executing task.
 */
gmosTaskState_t* gmosSchedulerCurrentTask (void);

/**
 * Adds a scheduler lifecycle monitor to receive notifications of
 * scheduler lifecycle management events.
 * @param lifecycleMonitor This is the new lifecycle monitor that is to
 *     be added to the lifecycle monitor list.
 * @param handlerFunction This is a pointer to the lifecycle handler
 *     function that is being registered with the lifecycle monitor.
 *     It should accept a single parameter which encodes the lifecycle
 *     status update and return a boolean value which indicates whether
 *     the lifecycle status update is acceptable.
 */
void gmosLifecycleAddMonitor (gmosLifecycleMonitor_t* lifecycleMonitor,
    bool (*handlerFunction) (gmosLifecycleStatus_t));

/**
 * Issues a scheduler lifecycle status notification to all of the
 * registered lifecycle monitors. This will normally be called
 * automatically as part of the platform specific 'gmosPalIdle'
 * implementation.
 * @param lifecycleStatus This is the lifecycle status indicator that
 *     is to be passed to all of the registered lifecycle status
 *     monitors.
 * @return Returns a boolean value which will be set to 'true' if all
 *     the lifecycle status monitors indicated that the lifecycle status
 *     update was acceptable and 'false' otherwise.
 */
bool gmosLifecycleNotify
    (gmosLifecycleStatus_t lifecycleStatus);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // GMOS_SCHEDULER_H
