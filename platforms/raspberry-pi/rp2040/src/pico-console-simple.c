/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2022 Zynaptic Limited
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
 * Implements GubbinsMOS debug serial console support using simple
 * single byte transfers with the Raspberry Pi SDK wrapper.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-scheduler.h"
#include "gmos-streams.h"
#include "gmos-driver-gpio.h"
#include "pico-driver-gpio.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"

// Statically allocate the task and stream state structures.
static gmosTaskState_t consoleTask;
static gmosStream_t consoleStream;

/*
 * This function implements the RP2040 serial debug task handler.
 */
static inline gmosTaskStatus_t gmosPalSerialConsoleTaskHandler (void* nullData)
{
    uint8_t txByte;
    uart_inst_t* debugUart;

    // Select the UART instance.
    if (GMOS_CONFIG_PICO_DEBUG_CONSOLE_UART_ID == 0) {
        debugUart = uart0;
    } else if (GMOS_CONFIG_PICO_DEBUG_CONSOLE_UART_ID == 1) {
        debugUart = uart1;
    } else {
        return GMOS_TASK_SUSPEND;
    }

    // Write as much data as will fit into the UART buffers.
    while (uart_is_writable (debugUart)) {
        if (gmosStreamReadByte (&consoleStream, &txByte)) {
            uart_putc_raw (debugUart, txByte);
        } else {
            return GMOS_TASK_SUSPEND;
        }
    }

    // Wait for the UART buffer to clear some space.
    return GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (5));
}

// Define the console task.
GMOS_TASK_DEFINITION (gmosPalSerialConsoleTask,
    gmosPalSerialConsoleTaskHandler, void);

/*
 * Initialises the RP2040 serial debug console.
 */
void gmosPalSerialConsoleInit (void)
{
    uart_inst_t* debugUart;

    // Select the UART instance.
    if (GMOS_CONFIG_PICO_DEBUG_CONSOLE_UART_ID == 0) {
        debugUart = uart0;
    } else if (GMOS_CONFIG_PICO_DEBUG_CONSOLE_UART_ID == 1) {
        debugUart = uart1;
    } else {
        return;
    }

    // Initialise the task and stream state.
    gmosStreamInit (&consoleStream,
        &consoleTask, GMOS_CONFIG_PICO_DEBUG_CONSOLE_BUFFER_SIZE);
    gmosPalSerialConsoleTask_start (&consoleTask, NULL, "Debug Console");

    // Configure the selected pin for UART transmit.
    gmosDriverGpioAltModeInit (
        GMOS_CONFIG_PICO_DEBUG_CONSOLE_UART_TX_PIN,
        PICO_GPIO_DRIVER_SLEW_FAST_4MA,
        GMOS_DRIVER_GPIO_INPUT_PULL_NONE, GPIO_FUNC_UART);

    // Initialise the UART.
    uart_init (debugUart, GMOS_CONFIG_PICO_DEBUG_CONSOLE_BAUD_RATE);
}

/*
 * Attempts to write the contents of the supplied data buffer to the
 * RP2040 serial debug console.
 */
bool gmosPalSerialConsoleWrite (uint8_t* writeData, uint16_t writeSize)
{
    return gmosStreamWriteAll (&consoleStream, writeData, writeSize);
}
