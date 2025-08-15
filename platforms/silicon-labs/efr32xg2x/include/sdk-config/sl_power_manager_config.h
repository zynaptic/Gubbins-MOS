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
 * the Silicon Labs Gecko SDK power manager implementation.
 */
#ifndef SL_POWER_MANAGER_CONFIG_H
#define SL_POWER_MANAGER_CONFIG_H

// Disable custom IRQ handler for external HF oscillator.
#define SL_POWER_MANAGER_CUSTOM_HF_OSCILLATOR_IRQ_HANDLER 0

// Disable fast wakeup (allows voltage scaling in EM2/3 mode).
#define SL_POWER_MANAGER_CONFIG_VOLTAGE_SCALING_FAST_WAKEUP 0

// Disable built in debugging features.
#define SL_POWER_MANAGER_DEBUG 0
#define SL_POWER_MANAGER_DEBUG_POOL_SIZE 0

// Disable support for EM4 shutoff operation.
#define SL_POWER_MANAGER_INIT_EMU_EM4_PIN_RETENTION_MODE \
        EMU_EM4CTRL_EM4IORETMODE_DISABLE

// Disable debug access in EM2 and EM4.
#define SL_POWER_MANAGER_INIT_EMU_EM2_DEBUG_ENABLE 0

#endif // SL_POWER_MANAGER_CONFIG_H
