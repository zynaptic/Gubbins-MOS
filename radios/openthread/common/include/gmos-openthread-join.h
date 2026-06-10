/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2023-2026 Zynaptic Limited
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
 * This header defines the common API for managing the OpenThread stack
 * as a network joiner.
 */

#ifndef GMOS_OPENTHREAD_JOIN_H
#define GMOS_OPENTHREAD_JOIN_H

#include <stdbool.h>
#include "gmos-openthread.h"

/**
 * Initiates the OpenThread network joining process using the standard
 * commissioning tool authentication process. The supplied password is
 * used as the shared secret for the PAKE authentication process.
 * @param openThreadStack This is the OpenThread stack data structure
 *     that will be used for the network joining process.
 * @param password This is a null terminated 'C' string which contains
 *     the password to be used as the shared secret during
 *     authentication. It is copied internally by the OpenThread stack,
 *     so does not need to remain valid after this function returns.
 * @return Returns a status value which indicates that the network
 *     joining process was successfully initiated or the reason for
 *     failure.
 */
gmosOpenThreadStatus_t gmosOpenThreadJoinStartJoiner (
    gmosOpenThreadStack_t* openThreadStack, const char* password);

/**
 * Indicates whether an OpenThread network is already commissioned for
 * the joining device. If the network is not already commissioned it
 * will be necessary to start the joining process with a valid PAKE
 * shared secret.
 * @param openThreadStack This is the OpenThread stack data structure
 *     that will be used for the network joining process.
 * @return Returns a boolean value which will be set to 'true' if a
 *     network is already commissioned for the device and 'false'
 *     otherwise.
 */
bool gmosOpenThreadJoinIsCommissioned (
    gmosOpenThreadStack_t* openThreadStack);

#endif // GMOS_OPENTHREAD_JOIN_H
