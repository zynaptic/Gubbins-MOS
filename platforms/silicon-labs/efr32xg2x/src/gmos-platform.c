/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2023-2024 Zynaptic Limited
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
 * layer for the Silicon Labs EFR32xG2x series of devices.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include <printf.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "efr32-device.h"
#include "efr32-driver-gpio.h"
#include "em_core.h"
#include "em_emu.h"

// Provide mapping of log levels to human readable strings.
static const char* logLevelNames [] = {
    "VERBOSE", "DEBUG  ", "INFO   ", "WARNING", "ERROR  ", "FAILURE" };

// Allocate storage for critical section interrupt state.
static CORE_DECLARE_IRQ_STATE;

/*
 * Initialises the platform abstraction layer on startup.
 */
void gmosPalInit (void)
{
    // Initialise the main system timer.
    gmosPalSystemTimerInit ();

    // Initialise the GPIO support.
    gmosPalGpioInit ();

    // Initialise the serial debug console if required.
    if (GMOS_CONFIG_LOG_LEVEL < LOG_UNUSED) {
        gmosPalSerialConsoleInit ();
    }
}

/*
 * Claims the main platform mutex lock.
 */
void gmosPalMutexLock (void)
{
    CORE_ENTER_CRITICAL ();
}

/*
 * Releases the main platform mutex lock.
 */
void gmosPalMutexUnlock (void)
{
    CORE_EXIT_CRITICAL ();
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
    char writeBuffer [GMOS_CONFIG_LOG_MESSAGE_SIZE + 2];
    size_t writeSize;
    va_list args;
    const char* levelString;

    // Map the log level to the corresponding text.
    if (logLevel > LOG_ERROR) {
        logLevel = LOG_ERROR;
    }
    levelString = logLevelNames [logLevel];

    // Add message debug prefix.
    va_start (args, msgPtr);
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
    va_end (args);
}

/*
 * Provides platform level handling of assert conditions. This logs the
 * assertion and then goes into an infinite loop to push the message
 * out onto the debug console.
 */
void gmosPalAssertFail (const char* fileName, uint32_t lineNo,
    const char* msgPtr)
{
    gmosPalLogFmt (fileName, lineNo, LOG_FAILURE, msgPtr);
    gmosPalSerialConsoleFlushAssertion ();
}

/*
 * If required, allocate memory for the heap.
 */
#if (GMOS_CONFIG_HEAP_SIZE > 0)
#include <stdlib.h>
uint8_t gmosPalHeap [GMOS_CONFIG_HEAP_SIZE] __attribute__ ((section (".heap")));
#endif

/*
 * Implement platform specific heap allocation.
 */
void* gmosPalMalloc (size_t size)
{
#if (GMOS_CONFIG_HEAP_SIZE > 0)
    return malloc (size);
#else
    (void) size;
    GMOS_ASSERT_FAIL ("No Dynamic Memory Support.");
    return NULL;
#endif
}

/*
 * Implement platform specific heap allocation.
 */
void* gmosPalCalloc (size_t num, size_t size)
{
#if (GMOS_CONFIG_HEAP_SIZE > 0)
    return calloc (num, size);
#else
    (void) num;
    (void) size;
    GMOS_ASSERT_FAIL ("No Dynamic Memory Support.");
    return NULL;
#endif
}

/*
 * Implement platform specific heap free.
 */
void gmosPalFree (void* memPtr)
{
#if (GMOS_CONFIG_HEAP_SIZE > 0)
    free (memPtr);
#else
    (void) memPtr;
    GMOS_ASSERT_FAIL ("No Dynamic Memory Support.");
#endif
}

/*
 * Include access to the core temperature sensor here. This reads the
 * core temperature directly without using the SDK driver.
 */
float gmosPalGetCoreTempFloat (void)
{
    return EMU_TemperatureGet ();
}

/*
 * Provide standard library stubs to avoid linker warnings.
 */
int _close (int fd)
{
    (void) fd;
    return -1;
}

int _fstat (int fd, void* st)
{
    (void) fd;
    (void) st;
    return -1;
}

int _isatty (int fd)
{
    (void) fd;
    return -1;
}

int _lseek (int fd, int ptr, int dir)
{
    (void) fd;
    (void) ptr;
    (void) dir;
    return 0;
}

void _exit (int status)
{
    (void) status;
}

void _kill (int pid, int sig)
{
    (void) pid;
    (void) sig;
    return;
}

int _getpid (void)
{
    return -1;
}

int _read (int fd, char *buf, int count)
{
    (void) fd;
    (void) buf;
    (void) count;
    return -1;
}

int _write (int fd, char *buf, int count)
{
    (void) fd;
    (void) buf;
    (void) count;
    return -1;
}
