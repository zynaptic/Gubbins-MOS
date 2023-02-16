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
 * Selects the EFR32xG2x printf library support options for the compact
 * printf implementation by Marco Paland.
 */

#ifndef PRINTF_CONFIG_H
#define PRINTF_CONFIG_H

// Disable floating point (%f) support.
#define PRINTF_DISABLE_SUPPORT_FLOAT

// Disable pointer difference (%t) support.
#define PRINTF_DISABLE_SUPPORT_PTRDIFF_T

#endif // PRINTF_CONFIG_H
