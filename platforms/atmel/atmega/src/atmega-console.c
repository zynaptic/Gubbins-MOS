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
 * Implements GubbinsMOS debug serial console support using ATMEGA
 * USART.
 */

#include <stdint.h>
#include <stddef.h>

#include "gmos-platform.h"
#include "gmos-scheduler.h"
#include "gmos-streams.h"
#include "atmega-device.h"

// Statically allocate the task and stream state structures.
static gmosTaskState_t consoleTask;
static gmosStream_t consoleStream;

/*
 * This function implements the ATMEGA serial debug task handler.
 */
static inline gmosTaskStatus_t gmosPalSerialConsoleTaskHandler (void* nullData)
{
    uint8_t txByte;

    // Poll the serial port for ready to transmit.
    if ((ATMEGA_CONSOLE_UCSRA_REG & (1 << ATMEGA_CONSOLE_UDRE_BIT)) == 0) {
        return GMOS_TASK_RUN_IMMEDIATE;
    }

    // Attempt to read a byte from the console stream.
    if (gmosStreamReadByte (&consoleStream, &txByte)) {
        ATMEGA_CONSOLE_UDR_REG = txByte;
        return GMOS_TASK_RUN_IMMEDIATE;
    } else {
        return GMOS_TASK_SUSPEND;
    }
}

// Define the console task.
GMOS_TASK_DEFINITION (gmosPalSerialConsoleTask,
    gmosPalSerialConsoleTaskHandler, void);

/*
 * Initialises the ATMEGA serial debug console.
 */
void gmosPalSerialConsoleInit (void)
{
    uint16_t brrValue;

    // Initialise the task and stream state.
    gmosStreamInit (&consoleStream,
        &consoleTask, GMOS_CONFIG_ATMEGA_DEBUG_CONSOLE_BUFFER_SIZE);
    gmosPalSerialConsoleTask_start (&consoleTask, NULL, "Debug Console");

    // Set the USART Baud rate and 8N1 format (the default setting).
    brrValue = (GMOS_CONFIG_ATMEGA_SYSTEM_CLOCK /
        (16 * GMOS_CONFIG_ATMEGA_DEBUG_CONSOLE_BAUD_RATE)) - 1;
    ATMEGA_CONSOLE_UBRRL_REG = (uint8_t) brrValue;
    ATMEGA_CONSOLE_UBRRH_REG = (uint8_t) (brrValue >> 8);

    // Enable the USART transmitter. This automatically enables the
    // alternative function for PD1 GPIO pin.
    ATMEGA_CONSOLE_UCSRB_REG = (1 << ATMEGA_CONSOLE_TXEN_BIT);
}

/*
 * Attempts to write the contents of the supplied data buffer to the
 * STM32 serial debug console.
 */
bool gmosPalSerialConsoleWrite (uint8_t* writeData, uint16_t writeSize)
{
    return gmosStreamWriteAll (&consoleStream, writeData, writeSize);
}
