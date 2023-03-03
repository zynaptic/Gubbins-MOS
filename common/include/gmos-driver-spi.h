/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2020-2023 Zynaptic Limited
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
 * connected to the microcontroller using the SPI bus.
 */

#ifndef GMOS_DRIVER_SPI_H
#define GMOS_DRIVER_SPI_H

#include <stdint.h>
#include <stdbool.h>

#include "gmos-scheduler.h"
#include "gmos-events.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * This enumeration specifies the SPI status values that are returned by
 * the transaction completion functions.
 */
typedef enum {
    GMOS_DRIVER_SPI_STATUS_IDLE,
    GMOS_DRIVER_SPI_STATUS_SUCCESS,
    GMOS_DRIVER_SPI_STATUS_ACTIVE,
    GMOS_DRIVER_SPI_STATUS_NOT_READY,
    GMOS_DRIVER_SPI_STATUS_DMA_ERROR,
    GMOS_DRIVER_SPI_STATUS_DRIVER_ERROR
} gmosDriverSpiStatus_t;

/**
 * This enumeration specifies the various SPI bus operating states.
 */
typedef enum {
    GMOS_DRIVER_SPI_BUS_RESET,
    GMOS_DRIVER_SPI_BUS_ERROR,
    GMOS_DRIVER_SPI_BUS_IDLE,
    GMOS_DRIVER_SPI_BUS_SELECTED,
    GMOS_DRIVER_SPI_BUS_ACTIVE
} gmosDriverSpiBusState_t;

/**
 * This enumeration specifies the standard SPI bus clock modes.
 */
typedef enum {
    GMOS_DRIVER_SPI_CLOCK_MODE_0 = 0,
    GMOS_DRIVER_SPI_CLOCK_MODE_1 = 1,
    GMOS_DRIVER_SPI_CLOCK_MODE_2 = 2,
    GMOS_DRIVER_SPI_CLOCK_MODE_3 = 3
} gmosDriverSpiClockMode_t;

/*
 * This set of definitions specify the event bit masks used to indicate
 * transaction completion status from the platform abstraction layer
 * driver.
 */
#define GMOS_DRIVER_SPI_EVENT_STATUS_OFFSET   0
#define GMOS_DRIVER_SPI_EVENT_SIZE_OFFSET     8
#define GMOS_DRIVER_SPI_EVENT_STATUS_MASK     0x000000FF
#define GMOS_DRIVER_SPI_EVENT_SIZE_MASK       0x00FFFF00
#define GMOS_DRIVER_SPI_EVENT_COMPLETION_FLAG 0x80000000

/**
 * Defines the platform specific SPI bus state data structure. The full
 * type definition must be provided by the associated platform
 * abstraction layer.
 */
typedef struct gmosPalSpiBusState_t gmosPalSpiBusState_t;

/**
 * Defines the platform specific SPI bus configuration options. The full
 * type definition must be provided by the associated platform
 * abstraction layer.
 */
typedef struct gmosPalSpiBusConfig_t gmosPalSpiBusConfig_t;

/**
 * Defines the GubbinsMOS SPI device information structure that is used
 * for storing the SPI bus parameters associated with a single attached
 * device.
 */
typedef struct gmosDriverSpiDevice_t {

    // This is the set of event flags that are used by the platform
    // abstraction layer to signal completion of a SPI device
    // transaction.
    gmosEvent_t completionEvent;

    // This is the GPIO pin which is to be used for driving the SPI
    // device chip select line.
    uint16_t spiChipSelectPin;

    // This is the SPI clock frequency to be used during the transfer,
    // expressed as an integer multiple of 1kHz.
    uint16_t spiClockRate;

    // This is the SPI clock mode to be used during the transfer,
    // expressed using the conventional SPI clock mode enumeration.
    uint8_t spiClockMode;

} gmosDriverSpiDevice_t;

/**
 * Defines the GubbinsMOS SPI bus state data structure that is used for
 * managing the low level I/O for a single SPI bus controller.
 */
typedef struct gmosDriverSpiBus_t {

    // This is an opaque pointer to the SPI platform abstraction layer
    // data structure that is used for accessing the SPI interface
    // hardware. The data structure will be platform specific.
    gmosPalSpiBusState_t* palData;

    // This is an opaque pointer to the SPI platform abstraction layer
    // configuration data structure that is used for setting up the
    // SPI interface hardware. The data structure will be platform
    // specific.
    const gmosPalSpiBusConfig_t* palConfig;

    // This is a pointer to the device data structure for the currently
    // active SPI device.
    gmosDriverSpiDevice_t* device;

    // This is a pointer to the write data buffer to be used during a
    // SPI I/O transaction.
    uint8_t* writeData;

    // This is a pointer to the read data buffer to be used during a SPI
    // I/O transaction.
    uint8_t* readData;

    // This is the size of the data transfer to be used during a SPI I/O
    // transaction.
    uint16_t transferSize;

    // This is the current internal SPI bus state.
    uint8_t busState;

} gmosDriverSpiBus_t;

/**
 * Provides a platform configuration setup macro to be used when
 * allocating a SPI driver I/O data structure. Assigning this macro to
 * a SPI driver I/O data structure on declaration will configure the
 * SPI driver to use the platform specific configuration.
 * @param _palData_ This is the SPI interface platform abstraction layer
 *     data structure that is to be used for accessing the platform
 *     specific hardware.
 * @param _palConfig_ This is a platform specific SPI interface
 *     configuration data structure that defines a set of fixed
 *     configuration options to be used with the SPI interface.
 */
#define GMOS_DRIVER_SPI_PAL_CONFIG(_palData_, _palConfig_)             \
    { _palData_, _palConfig_, NULL, NULL, NULL, 0, 0 }

/**
 * Initialises a SPI bus interface data structure and initiates the
 * platform specific SPI hardware setup process.
 * @param spiInterface This is the SPI interface data structure which
 *     is is to be initialised.
 */
bool gmosDriverSpiBusInit (gmosDriverSpiBus_t* spiInterface);

/**
 * Initialises a SPI device data structure with the specified SPI
 * protocol parameters.
 * @param spiDevice This is the SPI device data structure which is to
 *     be initialised.
 * @param clientTask This is the client task which is to be notified
 *     on completion of SPI interface I/O transactions.
 * @param spiChipSelectPin This is the GPIO pin which is to be used as
 *     the dedicated chip select for the SPI device.
 * @param spiClockRate This is the maximum SPI clock frequency to be
 *     used during bus transfers, expressed as an integer multiple of
 *     1kHz. This will typically be rounded down to the closest clock
 *     frequency supported by the underlying hardware.
 * @param spiClockMode This is the SPI clock mode to be used during bus
 *     transfers, expressed using the conventional SPI clock mode
 *     enumeration. Supported clock mode values are 0, 1, 2 and 3.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully completing the initialisation process and 'false'
 *     otherwise.
 */
bool gmosDriverSpiDeviceInit (gmosDriverSpiDevice_t* spiDevice,
    gmosTaskState_t* clientTask, uint16_t spiChipSelectPin,
    uint16_t spiClockRate, uint8_t spiClockMode);

/**
 * Selects a SPI device peripheral connected to the SPI bus. This
 * sets the device specific SPI bus frequency and bus mode then asserts
 * the chip select line at the start of a sequence of low level
 * transactions. The scheduler is automatically prevented from entering
 * low power mode while the SPI bus is active.
 * @param spiInterface This is the SPI interface data structure which
 *     is associated with the SPI bus.
 * @param spiDevice This is the SPI device data structure which is
 *     associated with the device being accessed.
 * @return Returns a boolean value which will be set to 'true' if the
 *     SPI bus was idle and has now been selected and 'false' otherwise.
 */
bool gmosDriverSpiDeviceSelect (gmosDriverSpiBus_t* spiInterface,
    gmosDriverSpiDevice_t* spiDevice);

/**
 * Releases a SPI device peripheral connected to the SPI bus. This
 * deasserts the chip select line at the end of a sequence of low level
 * transactions.
 * @param spiInterface This is the SPI interface data structure which
 *     is associated with the SPI bus.
 * @param spiDevice This is the SPI device data structure which is
 *     associated with the device being accessed.
 * @return Returns a boolean value which will be set to 'true' if the
 *     SPI device was selected and has now been deselected and 'false'
 *     otherwise.
 */
bool gmosDriverSpiDeviceRelease (gmosDriverSpiBus_t* spiInterface,
    gmosDriverSpiDevice_t* spiDevice);

/**
 * Initiates a SPI write request for a device peripheral connected to
 * the SPI interface. The chip select must already have been asserted
 * using 'gmosDriverSpiDeviceSelect'. On completion the number of bytes
 * transferred will be indicated via the device completion event.
 * @param spiInterface This is the SPI state data structure which is
 *     associated with the SPI bus.
 * @param writeData This is a pointer to the byte array that is to be
 *     written to the SPI peripheral. It must remain valid for the
 *     full duration of the transaction.
 * @param writeSize This specifies the number of bytes that are to be
 *     written to the SPI peripheral.
 * @return Returns a boolean value which will be set to 'true' if the
 *     SPI write was initiated and is now active and 'false' otherwise.
 */
bool gmosDriverSpiIoWrite (gmosDriverSpiBus_t* spiInterface,
    uint8_t* writeData, uint16_t writeSize);

/**
 * Initiates a SPI read request for a device peripheral connected to
 * the SPI interface. The chip select must already have been asserted
 * using 'gmosDriverSpiDeviceSelect'. On completion the number of bytes
 * transferred will be indicated via the device completion event.
 * @param spiInterface This is the SPI state data structure which is
 *     associated with the SPI bus.
 * @param readData This is a pointer to the byte array that will be
 *     updated with the data read from the SPI peripheral. It must
 *     remain valid for the full duration of the transaction.
 * @param readSize This specifies the number of bytes that are to be
 *     read from the SPI peripheral.
 * @return Returns a boolean value which will be set to 'true' if the
 *     SPI read was initiated and is now active and 'false' otherwise.
 */
bool gmosDriverSpiIoRead (gmosDriverSpiBus_t* spiInterface,
    uint8_t* readData, uint16_t readSize);

/**
 * Initiates a SPI bidirectional transfer request for a device
 * peripheral connected to the SPI interface. The chip select must
 * already have been asserted using 'gmosDriverSpiDeviceSelect'. On
 * completion the number of bytes transferred will be indicated via the
 * device completion event.
 * @param spiInterface This is the SPI state data structure which is
 *     associated with the SPI bus.
 * @param writeData This is a pointer to the byte array that is to be
 *     written to the SPI peripheral. It must remain valid for the
 *     full duration of the transaction.
 * @param readData This is a pointer to the byte array that will be
 *     updated with the data read from the SPI device. It must remain
 *     valid for the full duration of the transaction.
 * @param transferSize This specifies the number of bytes that are to be
 *     transferred to and from the SPI peripheral.
 * @return Returns a boolean value which will be set to 'true' if the
 *     SPI transfer was initiated and is now active and 'false'
 *     otherwise.
 */
bool gmosDriverSpiIoTransfer (gmosDriverSpiBus_t* spiInterface,
    uint8_t* writeData, uint8_t* readData, uint16_t transferSize);

/**
 * Completes an asynchronous SPI transaction for a device peripheral
 * connected to the SPI interface.
 * @param spiInterface This is the SPI state data structure which is
 *     associated with the SPI bus.
 * @param transferSize This is a pointer to a 16-bit unsigned integer
 *     which will be populated with the number of bytes transferred
 *     during the transaction. A null reference may be used to indicate
 *     that the transfer size information is not required.
 * @return Returns a driver status value which indicates the current
 *     SPI interface status. The transaction will be complete when this
 *     is no longer set to 'GMOS_DRIVER_SPI_STATUS_ACTIVE'.
 */
gmosDriverSpiStatus_t gmosDriverSpiIoComplete
    (gmosDriverSpiBus_t* spiInterface, uint16_t* transferSize);

/**
 * Requests an inline SPI write data transfer for short transactions
 * where the overhead of setting up an asynchronous transfer is likely
 * to exceed the cost of carrying out a simple polled transaction.
 * The chip select must already have been asserted using
 * 'gmosDriverSpiDeviceSelect'.
 * @param spiInterface This is the SPI state data structure which is
 *     associated with the SPI bus.
 * @param writeData This is a pointer to the byte array that is to be
 *     written to the SPI peripheral. It must remain valid for the
 *     full duration of the transaction.
 * @param writeSize This specifies the number of bytes that are to be
 *     written to the SPI peripheral.
 * @return Returns a driver status value which indicates the success or
 *     failure of the inline transfer request.
 */
gmosDriverSpiStatus_t gmosDriverSpiIoInlineWrite
    (gmosDriverSpiBus_t* spiInterface, uint8_t* writeData,
    uint16_t writeSize);

/**
 * Requests an inline SPI read data transfer for short transactions
 * where the overhead of setting up an asynchronous transfer is likely
 * to exceed the cost of carrying out a simple polled transaction.
 * The chip select must already have been asserted using
 * 'gmosDriverSpiDeviceSelect'.
 * @param spiInterface This is the SPI state data structure which is
 *     associated with the SPI bus.
 * @param readData This is a pointer to the byte array that will be
 *     updated with the data read from the SPI device. It must remain
 *     valid for the full duration of the transaction.
 * @param readSize This specifies the number of bytes that are to be
 *     read from the SPI peripheral.
 * @return Returns a driver status value which indicates the success or
 *     failure of the inline transfer request.
 */
gmosDriverSpiStatus_t gmosDriverSpiIoInlineRead
    (gmosDriverSpiBus_t* spiInterface, uint8_t* readData,
    uint16_t readSize);

/**
 * Requests a bidirectional inline SPI data transfer for short
 * transactions where the overhead of setting up an asynchronous
 * transfer is likely to exceed the cost of carrying out a simple polled
 * transaction. The chip select must already have been asserted using
 * 'gmosDriverSpiDeviceSelect'.
 * @param spiInterface This is the SPI state data structure which is
 *     associated with the SPI bus.
 * @param writeData This is a pointer to the byte array that is to be
 *     written to the SPI peripheral. It must remain valid for the
 *     full duration of the transaction.
 * @param readData This is a pointer to the byte array that will be
 *     updated with the data read from the SPI device. It must remain
 *     valid for the full duration of the transaction.
 * @param transferSize This specifies the number of bytes that are to be
 *     transferred to and from the SPI peripheral.
 * @return Returns a driver status value which indicates the success or
 *     failure of the inline transfer request.
 */
gmosDriverSpiStatus_t gmosDriverSpiIoInlineTransfer
    (gmosDriverSpiBus_t* spiInterface, uint8_t* writeData,
    uint8_t* readData, uint16_t transferSize);

/**
 * Initialises the platform abstraction layer for a given SPI interface.
 * Refer to the platform specific SPI implementation for details of the
 * platform data area and the SPI interface configuration options.
 * This function is called automatically by the 'gmosDriverSpiBusInit'
 * function.
 * @param spiInterface This is the SPI interface data structure with
 *     the platform data and platform configuration entries already
 *     populated.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully completing the initialisation process and 'false'
 *     otherwise.
 */
bool gmosDriverSpiPalInit (gmosDriverSpiBus_t* spiInterface);

/**
 * Sets up the platform abstraction layer for one or more SPI
 * transactions that share the same SPI clock configuration.
 * @param spiInterface This is the SPI interface data structure with
 *     the SPI device field already populated.
 */
void gmosDriverSpiPalClockSetup (gmosDriverSpiBus_t* spiInterface);

/**
 * Performs a platform specific SPI transaction using the given SPI
 * interface.
 * @param spiInterface This is the SPI interface data structure, which
 *     will have been configured with all the parameters required to
 *     initiate the SPI transaction.
 */
void gmosDriverSpiPalTransaction (gmosDriverSpiBus_t* spiInterface);

/**
 * Performs a platform specific SPI inline transaction using the given
 * SPI interface.
 * @param spiInterface This is the SPI interface data structure, which
 *     will have been configured with all the parameters required to
 *     initiate the SPI transaction.
 * @return Returns a driver status value which indicates the success or
 *     failure of the inline transfer request.
 */
gmosDriverSpiStatus_t gmosDriverSpiPalInlineTransaction
    (gmosDriverSpiBus_t* spiInterface);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // GMOS_DRIVER_SPI_H
