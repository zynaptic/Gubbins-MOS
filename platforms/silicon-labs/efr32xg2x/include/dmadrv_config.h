/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2023 Zynaptic Limited
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
 * This header defines the compile time configuration options used by
 * the Silicon Labs Gecko SDK DMA driver implementation.
 */

#ifndef DMADRV_CONFIG_H
#define DMADRV_CONFIG_H

#include "gmos-config.h"

// This sets the DMA interrupt priority.
#define EMDRV_DMADRV_DMA_IRQ_PRIORITY \
        GMOS_CONFIG_EFR32_DMA_INTERRUPT_PRIORITY

// This sets the number of DMA channels to support.
#define EMDRV_DMADRV_DMA_CH_COUNT \
        GMOS_CONFIG_EFR32_DMA_CHANNEL_COUNT

// Set all channels to use round robin scheduling.
#define EMDRV_DMADRV_DMA_CH_PRIORITY 0

#endif // DMADRV_CONFIG_H
