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
 * connected to the microcontroller using the SPI bus.
 */

#ifndef GMOS_DRIVER_SPI_H
#define GMOS_DRIVER_SPI_H

#include <stdint.h>
#include <stdbool.h>

#include "gmos-scheduler.h"
#include "gmos-events.h"

/**
 * This enumeration specifies the SPI status values that are returned by
 * the transaction completion functions.
 */
typedef enum {
    GMOS_DRIVER_SPI_STATUS_IDLE,
    GMOS_DRIVER_SPI_STATUS_SUCCESS,
    GMOS_DRIVER_SPI_STATUS_ACTIVE,
    GMOS_DRIVER_SPI_STATUS_DMA_ERROR,
    GMOS_DRIVER_SPI_STATUS_DRIVER_ERROR
} gmosDriverSpiStatus_t;

/**
 * This enumeration specifies the various SPI link operating states.
 */
typedef enum {
    GMOS_DRIVER_SPI_LINK_IDLE,
    GMOS_DRIVER_SPI_LINK_SELECTED,
    GMOS_DRIVER_SPI_LINK_ACTIVE
} gmosDriverSpiLinkState_t;

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
 * Defines the platform specific SPI I/O state data structure. The full
 * type definition must be provided by the associated platform
 * abstraction layer.
 */
typedef struct gmosPalSpiIoState_t gmosPalSpiIoState_t;

/**
 * Defines the platform specific SPI I/O configuration options. The full
 * type definition must be provided by the associated platform
 * abstraction layer.
 */
typedef struct gmosPalSpiIoConfig_t gmosPalSpiIoConfig_t;

/**
 * Defines the GubbinsMOS SPI I/O state data structure that is used for
 * managing the low level I/O for a single SPI bus controller.
 */
typedef struct gmosDriverSpiIo_t {

    // This is an opaque pointer to the SPI platform abstraction layer
    // data structure that is used for accessing the SPI interface
    // hardware. The data structure will be platform specific.
    gmosPalSpiIoState_t* palData;

    // This is an opaque pointer to the SPI platform abstraction layer
    // configuration data structure that is used for setting up the
    // SPI interface hardware. The data structure will be platform
    // specific.
    const gmosPalSpiIoConfig_t* palConfig;

    // This is the set of event flags that are used by the platform
    // abstraction layer to signal completion of a SPI bus transaction.
    gmosEvent_t completionEvent;

    // This is a pointer to the write data buffer to be used during a
    // SPI I/O transaction.
    uint8_t* writeData;

    // This is a pointer to the read data buffer to be used during a SPI
    // I/O transaction.
    uint8_t* readData;

    // This is the size of the data transfer to be used during a SPI I/O
    // transaction.
    uint16_t transferSize;

    // This is the GPIO pin which is to be used for driving the SPI
    // chip select line.
    uint16_t spiChipSelectPin;

    // This is the SPI clock frequency to be used during the transfer,
    // expressed as an integer multiple of 1kHz.
    uint16_t spiClockRate;

    // This is the SPI clock mode to be used during the transfer,
    // expressed using the conventional SPI clock mode enumeration.
    uint8_t spiClockMode;

    // This is the current internal SPI link state.
    uint8_t linkState;

} gmosDriverSpiIo_t;

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
    { _palData_, _palConfig_ }

/**
 * Initialises a SPI interface to use as a simple point to point link,
 * with the microcontroller as the SPI bus controller and a single
 * attached SPI device peripheral.
 * @param spiInterface This is the SPI interface data structure that is
 *     to be initialised for the simple point to point link.
 * @param clientTask This is the client task which is to be notified
 *     on completion of SPI interface I/O transactions.
 * @param spiChipSelectPin This is the GPIO pin which is to be used as
 *     the dedicated chip select.
 * @param spiClockRate This is the maximum SPI clock frequency to be
 *     used during link transfers, expressed as an integer multiple of
 *     1kHz. This will typically be rounded down to the closest clock
 *     frequency supported by the underlying hardware.
 * @param spiClockMode This is the SPI clock mode to be used during link
 *     transfers, expressed using the conventional SPI clock mode
 *     enumeration. Supported clock mode values are 0, 1, 2 and 3.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully completing the initialisation process and 'false'
 *     otherwise.
 */
bool gmosDriverSpiLinkInit (gmosDriverSpiIo_t* spiInterface,
    gmosTaskState_t* clientTask, uint16_t spiChipSelectPin,
    uint16_t spiClockRate, uint8_t spiClockMode);

/**
 * Selects a SPI device peripheral connected to the SPI interface using
 * a simple point to point link. This asserts the chip select line at
 * the start of a sequence of low level transactions. The scheduler is
 * automatically prevented from entering low power mode while a SPI link
 * is active.
 * @param spiInterface This is the SPI interface data structure which
 *     is associated with the point to point SPI link.
 * @return Returns a boolean value which will be set to 'true' if the
 *     SPI link was idle and has now been selected and 'false'
 *     otherwise.
 */
bool gmosDriverSpiLinkSelect (gmosDriverSpiIo_t* spiInterface);

/**
 * Releases a SPI device peripheral connected to the SPI interface using
 * a simple point to point link. This deasserts the chip select line at
 * the end of a sequence of low level transactions.
 * @param spiInterface This is the SPI interface data structure which
 *     is associated with the point to point SPI link.
 * @return Returns a boolean value which will be set to 'true' if the
 *     SPI link was selected and has now been deselected and 'false'
 *     otherwise.
 */
bool gmosDriverSpiLinkRelease (gmosDriverSpiIo_t* spiInterface);

/**
 * Initiates a SPI write request for a device peripheral connected to
 * the SPI interface using a simple point to point link. The chip select
 * must already have been asserted using 'gmosDriverSpiLinkSelect'.
 * On completion the number of bytes transferred will be indicated via
 * the completion event.
 * @param spiInterface This is the SPI interface data structure which
 *     is associated with the point to point SPI link.
 * @param writeData This is a pointer to the byte array that is to be
 *     written to the SPI peripheral. It must remain valid for the
 *     full duration of the transaction.
 * @param writeSize This specifies the number of bytes that are to be
 *     written to the SPI peripheral.
 * @return Returns a boolean value which will be set to 'true' if the
 *     SPI link was selected and is now active and 'false' otherwise.
 */
bool gmosDriverSpiLinkWrite (gmosDriverSpiIo_t* spiInterface,
    uint8_t* writeData, uint16_t writeSize);

/**
 * Initiates a SPI read request for a device peripheral connected to
 * the SPI interface using a simple point to point link. The chip select
 * must already have been asserted using 'gmosDriverSpiLinkSelect'.
 * On completion the number of bytes transferred will be indicated via
 * the completion event.
 * @param spiInterface This is the SPI interface data structure which
 *     is associated with the point to point SPI link.
 * @param readData This is a pointer to the byte array that will be
 *     updated with the data read from the SPI device. It must remain
 *     valid for the full duration of the transaction.
 * @param readSize This specifies the number of bytes that are to be
 *     read from the SPI peripheral.
 * @return Returns a boolean value which will be set to 'true' if the
 *     SPI link was selected and is now active and 'false' otherwise.
 */
bool gmosDriverSpiLinkRead (gmosDriverSpiIo_t* spiInterface,
    uint8_t* readData, uint16_t readSize);

/**
 * Initiates a SPI bidirectional transfer request for a device
 * peripheral connected to the SPI interface using a simple point to
 * point link. The chip select must already have been asserted using
 * 'gmosDriverSpiLinkSelect'. On completion the number of bytes
 * transferred will be indicated via the completion event.
 * @param spiInterface This is the SPI interface data structure which
 *     is associated with the point to point SPI link.
 * @param writeData This is a pointer to the byte array that is to be
 *     written to the SPI peripheral. It must remain valid for the
 *     full duration of the transaction.
 * @param readData This is a pointer to the byte array that will be
 *     updated with the data read from the SPI device. It must remain
 *     valid for the full duration of the transaction.
 * @param transferSize This specifies the number of bytes that are to be
 *     transferred to and from the SPI peripheral.
 * @return Returns a boolean value which will be set to 'true' if the
 *     SPI link was selected and is now active and 'false' otherwise.
 */
bool gmosDriverSpiLinkTransfer (gmosDriverSpiIo_t* spiInterface,
    uint8_t* writeData, uint8_t* readData, uint16_t transferSize);

/**
 * Completes a SPI transaction for a device peripheral connected to the
 * SPI interface using a simple point to point link.
 * @param spiInterface This is the SPI interface data structure which
 *     is associated with the point to point SPI link.
 * @param transferSize This is a pointer to a 16-bit unsigned integer
 *     which will be populated with the number of bytes transferred
 *     during the transaction. A null reference may be used to indicate
 *     that the transfer size information is not required.
 * @return Returns a driver status value which indicates the current
 *     SPI link status. The transaction will be complete when this is
 *     no longer set to 'GMOS_DRIVER_SPI_STATUS_ACTIVE'.
 */
gmosDriverSpiStatus_t gmosDriverSpiLinkComplete
    (gmosDriverSpiIo_t* spiInterface, uint16_t* transferSize);

/**
 * Initialises the platform abstraction layer for a given SPI interface.
 * Refer to the platform specific SPI implementation for details of the
 * platform data area and the SPI interface configuration options.
 * This function is called automatically by the 'gmosDriverSpiLinkInit'
 * function.
 * @param spiInterface This is the SPI interface data structure with
 *     the platform data and platform configuration entries already
 *     populated.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully completing the initialisation process and 'false'
 *     otherwise.
 */
bool gmosDriverSpiPalInit (gmosDriverSpiIo_t* spiInterface);

/**
 * Sets up the platform abstraction layer for one or more SPI
 * transactions that share the same SPI clock configuration.
 * @param spiInterface This is the SPI interface data structure with
 *     the SPI clock frequency and clock mode fields already populated.
 */
void gmosDriverSpiPalClockSetup (gmosDriverSpiIo_t* spiInterface);

/**
 * Performs a platform specific SPI transaction using the given SPI
 * interface.
 * @param spiInterface This is the SPI interface data structure, which
 *     will have been configured with all the parameters required to
 *     initiate the SPI transaction.
 */
void gmosDriverSpiPalTransaction (gmosDriverSpiIo_t* spiInterface);

#endif // GMOS_DRIVER_SPI_H
