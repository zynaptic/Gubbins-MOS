/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2022-2023 Zynaptic Limited
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
 * layer for the Raspberry Pi Pico RP2040 series of devices.
 */

#include <stdint.h>
#include <stdarg.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "pico-device.h"
#include "pico-driver-gpio.h"
#include "pico/critical_section.h"
#include "pico/printf.h"
#include "hardware/timer.h"

// Provide mapping of log levels to human readable strings.
static const char* logLevelNames [] = {
    "VERBOSE", "DEBUG  ", "INFO   ", "WARNING", "ERROR  ", "FAILURE" };

// Implement the platform mutex lock counter.
static critical_section_t mutexLockData;
static uint32_t mutexLockCount = 0;

/*
 * Initialises the platform abstraction layer on startup.
 */
void gmosPalInit (void)
{
    // Initialise the critical section lock used for the platform mutex.
    critical_section_init (&mutexLockData);

    // Initialise the serial debug console if required.
    if (GMOS_CONFIG_LOG_LEVEL < LOG_UNUSED) {
        gmosPalSerialConsoleInit ();
    }

    // Initialise the GPIO platform abstraction layer.
    gmosPalGpioInit ();
}

/*
 * Claims the main platform mutex lock.
 */
void gmosPalMutexLock (void)
{
    // Ensure interrupts are disabled before modifying the lock count.
    critical_section_enter_blocking (&mutexLockData);
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
        critical_section_exit (&mutexLockData);
    }
}

/*
 * Provides platform level handling of formatted string log messages
 * after argument list mapping.
 */
static void gmosPalLogFmtArgs (const char* fileName, uint32_t lineNo,
    gmosPalLogLevel_t logLevel, const char* msgPtr, va_list args)
{
    char writeBuffer [GMOS_CONFIG_LOG_MESSAGE_SIZE + 2];
    size_t writeSize;
    const char* levelString;

    // Map the log level to the corresponding text.
    if ((logLevel < LOG_VERBOSE) || (logLevel > LOG_ERROR)) {
        logLevel = LOG_ERROR;
    }
    levelString = logLevelNames [logLevel];

    // Add message debug prefix.
    if (GMOS_CONFIG_PICO_DEBUG_CONSOLE_INCLUDE_UPTIME) {
        uint64_t uptime = time_us_64 ();
        if (fileName != NULL) {
            writeSize = snprintf (writeBuffer, GMOS_CONFIG_LOG_MESSAGE_SIZE,
                "@%lld \t[%s:%ld] \t%s : ", uptime, fileName, lineNo, levelString);
        } else {
            writeSize = snprintf (writeBuffer, GMOS_CONFIG_LOG_MESSAGE_SIZE,
                "@%lld \t%s : ", uptime, levelString);
        }
    } else {
        if (fileName != NULL) {
            writeSize = snprintf (writeBuffer, GMOS_CONFIG_LOG_MESSAGE_SIZE,
                "[%s:%ld] \t%s : ", fileName, lineNo, levelString);
        } else {
            writeSize = snprintf (writeBuffer, GMOS_CONFIG_LOG_MESSAGE_SIZE,
                "%s : ", levelString);
        }
    }

    // Append the formatted message.
    if (writeSize < GMOS_CONFIG_LOG_MESSAGE_SIZE) {
        char* writePtr = writeBuffer + writeSize;
        size_t writeBufSize = GMOS_CONFIG_LOG_MESSAGE_SIZE - writeSize;
        writeSize += vsnprintf (writePtr, writeBufSize, msgPtr, args);
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
 * Provides platform level handling of formatted string log messages.
 */
void gmosPalLogFmt (const char* fileName, uint32_t lineNo,
    gmosPalLogLevel_t logLevel, const char* msgPtr, ...)
{
    va_list args;
    va_start (args, msgPtr);
    gmosPalLogFmtArgs (fileName, lineNo, logLevel, msgPtr, args);
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

/*
 * Log printf requests as info messages.
 */
int printf (const char* msgPtr, ...)
{
    va_list args;
    va_start (args, msgPtr);
    gmosPalLogFmtArgs (NULL, 0, LOG_INFO, msgPtr, args);
    va_end (args);
    return 0;
}

/*
 * Log vprintf requests as info messages.
 */
int vprintf (const char* msgPtr, va_list args)
{
    gmosPalLogFmtArgs (NULL, 0, LOG_INFO, msgPtr, args);
    return 0;
}

/*
 * Log puts requests as info messages.
 */
int puts (const char* msgPtr)
{
    gmosPalLog (NULL, 0, LOG_INFO, msgPtr);
    return 0;
}
