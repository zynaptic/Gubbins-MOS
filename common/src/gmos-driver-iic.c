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
 * Implements the common GubbinsMOS I2C driver framework.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-scheduler.h"
#include "gmos-driver-iic.h"

/*
 * Initialises an IIC bus interface data structure and initiates the
 * platform specific IIC hardware setup process.
 */
bool gmosDriverIicBusInit (gmosDriverIicBus_t* iicInterface)
{
    bool initOk = false;

    // Initialise the IIC interface platform specific hardware.
    if (iicInterface->busState == GMOS_DRIVER_IIC_BUS_RESET) {
        if (gmosDriverIicPalInit (iicInterface)) {
            iicInterface->busState = GMOS_DRIVER_IIC_BUS_IDLE;
            initOk = true;
        } else {
            iicInterface->busState = GMOS_DRIVER_IIC_BUS_ERROR;
        }
    }
    return initOk;
}

/*
 * Initialises an IIC device data structure with the specified IIC
 * protocol parameters.
 */
bool gmosDriverIicDeviceInit (gmosDriverIicDevice_t* iicDevice,
    gmosTaskState_t* clientTask, uint8_t iicAddr)
{
    // Populate the IIC device data structure.
    iicDevice->iicAddr = iicAddr;

    // Initialise the completion event data structure.
    gmosEventInit (&(iicDevice->completionEvent), clientTask);
    return true;
}

/*
 * Selects an IIC device peripheral connected to the IIC bus.
 */
bool gmosDriverIicDeviceSelect (gmosDriverIicBus_t* iicInterface,
    gmosDriverIicDevice_t* iicDevice)
{
    bool selectOk = false;
    if (iicInterface->busState == GMOS_DRIVER_IIC_BUS_IDLE) {
        iicInterface->busState = GMOS_DRIVER_IIC_BUS_SELECTED;
        iicInterface->device = iicDevice;
        gmosSchedulerStayAwake ();
        selectOk = true;
    }
    return selectOk;
}

/*
 * Releases an IIC device peripheral connected to the IIC bus.
 */
bool gmosDriverIicDeviceRelease (gmosDriverIicBus_t* iicInterface,
    gmosDriverIicDevice_t* iicDevice)
{
    bool releaseOk = false;
    if ((iicInterface->busState == GMOS_DRIVER_IIC_BUS_SELECTED) &&
        (iicInterface->device == iicDevice)) {
        iicInterface->busState = GMOS_DRIVER_IIC_BUS_IDLE;
        gmosSchedulerCanSleep ();
        releaseOk = true;
    }
    return releaseOk;
}

/*
 * Initiates an IIC write request for a device peripheral connected to
 * the IIC interface.
 */
bool gmosDriverIicIoWrite (gmosDriverIicBus_t* iicInterface,
    uint8_t* writeData, uint16_t writeSize)
{
    bool writeOk = false;
    if (iicInterface->busState == GMOS_DRIVER_IIC_BUS_SELECTED) {
        iicInterface->busState = GMOS_DRIVER_IIC_BUS_ACTIVE;
        iicInterface->writeData = writeData;
        iicInterface->readData = NULL;
        iicInterface->writeSize = writeSize;
        iicInterface->readSize = 0;
        gmosDriverIicPalTransaction (iicInterface);
        writeOk = true;
    }
    return writeOk;
}

/*
 * Initiates an IIC read request for a device peripheral connected to
 * the IIC interface.
 */
bool gmosDriverIicIoRead (gmosDriverIicBus_t* iicInterface,
    uint8_t* readData, uint16_t readSize)
{
    bool readOk = false;
    if (iicInterface->busState == GMOS_DRIVER_IIC_BUS_SELECTED) {
        iicInterface->busState = GMOS_DRIVER_IIC_BUS_ACTIVE;
        iicInterface->writeData = NULL;
        iicInterface->readData = readData;
        iicInterface->writeSize = 0;
        iicInterface->readSize = readSize;
        gmosDriverIicPalTransaction (iicInterface);
        readOk = true;
    }
    return readOk;
}

/*
 * Initiates an IIC bidirectional transfer request for a device
 * peripheral connected to the IIC interface, implemented as a write
 * immediately followed by a read.
 */
bool gmosDriverIicIoTransfer (gmosDriverIicBus_t* iicInterface,
    uint8_t* writeData, uint8_t* readData, uint16_t writeSize,
    uint16_t readSize)
{
    bool transferOk = false;
    if (iicInterface->busState == GMOS_DRIVER_IIC_BUS_SELECTED) {
        iicInterface->busState = GMOS_DRIVER_IIC_BUS_ACTIVE;
        iicInterface->writeData = writeData;
        iicInterface->readData = readData;
        iicInterface->writeSize = writeSize;
        iicInterface->readSize = readSize;
        gmosDriverIicPalTransaction (iicInterface);
        transferOk = true;
    }
    return transferOk;
}

/*
 * Completes an asynchronous IIC transaction for a device peripheral
 * connected to the IIC interface.
 */
gmosDriverIicStatus_t gmosDriverIicIoComplete
    (gmosDriverIicBus_t* iicInterface, uint16_t* transferSize)
{
    uint32_t eventBits;
    gmosEvent_t* completionEvent;
    gmosDriverIicStatus_t iicStatus = GMOS_DRIVER_IIC_STATUS_IDLE;

    // Only poll the completion event if a transaction is active.
    if (iicInterface->busState == GMOS_DRIVER_IIC_BUS_ACTIVE) {
        completionEvent = &(iicInterface->device->completionEvent);
        eventBits = gmosEventResetBits (completionEvent);
        if (eventBits != 0) {
            iicInterface->busState = GMOS_DRIVER_IIC_BUS_SELECTED;
            iicStatus = eventBits & GMOS_DRIVER_IIC_EVENT_STATUS_MASK;

            // Transfer size notifications are optional.
            if (transferSize != NULL) {
                *transferSize =
                    (eventBits & GMOS_DRIVER_IIC_EVENT_SIZE_MASK) >>
                    GMOS_DRIVER_IIC_EVENT_SIZE_OFFSET;
            }
        } else {
            iicStatus = GMOS_DRIVER_IIC_STATUS_ACTIVE;
        }
    }
    return iicStatus;
}

/*
 * Requests an inline IIC write data transfer for short transactions
 * where the overhead of setting up an asynchronous transfer is likely
 * to exceed the cost of carrying out a simple polled transaction.
 */
gmosDriverIicStatus_t gmosDriverIicIoInlineWrite
    (gmosDriverIicBus_t* iicInterface, uint8_t* writeData,
    uint16_t writeSize)
{
    gmosDriverIicStatus_t iicStatus = GMOS_DRIVER_IIC_STATUS_NOT_READY;
    if (iicInterface->busState == GMOS_DRIVER_IIC_BUS_SELECTED) {
        iicInterface->writeData = writeData;
        iicInterface->readData = NULL;
        iicInterface->writeSize = writeSize;
        iicInterface->readSize = 0;
        iicStatus = gmosDriverIicPalInlineTransaction (iicInterface);
    }
    return iicStatus;
}

/*
 * Requests an inline IIC read data transfer for short transactions
 * where the overhead of setting up an asynchronous transfer is likely
 * to exceed the cost of carrying out a simple polled transaction.
 */
gmosDriverIicStatus_t gmosDriverIicIoInlineRead
    (gmosDriverIicBus_t* iicInterface, uint8_t* readData,
    uint16_t readSize)
{
    gmosDriverIicStatus_t iicStatus = GMOS_DRIVER_IIC_STATUS_NOT_READY;
    if (iicInterface->busState == GMOS_DRIVER_IIC_BUS_SELECTED) {
        iicInterface->writeData = NULL;
        iicInterface->readData = readData;
        iicInterface->writeSize = 0;
        iicInterface->readSize = readSize;
        iicStatus = gmosDriverIicPalInlineTransaction (iicInterface);
    }
    return iicStatus;
}

/*
 * Requests a bidirectional inline IIC data transfer for short
 * transactions where the overhead of setting up an asynchronous
 * transfer is likely to exceed the cost of carrying out a simple polled
 * transaction.
 */
gmosDriverIicStatus_t gmosDriverIicIoInlineTransfer
    (gmosDriverIicBus_t* iicInterface, uint8_t* writeData,
    uint8_t* readData, uint16_t writeSize, uint16_t readSize)
{
    gmosDriverIicStatus_t iicStatus = GMOS_DRIVER_IIC_STATUS_NOT_READY;
    if (iicInterface->busState == GMOS_DRIVER_IIC_BUS_SELECTED) {
        iicInterface->writeData = writeData;
        iicInterface->readData = readData;
        iicInterface->writeSize = writeSize;
        iicInterface->readSize = readSize;
        iicStatus = gmosDriverIicPalInlineTransaction (iicInterface);
    }
    return iicStatus;
}
