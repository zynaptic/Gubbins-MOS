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
#include "em_emu.h"
#include "em_cmu.h"

// If supported, select the pre-calibrated high frequency oscillator
// tuning capacitor value from a fixed offset in the USERDATA page.
#define HFXO_MFG_CTUNE_ADDR 0x0FE00100UL
#define HFXO_MFG_CTUNE_VAL  (*((uint16_t *) (HFXO_MFG_CTUNE_ADDR)))

/*
 * Perform NVIC initialisation, setting all interrupts to the default
 * interrupt priority level.
 */
static inline void gmosPalNvicSetup (void)
{
    IRQn_Type i;
    for (i = SVCall_IRQn; i < EXT_IRQ_COUNT; i++) {
        NVIC_SetPriority(i, CORE_INTERRUPT_DEFAULT_PRIORITY);
    }
}

/*
 * Perform DC/DC regulator setup. This assumes that the target uses the
 * standard DC/DC buck converter configuration, as implemented on the
 * MGM240x radio modules.
 */
static inline void gmosPalRegulatorSetup (void)
{
    // Set the default DC/DC regulator configuration.
    EMU_DCDCInit_TypeDef dcdcInit = EMU_DCDCINIT_DEFAULT;
    EMU_DCDCInit(&dcdcInit);

    // Set DC/DC peak current to the recommended 60mA for the active and
    // sleep power modes (see reference manual table 11.6).
    EMU_DCDCSetPFMXModePeakCurrent (9);
}

/*
 * Perform high frequency crystal oscillator setup. This assumes that
 * the target uses the standard 39MHz crystal, as required for correct
 * radio operation.
 */
static inline void gmosPalHfxoSetup ()
{
    int ctune = -1;

    // Select the default high frequency crystal oscillator settings.
    CMU_HFXOInit_TypeDef hfxoInit = CMU_HFXOINIT_DEFAULT;

    // Use tuning value from DEVINFO if available (for PCB modules).
#ifdef _DEVINFO_MODXOCAL_HFXOCTUNEXIANA_MASK
    if ((DEVINFO->MODULEINFO & _DEVINFO_MODULEINFO_HFXOCALVAL_MASK) == 0) {
        ctune = DEVINFO->MODXOCAL & _DEVINFO_MODXOCAL_HFXOCTUNEXIANA_MASK;
    }
#endif

    // Use tuning value from USERDATA page if not already set.
    if ((ctune < 0) && (HFXO_MFG_CTUNE_VAL != 0xFFFF)) {
        ctune = HFXO_MFG_CTUNE_VAL;
    }

    // Use fixed tuning value if not already set.
    if (ctune < 0) {
        ctune = GMOS_CONFIG_EFR32_HFXO_FIXED_CTUNE_VAL;
    }

    // Adjust tuning capacitors to the set value. The output tuning
    // capacitor includes a delta value which accounts for internal
    // chip load imbalance on some series 2 chips.
    if (ctune >= 0) {
        int ctuneMax = (int) (_HFXO_XTALCTRL_CTUNEXOANA_MASK >>
            _HFXO_XTALCTRL_CTUNEXOANA_SHIFT);
        hfxoInit.ctuneXiAna = (uint8_t) ctune;
        ctune += CMU_HFXOCTuneDeltaGet();
        if (ctune < 0) {
            ctune = 0;
        } else if (ctune > ctuneMax) {
            ctune = ctuneMax;
        }
        hfxoInit.ctuneXoAna = (uint8_t) ctune;
    }
    SystemHFXOClockSet (39000000);
    CMU_HFXOInit (&hfxoInit);
}

/*
 * Perform low frequency crystal oscillator setup for a standard
 * 32.768kHz crystal.
 */
static inline void gmosPalLfxoSetup (void)
{
    CMU_LFXOInit_TypeDef lfxoInit = CMU_LFXOINIT_DEFAULT;
    CMU_LFXOInit (&lfxoInit);
    CMU_LFXOPrecisionSet (GMOS_CONFIG_EFR32_LFXO_PRECISION);
}

/*
 * Perform bus clock setup. This is currently based on the automatically
 * generated code which just enables all the bus clocks off the high
 * frequency oscillator. A more sophisticated implementation is required
 * which allows different clock configurations to be supported.
 */
static inline void gmosPalClockSetup (void)
{
    // Use the high frequency oscillator for all system and bus clocks.
    CMU_CLOCK_SELECT_SET (SYSCLK, HFXO);
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
