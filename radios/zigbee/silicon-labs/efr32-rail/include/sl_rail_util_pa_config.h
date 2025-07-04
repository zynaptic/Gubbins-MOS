/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2025 Zynaptic Limited
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
 * This header specifies the default configuration options to be used by
 * the EmberZNet Zigbee network stack for RAIL power amplifier settings.
 * These only apply to 'bare chip' targets, since pre-approved modules
 * must use the appropriate pre-compiled configuration library. The
 * default settings correspond to the EFR32xG24 Explorer Kit (BRD2703A).
 * If required, the application configuration file can override these
 * default values when targeting a different board.
 */

#ifndef SL_RAIL_UTIL_PA_CONFIG_H
#define SL_RAIL_UTIL_PA_CONFIG_H

#include "gmos-zigbee-config.h"
#include "rail_types.h"

/**
 * This configuration option sets the initial power amplifier level in
 * units of dBm/10 (such that a value of 100 corresponds to 10.0 dBm).
 */
#ifndef SL_RAIL_UTIL_PA_POWER_DECI_DBM
#define SL_RAIL_UTIL_PA_POWER_DECI_DBM 100
#endif

/**
 * This configuration option sets the power amplifier ramp up time as an
 * integer number of microseconds.
 */
#ifndef SL_RAIL_UTIL_PA_RAMP_TIME_US
#define SL_RAIL_UTIL_PA_RAMP_TIME_US 10
#endif

/**
 * This configuration option sets the input voltage of the power supply
 * to the power amplifier. This is the expected voltage applied to the
 * PA_VDD pin for a given board design, expressed as an integer number
 * of millivolts.
 */
#ifndef SL_RAIL_UTIL_PA_VOLTAGE_MV
#define SL_RAIL_UTIL_PA_VOLTAGE_MV 3300
#endif

/**
 * This configuration option specifies the 2.4 GHz radio transmit power
 * mode. The highest possible power mode is selected by default, but
 * other operating modes defined in the 'rail_types.h' header file may
 * be used instead.
 */
#ifndef SL_RAIL_UTIL_PA_SELECTION_2P4GHZ
#define SL_RAIL_UTIL_PA_SELECTION_2P4GHZ RAIL_TX_POWER_MODE_2P4GIG_HIGHEST
#endif

/*
 * This fixed configuration option disables low frequency radio
 * operation for all supported devices.
 */
#define SL_RAIL_UTIL_PA_SELECTION_SUBGHZ RAIL_TX_POWER_MODE_NONE

/*
 * This fixed configuration option selects the header file that contains
 * custom power amplifier curves. It selects the default Simplicity SDK
 * configuration file which has valid curves for the integrated power
 * amplifiers on all supported devices.
 */
#define SL_RAIL_UTIL_PA_CURVE_HEADER "pa_curves_efr32.h"

/*
 * This fixed configuration option selects the header file that contains
 * custom power amplifier curve type definitions. It selects the default
 * Simplicity SDK configuration file which has valid curve type
 * definitions for the integrated power amplifiers on all supported
 * devices.
 */
#define SL_RAIL_UTIL_PA_CURVE_TYPES "pa_curve_types_efr32.h"

/*
 * This fixed configuration option enables power amplifier calibration
 * for all supported devices.
 */
#define SL_RAIL_UTIL_PA_CALIBRATION_ENABLE 1

#endif // SL_RAIL_UTIL_PA_CONFIG_H
