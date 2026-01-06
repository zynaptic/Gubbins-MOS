/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2025-2026 Zynaptic Limited
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
 * This header defines the set of Silicon Labs SDK components that are
 * used in the standard build process.
 */

#ifndef SL_COMPONENT_CATALOG_H
#define SL_COMPONENT_CATALOG_H

// Enable the HFXO management component.
#define SL_CATALOG_HFXO_MANAGER_PRESENT 1

// Enable the sleep timer service component.
#define SL_CATALOG_SLEEPTIMER_PRESENT 1

// Enable the power management service.
#define SL_CATALOG_POWER_MANAGER_PRESENT 1

// Set persistent storage to use the Silicon Labs NVM3 non volatile
// memory library.
#define SL_CATALOG_NVM3_PRESENT 1

#endif // SL_COMPONENT_CATALOG_H
