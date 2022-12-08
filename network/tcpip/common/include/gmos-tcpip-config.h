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
 * This header specifies the default configuration options for vendor
 * supplied and hardware accelerated TCP/IP stacks.
 */

#ifndef GMOS_TCPIP_CONFIG_H
#define GMOS_TCPIP_CONFIG_H

// Include the main configuration file which can override the default
// TCP/IP stack configuration options.
#include "gmos-config.h"

/**
 * Specify the default primary IPv4 DNS server address in network byte
 * order. This is the CloudFlare primary public DNS server, but other
 * public servers may be used instead.
 */
#ifndef GMOS_CONFIG_TCPIP_DNS_IPV4_PRIMARY
#define GMOS_CONFIG_TCPIP_DNS_IPV4_PRIMARY 0x01010101
#endif

/**
 * Specify the default secondary IPv4 DNS server address in network byte
 * order. This is the CloudFlare secondary public DNS server, but other
 * public servers may be used instead.
 */
#ifndef GMOS_CONFIG_TCPIP_DNS_IPV4_SECONDARY
#define GMOS_CONFIG_TCPIP_DNS_IPV4_SECONDARY 0x01000001
#endif

#endif // GMOS_TCPIP_CONFIG_H
