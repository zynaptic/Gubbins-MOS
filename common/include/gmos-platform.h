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
 * This header file defines the common API for the GubbinsMOS platform
 * abstraction layer. Each target platform must provide a complete
 * implementation of all the functions defined here.
 */

#ifndef GMOS_PLATFORM_H
#define GMOS_PLATFORM_H

#include <stdint.h>
#include <stddef.h>
#include "gmos-config.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * Defines the supported log levels for the debug console logging
 * capability, ordered by increasing level of severity.
 */
typedef enum {
    LOG_VERBOSE = 0x00,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR,
    LOG_FAILURE,
    LOG_UNUSED = 0xFF
} gmosPalLogLevel_t;

/**
 * Defines the supported assertion levels for run time assertion
 * checking, ordered by increasing level of severity.
 */
typedef enum {
    ASSERT_PEDANTIC = 0x00,
    ASSERT_DEBUG,
    ASSERT_TESTING,
    ASSERT_CONFORMANCE,
    ASSERT_ERROR,
    ASSERT_FAILURE,
    ASSERT_UNUSED = 0xFF
} gmosPalAssertLevel_t;

/**
 * This is a macro that may be used to wrap message strings for
 * efficient storage on the target platform. The default option uses
 * standard 'C' strings. An alternative definition may be provided in
 * the platform configuration header if required.
 */
#ifndef GMOS_PLATFORM_STRING_WRAPPER
#define GMOS_PLATFORM_STRING_WRAPPER(_message_) _message_
#endif

/**
 * Initialises the platform abstraction layer on startup. This is called
 * automatically during system initialisation.
 */
void gmosPalInit (void);

/**
 * Initialises the application code on startup. This must be implemented
 * by application specific code to set up the application tasks. It is
 * called immediately prior to starting the main scheduler loop.
 */
void gmosAppInit (void);

/**
 * Converts the specified number of milliseconds to the closest number
 * of system timer ticks (rounding down).
 * @param _ms_ This is the number of milliseconds that are to be
 *     converted to system timer ticks.
 * @return Returns a 32-bit unsigned integer which holds the number of
 *     system timer ticks that correspond to the specified number of
 *     milliseconds.
 */
#define GMOS_MS_TO_TICKS(_ms_) ((uint32_t) \
    ((((uint64_t) _ms_) * GMOS_CONFIG_SYSTEM_TIMER_FREQUENCY) / 1000))

/**
 * Converts the specified number of system timer ticks to the closest
 * number of milliseconds (rounding down).
 * @param _ticks_ This is the number of system timer ticks that are to
 *     be converted to milliseconds.
 * @return Returns a 32-bit unsigned integer which holds the number of
 *     milliseconds that correspond to the specified number of system
 *     timer ticks.
 */
#define GMOS_TICKS_TO_MS(_ticks_) ((uint32_t) \
    ((((uint64_t) _ticks_) * 1000) / GMOS_CONFIG_SYSTEM_TIMER_FREQUENCY))

/**
 * Reads the contents of the system timer. This is a 32-bit timer that
 * increments at a rate defined by 'GMOS_CONFIG_SYSTEM_TIMER_FREQUENCY'.
 * Correct behaviour of this timer is only guaranteed for the task
 * execution context. The timer should wrap no more than once every 48
 * days.
 * @return Returns the current value of the system timer.
 */
uint32_t gmosPalGetTimer (void);

/**
 * Requests that the platform abstraction layer enter idle mode for
 * the specified number of platform timer ticks. Depending on
 * implementation, this may result in the underlying hardware entering
 * a low power sleep mode for the specified duration.
 * @param duration This is the period for which the underlying platform
 *     should enter idle mode, specified as an integer number of
 *     system timer ticks.
 */
void gmosPalIdle (uint32_t duration);

/**
 * Requests that the platform abstraction layer terminate all further
 * processing. The behaviour will be platform specific, but this
 * function is not expected to return.
 * @param status This is a status value which may be used to indicate
 *     the reason for terminating all further processing.
 */
void gmosPalExit (uint8_t status);

/**
 * Claims the main platform mutex lock. This is a recursive counting
 * mutex, so may be called multiple times from the same execution
 * context. For most microcontroller platforms this will map to a
 * global interrupt disable request.
 */
void gmosPalMutexLock (void);

/**
 * Releases the main platform mutex lock. This is a recursive counting
 * mutex, so the unlock request must be called once for each prior lock
 * request. For most microcontroller platforms this will map to a
 * global interrupt enable request.
 */
void gmosPalMutexUnlock (void);

/**
 * Provides a fixed string logging macro that is used to support debug
 * logs for GubbinsMOS applications. The required logging level can be
 * set using the 'GMOS_CONFIG_LOG_LEVEL' parameter in the GubbinsMOS
 * configuration header.
 * @param _level_ This is the log level for the associated log
 *     message. It should be one of the values specified by the
 *     'gmosPalLogLevel_t' enumeration, excluding 'LOG_UNUSED'.
 * @param _message_ This is the fixed string log message.
 */
#define GMOS_LOG(_level_, _message_) {                                 \
    if (_level_ >= GMOS_CONFIG_LOG_LEVEL) {                            \
        const char* msgPtr = GMOS_PLATFORM_STRING_WRAPPER (_message_); \
        if (GMOS_CONFIG_LOG_FILE_LOCATIONS) {                          \
            gmosPalLog (__FILE__, __LINE__, _level_, msgPtr);          \
        } else {                                                       \
            gmosPalLog (NULL, 0, _level_, msgPtr);                     \
        }                                                              \
    }                                                                  \
}

/**
 * Provides a formatted string logging macro that is used to support
 * debug logs for GubbinsMOS applications. The required logging level
 * can be set using the 'GMOS_CONFIG_LOG_LEVEL' parameter in the
 * GubbinsMOS configuration header.
 * @param _level_ This is the log level for the associated log
 *     message. It should be one of the values specified by the
 *     'gmosPalLogLevel_t' enumeration, excluding 'LOG_UNUSED'.
 * @param _message_ This is the formatted string log message. The log
 *     message format and any additional parameters conform to a
 *     platform specific subset of the standard C 'printf' conventions.
 *     In general this subset will include all formatting options that
 *     do not depend on floating point support.
 * @param ... This is an arbitrary number of message format parameters.
 */
#define GMOS_LOG_FMT(_level_, _message_, ...) {                        \
    if (_level_ >= GMOS_CONFIG_LOG_LEVEL) {                            \
        const char* msgPtr = GMOS_PLATFORM_STRING_WRAPPER (_message_); \
        if (GMOS_CONFIG_LOG_FILE_LOCATIONS) {                          \
            gmosPalLogFmt (__FILE__, __LINE__,                         \
                _level_, msgPtr, __VA_ARGS__);                         \
        } else {                                                       \
            gmosPalLogFmt (NULL, 0,                                    \
                _level_, msgPtr, __VA_ARGS__);                         \
        }                                                              \
    }                                                                  \
}

/**
 * Provides platform level handling of fixed string log messages. This
 * function should always be invoked using the GMOS_LOG macro.
 * @param fileName This is the name of the source file in which the
 *     log message occurred. A null reference indicates that source file
 *     information is not to be included in the log message.
 * @param lineNo This is the line number in the source file at which
 *     the log message occurred.
 * @param logLevel This is the log level for the associated log
 *     message. It should be one of the values specified by the
 *     'gmosPalLogLevel_t' enumeration, excluding 'LOG_UNUSED'.
 * @param msgPtr This is a pointer to the fixed string log message.
 */
void gmosPalLog (const char* fileName, uint32_t lineNo,
    gmosPalLogLevel_t logLevel, const char* msgPtr);

/**
 * Provides platform level handling of formatted string log messages.
 * This function should always be invoked using the GMOS_LOG_FMT macro.
 * @param fileName This is the name of the source file in which the
 *     log message occurred. A null reference indicates that source file
 *     information is not to be included in the log message.
 * @param lineNo This is the line number in the source file at which
 *     the log message occurred.
 * @param logLevel This is the log level for the associated log
 *     message. It should be one of the values specified by the
 *     'gmosPalLogLevel_t' enumeration, excluding 'LOG_UNUSED'.
 * @param msgPtr This is a pointer to the the log message, followed by
 *     an arbitrary number of message format parameters. The log message
 *     format and any additional parameters conform to a platform
 *     specific subset of the standard C 'printf' conventions. In
 *     general this subset will include all formatting options that do
 *     not depend on floating point support.
 */
void gmosPalLogFmt (const char* fileName, uint32_t lineNo,
    gmosPalLogLevel_t logLevel, const char* msgPtr, ...);

/**
 * Provides a conditional assert macro that is used to indicate various
 * runtime error conditions.
 * @param _assertLevel_ This is the assertion level for the associated
 *     assertion test. It should be one of the values specified by the
 *     'gmosPalAssertLevel_t' enumeration, excluding 'ASSERT_UNUSED'.
 * @param _condition_ This is a condition expression that for correct
 *     operation should evaluate to 'true'.
 * @param _message_ This is the error message that is associated with
 *     the assert condition.
 */
#define GMOS_ASSERT(_assertLevel_, _condition_, _message_) {           \
    if (_assertLevel_ >= GMOS_CONFIG_ASSERT_LEVEL) {                   \
        const char* msgPtr = GMOS_PLATFORM_STRING_WRAPPER (_message_); \
        if (!(_condition_)) {                                          \
            if (GMOS_CONFIG_LOG_FILE_LOCATIONS) {                      \
                gmosPalAssertFail (__FILE__, __LINE__, msgPtr);        \
            } else {                                                   \
                gmosPalAssertFail (NULL, 0, msgPtr);                   \
            }                                                          \
        }                                                              \
    }                                                                  \
}

/**
 * Provides an unconditional assert macro that is used to indicate fatal
 * runtime error conditions.
 * @param _message_ This is the error message that is associated with
 *     the assert condition.
 */
#define GMOS_ASSERT_FAIL(_message_) {                                  \
    const char* msgPtr = GMOS_PLATFORM_STRING_WRAPPER (_message_);     \
    if (GMOS_CONFIG_LOG_FILE_LOCATIONS) {                              \
        gmosPalAssertFail (__FILE__, __LINE__, msgPtr);                \
    } else {                                                           \
        gmosPalAssertFail (NULL, 0, msgPtr);                           \
    }                                                                  \
}

/**
 * Provides platform level handling of assert conditions. Assert
 * conditions indicate fatal runtime error conditions and depending
 * on implementation the associated assert message should be logged
 * and the hardware reset. This function should always be invoked using
 * the 'GMOS_ASSERT' or 'GMOS_ASSERT_FAIL' macro.
 * @param fileName This is the name of the source file in which the
 *     assert condition occurred.
 * @param lineNo This is the line number in the source file at which
 *     the assert condition occurred.
 * @param message This is the error message that is associated with the
 *     assert condition.
 */
void gmosPalAssertFail (const char* fileName, uint32_t lineNo,
    const char* message);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // GMOS_PLATFORM_H
