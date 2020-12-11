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
 * Implements the common GubbinsMOS I2C driver framework.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-scheduler.h"
#include "gmos-streams.h"
#include "gmos-events.h"
#include "gmos-driver-i2c.h"

// Defines the internal bus controller task state.
#define BUS_STATE_IDLE    0x00
#define BUS_STATE_WRITING 0x01
#define BUS_STATE_READING 0x02

// Defines the internal device state.
#define DEVICE_STATE_IDLE    0x00
#define DEVICE_STATE_WRITING 0x01
#define DEVICE_STATE_READING 0x02

/*
 * Processes a new I2C bus low level transaction request for the
 * currently selected device.
 */
static inline void gmosDriverI2CRequestHandler
    (gmosDriverI2CBus_t* busController, uint8_t* request)
{
    gmosDriverI2CDevice_t* device = busController->currentDevice;

    // Extract the write and read request sizes.
    busController->writeSize = request [0];
    busController->readSize = request [1];

    // Copy the write payload data. Should always succeed.
    if (busController->writeSize != 0) {
        gmosStreamReadAll (&(device->txStream),
            busController->dataBuffer, busController->writeSize);
    }

    // Determine if a read is expected.
    if (busController->readSize != 0) {
        busController->busState = BUS_STATE_READING;
    } else {
        busController->busState = BUS_STATE_WRITING;
    }

    // Initiate the transaction and prevent the microcontroller from
    // sleeping while the transaction is active.
    gmosDriverI2CPalTransaction (busController);
    gmosSchedulerStayAwake ();
}

/*
 * Processes the completion of a low level write transaction for the
 * currently selected device.
 */
static inline void gmosDriverI2CWriteHandler
    (gmosDriverI2CBus_t* busController, uint32_t eventBits)
{
    gmosDriverI2CDevice_t* device = busController->currentDevice;
    uint8_t eventStatus;
    uint8_t transferSize;
    uint8_t writeStatus [2];

    // Check for valid transfer size.
    transferSize = (uint8_t) (eventBits >> GMOS_DRIVER_I2C_EVENT_SIZE_OFFSET);
    if (transferSize > GMOS_CONFIG_I2C_BUFFER_SIZE) {
        transferSize = 0;
        eventStatus = GMOS_DRIVER_I2C_STATUS_DRIVER_ERROR;
    }

    // Format the write status response.
    eventStatus = (uint8_t) (eventBits >> GMOS_DRIVER_I2C_EVENT_STATUS_OFFSET);
    writeStatus [0] = eventStatus;
    writeStatus [1] = transferSize;

    // Attempt to send the write status response then revert to the
    // bus idle state.
    if (gmosStreamWriteAll (&(device->rxStream), writeStatus, 2)) {
        gmosEventClearBits (&(busController->completionEvent), 0xFFFFFFFF);
        busController->busState = BUS_STATE_IDLE;
        gmosSchedulerCanSleep ();
    }
}

/*
 * Processes the completion of a low level read transaction for the
 * currently selected device.
 */
static inline void gmosDriverI2CReadHandler
    (gmosDriverI2CBus_t* busController, uint32_t eventBits)
{
    uint16_t writeCapacity;
    uint8_t eventStatus = (uint8_t)
        (eventBits >> GMOS_DRIVER_I2C_EVENT_STATUS_OFFSET);
    uint8_t transferSize;
    uint8_t readStatus [2];
    gmosDriverI2CDevice_t* device = busController->currentDevice;

    // Check for valid transfer size.
    transferSize = (uint8_t) (eventBits >> GMOS_DRIVER_I2C_EVENT_SIZE_OFFSET);
    if (transferSize > GMOS_CONFIG_I2C_BUFFER_SIZE) {
        transferSize = 0;
        eventStatus = GMOS_DRIVER_I2C_STATUS_DRIVER_ERROR;
    }

    // Check that there is sufficient space in the device receive stream
    // to hold the complete read response.
    writeCapacity = gmosStreamGetWriteCapacity (&(device->rxStream));
    if (((uint16_t) transferSize) + 2 > writeCapacity) {
        return;
    }

    // Format the read status response.
    readStatus [0] = eventStatus;
    readStatus [1] = transferSize;

    // Send the read status. The prior capacity check ensures that this
    // will be succesful.
    gmosStreamWriteAll (&(device->rxStream), readStatus, 2);

    // Send the data from the I2C buffer. The prior capacity check
    // ensures that this will be succesful.
    gmosStreamWriteAll (&(device->rxStream),
        busController->dataBuffer, transferSize);

    // Revert to the bus idle state.
    gmosEventClearBits (&(busController->completionEvent), 0xFFFFFFFF);
    busController->busState = BUS_STATE_IDLE;
    gmosSchedulerCanSleep ();
}

/*
 * Implements the I2C bus controller task handler.
 */
static gmosTaskStatus_t gmosDriverI2CTaskHandler
    (gmosDriverI2CBus_t* busController)
{
    gmosDriverI2CDevice_t* device;
    gmosTaskStatus_t taskStatus = GMOS_TASK_SUSPEND;
    uint8_t request [2];
    uint32_t eventBits;

    switch (busController->busState) {

        // Wait for a write transaction completion event. On completion
        // run immediately to process any pending requests.
        case BUS_STATE_WRITING :
            eventBits = gmosEventGetBits (&(busController->completionEvent));
            if ((eventBits & GMOS_DRIVER_I2C_EVENT_COMPLETION_FLAG) != 0) {
                gmosDriverI2CWriteHandler (busController, eventBits);
                taskStatus = GMOS_TASK_RUN_IMMEDIATE;
            }
            break;

        // Wait for a read transaction completion event. On completion
        // run immediately to process any pending requests.
        case BUS_STATE_READING :
            eventBits = gmosEventGetBits (&(busController->completionEvent));
            if ((eventBits & GMOS_DRIVER_I2C_EVENT_COMPLETION_FLAG) != 0) {
                gmosDriverI2CReadHandler (busController, eventBits);
                taskStatus = GMOS_TASK_RUN_IMMEDIATE;
            }
            break;

        // From the idle state, scan the device drivers for an active
        // request. Suspend the task if no requests are ready.
        default :
            device = busController->devices;
            while (device != NULL) {
                if (gmosStreamReadAll (&(device->txStream), request, 2)) {
                    busController->currentDevice = device;
                    gmosDriverI2CRequestHandler (busController, request);
                    taskStatus = GMOS_TASK_RUN_IMMEDIATE;
                    break;
                } else {
                    device = device->nextDevice;
                }
            }
            break;
    }
    return taskStatus;
}

// Define the bus controller task.
GMOS_TASK_DEFINITION (gmosDriverI2CTask,
    gmosDriverI2CTaskHandler, gmosDriverI2CBus_t);

/*
 * Initialises a GubbinsMOS I2C bus controller. This should be called
 * exactly once for each bus controller instance prior to using any
 * other I2C driver functions.
 */
bool gmosDriverI2CBusInit (gmosDriverI2CBus_t* busController,
    gmosPalI2CBusState_t* platformData,
    const gmosPalI2CBusConfig_t* platformConfig)
{
    // Intialise the bus controller data.
    busController->platformData = platformData;
    busController->platformConfig = platformConfig;
    busController->devices = NULL;
    busController->currentDevice = NULL;

    // Attempt to initialise the platform specific driver.
    if (!gmosDriverI2CPalInit (busController)) {
        return false;
    }

    // Initialise the transaction complete event flags.
    gmosEventInit (&(busController->completionEvent),
        &(busController->taskState));

    // Schedule the bus controller task.
    gmosDriverI2CTask_start (&(busController->taskState),
        busController, "I2C Bus Controller");
    return true;
}

/*
 * Attaches a GubbinsMOS I2C device driver to the specified I2C bus
 * controller. This should be called exactly once for each I2C device
 * driver instance prior to using any other I2C driver functions.
 */
void gmosDriverI2CBusAddDevice (gmosDriverI2CBus_t* busController,
    gmosDriverI2CDevice_t* device, uint8_t address,
    gmosTaskState_t* clientTask)
{
    // Initialise the device state data.
    device->deviceState = DEVICE_STATE_IDLE;
    device->address = address;

    // Initialise the device data streams.
    gmosStreamInit (&(device->txStream), &(busController->taskState),
        GMOS_CONFIG_I2C_BUFFER_SIZE + 2);
    gmosStreamInit (&(device->rxStream), clientTask,
        GMOS_CONFIG_I2C_BUFFER_SIZE + 2);

    // Attach the device to the bus controller device list.
    device->nextDevice = busController->devices;
    busController->devices = device;
}

/*
 * Initiates an I2C write request for the specified I2C device.
 */
bool gmosDriverI2CWriteRequest (gmosDriverI2CDevice_t* device,
    uint8_t* writeData, uint8_t writeSize)
{
    uint16_t writeCapacity;
    uint8_t writeCommand [2];

    // Check that the driver is not currently active.
    if (device->deviceState != DEVICE_STATE_IDLE) {
        return false;
    }

    // Check that all the data can be written to the device write
    // stream. This guarantees that the specified write size will not
    // overflow the I2C data buffer.
    writeCapacity = gmosStreamGetWriteCapacity (&(device->txStream));
    if (((uint16_t) writeSize) + 2 > writeCapacity) {
        return false;
    }

    // Set the command byte and transfer size.
    writeCommand [0] = writeSize;
    writeCommand [1] = 0;

    // Write the command and data to the transmit data stream. The prior
    // capacity check ensures that this will be successful.
    gmosStreamWriteAll (&(device->txStream), writeCommand, 2);
    gmosStreamWriteAll (&(device->txStream), writeData, writeSize);
    device->deviceState = DEVICE_STATE_WRITING;
    return true;
}

/*
 * Polls the I2C device driver for completion of a write transaction.
 */
gmosDriverI2CStatus_t gmosDriverI2CWriteComplete
    (gmosDriverI2CDevice_t* device, uint8_t* writeSize)
{
    uint8_t writeResponse [2];

    // Check for inconsistent driver state.
    if (device->deviceState == DEVICE_STATE_IDLE) {
        return GMOS_DRIVER_I2C_STATUS_IDLE;
    } else if (device->deviceState == DEVICE_STATE_READING) {
        return GMOS_DRIVER_I2C_STATUS_READING;
    }

    // Check for a pending write response.
    if (!gmosStreamReadAll (&(device->rxStream), writeResponse, 2)) {
        return GMOS_DRIVER_I2C_STATUS_WRITING;
    }

    // Update the write transaction size and return the status value.
    device->deviceState = DEVICE_STATE_IDLE;
    *writeSize = writeResponse [1];
    return (gmosDriverI2CStatus_t) writeResponse [0];
}

/*
 * Initiates an I2C read request for the specified I2C device.
 */
bool gmosDriverI2CReadRequest
    (gmosDriverI2CDevice_t* device, uint8_t readSize)
{
    uint8_t readCommand [2];

    // Check that the driver is not currently active.
    if (device->deviceState != DEVICE_STATE_IDLE) {
        return false;
    }

    // Check that the requested read size is valid.
    if (readSize > GMOS_CONFIG_I2C_BUFFER_SIZE) {
        return false;
    }

    // Set the command byte and transfer size.
    readCommand [0] = 0;
    readCommand [1] = readSize;

    // Send the read command.
    if (gmosStreamWriteAll (&(device->txStream), readCommand, 2)) {
        device->deviceState = DEVICE_STATE_READING;
        return true;
    } else {
        return false;
    }
}

/*
 * Initiates an I2C indexed read request for the specified I2C device.
 */
bool gmosDriverI2CIndexedReadRequest (gmosDriverI2CDevice_t* device,
    uint8_t* writeData, uint8_t writeSize, uint8_t readSize)
{
    uint16_t writeCapacity;
    uint8_t readCommand [2];

    // Check that the driver is not currently active.
    if (device->deviceState != DEVICE_STATE_IDLE) {
        return false;
    }

    // Check that all the data can be written to the device write
    // stream. This guarantees that the specified write size will not
    // overflow the I2C data buffer.
    writeCapacity = gmosStreamGetWriteCapacity (&(device->txStream));
    if (((uint16_t) writeSize) + 2 > writeCapacity) {
        return false;
    }

    // Set the command byte and transfer size.
    readCommand [0] = writeSize;
    readCommand [1] = readSize;

    // Write the command and data to the transmit data stream. The prior
    // capacity check ensures that this will be successful.
    gmosStreamWriteAll (&(device->txStream), readCommand, 2);
    gmosStreamWriteAll (&(device->txStream), writeData, writeSize);
    device->deviceState = DEVICE_STATE_READING;
    return true;
}

/*
 * Polls the I2C device driver for completion of a read transaction.
 */
gmosDriverI2CStatus_t gmosDriverI2CReadComplete
    (gmosDriverI2CDevice_t* device, uint8_t* readBuffer, uint8_t* readSize)
{
    uint8_t readResponse [2];
    uint8_t dataSize;
    uint8_t overflowSize;
    gmosDriverI2CStatus_t status;

    // Check for inconsistent driver state.
    if (device->deviceState == DEVICE_STATE_IDLE) {
        return GMOS_DRIVER_I2C_STATUS_IDLE;
    } else if (device->deviceState == DEVICE_STATE_WRITING) {
        return GMOS_DRIVER_I2C_STATUS_WRITING;
    }

    // Check for a pending read response.
    if (!gmosStreamReadAll (&(device->rxStream), readResponse, 2)) {
        return GMOS_DRIVER_I2C_STATUS_READING;
    }

    // Update the read transaction size.
    dataSize = readResponse [1];

    // Check for read buffer overflow conditions.
    if (dataSize > *readSize) {
        overflowSize = dataSize - *readSize;
        dataSize = *readSize;
        status = GMOS_DRIVER_I2C_STATUS_OVERFLOW;
    } else {
        overflowSize = 0;
        status = (gmosDriverI2CStatus_t) readResponse [0];
    }

    // Copy the read data and drain any overflow bytes.
    device->deviceState = DEVICE_STATE_IDLE;
    gmosStreamReadAll (&(device->rxStream), readBuffer, dataSize);
    *readSize = dataSize;
    while (overflowSize > 0) {
        uint8_t overflowByte;
        overflowSize -= 1;
        gmosStreamReadByte (&(device->rxStream), &overflowByte);
    }
    return status;
}
