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
 * This header defines the compile time configuration options used by
 * the Silicon Labs Gecko SDK HFXO clock manager implementation.
 */

#ifndef SL_HFXO_MANAGER_CONFIG_H
#define SL_HFXO_MANAGER_CONFIG_H

// Disables the application specific HFXO IRQ handler.
#define SL_HFXO_MANAGER_CUSTOM_HFXO_IRQ_HANDLER  0

// Disables sleepy crystal support.
#define SL_HFXO_MANAGER_SLEEPY_CRYSTAL_SUPPORT  0

#endif // SL_HFXO_MANAGER_CONFIG_H
