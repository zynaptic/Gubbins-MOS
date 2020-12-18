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
 * Implements I2C bus controller functionality for the STM32L0XX series
 * of microcontrollers.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-driver-i2c.h"
#include "stm32-device.h"
#include "stm32-driver-gpio.h"
#include "stm32-driver-i2c.h"

// Add dummy definitions if I2C interface 2 is not supported.
#ifndef I2C2
#define I2C2 NULL
#define I2C2_IRQn 0
#define RCC_APB1ENR_I2C2EN 0
#endif

// Add dummy definitions if I2C interface 3 is not supported.
#ifndef I2C3
#define I2C3 NULL
#define I2C3_IRQn 0
#define RCC_APB1ENR_I2C3EN 0
#endif

// Define the interrupt enable and disable macros.
#if GMOS_CONFIG_STM32_I2C_USE_INTERRUPTS
static IRQn_Type i2cNvicIrqMap [] = {I2C1_IRQn, I2C2_IRQn, I2C3_IRQn};
#define STM32_I2C_INTERRUPT_ENABLE(_i2cIndex_) \
    NVIC_EnableIRQ (i2cNvicIrqMap [_i2cIndex_])
#define STM32_I2C_INTERRUPT_DISABLE(_i2cIndex_) \
    NVIC_DisableIRQ (i2cNvicIrqMap [_i2cIndex_])
#else
#define STM32_I2C_INTERRUPT_ENABLE(_i2cIndex_) do {} while (0)
#define STM32_I2C_INTERRUPT_DISABLE(_i2cIndex_) do {} while (0)
#endif

// Provide mapping of I2C interface IDs to register sets.
static I2C_TypeDef* i2cRegisterMap [] = {I2C1, I2C2, I2C3};

// Provide mapping of I2C interface IDs to APB1 clock enable bits.
static uint32_t i2cClockEnMap [] = {
    RCC_APB1ENR_I2C1EN, RCC_APB1ENR_I2C2EN, RCC_APB1ENR_I2C3EN};

// Provide reverse mapping of I2C interface IDs to bus state data
// structures.
static gmosDriverI2CBus_t* i2cBusControllerMap [] ={NULL, NULL, NULL};

// Defines the low level I2C transfer phase.
#define TRANSFER_PHASE_IDLE     0x00
#define TRANSFER_PHASE_WRITE    0x01
#define TRANSFER_PHASE_READ     0x02
#define TRANSFER_PHASE_PREWRITE 0x03

/*
 * Selects the precalculated timing options generated according to
 * application note AN4235. The default values are based on a 16MHz APB1
 * bus clock. If different bus clock rates are to be supported,
 * additional preset values will need to be added here.
 */
static inline uint32_t gmosDriverI2CPalTiming (uint8_t i2cBusSpeed)
{
    uint32_t timingOptions;
#if (GMOS_CONFIG_STM32_APB1_CLOCK == 16000000)
    if (i2cBusSpeed == 0) {
        timingOptions = 0x00301D7A;
    } else {
        timingOptions = 0x00300619;
    }
#else
    #error "Unsupported APB1 clock rate for I2C driver"
#endif
    return timingOptions;
}

/*
 * Implements the interrupt handler for I2C writes.
 */
static void gmosDriverI2CPalWriteIsr (gmosDriverI2CBus_t* busController)
{
    gmosDriverI2CDevice_t* device = busController->currentDevice;
    gmosPalI2CBusState_t* palData = busController->platformData;
    const gmosPalI2CBusConfig_t* palConfig = busController->platformConfig;
    uint8_t i2cIndex = palConfig->i2cInterfaceId - 1;
    I2C_TypeDef* i2cRegs = i2cRegisterMap [i2cIndex];
    uint32_t statusReg;
    uint32_t errorMask;
    uint32_t eventBits = 0;

    // Handle error conditions.
    statusReg = i2cRegs->ISR;
    errorMask = I2C_ISR_BERR | I2C_ISR_ARLO;
    if ((statusReg & errorMask) != 0) {
        eventBits = GMOS_DRIVER_I2C_EVENT_COMPLETION_FLAG |
            GMOS_DRIVER_I2C_STATUS_BUS_ERROR;
    }

    // Handle NACK conditions.
    else if ((statusReg & I2C_ISR_NACKF) != 0) {
        eventBits = GMOS_DRIVER_I2C_EVENT_COMPLETION_FLAG |
            GMOS_DRIVER_I2C_STATUS_NACK;
    }

    // Handle transaction completion. Send a bus restart if a follow on
    // read is required, otherwise indicate completion.
    else if ((statusReg & I2C_ISR_TC) != 0) {
        if (palData->transferPhase == TRANSFER_PHASE_PREWRITE) {
            palData->byteCount = 0;
            palData->transferPhase = TRANSFER_PHASE_READ;
            i2cRegs->CR2 = I2C_CR2_START | I2C_CR2_RD_WRN |
                (((uint32_t) device->address) << 1) |
                (((uint32_t) busController->readSize) << I2C_CR2_NBYTES_Pos);
        } else {
            i2cRegs->CR2 |= I2C_CR2_STOP;
            eventBits = GMOS_DRIVER_I2C_EVENT_COMPLETION_FLAG |
                GMOS_DRIVER_I2C_STATUS_SUCCESS;
        }
    }

    // Load the next data byte if requested.
    else if ((statusReg & I2C_ISR_TXIS) != 0) {
        i2cRegs->TXDR = busController->dataBuffer [palData->byteCount++];
    }

    // On transaction completion, disable the I2C bus controller and
    // associated interrupts before setting the event flags.
    if (eventBits != 0) {
        while ((i2cRegs->ISR & I2C_ISR_BUSY) != 0) {
            // Busy wait in ISR assumes a very short delay.
        }
        i2cRegs->CR1 = 0;
        eventBits |= (((uint32_t) palData->byteCount) <<
            GMOS_DRIVER_I2C_EVENT_SIZE_OFFSET);
        gmosEventSetBits (&(busController->completionEvent), eventBits);
        palData->transferPhase = TRANSFER_PHASE_IDLE;
        STM32_I2C_INTERRUPT_DISABLE (i2cIndex);
    }
}

/*
 * Implements the interrupt handler for I2C reads.
 */
static void gmosDriverI2CPalReadIsr (gmosDriverI2CBus_t* busController)
{
    gmosPalI2CBusState_t* palData = busController->platformData;
    const gmosPalI2CBusConfig_t* palConfig = busController->platformConfig;
    uint8_t i2cIndex = palConfig->i2cInterfaceId - 1;
    I2C_TypeDef* i2cRegs = i2cRegisterMap [i2cIndex];
    uint32_t statusReg;
    uint32_t errorMask;
    uint32_t eventBits = 0;

    // Handle error conditions.
    statusReg = i2cRegs->ISR;
    errorMask = I2C_ISR_BERR | I2C_ISR_ARLO;
    if ((statusReg & errorMask) != 0) {
        eventBits = GMOS_DRIVER_I2C_EVENT_COMPLETION_FLAG |
            GMOS_DRIVER_I2C_STATUS_BUS_ERROR;
    }

    // Handle NACK conditions.
    else if ((statusReg & I2C_ISR_NACKF) != 0) {
        eventBits = GMOS_DRIVER_I2C_EVENT_COMPLETION_FLAG |
            GMOS_DRIVER_I2C_STATUS_NACK;
    }

    // Read the next data byte if available.
    else if ((statusReg & I2C_ISR_RXNE) != 0) {
        busController->dataBuffer [palData->byteCount++] = i2cRegs->RXDR;
        if ((statusReg & I2C_ISR_TC) != 0) {
            i2cRegs->CR2 |= I2C_CR2_STOP;
            eventBits = GMOS_DRIVER_I2C_EVENT_COMPLETION_FLAG |
                GMOS_DRIVER_I2C_STATUS_SUCCESS;
        }
    }

    // On transaction completion, disable the I2C bus controller and
    // associated interrupts before setting the event flags.
    if (eventBits != 0) {
        while ((i2cRegs->ISR & I2C_ISR_BUSY) != 0) {
            // Busy wait in ISR assumes a very short delay.
        }
        i2cRegs->CR1 = 0;
        eventBits |= (((uint32_t) palData->byteCount) <<
            GMOS_DRIVER_I2C_EVENT_SIZE_OFFSET);
        gmosEventSetBits (&(busController->completionEvent), eventBits);
        palData->transferPhase = TRANSFER_PHASE_IDLE;
        STM32_I2C_INTERRUPT_DISABLE (i2cIndex);
    }
}

/*
 * Implement ISR handler for I2C interface 1.
 */
#if GMOS_CONFIG_STM32_I2C_USE_INTERRUPTS
void gmosPalIsrI2C1 (void)
{
    gmosDriverI2CBus_t* busController = i2cBusControllerMap [0];
    uint8_t transferPhase = busController->platformData->transferPhase;

    // Select the appropriate ISR for the transaction type.
    if ((transferPhase == TRANSFER_PHASE_WRITE) ||
        (transferPhase == TRANSFER_PHASE_PREWRITE)) {
        gmosDriverI2CPalWriteIsr (busController);
    } else if (transferPhase == TRANSFER_PHASE_READ) {
        gmosDriverI2CPalReadIsr (busController);
    }
}
#endif

/*
 * Implement ISR handler for I2C interface 2.
 */
#if GMOS_CONFIG_STM32_I2C_USE_INTERRUPTS
void gmosPalIsrI2C2 (void)
{
    gmosDriverI2CBus_t* busController = i2cBusControllerMap [1];
    uint8_t transferPhase = busController->platformData->transferPhase;

    // Select the appropriate ISR for the transaction type.
    if ((transferPhase == TRANSFER_PHASE_WRITE) ||
        (transferPhase == TRANSFER_PHASE_PREWRITE)) {
        gmosDriverI2CPalWriteIsr (busController);
    } else if (transferPhase == TRANSFER_PHASE_READ) {
        gmosDriverI2CPalReadIsr (busController);
    }
}
#endif

/*
 * Implement ISR handler for I2C interface 3.
 */
#if GMOS_CONFIG_STM32_I2C_USE_INTERRUPTS
void gmosPalIsrI2C3 (void)
{
    gmosDriverI2CBus_t* busController = i2cBusControllerMap [2];
    uint8_t transferPhase = busController->platformData->transferPhase;

    // Select the appropriate ISR for the transaction type.
    if ((transferPhase == TRANSFER_PHASE_WRITE) ||
        (transferPhase == TRANSFER_PHASE_PREWRITE)) {
        gmosDriverI2CPalWriteIsr (busController);
    } else if (transferPhase == TRANSFER_PHASE_READ) {
        gmosDriverI2CPalReadIsr (busController);
    }
}
#endif

/*
 * Implement a task based polling loop if interrupt driven operation is
 * not selected. This will typically be used for debug purposes only.
 */
#if !GMOS_CONFIG_STM32_I2C_USE_INTERRUPTS
static gmosTaskState_t pollingLoopTaskData;

static gmosTaskStatus_t pollingLoopTaskFn (void* nullData)
{
    uint8_t i2cIndex;
    gmosDriverI2CBus_t* busController;
    gmosPalI2CBusState_t* palData;
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_BACKGROUND;

    for (i2cIndex = 0; i2cIndex < 3; i2cIndex++) {
        busController = i2cBusControllerMap [i2cIndex];
        if (busController != NULL) {
            palData = busController->platformData;
            if ((palData->transferPhase == TRANSFER_PHASE_WRITE) ||
                (palData->transferPhase == TRANSFER_PHASE_PREWRITE)) {
                gmosDriverI2CPalWriteIsr (busController);
                taskStatus = GMOS_TASK_RUN_IMMEDIATE;
            } else if (palData->transferPhase == TRANSFER_PHASE_READ) {
                gmosDriverI2CPalReadIsr (busController);
                taskStatus = GMOS_TASK_RUN_IMMEDIATE;
            }
        }
    }
    return taskStatus;
}

GMOS_TASK_DEFINITION (pollingLoopTask, pollingLoopTaskFn, void);
#endif

/*
 * Initialises the platform abstraction layer for a given I2C bus
 * configuration. Refer to the platform specific I2C implementation for
 * details of the platform data area and the bus configuration options.
 * This function is called automatically by the 'gmosDriverI2CBusInit'
 * function.
 */
bool gmosDriverI2CPalInit (gmosDriverI2CBus_t* busController)
{
    gmosPalI2CBusState_t* palData = busController->platformData;
    const gmosPalI2CBusConfig_t* palConfig = busController->platformConfig;
    uint8_t i2cIndex;
    I2C_TypeDef* i2cRegs;

    // Keep a reference to the platform data structures.
    i2cIndex = palConfig->i2cInterfaceId - 1;
    if ((i2cIndex >= 3) || (i2cRegisterMap [i2cIndex] == NULL)) {
        return false;
    }
    i2cRegs = i2cRegisterMap [i2cIndex];
    i2cBusControllerMap [i2cIndex] = busController;

    // Configure both pins as high speed open drain with pullup.
    gmosDriverGpioAltModeInit (palConfig->sclPinId,
        STM32_GPIO_DRIVER_OPEN_DRAIN, STM32_GPIO_DRIVER_SLEW_FAST,
        STM32_GPIO_INPUT_PULL_UP, palConfig->sclPinAltFn);
    gmosDriverGpioAltModeInit (palConfig->sdaPinId,
        STM32_GPIO_DRIVER_OPEN_DRAIN, STM32_GPIO_DRIVER_SLEW_FAST,
        STM32_GPIO_INPUT_PULL_UP, palConfig->sdaPinAltFn);

    // Enable the I2C peripheral clock. Note that this is not enabled in
    // the corresponding sleep mode register, so it will automatically
    // be gated on entering sleep mode.
    RCC->APB1ENR |= i2cClockEnMap [i2cIndex];

    // Configure the I2C interface timing options.
    i2cRegs->TIMINGR = gmosDriverI2CPalTiming (palConfig->i2cBusSpeed);

    // Place the low level driver in its idle state.
    palData->transferPhase = TRANSFER_PHASE_IDLE;

    // Run the polling loop if interrupts are not being used.
#if !GMOS_CONFIG_STM32_I2C_USE_INTERRUPTS
    pollingLoopTask_start (&pollingLoopTaskData,
        NULL, "STM32 I2C Driver Polling Loop");
#endif
    return true;
}

/*
 * Initiates a low level I2C transfer request. After processing the
 * transaction, the transfer status will be indicated via the I2C bus
 * completion event.
 */
void gmosDriverI2CPalTransaction (gmosDriverI2CBus_t* busController)
{
    gmosDriverI2CDevice_t* device = busController->currentDevice;
    gmosPalI2CBusState_t* palData = busController->platformData;
    const gmosPalI2CBusConfig_t* palConfig = busController->platformConfig;
    uint8_t i2cIndex = palConfig->i2cInterfaceId - 1;
    I2C_TypeDef* i2cRegs = i2cRegisterMap [i2cIndex];
    uint32_t configReg1;
    uint32_t configReg2;

    // Reset the transfer byte counter.
    palData->byteCount = 0;

    // Set the default configuration register options.
    configReg1 = I2C_CR1_PE | I2C_CR1_NACKIE | I2C_CR1_ERRIE;
    configReg2 = I2C_CR2_START | (((uint32_t) device->address) << 1);

    // Select configuration for write only operation.
    if ((busController->writeSize != 0) &&
        (busController->readSize == 0)) {
        palData->transferPhase = TRANSFER_PHASE_WRITE;
        configReg1 |= I2C_CR1_TXIE | I2C_CR1_TCIE;
        configReg2 |=
            (((uint32_t) busController->writeSize) << I2C_CR2_NBYTES_Pos);
    }

    // Select configuration for write then read.
    else if ((busController->writeSize != 0) &&
        (busController->readSize != 0)) {
        palData->transferPhase = TRANSFER_PHASE_PREWRITE;
        configReg1 |= I2C_CR1_TXIE | I2C_CR1_TCIE | I2C_CR1_RXIE;
        configReg2 |=
            (((uint32_t) busController->writeSize) << I2C_CR2_NBYTES_Pos);
    }

    // Select configuration for read only operation.
    else {
        palData->transferPhase = TRANSFER_PHASE_READ;
        configReg1 |= I2C_CR1_RXIE;
        configReg2 |= I2C_CR2_RD_WRN |
            (((uint32_t) busController->readSize) << I2C_CR2_NBYTES_Pos);
    }

    // Initiate the transfer.
    i2cRegs->CR1 = configReg1;
    i2cRegs->CR2 = configReg2;

    // Enable the appropriate NVIC interrupt.
    STM32_I2C_INTERRUPT_ENABLE (i2cIndex);
}
