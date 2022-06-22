/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2020-2022 Zynaptic Limited
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

// Use the Harmony cryptographic random number source if selected.
#if (GMOS_CONFIG_RANDOM_SOUCE == GMOS_RANDOM_SOURCE_PLATFORM_SPECIFIC)
#include "system/random/sys_random.h"
#endif // GMOS_RANDOM_SOURCE_PLATFORM_SPECIFIC

// Provide mapping of log levels to human readable strings.
static const char* logLevelNames [] = {
    "GMOS-VERBOSE", "GMOS-DEBUG  ", "GMOS-INFO   ",
    "GMOS-WARNING", "GMOS-ERROR  ", "GMOS-FAILURE" };

// Specify the platform mutex lock state variables.
static uint32_t palMutexLockCount = 0;
static OSAL_CRITSECT_DATA_TYPE palMutexLockState;

// Specify host operating system mutex lock state variables.
#if GMOS_CONFIG_HOST_OS_SUPPORT
static OSAL_MUTEX_DECLARE (hostOsMutexLockState);
#endif

// Specify operating system specific variables for the latest version
// of FreeRTOS
#if GMOS_CONFIG_HOST_OS_SUPPORT && (OSAL_USE_RTOS == 9)
#include "FreeRTOS.h"
#include "task.h"
static TaskHandle_t hostOsTaskHandle = NULL;
#endif

/*
 * Initialises the platform abstraction layer on startup.
 */
void gmosPalInit (void)
{
    // Initialise the host operating system mutex if required.
#if GMOS_CONFIG_HOST_OS_SUPPORT
    OSAL_MUTEX_Create (&hostOsMutexLockState);
#endif
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
    if (palMutexLockCount == 0) {
        palMutexLockState = OSAL_CRIT_Enter (OSAL_CRIT_TYPE_HIGH);
    }
    palMutexLockCount += 1;
}

/*
 * Releases the main platform mutex lock.
 */
void gmosPalMutexUnlock (void)
{
    // Decrement the lock count and enable interrupts if required.
    palMutexLockCount -= 1;
    if (palMutexLockCount == 0) {
        OSAL_CRIT_Leave (OSAL_CRIT_TYPE_HIGH, palMutexLockState);
    }
}

/*
 * Claims the host operating system mutex lock. This is only used for
 * configurations where the GubbinsMOS platform is implemented within a
 * single thread of a multithreaded host operating system, such as a
 * conventional RTOS or a UNIX based emulation environment.
 */
#if GMOS_CONFIG_HOST_OS_SUPPORT
bool gmosPalHostOsMutexLock (uint16_t timeout)
{
    OSAL_RESULT osalResult;
    if (timeout == 0xFFFF) {
        timeout = OSAL_WAIT_FOREVER;
    }
    osalResult = OSAL_MUTEX_Lock (&hostOsMutexLockState, timeout);
    return (osalResult == OSAL_RESULT_TRUE) ? true : false;
}
#endif

/*
 * Releases the host operating system mutex lock.
 */
#if GMOS_CONFIG_HOST_OS_SUPPORT
void gmosPalHostOsMutexUnlock (void)
{
    OSAL_MUTEX_Unlock (&hostOsMutexLockState);
}
#endif

/*
 * Provides a platform specific method of adding entropy to the random
 * number generator.
 */
#if (GMOS_CONFIG_RANDOM_SOUCE == GMOS_RANDOM_SOURCE_PLATFORM_SPECIFIC)
void gmosPalAddRandomEntropy (uint32_t randomEntropy)
{
    uint8_t entropyByte = (uint8_t) randomEntropy;
    entropyByte ^= (uint8_t) (randomEntropy >> 8);
    entropyByte ^= (uint8_t) (randomEntropy >> 16);
    entropyByte ^= (uint8_t) (randomEntropy >> 24);
    SYS_RANDOM_CryptoEntropyAdd (entropyByte);
}
#endif // GMOS_RANDOM_SOURCE_PLATFORM_SPECIFIC

/*
 * Provides a platform specific random number generator.
 */
#if (GMOS_CONFIG_RANDOM_SOUCE == GMOS_RANDOM_SOURCE_PLATFORM_SPECIFIC)
void gmosPalGetRandomBytes (uint8_t* byteArray, size_t byteArraySize)
{
    SYS_RANDOM_CryptoBlockGet (byteArray, byteArraySize);
}
#endif // GMOS_RANDOM_SOURCE_PLATFORM_SPECIFIC

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
 * the specified number of system timer ticks. The GubbinsMOS native
 * version of this call just implements busy waiting.
 */
#if !GMOS_CONFIG_HOST_OS_SUPPORT
void gmosPalIdle (uint32_t duration)
{
}
#endif

/*
 * Requests that the platform abstraction layer wake the scheduler from
 * its idle state. The GubbinsMOS native version of this call has no
 * effect.
 */
#if !GMOS_CONFIG_HOST_OS_SUPPORT
void gmosPalWake (void)
{
}
#endif

/*
 * Requests that the platform abstraction layer enter idle state for
 * the specified number of system timer ticks. The FreeRTOS version of
 * this call uses the FreeRTOS timed task delay function.
 */
#if GMOS_CONFIG_HOST_OS_SUPPORT && (OSAL_USE_RTOS == 9)
void gmosPalIdle (uint32_t duration)
{
    TickType_t hostOsTicks;

    // This function should only ever be called by the GubbinsMOS
    // thread. It uses lazy initialisation to set the task handle on
    // the first call from the GubbinsMOS scheduler loop.
    if (hostOsTaskHandle == NULL) {
        hostOsTaskHandle = xTaskGetCurrentTaskHandle ();
    }

    // If the GubbinsMOS scheduler and FreeRTOS scheduler are using the
    // same system timer, the time base conversion should optimise out.
    if (GMOS_CONFIG_SYSTEM_TIMER_FREQUENCY == configTICK_RATE_HZ) {
        hostOsTicks = duration;
    } else {
        hostOsTicks = pdMS_TO_TICKS (GMOS_TICKS_TO_MS (duration));
    }
    vTaskDelay (hostOsTicks);
}
#endif

/*
 * Requests that the platform abstraction layer wake the scheduler from
 * its idle state. The FreeRTOS version of this call uses the delay
 * cancellation function.
 */
#if GMOS_CONFIG_HOST_OS_SUPPORT && (OSAL_USE_RTOS == 9)
void gmosPalWake (void)
{
    if (hostOsTaskHandle != NULL) {
        xTaskAbortDelay (hostOsTaskHandle);
    }
}
#endif
