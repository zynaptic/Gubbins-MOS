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
 * This file contains the various configuration options for setting up a
 * WIZnet W5500 TCP/IP offload device.
 */

#ifndef WIZNET_DRIVER_CONFIG_H
#define WIZNET_DRIVER_CONFIG_H

#include "gmos-tcpip-config.h"

/**
 * Set the number of active sockets supported by the W5500 device. The
 * maximum supported number of sockets is 8, each of which will be
 * configured with 2K on-chip transmit and receive buffers. Reducing the
 * number of sockets will increase the amount of memory that can be
 * allocated per socket.
 */
#ifndef GMOS_CONFIG_TCPIP_MAX_SOCKETS
#define GMOS_CONFIG_TCPIP_MAX_SOCKETS 4
#elif (GMOS_CONFIG_TCPIP_MAX_SOCKETS > 8)
#error "The W5500 TCP/IP stack only supports up to 8 sockets"
#endif

/**
 * This configuration option specifies whether or not IPv6 support is
 * enabled for the TCP/IP stack.
 */
#ifndef GMOS_CONFIG_TCPIP_IPV6_ENABLE
#define GMOS_CONFIG_TCPIP_IPV6_ENABLE false
#elif GMOS_CONFIG_TCPIP_IPV6_ENABLE
#error "The W5500 TCP/IP stack does not support IPv6"
#endif

#endif // WIZNET_DRIVER_CONFIG_H
