/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2023-2025 Zynaptic Limited
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
 * This header is used to set the default configuration options for the
 * main NVM3 (EEPROM) instance. It is a modified copy of the default
 * options provided by the Silicon Labs SDK.
 */

#ifndef NVM3_DEFAULT_CONFIG_H
#define NVM3_DEFAULT_CONFIG_H

/**
 * NVM3 Default Instance Cache Size. Number of NVM3 objects to cache. To
 * reduce access times this number should be equal to or higher than the
 * number of NVM3 objects in the default NVM3 instance. Original default
 * value 200.
 */
#ifndef NVM3_DEFAULT_CACHE_SIZE
#define NVM3_DEFAULT_CACHE_SIZE 200
#endif

/**
 * NVM3 Default Instance Maximum Object Size. Maximum NVM3 object size
 * that can be stored. Original default value 254.
 */
#ifndef NVM3_DEFAULT_MAX_OBJECT_SIZE
#define NVM3_DEFAULT_MAX_OBJECT_SIZE  1020
#endif

/**
 * NVM3 Default Instance User Repack Headroom. Headroom determining how
 * many bytes below the forced repack limit the user repack limit should
 * be placed. The original default is 0, which means the user and forced
 * repack limits are equal.
 */
#ifndef NVM3_DEFAULT_REPACK_HEADROOM
#define NVM3_DEFAULT_REPACK_HEADROOM  0
#endif

/**
 * NVM3 Default Instance Size. Size of the NVM3 storage region in flash.
 * This size should be aligned with the flash page size of the device
 * and must match the equivalent parameter value in the linker file
 * generation script. Original default value (5 * 8192).
 */
#ifndef NVM3_DEFAULT_NVM_SIZE
#define NVM3_DEFAULT_NVM_SIZE  (12 * 8192)
#endif

#endif // NVM3_DEFAULT_CONFIG_H
