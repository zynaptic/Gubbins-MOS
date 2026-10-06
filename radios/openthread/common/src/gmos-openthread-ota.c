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
 * This file implements the common functionality for managing OpenThread
 * over the air (OTA) upgrades using CoAP blockwise transfers from a given
 * CoAP resource URL. Individual blockwise transactions are used instead
 * of the OpenThread blockwise transfer API in order to control the rate
 * at which the data is transferred.
 */

#include <stdint.h>
#include <stdbool.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-bootloader.h"
#include "gmos-scheduler.h"
#include "gmos-buffers.h"
#include "gmos-openthread-config.h"
#include "gmos-openthread.h"
#include "gmos-openthread-ota.h"
#include "openthread/coap.h"

/*
 * Specify the polling interval for bootloader status.
 */
#define GMOS_OPENTHREAD_OTA_POLLING \
    (GMOS_TASK_RUN_LATER (GMOS_MS_TO_TICKS (100)))

/*
 * Specify the state space for the OpenThread OTA upgrade client state
 * machine.
 */
typedef enum {
    GMOS_OPENTHREAD_OTA_CLIENT_STATE_INIT,
    GMOS_OPENTHREAD_OTA_CLIENT_STATE_IDLE,
    GMOS_OPENTHREAD_OTA_CLIENT_STATE_BLOCK_REQ,
    GMOS_OPENTHREAD_OTA_CLIENT_STATE_BLOCK_CALLBACK,
    GMOS_OPENTHREAD_OTA_CLIENT_STATE_BLOCK_CHECK,
    GMOS_OPENTHREAD_OTA_CLIENT_STATE_IMAGE_OPEN,
    GMOS_OPENTHREAD_OTA_CLIENT_STATE_IMAGE_WRITE,
    GMOS_OPENTHREAD_OTA_CLIENT_STATE_IMAGE_LOOP,
    GMOS_OPENTHREAD_OTA_CLIENT_STATE_IMAGE_CLOSE,
    GMOS_OPENTHREAD_OTA_CLIENT_STATE_IMAGE_ABORT,
    GMOS_OPENTHREAD_OTA_CLIENT_STATE_IMAGE_VERIFY,
    GMOS_OPENTHREAD_OTA_CLIENT_STATE_IMAGE_READY,
    GMOS_OPENTHREAD_OTA_CLIENT_STATE_IMAGE_INVALID,
    GMOS_OPENTHREAD_OTA_CLIENT_STATE_IMAGE_DEPLOY,
    GMOS_OPENTHREAD_OTA_CLIENT_STATE_COMPLETE
} gmosOpenThreadOtaClientState_t;

/*
 * From the initialisation state, wait for the OpenThread network to
 * come online.
 */
static inline bool gmosOpenThreadOtaClientInitWait (
    gmosOpenThreadOtaClient_t* otaClient)
{
    gmosOpenThreadStatus_t netStatus =
        gmosOpenThreadNetStatus (otaClient->openThreadStack);
    return (netStatus == GMOS_OPENTHREAD_STATUS_SUCCESS) ? true : false;
}

/*
 * Callback handler for CoAP blockwise transfer requests.
 */
static void gmosOpenThreadOtaClientBlockReqCallback (void* callbackData,
    otMessage* coapMessage, const otMessageInfo* coapMessageInfo,
    otError otStatus)
{
    gmosOpenThreadOtaClient_t* otaClient =
        (gmosOpenThreadOtaClient_t*) callbackData;
    (void) coapMessageInfo;
    otCoapCode coapStatus;
    otCoapOptionIterator coapOptIter;
    const otCoapOption* coapOpt;
    uint64_t coapOptValue;
    uint_fast16_t msgOffset;
    uint_fast16_t msgLen;
    uint_fast16_t msgSize;
    uint8_t blockData [
        1 << (GMOS_CONFIG_OPENTHREAD_OTA_MAX_BLOCK_SIZE + 4)];

    // Drop responses received in an invalid state.
    if (otaClient->otaClientState !=
        GMOS_OPENTHREAD_OTA_CLIENT_STATE_BLOCK_CALLBACK) {
        return;
    }

    // Check the CoAP return code. This should be 2.05 (content).
    if (otStatus == OT_ERROR_NONE) {
        coapStatus = otCoapMessageGetCode (coapMessage);
        if (coapStatus != OT_COAP_CODE_CONTENT) {
            GMOS_LOG_FMT (LOG_DEBUG,
                "OpenThread : OTA block CoAP status %d.%02d.",
                (coapStatus >> 5) & 0x07, coapStatus & 0x1F);
            otStatus = OT_ERROR_REJECTED;
        }
    }

    // Attempt to read back the block 2 option and store it in the block
    // information field.
    if (otStatus == OT_ERROR_NONE) {
        otStatus = otCoapOptionIteratorInit (&coapOptIter, coapMessage);
    }
    if (otStatus == OT_ERROR_NONE) {
        coapOpt = otCoapOptionIteratorGetFirstOptionMatching (
            &coapOptIter, OT_COAP_OPTION_BLOCK2);
        if (coapOpt != NULL) {
            otStatus = otCoapOptionIteratorGetOptionUintValue (
                &coapOptIter, &coapOptValue);
            otaClient->blockInfo = (uint32_t) coapOptValue;
        } else {
            otStatus = OT_ERROR_PARSE;
        }
    }

    // Attempt to read back the size 2 option and store it in the size
    // information field. This is an optional value.
    if (otStatus == OT_ERROR_NONE) {
        coapOpt = otCoapOptionIteratorGetFirstOptionMatching (
            &coapOptIter, OT_COAP_OPTION_SIZE2);
        if (coapOpt != NULL) {
            otStatus = otCoapOptionIteratorGetOptionUintValue (
                &coapOptIter, &coapOptValue);
            otaClient->sizeInfo = (uint32_t) coapOptValue;
        }
    }

    // Only process successful responses that can fit in the buffer.
    if (otStatus == OT_ERROR_NONE) {
        msgOffset = otMessageGetOffset (coapMessage);
        msgLen = otMessageGetLength (coapMessage) - msgOffset;
        if (msgLen > sizeof (blockData)) {
            otStatus = OT_ERROR_NO_BUFS;
        }
    }

    // Read the data into the local buffer.
    if (otStatus == OT_ERROR_NONE) {
        msgSize = otMessageRead (coapMessage, msgOffset, blockData, msgLen);
        if (msgSize == msgLen) {
            if (!gmosBufferAppend (
                &(otaClient->blockData), blockData, msgSize)) {
                otStatus = OT_ERROR_NO_BUFS;
            }
        } else {
            otStatus = OT_ERROR_PARSE;
        }
    }

    // On successfully receiving a block, check for consistent transfer.
    if (otStatus == OT_ERROR_NONE) {
        otaClient->otaClientState =
            GMOS_OPENTHREAD_OTA_CLIENT_STATE_BLOCK_CHECK;
    } else {
        GMOS_LOG_FMT (LOG_DEBUG,
            "OpenThread : OTA blockwise callback failure status %d.",
            otStatus);
        otaClient->otaClientState =
            GMOS_OPENTHREAD_OTA_CLIENT_STATE_IMAGE_ABORT;
    }

    // Resume state machine task execution.
    gmosSchedulerTaskResume (&(otaClient->otaTask));
    return;
}

/*
 * Initiate CoAP request for blockwise firmware image transfer.
 */
static inline bool gmosOpenThreadOtaClientBlockReqSend (
    gmosOpenThreadOtaClient_t* otaClient)
{
    otInstance* otStack = otaClient->openThreadStack->otInstance;
    otMessage* coapMessage;
    otMessageInfo coapMessageInfo = { 0 };
    char* resourcePath;
    otError otStatus;
    uint_fast8_t i;

    // Allocate memory for the new CoAP message.
    coapMessage = otCoapNewMessage (otStack, NULL);
    if (coapMessage == NULL) {
        otStatus = OT_ERROR_NO_BUFS;
        goto fail;
    }

    // Strip the leading backslash from the resource path if required.
    resourcePath = otaClient->resourcePath;
    if (resourcePath [0] == '/') {
        resourcePath = &(resourcePath [1]);
    }

    // Fill in the common CoAP header for blockwise requests.
    otCoapMessageInit (coapMessage,
        OT_COAP_TYPE_CONFIRMABLE, OT_COAP_CODE_GET);
    otCoapMessageGenerateToken (coapMessage,
        OT_COAP_DEFAULT_TOKEN_LENGTH);
    otStatus = otCoapMessageAppendUriPathOptions (
        coapMessage, resourcePath);
    if (otStatus != OT_ERROR_NONE) {
        goto fail;
    }

    // Append the block 2 option.
    otStatus = otCoapMessageAppendBlock2Option (coapMessage,
        otaClient->blockCount, false, otaClient->blockSizeExp);
    if (otStatus != OT_ERROR_NONE) {
        goto fail;
    }

    // Set the CoAP message destination and send the request. All
    // additional options are left as zero to select the defaults.
    for (i = 0; i < 16; i++) {
        coapMessageInfo.mPeerAddr.mFields.m8 [i] =
            otaClient->remoteAddr [i];
    }
    coapMessageInfo.mPeerPort = otaClient->remotePort;
    otStatus = otCoapSendRequest (otStack, coapMessage,
        &coapMessageInfo, gmosOpenThreadOtaClientBlockReqCallback,
        otaClient);
    if (otStatus != OT_ERROR_NONE) {
        goto fail;
    }
    return true;

    // Release allocated memory on failure.
fail :
    GMOS_LOG_FMT (LOG_DEBUG,
        "OpenThread : OTA blockwise request failure status %d.",
        otStatus);
    if (coapMessage != NULL) {
        otMessageFree (coapMessage);
    }
    return false;
}

/*
 * Check that the new block parameters are consistent with expected
 * values.
 */
static inline bool gmosOpenThreadOtaClientBlockCheck (
    gmosOpenThreadOtaClient_t* otaClient)
{
    uint_fast8_t blockSizeExp;
    uint_fast16_t blockSize;
    uint_fast16_t dataSize;
    uint32_t blockCount;
    bool moreBlocks;
    bool checkOk = true;

    // TODO: Check overall size against available storage. Requires
    // bootloader module support.

    // Extract the fields from the block 2 response.
    blockSizeExp = otaClient->blockInfo & 0x07;
    blockCount = otaClient->blockInfo >> 4;
    moreBlocks = ((otaClient->blockInfo & 0x08) != 0) ? true : false;
    blockSize = (1 << (blockSizeExp + 4));

    // Check for matching block sequence numbers.
    if (blockCount != otaClient->blockCount) {
        checkOk = false;
        goto out;
    }

    // For the first block in the sequence, the server may reduce the
    // block transfer size.
    if (blockCount == 0) {
        if (blockSizeExp < otaClient->blockSizeExp) {
            otaClient->blockSizeExp = blockSizeExp;
        } else if (blockSizeExp > otaClient->blockSizeExp) {
            checkOk = false;
            goto out;
        }
    }

    // For subsequent blocks, the transfer size must be consistent.
    else {
        if (blockSizeExp != otaClient->blockSizeExp) {
            checkOk = false;
            goto out;
        }
    }

    // For normal blocks, the block size should equal the data size.
    dataSize = gmosBufferGetSize (&(otaClient->blockData));
    if (moreBlocks) {
        if (dataSize != blockSize) {
            checkOk = false;
            goto out;
        }
    }

    // For the final block, the data size should fit within the block
    // size and may be checked against the optional transfer size.
    else if (dataSize > blockSize) {
        checkOk = false;
        goto out;
    } else if ((otaClient->sizeInfo > 0) &&
        (otaClient->sizeInfo != (dataSize + blockCount * blockSize))) {
        checkOk = false;
        goto out;
    }
out :
    return checkOk;
}

/*
 * Implement the OTA upgrade client task.
 */
static inline gmosTaskStatus_t gmosOpenThreadOtaClientTaskFn (
    gmosOpenThreadOtaClient_t* otaClient)
{
    gmosBootloaderStatus_t bootStatus;
    gmosTaskStatus_t taskStatus = GMOS_TASK_RUN_IMMEDIATE;
    uint8_t nextState = otaClient->otaClientState;

    // Run the resource directory client state machine.
    switch (otaClient->otaClientState) {

        // Wait for network initialisation.
        case GMOS_OPENTHREAD_OTA_CLIENT_STATE_INIT :
            if (gmosOpenThreadOtaClientInitWait (otaClient)) {
                nextState = GMOS_OPENTHREAD_OTA_CLIENT_STATE_IDLE;
            } else {
                taskStatus = GMOS_OPENTHREAD_OTA_POLLING;
            }
            break;

        // From the idle state, wait for the download request.
        case GMOS_OPENTHREAD_OTA_CLIENT_STATE_IDLE :
            taskStatus = GMOS_TASK_SUSPEND;
            break;

        // Open the local bootloader image file for writing data.
        case GMOS_OPENTHREAD_OTA_CLIENT_STATE_IMAGE_OPEN :
            bootStatus = gmosBootloaderImageOpen ();
            if (bootStatus == GMOS_BOOTLOADER_STATUS_RETRY) {
                taskStatus = GMOS_OPENTHREAD_OTA_POLLING;
            } else if (bootStatus == GMOS_BOOTLOADER_STATUS_SUCCESS) {
                GMOS_LOG (LOG_DEBUG, "OpenThread : OTA firmware image opened.");
                nextState = GMOS_OPENTHREAD_OTA_CLIENT_STATE_BLOCK_REQ;
            } else {
                nextState = GMOS_OPENTHREAD_OTA_CLIENT_STATE_IMAGE_INVALID;
            }
            break;

        // Start the CoAP blockwise transfer.
        case GMOS_OPENTHREAD_OTA_CLIENT_STATE_BLOCK_REQ :
            if (gmosOpenThreadOtaClientBlockReqSend (otaClient)) {
                nextState = GMOS_OPENTHREAD_OTA_CLIENT_STATE_BLOCK_CALLBACK;
                taskStatus = GMOS_TASK_SUSPEND;
            } else {
                taskStatus = GMOS_OPENTHREAD_OTA_POLLING;
            }
            break;

        // Suspend processing while waiting for block transfer callback.
        case GMOS_OPENTHREAD_OTA_CLIENT_STATE_BLOCK_CALLBACK :
            taskStatus = GMOS_TASK_SUSPEND;
            break;

        // Check that the blockwise response is valid.
        case GMOS_OPENTHREAD_OTA_CLIENT_STATE_BLOCK_CHECK :
            if (gmosOpenThreadOtaClientBlockCheck (otaClient)) {
                nextState = GMOS_OPENTHREAD_OTA_CLIENT_STATE_IMAGE_WRITE;
            } else {
                nextState = GMOS_OPENTHREAD_OTA_CLIENT_STATE_IMAGE_ABORT;
            }
            break;

        // Write the block data to the firmware image.
        case GMOS_OPENTHREAD_OTA_CLIENT_STATE_IMAGE_WRITE :
            bootStatus = gmosBootloaderImageAppend (&(otaClient->blockData));
            if (bootStatus == GMOS_BOOTLOADER_STATUS_RETRY) {
                taskStatus = GMOS_OPENTHREAD_OTA_POLLING;
            } else if (bootStatus == GMOS_BOOTLOADER_STATUS_SUCCESS) {
                nextState = GMOS_OPENTHREAD_OTA_CLIENT_STATE_IMAGE_LOOP;
            } else {
                nextState = GMOS_OPENTHREAD_OTA_CLIENT_STATE_IMAGE_ABORT;
            }
            break;

        // Implement looping until end of firmware image, using the more
        // blocks flag from the block 2 option.
        case GMOS_OPENTHREAD_OTA_CLIENT_STATE_IMAGE_LOOP :
            if ((otaClient->blockInfo & 0x08) != 0) {
                otaClient->blockCount += 1;
                nextState = GMOS_OPENTHREAD_OTA_CLIENT_STATE_BLOCK_REQ;
            } else {
                nextState = GMOS_OPENTHREAD_OTA_CLIENT_STATE_IMAGE_CLOSE;
            }
            break;

        // Close the firmware image on completion.
        case GMOS_OPENTHREAD_OTA_CLIENT_STATE_IMAGE_CLOSE :
            bootStatus = gmosBootloaderImageClose ();
            if (bootStatus == GMOS_BOOTLOADER_STATUS_RETRY) {
                taskStatus = GMOS_OPENTHREAD_OTA_POLLING;
            } else if (bootStatus == GMOS_BOOTLOADER_STATUS_SUCCESS) {
                GMOS_LOG (LOG_DEBUG, "OpenThread : OTA firmware image downloaded.");
                nextState = GMOS_OPENTHREAD_OTA_CLIENT_STATE_IMAGE_VERIFY;
            } else {
                nextState = GMOS_OPENTHREAD_OTA_CLIENT_STATE_IMAGE_INVALID;
            }
            break;

        // Close the firmware image on aborting the transfer.
        case GMOS_OPENTHREAD_OTA_CLIENT_STATE_IMAGE_ABORT :
            bootStatus = gmosBootloaderImageClose ();
            if (bootStatus == GMOS_BOOTLOADER_STATUS_RETRY) {
                taskStatus = GMOS_OPENTHREAD_OTA_POLLING;
            } else {
                GMOS_LOG (LOG_DEBUG, "OpenThread : OTA firmware image download aborted.");
                nextState = GMOS_OPENTHREAD_OTA_CLIENT_STATE_IMAGE_INVALID;
            }
            break;

        // Verify the firmware image after writing.
        case GMOS_OPENTHREAD_OTA_CLIENT_STATE_IMAGE_VERIFY :
            bootStatus = gmosBootloaderImageVerify ();
            if (bootStatus == GMOS_BOOTLOADER_STATUS_RETRY) {
                taskStatus = GMOS_OPENTHREAD_OTA_POLLING;
            } else if (bootStatus == GMOS_BOOTLOADER_STATUS_SUCCESS) {
                GMOS_LOG (LOG_DEBUG, "OpenThread : OTA firmware image verified.");
                nextState = GMOS_OPENTHREAD_OTA_CLIENT_STATE_IMAGE_READY;
            } else {
                GMOS_LOG (LOG_DEBUG, "OpenThread : OTA firmware image not verified.");
                nextState = GMOS_OPENTHREAD_OTA_CLIENT_STATE_IMAGE_INVALID;
            }
            break;

        // Once the image has been downloaded and verified, wait for a
        // firmware deployment request.
        case GMOS_OPENTHREAD_OTA_CLIENT_STATE_IMAGE_READY :
            taskStatus = GMOS_TASK_SUSPEND;
            break;

        // Run firmware image deployment.
        case GMOS_OPENTHREAD_OTA_CLIENT_STATE_IMAGE_DEPLOY :
            bootStatus = gmosBootloaderImageDeploy ();
            if (bootStatus == GMOS_BOOTLOADER_STATUS_RETRY) {
                taskStatus = GMOS_OPENTHREAD_OTA_POLLING;
            } else if (bootStatus == GMOS_BOOTLOADER_STATUS_SUCCESS) {
                GMOS_LOG (LOG_DEBUG, "OpenThread : OTA firmware image deployed.");
                nextState = GMOS_OPENTHREAD_OTA_CLIENT_STATE_COMPLETE;
            } else {
                nextState = GMOS_OPENTHREAD_OTA_CLIENT_STATE_IMAGE_INVALID;
            }
            break;

        // Wait for restart in completed state.
        case GMOS_OPENTHREAD_OTA_CLIENT_STATE_COMPLETE :
            GMOS_LOG (LOG_DEBUG, "OpenThread : OTA firmware upgrade complete.");
            taskStatus = GMOS_TASK_SUSPEND;
            break;

        // Indicate unsuccessful downloads and wait for service reset.
        default :
            GMOS_LOG (LOG_DEBUG, "OpenThread : OTA firmware image not valid.");
            taskStatus = GMOS_TASK_SUSPEND;
            break;
    }
    otaClient->otaClientState = nextState;
    return taskStatus;
}

// Define the OTA upgrade client task.
GMOS_TASK_DEFINITION (gmosOpenThreadOtaClientTask,
    gmosOpenThreadOtaClientTaskFn, gmosOpenThreadOtaClient_t);

/*
 * Initialises the GubbinsMOS OpenThread OTA upgrade client on startup.
 */
bool gmosOpenThreadOtaClientInit (gmosOpenThreadOtaClient_t* otaClient,
    gmosOpenThreadStack_t* openThreadStack)
{
    uint_fast8_t i;

    // Initialise the platform bootloader support.
    if (!gmosBootloaderInit ()) {
        return false;
    }

    // Reset the OTA upgrade client state machine.
    gmosBufferInit (&(otaClient->blockData));
    otaClient->openThreadStack = openThreadStack;
    otaClient->blockCount = 0;
    otaClient->blockInfo = 0;
    otaClient->sizeInfo = 0;
    otaClient->blockSizeExp = GMOS_CONFIG_OPENTHREAD_OTA_MAX_BLOCK_SIZE;
    otaClient->otaClientState = GMOS_OPENTHREAD_OTA_CLIENT_STATE_INIT;

    // Clear the server configuration.
    for (i = 0; i < sizeof (otaClient->remoteAddr); i++) {
        otaClient->remoteAddr [i] = 0;
    }
    otaClient->remotePort = 0;
    otaClient->resourcePath [0] = '\0';

    // Run the OTA upgrade client task.
    gmosOpenThreadOtaClientTask_start (&(otaClient->otaTask),
        otaClient, "OpenThread OTA Client");
    return true;
}

/*
 * Resets the GubbinsMOS OpenThread OTA upgrade client after an
 * incomplete upgrade attempt.
 */
gmosOpenThreadStatus_t gmosOpenThreadOtaClientReset (
    gmosOpenThreadOtaClient_t* otaClient)
{
    uint_fast8_t otaState = otaClient->otaClientState;
    gmosOpenThreadStatus_t otaStatus;

    // The OTA upgrade client must be in a waiting state prior to reset.
    if ((otaState != GMOS_OPENTHREAD_OTA_CLIENT_STATE_IDLE) &&
        (otaState != GMOS_OPENTHREAD_OTA_CLIENT_STATE_IMAGE_READY) &&
        (otaState != GMOS_OPENTHREAD_OTA_CLIENT_STATE_IMAGE_INVALID) &&
        (otaState != GMOS_OPENTHREAD_OTA_CLIENT_STATE_COMPLETE)) {
        otaStatus = GMOS_OPENTHREAD_STATUS_RETRY;
        goto out;
    }

    // Reset the OTA upgrade client state machine.
    gmosBufferReset (&(otaClient->blockData), 0);
    otaClient->blockCount = 0;
    otaClient->blockInfo = 0;
    otaClient->sizeInfo = 0;
    otaClient->blockSizeExp = GMOS_CONFIG_OPENTHREAD_OTA_MAX_BLOCK_SIZE;
    otaClient->otaClientState = GMOS_OPENTHREAD_OTA_CLIENT_STATE_IDLE;
    otaStatus = GMOS_OPENTHREAD_STATUS_SUCCESS;

out :
    return otaStatus;
}

/*
 * Configure the firmware image download.
 */
gmosOpenThreadStatus_t gmosOpenThreadOtaClientConfigure (
    gmosOpenThreadOtaClient_t* otaClient, const uint8_t* remoteAddr,
    uint16_t remotePort, const char* remotePath, uint8_t remotePathSize)
{
    uint_fast8_t otaState = otaClient->otaClientState;
    gmosOpenThreadStatus_t otaStatus;
    uint_fast8_t i;

    // Wait for initialisation.
    if (otaState == GMOS_OPENTHREAD_OTA_CLIENT_STATE_INIT) {
        otaStatus = GMOS_OPENTHREAD_STATUS_RETRY;
        goto out;
    }

    // The OTA upgrade client must be in the idle state to update the
    // configuration.
    if (otaState != GMOS_OPENTHREAD_OTA_CLIENT_STATE_IDLE) {
        otaStatus = GMOS_OPENTHREAD_STATUS_INVALID_STATE;
        goto out;
    }

    // The specified resource path must fit in the local array.
    if (remotePathSize > GMOS_CONFIG_OPENTHREAD_OTA_MAX_PATH_SIZE) {
        otaStatus = GMOS_OPENTHREAD_STATUS_ILLEGAL_ARGUMENT;
        goto out;
    }

    // Copy over the CoAP remote firmware server parameters.
    for (i = 0; i < sizeof (otaClient->remoteAddr); i++) {
        otaClient->remoteAddr [i] = remoteAddr [i];
    }
    for (i = 0; i < remotePathSize; i++) {
        otaClient->resourcePath [i] = remotePath [i];
    }
    otaClient->resourcePath [remotePathSize] = '\0';
    otaClient->remotePort = remotePort;
    otaStatus = GMOS_OPENTHREAD_STATUS_SUCCESS;

out :
    return otaStatus;
}

/*
 * Initiate firmware image download.
 */
gmosOpenThreadStatus_t gmosOpenThreadOtaClientDownloadStart (
    gmosOpenThreadOtaClient_t* otaClient)
{
    uint_fast8_t otaState = otaClient->otaClientState;
    gmosOpenThreadStatus_t otaStatus;

    // Wait for initialisation.
    if (otaState == GMOS_OPENTHREAD_OTA_CLIENT_STATE_INIT) {
        otaStatus = GMOS_OPENTHREAD_STATUS_RETRY;
        goto out;
    }

    // The OTA upgrade client must be in the idle state to initiate the
    // firmware image download.
    if (otaState != GMOS_OPENTHREAD_OTA_CLIENT_STATE_IDLE) {
        otaStatus = GMOS_OPENTHREAD_STATUS_INVALID_STATE;
        goto out;
    }

    // Initiate the firmware image download.
    otaClient->otaClientState =
        GMOS_OPENTHREAD_OTA_CLIENT_STATE_IMAGE_OPEN;
    gmosSchedulerTaskResume (&(otaClient->otaTask));
    otaStatus = GMOS_OPENTHREAD_STATUS_SUCCESS;

out :
    return otaStatus;
}

/*
 * Monitor the status of an ongoing firmware image download.
 */
gmosOpenThreadStatus_t gmosOpenThreadOtaClientDownloadStatus (
    gmosOpenThreadOtaClient_t* otaClient, uint32_t* imageProgress,
    uint32_t* imageSize)
{
    gmosOpenThreadStatus_t otaStatus;
    uint32_t otaProgress;

    // Select status and progress counters.
    switch (otaClient->otaClientState) {
        case GMOS_OPENTHREAD_OTA_CLIENT_STATE_IMAGE_READY :
        case GMOS_OPENTHREAD_OTA_CLIENT_STATE_COMPLETE :
            otaStatus = GMOS_OPENTHREAD_STATUS_SUCCESS;
            otaProgress = otaClient->sizeInfo;
            break;
        case GMOS_OPENTHREAD_OTA_CLIENT_STATE_IMAGE_INVALID :
            otaStatus = GMOS_OPENTHREAD_STATUS_FAILED;
            otaProgress = 0;
            break;
        default :
            otaStatus = GMOS_OPENTHREAD_STATUS_RETRY;
            otaProgress = otaClient->blockCount <<
                (otaClient->blockSizeExp + 4);
            break;
    }

    // Fill in the optional progress indicators.
    if (imageProgress != NULL) {
        *imageProgress = otaProgress;
    }
    if (imageSize != NULL) {
        *imageSize = otaClient->sizeInfo;
    }
    return otaStatus;
}

/*
 * Initiate firmware image deployment after successful image download.
 */
gmosOpenThreadStatus_t gmosOpenThreadOtaClientDeploy (
    gmosOpenThreadOtaClient_t* otaClient)
{
    uint_fast8_t otaState = otaClient->otaClientState;
    gmosOpenThreadStatus_t otaStatus;

    // Indicate failure if the firmware image is not valid.
    if (otaState == GMOS_OPENTHREAD_OTA_CLIENT_STATE_IMAGE_INVALID) {
        otaStatus = GMOS_OPENTHREAD_STATUS_FAILED;
        goto out;
    }

    // Indicate retry required if the firmware image is not ready.
    if (otaState != GMOS_OPENTHREAD_OTA_CLIENT_STATE_IMAGE_READY) {
        otaStatus = GMOS_OPENTHREAD_STATUS_RETRY;
        goto out;
    }

    // Initiate the firmware image deployment.
    otaClient->otaClientState =
        GMOS_OPENTHREAD_OTA_CLIENT_STATE_IMAGE_DEPLOY;
    gmosSchedulerTaskResume (&(otaClient->otaTask));
    otaStatus = GMOS_OPENTHREAD_STATUS_SUCCESS;

out :
    return otaStatus;
}
