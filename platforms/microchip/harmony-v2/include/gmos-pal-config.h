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
 * Specifies the Microchip Harmony vendor framework default
 * configuration options.
 */

#ifndef GMOS_PAL_CONFIG_H
#define GMOS_PAL_CONFIG_H

/**
 * The typical Microchip Harmony project will have sufficient memory
 * to support the use of heap memory allocation. The amount of memory
 * dedicated to the heap will be set in the Harmony configuration tool.
 * The Harmony OSAL malloc and free functions are used by default.
 */
#ifndef GMOS_MALLOC
#include "osal/osal.h"
#define GMOS_MALLOC(_size_) OSAL_Malloc(_size_)
#define GMOS_FREE(_mem_) OSAL_Free(_mem_)
#endif

/**
 * Enable heap based allocation for the memory pool.
 */
#ifndef GMOS_CONFIG_MEMPOOL_USE_HEAP
#define GMOS_CONFIG_MEMPOOL_USE_HEAP true
#endif

/**
 * Specify the system timer frequency. The default option is to use the
 * host OS tick timer or the configured Harmony system timer frequency.
 */
#ifndef GMOS_CONFIG_SYSTEM_TIMER_FREQUENCY

// Use FreeRTOS tick timer.
#if GMOS_CONFIG_HOST_OS_SUPPORT && (OSAL_USE_RTOS == 9)
#define GMOS_CONFIG_SYSTEM_TIMER_FREQUENCY configTICK_RATE_HZ

// Use Harmony system timer service by default.
#else
#define GMOS_CONFIG_SYSTEM_TIMER_FREQUENCY SYS_TMR_FREQUENCY
#endif
#endif

/**
 * Specify the system timer request function. The default option is to
 * use the host OS tick timer or the current Harmony system timer value.
 */
#ifndef GMOS_CONFIG_HARMONY_SYSTEM_TIMER_READ

// Access 32-bit FreeRTOS tick timer.
#if GMOS_CONFIG_HOST_OS_SUPPORT && (OSAL_USE_RTOS == 9)
#include "FreeRTOS.h"
#include "task.h"
#if (configUSE_16_BIT_TICKS != 0)
#error "32-bit FreeRTOS tick timer required."
#else
#define GMOS_CONFIG_HARMONY_SYSTEM_TIMER_READ() \
    xTaskGetTickCount()
#endif

// Access Harmony system timer service by default.
#else
#include "system_config.h"
#include "system/tmr/sys_tmr.h"
#define GMOS_CONFIG_HARMONY_SYSTEM_TIMER_READ() \
    SYS_TMR_TickCountGet()
#endif
#endif

/**
 * This configuration option is used to select the random number source
 * to be used. The default setting is the configured Harmony platform
 * cryptographic random number generator.
 */
#ifndef GMOS_CONFIG_RANDOM_SOUCE
#define GMOS_CONFIG_RANDOM_SOUCE GMOS_RANDOM_SOURCE_PLATFORM_SPECIFIC
#endif

/**
 * Specify the vendor framework function to be used for writing GMOS
 * debug messages to the console. The default debug console write
 * function uses the standard Harmony system console.
 * @param _msgBuf_ This is a pointer to a null terminated string that
 *     is to be written to the debug console.
 * @param _msgSize_ This is the length of the message to be written to
 *     the debug console, excluding the null terminator.
 */
#ifndef GMOS_CONFIG_HARMONY_DEBUG_CONSOLE_WRITE
#include "system/console/sys_console.h"
#define GMOS_CONFIG_HARMONY_DEBUG_CONSOLE_WRITE(_msgBuf_, _msgSize_) { \
    SYS_CONSOLE_Write (SYS_CONSOLE_INDEX_0, 0, _msgBuf_, _msgSize_);   \
    SYS_CONSOLE_Flush (SYS_CONSOLE_INDEX_0);                           \
}
#endif

/**
 * Specify a bitmask which determines which external interrupt lines
 * are reserved for use by the Harmony framework. Any of the external
 * interrupt lines which are not reserved will be available for use
 * by the GMOS GPIO driver.
 */
#ifndef GMOS_CONFIG_HARMONY_RESERVED_EXTI_MASK
#define GMOS_CONFIG_HARMONY_RESERVED_EXTI_MASK 0
#endif

/**
 * The hardware real time clock is not currently supported for this
 * platform.
 */
#define GMOS_CONFIG_RTC_SOFTWARE_EMULATION true

/**
 * EEPROM data storage is not currently supported for this platform.
 */
#define GMOS_CONFIG_EEPROM_SOFTWARE_EMULATION true

#endif // GMOS_PAL_CONFIG_H
