/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2023 Zynaptic Limited
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
 * polled UART write operations on USART0.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-scheduler.h"
#include "gmos-streams.h"
#include "gmos-driver-gpio.h"
#include "efr32-device.h"
#include "efr32-driver-gpio.h"
#include "em_cmu.h"
#include "em_gpio.h"
#include "em_usart.h"

// Use this implementation if the basic serial console is selected.
#if !GMOS_CONFIG_EFR32_DEBUG_CONSOLE_USE_DMA

// Statically allocate the task and stream state structures.
static gmosTaskState_t consoleTask;
static gmosStream_t consoleStream;

/*
 * This function implements the EFR32 serial debug task handler.
 */
static inline gmosTaskStatus_t gmosPalSerialConsoleTaskHandler (void* nullData)
{
    (void) nullData;
    uint8_t txByte;

    // Poll the serial port for completion.
    if ((USART0->STATUS & USART_STATUS_TXBL) == 0) {
        return GMOS_TASK_RUN_IMMEDIATE;
    }

    // Attempt to read a byte from the console stream.
    if (gmosStreamReadByte (&consoleStream, &txByte)) {
        USART0->TXDATA = (uint32_t) txByte;
        return GMOS_TASK_RUN_IMMEDIATE;
    } else {
        return GMOS_TASK_SUSPEND;
    }
}

// Define the console task.
GMOS_TASK_DEFINITION (gmosPalSerialConsoleTask,
    gmosPalSerialConsoleTaskHandler, void);

/*
 * Initialises the EFR32 serial debug console.
 */
void gmosPalSerialConsoleInit (void)
{
    USART_InitAsync_TypeDef usartInit = USART_INITASYNC_DEFAULT;
    GPIO_Port_TypeDef txPinPort;
    uint16_t txPinSel;

    // Initialise the task and stream state.
    gmosStreamInit (&consoleStream,
        &consoleTask, GMOS_CONFIG_EFR32_DEBUG_CONSOLE_BUFFER_SIZE);
    gmosPalSerialConsoleTask_start (&consoleTask, NULL, "Debug Console");

    // Enable the USART0 clock.
    CMU_ClockEnable (cmuClock_USART0, true);

    // Configure the selected GPIO pin for USART transmit.
    gmosDriverGpioPinInit (GMOS_CONFIG_EFR32_DEBUG_CONSOLE_TX_PIN,
        GMOS_DRIVER_GPIO_OUTPUT_PUSH_PULL, EFR32_GPIO_DRIVER_SLEW_FAST,
        GMOS_DRIVER_GPIO_INPUT_PULL_NONE);
    gmosDriverGpioSetAsOutput (GMOS_CONFIG_EFR32_DEBUG_CONSOLE_TX_PIN);

    // Configure the selected GPIO pin for USART CTS if required.
    if (GMOS_CONFIG_EFR32_DEBUG_CONSOLE_CTS_PIN != GMOS_DRIVER_GPIO_UNUSED_PIN_ID) {
        gmosDriverGpioPinInit (GMOS_CONFIG_EFR32_DEBUG_CONSOLE_CTS_PIN,
            GMOS_DRIVER_GPIO_OUTPUT_PUSH_PULL, EFR32_GPIO_DRIVER_SLEW_SLOW,
            GMOS_DRIVER_GPIO_INPUT_PULL_UP);
    }

    // Assert the debug console enable pin if required.
    if (GMOS_CONFIG_EFR32_DEBUG_CONSOLE_EN_PIN != GMOS_DRIVER_GPIO_UNUSED_PIN_ID) {
        gmosDriverGpioPinInit (GMOS_CONFIG_EFR32_DEBUG_CONSOLE_EN_PIN,
            GMOS_DRIVER_GPIO_OUTPUT_PUSH_PULL, EFR32_GPIO_DRIVER_SLEW_SLOW,
            GMOS_DRIVER_GPIO_INPUT_PULL_NONE);
        gmosDriverGpioSetAsOutput (GMOS_CONFIG_EFR32_DEBUG_CONSOLE_EN_PIN);
        gmosDriverGpioSetPinState (GMOS_CONFIG_EFR32_DEBUG_CONSOLE_EN_PIN, true);
    }

    // Route the USART0 transmit signal to the specified pin.
    txPinPort = (GMOS_CONFIG_EFR32_DEBUG_CONSOLE_TX_PIN >> 8) & 0x03;
    txPinSel = GMOS_CONFIG_EFR32_DEBUG_CONSOLE_TX_PIN & 0x0F;
    GPIO->USARTROUTE[0].TXROUTE =
        (txPinPort << _GPIO_USART_TXROUTE_PORT_SHIFT) |
        (txPinSel << _GPIO_USART_TXROUTE_PIN_SHIFT);
    GPIO->USARTROUTE[0].ROUTEEN = GPIO_USART_ROUTEEN_TXPEN;

    // Route the USART0 CTS signal from the specified pin.
    if (GMOS_CONFIG_EFR32_DEBUG_CONSOLE_CTS_PIN != GMOS_DRIVER_GPIO_UNUSED_PIN_ID) {
        txPinPort = (GMOS_CONFIG_EFR32_DEBUG_CONSOLE_CTS_PIN >> 8) & 0x03;
        txPinSel = GMOS_CONFIG_EFR32_DEBUG_CONSOLE_CTS_PIN & 0x0F;
        GPIO->USARTROUTE[0].CTSROUTE =
            (txPinPort << _GPIO_USART_CTSROUTE_PORT_SHIFT) |
            (txPinSel << _GPIO_USART_CTSROUTE_PIN_SHIFT);
    }

    // Initialise USART0 ready for use.
    usartInit.baudrate = GMOS_CONFIG_EFR32_DEBUG_CONSOLE_BAUD_RATE;
    if (GMOS_CONFIG_EFR32_DEBUG_CONSOLE_CTS_PIN != GMOS_DRIVER_GPIO_UNUSED_PIN_ID) {
        usartInit.hwFlowControl = usartHwFlowControlCts;
    }
    USART_InitAsync (USART0, &usartInit);

    // Invert the CTS signal if required.
    if (GMOS_CONFIG_EFR32_DEBUG_CONSOLE_RTS_CTS_INV) {
        USART0->CTRLX |= USART_CTRLX_CTSINV;
    }
}

/*
 * Attempts to write the contents of the supplied data buffer to the
 * EFR32 serial debug console.
 */
bool gmosPalSerialConsoleWrite (
    const uint8_t* writeData, uint16_t writeSize)
{
    return gmosStreamWriteAll (&consoleStream, writeData, writeSize);
}

/*
 * Flushes the EFR32 serial debug console after an assertion. This
 * function does not return.
 */
void gmosPalSerialConsoleFlushAssertion (void)
{
    uint8_t txByte;
    while (true) {
        if ((USART0->STATUS & USART_STATUS_TXBL) != 0) {
            if (gmosStreamReadByte (&consoleStream, &txByte)) {
                USART0->TXDATA = (uint32_t) txByte;
            }
        }
    }
}

#endif // GMOS_CONFIG_EFR32_DEBUG_CONSOLE_USE_DMA
