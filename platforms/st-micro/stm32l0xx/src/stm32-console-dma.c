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
 * Implements GubbinsMOS debug serial console support using USART2 and
 * DMA channel 4.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-scheduler.h"
#include "gmos-streams.h"
#include "gmos-driver-gpio.h"
#include "stm32-device.h"
#include "stm32-driver-gpio.h"

// Use this implementation if the DMA based serial console is selected.
#if GMOS_CONFIG_STM32_DEBUG_CONSOLE_USE_DMA

// Specify the DMA buffer size to be used.
#define SERIAL_CONSOLE_DMA_BUFFER_SIZE 64

// Define the state space for the console task.
#define CONSOLE_STATE_DMA_IDLE  0
#define CONSOLE_STATE_DMA_BUF_A 1
#define CONSOLE_STATE_DMA_BUF_B 2

// Statically allocate the task and stream state structures.
static gmosTaskState_t consoleTask;
static gmosStream_t consoleStream;

// Statically allocate the serial console state variables.
static uint8_t currentState;
static uint8_t bufferOffset;
static uint8_t dmaBufferA [SERIAL_CONSOLE_DMA_BUFFER_SIZE];
static uint8_t dmaBufferB [SERIAL_CONSOLE_DMA_BUFFER_SIZE];

/*
 * This function implements the STM32 serial debug task handler.
 */
static inline gmosTaskStatus_t gmosPalSerialConsoleTaskHandler (void* nullData)
{
    uint8_t* inputBuffer;

    // Select the current input buffer.
    if (currentState == CONSOLE_STATE_DMA_BUF_A) {
        inputBuffer = dmaBufferB;
    } else {
        inputBuffer = dmaBufferA;
    }

    // Read data from the input stream into the input buffer.
    if (bufferOffset < SERIAL_CONSOLE_DMA_BUFFER_SIZE) {
        uint8_t* readPtr = &inputBuffer [bufferOffset];
        uint16_t readSize = gmosStreamRead (&consoleStream, readPtr,
            SERIAL_CONSOLE_DMA_BUFFER_SIZE - bufferOffset);
        bufferOffset += readSize;
    }

    // Poll the DMA controller for completion and then disable it. If a
    // DMA transfer is still active, reschedule the task for immediate
    // execution. This prevents the device from sleeping while a DMA is
    // in progress.
    if (currentState != CONSOLE_STATE_DMA_IDLE) {
        if (((DMA1->ISR & (DMA_ISR_TCIF4 | DMA_ISR_TEIF4)) == 0) ||
            ((USART2->ISR & USART_ISR_TXE) == 0)) {
            return GMOS_TASK_RUN_IMMEDIATE;
        } else {
            DMA1_Channel4->CCR &= ~DMA_CCR_EN;
            DMA1->IFCR = DMA_IFCR_CTCIF4 | DMA_IFCR_CTEIF4;
        }
    }

    // If there is no more data to be transmitted, enter the idle state
    // and suspend the task until new data is ready.
    if (bufferOffset == 0) {
        currentState = CONSOLE_STATE_DMA_IDLE;
        return GMOS_TASK_SUSPEND;
    }

    // Set up the DMA to transfer data from the input buffer.
    DMA1_Channel4->CNDTR = (uint32_t) bufferOffset;
    DMA1_Channel4->CMAR = (uint32_t)(uintptr_t) inputBuffer;
    DMA1_Channel4->CCR |= DMA_CCR_EN;

    // Update the task state to indicate the DMA buffer in use, then
    // reschedule the task for immediate execution.
    if (currentState == CONSOLE_STATE_DMA_BUF_A) {
        currentState = CONSOLE_STATE_DMA_BUF_B;
    } else {
        currentState = CONSOLE_STATE_DMA_BUF_A;
    }
    bufferOffset = 0;
    return GMOS_TASK_RUN_IMMEDIATE;
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

    // Initialise the serial console state variables.
    currentState = CONSOLE_STATE_DMA_IDLE;
    bufferOffset = 0;

    // Initialise the task and stream state.
    gmosStreamInit (&consoleStream,
        &consoleTask, GMOS_CONFIG_STM32_DEBUG_CONSOLE_BUFFER_SIZE);
    gmosPalSerialConsoleTask_start (&consoleTask, NULL, "Debug Console");

    // Configure GPIO A2 pin for USART2 transmit (high speed push/pull).
    gmosDriverGpioAltModeInit (STM32_GPIO_BANK_A | 2,
        GMOS_DRIVER_GPIO_OUTPUT_PUSH_PULL, STM32_GPIO_DRIVER_SLEW_FAST,
        GMOS_DRIVER_GPIO_INPUT_PULL_NONE, 4);

    // Enable clocks for USART2 and DMA. Note that these are not enabled
    // in the corresponding sleep mode registers, so the clocks will
    // automatically be gated on entering sleep mode.
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    RCC->AHBENR |= RCC_AHBENR_DMAEN;

    // Set the USART2 baud rate (8N1 format is selected by default).
    usartDiv = GMOS_CONFIG_STM32_APB1_CLOCK /
        GMOS_CONFIG_STM32_DEBUG_CONSOLE_BAUD_RATE;
    USART2->BRR = usartDiv;

    // Configure USART2 to use DMA based transmission.
    USART2->CR3 = USART_CR3_DMAT;

    // Set up DMA channel 4 for use with the console UART. This DMA
    // channel is the only one available for use with USART2 on all
    // device categories.
    DMA1_CSELR->CSELR |= (4 << DMA_CSELR_C4S_Pos);
    DMA1_Channel4->CCR = DMA_CCR_DIR | DMA_CCR_MINC;
    DMA1_Channel4->CPAR = (uint32_t)(uintptr_t) &(USART2->TDR);

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

#endif // GMOS_CONFIG_STM32_DEBUG_CONSOLE_USE_DMA
