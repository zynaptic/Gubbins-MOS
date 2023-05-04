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
 * This header defines the data structures and management functions for
 * the Silicon Labs Si7020 and Si7021 hygrometer and temperature
 * sensors.
 */

#ifndef GMOS_SENSOR_SI702X
#define GMOS_SENSOR_SI702X

#include <stdint.h>
#include <stdbool.h>
#include "gmos-config.h"
#include "gmos-scheduler.h"
#include "gmos-driver-iic.h"
#include "gmos-sensor-feeds.h"

/**
 * Specify the measurement resolution code to use for selecting the
 * resolution of both temperature and humidity samples. Valid settings
 * are as follows:
 *     0x00 : 12 bit RH, 14 bit temperature
 *     0x01 :  8 bit RH, 12 bit temperature
 *     0x80 : 10 bit RH, 13 bit temperature
 *     0x81 : 11 bit RH, 11 bit temperature
 * Higher resolutions imply longer sample times, which may be an issue
 * for battery powered devices. For further details see the device
 * datasheet.
 */
#ifndef GMOS_CONFIG_SENSOR_SI702X_RESOLUTION_CODE
#define GMOS_CONFIG_SENSOR_SI702X_RESOLUTION_CODE 0x00
#endif

/**
 * Specify the fixed IIC address for the Si702x devices.
 */
#define GMOS_SENSOR_SI702X_IIC_ADDR 0x40

/**
 * This structure defines the sensor state associated with a single
 * Si702x temperature and hygrometer sensor device.
 */
typedef struct gmosSensorSi702x_t {

    // Specify the IIC bus to which the device is attached.
    gmosDriverIicBus_t* iicInterface;

    // Specify the sensor feed which is to be used for distributing the
    // sensor readings.
    gmosSensorFeed_t* sensorFeed;

    // Allocate the main task data structure.
    gmosTaskState_t sensorTask;

    // Allocate the IIC device instance.
    gmosDriverIicDevice_t iicDevice;

    // Specify the timestamp for the next temperature sensor reading.
    uint32_t timestampTemp;

    // Specify the timestamp for the next hygrometer sensor reading.
    uint32_t timestampHygro;

    // Specify the timestamp for the start of the next heating cycle.
    uint32_t timestampHeater;

    // Specify the temperature sampling interval in seconds.
    uint16_t intervalTemp;

    // Specify the humidity sampling interval in seconds.
    uint16_t intervalHygro;

    // Specify the heating cycle interval in seconds.
    uint16_t intervalHeater;

    // Specify the heating cycle active period in seconds.
    uint16_t heatingPeriod;

    // Specify the heating cycle cooldown period in seconds.
    uint16_t heatingCooldown;

    // Specify the sensor ID which is to be used for associating sensor
    // readings with the sensor.
    uint8_t sensorId;

    // Specify the heating cycle level to be used.
    uint8_t heatingLevel;

    // Specify the current sensor operating phase.
    uint8_t sensorPhase;

    // Specify the current sensor operating state.
    uint8_t sensorState;

    // Allocate storage for IIC transmit buffer.
    uint8_t txBuffer [2];

    // Allocate storage for IIC receive buffer.
    uint8_t rxBuffer [8];

    // Allocate storage for the device serial number.
    uint8_t serialNumber [8];

} gmosSensorSi702x_t;

/**
 * Initialise a Si702x sensor device on startup.
 * @param sensorData This is a pointer to the sensor state data
 *     structure which corresponds to the sensor being initialised.
 * @param iicInterface This is a pointer to an initialised IIC bus
 *     interface data structure which corresponds to the IIC bus that
 *     is connected to the sensor device.
 * @param sensorFeed This is the sensor feed which will be used for
 *     distributing the sensor readings.
 * @param sensorId This is the sensor ID which will be used to associate
 *     sensor readings with the sensor.
 * @return Returns a boolean value which will be set to 'true' on
 *     successful initialisation and 'false' otherwise.
 */
bool gmosSensorSi702xInit (gmosSensorSi702x_t* sensorData,
    gmosDriverIicBus_t* iicInterface, gmosSensorFeed_t* sensorFeed,
    uint8_t sensorId);

/**
 * Set the sensor configuration options. This updates the intervals used
 * for scheduling temperature and humidity sensor readings and also
 * schedules an immediate sensor reading to start the sampling cycle.
 * @param sensorData This is a pointer to the sensor state data
 *     structure which corresponds to the sensor being configured.
 * @param tempInterval This specifies the nominal sampling interval to
 *     be used for the temperature sensor, expressed as an integer
 *     number of seconds. A value of zero may be used to disable
 *     periodic temperature sensing.
 * @param hygroInterval This specifies the nominal sampling interval to
 *     be used for the relative humidity sensor, expressed as an integer
 *     number of seconds. A value of zero may be used to disable
 *     periodic relative humidity sensing.
 */
void gmosSensorSi702xSetSensorConfig (gmosSensorSi702x_t* sensorData,
    uint16_t tempInterval, uint16_t hygroInterval);

/**
 * Set the heater configuration options. This updates the parameters
 * used for scheduling the periodic sensor heating cycles that may be
 * used to drive off accumulated condensation.
 * @param sensorData This is a pointer to the sensor state data
 *     structure which corresponds to the sensor being configured.
 * @param heaterInterval This specifies the nominal interval between
 *     heating cycles, expressed as an integer number of seconds. A
 *     value of zero may be used to disable the heater.
 * @param heaterPeriod This specifies the period for which the heater
 *     will be operated during the periodic heating cycle, expressed as
 *     an integer number of seconds.
 * @param heaterCooldown This specifies the cooldown period between
 *     turning off the heater and restarting normal sensor operation,
 *     expressed as an integer number of seconds.
 * @param heaterLevel This specifies the heating level to be used during
 *     the periodic heating cycle, encoded as an integer in the range
 *     from 0 to 15. See the data sheet for further information about
 *     how these values map to the heater current.
 */
void gmosSensorSi702xSetHeaterConfig (gmosSensorSi702x_t* sensorData,
    uint16_t heaterInterval, uint16_t heaterPeriod,
    uint16_t heaterCooldown, uint8_t heaterLevel);

#endif // GMOS_SENSOR_SI702X
