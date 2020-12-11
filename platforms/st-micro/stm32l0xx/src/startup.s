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
 * Implements initial device configuration and C runtime setup for
 * STM32L0XX family devices.
 */

.syntax unified
.cpu cortex-m0plus
.fpu softvfp
.thumb

.global gmosPalArmReset

/*
 * Implement the device reset handler which is called via the Cortex-M0
 * reset vector.
 */
.section .text.gmosPalArmReset, "ax"
.type gmosPalArmReset, %function

gmosPalArmReset:

    // Set the stack pointer to the top of RAM, as defined by '_estack'
    // in the linker script.
    LDR   r0, =_estack
    MOV   sp, r0

    // Prepare to copy the initialisation data area to RAM. This uses
    // four register LDMIA and STMIA transfers for speed, which means
    // that the data area must have 16 byte alignment.
    LDR   r1, =_edata
    LDR   r2, =_sdata      // R2 is used as the write index.
    LDR   r3, =16          // The counter decrement value.
    SUBS  r0, r1, r2
    ADDS  r0, r3           // R0 is the size of the data area plus 16.
    LDR   r1, =_sidata     // R1 is used as the read index.
    B     gmosPalArmInitCopyDataLoop

    // Use multiple register loads and stores into R4, R5, R6 and R7.
gmosPalArmInitCopyData:
    LDMIA r1!, {r4, r5, r6, r7}
    STMIA r2!, {r4, r5, r6, r7}

    // Use a decrement and test operation on R0 to check for loop
    // termination.
gmosPalArmInitCopyDataLoop:
    SUBS  r0, r3
    BNE   gmosPalArmInitCopyData

    // Prepare to zero out the BSS region. This uses four register
    // STMIA transfers for speed, which means that the BSS area must
    // have 16 byte alignmnent.
    LDR   r1, =_ebss
    LDR   r2, =_sbss       // R2 is used as the write index.
    SUBS  r0, r1, r2
    ADDS  r0, r3           // R0 is the size of the BSS area plus 16.
    LDR   r4, =0
    LDR   r5, =0
    LDR   r6, =0
    LDR   r7, =0
    B     gmosPalArmInitZeroDataLoop

    // Use multiple register stores from R4, R5, R6 and R7.
gmosPalArmInitZeroData:
    STMIA r2!, {r4, r5, r6, r7}

    // Use a decrement and test operation on R0 to check for loop
    // termination.
gmosPalArmInitZeroDataLoop:
    SUBS  r0, r3
    BNE   gmosPalArmInitZeroData

    // Initialise the microcontroller.
gmosPalArmInitSystemSetup:
    BL    gmosPalSystemSetup

    // Initialise the common platform components.
    BL    gmosMempoolInit

    // Initialise the platform abstraction layer.
    BL    gmosPalInit

    // Initialise the application code.
gmosPalArmInitStartApplication:
    BL    gmosAppInit

    // Enter the scheduler loop. This should never return.
    BL    gmosSchedulerStart

.size gmosPalArmReset, .-gmosPalArmReset
