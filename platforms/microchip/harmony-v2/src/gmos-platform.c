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
 * Implements the common API for the GubbinsMOS platform abstraction
 * layer for the Microchip Harmony V2 vendor framework.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>

#include "gmos-config.h"
#include "gmos-platform.h"

// Use the Harmony framework host OS abstraction layer.
#include "osal/osal.h"

// Provide mapping of log levels to human readable strings.
static const char* logLevelNames [] = {
    "GMOS-VERBOSE", "GMOS-DEBUG  ", "GMOS-INFO   ",
    "GMOS-WARNING", "GMOS-ERROR  ", "GMOS-FAILURE" };

// Specify the platform mutex lock state variables.
static uint32_t mutexLockCount = 0;
static OSAL_CRITSECT_DATA_TYPE mutexLockState;

/*
 * Initialises the platform abstraction layer on startup.
 */
void gmosPalInit (void)
{
}

/*
 * Requests that the platform abstraction layer terminate all further
 * processing.
 */
void gmosPalExit (uint8_t status)
{
    // Temporary implementation - enter an infinite loop.
    while (true) {};
}

/*
 * Claims the main platform mutex lock.
 */
void gmosPalMutexLock (void)
{
    // Ensure interrupts are disabled before modifying the lock count.
    if (mutexLockCount == 0) {
        mutexLockState = OSAL_CRIT_Enter (OSAL_CRIT_TYPE_HIGH);
    }
    mutexLockCount += 1;
}

/*
 * Releases the main platform mutex lock.
 */
void gmosPalMutexUnlock (void)
{
    // Decrement the lock count and enable interrupts if required.
    mutexLockCount -= 1;
    if (mutexLockCount == 0) {
        OSAL_CRIT_Leave (OSAL_CRIT_TYPE_HIGH, mutexLockState);
    }
}

/*
 * Provides platform level handling of fixed string log messages.
 */
void gmosPalLog (const char* fileName, uint32_t lineNo,
    gmosPalLogLevel_t logLevel, const char* msgPtr)
{
    gmosPalLogFmt (fileName, lineNo, logLevel, msgPtr);
}

/*
 * Provides platform level handling of formatted log messages.
 */
void gmosPalLogFmt (const char* fileName, uint32_t lineNo,
    gmosPalLogLevel_t logLevel, const char* msgPtr, ...)
{
    char writeBuffer [GMOS_CONFIG_LOG_MESSAGE_SIZE + 3];
    size_t writeSize;
    va_list args;
    const char* levelString;

    // Map the log level to the corresponding text.
    if ((logLevel < LOG_VERBOSE) || (logLevel > LOG_ERROR)) {
        logLevel = LOG_ERROR;
    }
    levelString = logLevelNames [logLevel];

    // Map the log level to the corresponding text.
    if ((logLevel < LOG_VERBOSE) || (logLevel > LOG_ERROR)) {
        logLevel = LOG_ERROR;
    }

    // Add message debug prefix.
    if ((GMOS_CONFIG_LOG_FILE_LOCATIONS) && (fileName != NULL)) {
        writeSize = snprintf (writeBuffer, GMOS_CONFIG_LOG_MESSAGE_SIZE,
            "[%s:%ld] \t%s : ", fileName, (long) lineNo, levelString);
    } else {
        writeSize = snprintf (writeBuffer, GMOS_CONFIG_LOG_MESSAGE_SIZE,
            "%s : ", levelString);
    }

    // Append the formatted message.
    va_start (args, msgPtr);
    if (writeSize < GMOS_CONFIG_LOG_MESSAGE_SIZE) {
        char* writePtr = writeBuffer + writeSize;
        size_t writeBufSize = GMOS_CONFIG_LOG_MESSAGE_SIZE - writeSize;
        writeSize += vsnprintf (writePtr, writeBufSize, msgPtr, args);
    }
    if (writeSize > GMOS_CONFIG_LOG_MESSAGE_SIZE) {
        writeSize = GMOS_CONFIG_LOG_MESSAGE_SIZE;
    }
    va_end (args);

    // Append the line feed sequence.
    if (GMOS_CONFIG_LOG_MESSAGE_CRLF) {
        writeBuffer [writeSize++] = '\r';
    }
    writeBuffer [writeSize++] = '\n';
    writeBuffer [writeSize] = '\0';

    // Attempt to write the debug message to the console.
    GMOS_CONFIG_HARMONY_DEBUG_CONSOLE_WRITE (writeBuffer, writeSize);
}

/*
 * Provides platform level handling of assert conditions.
 */
void gmosPalAssertFail (const char* fileName, uint32_t lineNo,
    const char* message)
{
    // Not currently implemented.
    while (true) {};
}

/*
 * Accesses the configured system level timer.
 */
uint32_t gmosPalGetTimer (void) {
    return GMOS_CONFIG_HARMONY_SYSTEM_TIMER_READ ();
}

/*
 * Requests that the platform abstraction layer enter idle state for
 * the specified number of system timer ticks.
 */
void gmosPalIdle (uint32_t duration)
{
}

/*
 * Requests that the platform abstraction layer wake the scheduler from
 * its idle state.
 */
void gmosPalWake (void)
{
}
