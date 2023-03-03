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
 * This header defines the common API for accessing peripheral devices
 * connected to the microcontroller using the IIC (AKA I2C) bus.
 */

#ifndef GMOS_DRIVER_IIC_H
#define GMOS_DRIVER_IIC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "gmos-scheduler.h"
#include "gmos-events.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * This enumeration specifies the IIC status values that are returned by
 * the transaction completion functions.
 */
typedef enum {
    GMOS_DRIVER_IIC_STATUS_IDLE,
    GMOS_DRIVER_IIC_STATUS_SUCCESS,
    GMOS_DRIVER_IIC_STATUS_ACTIVE,
    GMOS_DRIVER_IIC_STATUS_NOT_READY,
    GMOS_DRIVER_IIC_STATUS_NACK,
    GMOS_DRIVER_IIC_STATUS_TIMEOUT,
    GMOS_DRIVER_IIC_STATUS_DRIVER_ERROR
} gmosDriverIicStatus_t;

/**
 * This enumeration specifies the various IIC bus operating states.
 */
typedef enum {
    GMOS_DRIVER_IIC_BUS_RESET,
    GMOS_DRIVER_IIC_BUS_ERROR,
    GMOS_DRIVER_IIC_BUS_IDLE,
    GMOS_DRIVER_IIC_BUS_SELECTED,
    GMOS_DRIVER_IIC_BUS_ACTIVE
} gmosDriverIicBusState_t;

/*
 * This set of definitions specify the event bit masks used to indicate
 * transaction completion status from the platform abstraction layer
 * driver.
 */
#define GMOS_DRIVER_IIC_EVENT_STATUS_OFFSET   0
#define GMOS_DRIVER_IIC_EVENT_SIZE_OFFSET     8
#define GMOS_DRIVER_IIC_EVENT_STATUS_MASK     0x000000FF
#define GMOS_DRIVER_IIC_EVENT_SIZE_MASK       0x00FFFF00
#define GMOS_DRIVER_IIC_EVENT_COMPLETION_FLAG 0x80000000

/**
 * Defines the platform specific IIC bus state data structure. The full
 * type definition must be provided by the associated platform
 * abstraction layer.
 */
typedef struct gmosPalIicBusState_t gmosPalIicBusState_t;

/**
 * Defines the platform specific IIC bus configuration options. The full
 * type definition must be provided by the associated platform
 * abstraction layer.
 */
typedef struct gmosPalIicBusConfig_t gmosPalIicBusConfig_t;

/**
 * Defines the GubbinsMOS IIC device information structure that is used
 * for storing the IIC bus parameters associated with a single attached
 * device.
 */
typedef struct gmosDriverIicDevice_t {

    // This is the set of event flags that are used by the platform
    // abstraction layer to signal completion of an IIC device
    // transaction.
    gmosEvent_t completionEvent;

    // This is the seven bit IIC address which is used for selecting
    // the device on the IIC bus.
    uint8_t iicAddr;

} gmosDriverIicDevice_t;

/**
 * Defines the GubbinsMOS IIC bus state data structure that is used for
 * managing the low level I/O for a single IIC bus controller.
 */
typedef struct gmosDriverIicBus_t {

    // This is an opaque pointer to the IIC platform abstraction layer
    // data structure that is used for accessing the IIC interface
    // hardware. The data structure will be platform specific.
    gmosPalIicBusState_t* palData;

    // This is an opaque pointer to the IIC platform abstraction layer
    // configuration data structure that is used for setting up the
    // IIC interface hardware. The data structure will be platform
    // specific.
    const gmosPalIicBusConfig_t* palConfig;

    // This is a pointer to the device data structure for the currently
    // active IIC device.
    gmosDriverIicDevice_t* device;

    // This is a pointer to the write data buffer to be used during an
    // IIC I/O transaction.
    uint8_t* writeData;

    // This is a pointer to the read data buffer to be used during an
    // IIC I/O transaction.
    uint8_t* readData;

    // This is the size of the write data transfer to be used during an
    // IIC I/O transaction.
    uint16_t writeSize;

    // This is the size of the read data transfer to be used during an
    // IIC I/O transaction.
    uint16_t readSize;

    // This is the current internal IIC bus state.
    uint8_t busState;

} gmosDriverIicBus_t;

/**
 * Provides a platform configuration setup macro to be used when
 * allocating an IIC driver I/O data structure. Assigning this macro to
 * an IIC driver I/O data structure on declaration will configure the
 * IIC driver to use the platform specific configuration.
 * @param _palData_ This is the IIC interface platform abstraction layer
 *     data structure that is to be used for accessing the platform
 *     specific hardware.
 * @param _palConfig_ This is a platform specific IIC interface
 *     configuration data structure that defines a set of fixed
 *     configuration options to be used with the IIC interface.
 */
#define GMOS_DRIVER_IIC_PAL_CONFIG(_palData_, _palConfig_)             \
    { _palData_, _palConfig_, NULL, NULL, NULL, 0, 0, 0 }

/**
 * Initialises an IIC bus interface data structure and initiates the
 * platform specific IIC hardware setup process.
 * @param iicInterface This is the IIC interface data structure which
 *     is to be initialised.
 */
bool gmosDriverIicBusInit (gmosDriverIicBus_t* iicInterface);

/**
 * Initialises an IIC device data structure with the specified IIC
 * protocol parameters.
 * @param iicDevice This is the IIC device data structure that is to be
 *     initialised.
 * @param clientTask This is the client task which is to be notified
 *     on completion of IIC I/O transactions.
 * @param iicAddr This is the dedicated address for the IIC device.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully completing the initialisation process and 'false'
 *     otherwise.
 */
bool gmosDriverIicDeviceInit (gmosDriverIicDevice_t* iicDevice,
    gmosTaskState_t* clientTask, uint8_t iicAddr);

/**
 * Selects an IIC device peripheral connected to the IIC bus. This
 * sets the device specific IIC address ready to intiate the first
 * transaction. The scheduler is automatically prevented from entering
 * low power mode while the IIC bus is active.
 * @param iicInterface This is the IIC interface data structure which
 *     is associated with the IIC bus.
 * @param iicDevice This is the IIC device data structure which is
 *     associated with the device being accessed.
 * @return Returns a boolean value which will be set to 'true' if the
 *     IIC bus was idle and has now been selected and 'false' otherwise.
 */
bool gmosDriverIicDeviceSelect (gmosDriverIicBus_t* iicInterface,
    gmosDriverIicDevice_t* iicDevice);

/**
 * Releases an IIC device peripheral connected to the IIC bus.
 * @param iicInterface This is the IIC interface data structure which
 *     is associated with the IIC bus.
 * @param iicDevice This is the IIC device data structure which is
 *     associated with the device being accessed.
 * @return Returns a boolean value which will be set to 'true' if the
 *     IIC device was selected and has now been deselected and 'false'
 *     otherwise.
 */
bool gmosDriverIicDeviceRelease (gmosDriverIicBus_t* iicInterface,
    gmosDriverIicDevice_t* iicDevice);

/**
 * Initiates an IIC write request for a device peripheral connected to
 * the IIC interface. The IIC device must already have been selected
 * using 'gmosDriverIicDeviceSelect'. On completion the number of bytes
 * transferred will be indicated via the device completion event.
 * @param iicInterface This is the IIC state data structure which is
 *     associated with the IIC bus.
 * @param writeData This is a pointer to the byte array that is to be
 *     written to the IIC peripheral. It must remain valid for the
 *     full duration of the transaction.
 * @param writeSize This specifies the number of bytes that are to be
 *     written to the IIC peripheral.
 * @return Returns a boolean value which will be set to 'true' if the
 *     IIC write was initiated and is now active and 'false' otherwise.
 */
bool gmosDriverIicIoWrite (gmosDriverIicBus_t* iicInterface,
    uint8_t* writeData, uint16_t writeSize);

/**
 * Initiates an IIC read request for a device peripheral connected to
 * the IIC interface. The IIC device must already have been selected
 * using 'gmosDriverIICDeviceSelect'. On completion the number of bytes
 * transferred will be indicated via the device completion event.
 * @param iicInterface This is the IIC state data structure which is
 *     associated with the IIC bus.
 * @param readData This is a pointer to the byte array that will be
 *     updated with the data read from the IIC peripheral. It must
 *     remain valid for the full duration of the transaction.
 * @param readSize This specifies the number of bytes that are to be
 *     read from the IIC peripheral.
 * @return Returns a boolean value which will be set to 'true' if the
 *     IIC read was initiated and is now active and 'false' otherwise.
 */
bool gmosDriverIicIoRead (gmosDriverIicBus_t* iicInterface,
    uint8_t* readData, uint16_t readSize);

/**
 * Initiates an IIC bidirectional transfer request for a device
 * peripheral connected to the IIC interface, implemented as a write
 * immediately followed by a read. The IIC device must already have been
 * selected using 'gmosDriverIicDeviceSelect'. On completion the number
 * of bytes transferred will be indicated via the device completion
 * event.
 * @param iicInterface This is the IIC state data structure which is
 *     associated with the IIC bus.
 * @param writeData This is a pointer to the byte array that is to be
 *     written to the IIC peripheral. It must remain valid for the
 *     full duration of the transaction.
 * @param readData This is a pointer to the byte array that will be
 *     updated with the data read from the IIC device. It must remain
 *     valid for the full duration of the transaction.
 * @param writeSize This specifies the number of bytes that are to be
 *     written to the IIC peripheral.
 * @param readSize This specifies the number of bytes that are to be
 *     read from the IIC peripheral.
 * @return Returns a boolean value which will be set to 'true' if the
 *     IIC transaction was initiated and is now active and 'false'
 *     otherwise.
 */
bool gmosDriverIicIoTransfer (gmosDriverIicBus_t* iicInterface,
    uint8_t* writeData, uint8_t* readData, uint16_t writeSize,
    uint16_t readSize);

/**
 * Completes an asynchronous IIC transaction for a device peripheral
 * connected to the IIC interface.
 * @param iicInterface This is the IIC state data structure which is
 *     associated with the IIC bus.
 * @param transferSize This is a pointer to a 16-bit unsigned integer
 *     which will be populated with the number of bytes transferred
 *     during the transaction. For combined write and read transactions
 *     this will be the sum of the read and write transfer sizes. A null
 *     reference may be used to indicate that the transfer size
 *     information is not required.
 * @return Returns a driver status value which indicates the current
 *     IIC interface status. The transaction will be complete when this
 *     is no longer set to 'GMOS_DRIVER_IIC_STATUS_ACTIVE'.
 */
gmosDriverIicStatus_t gmosDriverIicIoComplete
    (gmosDriverIicBus_t* iicInterface, uint16_t* transferSize);

/**
 * Requests an inline IIC write data transfer for short transactions
 * where the overhead of setting up an asynchronous transfer is likely
 * to exceed the cost of carrying out a simple polled transaction.
 * The IIC device must already have been selected using
 * 'gmosDriverIicDeviceSelect'.
 * @param iicInterface This is the IIC state data structure which is
 *     associated with the IIC bus.
 * @param writeData This is a pointer to the byte array that is to be
 *     written to the IIC peripheral. It must remain valid for the
 *     full duration of the transaction.
 * @param writeSize This specifies the number of bytes that are to be
 *     written to the IIC peripheral.
 * @return Returns a driver status value which indicates the success or
 *     failure of the inline transfer request.
 */
gmosDriverIicStatus_t gmosDriverIicIoInlineWrite
    (gmosDriverIicBus_t* iicInterface, uint8_t* writeData,
    uint16_t writeSize);

/**
 * Requests an inline IIC read data transfer for short transactions
 * where the overhead of setting up an asynchronous transfer is likely
 * to exceed the cost of carrying out a simple polled transaction.
 * The IIC device must already have been selected using
 * 'gmosDriverIicDeviceSelect'.
 * @param iicInterface This is the IIC state data structure which is
 *     associated with the IIC bus.
 * @param readData This is a pointer to the byte array that will be
 *     updated with the data read from the IIC device. It must remain
 *     valid for the full duration of the transaction.
 * @param readSize This specifies the number of bytes that are to be
 *     read from the IIC peripheral.
 * @return Returns a driver status value which indicates the success or
 *     failure of the inline transfer request.
 */
gmosDriverIicStatus_t gmosDriverIicIoInlineRead
    (gmosDriverIicBus_t* iicInterface, uint8_t* readData,
    uint16_t readSize);

/**
 * Requests a bidirectional inline IIC data transfer for short
 * transactions where the overhead of setting up an asynchronous
 * transfer is likely to exceed the cost of carrying out a simple polled
 * transaction. The transaction is implemented as a write immediately
 * followed by a read. The IIC device must already have been selected
 * using 'gmosDriverIicDeviceSelect'.
 * @param iicInterface This is the IIC state data structure which is
 *     associated with the IIC bus.
 * @param writeData This is a pointer to the byte array that is to be
 *     written to the IIC peripheral. It must remain valid for the
 *     full duration of the transaction.
 * @param readData This is a pointer to the byte array that will be
 *     updated with the data read from the IIC device. It must remain
 *     valid for the full duration of the transaction.
 * @param writeSize This specifies the number of bytes that are to be
 *     written to the IIC peripheral.
 * @param readSize This specifies the number of bytes that are to be
 *     read from the IIC peripheral.
 * @return Returns a driver status value which indicates the success or
 *     failure of the inline transfer request.
 */
gmosDriverIicStatus_t gmosDriverIicIoInlineTransfer
    (gmosDriverIicBus_t* iicInterface, uint8_t* writeData,
    uint8_t* readData, uint16_t writeSize, uint16_t readSize);

/**
 * Initialises the platform abstraction layer for a given IIC interface.
 * Refer to the platform specific IIC implementation for details of the
 * platform data area and the IIC interface configuration options.
 * This function is called automatically by the 'gmosDriverIicBusInit'
 * function.
 * @param iicInterface This is the IIC interface data structure with
 *     the platform data and platform configuration entries already
 *     populated.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully completing the initialisation process and 'false'
 *     otherwise.
 */
bool gmosDriverIicPalInit (gmosDriverIicBus_t* iicInterface);

/**
 * Performs a platform specific IIC transaction using the given IIC
 * interface. Start, restart and stop bits are generated by the platform
 * specific driver as required.
 * @param iicInterface This is the IIC interface data structure, which
 *     will have been configured with all the parameters required to
 *     initiate the IIC transaction.
 */
void gmosDriverIicPalTransaction (gmosDriverIicBus_t* iicInterface);

/**
 * Performs a platform specific IIC inline transaction using the given
 * IIC interface. Start, restart and stop bits are generated by the
 * platform specific driver as required.
 * @param iicInterface This is the IIC interface data structure, which
 *     will have been configured with all the parameters required to
 *     initiate the IIC transaction.
 * @return Returns a driver status value which indicates the success or
 *     failure of the inline transfer request.
 */
gmosDriverIicStatus_t gmosDriverIicPalInlineTransaction
    (gmosDriverIicBus_t* iicInterface);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // GMOS_DRIVER_IIC_H
