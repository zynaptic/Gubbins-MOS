/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2020-2022 Zynaptic Limited
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
 * the STM32L0XX SPI driver implementation.
 */

#ifndef STM32_DRIVER_SPI_H
#define STM32_DRIVER_SPI_H

/**
 * Defines the platform specific SPI interface hardware configuration
 * settings data structure.
 */
typedef struct gmosPalSpiBusConfig_t {

    // Specify the GPIO pin used for the SPI clock. The most significant
    // byte is the bank and the least significant byte is the pin.
    uint16_t sclkPinId;

    // Specify the GPIO pin used for the MOSI data. The most significant
    // byte is the bank and the least significant byte is the pin.
    uint16_t mosiPinId;

    // Specify the GPIO pin used for the MISO data. The most significant
    // byte is the bank and the least significant byte is the pin.
    uint16_t misoPinId;

    // Specify the GPIO pin alternate function to use for the SPI clock.
    uint8_t sclkPinAltFn;

    // Specify the GPIO pin alternate function to use for MOSI data.
    uint8_t mosiPinAltFn;

    // Specify the GPIO pin alternate function to use for MISO data.
    uint8_t misoPinAltFn;

    // Specify the STM32 SPI interface to use.
    uint8_t spiInterfaceId;

} gmosPalSpiBusConfig_t;

/**
 * Defines the platform specific SPI interface dynamic data structure
 * to be used for the DMA based SPI driver.
 */
typedef struct gmosPalSpiBusState_t {

} gmosPalSpiBusState_t;

#endif // STM32_DRIVER_SPI_H
