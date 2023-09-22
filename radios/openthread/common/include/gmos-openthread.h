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
 * This header defines the common API for integrating the OpenThread
 * stack into the GubbinsMOS runtime framework.
 */

#ifndef GMOS_OPENTHREAD_H
#define GMOS_OPENTHREAD_H

#include <stdbool.h>
#include <stddef.h>
#include "gmos-scheduler.h"

/**
 * Defines the OpenThread radio specific I/O state data structure. The
 * full type definition must be provided by the associated radio
 * abstraction layer.
 */
typedef struct gmosRalOpenThreadState_t gmosRalOpenThreadState_t;

/**
 * Defines the OpenThread radio specific I/O configuration options. The
 * full type definition must be provided by the associated radio
 * abstraction layer.
 */
typedef struct gmosRalOpenThreadConfig_t gmosRalOpenThreadConfig_t;

/**
 * Defines the GubbinsMOS OpenThread stack data structure that is used
 * for encapsulating all the OpenThread stack data.
 */
typedef struct gmosOpenThreadStack_t {

    // This is an opaque pointer to the OpenThread radio abstraction
    // layer data structure that is used for accessing the OpenThread
    // radio hardware. The data structure will be radio device specific.
    gmosRalOpenThreadState_t* ralData;

    // This is an opaque pointer to the OpenThread radio abstraction
    // layer configuration data structure that is used for setting up
    // the OpenThread radio hardware. The data structure will be radio
    // device specific.
    const gmosRalOpenThreadConfig_t* ralConfig;

    // This is an anonymous pointer to the singleton OpenThread stack
    // instance data structure.
    void* otInstance;

    // This is the GubbinsMOS scheduler task state that is used to
    // manage the OpenThread GubbinsMOS task.
    gmosTaskState_t openThreadTask;

} gmosOpenThreadStack_t;

/**
 * Provides a radio hardware configuration setup macro to be used when
 * allocating OpenThread stack data structures. Assigning this macro to
 * an OpenThread stack data structure on declaration will set the radio
 * specific configuration. Refer to the radio specific OpenThread
 * implementation for full details of the configuration options.
 * @param _ralData_ This is the radio abstraction layer state data
 *     structure that is to be used for accessing the radio specific
 *     hardware.
 * @param _ralConfig_ This is a radio hardware specific configuration
 *     data structure that defines a set of fixed configuration options
 *     to be used with the OpenThread radio.
 */
#define GMOS_OPENTHREAD_RAL_CONFIG(_ralData_, _ralConfig_)             \
    { _ralData_, _ralConfig_, NULL, {} }

/**
 * Initialises an OpenThread stack on startup.
 * @param openThreadStack This is the OpenThread stack data structure
 *     that will be used for managing GubbinsMOS access to the
 *     OpenThread stack.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully completing the initialisation process and 'false'
 *     otherwise.
 */
bool gmosOpenThreadInit (gmosOpenThreadStack_t* openThreadStack);

/**
 * Initialises the OpenThread CLI on startup. This may be used during
 * development for interactive control of the OpenThread stack and is
 * currently implemented as part of the radio abstraction layer.
 * @param openThreadStack This is the OpenThread stack data structure
 *     that is used for managing GubbinsMOS access to the OpenThread
 *     stack.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully completing the initialisation process and 'false'
 *     otherwise.
 */
bool gmosOpenThreadCliInit (gmosOpenThreadStack_t* openThreadStack);

/**
 * Initialises an OpenThread radio abstraction on startup. This will
 * be called by the OpenThread initialisation function in order to set
 * up the radio abstraction layer prior to any further processing. The
 * radio specific configuration options should already have been
 * populated using the 'GMOS_OPENTHREAD_RAL_CONFIG' macro.
 * @param openThreadStack This is the OpenThread stack data structure
 *     that will be used for managing GubbinsMOS access to the
 *     OpenThread stack.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully completing the initialisation process and 'false'
 *     otherwise.
 */
bool gmosOpenThreadRalInit (gmosOpenThreadStack_t* openThreadStack);

/**
 * Provides a processing tick function for the OpenThread radio
 * abstraction layer. This will be called repeatedly in the context of
 * the OpenThread task function in order to carry out low level radio
 * processing tasks.
 * @param openThreadStack This is the OpenThread stack data structure
 *     that will be used for managing GubbinsMOS access to the
 *     OpenThread stack.
 * @return Returns a GubbinsMOS task state which may be used to
 *     determine the scheduling of subsequent calls to this function.
 */
gmosTaskStatus_t gmosOpenThreadRalTick (
    gmosOpenThreadStack_t* openThreadStack);

#endif // GMOS_OPENTHREAD_H
