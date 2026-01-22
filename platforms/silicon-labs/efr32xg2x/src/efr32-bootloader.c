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
 * Implements bootloader functionality for the Silicon Labs EFR32xG2x
 * series of microcontrollers by wrapping the standard Simplicity SDK
 * bootloader support.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-config.h"
#include "gmos-platform.h"
#include "gmos-bootloader.h"
#include "btl_interface.h"
#include "btl_interface_storage.h"
#include "application_properties.h"

/*
 * This enumeration specifies the various operating states for the
 * bootloader platform abstraction layer.
 */
typedef enum {
    GMOS_BOOTLOADER_PAL_STATE_IDLE,
    GMOS_BOOTLOADER_PAL_STATE_FAILED,
    GMOS_BOOTLOADER_PAL_STATE_ERASING,
    GMOS_BOOTLOADER_PAL_STATE_WRITING,
    GMOS_BOOTLOADER_PAL_STATE_VERIFYING,
    GMOS_BOOTLOADER_PAL_STATE_VERIFIED
} gmosBootloaderPalState_t;

/*
 * Define the application properties data structure which is used by the
 * standard SDK bootloader to obtain information about the application
 * firmware image.
 */
const ApplicationProperties_t sl_app_properties = {

    // Magic value indicating that this contains application properties.
    .magic = APPLICATION_PROPERTIES_MAGIC,

    // Version number of this data structure format.
    .structVersion = APPLICATION_PROPERTIES_VERSION,

    // Type of signature this application is signed with. This will be
    // updated to the correct signature type by the Simplicity Commander
    // tool when signing the firmware image.
    .signatureType = APPLICATION_SIGNATURE_NONE,

    // Location of the signature. This will be updated to the correct
    // signature location by the Simplicity Commander tool when signing
    // the firmware image.
    .signatureLocation = 0,

    // Information about the application.
    .app = {

        // Bitfield representing type of application.
        .type = APPLICATION_TYPE_MCU,

        // Version number for this application.
        .version = 0,

        // Capabilities of this application.
        .capabilities = 0,

        // Unique ID for the product this application is built for.
        .productId = {0}

    },

    // Pointer to information about the certificate.
    .cert = NULL,

    // Pointer to long token data section.
    .longTokenSectionAddress = NULL,

    // Parser decryption key.
    .decryptKey = {0}

};

// Allocate memory for bootloader state machine.
static uint8_t gmosBootloaderPalState;

// Allocate phase specific context data.
static union {
    uint32_t alignment;
    BootloaderEraseStatus_t erasing;
    uint8_t verifying [BOOTLOADER_STORAGE_VERIFICATION_CONTEXT_SIZE];
} context;

/*
 * Map bootloader error codes.
 */
static gmosBootloaderStatus_t gmosBootloaderPalMapErrorCodes (
   int32_t errorCode)
{
    gmosBootloaderStatus_t btlStatus;
    switch (errorCode) {
        case BOOTLOADER_ERROR_STORAGE_CONTINUE :
            btlStatus = GMOS_BOOTLOADER_STATUS_RETRY;
            break;
        case BOOTLOADER_ERROR_PARSE_CONTINUE :
            btlStatus = GMOS_BOOTLOADER_STATUS_RETRY;
            break;
        case BOOTLOADER_ERROR_PARSE_SUCCESS :
            btlStatus = GMOS_BOOTLOADER_STATUS_SUCCESS;
            break;
        case BOOTLOADER_ERROR_STORAGE_INVALID_ADDRESS :
            btlStatus = GMOS_BOOTLOADER_STATUS_OVERSIZED_IMAGE;
            break;
        default :
            btlStatus = GMOS_BOOTLOADER_STATUS_FATAL_ERROR;
            break;
    }
    return btlStatus;
}

/*
 * Map bootloader status values.
 */
static inline gmosBootloaderStatus_t gmosBootloaderPalMapStatusCodes (
   int32_t statusCode)
{
    if (statusCode == BOOTLOADER_OK) {
        return GMOS_BOOTLOADER_STATUS_SUCCESS;
    } else {
        return gmosBootloaderPalMapErrorCodes (statusCode);
    }
}

/*
 * Initialises the bootloader platform abstraction layer on startup.
 */
bool gmosBootloaderPalInit (void)
{
    int32_t statusCode;
    BootloaderInformation_t info;
    uint32_t versionMajor;
    uint32_t versionMinor;
    uint32_t upgradeLocation;
    uint32_t signatureLocation;
    bool initOk = false;

    // On successful initialisation report the bootloader information.
    statusCode = bootloader_init ();
    if (statusCode == BOOTLOADER_OK) {
        bootloader_getInfo (&info);
        versionMajor = (info.version & BOOTLOADER_VERSION_MAJOR_MASK) >>
            BOOTLOADER_VERSION_MAJOR_SHIFT;
        versionMinor = (info.version & BOOTLOADER_VERSION_MINOR_MASK) >>
            BOOTLOADER_VERSION_MINOR_SHIFT;
        GMOS_LOG_FMT (LOG_DEBUG,
            "Bootloader type %d, version %d.%d, capabilities 0x%08X.",
            info.type, versionMajor, versionMinor, info.capabilities);

        // Check for the upgrade image location in flash memory.
        if (bootloader_getUpgradeLocation (&upgradeLocation)) {
            signatureLocation = *((volatile uint32_t*)
                &sl_app_properties.signatureLocation);
            GMOS_LOG_FMT (LOG_VERBOSE,
                "Bootloader application location = 0x%08X.",
                upgradeLocation);
            GMOS_LOG_FMT (LOG_VERBOSE,
                "Bootloader signature location   = 0x%08X.",
                signatureLocation);
            initOk = true;
        }
    }

    // Report failure condition.
    if (initOk) {
        gmosBootloaderPalState = GMOS_BOOTLOADER_PAL_STATE_IDLE;
    } else {
        gmosBootloaderPalState = GMOS_BOOTLOADER_PAL_STATE_FAILED;
        GMOS_LOG_FMT (LOG_ERROR,
            "Bootloader initialisation failed with status 0x%04X.",
            statusCode);
    }
    return initOk;
}

/*
 * Polls the status of bootloader platform abstraction layer during
 * erase operations.
 */
static inline gmosBootloaderStatus_t gmosBootloaderPollErasing (void)
{
    int32_t statusCode;
    gmosBootloaderStatus_t btlStatus;

    // Erase the next chunk of bootloader storage.
    statusCode = bootloader_chunkedEraseStorageSlot (&context.erasing);
    btlStatus = gmosBootloaderPalMapStatusCodes (statusCode);

    // Proceed to writing state on erase completion.
    if (btlStatus == GMOS_BOOTLOADER_STATUS_SUCCESS) {
        gmosBootloaderPalState = GMOS_BOOTLOADER_PAL_STATE_WRITING;
    } else if (btlStatus != GMOS_BOOTLOADER_STATUS_RETRY) {
        gmosBootloaderPalState = GMOS_BOOTLOADER_PAL_STATE_FAILED;
    }
    return btlStatus;
}

/*
 * Polls the status of bootloader platform abstraction layer during
 * verification operations.
 */
static inline gmosBootloaderStatus_t gmosBootloaderPollVerifying (void)
{
    int32_t statusCode;
    gmosBootloaderStatus_t btlStatus;

    // Verify the next chunk of bootloader storage.
    statusCode = bootloader_continueVerifyImage (&context.verifying, NULL);
    btlStatus = gmosBootloaderPalMapStatusCodes (statusCode);

    // Proceed to verified state on verification completion.
    if (btlStatus != GMOS_BOOTLOADER_STATUS_RETRY) {
        gmosBootloaderPalState = GMOS_BOOTLOADER_PAL_STATE_VERIFIED;
    }
    return btlStatus;
}

/*
 * Polls the status of bootloader platform abstraction layer during long
 * running operations.
 */
gmosBootloaderStatus_t gmosBootloaderPalStatusPoll (void)
{
    gmosBootloaderStatus_t btlStatus;

    // Select appropriate polling action.
    switch (gmosBootloaderPalState) {
        case GMOS_BOOTLOADER_PAL_STATE_ERASING :
            btlStatus = gmosBootloaderPollErasing ();
            break;
        case GMOS_BOOTLOADER_PAL_STATE_WRITING :
            btlStatus = GMOS_BOOTLOADER_STATUS_SUCCESS;
            break;
        case GMOS_BOOTLOADER_PAL_STATE_VERIFYING :
            btlStatus = gmosBootloaderPollVerifying ();
            break;
        case GMOS_BOOTLOADER_PAL_STATE_FAILED :
            btlStatus = GMOS_BOOTLOADER_STATUS_FATAL_ERROR;
            break;
        default :
            btlStatus = GMOS_BOOTLOADER_STATUS_NOT_READY;
            break;
    }
    return btlStatus;
}

/*
 * Initiates an erase request for the contents of the stored bootloader
 * application upgrade image in the platform abstraction layer.
 */
gmosBootloaderStatus_t gmosBootloaderPalImageErase (void)
{
    int32_t statusCode;
    gmosBootloaderStatus_t btlStatus =
        GMOS_BOOTLOADER_STATUS_FATAL_ERROR;

    // Initialise the chunked erase operation. Erase requests can occur
    // at any phase of operation.
    if (gmosBootloaderPalState != GMOS_BOOTLOADER_PAL_STATE_FAILED) {
        statusCode = bootloader_initChunkedEraseStorageSlot (
            0, &context.erasing);
        btlStatus = gmosBootloaderPalMapStatusCodes (statusCode);
    }

    // Initiate erase processing on successful initialisation.
    if (btlStatus == GMOS_BOOTLOADER_STATUS_SUCCESS) {
        gmosBootloaderPalState = GMOS_BOOTLOADER_PAL_STATE_ERASING;
    }
    return btlStatus;
}

/*
 * Initiates a write request for the stored bootloader application
 * upgrade image in the platform abstraction layer.
 */
gmosBootloaderStatus_t gmosBootloaderPalImageWrite (
    uint32_t writeOffset, uint8_t* writeData, uint16_t writeSize)
{
    int32_t statusCode;
    gmosBootloaderStatus_t btlStatus =
        GMOS_BOOTLOADER_STATUS_FATAL_ERROR;

    // Perform bootloader write.
    if (gmosBootloaderPalState == GMOS_BOOTLOADER_PAL_STATE_WRITING) {
        statusCode = bootloader_writeStorage (
            0, writeOffset, writeData, writeSize);
        btlStatus = gmosBootloaderPalMapStatusCodes (statusCode);
    }

    // Stop processing writes after an error condition.
    if (btlStatus != GMOS_BOOTLOADER_STATUS_SUCCESS) {
        gmosBootloaderPalState = GMOS_BOOTLOADER_PAL_STATE_IDLE;
    }
    return btlStatus;
}

/*
 * Initiates a verification request for the stored bootloader
 * application upgrade image in the platform abstraction layer.
 */
gmosBootloaderStatus_t gmosBootloaderPalImageVerify (void)
{
    int32_t statusCode;
    gmosBootloaderStatus_t btlStatus =
        GMOS_BOOTLOADER_STATUS_FATAL_ERROR;

    // Initialise the chunked verify operation. Verify requests should
    // occur after writing is complete. Repeat verification is also
    // supported if required.
    if ((gmosBootloaderPalState == GMOS_BOOTLOADER_PAL_STATE_WRITING) ||
        (gmosBootloaderPalState == GMOS_BOOTLOADER_PAL_STATE_VERIFIED)) {
        statusCode = bootloader_initVerifyImage (
            0, &context.verifying, sizeof (context.verifying));
        btlStatus = gmosBootloaderPalMapStatusCodes (statusCode);

        // Initiate verification processing on successful initialisation.
        if (btlStatus == GMOS_BOOTLOADER_STATUS_SUCCESS) {
            gmosBootloaderPalState = GMOS_BOOTLOADER_PAL_STATE_VERIFYING;
        }
    }
    return btlStatus;
}

/*
 * Deploys the stored bootloader application upgrade image. This will
 * only proceed once a valid image has been loaded into local storage
 * and verified.
 */
gmosBootloaderStatus_t gmosBootloaderPalImageDeploy (void)
{
    gmosBootloaderStatus_t btlStatus =
        GMOS_BOOTLOADER_STATUS_FATAL_ERROR;

    // Initialise the firmware deployment operation. Deployment should
    // occur after the application upgrade image has been verified.
    if (gmosBootloaderPalState == GMOS_BOOTLOADER_PAL_STATE_VERIFIED) {
        bootloader_rebootAndInstall ();
        btlStatus = GMOS_BOOTLOADER_STATUS_SUCCESS;
    }
    return btlStatus;
}
