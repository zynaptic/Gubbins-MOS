/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2026 Zynaptic Limited
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
 * This header file specifies the Simplicity SDK components which need
 * to be included in the OpenThread stack build.
 */

#ifndef SL_COMPONENT_CATALOG_OPENTHREAD_H
#define SL_COMPONENT_CATALOG_OPENTHREAD_H

/*
 * Include the common modular components for the platform.
 */
#include "sdk-config/sl_component_catalog.h"

/*
 * Select the modular components for the OpenThread stack.
 */
#define SL_CATALOG_EMLIB_RMU_PRESENT

#endif // SL_COMPONENT_CATALOG_OPENTHREAD_H
