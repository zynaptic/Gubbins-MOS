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
 * This header file specifies the GubbinsMOS compile time configuration
 * options. It sets the default configuration options which may be
 * overridden by the 'gmos-pal-config.h' platform configuration header
 * and the 'gmos-app.config.h' application specific header.
 */

#ifndef GMOS_CONFIG_H
#define GMOS_CONFIG_H

#include <stdbool.h>

#include "gmos-app-config.h"
#include "gmos-pal-config.h"

/**
 * This configuration option specifies the default system timer tick
 * frequency. The default frequency corresponds to that of a 32.768 kHz
 * watch crystal divided down to 1.024 kHz, which causes the 32-bit
 * timer counter to wrap approximately every 48 days. For portability,
 * any platform specific settings should not exceed this value, which
 * means that scheduler intervals of up to 24 days can safely be used
 * across all platforms.
 */
#ifndef GMOS_CONFIG_SYSTEM_TIMER_FREQUENCY
#define GMOS_CONFIG_SYSTEM_TIMER_FREQUENCY 1024
#endif

/**
 * This configuration option specifies the default background task
 * interval. This is the number of system timer ticks that will be
 * inserted between task function calls as a result of returning the
 * 'GMOS_TASK_RUN_BACKGROUND' status value.
 */
#ifndef GMOS_CONFIG_BACKGROUND_TASK_INTERVAL
#define GMOS_CONFIG_BACKGROUND_TASK_INTERVAL 10
#endif

/**
 * This configuration option specifies whether the memory pool should
 * use the heap for data storage. This will only be possible if the
 * 'GMOS_MALLOC' and 'GMOS_FREE' macros are supported by the target
 * platform.
 */
#ifndef GMOS_CONFIG_MEMPOOL_USE_HEAP
#define GMOS_CONFIG_MEMPOOL_USE_HEAP false
#endif

/**
 * This configuration option specifies the size of individual memory
 * pool segments as an integer number of bytes. This must be an integer
 * multiple of 4.
 */
#ifndef GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE
#define GMOS_CONFIG_MEMPOOL_SEGMENT_SIZE 64
#endif

/**
 * This configuration option specifies the number of memory pool
 * segments to be allocated.
 */
#ifndef GMOS_CONFIG_MEMPOOL_SEGMENT_NUMBER
#define GMOS_CONFIG_MEMPOOL_SEGMENT_NUMBER 64
#endif

/**
 * This configuration option is used to select memcpy as the method for
 * transferring data to and from the stream buffers. By default an
 * inlined byte based copy is used, since buffer transfers are expected
 * to be unaligned and relatively short.
 */
#ifndef GMOS_CONFIG_STREAMS_USE_MEMCPY
#define GMOS_CONFIG_STREAMS_USE_MEMCPY false
#endif

/**
 * This configuration option is used to select memcpy as the method for
 * transferring data to and from data buffers. By default an inlined
 * byte based copy is used, since buffer transfers are expected to be
 * unaligned and relatively short.
 */
#ifndef GMOS_CONFIG_BUFFERS_USE_MEMCPY
#define GMOS_CONFIG_BUFFERS_USE_MEMCPY false
#endif

/**
 * This configuration option is used to select the random number source
 * to be used. The default setting is the simplest XOR shift option.
 */
#ifndef GMOS_CONFIG_RANDOM_SOUCE
#define GMOS_CONFIG_RANDOM_SOUCE GMOS_RANDOM_SOURCE_XOSHIRO128PP
#endif

// Specify the supported options for the random number source.
#define GMOS_RANDOM_SOURCE_PLATFORM_SPECIFIC 0
#define GMOS_RANDOM_SOURCE_XOSHIRO128PP      1

/**
 * This configuration option selects whether file name and location
 * information is to be included when generating log messages. Set to
 * 'true' to enable file name and location logging for debug purposes.
 */
#ifndef GMOS_CONFIG_LOG_FILE_LOCATIONS
#define GMOS_CONFIG_LOG_FILE_LOCATIONS false
#endif

/**
 * This configuration option selects the minimum log severity level
 * which will be logged during operation. This may be any value from
 * the 'gmosPalLogLevel_t' enumeration. Set to 'LOG_UNUSED' to disable
 * the debug console completely.
 */
#ifndef GMOS_CONFIG_LOG_LEVEL
#define GMOS_CONFIG_LOG_LEVEL LOG_INFO
#endif

/**
 * This configuration option sets the maximum log message size that
 * is supported. Any log messages larger than this will be truncated.
 */
#ifndef GMOS_CONFIG_LOG_MESSAGE_SIZE
#define GMOS_CONFIG_LOG_MESSAGE_SIZE 100
#endif

/**
 * This configuration option specifies the type of log message output
 * line termination. If set to 'true' then '\r\n' line terminations will
 * be used, otherwise '\n' line terminations will be used.
 */
#ifndef GMOS_CONFIG_LOG_MESSAGE_CRLF
#define GMOS_CONFIG_LOG_MESSAGE_CRLF true
#endif

/**
 * This configuration option selects the minimum assertion severity
 * level which will be trapped during operation. This may be any value
 * from the 'gmosPalAssertLevel_t' enumeration. Set to 'ASSERT_UNUSED'
 * to disable assert handling completely.
 */
#ifndef GMOS_CONFIG_ASSERT_LEVEL
#define GMOS_CONFIG_ASSERT_LEVEL ASSERT_FAILURE
#endif

/**
 * This configuration option determines whether task names are included
 * for the various system and driver tasks. These are usually only
 * useful for debugging and can be omitted in production builds to
 * save memory.
 */
#ifndef GMOS_CONFIG_INCLUDE_TASK_NAMES
#define GMOS_CONFIG_INCLUDE_TASK_NAMES false
#endif

/**
 * This configuration option specifies the size of the I2C data buffers
 * that are used for read and write transactions. This places an upper
 * limit on the size of I2C transactions that are supported.
 */
#ifndef GMOS_CONFIG_I2C_BUFFER_SIZE
#define GMOS_CONFIG_I2C_BUFFER_SIZE 32
#endif

/**
 * This configuration option specifies the size of the platform EEPROM
 * tags which are used to identify distinct EEPROM data records in tag,
 * length, value format.
 */
#ifndef GMOS_CONFIG_EEPROM_TAG_SIZE
#define GMOS_CONFIG_EEPROM_TAG_SIZE 1
#endif

/**
 * This configuration option specifies the length field of the platform
 * EEPROM data records in tag, length, value format.
 */
#ifndef GMOS_CONFIG_EEPROM_LENGTH_SIZE
#define GMOS_CONFIG_EEPROM_LENGTH_SIZE 1
#endif

/**
 * This configuration option is used to select the real time clock
 * implementation to be used. This may be either a platform specific
 * hardware peripheral or a software emulation running off the system
 * timer.
 */
#ifndef GMOS_CONFIG_RTC_SOFTWARE_EMULATION
#define GMOS_CONFIG_RTC_SOFTWARE_EMULATION false
#endif

#endif // GMOS_CONFIG_H
