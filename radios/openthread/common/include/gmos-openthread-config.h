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
 * This header defines the common configuration options used for
 * integrating the OpenThread stack into the GubbinsMOS runtime
 * framework.
 */

#ifndef GMOS_OPENTHREAD_CONFIG_H
#define GMOS_OPENTHREAD_CONFIG_H

// The package name is normally set by CMake in the standard OpenThread
// build process. Configure it here instead.
#define PACKAGE_NAME "GMOS-OPENTHREAD"

// The package version is normally set by CMake in the standard
// OpenThread build process. Configure it to a fixed version number
// here instead.
// TODO: This should be automatically derived from the repository
// version by the Makefile.
#define PACKAGE_VERSION "0.0.1"

// Always build as a full thread device.
// TODO: Make this a configurable option.
#define OPENTHREAD_FTD 1

// Don't use the thread protocol heap for mbedTLS allocations, since
// this fails at compile time with a missing function prototype for
// mbedtls_platform_set_calloc_free.
#define OPENTHREAD_CONFIG_ENABLE_BUILTIN_MBEDTLS_MANAGEMENT 0

// Disable TCP support since this still appears to be in development.
#define OPENTHREAD_CONFIG_TCP_ENABLE 0

#endif // GMOS_OPENTHREAD_CONFIG_H
