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
 * This header defines the compile time configuration options used by the
 * Silicon Labs memory manager region layout.
 */

#ifndef SL_MEMORY_MANAGER_REGION_CONFIG_H
#define SL_MEMORY_MANAGER_REGION_CONFIG_H

/**
 * Specifies the stack size used by the memory manager. The current GubbinsMOS
 * configuration settings involve specifying a fixed heap size and allowing
 * that to define the available stack space, so this value is 'advisory'. In the
 * future this should be changed to an alternative approach which follows the
 * memory manager model of fixing the stack size and allowing the heap to occupy
 * the remaining memory.
 */
#ifndef SL_STACK_SIZE
#define SL_STACK_SIZE 0x8000
#endif

#endif // SL_MEMORY_MANAGER_REGION_CONFIG_H
