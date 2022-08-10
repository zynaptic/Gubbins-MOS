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
 * Provides device specific configuration and support routines for
 * Raspberry Pi Pico RP2040 family devices.
 */

#include <stdint.h>
#include <stdbool.h>

#include "gmos-platform.h"
#include "gmos-mempool.h"
#include "gmos-scheduler.h"
#include "pico-device.h"
#include "hardware/irq.h"
#include "hardware/dma.h"

// Track DMA ISRs that have been enabled for each core.
static uint16_t enabledDmaIsrs0 = 0;
static uint16_t enabledDmaIsrs1 = 0;

// Store pointers to the attached DMA interrupt service routines.
static gmosPalDmaIsr_t attachedDmaIsrs [12];

/*
 * The device setup and scheduler loop are all implemented from the
 * main application entry point.
 */
int main(void)
{
    // Initialise the common platform components.
    gmosMempoolInit ();

    // Initialise the platform abstraction layer.
    gmosPalInit ();

    // Initialise the application code.
    gmosAppInit ();

    // Enter the scheduler loop. This is implemented in the 'main'
    // function to avoid adding an extra stack frame.
    gmosLifecycleNotify (SCHEDULER_STARTUP);
    while (true) {
        uint32_t execDelay = 0;
        while (execDelay == 0) {
            execDelay = gmosSchedulerStep ();
        }
        gmosPalIdle (execDelay);
    }
}

/*
 * Implement main handler for DMA ISR 0 on core 0.
 */
static void gmosPalDmaIsr0 (void)
{
    uint32_t channel;
    bool clearInt;

    // Service any outstanding DMA ISRs.
    for (channel = 0; channel < 12; channel++) {
        if (dma_channel_get_irq0_status (channel)) {
            if ((enabledDmaIsrs0 & (1 << channel)) != 0) {
                clearInt = attachedDmaIsrs [channel] ();
            } else {
                clearInt = true;
            }
            if (clearInt) {
                dma_channel_acknowledge_irq0 (channel);
            }
        }
    }
}

/*
 * Implement main handler for DMA ISR 1 on core 1.
 */
static void gmosPalDmaIsr1 (void)
{
    uint32_t channel;
    bool clearInt;

    // Service any outstanding DMA ISRs.
    for (channel = 0; channel < 12; channel++) {
        if (dma_channel_get_irq1_status (channel)) {
            if ((enabledDmaIsrs1 & (1 << channel)) != 0) {
                clearInt = attachedDmaIsrs [channel] ();
            } else {
                clearInt = true;
            }
            if (clearInt) {
                dma_channel_acknowledge_irq1 (channel);
            }
        }
    }
}

/*
 * Attaches a DMA interrupt service routine for the specified DMA
 * channel.
 */
bool gmosPalDmaIsrAttach (uint8_t channel, gmosPalDmaIsr_t isr)
{
    bool attachOk = false;
    uint16_t channelMask;

    // Protect the DMA configuration from concurrent access.
    gmosPalMutexLock ();

    // Check that the specified DMA channel is free.
    channelMask = (1 << channel);
    if ((channel >= 12) || (isr == NULL) ||
        ((enabledDmaIsrs0 & channelMask) != 0) ||
        ((enabledDmaIsrs1 & channelMask) != 0)) {
        goto out;
    }

    // Register the ISR for core 0.
    if (get_core_num () == 0) {

        // Hook up the DMA ISR0 on first registration.
        if (enabledDmaIsrs0 == 0) {
            irq_set_exclusive_handler (DMA_IRQ_0, gmosPalDmaIsr0);
            irq_set_enabled (DMA_IRQ_0, true);
        }
        enabledDmaIsrs0 |= channelMask;
        attachedDmaIsrs [channel] = isr;
        attachOk = true;
    }

    // Register the ISR for core 1.
    else if (get_core_num () == 1) {

        // Hook up the DMA ISR1 on first registration.
        if (enabledDmaIsrs1 == 0) {
            irq_set_exclusive_handler (DMA_IRQ_1, gmosPalDmaIsr1);
            irq_set_enabled (DMA_IRQ_1, true);
        }
        enabledDmaIsrs1 |= channelMask;
        attachedDmaIsrs [channel] = isr;
        attachOk = true;
    }

    // Clean up on exit.
out:
    gmosPalMutexUnlock ();
    return attachOk;
}

/*
 * Enables a DMA interrupt service routine for the specified DMA
 * channel. The corresponding ISR should previously have been attached
 * to the DMA interrupt handler using the same CPU core.
 */
bool gmosPalDmaIsrSetEnabled (uint8_t channel, bool enabled)
{
    uint16_t channelMask = (1 << channel);
    bool setOk = false;

    // Enable the ISR for core 0.
    if (get_core_num () == 0) {
        if ((enabledDmaIsrs0 & channelMask) != 0) {
            dma_channel_set_irq0_enabled (channel, enabled);
            setOk = true;
        }
    }

    // Enable the ISR for core 1.
    else if (get_core_num () == 1) {
        if ((enabledDmaIsrs1 & channelMask) != 0) {
            dma_channel_set_irq1_enabled (channel, enabled);
            setOk = true;
        }
    }
    return setOk;
}
