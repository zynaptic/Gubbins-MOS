/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2025 Zynaptic Limited
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
 * Specify the default DC/DC converter settings for use on system
 * startup. The default values are suitable for use on all current
 * development targets.
 */

#ifndef SL_DEVICE_INIT_DCDC_CONFIG_H
#define SL_DEVICE_INIT_DCDC_CONFIG_H

/**
 * Enable DC/DC buck converter by default.
 */
#ifndef SL_DEVICE_INIT_DCDC_ENABLE
#define SL_DEVICE_INIT_DCDC_ENABLE 1
#endif

/**
 * Disable DC/DC converter bypass by default.
 */
#ifndef SL_DEVICE_INIT_DCDC_BYPASS
#define SL_DEVICE_INIT_DCDC_BYPASS 0
#endif

/**
 * Do not override default DC/DC peak current settings.
 */
#ifndef SL_DEVICE_INIT_DCDC_PFMX_IPKVAL_OVERRIDE
#define SL_DEVICE_INIT_DCDC_PFMX_IPKVAL_OVERRIDE 0
#endif

/**
 * Select default peak current limit setting.
 */
#ifndef SL_DEVICE_INIT_DCDC_PFMX_IPKVAL
#define SL_DEVICE_INIT_DCDC_PFMX_IPKVAL DCDC_PFMXCTRL_IPKVAL_DEFAULT
#endif

#endif // SL_DEVICE_INIT_DCDC_CONFIG_H
