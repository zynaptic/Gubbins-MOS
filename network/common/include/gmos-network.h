/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2022-2025 Zynaptic Limited
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
 * This header provides the common data types and structures for use
 * with generic GubbinsMOS networking services.
 */

#ifndef GMOS_NETWORK_H
#define GMOS_NETWORK_H

/**
 * This enumeration specifies the set of general purpose network status
 * codes that may be returned by GubbinsMOS network services.
 */
typedef enum {

    // Indicates successful completion of a network operation.
    GMOS_NETWORK_STATUS_SUCCESS,

    // Indicates that a network connection is already established.
    GMOS_NETWORK_STATUS_CONNECTED,

    // Indicates that a network connection has not been established.
    GMOS_NETWORK_STATUS_NOT_CONNECTED,

    // Indicates that a request can not be completed because a network
    // connection was not open for the required protocol.
    GMOS_NETWORK_STATUS_NOT_OPEN,

    // Indicates that a request is not valid, usually due to invalid or
    // malformed parameters.
    GMOS_NETWORK_STATUS_NOT_VALID,

    // Indicate that an operation can not be completed at this time,
    // but may be retried later.
    GMOS_NETWORK_STATUS_RETRY,

    // Indicates that a data buffer is too large for transmission by
    // the network connection. This may be as a result of hardware
    // buffer size limitations or exceeding a protocol imposed limit.
    GMOS_NETWORK_STATUS_OVERSIZED,

    // Indicates that the network connection is down. This may be due to
    // a loss of local connectivity or lack of valid network settings.
    GMOS_NETWORK_STATUS_NETWORK_DOWN,

    // Indicates that a network transaction timed out.
    GMOS_NETWORK_STATUS_TIMEOUT,

    // Indicates that an unsupported protocol has been requested. For
    // example, attempting to open an IPv6 socket when using a TCP/IP
    // stack that only supports IPv4.
    GMOS_NETWORK_STATUS_UNSUPPORTED,

    // Indicates that a network data loss has occurred, usually as the
    // result of running out of local buffer memory.
    GMOS_NETWORK_STATUS_DATA_LOSS,

    // Indicates that a network protocol error has occurred. Usually
    // this will not be recoverable and indicates that a connection
    // needs to be restarted.
    GMOS_NETWORK_STATUS_PROTOCOL_ERROR,

    // Indicates that a network interface driver fault has occurred.
    // Usually this will not be recoverable and indicates that an
    // unexpected network hardware or driver failure has occurred.
    GMOS_NETWORK_STATUS_DRIVER_FAILURE

} gmosNetworkStatus_t;

/**
 * This enumeration specifies the set of general purpose network status
 * notification codes that may be forwarded by GubbinsMOS network
 * services via status notification callbacks.
 */
typedef enum {

    // Indicates that a network connection has been established.
    GMOS_NETWORK_NOTIFY_CONNECTED,

    // Indicates that a network connection has been terminated.
    GMOS_NETWORK_NOTIFY_DISCONNECTED

} gmosNetworkNotify_t;

/**
 * Specifies the function prototype to be used for general purpose
 * network status notification callbacks.
 * @param context This is an opaque pointer which references the
 *     implementation specific network context data that was specified
 *     when registering the callback handler.
 * @param notification This is the network status notification that is
 *     being sent via the callback.
 */
typedef void (*gmosNetworkNotifyHandler_t) (
    void* context, gmosNetworkNotify_t notification);

#endif // GMOS_NETWORK_H
