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
 * This header provides device specific definitions for the ATMEGA
 * range of devices. Most of these definitions are provided by the
 * vendor device headers, which are selected according to the target
 * device for the build.
 */

#ifndef ATMEGA_DEVICE_H
#define ATMEGA_DEVICE_H

// Provide an enumerated list of the supported devices. These are the
// same device identification strings as used in the AVR GCC headers.
#define atmega32   1
#define atmega32a  2
#define atmega328p 3

// Select the appropriate vendor headers. These are provided as part of
// the AVR-GCC distribution.
#include "avr/io.h"
#include "avr/interrupt.h"

/**
 * Initialises the ATMEGA system timer implementation using the 16-bit
 * low power timer.
 */
void gmosPalSystemTimerInit (void);

/**
 * Initialises the ATMEGA serial debug console.
 */
void gmosPalSerialConsoleInit (void);

/**
 * Writes the contents of the specified write data buffer to the ATMEGA
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
 * Enable the I/O clock for the duration of an I/O operation. This
 * increments the I/O peripheral active counter and prevents the
 * microcontroller from entering power save or extended standby.
 */
void gmosPalIoSetActive (void);

/**
 * Disable the I/O clock after completion of an I/O operation. This
 * decrements the I/O peripheral active counter and allows the
 * microcontroller to enter the power save or extended standby once
 * the counter reaches zero.
 */
void gmosPalIoSetInactive (void);

// Specify the registers used for the serial debug console. This depends
// on the selected target device.
#if ((TARGET_DEVICE == atmega32) || (TARGET_DEVICE == atmega32a))
#define ATMEGA_CONSOLE_UCSRA_REG UCSRA
#define ATMEGA_CONSOLE_UDRE_BIT  UDRE
#define ATMEGA_CONSOLE_UCSRB_REG UCSRB
#define ATMEGA_CONSOLE_TXEN_BIT  TXEN
#define ATMEGA_CONSOLE_UDR_REG   UDR
#define ATMEGA_CONSOLE_UBRRL_REG UBRRL
#define ATMEGA_CONSOLE_UBRRH_REG UBRRH
#define ATMEGA_TIMER_TCCR_REG    TCCR2
#define ATMEGA_TIMER_TIMSK_REG   TIMSK

#elif (TARGET_DEVICE == atmega328p)
#define ATMEGA_CONSOLE_UCSRA_REG UCSR0A
#define ATMEGA_CONSOLE_UDRE_BIT  UDRE0
#define ATMEGA_CONSOLE_UCSRB_REG UCSR0B
#define ATMEGA_CONSOLE_TXEN_BIT  TXEN0
#define ATMEGA_CONSOLE_UDR_REG   UDR0
#define ATMEGA_CONSOLE_UBRRL_REG UBRR0L
#define ATMEGA_CONSOLE_UBRRH_REG UBRR0H
#define ATMEGA_TIMER_TCCR_REG    TCCR2B
#define ATMEGA_TIMER_TIMSK_REG   TIMSK2

// Device not currently supported.
#else
#error ("ATMEGA Target Device Not Supported By Serial Debug Console");
#endif

#endif // ATMEGA_DEVICE_H
