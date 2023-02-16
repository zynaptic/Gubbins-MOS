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
 * This header defines the platform specific data structures used for
 * the Silicon Labs EFR32xG2x SPI driver implementation.
 */

#ifndef EFR32_DRIVER_SPI_H
#define EFR32_DRIVER_SPI_H

#include <stdint.h>

/**
 * This enumeration is a list of the USART interfaces on the device
 * which can support SPI operation.
 */
typedef enum {
    GMOS_PAL_SPI_BUS_ID_USART0,
    GMOS_PAL_SPI_BUS_ID_EUSART0,
    GMOS_PAL_SPI_BUS_ID_EUSART1
} gmosPalSpiBusId_t;

/**
 * Defines the platform specific SPI interface hardware configuration
 * settings data structure.
 */
typedef struct gmosPalSpiBusConfig_t {

    // Specify the GPIO pin used for the SPI clock. The pin must support
    // the SPI SCLK alternate function for the selected SPI interface.
    uint16_t sclkPinId;

    // Specify the GPIO pin used for the MOSI data. The pin must support
    // the SPI TX alternate function for the selected SPI interface.
    uint16_t mosiPinId;

    // Specify the GPIO pin used for the MISO data. The pin must support
    // the SPI RX alternate function for the selected SPI interface.
    uint16_t misoPinId;

    // Specify the EFR32 USART interface instance to use.
    uint8_t usartInterfaceId;

    // Specify the bus clock mode to be used for all devices on the bus.
    // The EFR32 driver does not support dynamic clock mode assignment.
    // The conventional SPI clock mode enumeration should be used.
    uint8_t spiClockMode;

} gmosPalSpiBusConfig_t;

/**
 * Defines the platform specific SPI interface dynamic data structure
 * to be used for the DMA based SPI driver.
 */
typedef struct gmosPalSpiBusState_t {

    // Index into the associated SPI bus state data structures.
    uint8_t spiIndex;

} gmosPalSpiBusState_t;

#endif // EFR32_DRIVER_SPI_H
