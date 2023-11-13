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
 * This is the OpenThread configuration header file which is used to set
 * the required OpenThread build options for a given application.
 */

#ifndef OPENTHREAD_CORE_USER_CONFIG_H
#define OPENTHREAD_CORE_USER_CONFIG_H

// The package name is normally set by CMake in the standard OpenThread
// build process. Configure it here instead.
#define PACKAGE_NAME "GMOS-OPENTHREAD"

// The package version is normally set by CMake in the standard
// OpenThread build process. Configure it to a fixed version number here
// instead.
// TODO: This should be derived from the repository version.
#define PACKAGE_VERSION "0.0.1"

// Always build as a full thread device.
// TODO: Make this a configurable option.
#define OPENTHREAD_FTD 1

// Set OpenThread logging to use the GubbinsMOS log target.
#define OPENTHREAD_CONFIG_LOG_OUTPUT OPENTHREAD_CONFIG_LOG_OUTPUT_APP
#define OPENTHREAD_CONFIG_LOG_LEVEL OT_LOG_LEVEL_NOTE

// Set assertion handling to use the GubbinsMOS assertion target.
#define OPENTHREAD_CONFIG_ASSERT_ENABLE 1
#define OPENTHREAD_CONFIG_PLATFORM_ASSERT_MANAGEMENT 1

// Set persistent storage to use the Silicon Labs NVM3 non volatile
// memory library.
#define SL_CATALOG_NVM3_PRESENT 1

// Specify 2.4 GHz operation only.
#define RADIO_CONFIG_2P4GHZ_OQPSK_SUPPORT 1

// Support ping operation from the console.
#define OPENTHREAD_CONFIG_PING_SENDER_ENABLE 1

// Don't use the OpenThread protocol heap for mbedTLS allocations.
#define OPENTHREAD_CONFIG_ENABLE_BUILTIN_MBEDTLS_MANAGEMENT 0

// Use the standard library heap for OpenThread allocations.
#define OPENTHREAD_CONFIG_HEAP_EXTERNAL_ENABLE 1

// Support network time synchronisation.
#define OPENTHREAD_CONFIG_SNTP_CLIENT_ENABLE 1

// CoAP API support is required.
#define OPENTHREAD_CONFIG_COAP_API_ENABLE 1
#define OPENTHREAD_CONFIG_COAP_OBSERVE_API_ENABLE 1

// Enable device joining support using pre-shared key. The commissioner
// role is not currently supported.
#define OPENTHREAD_CONFIG_JOINER_ENABLE 1

// Use SLAAC to support routing outside the mesh.
#define OPENTHREAD_CONFIG_IP6_SLAAC_ENABLE 1

// Support DNS based service discovery.
#define OPENTHREAD_CONFIG_DNS_CLIENT_ENABLE 1
#define OPENTHREAD_CONFIG_DNS_CLIENT_SERVICE_DISCOVERY_ENABLE 1

// Disable TCP support since this still appears to be in development.
#define OPENTHREAD_CONFIG_TCP_ENABLE 0

// The EFR32 standard configuration settings override any in the
// standard OpenThread configuration header.
#include "openthread-core-efr32-config.h"

#endif // OPENTHREAD_CORE_USER_CONFIG_H
