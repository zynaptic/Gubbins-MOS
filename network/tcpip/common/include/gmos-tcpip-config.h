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
 * Specifies the maximum number of DNS cache table entries.
 */
#ifndef GMOS_CONFIG_TCPIP_DNS_CACHE_SIZE
#define GMOS_CONFIG_TCPIP_DNS_CACHE_SIZE 2
#endif

/**
 * Specifies the DNS cache entry retention period as an integer number
 * of seconds. The DNS cache is only for local use, so the retention
 * period is kept short in order to release DNS cache resources in a
 * timely manner.
 */
#ifndef GMOS_CONFIG_TCPIP_DNS_RETENTION_TIME
#define GMOS_CONFIG_TCPIP_DNS_RETENTION_TIME 60
#endif

/**
 * Specifies the number of DNS retry attempts. The DNS client will issue
 * a first request to the primary DNS server followed by the specified
 * number of round robin retry attempts to each server in the DNS server
 * list.
 */
#ifndef GMOS_CONFIG_TCPIP_DNS_RETRY_COUNT
#define GMOS_CONFIG_TCPIP_DNS_RETRY_COUNT 3
#endif

/**
 * Specifies the timeout interval between DNS retry attempts as an
 * integer number of seconds.
 */
#ifndef GMOS_CONFIG_TCPIP_DNS_RETRY_INTERVAL
#define GMOS_CONFIG_TCPIP_DNS_RETRY_INTERVAL 4
#endif

/**
 * Specifies whether the DNS client supports IPv6 requests.
 */
#ifndef GMOS_CONFIG_TCPIP_DNS_SUPPORT_IPV6
#define GMOS_CONFIG_TCPIP_DNS_SUPPORT_IPV6 false
#endif

/**
 * Specify the default primary IPv4 DNS server address as a 32-bit
 * integer in network byte order. This is the CloudFlare primary public
 * DNS server, but other public servers may be used instead.
 */
#ifndef GMOS_CONFIG_TCPIP_DNS_IPV4_PRIMARY
#define GMOS_CONFIG_TCPIP_DNS_IPV4_PRIMARY 0x01010101
#endif

/**
 * Specify the default secondary IPv4 DNS server address as a 32-bit
 * integer in network byte order. This is the CloudFlare secondary
 * public DNS server, but other public servers may be used instead.
 */
#ifndef GMOS_CONFIG_TCPIP_DNS_IPV4_SECONDARY
#define GMOS_CONFIG_TCPIP_DNS_IPV4_SECONDARY 0x01000001
#endif

#endif // GMOS_TCPIP_CONFIG_H
