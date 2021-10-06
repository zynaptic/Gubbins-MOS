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
 * This header provides device specific hardware timer definitions and
 * functions for the Microchip/Atmel ATMEGA range of devices.
 */

#ifndef ATMEGA_DRIVER_TIMER_H
#define ATMEGA_DRIVER_TIMER_H

// Map the ATMEGA timer names to useful timer ID values.
#define ATMEGA_DRIVER_TIMER_ID_TIM0  0
#define ATMEGA_DRIVER_TIMER_ID_TIM1  1

/**
 * Defines the platform specific hardware timer configuration settings
 * data structure.
 */
typedef struct gmosPalTimerConfig_t {

    // Specify the timer instance to use, taken from the list of defined
    // timer ID values.
    uint8_t timerId;

} gmosPalTimerConfig_t;

/**
 * Defines the platform specific hardware timer dynamic data structure.
 */
typedef struct gmosPalTimerState_t {

    // Specify the active clock select value.
    uint8_t clockSelect;

} gmosPalTimerState_t;

// Specify the registers used for timer interrupts.
// This depends on the selected target device.
#if ((TARGET_DEVICE == atmega32) || (TARGET_DEVICE == atmega32a))
#define ATMEGA_TIMER0_TCLK_REG     TCCR0
#define ATMEGA_TIMER0_TCFG_REG     TCCR0
#define ATMEGA_TIMER0_CTC_BIT      WGM01
#define ATMEGA_TIMER0_INT_MASK_REG TIMSK
#define ATMEGA_TIMER0_INT_MASK_BIT OCIE0
#define ATMEGA_TIMER0_INT_FLAG_REG TIFR
#define ATMEGA_TIMER0_INT_FLAG_BIT OCF0
#define ATMEGA_TIMER0_MATCH_REG    OCR0
#define ATMEGA_TIMER0_INT_VECT     TIMER0_COMP_vect

#define ATMEGA_TIMER1_TCLK_REG     TCCR1B
#define ATMEGA_TIMER1_TCFG_REG     TCCR1B
#define ATMEGA_TIMER1_CTC_BIT      WGM12
#define ATMEGA_TIMER1_INT_MASK_REG TIMSK
#define ATMEGA_TIMER1_INT_MASK_BIT OCIE1A
#define ATMEGA_TIMER1_INT_FLAG_REG TIFR
#define ATMEGA_TIMER1_INT_FLAG_BIT OCF1A
#define ATMEGA_TIMER1_MATCH_REG_L  OCR1AL
#define ATMEGA_TIMER1_MATCH_REG_H  OCR1AH
#define ATMEGA_TIMER1_INT_VECT     TIMER1_COMPA_vect

#elif (TARGET_DEVICE == atmega328p)
#define ATMEGA_TIMER0_TCLK_REG     TCCR0B
#define ATMEGA_TIMER0_TCFG_REG     TCCR0A
#define ATMEGA_TIMER0_CTC_BIT      WGM01
#define ATMEGA_TIMER0_INT_MASK_REG TIMSK0
#define ATMEGA_TIMER0_INT_MASK_BIT OCIE0A
#define ATMEGA_TIMER0_INT_FLAG_REG TIFR0
#define ATMEGA_TIMER0_INT_FLAG_BIT OCF0A
#define ATMEGA_TIMER0_MATCH_REG    OCR0A
#define ATMEGA_TIMER0_INT_VECT     TIMER0_COMPA_vect

#define ATMEGA_TIMER1_TCLK_REG     TCCR1B
#define ATMEGA_TIMER1_TCFG_REG     TCCR1B
#define ATMEGA_TIMER1_CTC_BIT      WGM12
#define ATMEGA_TIMER1_INT_MASK_REG TIMSK1
#define ATMEGA_TIMER1_INT_MASK_BIT OCIE1A
#define ATMEGA_TIMER1_INT_FLAG_REG TIFR1
#define ATMEGA_TIMER1_INT_FLAG_BIT OCF1A
#define ATMEGA_TIMER1_MATCH_REG_L  OCR1AL
#define ATMEGA_TIMER1_MATCH_REG_H  OCR1AH
#define ATMEGA_TIMER1_INT_VECT     TIMER1_COMPA_vect

// Device not currently supported.
#else
#error ("ATMEGA Target Device Not Supported By Timer Driver");
#endif

#endif // ATMEGA_DRIVER_TIMER_H
