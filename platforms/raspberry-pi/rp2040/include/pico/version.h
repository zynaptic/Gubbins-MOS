/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2022 Zynaptic Limited
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
 * This header is required by the Raspberry Pi Pico SDK. When using the
 * default SDK build process it is normally generated by CMAKE, but a
 * hardcoded version is used here which should mirror the SDK version
 * being used.
 */

#ifndef PICO_VERSION_H
#define PICO_VERSION_H

#define PICO_SDK_VERSION_MAJOR    1
#define PICO_SDK_VERSION_MINOR    4
#define PICO_SDK_VERSION_REVISION 0
#define PICO_SDK_VERSION_STRING   "1.4.0"

#endif // PICO_VERSION_H
