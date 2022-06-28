/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2020-2022 Zynaptic Limited
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
 * Provides the Microchip Harmony-V2 application module header for the
 * GubbinsMOS scheduler. This requires the Harmony code generator to
 * be configured in such a way that it creates an application module
 * named 'gmos_scheduler_app'.
 */

#ifndef GMOS_SCHEDULER_APP_H
#define GMOS_SCHEDULER_APP_H
#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialises the GubbinsMOS scheduler as a Harmony framework task.
 */
void GMOS_SCHEDULER_APP_Initialize (void);

/**
 * Implements a single cycle of the GubbinsMOS scheduler as a periodic
 * Harmony framework task.
 */
void GMOS_SCHEDULER_APP_Tasks (void);

#ifdef __cplusplus
}
#endif
#endif // GMOS_SCHEDULER_APP_H
