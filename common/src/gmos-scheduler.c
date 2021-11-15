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
 * This common source file implements the GubbinsMOS scheduler.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-platform.h"
#include "gmos-scheduler.h"
#include "gmos-events.h"

// Define the internal task state encodings.
#define TASK_STATE_INITIALISING 0x00
#define TASK_STATE_SCHEDULED    0x01
#define TASK_STATE_BACKGROUND   0x02
#define TASK_STATE_READY        0x03
#define TASK_STATE_ACTIVE       0x04
#define TASK_STATE_SUSPENDED    0x05

// Specifies the start of the scheduled task list.
static gmosTaskState_t* scheduledTasks = NULL;

// Specifies the start of the background task list.
static gmosTaskState_t* backgroundTasks = NULL;

// Specifies the start of the ready task list.
static gmosTaskState_t* readyTaskListHead = NULL;

// Specifies the end of the ready task list.
static gmosTaskState_t* readyTaskListEnd = NULL;

// Specifies the currently executing task.
static gmosTaskState_t* currentTask = NULL;

// Specifies the head of the scheduler lifecycle monitor list.
static gmosLifecycleMonitor_t* lifecycleMonitors = NULL;

// Tracks the number of 'stay awake' requests.
static uint32_t stayAwakeCounter = 0;

/*
 * Adds a task to the list of tasks that are ready for immediate
 * execution.
 */
static void gmosSchedulerMakeTaskReady (gmosTaskState_t* taskState)
{
    taskState->taskState = TASK_STATE_READY;
    taskState->nextTask = NULL;
    if (readyTaskListHead == NULL) {
        readyTaskListHead = taskState;
        readyTaskListEnd = taskState;
    } else {
        readyTaskListEnd->nextTask = taskState;
        readyTaskListEnd = taskState;
    }
}

/*
 * Inserts a new task into a task queue, ordered according to the
 * specified timestamps. Uses the supplied task status to determine
 * the task queue to use and the associated scheduling timestamp.
 */
static void gmosSchedulerInsertTask (
    gmosTaskState_t* taskState, gmosTaskStatus_t taskStatus)
{
    gmosTaskState_t** taskSearchPtr;

    // Add immediate tasks to the ready task list.
    if (taskStatus == GMOS_TASK_RUN_IMMEDIATE) {
        gmosSchedulerMakeTaskReady (taskState);
        return;
    }

    // Do not insert suspended tasks into the queue.
    if (taskStatus == GMOS_TASK_SUSPEND) {
        taskState->taskState = TASK_STATE_SUSPENDED;
        return;
    }

    // Select the appropriate queue for inserting the task. Tasks that
    // will initiate a device wakeup go into the scheduled queue and
    // those which can execute opportunistically go into the background
    // queue.
    if ((taskStatus & 0x80000000) == 0) {
        taskState->taskState = TASK_STATE_SCHEDULED;
        taskSearchPtr = &scheduledTasks;
    } else {
        taskState->taskState = TASK_STATE_BACKGROUND;
        taskSearchPtr = &backgroundTasks;
    }

    // Calculate the timestamp from the delay field of the task status.
    taskState->timestamp = (int32_t) gmosPalGetTimer();
    taskState->timestamp += (int32_t) (taskStatus & 0x7FFFFFFF);

    // Search from the start of the task list for the correct insertion
    // point.
    while (*taskSearchPtr != NULL) {
        if (((*taskSearchPtr)->timestamp - taskState->timestamp) > 0) {
            break;
        } else {
            taskSearchPtr = &((*taskSearchPtr)->nextTask);
        }
    }

    // Insert the task into the list.
    taskState->nextTask = *taskSearchPtr;
    *taskSearchPtr = taskState;
}

/*
 * Remove a task from the scheduled or background task queue, which
 * converts it to a suspended task.
 */
static void gmosSchedulerRemoveTask (gmosTaskState_t* taskState)
{
    gmosTaskState_t** taskSearchPtr;

    // Select the appropriate queue for removing the task.
    if (taskState->taskState == TASK_STATE_SCHEDULED) {
        taskSearchPtr = &scheduledTasks;
    } else if (taskState->taskState == TASK_STATE_BACKGROUND) {
        taskSearchPtr = &backgroundTasks;
    } else {
        return;
    }

    // Search for the task in the queue.
    while (*taskSearchPtr != NULL) {
        if (*taskSearchPtr == taskState) {
            taskState->taskState = TASK_STATE_SUSPENDED;
            *taskSearchPtr = taskState->nextTask;
            break;
        } else {
            taskSearchPtr = &((*taskSearchPtr)->nextTask);
        }
    }
}

/*
 * Gets the next pending task from a given task list if it is ready
 * to run.
 */
static gmosTaskState_t* gmosSchedulerGetPendingTask (
    gmosTaskState_t** taskListPtr)
{
    gmosTaskState_t* pendingTask = *taskListPtr;
    int32_t currentTime = (int32_t) gmosPalGetTimer();

    if (pendingTask != NULL) {
        if ((pendingTask->timestamp - currentTime) <= 0) {
            *taskListPtr = pendingTask->nextTask;
        } else {
            pendingTask = NULL;
        }
    }
    return pendingTask;
}

/*
 * Gets the time until the next pending task is due to run, expressed
 * as an integer number of system ticks. Negative values imply that the
 * task is overdue.
 */
static int32_t gmosSchedulerGetPendingTaskDelay (
    gmosTaskState_t** taskListPtr)
{
    gmosTaskState_t* pendingTask = *taskListPtr;
    int32_t currentTime = (int32_t) gmosPalGetTimer();
    int32_t pendingTaskDelay;

    if (pendingTask != NULL) {
        pendingTaskDelay = pendingTask->timestamp - currentTime;
    } else {
        pendingTaskDelay = INT32_MAX;
    }
    return pendingTaskDelay;
}

/*
 * Implements the core GubbinsMOS scheduler loop.
 */
void gmosSchedulerStart (void)
{
    gmosLifecycleNotify (SCHEDULER_STARTUP);
    while (true) {
        uint32_t execDelay = 0;
        while (execDelay == 0) {
            execDelay = gmosSchedulerStep ();
        }
        gmosPalIdle (execDelay);
    }
}

/*
 * Performs a single GubbinsMOS scheduler iteration and then returns to
 * the caller.
 */
uint32_t gmosSchedulerStep (void)
{
    uint32_t execDelay = 0;
    gmosTaskState_t* queuedTask;

    // Process waiting event consumer tasks, marking them ready to run.
    queuedTask = gmosEventGetNextConsumer ();
    while (queuedTask != NULL) {
        if (queuedTask->taskState != TASK_STATE_READY) {
            gmosSchedulerRemoveTask (queuedTask);
            gmosSchedulerMakeTaskReady (queuedTask);
        }
        queuedTask = gmosEventGetNextConsumer ();
    }

    // Process scheduled tasks, marking them ready to run if required.
    queuedTask = gmosSchedulerGetPendingTask (&scheduledTasks);
    while (queuedTask != NULL) {
        gmosSchedulerMakeTaskReady (queuedTask);
        queuedTask = gmosSchedulerGetPendingTask (&scheduledTasks);
    }

    // Process background tasks, marking them ready to run if required.
    queuedTask = gmosSchedulerGetPendingTask (&backgroundTasks);
    while (queuedTask != NULL) {
        gmosSchedulerMakeTaskReady (queuedTask);
        queuedTask = gmosSchedulerGetPendingTask (&backgroundTasks);
    }

    // Run the next task in the ready task queue.
    if (readyTaskListHead != NULL) {
        gmosTaskStatus_t taskStatus;

        // Pop the next task from the head of the ready task list.
        currentTask = readyTaskListHead;
        readyTaskListHead = currentTask->nextTask;

        // Mark the task as active during execution.
        currentTask->taskState = TASK_STATE_ACTIVE;
        taskStatus = currentTask->taskTickFn (currentTask->taskData);

        // Place the task back in the appropriate task list.
        gmosSchedulerInsertTask (currentTask, taskStatus);
        currentTask = NULL;
    }

    // Calculate the idle period if no tasks are ready. Implement busy
    // waiting if one or more scheduler stay awake requests are
    // currently active.
    else if (stayAwakeCounter == 0) {
        int32_t delay = gmosSchedulerGetPendingTaskDelay (&scheduledTasks);
        execDelay = (delay < 0) ? 0 : (uint32_t) delay;
    }
    return execDelay;
}

/*
 * Starts a new task, making it ready for scheduler execution.
 */
void gmosSchedulerTaskStart (gmosTaskState_t* newTask)
{
    gmosSchedulerMakeTaskReady (newTask);
}

/*
 * Resumes scheduling of a suspended or delayed task, making it ready
 * for scheduler execution.
 */
void gmosSchedulerTaskResume (gmosTaskState_t* resumedTask)
{
    if ((resumedTask->taskState != TASK_STATE_READY) &&
        (resumedTask->taskState != TASK_STATE_ACTIVE)) {
        gmosSchedulerRemoveTask (resumedTask);
        gmosSchedulerMakeTaskReady (resumedTask);
    }
}

/*
 * Requests that the scheduler avoids powering down the device.
 */
void gmosSchedulerStayAwake (void)
{
    GMOS_ASSERT (ASSERT_FAILURE, (stayAwakeCounter < UINT32_MAX),
        "Scheduler wake counter overflow detected");
    stayAwakeCounter += 1;
}

/*
 * Requests that the scheduler allows the device to sleep when possible.
 */
void gmosSchedulerCanSleep (void)
{
    GMOS_ASSERT (ASSERT_FAILURE, (stayAwakeCounter > 0),
        "Scheduler wake counter underflow detected");
    stayAwakeCounter -= 1;
}

/*
 * Accesses the task state data for the currently executing task.
 */
gmosTaskState_t* gmosSchedulerCurrentTask (void)
{
    return currentTask;
}

/*
 * Prioritises between two task status values. This selects the task
 * status value that is most immediate in terms of the task scheduling
 * requirements.
 */
gmosTaskStatus_t gmosSchedulerPrioritise (
    gmosTaskStatus_t taskStatusA, gmosTaskStatus_t taskStatusB)
{
    // If either of the task status values is 'task suspend', the other
    // value will be taken by default.
    if (taskStatusA == GMOS_TASK_SUSPEND) {
        return taskStatusB;
    }
    if (taskStatusB == GMOS_TASK_SUSPEND) {
        return taskStatusA;
    }

    // If the task status values refer to different scheduler queues,
    // convert them both to foreground tasks.
    if ((taskStatusA & 0x80000000) != (taskStatusB & 0x80000000)) {
        taskStatusA &= 0x7FFFFFFF;
        taskStatusB &= 0x7FFFFFFF;
    }

    // Select the earliest scheduled option.
    return (taskStatusA < taskStatusB) ? taskStatusA : taskStatusB;
}

/*
 * Adds a scheduler lifecycle monitor to receive notifications of
 * scheduler lifecycle management events. The new monitor is added to
 * the head of the list.
 */
void gmosLifecycleAddMonitor (gmosLifecycleMonitor_t* lifecycleMonitor,
    bool (*handlerFunction) (gmosLifecycleStatus_t))
{
    lifecycleMonitor->handlerFn = handlerFunction;
    lifecycleMonitor->nextMonitor = lifecycleMonitors;
    lifecycleMonitors = lifecycleMonitor;
}

/*
 * Issues a scheduler lifecycle status notification to all of the
 * registered lifecycle monitors. Calls each lifecycle monitor in the
 * reverse order to which they were added to the list.
 */
bool gmosLifecycleNotify
    (gmosLifecycleStatus_t lifecycleStatus)
{
    bool retVal = true;
    gmosLifecycleMonitor_t* currentMonitor = lifecycleMonitors;
    while (currentMonitor != NULL) {
        retVal &= currentMonitor->handlerFn (lifecycleStatus);
        currentMonitor = currentMonitor->nextMonitor;
    }
    return retVal;
}

