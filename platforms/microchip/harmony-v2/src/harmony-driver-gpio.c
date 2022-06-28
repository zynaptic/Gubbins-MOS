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
 * Implements GPIO driver functionality for Microchip devices that
 * utilise the Harmony V2 vendor framework.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-platform.h"
#include "gmos-driver-gpio.h"
#include "harmony-driver-gpio.h"

// Use the Harmony system library for GPIO.
#include "system/ports/sys_ports.h"
#include "system/int/sys_int.h"

// Specify the dedicated pins that may be remapped for external
// interrupt inputs.
static const uint32_t gpioExtiPinMap [] = HARMONY_GPIO_EXTINT_PINS;

// Specify the lookup from interrupt ID to Harmony global interrupt
// source ID.
static const INT_SOURCE gpioExtiGlobalSourceMap [] = {
    INT_SOURCE_EXTERNAL_0, INT_SOURCE_EXTERNAL_1,
    INT_SOURCE_EXTERNAL_2, INT_SOURCE_EXTERNAL_3,
    INT_SOURCE_EXTERNAL_4 };

// Specify the lookup from interrupt ID to Harmony external interrupt
// source ID.
static const INT_EXTERNAL_SOURCES gpioExtiLocalSourceMap [] = {
    INT_EXTERNAL_INT_SOURCE0, INT_EXTERNAL_INT_SOURCE1,
    INT_EXTERNAL_INT_SOURCE2, INT_EXTERNAL_INT_SOURCE3,
    INT_EXTERNAL_INT_SOURCE4 };

// Specify the lookup from interrupt ID to Harmony interrupt vector.
static const INT_VECTOR gpioExtiVectorsMap [] = {
    INT_VECTOR_INT0, INT_VECTOR_INT1,
    INT_VECTOR_INT2, INT_VECTOR_INT3,
    INT_VECTOR_INT4 };

// Specify the pins that have been remapped to external interrupts.
static uint16_t gpioExtiPinSelect [HARMONY_GPIO_EXTINT_NUM];

// Provide mapping of external interrupt lines to interrupt service
// routines.
static gmosDriverGpioIsr_t gpioIsrMap [HARMONY_GPIO_EXTINT_NUM] = { NULL };

// Provide mapping of external interrupt lines to interrupt service
// routine data items.
static void* gpioIsrDataMap [HARMONY_GPIO_EXTINT_NUM] = { NULL };

/*
 * Initialises a general purpose IO pin for conventional use. The pin
 * should have been defined and initialised as a GPIO in the Harmony
 * framework, so this function will just apply the specified pin
 * options.
 */
bool gmosDriverGpioPinInit (uint16_t gpioPinId, bool openDrain,
    uint8_t driveStrength, int8_t biasResistor)
{
    PORTS_CHANNEL portChannel = (gpioPinId >> 8) & 0x0F;
    PORTS_BIT_POS portPin = gpioPinId & 0x0F;

    // Set the open drain option if supported.
    if (openDrain) {
        SYS_PORTS_PinOpenDrainEnable (PORTS_ID_0, portChannel, portPin);
    } else {
        SYS_PORTS_PinOpenDrainDisable (PORTS_ID_0, portChannel, portPin);
    }

    // Enable pull up resistor if required.
    if (biasResistor == GMOS_DRIVER_GPIO_INPUT_PULL_UP) {
        SYS_PORTS_PinPullDownDisable (PORTS_ID_0, portChannel, portPin);
        SYS_PORTS_PinPullUpEnable (PORTS_ID_0, portChannel, portPin);
    }

    // Enable pull down resistor if required.
    else if (biasResistor == GMOS_DRIVER_GPIO_INPUT_PULL_DOWN) {
        SYS_PORTS_PinPullUpDisable (PORTS_ID_0, portChannel, portPin);
        SYS_PORTS_PinPullDownEnable (PORTS_ID_0, portChannel, portPin);
    }

    // Disable all bias resistors.
    else {
        SYS_PORTS_PinPullUpDisable (PORTS_ID_0, portChannel, portPin);
        SYS_PORTS_PinPullDownDisable (PORTS_ID_0, portChannel, portPin);
    }
    return true;
}

/*
 * Sets a general purpose IO pin as a conventional input, using the
 * configuration previously assigned by the 'gmosDriverGpioPinInit'
 * function.
 */
bool gmosDriverGpioSetAsInput (uint16_t gpioPinId)
{
    PORTS_CHANNEL portChannel = (gpioPinId >> 8) & 0x0F;
    PORTS_BIT_POS portPin = gpioPinId & 0x0F;

    SYS_PORTS_PinDirectionSelect (PORTS_ID_0,
        SYS_PORTS_DIRECTION_INPUT, portChannel, portPin);
    return true;
}

/*
 * Sets a general purpose IO pin as a conventional output, using the
 * configuration previously assigned by the 'gmosDriverGpioPinInit'
 * function.
 */
bool gmosDriverGpioSetAsOutput (uint16_t gpioPinId)
{
    PORTS_CHANNEL portChannel = (gpioPinId >> 8) & 0x0F;
    PORTS_BIT_POS portPin = gpioPinId & 0x0F;

    SYS_PORTS_PinDirectionSelect (PORTS_ID_0,
        SYS_PORTS_DIRECTION_OUTPUT, portChannel, portPin);
    return true;
}

/*
 * Sets the GPIO pin state. If the GPIO is configured as an output this
 * will update the output value.
 */
void gmosDriverGpioSetPinState (uint16_t gpioPinId, bool pinState)
{
    PORTS_CHANNEL portChannel = (gpioPinId >> 8) & 0x0F;
    PORTS_BIT_POS portPin = gpioPinId & 0x0F;

    SYS_PORTS_PinWrite (PORTS_ID_0, portChannel, portPin, pinState);
}

/*
 * Gets the GPIO pin state. If the GPIO is configured as an input this
 * will be the sampled value and if configured as an output this will
 * be the current output value.
 */
bool gmosDriverGpioGetPinState (uint16_t gpioPinId)
{
    PORTS_CHANNEL portChannel = (gpioPinId >> 8) & 0x0F;
    PORTS_BIT_POS portPin = gpioPinId & 0x0F;

    return SYS_PORTS_PinRead (PORTS_ID_0, portChannel, portPin);
}

/*
 * Initialises a general purpose IO pin for interrupt generation. This
 * should be called for each interrupt input GPIO pin prior to accessing
 * it via any of the other API functions. The interrupt is not enabled
 * at this stage. When using the Harmony framework, the pin should
 * already have been configured as an interrupt input using the pin
 * configuration tool, so pin function remapping is not implemented
 * here.
 */
bool gmosDriverGpioInterruptInit (uint16_t gpioPinId,
    gmosDriverGpioIsr_t gpioIsr, void* gpioIsrData,
    int8_t biasResistor)
{
    uint32_t i;
    uint8_t interruptId;
    INT_VECTOR intVector;

    // Determine if the specified pin can be remapped to an external
    // interrupt.
    interruptId = 0xFF;
    for (i = 0; i < sizeof (gpioExtiPinMap) / sizeof (uint32_t); i++) {
        uint32_t extiPinDef = gpioExtiPinMap [i];
        if ((extiPinDef & 0xFFFF) == gpioPinId) {
            interruptId = (uint8_t) (extiPinDef >> 24);
            break;
        }
    }
    if (interruptId >= HARMONY_GPIO_EXTINT_NUM) {
        return false;
    }

    // Check that the external interrupt is not reserved for use by
    // the Harmony framework and that it is not in use by another ISR.
    if ((GMOS_CONFIG_HARMONY_RESERVED_EXTI_MASK & (1 << interruptId)) != 0) {
        return false;
    }
    if (gpioIsrMap [interruptId] != NULL) {
        return false;
    }

    // Configure the common GPIO pin options.
    if (!gmosDriverGpioPinInit (gpioPinId, false,
        HARMONY_GPIO_DRIVER_SLEW_DEFAULT, biasResistor)) {
        return false;
    }

    // Clear any stale interrupts.
    SYS_INT_SourceDisable (gpioExtiGlobalSourceMap [interruptId]);
    PLIB_INT_SourceFlagClear(INT_ID_0, gpioExtiGlobalSourceMap [interruptId]);

    // Set the interrupt priority level.
    intVector = gpioExtiVectorsMap [interruptId];
    SYS_INT_VectorPrioritySet(intVector, INT_PRIORITY_LEVEL1);
    SYS_INT_VectorSubprioritySet(intVector, INT_SUBPRIORITY_LEVEL0);

    // Populate the interrupt handler table.
    gpioExtiPinSelect [interruptId] = gpioPinId;
    gpioIsrMap [interruptId] = gpioIsr;
    gpioIsrDataMap [interruptId] = gpioIsrData;
    return true;
}

/*
 * Enables a GPIO interrupt for rising and/or falling edge detection.
 * This should be called after initialising a general purpose IO pin
 * as an interrupt source in order to receive interrupt notifications.
 */
void gmosDriverGpioInterruptEnable (uint16_t gpioPinId,
    bool risingEdge, bool fallingEdge)
{
    uint32_t i;
    INT_EXTERNAL_EDGE_TRIGGER edgeTrigger;

    // The Harmony framework does not support triggering on both edges
    // of an external interrupt.
    GMOS_ASSERT (ASSERT_ERROR, (risingEdge != fallingEdge),
        "Microchip Harmony does not support interrupts on both edges.");

    // Find the matching GPIO pin ID.
    for (i = 0; i < HARMONY_GPIO_EXTINT_NUM; i++) {
        if (gpioExtiPinSelect [i] == gpioPinId) {
            break;
        }
    }
    if (i >= HARMONY_GPIO_EXTINT_NUM) {
        return;
    }

    // Select rising or falling edge trigger.
    if (fallingEdge) {
        edgeTrigger = INT_EDGE_TRIGGER_FALLING;
    } else {
        edgeTrigger = INT_EDGE_TRIGGER_RISING;
    }
    SYS_INT_ExternalInterruptTriggerSet (
        gpioExtiLocalSourceMap [i], edgeTrigger);

    // Clear any stale interrupts.
    PLIB_INT_SourceFlagClear(INT_ID_0, gpioExtiGlobalSourceMap [i]);

    // Enable the interrupt.
    SYS_INT_SourceEnable (gpioExtiGlobalSourceMap [i]);
}

/*
 * Disables a GPIO interrupt for the specified GPIO pin. This should be
 * called after enabling a general purpose IO pin as an interrupt source
 * in order to stop receiving interrupt notifications.
 */
void gmosDriverGpioInterruptDisable (uint16_t gpioPinId)
{
    uint32_t i;

    // Find the matching GPIO pin ID.
    for (i = 0; i < HARMONY_GPIO_EXTINT_NUM; i++) {
        if (gpioExtiPinSelect [i] == gpioPinId) {
            break;
        }
    }
    if (i >= HARMONY_GPIO_EXTINT_NUM) {
        return;
    }

    // Enable the interrupt.
    SYS_INT_SourceDisable (gpioExtiGlobalSourceMap [i]);
}

// Implement ISR for external interrupt 0.
#if ((GMOS_CONFIG_HARMONY_RESERVED_EXTI_MASK & 0x01) == 0)
void __ISR (_EXTERNAL_0_VECTOR, IPL2AUTO) harmonyIsrExti0 (void) {
    PLIB_INT_SourceFlagClear(INT_ID_0, INT_SOURCE_EXTERNAL_0);
    if (gpioIsrMap [0] != NULL) {
        gpioIsrMap [0] (gpioIsrDataMap [0]);
    }
}
#endif

// Implement ISR for external interrupt 1.
#if ((GMOS_CONFIG_HARMONY_RESERVED_EXTI_MASK & 0x02) == 0)
void __ISR (_EXTERNAL_1_VECTOR, IPL2AUTO) harmonyIsrExti1 (void) {
    PLIB_INT_SourceFlagClear(INT_ID_0, INT_SOURCE_EXTERNAL_1);
    if (gpioIsrMap [1] != NULL) {
        gpioIsrMap [1] (gpioIsrDataMap [1]);
    }
}
#endif

// Implement ISR for external interrupt 2.
#if ((GMOS_CONFIG_HARMONY_RESERVED_EXTI_MASK & 0x04) == 0)
void __ISR (_EXTERNAL_2_VECTOR, IPL2AUTO) harmonyIsrExti2 (void) {
    PLIB_INT_SourceFlagClear(INT_ID_0, INT_SOURCE_EXTERNAL_2);
    if (gpioIsrMap [2] != NULL) {
        gpioIsrMap [2] (gpioIsrDataMap [2]);
    }
}
#endif

// Implement ISR for external interrupt 3.
#if ((GMOS_CONFIG_HARMONY_RESERVED_EXTI_MASK & 0x08) == 0)
void __ISR (_EXTERNAL_3_VECTOR, IPL2AUTO) harmonyIsrExti3 (void) {
    PLIB_INT_SourceFlagClear(INT_ID_0, INT_SOURCE_EXTERNAL_3);
    if (gpioIsrMap [3] != NULL) {
        gpioIsrMap [3] (gpioIsrDataMap [3]);
    }
}
#endif

// Implement ISR for external interrupt 4.
#if ((GMOS_CONFIG_HARMONY_RESERVED_EXTI_MASK & 0x10) == 0)
void __ISR (_EXTERNAL_4_VECTOR, IPL2AUTO) harmonyIsrExti4 (void) {
    PLIB_INT_SourceFlagClear(INT_ID_0, INT_SOURCE_EXTERNAL_4);
    if (gpioIsrMap [4] != NULL) {
        gpioIsrMap [4] (gpioIsrDataMap [4]);
    }
}
#endif
