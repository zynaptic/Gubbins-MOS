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
 * Implements GubbinsMOS debug serial console support using USART2.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-scheduler.h"
#include "gmos-streams.h"
#include "stm32-device.h"

// Use this implementation if the basic serial console is selected.
#if !GMOS_CONFIG_STM32_DEBUG_CONSOLE_USE_DMA

// Statically allocate the task and stream state structures.
static gmosTaskState_t consoleTask;
static gmosStream_t consoleStream;

/*
 * This function implements the STM32 serial debug task handler.
 */
static inline gmosTaskStatus_t gmosPalSerialConsoleTaskHandler (void* nullData)
{
    uint8_t txByte;

    // Poll the serial port for completion.
    if ((USART2->ISR & USART_ISR_TXE) == 0) {
        return GMOS_TASK_RUN_IMMEDIATE;
    }

    // Attempt to read a byte from the console stream.
    if (gmosStreamReadByte (&consoleStream, &txByte)) {
        USART2->TDR = txByte;
        return GMOS_TASK_RUN_IMMEDIATE;
    } else {
        return GMOS_TASK_SUSPEND;
    }
}

// Define the console task.
GMOS_TASK_DEFINITION (gmosPalSerialConsoleTask,
    gmosPalSerialConsoleTaskHandler, void);

/*
 * Initialises the STM32 serial debug console.
 */
void gmosPalSerialConsoleInit (void)
{
    uint16_t usartDiv;
    bool disableGpioClock = false;

    // Initialise the task and stream state.
    gmosStreamInit (&consoleStream,
        &consoleTask, GMOS_CONFIG_STM32_DEBUG_CONSOLE_BUFFER_SIZE);
    gmosPalSerialConsoleTask_start (&consoleTask, NULL, "Debug Console");

    // Temporarily enable the GPIO clock if required.
    if ((RCC->IOPENR & RCC_IOPENR_GPIOAEN) == 0) {
        RCC->IOPENR |= RCC_IOPENR_GPIOAEN;
        disableGpioClock = true;
    }

    // Configure GPIO A2 pin for USART2 transmit (high speed push/pull).
    gmosPalGpioSetAltFunction (STM32_GPIO_BANK_A, 2,
        STM32_GPIO_DRIVER_PUSH_PULL, STM32_GPIO_DRIVER_SLEW_FAST,
        STM32_GPIO_INPUT_PULL_NONE, 4);

    // Disable the GPIO clock if no longer required.
    if (disableGpioClock) {
        RCC->IOPENR &= ~RCC_IOPENR_GPIOAEN;
    }

    // Enable clock for USART2. Note that this is not enabled in the
    // corresponding sleep mode register, so it will automatically be
    // gated on entering sleep mode.
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    // Set the USART2 baud rate (8N1 format is selected by default).
    usartDiv = GMOS_CONFIG_STM32_APB1_CLOCK /
        GMOS_CONFIG_STM32_DEBUG_CONSOLE_BAUD_RATE;
    USART2->BRR = usartDiv;

    // Enable USART2 in transmit only mode.
    USART2->CR1 = USART_CR1_UE | USART_CR1_TE;
}

/*
 * Attempts to write the contents of the supplied data buffer to the
 * STM32 serial debug console.
 */
bool gmosPalSerialConsoleWrite (uint8_t* writeData, uint16_t writeSize)
{
    return gmosStreamWriteAll (&consoleStream, writeData, writeSize);
}

#endif // !GMOS_CONFIG_STM32_DEBUG_CONSOLE_USE_DMA
