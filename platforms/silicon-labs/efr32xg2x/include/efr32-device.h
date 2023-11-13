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
 * This header provides device specific definitions for the EFR32xG2x
 * range of devices.
 */

#ifndef EFR32_DEVICE_H
#define EFR32_DEVICE_H

/**
 * Initialises the EFR32 system timer implementation using the Gecko
 * SDK sleep timer library.
 */
void gmosPalSystemTimerInit (void);

/**
 * Initialises the EFR32 serial debug console using USART0.
 */
void gmosPalSerialConsoleInit (void);

/**
 * Writes the contents of the specified write data buffer to the EFR32
 * serial debug console.
 * @param writeData This is a pointer to the write data buffer that is
 *     to be written to the serial debug console.
 * @param writeSize This specifies the number of bytes in the write data
 *     buffer that are to be written to the serial debug console.
 * @return Returns a boolean value which will be set to 'true' if all
 *     the contents of the write data buffer could be queued for
 *     transmission and 'false' if there is currently insufficient space
 *     in the serial console transmit queue.
 */
bool gmosPalSerialConsoleWrite (
    const uint8_t* writeData, uint16_t writeSize);

/**
 * Flushes the EFR32 serial debug console after an assertion. This
 * function does not return.
 */
void gmosPalSerialConsoleFlushAssertion (void);

/**
 * Reads the EFR32 core temperature sensor value as a 32-bit floating
 * point value.
 * @return Returns the current EFR32 core temperature as a floating
 *     point value.
 */
float gmosPalGetCoreTempFloat (void);

#endif // EFR32_DEVICE_H
