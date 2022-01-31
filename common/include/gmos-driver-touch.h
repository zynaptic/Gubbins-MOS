/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2020-2022 Zynaptic Limited
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
 * This header defines the common API for accessing capacitive touch
 * sensor hardware on devices that support it, as well as common
 * routines for combining multiple capacitive sensing channels into
 * convenient user interface components.
 */

#ifndef GMOS_DRIVER_TOUCH_H
#define GMOS_DRIVER_TOUCH_H

#include <stdint.h>
#include <stdbool.h>

#include "gmos-config.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * Specify the capacitive touch sensor sampling interval, expressed as
 * an integer number of milliseconds.
 */
#ifndef GMOS_CONFIG_DRIVER_TOUCH_SAMPLE_INTERVAL
#define GMOS_CONFIG_DRIVER_TOUCH_SAMPLE_INTERVAL 10
#endif

/**
 * Specify the capacitive touch sensor sampling polarity. Inverted
 * polarity implies that the channel value reported by the underlying
 * hardware reduces when the sensing channel is activated.
 */
#ifndef GMOS_CONFIG_DRIVER_TOUCH_SAMPLE_INVERTED
#define GMOS_CONFIG_DRIVER_TOUCH_SAMPLE_INVERTED true
#endif

/**
 * Specify the capacitive touch sensor sampling activation threshold.
 * This needs to be set to an appropriate level after testing each
 * underlying hardware implementation.
 */
#ifndef GMOS_CONFIG_DRIVER_TOUCH_SAMPLE_THRESHOLD
#define GMOS_CONFIG_DRIVER_TOUCH_SAMPLE_THRESHOLD 100
#endif

/**
 * Specify that AGC correction support is to be used for channel groups.
 * This is only required for normalising channel levels within a channel
 * group, so may be disabled for applications that only use single
 * channel touch buttons.
 */
#ifndef GMOS_CONFIG_DRIVER_TOUCH_AGC_ENABLE
#define GMOS_CONFIG_DRIVER_TOUCH_AGC_ENABLE true
#endif

/**
 * Provide a forward reference to the touch channel data structure for
 * use in the callback function prototype.
 */
typedef struct gmosDriverTouchChannel_t gmosDriverTouchChannel_t;

/**
 * Defines the platform specific capacitive touch sensing channel state
 * data structure. The full type definition must be provided by the
 * associated platform abstraction layer.
 */
typedef struct gmosPalTouchState_t gmosPalTouchState_t;

/**
 * Defines the platform specific capacitive touch sensing channel
 * configuration options. The full type definition must be provided by
 * the associated platform abstraction layer.
 */
typedef struct gmosPalTouchConfig_t gmosPalTouchConfig_t;

/**
 * Defines the callback function prototype to be used by platform
 * specific touch channel implementations for notifying their associated
 * channel groups of a new sample.
 * @param channel This is a pointer to the channel data structure that
 *     is associated with the touch channel making the callback.
 * @param channelSample This is the latest capacitive touch channel
 *     sample that is being passed to the channel group. To avoid
 *     saturating the IIR channel filter this should not exceed a value
 *     of 8191.
 */
typedef void (*gmosPalTouchCallback_t) (
    gmosDriverTouchChannel_t* channel, uint16_t channelSample);

/**
 * Defines the GubbinsMOS capacitive touch sensing channel group data
 * structure that is used for managing groups of sensing channels that
 * make up various touch sensing user interface components.
 */
typedef struct gmosDriverTouchGroup_t {

    // This is the callback to be used for handling channel group sample
    // notifications.
    gmosPalTouchCallback_t palTouchCallback;

    // This is a pointer to the linked list of capacitive touch sensing
    // channels that make up the touch channel group.
    gmosDriverTouchChannel_t* channelList;

} gmosDriverTouchGroup_t;

/**
 * Defines the GubbinsMOS capacitive touch sensing channel state data
 * structure that is used for managing the low level hardware for a
 * single capacitive touch channel.
 */
typedef struct gmosDriverTouchChannel_t {

    // This is an opaque pointer to the platform abstraction layer data
    // structure that is used for accessing the touch channel hardware.
    // The data structure will be platform specific.
    gmosPalTouchState_t* palData;

    // This is an opaque pointer to the platform abstraction layer
    // configuration data structure that is used for setting up the
    // touch channel hardware. The data structure will be platform
    // specific.
    const gmosPalTouchConfig_t* palConfig;

    // This is a pointer to the channel group to which the sensing
    // channel belongs.
    gmosDriverTouchGroup_t* channelGroup;

    // This is a link to the next channel in the channel group to which
    // the sensing channel belongs.
    gmosDriverTouchChannel_t* nextChannel;

    // This is the state of the baseline IIR channel filter.
    uint32_t baselineState;

    // This is the current state of the IIR channel filter.
    uint16_t filterState;

    // This is the baseline acquisition sample counter.
    uint16_t baselineAcqCount;

    // This is the AGC coefficient to use for the channel.
    #if GMOS_CONFIG_DRIVER_TOUCH_AGC_ENABLE
    uint16_t agcCoefficient;
    #endif

} gmosDriverTouchChannel_t;

/**
 * Provides a platform configuration setup macro to be used when
 * allocating a capacitive touch sensing channel data structure.
 * Assigning this macro to a sensing channel data structure on
 * declaration will configure the sensing channel to use the platform
 * specific configuration.
 * @param _palData_ This is a pointer to the platform abstraction layer
 *     data structure that is to be used for accessing the platform
 *     specific hardware.
 * @param _palConfig_ This is a pointer to the platform specific touch
 *     sensing configuration data structure that defines a set of fixed
 *     configuration options to be used with the platform hardware.
 */
#define GMOS_DRIVER_TOUCH_CHANNEL_PAL_CONFIG(_palData_, _palConfig_) \
    { _palData_, _palConfig_ }

/**
 * Initialises a capacitive touch sensing group for subsequent use.
 * This should be called for each touch sensing group prior to accessing
 * it via any of the other API functions.
 * @param channelGroup This is the touch channel group which is to be
 *     initialised.
 * @param palTouchCallback This is the callback which will be used by
 *     the platform abstraction layer to process a new sample on the
 *     specified channel.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully setting up the capacitive touch sensing channel
 *     group and 'false' on failure.
 */
bool gmosDriverTouchGroupInit (gmosDriverTouchGroup_t* channelGroup,
    gmosPalTouchCallback_t palTouchCallback);

/**
 * Initialises a capacitive touch sensing channel for subsequent use.
 * This should be called for each touch sensing channel prior to
 * accessing it via any of the other API functions.
 * @param touchChannel This is the capacitive touch sensing channel that
 *     is to be initialised. It should previously have been configured
 *     using the 'GMOS_DRIVER_TOUCH_PAL_CONFIG' macro.
 * @param channelGroup This is an initialised channel group data
 *     structure to which the channel will be added.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully setting up the capacitive touch sensing channel and
 *     'false' on failure.
 */
bool gmosDriverTouchChannelInit (gmosDriverTouchChannel_t* touchChannel,
    gmosDriverTouchGroup_t* channelGroup);

/**
 * Reads the current filtered sample value associated with the specified
 * touch channel.
 * @param touchChannel This is the capacitive touch sensing channel for
 *     which the current sample value is being read.
 * @return Returns the latest result of filtering the touch channel
 *     samples. The filter has an implicit gain of 8, so this will use
 *     the full scale range of the 16-bit signed integer.
 */
int16_t gmosDriverTouchChannelRead (
   gmosDriverTouchChannel_t* touchChannel);

/**
 * Performs IIR filtering on the touch channel samples. This is called
 * from the platform abstraction layer in order to update the channel
 * filter state with a new sample value.
 * @param touchChannel This is the capacitive touch sensing channel for
 *     which the sampled data is being filtered.
 * @param channelSample This is the latest capacitive touch channel
 *     sample. To avoid saturating the IIR channel filter this should
 *     not exceed a value of 8191.
 */
void gmosDriverTouchChannelFilter (
    gmosDriverTouchChannel_t* touchChannel, uint16_t channelSample);

/**
 * Performs an automatic gain control iteration on the channels in a
 * touch sensing group, which normalises the sensing levels of the
 * channels. Normalisation is carried out using the current baseline
 * channel levels and will usually be carried out about once per second.
 * @param touchGroup This is the touch sensing group for which the
 *     automatic gain control iteration is to be executed.
 */
#if GMOS_CONFIG_DRIVER_TOUCH_AGC_ENABLE
void gmosDriverTouchGroupRunAgc (gmosDriverTouchGroup_t* touchGroup);
#else
#define gmosDriverTouchGroupRunAgc(_touchGroup_) do {} while (false)
#endif

/**
 * Initialises the platform specific hardware for a capacitive touch
 * sensing channel. This will be called by the common initialisation
 * function on startup.
 * @param touchChannel This is the capacitive touch sensing channel that
 *     is to be initialised. It should previously have been configured
 *     using the 'GMOS_DRIVER_TOUCH_PAL_CONFIG' macro.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully setting up the capacitive touch sensing channel and
 *     'false' on failure.
 */
bool gmosDriverTouchChannelPalInit (gmosDriverTouchChannel_t* touchChannel);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // GMOS_DRIVER_TOUCH_H
