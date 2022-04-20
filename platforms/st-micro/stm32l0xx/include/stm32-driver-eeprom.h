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
 * This header provides device specific EEPROM definitions and functions
 * for the STM32L0XX range of devices.
 */

#ifndef STM32_DRIVER_EEPROM_H
#define STM32_DRIVER_EEPROM_H

#include "stm32-device.h"

/*
 * Select the EEPROM size based on the target device. For devices with
 * dual bank EEPROM, the two banks are assumed to form a single
 * contiguous EEPROM area.
 */
#ifdef DATA_EEPROM_END
#define STM32_DRIVER_EEPROM_SIZE \
    (DATA_EEPROM_END - DATA_EEPROM_BASE + 1)
#endif
#ifdef DATA_EEPROM_BANK2_END
#define STM32_DRIVER_EEPROM_SIZE \
    (DATA_EEPROM_BANK2_END - DATA_EEPROM_BASE + 1)
#endif

/**
 * Defines the platform specific EEPROM driver configuration settings
 * data structure.
 */
typedef struct gmosPalEepromConfig_t {

} gmosPalEepromConfig_t;

/**
 * Defines the platform specific EEPROM driver dynamic data structure.
 */
typedef struct gmosPalEepromState_t {

    // Specify a pointer to the next EEPROM write data.
    const uint8_t* writeData;

    // Specify the number of bytes still to be written to EEPROM.
    uint16_t writeCount;

    // Specify the current address offset within the EEPROM.
    uint16_t addrOffset;

} gmosPalEepromState_t;

#endif // STM32_DRIVER_EEPROM_H
