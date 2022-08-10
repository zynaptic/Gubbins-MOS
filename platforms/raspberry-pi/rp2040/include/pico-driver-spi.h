/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2022 Zynaptic Limited
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
 * the Raspberry Pi RP2040 SPI driver implementation.
 */

#ifndef PICO_DRIVER_SPI_H
#define PICO_DRIVER_SPI_H

#include <stdint.h>

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

    // Specify the SPI interface instance to use.
    uint8_t spiInterfaceId;

} gmosPalSpiBusConfig_t;

/**
 * Defines the platform specific SPI interface dynamic data structure
 * to be used for the DMA based SPI driver.
 */
typedef struct gmosPalSpiBusState_t {

    // Hold a pointer to the Pico SDK SPI interface data structure.
    void* spiInst;

    // Specify the DMA transmit channel to use. This is allocated at
    // runtime.
    uint8_t dmaTxChannel;

    // Specify the DMA receive channel to use. This is allocated at
    // runtime.
    uint8_t dmaRxChannel;

} gmosPalSpiBusState_t;

#endif // PICO_DRIVER_SPI_H
