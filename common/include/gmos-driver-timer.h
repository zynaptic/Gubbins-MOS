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
 * This header defines the common API for accessing general purpose
 * microcontroller hardware timers. Hardware timers may be used for
 * situations where the scheduler system timer does not provide
 * sufficient accuracy. The maximum timer counter size is 16 bits, since
 * this is the most common hardware timer size for the type of low end
 * microcontrollers supported by GubbinsMOS.
 */

#ifndef GMOS_DRIVER_TIMER_H
#define GMOS_DRIVER_TIMER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * Defines the function prototype to be used for timer interrupt service
 * routine callbacks.
 */
typedef void (*gmosDriverTimerIsr_t) (void* timerIsrData);

/**
 * Defines the timer active state enumeration.
 */
typedef enum  {
    GMOS_DRIVER_TIMER_STATE_RESET = 0,
    GMOS_DRIVER_TIMER_STATE_ONE_SHOT,
    GMOS_DRIVER_TIMER_STATE_CONTINUOUS
} gmosDriverTimerState_t;

/**
 * Defines the platform specific timer state data structure. The full
 * type definition must be provided by the associated platform
 * abstraction layer.
 */
typedef struct gmosPalTimerState_t gmosPalTimerState_t;

/**
 * Defines the platform specific timer configuration options. The full
 * type definition must be provided by the associated platform
 * abstraction layer.
 */
typedef struct gmosPalTimerConfig_t gmosPalTimerConfig_t;

/**
 * Defines the GubbinsMOS timer state data structure that is used for
 * managing the low level hardware for a single timer.
 */
typedef struct gmosDriverTimer_t {

    // This is an opaque pointer to the platform abstraction layer data
    // structure that is used for accessing the timer hardware. The data
    // structure will be platform specific.
    gmosPalTimerState_t* palData;

    // This is an opaque pointer to the platform abstraction layer
    // configuration data structure that is used for setting up the
    // timer hardware. The data structure will be platform
    // specific.
    const gmosPalTimerConfig_t* palConfig;

    // This is a pointer to the current timer ISR callback function.
    gmosDriverTimerIsr_t timerIsr;

    // This is an opaque pointer to the data area associated with the
    // current timer ISR callback function.
    void* timerIsrData;

    // Specifies the timer clock frequency currently in use.
    uint32_t frequency;

    // Specifies the maximum supported value for the timer counter.
    uint16_t maxValue;

    // Specifies the current active timer state.
    uint8_t activeState;

} gmosDriverTimer_t;

/**
 * Provides a platform configuration setup macro to be used when
 * allocating a timer driver data structure. Assigning this macro to
 * a timer driver data structure on declaration will configure the
 * timer driver to use the platform specific configuration.
 * @param _palData_ This is a pointer to the platform abstraction layer
 *     data structure that is to be used for accessing the platform
 *     specific hardware.
 * @param _palConfig_ This is a pointer to the platform specific timer
 *     configuration data structure that defines a set of fixed
 *     configuration options to be used with the hardware timer.
 */
#define GMOS_DRIVER_TIMER_PAL_CONFIG(_palData_, _palConfig_)           \
    { _palData_, _palConfig_, NULL, NULL, 0, 0, 0 }

/**
 * Initialises a timer for interrupt generation. This should be called
 * for each timer prior to accessing it via any of the other API
 * functions. The timer and associated interrupt are not enabled at this
 * stage.
 * @param timer This is the hardware timer data structure that is to be
 *     initialised. It should previously have been configured using the
 *     'GMOS_DRIVER_TIMER_PAL_CONFIG' macro.
 * @param frequency This is the timer counter increment frequency to be
 *     used. It should be a value that can be derived from the timer
 *     base clock. If this is not the case, the frequency will be
 *     rounded down to the nearest available option and a warning will
 *     be logged to the console.
 * @param timerIsr This is the interrupt service routine that will be
 *     called on any subsequent timer events.
 * @param timerIsrData This is an opaque pointer to a data item that
 *     will be passed to the interrupt service routine whenever it is
 *     invoked. A null reference may be used if no such data item is
 *     required.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully setting up the timer and 'false' on failure.
 */
bool gmosDriverTimerInit (gmosDriverTimer_t* timer, uint32_t frequency,
    gmosDriverTimerIsr_t timerIsr, void* timerIsrData);

/**
 * Enables a timer and associated interrupt for subsequent use. The
 * timer will be placed in its reset hold state once it has been
 * enabled.
 * @param timer This is the hardware timer data structure for the timer
 *     that is to be enabled.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully enabling the timer and 'false' on failure.
 */
bool gmosDriverTimerEnable (gmosDriverTimer_t* timer);

/**
 * Disables a timer and associated interrupt for subsequent use. This
 * allows the timer counter to be placed in a low power state.
 * @param timer This is the hardware timer data structure for the timer
 *     that is to be enabled.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully disabling the timer and 'false' on failure.
 */
bool gmosDriverTimerDisable (gmosDriverTimer_t* timer);

/**
 * Resets the current value of the timer counter to zero. The timer must
 * be enabled prior to performing a timer reset.
 * @param timer This is the hardware timer data structure for the timer
 *     that is to be updated.
 * @param resetHold This is a boolean value which selects the timer
 *     behaviour after reset. When set to 'true' the timer will be
 *     held in its reset state until another timer action releases it.
 *     When set to 'false' the timer counter remains in its current
 *     state (either reset, one-shot or continuous).
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully resetting the timer value and 'false' on failure.
 */
bool gmosDriverTimerReset (gmosDriverTimer_t* timer, bool resetHold);

/**
 * Accesses the current timer counter value.
 * @param timer This is the hardware timer data structure for the timer
 *     that is to be accessed.
 * @return Returns the current contents of the timer counter register.
 */
uint16_t gmosDriverTimerGetValue (gmosDriverTimer_t* timer);

/**
 * Sets a one-shot alarm for the timer counter. This is a 16-bit value
 * which will be compared against the current timer counter value,
 * triggering a call to the interrupt service routine on the timer clock
 * tick following a match. If the timer is currently in its reset hold
 * state, it is released from reset and the counter will immediately
 * start incrementing. After triggering the interrupt, the timer will
 * always be placed in the reset hold state.
 * @param timer This is the hardware timer data structure for the timer
 *     that is to be updated.
 * @param alarm This is the alarm value that is to be compared against
 *     the contents of the timer counter, triggering a call to the
 *     interrupt service routine on the timer clock tick following a
 *     match. It must be in the range from 1 to maxValue.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully setting the alarm and 'false' on failure.
 */
bool gmosDriverTimerRunOneShot (gmosDriverTimer_t* timer, uint16_t alarm);

/**
 * Sets a repeating alarm for the timer counter. This is a 16-bit value
 * which will be compared against the current timer counter value,
 * triggering a call to the interrupt service routine on the timer clock
 * tick following a match. If the timer is currently in its reset hold
 * state, it is released from reset and the counter will immediately
 * start incrementing. After triggering the interrupt, the timer will be
 * reset to zero and then continue counting.
 * @param timer This is the hardware timer data structure for the timer
 *     that is to be updated.
 * @param alarm This is the alarm value that is to be compared against
 *     the contents of the timer counter, triggering a call to the
 *     interrupt service routine on the timer clock tick following a
 *     match. It must be in the range from 1 to maxValue.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully setting the alarm and 'false' on failure.
 */
bool gmosDriverTimerRunRepeating (gmosDriverTimer_t* timer, uint16_t alarm);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // GMOS_DRIVER_TIMER_H
