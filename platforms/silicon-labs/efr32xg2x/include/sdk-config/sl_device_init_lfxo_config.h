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
 * Specify the default LFXO settings for use on system startup. The
 * default values are suitable for use on all current development
 * targets.
 */

#ifndef SL_DEVICE_INIT_LFXO_CONFIG_H
#define SL_DEVICE_INIT_LFXO_CONFIG_H

/**
 * Use an external crystal oscillator by default.
 */
#ifndef SL_DEVICE_INIT_LFXO_MODE
#define SL_DEVICE_INIT_LFXO_MODE cmuLfxoOscMode_Crystal
#endif

/**
 * Select the crystal oscillator capacitive tuning to be used. The
 * default value is suitable for use with the crystals included in the
 * standard Silicon Labs development kits and radio modules.
 */
#ifndef SL_DEVICE_INIT_LFXO_CTUNE
#define SL_DEVICE_INIT_LFXO_CTUNE 63
#endif

/**
 * Select the crystal oscillator precision in parts per million. The
 * default value is suitable for use with the crystals included in the
 * standard Silicon Labs development kits and radio modules.
 */
#ifndef SL_DEVICE_INIT_LFXO_PRECISION
#define SL_DEVICE_INIT_LFXO_PRECISION 50
#endif

/**
 * Select the crystal oscillator startup timeout delay using the startup
 * delay enumeration. The default value is suitable for use with the
 * crystals included in the standard Silicon Labs development kits and
 * radio modules.
 */
#ifndef SL_DEVICE_INIT_LFXO_TIMEOUT
#define SL_DEVICE_INIT_LFXO_TIMEOUT cmuLfxoStartupDelay_4KCycles
#endif

#endif // SL_DEVICE_INIT_LFXO_CONFIG_H
