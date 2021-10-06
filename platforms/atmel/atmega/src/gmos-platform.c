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
 * layer for the Microchip/Atmel ATMEGA series of devices.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <printf.h>

#include "gmos-platform.h"
#include "atmega-device.h"

// Provide mapping of log levels to human readable strings.
static const char* logLevelNames [] = {
    "VERBOSE", "DEBUG  ", "INFO   ", "WARNING", "ERROR  ", "FAILURE" };

// Implement the platform mutex lock counter.
static uint32_t mutexLockCount = 0;

/*
 * Initialises the platform abstraction layer on startup.
 */
void gmosPalInit (void)
{
    // Initialise the serial debug console if required.
    if (GMOS_CONFIG_LOG_LEVEL < LOG_UNUSED) {
        gmosPalSerialConsoleInit ();
    }

    // Initialise the system timer.
    gmosPalSystemTimerInit ();

    // Enable all interrupts.
    sei ();
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
    cli ();
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
        sei ();
    }
}

/*
 * Provides platform level handling of log messages.
 */
void gmosPalLog (const char* fileName, uint32_t lineNo,
    gmosPalLogLevel_t logLevel, const char* message, ...)
{
    char writeBuffer [GMOS_CONFIG_LOG_MESSAGE_SIZE + 2];
    size_t writeSize;
    va_list args;
    const char* levelString;

    // Map the log level to the corresponding text.
    if ((logLevel < LOG_VERBOSE) || (logLevel > LOG_ERROR)) {
        logLevel = LOG_ERROR;
    }
    levelString = logLevelNames [logLevel];

    // Add message debug prefix.
    va_start (args, message);
    if (fileName != NULL) {
        writeSize = snprintf (writeBuffer, GMOS_CONFIG_LOG_MESSAGE_SIZE,
            "[%s:%ld] \t%s : ", fileName, lineNo, levelString);
    } else {
        writeSize = snprintf (writeBuffer, GMOS_CONFIG_LOG_MESSAGE_SIZE,
            "%s : ", levelString);
    }

    // Append the formatted message.
    if (writeSize < GMOS_CONFIG_LOG_MESSAGE_SIZE) {
        char* writePtr = writeBuffer + writeSize;
        size_t writeBufSize = GMOS_CONFIG_LOG_MESSAGE_SIZE - writeSize;
        writeSize += vsnprintf (writePtr, writeBufSize, message, args);
    }
    if (writeSize > GMOS_CONFIG_LOG_MESSAGE_SIZE) {
        writeSize = GMOS_CONFIG_LOG_MESSAGE_SIZE;
    }

    // Append the line feed sequence.
    if (GMOS_CONFIG_LOG_MESSAGE_CRLF) {
        writeBuffer [writeSize++] = '\r';
    }
    writeBuffer [writeSize++] = '\n';

    // Attempt to write the debug message to the console. On failure,
    // attempt to send a 'message lost' indicator instead.
    if (!gmosPalSerialConsoleWrite ((uint8_t*) writeBuffer, writeSize)) {
        if (GMOS_CONFIG_LOG_MESSAGE_CRLF) {
            gmosPalSerialConsoleWrite ((uint8_t*) "...\r\n", 5);
        } else {
            gmosPalSerialConsoleWrite ((uint8_t*) "...\n", 4);
        }
    }
    va_end (args);
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
