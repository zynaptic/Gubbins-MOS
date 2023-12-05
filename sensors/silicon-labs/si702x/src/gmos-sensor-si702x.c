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
 * This file implements the IIC device driver for the Silicon Labs
 * Si7020 and Si7021 hygrometer and temperature sensors.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-sensor-si702x.h"

/*
 * Define the set of sensor operating phases.
 */
typedef enum {
    GMOS_SENSOR_SI702X_TASK_PHASE_FAILED,
    GMOS_SENSOR_SI702X_TASK_PHASE_INIT,
    GMOS_SENSOR_SI702X_TASK_PHASE_IDLE,
    GMOS_SENSOR_SI702X_TASK_PHASE_SAMPLE_T,
    GMOS_SENSOR_SI702X_TASK_PHASE_SAMPLE_H,
    GMOS_SENSOR_SI702X_TASK_PHASE_HEATING
} gmosSensorSi702xTaskPhase_t;

/*
 * Define the state space for the driver initialisation state machine.
 */
typedef enum {
    GMOS_SENSOR_SI702X_TASK_STATE_INIT_IDLE,
    GMOS_SENSOR_SI702X_TASK_STATE_INIT_START,
    GMOS_SENSOR_SI702X_TASK_STATE_INIT_SA_REQ,
    GMOS_SENSOR_SI702X_TASK_STATE_INIT_SA_POLL,
    GMOS_SENSOR_SI702X_TASK_STATE_INIT_SB_REQ,
    GMOS_SENSOR_SI702X_TASK_STATE_INIT_SB_POLL,
    GMOS_SENSOR_SI702X_TASK_STATE_INIT_CR_SET_REQ,
    GMOS_SENSOR_SI702X_TASK_STATE_INIT_CR_SET_POLL,
    GMOS_SENSOR_SI702X_TASK_STATE_INIT_CR_GET_REQ,
    GMOS_SENSOR_SI702X_TASK_STATE_INIT_CR_GET_POLL,
    GMOS_SENSOR_SI702X_TASK_STATE_INIT_RELEASE
} gmosSensorSi702xTaskStateInit_t;

/*
 * Define the state space for the sensor sampling state machine.
 */
typedef enum {
    GMOS_SENSOR_SI702X_TASK_STATE_SAMPLE_IDLE,
    GMOS_SENSOR_SI702X_TASK_STATE_SAMPLE_REQ,
    GMOS_SENSOR_SI702X_TASK_STATE_SAMPLE_POLL,
    GMOS_SENSOR_SI702X_TASK_STATE_SAMPLE_PROCESS,
    GMOS_SENSOR_SI702X_TASK_STATE_SAMPLE_QUEUE,
    GMOS_SENSOR_SI702X_TASK_STATE_SAMPLE_RELEASE
} gmosSensorSi702xTaskStateSample_t;

/*
 * Define the state space for the device heater state machine.
 */
typedef enum {
    GMOS_SENSOR_SI702X_TASK_STATE_HEATER_IDLE,
    GMOS_SENSOR_SI702X_TASK_STATE_HEATER_LVL_REQ,
    GMOS_SENSOR_SI702X_TASK_STATE_HEATER_LVL_POLL,
    GMOS_SENSOR_SI702X_TASK_STATE_HEATER_EN_REQ,
    GMOS_SENSOR_SI702X_TASK_STATE_HEATER_EN_POLL,
    GMOS_SENSOR_SI702X_TASK_STATE_HEATER_STARTED,
    GMOS_SENSOR_SI702X_TASK_STATE_HEATER_RUNNING,
    GMOS_SENSOR_SI702X_TASK_STATE_HEATER_STOP,
    GMOS_SENSOR_SI702X_TASK_STATE_HEATER_NEN_REQ,
    GMOS_SENSOR_SI702X_TASK_STATE_HEATER_NEN_POLL,
    GMOS_SENSOR_SI702X_TASK_STATE_HEATER_RELEASE,
    GMOS_SENSOR_SI702X_TASK_STATE_HEATER_COOLDOWN,
} gmosSensorSi702xTaskStateHeater_t;

/*
 * Calculate the Si702x CRC over the specified number of data bytes.
 * This uses a CRC polynomial of x^8 + x^5 + x^4 + 1 with an initial
 * value of 0.
 */
static uint8_t gmosSensorSi702xCalculateCRC (
    uint8_t* data, uint8_t length)
{
    uint8_t crc = 0;
    uint8_t i, j;
    for (i = 0; i < length; i++) {
        crc ^= data [i];
        for (j = 8; j > 0; j--) {
            if ((crc & 0x80) != 0) {
                crc = (crc << 1) ^ 0x131;
            } else {
                crc = (crc << 1);
            }
        }
    }
    return crc;
}

/*
 * Process the serial number block A response message. The format is not
 * clearly described in the datasheet, but the CRC is a running CRC over
 * all of the serial number bytes, so it is only necessary to check the
 * final CRC byte.
 */
static inline bool gmosSensorSi702xCheckSerialA (
    gmosSensorSi702x_t* sensorData)
{
    uint8_t* rxBuf = sensorData->rxBuffer;
    uint8_t* snData = &(sensorData->serialNumber [0]);
    uint8_t crc;

    // Copy the four serial number bytes to local storage.
    snData [0] = rxBuf [0];
    snData [1] = rxBuf [2];
    snData [2] = rxBuf [4];
    snData [3] = rxBuf [6];

    // Check the CRC over all the serial number bytes.
    crc = gmosSensorSi702xCalculateCRC (snData, 4);
    return (crc == rxBuf [7]) ? true : false;
}

/*
 * Process the serial number block B response message. The format is not
 * clearly described in the datasheet, but the CRC is a running CRC over
 * all of the serial number bytes, so it is only necessary to check the
 * final CRC byte.
 */
static inline bool gmosSensorSi702xCheckSerialB (
    gmosSensorSi702x_t* sensorData)
{
    uint8_t* rxBuf = sensorData->rxBuffer;
    uint8_t* snData = &(sensorData->serialNumber [4]);
    uint8_t crc;

    // Copy the four serial number bytes to local storage.
    snData [0] = rxBuf [0];
    snData [1] = rxBuf [1];
    snData [2] = rxBuf [3];
    snData [3] = rxBuf [4];

    // Check the CRC over all the serial number bytes.
    crc = gmosSensorSi702xCalculateCRC (snData, 4);
    if (crc == rxBuf [5]) {
        uint8_t* snFull = &(sensorData->serialNumber [0]);
        GMOS_LOG_FMT (LOG_INFO, "Si702x valid S/N : "
            "%02X %02X %02X %02X %02X %02X %02X %02X",
            snFull [0], snFull [1], snFull [2], snFull [3],
            snFull [4], snFull [5], snFull [6], snFull [7]);
        return true;
    } else {
        return false;
    }
}

/*
 * Implement the startup phase of the sensor state machine.
 */
static inline gmosTaskStatus_t gmosSensorSi702xStartup (
    gmosSensorSi702x_t* sensorData)
{
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    gmosDriverIicBus_t* iicInterface = sensorData->iicInterface;
    gmosDriverIicDevice_t* iicDevice = &(sensorData->iicDevice);
    gmosDriverIicStatus_t iicStatus;
    uint8_t nextPhase = sensorData->sensorPhase;
    uint8_t nextState = sensorData->sensorState;
    uint8_t* txBuf = sensorData->txBuffer;
    uint8_t* rxBuf = sensorData->rxBuffer;

    // Implement startup phase state machine.
    switch (sensorData->sensorState) {

        // Insert a short delay after reset before attempting to access
        // the device. The maximum power up time is 80ms.
        case GMOS_SENSOR_SI702X_TASK_STATE_INIT_IDLE :
            nextState = GMOS_SENSOR_SI702X_TASK_STATE_INIT_START;
            taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (100));
            break;

        // Select the IIC device to start the initialisation process.
        case GMOS_SENSOR_SI702X_TASK_STATE_INIT_START :
            if (gmosDriverIicDeviceSelect (iicInterface, iicDevice)) {
                nextState = GMOS_SENSOR_SI702X_TASK_STATE_INIT_SA_REQ;
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (500));
            }
            break;

        // Issue a request for serial number block A.
        case GMOS_SENSOR_SI702X_TASK_STATE_INIT_SA_REQ :
            txBuf [0] = 0xFA;
            txBuf [1] = 0x0F;
            if (gmosDriverIicIoTransfer (
                iicInterface, txBuf, rxBuf, 2, 8)) {
                nextState = GMOS_SENSOR_SI702X_TASK_STATE_INIT_SA_POLL;
                taskStatus = GMOS_TASK_SUSPEND;
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (10));
            }
            break;

        // Poll for completion of the serial number block A request.
        case GMOS_SENSOR_SI702X_TASK_STATE_INIT_SA_POLL :
            iicStatus = gmosDriverIicIoComplete (iicInterface, NULL);
            if (iicStatus == GMOS_DRIVER_IIC_STATUS_SUCCESS) {
                if (gmosSensorSi702xCheckSerialA (sensorData)) {
                    nextState = GMOS_SENSOR_SI702X_TASK_STATE_INIT_SB_REQ;
                } else {
                    nextPhase = GMOS_SENSOR_SI702X_TASK_PHASE_FAILED;
                }
            } else if (iicStatus == GMOS_DRIVER_IIC_STATUS_ACTIVE) {
                taskStatus = GMOS_TASK_SUSPEND;
            } else {
                nextPhase = GMOS_SENSOR_SI702X_TASK_PHASE_FAILED;
            }
            break;

        // Issue a request for serial number block B.
        case GMOS_SENSOR_SI702X_TASK_STATE_INIT_SB_REQ :
            txBuf [0] = 0xFC;
            txBuf [1] = 0xC9;
            if (gmosDriverIicIoTransfer (
                iicInterface, txBuf, rxBuf, 2, 6)) {
                nextState = GMOS_SENSOR_SI702X_TASK_STATE_INIT_SB_POLL;
                taskStatus = GMOS_TASK_SUSPEND;
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (10));
            }
            break;

        // Poll for completion of the serial number block B request.
        case GMOS_SENSOR_SI702X_TASK_STATE_INIT_SB_POLL :
            iicStatus = gmosDriverIicIoComplete (iicInterface, NULL);
            if (iicStatus == GMOS_DRIVER_IIC_STATUS_SUCCESS) {
                if (gmosSensorSi702xCheckSerialB (sensorData)) {
                    nextState = GMOS_SENSOR_SI702X_TASK_STATE_INIT_CR_SET_REQ;
                } else {
                    nextPhase = GMOS_SENSOR_SI702X_TASK_PHASE_FAILED;
                }
            } else if (iicStatus == GMOS_DRIVER_IIC_STATUS_ACTIVE) {
                taskStatus = GMOS_TASK_SUSPEND;
            } else {
                nextPhase = GMOS_SENSOR_SI702X_TASK_PHASE_FAILED;
            }
            break;

        // Set the control register to select the appropriate sample
        // resolutions.
        case GMOS_SENSOR_SI702X_TASK_STATE_INIT_CR_SET_REQ :
            txBuf [0] = 0xE6;
            txBuf [1] = 0x3A |
                (GMOS_CONFIG_SENSOR_SI702X_RESOLUTION_CODE & 0x81);
            if (gmosDriverIicIoWrite (iicInterface, txBuf, 2)) {
                nextState = GMOS_SENSOR_SI702X_TASK_STATE_INIT_CR_SET_POLL;
                taskStatus = GMOS_TASK_SUSPEND;
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (10));
            }
            break;

        // Poll for completion of the sample resolution set request.
        case GMOS_SENSOR_SI702X_TASK_STATE_INIT_CR_SET_POLL :
            iicStatus = gmosDriverIicIoComplete (iicInterface, NULL);
            if (iicStatus == GMOS_DRIVER_IIC_STATUS_SUCCESS) {
                nextState = GMOS_SENSOR_SI702X_TASK_STATE_INIT_CR_GET_REQ;
            } else if (iicStatus == GMOS_DRIVER_IIC_STATUS_ACTIVE) {
                taskStatus = GMOS_TASK_SUSPEND;
            } else {
                nextPhase = GMOS_SENSOR_SI702X_TASK_PHASE_FAILED;
            }
            break;

        // Get the control register to verify the prior write request.
        case GMOS_SENSOR_SI702X_TASK_STATE_INIT_CR_GET_REQ :
            txBuf [0] = 0xE7;
            if (gmosDriverIicIoTransfer (
                iicInterface, txBuf, rxBuf, 1, 1)) {
                nextState = GMOS_SENSOR_SI702X_TASK_STATE_INIT_CR_GET_POLL;
                taskStatus = GMOS_TASK_SUSPEND;
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (10));
            }
            break;

        // Poll for completion of the control register read request.
        case GMOS_SENSOR_SI702X_TASK_STATE_INIT_CR_GET_POLL :
            iicStatus = gmosDriverIicIoComplete (iicInterface, NULL);
            if (iicStatus == GMOS_DRIVER_IIC_STATUS_SUCCESS) {
                if (rxBuf [0] == (0x3A |
                    (GMOS_CONFIG_SENSOR_SI702X_RESOLUTION_CODE & 0x81))) {
                    nextState = GMOS_SENSOR_SI702X_TASK_STATE_INIT_RELEASE;
                } else {
                    nextPhase = GMOS_SENSOR_SI702X_TASK_PHASE_FAILED;
                }
            } else if (iicStatus == GMOS_DRIVER_IIC_STATUS_ACTIVE) {
                taskStatus = GMOS_TASK_SUSPEND;
            } else {
                nextPhase = GMOS_SENSOR_SI702X_TASK_PHASE_FAILED;
            }
            break;

        // Release the IIC device at the end of initialisation.
        case GMOS_SENSOR_SI702X_TASK_STATE_INIT_RELEASE :
            if (gmosDriverIicDeviceRelease (iicInterface, iicDevice)) {
                nextPhase = GMOS_SENSOR_SI702X_TASK_PHASE_IDLE;
                nextState = GMOS_SENSOR_SI702X_TASK_STATE_INIT_IDLE;
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (500));
            }
            break;

        // Suspend further processing on failure.
        default :
            nextPhase = GMOS_SENSOR_SI702X_TASK_PHASE_FAILED;
            break;
    }
    sensorData->sensorPhase = nextPhase;
    sensorData->sensorState = nextState;
    return taskStatus;
}

/*
 * Process temperature samples in the receive buffer.
 */
static inline bool gmosSensorSi702xProcessTemperature (
    gmosSensorSi702x_t* sensorData)
{
    uint8_t* rxBuf = sensorData->rxBuffer;
    int32_t value;
    bool processedOk = true;

    // Update the timestamp for the next temperature sample.
    sensorData->timestampTemp = gmosPalGetTimer () +
        GMOS_MS_TO_TICKS (((uint32_t) sensorData->intervalTemp) * 1000);

    // The temperature is calculated in units of 1/1000 of a degree
    // Celsius.
    value = (((uint32_t) rxBuf [0]) << 8) + ((uint32_t) rxBuf [1]);
    value = ((value * 21965L) >> 13) - 46850;
    GMOS_LOG_FMT (LOG_VERBOSE,
        "Si702x temperature is %d/1000 C.", value);

    // Submit the temperature reading to the sensor feed.
    if (sensorData->sensorFeed != NULL) {
        processedOk = gmosSensorFeedWriteScalar (
            sensorData->sensorFeed, sensorData->sensorId,
            value, GMOS_SENSOR_UNITS_CELSIUS, -3);
    }
    return processedOk;
}

/*
 * Process relative humidity samples in the receive buffer.
 */
static inline bool gmosSensorSi702xProcessHumidity (
    gmosSensorSi702x_t* sensorData)
{
    uint8_t* rxBuf = sensorData->rxBuffer;
    int32_t value;
    bool processedOk = true;

    // Update the timestamp for the next humidity sample.
    sensorData->timestampHygro = gmosPalGetTimer () +
        GMOS_MS_TO_TICKS (((uint32_t) sensorData->intervalHygro) * 1000);

    // The humidity is calculated in units of 1/1000 of a percentage
    // point.
    value = (((uint32_t) rxBuf [0]) << 8) + ((uint32_t) rxBuf [1]);
    value = ((value * 15625L) >> 13) - 6000;
    value = (value < 0) ? 0 : (value > 100000) ? 100000 : value;
    GMOS_LOG_FMT (LOG_VERBOSE,
        "Si702x humidity is %d/1000 %%.", value);

    // Submit the humidity reading to the sensor feed.
    if (sensorData->sensorFeed != NULL) {
        processedOk = gmosSensorFeedWriteScalar (
            sensorData->sensorFeed, sensorData->sensorId,
            value, GMOS_SENSOR_UNITS_REL_HUMIDITY, -5);
    }
    return processedOk;
}

/*
 * Implement the sampling phase of the sensor state machine.
 */
static inline gmosTaskStatus_t gmosSensorSi702xSampling (
    gmosSensorSi702x_t* sensorData)
{
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    gmosDriverIicBus_t* iicInterface = sensorData->iicInterface;
    gmosDriverIicDevice_t* iicDevice = &(sensorData->iicDevice);
    gmosDriverIicStatus_t iicStatus;
    uint8_t nextPhase = sensorData->sensorPhase;
    uint8_t nextState = sensorData->sensorState;
    uint8_t* txBuf = sensorData->txBuffer;
    uint8_t* rxBuf = sensorData->rxBuffer;

    // Implement sampling phase state machine.
    switch (sensorData->sensorState) {

        // From the idle state attempt to select the sensor device.
        case GMOS_SENSOR_SI702X_TASK_STATE_SAMPLE_IDLE :
            if (gmosDriverIicDeviceSelect (iicInterface, iicDevice)) {
                nextState = GMOS_SENSOR_SI702X_TASK_STATE_SAMPLE_REQ;
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (500));
            }
            break;

        // Issue the sample request using hold master mode, which blocks
        // further transactions on the bus until the conversion is
        // complete.
        case GMOS_SENSOR_SI702X_TASK_STATE_SAMPLE_REQ :
            txBuf [0] = (sensorData->sensorPhase ==
                GMOS_SENSOR_SI702X_TASK_PHASE_SAMPLE_T) ? 0xE3 : 0xE5;
            if (gmosDriverIicIoTransfer (
                iicInterface, txBuf, rxBuf, 1, 3)) {
                nextState = GMOS_SENSOR_SI702X_TASK_STATE_SAMPLE_POLL;
                taskStatus = GMOS_TASK_SUSPEND;
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (10));
            }
            break;

        // Poll for completion of the sampling process.
        case GMOS_SENSOR_SI702X_TASK_STATE_SAMPLE_POLL :
            iicStatus = gmosDriverIicIoComplete (iicInterface, NULL);
            if (iicStatus == GMOS_DRIVER_IIC_STATUS_SUCCESS) {
                nextState = GMOS_SENSOR_SI702X_TASK_STATE_SAMPLE_PROCESS;
            } else if (iicStatus == GMOS_DRIVER_IIC_STATUS_ACTIVE) {
                taskStatus = GMOS_TASK_SUSPEND;
            } else {
                nextPhase = GMOS_SENSOR_SI702X_TASK_PHASE_FAILED;
            }
            break;

        // Process the sampled value after checking the CRC. On CRC
        // failure the appropriate sample timestamp will not be updated,
        // which has the effect of forcing an immediate retry attempt.
        case GMOS_SENSOR_SI702X_TASK_STATE_SAMPLE_PROCESS :
            rxBuf [3] = gmosSensorSi702xCalculateCRC (rxBuf, 2);
            GMOS_LOG_FMT (LOG_VERBOSE,
                "Si702x Sample Bytes : %02X %02X %02X (CRC %02X)",
                rxBuf [0], rxBuf [1], rxBuf [2], rxBuf [3]);
            if (rxBuf [2] != rxBuf [3]) {
                GMOS_LOG (LOG_WARNING, "Si702x Bad Sample CRC.");
                nextState = GMOS_SENSOR_SI702X_TASK_STATE_SAMPLE_RELEASE;
            } else {
                nextState = GMOS_SENSOR_SI702X_TASK_STATE_SAMPLE_QUEUE;
            }
            break;

        // Queue the sample to the sensor data feed.
        case GMOS_SENSOR_SI702X_TASK_STATE_SAMPLE_QUEUE :
            if (sensorData->sensorPhase ==
                GMOS_SENSOR_SI702X_TASK_PHASE_SAMPLE_T) {
                if (gmosSensorSi702xProcessTemperature (sensorData)) {
                    nextState = GMOS_SENSOR_SI702X_TASK_STATE_SAMPLE_RELEASE;
                }
            } else {
                if (gmosSensorSi702xProcessHumidity (sensorData)) {
                    nextState = GMOS_SENSOR_SI702X_TASK_STATE_SAMPLE_RELEASE;
                }
            }
            break;

        // Release the device after processing the sample.
        case GMOS_SENSOR_SI702X_TASK_STATE_SAMPLE_RELEASE :
            if (gmosDriverIicDeviceRelease (iicInterface, iicDevice)) {
                nextPhase = GMOS_SENSOR_SI702X_TASK_PHASE_IDLE;
                nextState = GMOS_SENSOR_SI702X_TASK_STATE_SAMPLE_IDLE;
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (500));
            }
            break;

        // Suspend further processing on failure.
        default :
            nextPhase = GMOS_SENSOR_SI702X_TASK_PHASE_FAILED;
            break;
    }
    sensorData->sensorPhase = nextPhase;
    sensorData->sensorState = nextState;
    return taskStatus;
}

/*
 * Implement the heating phase of the sensor state machine.
 */
static inline gmosTaskStatus_t gmosSensorSi702xHeating (
    gmosSensorSi702x_t* sensorData)
{
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    gmosDriverIicBus_t* iicInterface = sensorData->iicInterface;
    gmosDriverIicDevice_t* iicDevice = &(sensorData->iicDevice);
    gmosDriverIicStatus_t iicStatus;
    uint8_t nextPhase = sensorData->sensorPhase;
    uint8_t nextState = sensorData->sensorState;
    uint8_t* txBuf = sensorData->txBuffer;
    uint32_t timerValue = gmosPalGetTimer ();
    int32_t delay;

    // Implement heating phase state machine.
    switch (sensorData->sensorState) {

        // From the idle state attempt to select the sensor device.
        case GMOS_SENSOR_SI702X_TASK_STATE_HEATER_IDLE :
            if (gmosDriverIicDeviceSelect (iicInterface, iicDevice)) {
                nextState = GMOS_SENSOR_SI702X_TASK_STATE_HEATER_LVL_REQ;
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (500));
            }
            break;

        // Set the heater register to select the power output level.
        case GMOS_SENSOR_SI702X_TASK_STATE_HEATER_LVL_REQ :
            txBuf [0] = 0x51;
            txBuf [1] = sensorData->heatingLevel & 0x0F;
            if (gmosDriverIicIoWrite (iicInterface, txBuf, 2)) {
                nextState = GMOS_SENSOR_SI702X_TASK_STATE_HEATER_LVL_POLL;
                taskStatus = GMOS_TASK_SUSPEND;
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (10));
            }
            break;

        // Poll for completion of the heater register set request.
        case GMOS_SENSOR_SI702X_TASK_STATE_HEATER_LVL_POLL :
            iicStatus = gmosDriverIicIoComplete (iicInterface, NULL);
            if (iicStatus == GMOS_DRIVER_IIC_STATUS_SUCCESS) {
                nextState = GMOS_SENSOR_SI702X_TASK_STATE_HEATER_EN_REQ;
            } else if (iicStatus == GMOS_DRIVER_IIC_STATUS_ACTIVE) {
                taskStatus = GMOS_TASK_SUSPEND;
            } else {
                nextPhase = GMOS_SENSOR_SI702X_TASK_PHASE_FAILED;
            }
            break;

        // Set the control register to enable the heater.
        case GMOS_SENSOR_SI702X_TASK_STATE_HEATER_EN_REQ :
            txBuf [0] = 0xE6;
            txBuf [1] = 0x3E |
                (GMOS_CONFIG_SENSOR_SI702X_RESOLUTION_CODE & 0x81);
            if (gmosDriverIicIoWrite (iicInterface, txBuf, 2)) {
                nextState = GMOS_SENSOR_SI702X_TASK_STATE_HEATER_EN_POLL;
                taskStatus = GMOS_TASK_SUSPEND;
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (10));
            }
            break;

        // Poll for completion of the heater enable request.
        case GMOS_SENSOR_SI702X_TASK_STATE_HEATER_EN_POLL :
            iicStatus = gmosDriverIicIoComplete (iicInterface, NULL);
            if (iicStatus == GMOS_DRIVER_IIC_STATUS_SUCCESS) {
                sensorData->timestampHeater = timerValue +
                    GMOS_MS_TO_TICKS (sensorData->heatingPeriod * 1000);
                nextState = GMOS_SENSOR_SI702X_TASK_STATE_HEATER_STARTED;
            } else if (iicStatus == GMOS_DRIVER_IIC_STATUS_ACTIVE) {
                taskStatus = GMOS_TASK_SUSPEND;
            } else {
                nextPhase = GMOS_SENSOR_SI702X_TASK_PHASE_FAILED;
            }
            break;

        // Release the I2C bus while the heater is running.
        case GMOS_SENSOR_SI702X_TASK_STATE_HEATER_STARTED :
            if (gmosDriverIicDeviceRelease (iicInterface, iicDevice)) {
                nextState = GMOS_SENSOR_SI702X_TASK_STATE_HEATER_RUNNING;
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (500));
            }
            break;

        // Run the timer for the specified time.
        case GMOS_SENSOR_SI702X_TASK_STATE_HEATER_RUNNING :
            delay = (int32_t) (sensorData->timestampHeater - timerValue);
            if (delay <= 0) {
                nextState = GMOS_SENSOR_SI702X_TASK_STATE_HEATER_STOP;
            } else {
                taskStatus = GMOS_TASK_RUN_LATER ((uint32_t) delay);
            }
            break;

        // Claim the I2C bus to disable the heater.
        case GMOS_SENSOR_SI702X_TASK_STATE_HEATER_STOP :
            if (gmosDriverIicDeviceSelect (iicInterface, iicDevice)) {
                nextState = GMOS_SENSOR_SI702X_TASK_STATE_HEATER_NEN_REQ;
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (500));
            }
            break;

        // Set the control register to disable the heater.
        case GMOS_SENSOR_SI702X_TASK_STATE_HEATER_NEN_REQ :
            txBuf [0] = 0xE6;
            txBuf [1] = 0x3A |
                (GMOS_CONFIG_SENSOR_SI702X_RESOLUTION_CODE & 0x81);
            if (gmosDriverIicIoWrite (iicInterface, txBuf, 2)) {
                nextState = GMOS_SENSOR_SI702X_TASK_STATE_HEATER_NEN_POLL;
                taskStatus = GMOS_TASK_SUSPEND;
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (10));
            }
            break;

        // Poll for completion of the heater disable request.
        case GMOS_SENSOR_SI702X_TASK_STATE_HEATER_NEN_POLL :
            iicStatus = gmosDriverIicIoComplete (iicInterface, NULL);
            if (iicStatus == GMOS_DRIVER_IIC_STATUS_SUCCESS) {
                sensorData->timestampHeater = timerValue +
                    GMOS_MS_TO_TICKS (sensorData->heatingCooldown * 1000);
                nextState = GMOS_SENSOR_SI702X_TASK_STATE_HEATER_RELEASE;
            } else if (iicStatus == GMOS_DRIVER_IIC_STATUS_ACTIVE) {
                taskStatus = GMOS_TASK_SUSPEND;
            } else {
                nextPhase = GMOS_SENSOR_SI702X_TASK_PHASE_FAILED;
            }
            break;

        // Release the I2C bus prior to cooldown.
        case GMOS_SENSOR_SI702X_TASK_STATE_HEATER_RELEASE :
            if (gmosDriverIicDeviceRelease (iicInterface, iicDevice)) {
                nextState = GMOS_SENSOR_SI702X_TASK_STATE_HEATER_COOLDOWN;
            } else {
                taskStatus = GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (500));
            }
            break;

        // Implement the cooldown delay.
        case GMOS_SENSOR_SI702X_TASK_STATE_HEATER_COOLDOWN :
            delay = (int32_t) (sensorData->timestampHeater - timerValue);
            if (delay <= 0) {
                sensorData->timestampHeater = timerValue +
                    GMOS_MS_TO_TICKS (sensorData->intervalHeater * 1000);
                nextPhase = GMOS_SENSOR_SI702X_TASK_PHASE_IDLE;
                nextState = GMOS_SENSOR_SI702X_TASK_STATE_HEATER_IDLE;
            } else {
                taskStatus = GMOS_TASK_RUN_LATER ((uint32_t) delay);
            }
            break;

        // Suspend further processing on failure.
        default :
            nextPhase = GMOS_SENSOR_SI702X_TASK_PHASE_FAILED;
            break;
    }
    sensorData->sensorPhase = nextPhase;
    sensorData->sensorState = nextState;
    return taskStatus;
}

/*
 * Implement the scheduling phase of the sensor state machine.
 */
static inline gmosTaskStatus_t gmosSensorSi702xSchedule (
    gmosSensorSi702x_t* sensorData)
{
    gmosTaskStatus_t taskStatus = GMOS_TASK_SUSPEND;
    uint32_t timerValue = gmosPalGetTimer ();
    int32_t delay;

    // Determine if a heating cycle is scheduled. This has the lowest
    // priority.
    if (sensorData->intervalHeater > 0) {
        delay = (int32_t) (sensorData->timestampHeater - timerValue);
        if (delay <= 0) {
            sensorData->sensorPhase =
                GMOS_SENSOR_SI702X_TASK_PHASE_HEATING;
            taskStatus = GMOS_TASK_RUN_IMMEDIATE;
        } else {
            taskStatus = gmosSchedulerPrioritise (
                taskStatus, GMOS_TASK_RUN_LATER ((uint32_t) delay));
        }
    }

    // Determine if a temperature sample is scheduled. This will
    // override heating cycles.
    if (sensorData->intervalTemp > 0) {
        delay = (int32_t) (sensorData->timestampTemp - timerValue);
        if (delay <= 0) {
            sensorData->sensorPhase =
                GMOS_SENSOR_SI702X_TASK_PHASE_SAMPLE_T;
            taskStatus = GMOS_TASK_RUN_IMMEDIATE;
        } else {
            taskStatus = gmosSchedulerPrioritise (
                taskStatus, GMOS_TASK_RUN_LATER ((uint32_t) delay));
        }
    }

    // Determine if a humidity sample is scheduled. This will override
    // temperature samples.
    if (sensorData->intervalHygro > 0) {
        delay = (int32_t) (sensorData->timestampHygro - timerValue);
        if (delay <= 0) {
            sensorData->sensorPhase =
                GMOS_SENSOR_SI702X_TASK_PHASE_SAMPLE_H;
            taskStatus = GMOS_TASK_RUN_IMMEDIATE;
        } else {
            taskStatus = gmosSchedulerPrioritise (
                taskStatus, GMOS_TASK_RUN_LATER ((uint32_t) delay));
        }
    }
    return taskStatus;
}

/*
 * Implement the IIC sensor state machine task.
 */
static inline gmosTaskStatus_t gmosSensorSi702xTaskFn (
    gmosSensorSi702x_t* sensorData)
{
    gmosTaskStatus_t taskStatus;

    // Select the current operating phase for the sensor state machine.
    switch (sensorData->sensorPhase) {

        // Run the initialisation state machine.
        case GMOS_SENSOR_SI702X_TASK_PHASE_INIT :
            taskStatus = gmosSensorSi702xStartup (sensorData);
            break;

        // From the idle state, check for scheduled sample requests.
        case GMOS_SENSOR_SI702X_TASK_PHASE_IDLE :
            taskStatus = gmosSensorSi702xSchedule (sensorData);
            break;

        // Process temperature and humidity sample requests.
        case GMOS_SENSOR_SI702X_TASK_PHASE_SAMPLE_T :
        case GMOS_SENSOR_SI702X_TASK_PHASE_SAMPLE_H :
            taskStatus = gmosSensorSi702xSampling (sensorData);
            break;

        // Process sensor heating cycle requests.
        case GMOS_SENSOR_SI702X_TASK_PHASE_HEATING :
            taskStatus = gmosSensorSi702xHeating (sensorData);
            break;

        // Suspend operation on failure.
        default :
            GMOS_LOG (LOG_ERROR, "Si702x Driver Failed.");
            taskStatus = GMOS_TASK_SUSPEND;
            break;
    }
    return taskStatus;
}

// Define the sensor processing task.
GMOS_TASK_DEFINITION (
    gmosSensorSi702xTask, gmosSensorSi702xTaskFn, gmosSensorSi702x_t)

/*
 * Initialise the IIC sensor device driver on startup.
 */
bool gmosSensorSi702xInit (gmosSensorSi702x_t* sensorData,
    gmosDriverIicBus_t* iicInterface, gmosSensorFeed_t* sensorFeed,
    uint8_t sensorId)
{
    gmosTaskState_t* sensorTask = &(sensorData->sensorTask);

    // Set up the IIC device data structure.
    sensorData->iicInterface = iicInterface;
    sensorData->sensorFeed = sensorFeed;
    sensorData->sensorId = sensorId;
    if (!gmosDriverIicDeviceInit (&(sensorData->iicDevice),
        &(sensorData->sensorTask), GMOS_SENSOR_SI702X_IIC_ADDR)) {
        return false;
    }

    // Sampling and periodic heating is disabled on initialisation.
    sensorData->timestampTemp = 0;
    sensorData->timestampHygro = 0;
    sensorData->timestampHeater = 0;
    sensorData->intervalTemp = 0;
    sensorData->intervalHygro = 0;
    sensorData->intervalHeater = 0;
    sensorData->heatingPeriod = 0;
    sensorData->heatingCooldown = 0;
    sensorData->heatingLevel = 0;

    // Initialise the state machine task.
    sensorData->sensorPhase = GMOS_SENSOR_SI702X_TASK_PHASE_INIT;
    sensorData->sensorState = GMOS_SENSOR_SI702X_TASK_STATE_INIT_IDLE;
    gmosSensorSi702xTask_start (
        sensorTask, sensorData, "Si702x Driver Task");
    return true;
}

/*
 * Set the sensor configuration options.
 */
void gmosSensorSi702xSetSensorConfig (gmosSensorSi702x_t* sensorData,
    uint16_t tempInterval, uint16_t hygroInterval)
{
    uint32_t timestamp = gmosPalGetTimer ();

    // Set the sensor timestamps for immediate triggering.
    sensorData->timestampTemp = timestamp;
    sensorData->timestampHygro = timestamp;

    // Set up the sensor configuration.
    sensorData->intervalTemp = tempInterval;
    sensorData->intervalHygro = hygroInterval;

    // Resume the sensor task if it is suspended in the idle state.
    if (sensorData->sensorPhase == GMOS_SENSOR_SI702X_TASK_PHASE_IDLE) {
        gmosSchedulerTaskResume (&(sensorData->sensorTask));
    }
}

/*
 * Set the heater configuration options.
 */
void gmosSensorSi702xSetHeaterConfig (gmosSensorSi702x_t* sensorData,
    uint16_t heaterInterval, uint16_t heaterPeriod,
    uint16_t heaterCooldown, uint8_t heaterLevel)
{
    uint32_t timestamp = gmosPalGetTimer ();

    // Set the heater timestamp for immediate triggering.
    sensorData->timestampHeater = timestamp;

    // Set up the heater configuration.
    sensorData->intervalHeater = heaterInterval;
    sensorData->heatingPeriod = heaterPeriod;
    sensorData->heatingCooldown = heaterCooldown;
    sensorData->heatingLevel = heaterLevel;

    // Resume the sensor task if it is suspended in the idle state.
    if (sensorData->sensorPhase == GMOS_SENSOR_SI702X_TASK_PHASE_IDLE) {
        gmosSchedulerTaskResume (&(sensorData->sensorTask));
    }
}
