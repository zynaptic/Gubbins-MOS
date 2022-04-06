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
 * Defines the vectored interrupt table for STM32L0XX family devices.
 */

.syntax unified
.cpu cortex-m0plus
.fpu softvfp
.thumb

.global gmosPalArmVectorTable
.global gmosPalIsrUnused

/*
 * Allocate the vectored interrupt table.
 */
.section .gmosPalArmVectors, "a", %progbits
.type gmosPalArmVectorTable, %object

gmosPalArmVectorTable:

    // ARM Cortex M0+ standard vectors.
    .word _estack
    .word gmosPalArmReset
    .word gmosPalArmNMI
    .word gmosPalArmFault
    .word 0
    .word 0
    .word 0
    .word 0
    .word 0
    .word 0
    .word 0
    .word gmosPalArmSVC
    .word 0
    .word 0
    .word gmosPalArmPendSVC
    .word gmosPalArmSysTick

    // STM32 peripheral interrupt vectors (block 1).
    .word gmosPalIsrWatchdog
    .word 0
    .word gmosPalIsrRTC
    .word gmosPalIsrFlash
    .word gmosPalIsrRCC
    .word gmosPalIsrEXTIA
    .word gmosPalIsrEXTIB
    .word gmosPalIsrEXTIC
    .word 0
    .word gmosPalIsrDMA1A
    .word gmosPalIsrDMA1B
    .word gmosPalIsrDMA1C
    .word gmosPalIsrADC
    .word gmosPalIsrLPTIM1
    .word 0
    .word gmosPalIsrTIM2

    // STM32 peripheral interrupt vectors (block 2).
    .word gmosPalIsrTIM3
    .word gmosPalIsrTIM6
    .word gmosPalIsrTIM7
    .word 0
    .word gmosPalIsrTIM21
    .word gmosPalIsrI2C3
    .word gmosPalIsrTIM22
    .word gmosPalIsrI2C1
    .word gmosPalIsrI2C2
    .word gmosPalIsrSPI1
    .word gmosPalIsrSPI2
    .word gmosPalIsrUSART1
    .word gmosPalIsrUSART2
    .word gmosPalIsrLPUART1
    .word 0
    .word 0

.size gmosPalArmVectorTable, .-gmosPalArmVectorTable

/*
 * Provide weak aliases for unused ISR hooks. These will be overridden
 * if any drivers implement an ISR of the same name.
 */

    .weak      gmosPalArmNMI
    .thumb_set gmosPalArmNMI, gmosPalIsrUnused

    .weak      gmosPalArmFault
    .thumb_set gmosPalArmFault, gmosPalIsrUnused

    .weak      gmosPalArmSVC
    .thumb_set gmosPalArmSVC, gmosPalIsrUnused

    .weak      gmosPalArmPendSVC
    .thumb_set gmosPalArmPendSVC, gmosPalIsrUnused

    .weak      gmosPalArmSysTick
    .thumb_set gmosPalArmSysTick, gmosPalIsrUnused

    .weak      gmosPalIsrWatchdog
    .thumb_set gmosPalIsrWatchdog, gmosPalIsrUnused

    .weak      gmosPalIsrRTC
    .thumb_set gmosPalIsrRTC, gmosPalIsrUnused

    .weak      gmosPalIsrFlash
    .thumb_set gmosPalIsrFlash, gmosPalIsrUnused

    .weak      gmosPalIsrRCC
    .thumb_set gmosPalIsrRCC, gmosPalIsrUnused

    .weak      gmosPalIsrEXTIA
    .thumb_set gmosPalIsrEXTIA, gmosPalIsrUnused

    .weak      gmosPalIsrEXTIB
    .thumb_set gmosPalIsrEXTIB, gmosPalIsrUnused

    .weak      gmosPalIsrEXTIC
    .thumb_set gmosPalIsrEXTIC, gmosPalIsrUnused

    .weak      gmosPalIsrDMA1A
    .thumb_set gmosPalIsrDMA1A, gmosPalIsrUnused

    .weak      gmosPalIsrDMA1B
    .thumb_set gmosPalIsrDMA1B, gmosPalIsrUnused

    .weak      gmosPalIsrDMA1C
    .thumb_set gmosPalIsrDMA1C, gmosPalIsrUnused

    .weak      gmosPalIsrADC
    .thumb_set gmosPalIsrADC, gmosPalIsrUnused

    .weak      gmosPalIsrLPTIM1
    .thumb_set gmosPalIsrLPTIM1, gmosPalIsrUnused

    .weak      gmosPalIsrTIM2
    .thumb_set gmosPalIsrTIM2, gmosPalIsrUnused

    .weak      gmosPalIsrTIM3
    .thumb_set gmosPalIsrTIM3, gmosPalIsrUnused

    .weak      gmosPalIsrTIM6
    .thumb_set gmosPalIsrTIM6, gmosPalIsrUnused

    .weak      gmosPalIsrTIM7
    .thumb_set gmosPalIsrTIM7, gmosPalIsrUnused

    .weak      gmosPalIsrTIM21
    .thumb_set gmosPalIsrTIM21, gmosPalIsrUnused

    .weak      gmosPalIsrTIM22
    .thumb_set gmosPalIsrTIM22, gmosPalIsrUnused

    .weak      gmosPalIsrI2C1
    .thumb_set gmosPalIsrI2C1, gmosPalIsrUnused

    .weak      gmosPalIsrI2C2
    .thumb_set gmosPalIsrI2C2, gmosPalIsrUnused

    .weak      gmosPalIsrI2C3
    .thumb_set gmosPalIsrI2C3, gmosPalIsrUnused

    .weak      gmosPalIsrSPI1
    .thumb_set gmosPalIsrSPI1, gmosPalIsrUnused

    .weak      gmosPalIsrSPI2
    .thumb_set gmosPalIsrSPI2, gmosPalIsrUnused

    .weak      gmosPalIsrUSART1
    .thumb_set gmosPalIsrUSART1, gmosPalIsrUnused

    .weak      gmosPalIsrUSART2
    .thumb_set gmosPalIsrUSART2, gmosPalIsrUnused

    .weak      gmosPalIsrLPUART1
    .thumb_set gmosPalIsrLPUART1, gmosPalIsrUnused

/*
 * Provide the unused interrupt handler. It implements an infinite loop
 * which can be used to catch errors in debugging, and which will cause
 * a watchdog reboot in production.
 */
.section .text.gmosPalIsrUnused, "ax"
.type gmosPalIsrUnused, %function

gmosPalIsrUnused:
gmosPalIsrUnusedLoop:
    B gmosPalIsrUnusedLoop

.size gmosPalIsrUnused, .-gmosPalIsrUnused
