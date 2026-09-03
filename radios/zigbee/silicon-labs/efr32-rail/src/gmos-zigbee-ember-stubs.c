/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2025-2026 Zynaptic Limited
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
 * This file implements various stub callback handlers that are required
 * to support the EmberZNet Zigbee stack implementation.
 */

#include <stdint.h>
#include <stdbool.h>

#include "sl_zigbee_types.h"
#include "sl_rail_util_ieee802154_stack_event.h"
#include "stack-info.h"

/*
 * The EmberZNet specific ZDO endpoint handling is not required, but the
 * associated symbols still need to be defined for linking.
 */
uint8_t sl_zigbee_endpoint_count = 0;
sl_zigbee_endpoint_t sl_zigbee_endpoints [] = {};

/*
 * The GubbinsMOS implementation does not have an independent common
 * application task that requires waking up.
 */
void sl_zigbee_wakeup_common_task (void)
{
}

/*
 * The GubbinsMOS implementation does not have an independent ISR
 * handler task that requires waking up.
 */
void sli_zigbee_stack_rtos_stack_wakeup_isr_handler (void)
{
}

/*
 * The GubbinsMOS implementation always runs in either ISR or main
 * thread context.
 */
bool sli_zigbee_is_stack_task_or_isr_current_context (void)
{
  return true;
}

/*
 * The stack statistic counters support is part of the Silicon Labs
 * application framework, which is not used by the GubbinsMOS
 * implementation.
 */
void sli_zigbee_stack_populate_counters (
    sl_zigbee_counter_type_t type, sl_zigbee_counter_info_t info)
{
    (void) type;
    (void) info;
}

/*
 * The GubbinsMOS implementation does not currently support Zigbee R23
 * enhanced routing.
 */
bool zigbee_enhanced_routing_is_active (void)
{
    return false;
}

/*
 * IEEE 802.15.4 stack event notifications are not currently tracked.
 */
sl_rail_util_ieee802154_stack_status_t sl_rail_util_ieee802154_on_event (
  sl_rail_util_ieee802154_stack_event_t stack_event, uint32_t supplement)
{
    (void) stack_event;
    (void) supplement;
    return SL_RAIL_UTIL_IEEE802154_STACK_STATUS_SUCCESS;
}
