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
 * This header provides device specific definitions and support
 * functions for the Raspberry Pi RP2040 range of devices.
 */

#ifndef PICO_DEVICE_H
#define PICO_DEVICE_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Define the function prototype to be used for DMA interrupt service
 * routines. Each ISR will be be invoked when a DMA interrupt for the
 * registered channel occurs and the associated interrupt condition is
 * cleared when the ISR returns a boolean value of 'true'.
 */
typedef bool (*gmosPalDmaIsr_t) (void);

/**
 * Initialises the serial debug console using the configured UART
 * connection.
 */
void gmosPalSerialConsoleInit (void);

/**
 * Writes the contents of the specified write data buffer to the UART
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
bool gmosPalSerialConsoleWrite (uint8_t* writeData, uint16_t writeSize);

/**
 * Attaches a DMA interrupt service routine for the specified DMA
 * channel. The attached ISR will be be invoked when a DMA interrupt
 * for the specified channel occurs and the associated interrupt
 * condition is cleared when the ISR returns a boolean value of 'true'.
 * @param channel This is the DMA channel to which the interrupt service
 *     routine is to be attached, numbered from 0 to 11.
 * @param isr This is the interrupt service routine which is to be
 *     attached to the specified DMA channel.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully attaching the ISR and 'false' otherwise.
 */
bool gmosPalDmaIsrAttach (uint8_t channel, gmosPalDmaIsr_t isr);

#endif // PICO_DEVICE_H
