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
 * This file implements the platform specific capacitive touch sensor
 * functions for the STM32L1XX series of devices. This implementation
 * only supports those devices that can use timer based acquisition
 * with TIM9 and TIM10, which are used for all touch sensor instances.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-platform.h"
#include "gmos-scheduler.h"
#include "gmos-events.h"
#include "gmos-driver-gpio.h"
#include "gmos-driver-touch.h"
#include "stm32-device.h"
#include "stm32-driver-gpio.h"
#include "stm32-driver-touch.h"

// Specify the nominal sense capacitor PWM frequency.
#define TOUCH_SENSOR_PWM_FREQUENCY 250000

// Specify the number of microseconds delay that are required to
// confirm that the sensing capacitors have discharged.
#define CAPACITOR_DISCHARGE_DELAY 60

// Specify the event flags.
#define TOUCH_SENSOR_EVENT_CAPTURE    0x80000000
#define TOUCH_SENSOR_EVENT_TIMEOUT    0x40000000
#define TOUCH_SENSOR_EVENT_COUNT_MASK 0x0000FFFF

// Specify the state space for the touch sensing state machine.
typedef enum {
    TOUCH_SENSOR_STATE_IDLE,
    TOUCH_SENSOR_STATE_FAILED,
    TOUCH_SENSOR_STATE_SLEEP,
    TOUCH_SENSOR_STATE_CHANNEL_SELECT,
    TOUCH_SENSOR_STATE_CHANNEL_CLEAR,
    TOUCH_SENSOR_STATE_CHANNEL_SETUP,
    TOUCH_SENSOR_STATE_CHANNEL_START,
    TOUCH_SENSOR_STATE_CHANNEL_PROCESS
} stm32DriverTouchState_t;

// Specify mapping of GPIO pins to analogue routing interface groups
// represented by the ASCR registers.
static const uint8_t ascrRegMapGpioA [] = {
    0, 1, 2, 3, 255, 255, 6, 7, 41, 42, 43, 255, 255, 38, 39, 40 };

static const uint8_t ascrRegMapGpioB [] = {
    8, 9, 48, 255, 36, 37, 59, 60, 255, 255, 255, 255, 18, 19, 20, 21 };

static const uint8_t ascrRegMapGpioC [] = {
    10, 11, 12, 13, 14, 15, 32, 33, 34, 35, 255, 255, 255, 255, 255, 255 };

#ifdef GPIOF
static const uint8_t ascrRegMapGpioF [] = {
    255, 255, 255, 255, 255, 255, 27, 28, 29, 30, 16, 49, 50, 51, 52, 53 };
#endif

#ifdef GPIOG
static const uint8_t ascrRegMapGpioG [] = {
    54, 55, 56, 57, 58, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 };
#endif

// Specifies the start of the linked list of active touch channels.
static gmosDriverTouchChannel_t* touchChannelList = NULL;

// Speciies the currently active touch channel.
static gmosDriverTouchChannel_t* activeTouchChannel = NULL;

// Allocate the touch sensor task data structure.
static gmosTaskState_t touchSensorTask;

// Allocate the capture complete event data structure.
static gmosEvent_t touchSensorEvent = GMOS_EVENT_INIT (&touchSensorTask);

// Specify the current touch sensor channel processing state.
static uint8_t touchSensorState;

// Specify the current touch sensor sampling timestamp.
static uint32_t touchSensorTimestamp;

/*
 * Change the mode of a specific GPIO pin. This is not part of the
 * standard GPIO API, so is implemented here. Mode may be set to
 * input (0), output (1) or alternate function (2).
 */
static void stm32DriverTouchSetGpioMode (uint16_t pinId, uint8_t mode)
{
    uint8_t gpioPinIndex = pinId & 0x0F;
    uint32_t gpioModeMask;
    uint32_t gpioModeSet;

    // Determine the GPIO mode pins to be set.
    gpioModeMask = ~(3 << (2 * gpioPinIndex));
    gpioModeSet = mode << (2 * gpioPinIndex);

    // Modify the appropriate GPIO bank register.
    switch (pinId & 0x0700) {
        case STM32_GPIO_BANK_A :
            GPIOA->MODER = (GPIOA->MODER & gpioModeMask) | gpioModeSet;
            break;
        case STM32_GPIO_BANK_B :
            GPIOB->MODER = (GPIOB->MODER & gpioModeMask) | gpioModeSet;
            break;
        case STM32_GPIO_BANK_C :
            GPIOC->MODER = (GPIOC->MODER & gpioModeMask) | gpioModeSet;
            break;
        #ifdef GPIOF
        case STM32_GPIO_BANK_F :
            GPIOF->MODER = (GPIOF->MODER & gpioModeMask) | gpioModeSet;
            break;
        #endif
        #ifdef GPIOG
        case STM32_GPIO_BANK_G :
            GPIOG->MODER = (GPIOG->MODER & gpioModeMask) | gpioModeSet;
            break;
        #endif
    }
}

/*
 * Set or clear the ASMR flag for the sensor pin.
 */
static bool stm32DriverTouchSetAsmrFlag (bool flagState)
{
    const gmosPalTouchConfig_t* palConfig = activeTouchChannel->palConfig;
    volatile uint32_t* riAsmrReg;
    uint8_t sensorPinIndex;
    uint32_t regValue;

    // Select the registers to use for the sensor pin.
    switch (palConfig->sensorPinId & 0x0700) {
        case STM32_GPIO_BANK_A :
            riAsmrReg = &(RI->ASMR1);
            break;
        case STM32_GPIO_BANK_B :
            riAsmrReg = &(RI->ASMR2);
            break;
        case STM32_GPIO_BANK_C :
            riAsmrReg = &(RI->ASMR3);
            break;
        #ifdef GPIOF
        case STM32_GPIO_BANK_F :
            riAsmrReg = &(RI->ASMR4);
            break;
        #endif
        #ifdef GPIOG
        case STM32_GPIO_BANK_G :
            riAsmrReg = &(RI->ASMR5);
            break;
        #endif
        default :
            return false;
    }

    // Enable PWM control of the analogue switches for the sensor pin.
    sensorPinIndex = palConfig->sensorPinId & 0x0F;
    regValue = *riAsmrReg;
    if (flagState) {
        regValue |= 1 << sensorPinIndex;
    } else {
        regValue &= ~(1 << sensorPinIndex);
    }
    *riAsmrReg = regValue;
    return true;
}

/*
 * Set or clear the CMR flag for the sampling pin.
 */
static bool stm32DriverTouchSetCmrFlag (bool flagState)
{
    const gmosPalTouchConfig_t* palConfig = activeTouchChannel->palConfig;
    volatile uint32_t* riCmrReg;
    uint8_t samplingPinIndex;
    uint32_t regValue;

    // Select the registers to use for the sampling pin.
    switch (palConfig->samplingPinId & 0x0700) {
        case STM32_GPIO_BANK_A :
            riCmrReg = &(RI->CMR1);
            break;
        case STM32_GPIO_BANK_B :
            riCmrReg = &(RI->CMR2);
            break;
        case STM32_GPIO_BANK_C :
            riCmrReg = &(RI->CMR3);
            break;
        #ifdef GPIOF
        case STM32_GPIO_BANK_F :
            riCmrReg = &(RI->CMR4);
            break;
        #endif
        #ifdef GPIOG
        case STM32_GPIO_BANK_G :
            riCmrReg = &(RI->CMR5);
            break;
        #endif
        default :
            return false;
    }

    // Enable timer capture control for the sampling pin.
    samplingPinIndex = palConfig->samplingPinId & 0x0F;
    regValue = *riCmrReg;
    if (flagState) {
        regValue |= 1 << samplingPinIndex;
    } else {
        regValue &= ~(1 << samplingPinIndex);
    }
    *riCmrReg = regValue;
    return true;
}

/*
 * Start the timers running with the specified maximum interval for
 * timer 10.
 */
static void stm32DriverTouchStartTimers (uint16_t maxDelay)
{
    TIM9->CNT = 0;
    TIM10->CNT = 0;
    TIM10->ARR = maxDelay;
    TIM10->SR &= ~(TIM_SR_UIF | TIM_SR_CC1IF | TIM_SR_CC1OF);
    TIM10->CR1 |= TIM_CR1_CEN;
    TIM9->CR1 |= TIM_CR1_CEN;
}

/*
 * Discharge the sampling capacitors by driving the GPIO open drain
 * output low.
 */
static inline void stm32DriverTouchSampleClear (void)
{
    const gmosPalTouchConfig_t* palConfig = activeTouchChannel->palConfig;
    uint32_t dischargeDelay;

    // Set the sampling capacitor pin as an output. The associated
    // output data register will always be zero.
    stm32DriverTouchSetGpioMode (palConfig->samplingPinId, 1);

    // Set the timer 10 auto reload limit to the capacitor discharge
    // time.
    dischargeDelay = (CAPACITOR_DISCHARGE_DELAY *
        TOUCH_SENSOR_PWM_FREQUENCY) / 1000000;
    stm32DriverTouchStartTimers (dischargeDelay);
}

/*
 * Start the sampling process for the active touch channel.
 */
static inline bool stm32DriverTouchSampleStart (void)
{
    const gmosPalTouchConfig_t* palConfig = activeTouchChannel->palConfig;
    uint32_t sampleDelay;

    // Place sampling capacitor pin in high impedance input state after
    // capacitor discharge has completed.
    stm32DriverTouchSetGpioMode (palConfig->samplingPinId, 0);

    // Set the CMR flag for the sampling pin.
    if (!stm32DriverTouchSetCmrFlag (true)) {
        return false;
    }

    // Set the ASMR flag for the sensor pin.
    if (!stm32DriverTouchSetAsmrFlag (true)) {
        return false;
    }

    // Place the sensor pin in alternate function mode.
    stm32DriverTouchSetGpioMode (palConfig->sensorPinId, 2);

    // Clear the event flags prior to starting the capture.
    gmosEventResetBits (&touchSensorEvent);

    // Reset timer 10 and then enable timer 9 to start the capture.
    sampleDelay = GMOS_CONFIG_STM32_TOUCH_ACQ_MAX_LEVEL + 1;
    stm32DriverTouchStartTimers (sampleDelay);
    return true;
}

/*
 * Process the touch channel results on completion.
 */
static inline bool stm32DriverTouchSampleProcess (uint32_t eventData)
{
    const gmosPalTouchConfig_t* palConfig = activeTouchChannel->palConfig;
    gmosPalTouchCallback_t touchCallback;

    // Clear the ASMR flag for the sensor pin.
    if (!stm32DriverTouchSetAsmrFlag (false)) {
        return false;
    }

    // Clear the CMR flag for the sampling pin.
    if (!stm32DriverTouchSetCmrFlag (false)) {
        return false;
    }

    // Set the GPIO pins into their idle state.
    stm32DriverTouchSetGpioMode (palConfig->sensorPinId, 0);
    stm32DriverTouchSetGpioMode (palConfig->samplingPinId, 0);

    // Update channel sensing level.
    if ((eventData & TOUCH_SENSOR_EVENT_CAPTURE) != 0) {
        gmosDriverTouchChannelFilter (activeTouchChannel,
            eventData & TOUCH_SENSOR_EVENT_COUNT_MASK);
        touchCallback = activeTouchChannel->channelGroup->palTouchCallback;
        if (touchCallback != NULL) {
            touchCallback (activeTouchChannel,
                gmosDriverTouchChannelRead (activeTouchChannel));
        }
    }

    // Clear the event flags on completion.
    gmosEventResetBits (&touchSensorEvent);
    return true;
}

/*
 * Implement the touch sensor processing task.
 */
static gmosTaskStatus_t stm32DriverTouchTaskHandler (void* nullData)
{
    gmosDriverTouchChannel_t* nextChannel;
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    uint32_t eventData;
    int32_t nextDelay;

    // Implement the main processing task state machine.
    switch (touchSensorState) {

        // From the idle state, select the first channel in the list and
        // initiate channel processing.
        case TOUCH_SENSOR_STATE_IDLE :
            activeTouchChannel = touchChannelList;
            touchSensorState = TOUCH_SENSOR_STATE_CHANNEL_CLEAR;
            touchSensorTimestamp +=
                GMOS_MS_TO_TICKS (GMOS_CONFIG_DRIVER_TOUCH_SAMPLE_INTERVAL);
            break;

        // Wait for the sample interval period to expire.
        case TOUCH_SENSOR_STATE_SLEEP :
            nextDelay = (int32_t)
                (touchSensorTimestamp - gmosPalGetTimer ());
            if (nextDelay > 0) {
                taskStatus = GMOS_TASK_RUN_LATER ((uint32_t) nextDelay);
            } else {
                touchSensorState = TOUCH_SENSOR_STATE_IDLE;
            }
            break;

        // Select the next channel in the list and initiate channel
        // processing.
        case TOUCH_SENSOR_STATE_CHANNEL_SELECT :
            nextChannel = activeTouchChannel->palData->nextChannel;
            if (nextChannel == NULL) {
                touchSensorState = TOUCH_SENSOR_STATE_SLEEP;
            } else {
                activeTouchChannel = nextChannel;
                touchSensorState = TOUCH_SENSOR_STATE_CHANNEL_CLEAR;
            }
            break;

        // Clear the channel capacitance prior to processing.
        case TOUCH_SENSOR_STATE_CHANNEL_CLEAR :
            stm32DriverTouchSampleClear ();
            touchSensorState = TOUCH_SENSOR_STATE_CHANNEL_START;
            taskStatus = GMOS_TASK_SUSPEND;
            break;

        // Start channel processing once the capacitor discharge delay
        // has elapsed.
        case TOUCH_SENSOR_STATE_CHANNEL_START :
            eventData = gmosEventGetBits (&touchSensorEvent);
            if (eventData == 0) {
                taskStatus = GMOS_TASK_SUSPEND;
            } else if (stm32DriverTouchSampleStart ()) {
                touchSensorState = TOUCH_SENSOR_STATE_CHANNEL_PROCESS;
                taskStatus = GMOS_TASK_SUSPEND;
            } else {
                touchSensorState = TOUCH_SENSOR_STATE_FAILED;
            }
            break;

        // Process the channel results if a channel event is ready.
        case TOUCH_SENSOR_STATE_CHANNEL_PROCESS :
            eventData = gmosEventGetBits (&touchSensorEvent);
            if (eventData == 0) {
                taskStatus = GMOS_TASK_SUSPEND;
            } else if (stm32DriverTouchSampleProcess (eventData)) {
                touchSensorState = TOUCH_SENSOR_STATE_CHANNEL_SELECT;
            } else {
                touchSensorState = TOUCH_SENSOR_STATE_FAILED;
            }
            break;

        // TODO: Deal correctly with failures.
        default :
            break;
    }
    return taskStatus;
}

// Define the touch sensor task.
GMOS_TASK_DEFINITION (stm32DriverTouchTask,
    stm32DriverTouchTaskHandler, void);

/*
 * Perform one-time setup of common touch sensor processing components.
 */
static inline bool stm32DriverTouchSetup (void)
{
    uint32_t pwmMaxCountValue;
    uint32_t pwmThreshold1;
    uint32_t pwmThreshold2;

    // Enable peripheral clock for the comparator component, which is
    // required for accessing the analogue routing registers.
    RCC->APB1ENR |= RCC_APB1ENR_COMPEN;

    // Enable peripheral clocks for TIM9 and TIM10 (including low power
    // mode).
    RCC->APB2ENR |= RCC_APB2ENR_TIM9EN | RCC_APB2ENR_TIM10EN;
    RCC->APB2LPENR |= RCC_APB2LPENR_TIM9LPEN | RCC_APB2LPENR_TIM10LPEN;

    // Calculate the PWM counter settings to yield a nominal 250kHz
    // PWM signal using the up/down counter.
    pwmMaxCountValue = 0xFFFFFFFE &
        (GMOS_CONFIG_STM32_APB2_CLOCK / (2 * TOUCH_SENSOR_PWM_FREQUENCY));
    if (pwmMaxCountValue < 8) {
        pwmMaxCountValue = 8;
    }
    pwmThreshold1 = (5 * pwmMaxCountValue) / 8;
    pwmThreshold2 = (3 * pwmMaxCountValue) / 8;

    // Configure the TIM9 timer in centre aligned mode to generate PWM
    // signals on OC1 and OC2.
    TIM9->CR1 |= TIM_CR1_CMS_0;        // Centre aligned mode 1.
    TIM9->CR2 |= TIM_CR2_MMS_2;        // OC1REF used as trigger out.
    TIM9->SMCR |= TIM_SMCR_MSM |       // Master mode timer.
        (1 << TIM_SMCR_TS_Pos) |       // ITR1 (from TIM3 remap mux).
        (5 << TIM_SMCR_SMS_Pos);       // Clock enabled when TRGI is high.
    TIM9->EGR |= TIM_EGR_UG;           // Update settings on timer start.
    TIM9->CCMR1 |=
        (7 << TIM_CCMR1_OC1M_Pos) |    // Output 1 PWM mode 2.
        (6 << TIM_CCMR1_OC2M_Pos);     // Output 2 PWM mode 1.
    TIM9->CCER |= TIM_CCER_CC1E |      // Enable compare output 1.
        TIM_CCER_CC2E;                 // Enable compare output 2.
    TIM9->ARR = pwmMaxCountValue;      // Upper counter limit.
    TIM9->CCR1 = pwmThreshold1;        // PWM output threshold 1.
    TIM9->CCR2 = pwmThreshold2;        // PWM output threshold 2.
    TIM9->OR |= TIM9_OR_ITR1_RMP;      // Mux ITR1 to touch sense I/O.

    // Configure TIM10 in slave mode with the clock signal generated by
    // TIM9. In addition, IC1 is enabled to capture the counter value on
    // detection of an end of acquisition.
    TIM10->SMCR |= TIM_SMCR_ECE;       // Use external clock mode.
    TIM10->DIER |= TIM_DIER_UIE |      // Interrupt on timeout.
        TIM_DIER_CC1IE;                // Interrupt on timer capture.
    TIM10->CCMR1 |= TIM_CCMR1_CC1S_0 | // Capture on TI1.
        (0x07 << TIM_CCMR1_IC1F_Pos);  // 8x filter at 1/4 rate.
    TIM10->CCER |= TIM_CCER_CC1E;      // Capture enable.
    TIM10->OR |= TIM_OR_TI1_RMP_RI |   // Channel 1 from routing block.
        TIM_OR_ETR_RMP;                // ETR connected to TIM9_TRGO.

    // Enable NVIC interrupts with default priority.
    NVIC_EnableIRQ (TIM10_IRQn);

    // Run the touch sensing task.
    touchSensorState = TOUCH_SENSOR_STATE_IDLE;
    touchSensorTimestamp = gmosPalGetTimer ();
    stm32DriverTouchTask_start (&touchSensorTask, NULL, "Touch Sensor");

    return true;
}

/*
 * Initialises a capacitive touch sensing channel for subsequent use.
 */
bool gmosDriverTouchChannelPalInit (gmosDriverTouchChannel_t* touchChannel)
{
    gmosPalTouchState_t* palData = touchChannel->palData;
    const gmosPalTouchConfig_t* palConfig = touchChannel->palConfig;
    uint8_t samplingPinIndex;
    uint8_t routingIndex;

    // Perform one-time setup if required.
    if (touchChannelList == NULL) {
        if (!stm32DriverTouchSetup ()) {
            return false;
        }
    }

    // Configure the channel sensor GPIO for alternative mode connection
    // to the routing interface. Then set it to be a high impedance
    // input. This is the default inactive state.
    gmosDriverGpioAltModeInit (palConfig->sensorPinId,
        GMOS_DRIVER_GPIO_OUTPUT_PUSH_PULL,
        STM32_GPIO_DRIVER_SLEW_SLOW,
        GMOS_DRIVER_GPIO_INPUT_PULL_NONE, 14);
    stm32DriverTouchSetGpioMode (palConfig->sensorPinId, 0);

    // Configure the capacitive sampling GPIO as a high impedance input.
    // This is the default inactive state.
    gmosDriverGpioPinInit (palConfig->samplingPinId,
        GMOS_DRIVER_GPIO_OUTPUT_OPEN_DRAIN,
        STM32_GPIO_DRIVER_SLEW_SLOW,
        GMOS_DRIVER_GPIO_INPUT_PULL_NONE);

    // Disable hysterisis and select sampling mode for the capacitive
    // sampling GPIO.
    samplingPinIndex = palConfig->samplingPinId & 0x0F;
    switch (palConfig->samplingPinId & 0x0700) {
        case STM32_GPIO_BANK_A :
            routingIndex = ascrRegMapGpioA [samplingPinIndex];
            RI->HYSCR1 |= (1 << samplingPinIndex);
            RI->CICR1 |= (1 << samplingPinIndex);
            break;
        case STM32_GPIO_BANK_B :
            routingIndex = ascrRegMapGpioB [samplingPinIndex];
            RI->HYSCR1 |= (1 << (16 + samplingPinIndex));
            RI->CICR2 |= (1 << samplingPinIndex);
            break;
        case STM32_GPIO_BANK_C :
            routingIndex = ascrRegMapGpioC [samplingPinIndex];
            RI->HYSCR2 |= (1 << samplingPinIndex);
            RI->CICR3 |= (1 << samplingPinIndex);
            break;
        #ifdef GPIOF
        case STM32_GPIO_BANK_F :
            routingIndex = ascrRegMapGpioF [samplingPinIndex];
            RI->HYSCR3 |= (1 << (16 + samplingPinIndex));
            RI->CICR4 |= (1 << samplingPinIndex);
            break;
        #endif
        #ifdef GPIOG
        case STM32_GPIO_BANK_G :
            routingIndex = ascrRegMapGpioG [samplingPinIndex];
            RI->HYSCR4 |= (1 << samplingPinIndex);
            RI->CICR5 |= (1 << samplingPinIndex);
            break;
        #endif
        default :
            return false;
    }

    // Implement analogue signal routing for signals groups.
    if (routingIndex < 32) {
        RI->ASCR1 |= (1 << routingIndex);
    } else if (routingIndex < 64) {
        RI->ASCR2 |= (1 << (routingIndex - 32));
    } else {
        return false;
    }

    // Add the channel to the linked list of configured channels.
    palData->nextChannel = touchChannelList;
    touchChannelList = touchChannel;
    return true;
}

/*
 * Implement the touch sensor processing ISR.
 */
void gmosPalIsrTIM10 (void)
{
    uint32_t eventData;

    // Populate the event data.
    if ((TIM10->SR & (TIM_SR_CC1IF | TIM_SR_CC1OF)) != 0) {
        eventData = TOUCH_SENSOR_EVENT_CAPTURE;
        eventData |= (TIM10->CCR1 & TOUCH_SENSOR_EVENT_COUNT_MASK);
    } else {
        eventData = TOUCH_SENSOR_EVENT_TIMEOUT;
    }

    // Disable timers and clear interrupts.
    TIM9->CR1 &= ~TIM_CR1_CEN;
    TIM10->CR1 &= ~TIM_CR1_CEN;
    TIM10->SR &= ~(TIM_SR_UIF | TIM_SR_CC1IF | TIM_SR_CC1OF);

    // Signal the event.
    gmosEventAssignBits (&touchSensorEvent, eventData);
}
