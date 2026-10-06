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
 * This header defines the common API for managing OpenThread over the
 * air (OTA) upgrades using CoAP blockwise transfers from a given CoAP
 * resource URL.
 */

#ifndef GMOS_OPENTHREAD_OTA_H
#define GMOS_OPENTHREAD_OTA_H

#include <stdint.h>
#include <stdbool.h>
#include "gmos-scheduler.h"
#include "gmos-buffers.h"
#include "gmos-openthread-config.h"
#include "gmos-openthread.h"

/**
 * Defines the GubbinsMOS OpenThread over the air upgrade (OTA) client
 * structure that is used for encapsulating all the client data.
 */
typedef struct gmosOpenThreadOtaClient_t {

    // This is a pointer to the GubbinsMOS OpenThread stack instance
    // that is to be used for communication with the upgrade image server.
    gmosOpenThreadStack_t* openThreadStack;

    // This is the GubbinsMOS scheduler task state that is used to
    // run the OTA upgrade task.
    gmosTaskState_t otaTask;

    // This is a GubbinsMOS buffer that is used to store received block
    // data prior to writing it to the bootloader upgrade image area.
    gmosBuffer_t blockData;

    // This is the CoAP transfer block counter.
    uint32_t blockCount;

    // This is the CoAP transfer block received information.
    uint32_t blockInfo;

    // This is the CoAP transfer total size information.
    uint32_t sizeInfo;

    // This is the remote CoAP UDP port number to be used for accessing
    // the firmware server.
    uint16_t remotePort;

    // This is the CoAP transfer block size, using the exponent format.
    uint8_t blockSizeExp;

    // This is the current state of the OpenThread OTA upgrade client
    // state machine.
    uint8_t otaClientState;

    // This is the IPv6 address to be used for accessing the firmware
    // server.
    uint8_t remoteAddr [16];

    // This is the CoAP resource path string used for firmware requests.
    char resourcePath [GMOS_CONFIG_OPENTHREAD_OTA_MAX_PATH_SIZE + 1];

} gmosOpenThreadOtaClient_t;

/**
 * Initialises the GubbinsMOS OpenThread OTA upgrade client on startup.
 * @param otaClient This is the OpenThread OTA upgrade client instance
 *     that is to be initialised on startup.
 * @param openThreadStack This is the OpenThread stack instance that is
 *     to be used for accessing the OTA upgrade image.
 * @return Returns a boolean value which will be set to true on
 *     successfully initialising the OTA upgrade client and false
 *     otherwise.
 */
bool gmosOpenThreadOtaClientInit (
    gmosOpenThreadOtaClient_t* otaClient,
    gmosOpenThreadStack_t* openThreadStack);

/**
 * Resets the GubbinsMOS OpenThread OTA upgrade client after an
 * incomplete upgrade attempt.
 * @param otaClient This is the OpenThread OTA upgrade client instance
 *     that is to be reset.
 * @return Returns a status value which indicates that the OTA upgrade
 *     client was successfully reset or the reason for failure.
 */
gmosOpenThreadStatus_t gmosOpenThreadOtaClientReset (
    gmosOpenThreadOtaClient_t* otaClient);

/**
 * Configure the firmware image download.
 * @param otaClient This is the OpenThread OTA upgrade client instance
 *     that is to be used for the firmware image download.
 * @param remoteAddr This is a pointer to a byte array that contains the
 *     IPv6 address of the remote firmware server.
 * @param remotePort This is the IP port on the remote server which is
 *     to be used for downloading the firmware image.
 * @param remotePath This is a pointer to a character string which
 *     specifies the resource path on the remote server, excluding the
 *     leading backslash character.
 * @param remotePathSize This is the length of the associated remote
 *     path string.
 * @return Returns a status value which indicates that the firmware
 *     image download configuration was successfully processed or the
 *     reason for failure.
 */
gmosOpenThreadStatus_t gmosOpenThreadOtaClientConfigure (
    gmosOpenThreadOtaClient_t* otaClient, const uint8_t* remoteAddr,
    uint16_t remotePort, const char* remotePath, uint8_t remotePathSize);

/**
 * Initiate firmware image download using preconfigured options.
 * @param otaClient This is the OpenThread OTA upgrade client instance
 *     that is to be used for the firmware image download.
 * @return Returns a status value which indicates that the firmware
 *     image download was successfully started or the reason for
 *     failure.
 */
gmosOpenThreadStatus_t gmosOpenThreadOtaClientDownloadStart (
    gmosOpenThreadOtaClient_t* otaClient);

/**
 * Monitor the status of an ongoing firmware image download.
 * @param otaClient This is the OpenThread OTA upgrade client instance
 *     that is being used for the firmware image download.
 * @param imageProgress This is a pointer to a 32-bit integer which will
 *     be updated to reflect the number of bytes that have been
 *     downloaded. A null reference may be specified if this information
 *     is not required.
 * @param imageSize This is a pointer to a 32-bit integer which will be
 *     updated to reflect the overall size of the firmware image being
 *     downloaded. A null reference may be specified if this information
 *     is not required.
 * @return Returns a status value which indicates that the firmware
 *     image download was successfully completed or the reason for
 *     failure.
 */
gmosOpenThreadStatus_t gmosOpenThreadOtaClientDownloadStatus (
    gmosOpenThreadOtaClient_t* otaClient, uint32_t* imageProgress,
    uint32_t* imageSize);

/**
 * Initiate firmware image deployment after successful image download.
 * @param otaClient This is the OpenThread OTA upgrade client instance
 *     that was used for the firmware image download.
 * @return Returns a status value which indicates that the firmware
 *     image deployment was successfully initiated or the reason for
 *     failure.
 */
gmosOpenThreadStatus_t gmosOpenThreadOtaClientDeploy (
    gmosOpenThreadOtaClient_t* otaClient);

#endif // GMOS_OPENTHREAD_OTA_H
