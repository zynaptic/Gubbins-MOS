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
 * Implements the OpenThread command line interface adaptor using the
 * standard GubbinsMOS debug console as the output and direct polling
 * of the debug console UART as the input.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <ctype.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-driver-gpio.h"
#include "gmos-scheduler.h"
#include "gmos-openthread.h"
#include "efr32-device.h"
#include "efr32-driver-gpio.h"
#include "em_gpio.h"
#include "em_usart.h"
#include "openthread/cli.h"
#include "openthread/error.h"

// Define the maximum transmit and receive buffer sizes.
#define GMOS_OPENTHREAD_CLI_TX_BUF_SIZE 256
#define GMOS_OPENTHREAD_CLI_RX_BUF_SIZE 128

// Define the startup delay to use before issuing the first command
// prompt.
#define GMOS_OPENTHREAD_CLI_STARTUP_DELAY (GMOS_MS_TO_TICKS (5000))

// Define the maximum polling receive data polling interval to use.
#define GMOS_OPENTHREAD_CLI_MAX_POLL_INTERVAL (GMOS_MS_TO_TICKS (200))

// Specify the command reader state space.
typedef enum {
    GMOS_OPENTHREAD_CLI_STATE_RESET,
    GMOS_OPENTHREAD_CLI_STATE_INIT,
    GMOS_OPENTHREAD_CLI_STATE_CMD_INIT,
    GMOS_OPENTHREAD_CLI_STATE_CMD_POLL,
    GMOS_OPENTHREAD_CLI_STATE_CMD_ECHO,
    GMOS_OPENTHREAD_CLI_STATE_CMD_ISSUE,
    GMOS_OPENTHREAD_CLI_STATE_CMD_OVERFLOW,
    GMOS_OPENTHREAD_CLI_STATE_CMD_DISCARD
} gmosOpenThreadCliState_t;

// Specify the current operating state for the CLI command processor.
static uint8_t gmosOpenThreadCliState;

// Allocate space for the received command buffer.
static uint8_t cmdBuffer [GMOS_OPENTHREAD_CLI_RX_BUF_SIZE];

// Specify the current command buffer offset.
static uint8_t cmdBufferOffset;

// Specify the command buffer echo character.
static uint8_t cmdBufferChar;

// Specify the current receive interval backoff.
static uint32_t pollInterval;

// Allocate memory for the command processing task.
static gmosTaskState_t gmosOpenThreadCliTask;

// Set the end of line string to use for the terminal echo.
#if GMOS_CONFIG_LOG_MESSAGE_CRLF
static uint8_t lineTerminator [] = "\r\n";
#else
static uint8_t lineTerminator [] = "\n";
#endif

/*
 * Implement callback for forwarding OpenThread CLI output to the
 * GubbinsMOS debug console.
 */
static int gmosOpenThreadCliWrite (
    void *aContext, const char *aFormat, va_list aArguments)
{
    (void) aContext;

    // Set the formatting buffer is an arbitrary maximum size.
    uint8_t writeData [GMOS_OPENTHREAD_CLI_TX_BUF_SIZE];
    size_t writeSize;

    // Format the CLI output.
    writeSize = vsnprintf ((char *) writeData,
        sizeof (writeData), aFormat, aArguments);
    if (writeSize >= sizeof (writeData)) {
        writeSize = sizeof (writeData) - 1;
        GMOS_LOG (LOG_WARNING, "Truncated OpenThread CLI output.");
    }

    // Attempt to queue the CLI output to the output stream.
    if (!gmosPalSerialConsoleWrite (writeData, writeSize)) {
        writeSize = 0;
    }
    return writeSize;
}

/*
 * Implement UART polling interval backoff.
 */
static inline gmosTaskStatus_t gmosOpenThreadCliPollBackoff (void)
{
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_LATER (pollInterval);
    if (pollInterval == 0) {
        pollInterval = 1;
    } else if (pollInterval < GMOS_OPENTHREAD_CLI_MAX_POLL_INTERVAL) {
        pollInterval *= 2;
        if (pollInterval > GMOS_OPENTHREAD_CLI_MAX_POLL_INTERVAL) {
            pollInterval = GMOS_OPENTHREAD_CLI_MAX_POLL_INTERVAL;
        }
    }
    return taskStatus;
}

/*
 * Implement the command processing
 */
static gmosTaskStatus_t gmosOpenThreadCliTaskFn (
    gmosOpenThreadStack_t* openThreadStack)
{
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    uint8_t nextState = gmosOpenThreadCliState;

    // Implement the command processor state machine.
    switch (gmosOpenThreadCliState) {

        // Insert the specified startup delay after reset.
        case GMOS_OPENTHREAD_CLI_STATE_RESET :
            nextState = GMOS_OPENTHREAD_CLI_STATE_INIT;
            taskStatus = GMOS_TASK_RUN_LATER
                (GMOS_OPENTHREAD_CLI_STARTUP_DELAY);
            break;

        // Initialise the command processor after the startup delay.
        case GMOS_OPENTHREAD_CLI_STATE_INIT :
            GMOS_LOG (LOG_INFO, "Starting OpenThread interactive console.");
            otCliInit (openThreadStack->otInstance,
                gmosOpenThreadCliWrite, NULL);
            nextState = GMOS_OPENTHREAD_CLI_STATE_CMD_INIT;
            break;

        // Initialise the next command.
        case GMOS_OPENTHREAD_CLI_STATE_CMD_INIT :
            cmdBufferOffset = 0;
            pollInterval = 0;
            nextState = GMOS_OPENTHREAD_CLI_STATE_CMD_POLL;
            break;

        // Poll the UART for received data. Carriage returns are used
        // for command termination.
        case GMOS_OPENTHREAD_CLI_STATE_CMD_POLL :
            if ((USART0->STATUS & USART_STATUS_RXDATAV) == 0) {
                taskStatus = gmosOpenThreadCliPollBackoff ();
            } else {
                cmdBufferChar = USART0->RXDATA;
                if (cmdBufferChar == '\r') {
                    nextState = GMOS_OPENTHREAD_CLI_STATE_CMD_ISSUE;
                } else if (cmdBufferChar == '\b') {
                    if (cmdBufferOffset > 0) {
                        cmdBufferOffset -= 1;
                        nextState = GMOS_OPENTHREAD_CLI_STATE_CMD_ECHO;
                    }
                } else if (isprint (cmdBufferChar)) {
                    if (cmdBufferOffset < sizeof (cmdBuffer) - 1) {
                        cmdBuffer [cmdBufferOffset++] = cmdBufferChar;
                        nextState = GMOS_OPENTHREAD_CLI_STATE_CMD_ECHO;
                    } else {
                        nextState = GMOS_OPENTHREAD_CLI_STATE_CMD_OVERFLOW;
                    }
                }
            }
            break;

        // Echo valid characters back to the console.
        case GMOS_OPENTHREAD_CLI_STATE_CMD_ECHO :
            if (gmosPalSerialConsoleWrite (&cmdBufferChar, 1)) {
                pollInterval = 0;
                nextState = GMOS_OPENTHREAD_CLI_STATE_CMD_POLL;
            }
            break;

        // Issue a new command.
        case GMOS_OPENTHREAD_CLI_STATE_CMD_ISSUE :
            if (gmosPalSerialConsoleWrite (
                lineTerminator, sizeof (lineTerminator))) {
                cmdBuffer [cmdBufferOffset] = '\0';
                otCliInputLine ((char*) cmdBuffer);
                nextState = GMOS_OPENTHREAD_CLI_STATE_CMD_INIT;
            }
            break;
    }

    // Update the task state on exit.
    gmosOpenThreadCliState = nextState;
    return taskStatus;
}

// Define the command processing task.
GMOS_TASK_DEFINITION (gmosOpenThreadCliTask,
    gmosOpenThreadCliTaskFn, gmosOpenThreadStack_t);

/*
 * Initialise the OpenThread CLI support using the GubbinsMOS debug
 * console. The EFR32 debug console is normally configured for transmit
 * only using USART0. This sets up the same USART to support receive
 * operation.
 */
bool gmosOpenThreadCliInit (gmosOpenThreadStack_t* openThreadStack)
{
    GPIO_Port_TypeDef rxPinPort;
    uint16_t rxPinSel;

    // The debug console receive pin must be defined for use with the
    // OpenThread CLI.
    if (GMOS_CONFIG_EFR32_DEBUG_CONSOLE_RX_PIN ==
        GMOS_DRIVER_GPIO_UNUSED_PIN_ID) {
        return false;
    }

    // Configure the selected GPIO pin for USART0 receive.
    gmosDriverGpioPinInit (GMOS_CONFIG_EFR32_DEBUG_CONSOLE_RX_PIN,
        GMOS_DRIVER_GPIO_OUTPUT_PUSH_PULL, EFR32_GPIO_DRIVER_SLEW_SLOW,
        GMOS_DRIVER_GPIO_INPUT_PULL_NONE);

    // Configure the RTS output pin if required.
    if (GMOS_CONFIG_EFR32_DEBUG_CONSOLE_RTS_PIN !=
        GMOS_DRIVER_GPIO_UNUSED_PIN_ID) {
        gmosDriverGpioPinInit (GMOS_CONFIG_EFR32_DEBUG_CONSOLE_RTS_PIN,
            GMOS_DRIVER_GPIO_OUTPUT_PUSH_PULL, EFR32_GPIO_DRIVER_SLEW_FAST,
            GMOS_DRIVER_GPIO_INPUT_PULL_NONE);
        gmosDriverGpioSetAsOutput (GMOS_CONFIG_EFR32_DEBUG_CONSOLE_RTS_PIN);
    }

    // Route the USART0 receive signal to the specified pin.
    rxPinPort = (GMOS_CONFIG_EFR32_DEBUG_CONSOLE_RX_PIN >> 8) & 0x03;
    rxPinSel = GMOS_CONFIG_EFR32_DEBUG_CONSOLE_RX_PIN & 0x0F;
    GPIO->USARTROUTE[0].RXROUTE =
        (rxPinPort << _GPIO_USART_RXROUTE_PORT_SHIFT) |
        (rxPinSel << _GPIO_USART_RXROUTE_PIN_SHIFT);
    GPIO->USARTROUTE[0].ROUTEEN |= GPIO_USART_ROUTEEN_RXPEN;

    // Route the RTS output pin and enable RTS flow control if required.
    if (GMOS_CONFIG_EFR32_DEBUG_CONSOLE_RTS_PIN !=
        GMOS_DRIVER_GPIO_UNUSED_PIN_ID) {
        rxPinPort = (GMOS_CONFIG_EFR32_DEBUG_CONSOLE_RTS_PIN >> 8) & 0x03;
        rxPinSel = GMOS_CONFIG_EFR32_DEBUG_CONSOLE_RTS_PIN & 0x0F;
        GPIO->USARTROUTE[0].RTSROUTE =
            (rxPinPort << _GPIO_USART_RTSROUTE_PORT_SHIFT) |
            (rxPinSel << _GPIO_USART_RTSROUTE_PIN_SHIFT);
        GPIO->USARTROUTE[0].ROUTEEN |= GPIO_USART_ROUTEEN_RTSPEN;
    }

    // Initialise the local state.
    gmosOpenThreadCliState = GMOS_OPENTHREAD_CLI_STATE_RESET;
    cmdBufferOffset = 0;
    pollInterval = 0;

    // Run the command processing task.
    gmosOpenThreadCliTask_start (&gmosOpenThreadCliTask,
        openThreadStack, "OpenThread CLI");
    return true;
}
