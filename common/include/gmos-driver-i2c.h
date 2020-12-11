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
 * This header defines the common API for accessing peripheral devices
 * connected to the microcontroller using the I2C bus.
 */

#ifndef GMOS_DRIVER_I2C_H
#define GMOS_DRIVER_I2C_H

#include <stdint.h>
#include <stdbool.h>

#include "gmos-config.h"
#include "gmos-scheduler.h"
#include "gmos-streams.h"
#include "gmos-events.h"

/**
 * This enumeration specifies the I2C status values that are returned by
 * the transaction completion functions.
 */
typedef enum {
    GMOS_DRIVER_I2C_STATUS_IDLE,
    GMOS_DRIVER_I2C_STATUS_SUCCESS,
    GMOS_DRIVER_I2C_STATUS_NACK,
    GMOS_DRIVER_I2C_STATUS_READING,
    GMOS_DRIVER_I2C_STATUS_WRITING,
    GMOS_DRIVER_I2C_STATUS_OVERFLOW,
    GMOS_DRIVER_I2C_STATUS_BUS_ERROR,
    GMOS_DRIVER_I2C_STATUS_DRIVER_ERROR
} gmosDriverI2CStatus_t;

/*
 * This set of definitions specify the event bit masks used to indicate
 * transaction completion status from the platform abstraction layer
 * driver.
 */
#define GMOS_DRIVER_I2C_EVENT_STATUS_OFFSET   0
#define GMOS_DRIVER_I2C_EVENT_SIZE_OFFSET     8
#define GMOS_DRIVER_I2C_EVENT_STATUS_MASK     0x000000FF
#define GMOS_DRIVER_I2C_EVENT_SIZE_MASK       0x0000FF00
#define GMOS_DRIVER_I2C_EVENT_COMPLETION_FLAG 0x80000000

/**
 * Defines the platform specific I2C bus state data structure. The full
 * type definition must be provided by the associated platform
 * abstraction layer.
 */
typedef struct gmosPalI2CBusState_t gmosPalI2CBusState_t;

/**
 * Defines the platform specific I2C bus configuration options. The full
 * type definition must be provided by the associated platform
 * abstraction layer.
 */
typedef struct gmosPalI2CBusConfig_t gmosPalI2CBusConfig_t;

/**
 * Defines the GubbinsMOS I2C device state data structure that is used
 * for managing a single I2C device connected to an associated I2C bus.
 */
typedef struct gmosDriverI2CDevice_t {

    // This is the GMOS stream that is to be used for sending requests
    // from the I2C device driver to the I2C bus.
    gmosStream_t txStream;

    // This is the GMOS stream that is to be used by the I2C device
    // driver for receiving responses from the I2C bus.
    gmosStream_t rxStream;

    // This is a pointer to the next I2C device in the bus device list.
    struct gmosDriverI2CDevice_t* nextDevice;

    // This is the current internal device state.
    uint8_t deviceState;

    // This is the address of the device on the I2C bus.
    uint8_t address;

} gmosDriverI2CDevice_t;

/**
 * Defines the GubbinsMOS I2C bus state data structure that is used for
 * managing a single I2C bus controller and the devices attached to it.
 */
typedef struct gmosDriverI2CBus_t {

    // This is an opaque pointer to the I2C platform abstraction layer
    // data structure that is used for accessing the I2C bus controller
    // hardware. The data structure will be platform specific.
    gmosPalI2CBusState_t* platformData;

    // This is an opaque pointer to the I2C platform abstraction layer
    // configuration data structure that is used for setting up the
    // I2C bus controller hardware. The data structure will be platform
    // specific.
    const gmosPalI2CBusConfig_t* platformConfig;

    // This is a pointer to the start of the I2C device list that is
    // used to connect individual I2C device drivers to the bus driver.
    gmosDriverI2CDevice_t* devices;

    // This is a pointer to the currently active I2C device data.
    gmosDriverI2CDevice_t* currentDevice;

    // This is the task state data structure for the I2C bus controller.
    gmosTaskState_t taskState;

    // This is the set of event flags that are used by the platform
    // abstraction layer to signal completion of an I2C transaction.
    gmosEvent_t completionEvent;

    // This is the current internal bus state.
    uint8_t busState;

    // The number of bytes to write in the current transaction.
    uint8_t writeSize;

    // The number of bytes to read in the current transaction.
    uint8_t readSize;

    // This is the buffer used for low level data transfers.
    uint8_t dataBuffer [GMOS_CONFIG_I2C_BUFFER_SIZE];

} gmosDriverI2CBus_t;

/**
 * Initialises a GubbinsMOS I2C bus controller. This should be called
 * exactly once for each bus controller instance prior to using any
 * other I2C driver functions.
 * @param busController This is the I2C bus controller data structure
 *     that is to be initialised.
 * @param platformData This is the I2C platform abstraction layer data
 *     structure that is to be used for accessing the I2C bus controller
 *     hardware.
 * @param platformConfig This is a platform specific bus configuration
 *     data structure that defines a set of fixed configuration options
 *     to be used with the I2C bus. This can include information such as
 *     GPIO mappings.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully completing the initialisation process and 'false'
 *     otherwise.
 */
bool gmosDriverI2CBusInit (gmosDriverI2CBus_t* busController,
    gmosPalI2CBusState_t* platformData,
    const gmosPalI2CBusConfig_t* platformConfig);

/**
 * Attaches a GubbinsMOS I2C device driver to the specified I2C bus
 * controller. This should be called exactly once for each I2C device
 * driver instance prior to using any other I2C driver functions.
 * @param busController This is the initialised I2C bus controller that
 *     is to be used for communicating with the associated I2C device.
 * @param device This is the I2C device driver data structure that is to
 *     be registered with the I2C bus controller.
 * @param address This is the I2C bus address to be used when accessing
 *     the I2C device.
 * @param clientTask This is the driver client task which will be
 *     automatically resumed on completion of a device transaction.
 *     A null reference may be passed if no client task is to be
 *     specified.
 */
void gmosDriverI2CBusAddDevice (gmosDriverI2CBus_t* busController,
    gmosDriverI2CDevice_t* device, uint8_t address,
    gmosTaskState_t* clientTask);

/**
 * Initiates an I2C write request for the specified I2C device.
 * @param device This is the I2C device driver data structure that is
 *     to be used for initiating the write request.
 * @param writeData This is a pointer to a buffer containing the data
 *     to be written to the I2C device.
 * @param writeSize This is the length of the data buffer containing the
 *     data to be written to the I2C device.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully submitting the write request and 'false' otherwise.
 */
bool gmosDriverI2CWriteRequest (gmosDriverI2CDevice_t* device,
    uint8_t* writeData, uint8_t writeSize);

/**
 * Polls the I2C device driver for completion of a write transaction.
 * @param device This is the I2C device driver data structure that is
 *     to be polled for write request completion.
 * @param writeSize This is a pointer to an 8-bit integer value which
 *     will be populated with the number of bytes written to the I2C
 *     device on successful completion.
 * @return Returns a status value which indicates the current driver
 *     status. The write transaction will be complete when this value
 *     is no longer set to 'GMOS_DRIVER_I2C_STATUS_WRITING'.
 */
gmosDriverI2CStatus_t gmosDriverI2CWriteComplete
    (gmosDriverI2CDevice_t* device, uint8_t* writeSize);

/**
 * Initiates an I2C read request for the specified I2C device.
 * @param device This is the I2C device driver data structure that is
 *     to be used for initiating the read request.
 * @param readSize This is the number of bytes that are to be requested
 *     from the I2C device.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully submitting the read request and 'false' otherwise.
 */
bool gmosDriverI2CReadRequest
    (gmosDriverI2CDevice_t* device, uint8_t readSize);

/**
 * Initiates an I2C read request prefixed by a write for the specified
 * I2C device. This will typically be used for devices that use pointer
 * indexing to select the internal register to be read.
 * @param device This is the I2C device driver data structure that is
 *     to be used for initiating the read request.
 * @param writeData This is a pointer to a buffer containing the data
 *     to be written to the I2C device.
 * @param writeSize This is the length of the data buffer containing the
 *     data to be written to the I2C device.
 * @param readSize This is the number of bytes that are to be requested
 *     from the I2C device.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully submitting the read request and 'false' otherwise.
 */
bool gmosDriverI2CIndexedReadRequest (gmosDriverI2CDevice_t* device,
    uint8_t* writeData, uint8_t writeSize, uint8_t readSize);

/**
 * Polls the I2C device driver for completion of a conventonal read or
 * an indexed read transaction.
 * @param device This is the I2C device driver data structure that is
 *     to be polled for read request completion.
 * @param readBuffer This is a pointer to the read buffer into which
 *     the read transaction data is to be copied.
 * @param readSize This is a pointer to an 8-bit integer value which
 *     on calling the function should hold the size of the read buffer.
 *     On successful completion this will be updated with the number of
 *     bytes placed in the read buffer.
 * @return Returns a status value which indicates the current driver
 *     status. The read transaction will be complete when this value
 *     is no longer set to 'GMOS_DRIVER_I2C_STATUS_READING'.
 */
gmosDriverI2CStatus_t gmosDriverI2CReadComplete
    (gmosDriverI2CDevice_t* device, uint8_t* readBuffer, uint8_t* readSize);

/**
 * Initialises the platform abstraction layer for a given I2C bus
 * configuration. Refer to the platform specific I2C implementation for
 * details of the platform data area and the bus configuration options.
 * This function is called automatically by the 'gmosDriverI2CBusInit'
 * function.
 * @param busController This is the bus controller data structure with
 *     the platform data and platform configuration entries already
 *     populated.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully completing the initialisation process and 'false'
 *     otherwise.
 */
bool gmosDriverI2CPalInit (gmosDriverI2CBus_t* busController);

/**
 * Initiates a low level I2C transfer request. After processing the
 * transaction, the transfer status will be indicated via the I2C bus
 * completion event. This function should be implemented by the platform
 * abstraction layer.
 * @param busController This is the bus controller data structure which
 *     should be used for processing the transaction. It should already
 *     be configured with the required transaction parameters.
 */
void gmosDriverI2CPalTransaction (gmosDriverI2CBus_t* busController);

#endif // GMOS_DRIVER_I2C_H
