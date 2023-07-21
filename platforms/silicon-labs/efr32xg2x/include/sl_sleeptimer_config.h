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
 * This header defines the compile time configuration options used by
 * the Silicon Labs Gecko SDK sleep timer implementation.
 */

#ifndef SL_SLEEPTIMER_CONFIG_H
#define SL_SLEEPTIMER_CONFIG_H

#include "gmos-config.h"

// This list specifies the available sleep timer options, copied from
// the default sleep timer configuration file.
#define SL_SLEEPTIMER_PERIPHERAL_DEFAULT 0
#define SL_SLEEPTIMER_PERIPHERAL_RTCC    1
#define SL_SLEEPTIMER_PERIPHERAL_PRORTC  2
#define SL_SLEEPTIMER_PERIPHERAL_RTC     3
#define SL_SLEEPTIMER_PERIPHERAL_SYSRTC  4
#define SL_SLEEPTIMER_PERIPHERAL_BURTC   5
#define SL_SLEEPTIMER_PERIPHERAL_WTIMER  6
#define SL_SLEEPTIMER_PERIPHERAL_TIMER   7

// Selects the counter to use for the sleep timer. This will be the
// SYSRTC counter for EFR32xG2x devices to ensure that it is correctly
// configured for use by the vendor radio libraries.
#define SL_SLEEPTIMER_PERIPHERAL \
        SL_SLEEPTIMER_PERIPHERAL_SYSRTC

// Wallclock support is provided by the common GubbinsMOS RTC library.
#define SL_SLEEPTIMER_WALLCLOCK_CONFIG 0

// Halt the sleep timer in debug.
#define SL_SLEEPTIMER_DEBUGRUN 0

#endif
