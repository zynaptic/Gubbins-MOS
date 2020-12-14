/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2020 Zynaptic Limited
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
 * Specifies the STM32L0XX demo application configuration options.
 */

#ifndef GMOS_APP_CONFIG_H
#define GMOS_APP_CONFIG_H

/*
 * Set the debug console log level to use for the demo application.
 */
#define GMOS_CONFIG_LOG_LEVEL LOG_DEBUG

/*
 * Specifies whether scheduler lifecycle messages should be sent to the
 * debug log.
 */
#define GMOS_DEMO_APP_LOG_LIFECYCLE_INFO true

/*
 * Specifies the interval between temperature sensor samples as an
 * integer number of seconds.
 */
#define GMOS_DEMO_APP_TEMP_SAMPLE_INTERVAL 15

#endif // GMOS_APP_CONFIG_H
