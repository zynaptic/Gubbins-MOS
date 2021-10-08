/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2020-2021 Zynaptic Limited
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
 * This header provides device specific SPI definitions and functions
 * for the Microchip/Atmel ATMEGA range of devices. It supports a
 * single instance of the standard SPI interface operating in master
 * mode. Additional SPI interfaces using the universal serial interface
 * peripheral are not supported.
 */

#ifndef ATMEGA_DRIVER_SPI_H
#define ATMEGA_DRIVER_SPI_H

#include "atmega-device.h"
#include "atmega-driver-gpio.h"

/**
 * Defines the platform specific SPI interface hardware configuration
 * settings data structure.
 */
typedef struct gmosPalSpiBusConfig_t {

} gmosPalSpiBusConfig_t;

/**
 * Defines the platform specific SPI interface dynamic data structure
 * to be used for the DMA based SPI driver.
 */
typedef struct gmosPalSpiBusState_t {

    // Specify the current transfer byte count.
    uint16_t transferCount;

} gmosPalSpiBusState_t;

// Specify the pins used for the SPI interface. This depends on the
// selected target device.
#if ((TARGET_DEVICE == atmega32) || (TARGET_DEVICE == atmega32a))
#define ATMEGA_SPI_PIN_MOSI (ATMEGA_GPIO_BANK_B | 5)
#define ATMEGA_SPI_PIN_MISO (ATMEGA_GPIO_BANK_B | 6)
#define ATMEGA_SPI_PIN_SCLK (ATMEGA_GPIO_BANK_B | 7)

#elif (TARGET_DEVICE == atmega328p)
#define ATMEGA_SPI_PIN_MOSI (ATMEGA_GPIO_BANK_B | 3)
#define ATMEGA_SPI_PIN_MISO (ATMEGA_GPIO_BANK_B | 4)
#define ATMEGA_SPI_PIN_SCLK (ATMEGA_GPIO_BANK_B | 5)

// Device not currently supported.
#else
#error ("ATMEGA Target Device Not Supported By SPI Driver");
#endif

#endif // ATMEGA_DRIVER_SPI_H
