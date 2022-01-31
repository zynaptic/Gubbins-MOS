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
 * This header implements the common routines for combining multiple
 * capacitive sensing channels into convenient user interface
 * components.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-driver-touch.h"

/*
 * Define coefficients for exponential decay smoothing filters. These
 * are specified as the number of bit shifts to be applied to the filter
 * accumulator, so the derived coefficient alpha is 2^-coeff_shift.
 * Time constants are shown assuming a 10 ms sampling interval (100 Hz).
 */
#define NOISE_FILTER_COEFF_SHIFT     3   // Alpha = 1/8, TC = 75 ms.
#define BASELINE_FILTER_COEFF_SHIFT 16   // Alpha = 1/2^16, TC = 11 min.

/*
 * Define options for rapid acquisition of baseline level at startup.
 * This uses a much smaller time constant than the tracking coefficient
 * over a fixed acquisition period.
 */
#define BASELINE_FILTER_ACQ_SAMPLES     750 // 7.5 seconds at 100 Hz.
#define BASELINE_FILTER_ACQ_COEFF_SHIFT   8 // Alpha = 1/128, TC = 2.5 s.

/*
 * Define the loop gain used for AGC updates. These typically occur
 * about every second and are derived from the relatively stable
 * baseline signals, so a fast time constant may be used.
 */
#define AGC_UPDATE_COEFF_SHIFT 14

/*
 * Initialises a capacitive touch sensing group for subsequent use.
 */
bool gmosDriverTouchGroupInit (gmosDriverTouchGroup_t* channelGroup,
    gmosPalTouchCallback_t palTouchCallback)
{
    if ((channelGroup == NULL) || (palTouchCallback == NULL)) {
        return false;
    }
    channelGroup->channelList = NULL;
    channelGroup->palTouchCallback = palTouchCallback;
    return true;
}

/*
 * Initialises a capacitive touch sensing channel for subsequent use.
 */
bool gmosDriverTouchChannelInit (gmosDriverTouchChannel_t* touchChannel,
    gmosDriverTouchGroup_t* channelGroup)
{
    gmosDriverTouchChannel_t** channelPtr;

    // Set the touch channel callback parameters.
    if ((touchChannel == NULL) || (channelGroup == NULL)) {
        return false;
    }
    touchChannel->channelGroup = channelGroup;
    touchChannel->nextChannel = NULL;
    touchChannel->filterState = 0;
    touchChannel->baselineState = 0;
    touchChannel->baselineAcqCount = BASELINE_FILTER_ACQ_SAMPLES;
    #if GMOS_DRIVER_TOUCH_CONFIG_AGC_ENABLE
        touchChannel->agcCoefficient = 0x4000;
    #endif

    // Append the channel to the end of the channel group list.
    channelPtr = &(channelGroup->channelList);
    while (*channelPtr != NULL) {
        channelPtr = &((*channelPtr)->nextChannel);
    }
    *channelPtr = touchChannel;

    // Perform the platform specific initialisation.
    return gmosDriverTouchChannelPalInit (touchChannel);
}

/*
 * Reads the current sample value associated with the specified touch
 * channel.
 */
int16_t gmosDriverTouchChannelRead (
   gmosDriverTouchChannel_t* touchChannel)
{
    uint32_t filterValue;
    uint32_t baselineValue;
    int32_t deltaValue;

    // Get the delta of the filtered sample value against the current
    // baseline level. Saturate to 16 bit unsigned range.
    filterValue = (uint32_t) touchChannel->filterState;
    baselineValue = touchChannel->baselineState >>
        (BASELINE_FILTER_COEFF_SHIFT - NOISE_FILTER_COEFF_SHIFT);
    if (GMOS_CONFIG_DRIVER_TOUCH_SAMPLE_INVERTED) {
        deltaValue = (int32_t) (baselineValue - filterValue);
    } else {
        deltaValue = (int32_t) (filterValue - baselineValue);
    }

    // Perform AGC correction if supported. Unit gain is defined as a
    // gain coefficient of 2^14.
    #if GMOS_CONFIG_DRIVER_TOUCH_AGC_ENABLE
        deltaValue *= touchChannel->agcCoefficient;
        deltaValue >>= 14;
    #endif

    // Saturate the result to 16-bit signed range.
    if (deltaValue > 0x7FFF) {
        deltaValue = 0x7FFF;
    } else if (deltaValue < -0x7FFF) {
        deltaValue = -0x7FFF;
    }
    return (int16_t) deltaValue;
}

/*
 * Performs IIR filtering on the touch channel samples.
 */
void gmosDriverTouchChannelFilter (
    gmosDriverTouchChannel_t* touchChannel, uint16_t channelSample)
{
    uint32_t filterValue;
    uint32_t baselineValue;

    // Initialise the filters on first call.
    if (touchChannel->filterState == 0) {
        touchChannel->filterState =
            channelSample << NOISE_FILTER_COEFF_SHIFT;
    }
    if (touchChannel->baselineState == 0) {
        touchChannel->baselineState =
            ((uint32_t) channelSample) << BASELINE_FILTER_COEFF_SHIFT;
    }

    // Implement the sample noise IIR filter.
    filterValue = (uint32_t) touchChannel->filterState;
    filterValue -= filterValue >> NOISE_FILTER_COEFF_SHIFT;
    filterValue += (uint32_t) channelSample;
    if (filterValue > 0xFFFF) {
        filterValue = 0xFFFF;
    }
    touchChannel->filterState = filterValue;

    // Update the baseline IIR filter (tracking mode).
    if (touchChannel->baselineAcqCount == 0) {
        baselineValue = touchChannel->baselineState;
        baselineValue -= baselineValue >> BASELINE_FILTER_COEFF_SHIFT;
        baselineValue += (uint32_t) channelSample;
        touchChannel->baselineState = baselineValue;
    }

    // Update the baseline IIR filter (acquisition mode).
    else {
        touchChannel->baselineAcqCount -= 1;
        baselineValue = touchChannel->baselineState;
        baselineValue -= baselineValue >> BASELINE_FILTER_ACQ_COEFF_SHIFT;
        baselineValue += ((uint32_t) channelSample) <<
            (BASELINE_FILTER_COEFF_SHIFT - BASELINE_FILTER_ACQ_COEFF_SHIFT);
        touchChannel->baselineState = baselineValue;
    }
}

/*
 * Performs an automatic gain control iteration on the channels in a
 * touch sensing group, which normalises the sensing levels of the
 * channels.
 */
#if GMOS_CONFIG_DRIVER_TOUCH_AGC_ENABLE
void gmosDriverTouchGroupRunAgc (gmosDriverTouchGroup_t* touchGroup)
{
    gmosDriverTouchChannel_t* currentChannel;
    int32_t referenceBaseline;
    int32_t currentBaseline;
    int32_t agcCorrection;

    // The first channel is used as the reference baseline. All other
    // channel AGC settings are expected to converge towards this.
    currentChannel = touchGroup->channelList;
    referenceBaseline =
        (int32_t) (currentChannel->baselineState >> 14) *
        (int32_t) currentChannel->agcCoefficient;

    // Iterate over the remaining channels in the touch sensing group,
    // performing AGC corrections with respect to the reference level.
    while (currentChannel->nextChannel != NULL) {
        currentChannel = currentChannel->nextChannel;
        currentBaseline =
            (int32_t) (currentChannel->baselineState >> 14) *
            (int32_t) currentChannel->agcCoefficient;
        agcCorrection = referenceBaseline - currentBaseline +
            (1 << (AGC_UPDATE_COEFF_SHIFT -1));
        currentChannel->agcCoefficient +=
            (int16_t) (agcCorrection >> (AGC_UPDATE_COEFF_SHIFT));
    }
}
#endif
