/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2023-2025 Zynaptic Limited
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
 * Provides the main entry point and various device configuration and
 * setup routines for Silicon Labs EFR32xG2x family devices.
 */

#include <stdint.h>
#include <stdbool.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-mempool.h"
#include "gmos-scheduler.h"
#include "efr32-device.h"
#include "em_chip.h"
#include "em_cmu.h"
#include "sl_device_init_dcdc.h"
#include "sl_device_init_lfxo.h"
#include "sl_device_init_hfxo.h"
#include "sl_device_init_hfxo_config.h"
#include "sl_hfxo_manager.h"
#include "sl_interrupt_manager.h"

/*
 * Perform NVIC initialisation, setting all interrupts to the default
 * interrupt priority level.
 */
static inline void gmosPalNvicSetup (void)
{
    int32_t i;
    uint32_t defaultPriority;
    defaultPriority = sl_interrupt_manager_get_default_priority ();
    for (i = SVCall_IRQn; i < EXT_IRQ_COUNT; i++) {
        sl_interrupt_manager_set_irq_priority (i, defaultPriority);
    }
}

/*
 * Perform DC/DC regulator setup. This assumes that the target uses the
 * standard DC/DC buck converter configuration, as implemented on the
 * MGM240x radio modules.
 */
static inline void gmosPalRegulatorSetup (void)
{
    // Set the initial regulator configuration.
    sl_device_init_dcdc ();
}

/*
 * Perform high frequency crystal oscillator setup. This assumes that
 * the target uses the standard 39MHz crystal, as required for correct
 * radio operation.
 */
static inline void gmosPalHfxoSetup ()
{
    // Set the initial HFXO configuration.
    sl_device_init_hfxo ();

    // Initialise the HFXO manager hardware.
    sl_hfxo_manager_init_hardware ();

    // Initialise the HFXO management service from the SDK.
    sl_hfxo_manager_init ();
}

/*
 * Perform low frequency crystal oscillator setup for a standard
 * 32.768kHz crystal.
 */
static inline void gmosPalLfxoSetup (void)
{
    // Set the initial LFXO configuration.
    sl_device_init_lfxo ();
}

/*
 * Perform bus clock setup. This is currently based on the automatically
 * generated code which just enables all the bus clocks off the high
 * frequency oscillator. A more sophisticated implementation is required
 * which allows different clock configurations to be supported.
 */
static inline void gmosPalClockSetup (void)
{
    // Use the high frequency oscillator as the system clock.
#if (GMOS_CONFIG_EFR32_SYSTEM_CLOCK == SL_DEVICE_INIT_HFXO_FREQ)
    CMU_CLOCK_SELECT_SET (SYSCLK, HFXO);
#else
#error "System clock PLL is not currently supported."
#endif

    // Use the high frequency oscillator for all the main peripheral
    // group clocks.
#if defined(_CMU_EM01GRPACLKCTRL_MASK)
    CMU_CLOCK_SELECT_SET (EM01GRPACLK, HFXO);
#endif
#if defined(_CMU_EM01GRPBCLKCTRL_MASK)
    CMU_CLOCK_SELECT_SET (EM01GRPBCLK, HFXO);
#endif
#if defined(_CMU_EM01GRPCCLKCTRL_MASK)
    CMU_CLOCK_SELECT_SET (EM01GRPCCLK, HFXO);
#endif

    // Use the low frequency oscillator for all low power peripherals.
    CMU_CLOCK_SELECT_SET (EM23GRPACLK, LFXO);
    CMU_CLOCK_SELECT_SET (EM4GRPACLK, LFXO);
    CMU_CLOCK_SELECT_SET (SYSRTC, LFXO);
    CMU_CLOCK_SELECT_SET (WDOG0, LFXO);
#if WDOG_COUNT > 1
    CMU_CLOCK_SELECT_SET (WDOG1, LFXO);
#endif
}

/*
 * The device setup and scheduler loop are all implemented from the
 * main application entry point.
 */
int main(void)
{
    // Chip initialisation routine for revision errata workarounds.
    // This function must be called immediately in main().
    CHIP_Init ();

    // Initialise the platform abstraction layer components.
    gmosPalNvicSetup ();
    gmosPalRegulatorSetup ();
    gmosPalHfxoSetup ();
    gmosPalLfxoSetup ();
    gmosPalClockSetup ();

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
