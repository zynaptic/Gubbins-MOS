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
 * This header defines the platform specific data structures used for
 * the STM32L0XX I2C driver implementation.
 */

#ifndef STM32_DRIVER_I2C_H
#define STM32_DRIVER_I2C_H

/**
 * Defines the platform specific I2C hardware configuration settings
 * data structure.
 */
typedef struct gmosPalI2CBusConfig_t {

    // Specify the GPIO pin used for SCL. The most significant nibble is
    // the bank and the least significant nibble is the pin.
    uint8_t sclPinId;

    // Specify the GPIO pin alternate function to use for SCL.
    uint8_t sclPinAltFn;

    // Specify the GPIO pin used for SDA. The most significant nibble is
    // the bank and the least significant nibble is the pin.
    uint8_t sdaPinId;

    // Specify the GPIO pin alternate function to use for SDA.
    uint8_t sdaPinAltFn;

    // Specify the STM32 I2C interface to use.
    uint8_t i2cInterfaceId;

    // Specify the I2C bus speed to use. Set to 0 for 100kHz and 1 for
    // 400kHz.
    uint8_t i2cBusSpeed;

} gmosPalI2CBusConfig_t;

/**
 * Define the platform specific I2C hardware interface dynamic data
 * structure.
 */
typedef struct gmosPalI2CBusState_t {

    // Identifies the currently active phase of operation.
    uint8_t transferPhase;

    // Specifies the transfer byte count.
    uint8_t byteCount;

} gmosPalI2CBusState_t;

/**
 * Provides a default I2C configuration for I2C1 on STM32L0X0 devices,
 * with SCL mapped to pin B8 and SDA mapped to pin B9. This corresponds
 * to the Arduino I2C connection on the STM32 Nucleo boards.
 */
static const gmosPalI2CBusConfig_t gmosPalI2CBusConfig_STM32L0X0_I2C1 = {
    0x18, // sclPinId = PB8
    4,    // sclPinAltFn
    0x19, // sdaPinId = PB9
    4,    // sdaPinAltFn
    1,    // i2cInterfaceId
    1     // i2cBusSpeed = 400 kHz
};

#endif // STM32_DRIVER_I2C_H
