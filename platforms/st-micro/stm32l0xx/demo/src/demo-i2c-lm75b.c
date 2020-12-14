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
 * Demonstrates the use of the LM75B I2C temperature sensor that is
 * included on the ARM MBed development shield. This is a simple
 * implementation that does not make use of the power saving shutdown
 * feature of the LM75B.
 */

#include "gmos-platform.h"
#include "gmos-scheduler.h"
#include "gmos-driver-i2c.h"

// Define the temperature sensor state space.
#define TEMPERATURE_READ_IDLE   0x00
#define TEMPERATURE_READ_ACTIVE 0x01

// Hold the temperature sensor state information.
static uint8_t  tempReadState;

// Allocate the I2C device data structure.
static gmosDriverI2CDevice_t i2cDevice;

// Allocate the temperature reader task state data structure.
static gmosTaskState_t tempReadTaskState;

/*
 * Implements the temperature sensor read task.
 */
static gmosTaskStatus_t tempReadHandler (void* nullData)
{
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_BACKGROUND;
    gmosDriverI2CStatus_t i2cStatus;
    uint8_t i2cDataBuffer [2];
    uint8_t i2cDataSize;
    int16_t tempResult;

    switch (tempReadState) {

        // Initiate temperature read request from register 0.
        case TEMPERATURE_READ_IDLE :
            i2cDataBuffer [0] = 0;
            if (gmosDriverI2CIndexedReadRequest (&i2cDevice, i2cDataBuffer, 1, 2)) {
                GMOS_LOG (LOG_DEBUG, "I2C indexed read transaction started");
                tempReadState = TEMPERATURE_READ_ACTIVE;
                taskStatus = GMOS_TASK_RUN_IMMEDIATE;
            }
            break;

        // Poll for temperature read completion.
        case TEMPERATURE_READ_ACTIVE :
            i2cDataSize = 2;
            i2cStatus = gmosDriverI2CReadComplete
                (&i2cDevice, i2cDataBuffer, &i2cDataSize);
            if (i2cStatus == GMOS_DRIVER_I2C_STATUS_READING) {
                taskStatus = GMOS_TASK_RUN_IMMEDIATE;
            } else {
                GMOS_LOG (LOG_DEBUG, "I2C read transaction status = %d (%d bytes)",
                    i2cStatus, i2cDataSize);
                if (i2cStatus == GMOS_DRIVER_I2C_STATUS_SUCCESS) {
                    tempResult = ((int16_t) i2cDataBuffer [0]) << 8;
                    tempResult |= ((int16_t) i2cDataBuffer [1]) & 0xFF;
                    tempResult >>= 5;
                    GMOS_LOG (LOG_INFO, "LM75B temperature = %d.%d C",
                        tempResult / 8, (tempResult % 8) * 125);
                }
                tempReadState = TEMPERATURE_READ_IDLE;
                taskStatus = GMOS_TASK_RUN_AFTER (GMOS_MS_TO_TICKS
                    (GMOS_DEMO_APP_TEMP_SAMPLE_INTERVAL * 1000));
            }
            break;
    }
    return taskStatus;
}

// Define the temperature reader task.
GMOS_TASK_DEFINITION (tempRead, tempReadHandler, void);

/*
 * Initialise the LM75B I2C sensor using the specified I2C bus.
 */
void demoTempSensorInit (gmosDriverI2CBus_t* i2cBus)
{
    // Attach the temperature sensor to the I2C bus.
    gmosDriverI2CBusAddDevice (i2cBus, &i2cDevice, 0x48, &tempReadTaskState);

    // Run the temperature reader task.
    tempReadState = TEMPERATURE_READ_IDLE;
    tempRead_start (&tempReadTaskState, NULL, "Temperature Read Task");
}
