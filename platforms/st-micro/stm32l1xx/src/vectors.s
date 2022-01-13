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
 * Defines the vectored interrupt table for STM32L1XX family devices.
 */

.syntax unified
.cpu cortex-m3
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

    // ARM Cortex M3 standard vectors.
    .word _estack
    .word gmosPalArmReset
    .word gmosPalArmNMI
    .word gmosPalArmFault
    .word gmosPalMemManage
    .word gmosPalBusFault
    .word gmosPalUsageFault
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
    .word gmosPalIsrPVD
    .word gmosPalIsrTamper
    .word gmosPalIsrRTCWK
    .word gmosPalIsrFlash
    .word gmosPalIsrRCC
    .word gmosPalIsrEXTIA
    .word gmosPalIsrEXTIB
    .word gmosPalIsrEXTIC
    .word gmosPalIsrEXTID
    .word gmosPalIsrEXTIE
    .word gmosPalIsrDMA1A
    .word gmosPalIsrDMA1B
    .word gmosPalIsrDMA1C
    .word gmosPalIsrDMA1D
    .word gmosPalIsrDMA1E

    // STM32 peripheral interrupt vectors (block 2).
    .word gmosPalIsrDMA1F
    .word gmosPalIsrDMA1G
    .word gmosPalIsrADC1
    .word gmosPalIsrUSBH
    .word gmosPalIsrUSBL
    .word gmosPalIsrDAC
    .word gmosPalIsrCOMPWK
    .word gmosPalIsrEXTIF
    .word gmosPalIsrLCD
    .word gmosPalIsrTIM9
    .word gmosPalIsrTIM10
    .word gmosPalIsrTIM11
    .word gmosPalIsrTIM2
    .word gmosPalIsrTIM3
    .word gmosPalIsrTIM4
    .word gmosPalIsrI2C1EV

    // STM32 peripheral interrupt vectors (block 3).
    .word gmosPalIsrI2C1ER
    .word gmosPalIsrI2C2EV
    .word gmosPalIsrI2C2ER
    .word gmosPalIsrSPI1
    .word gmosPalIsrSPI2
    .word gmosPalIsrUSART1
    .word gmosPalIsrUSART2
    .word gmosPalIsrUSART3
    .word gmosPalIsrEXTIG
    .word gmosPalIsrRTCAL
    .word gmosPalIsrUSBWK
    .word gmosPalIsrTIM6
    .word gmosPalIsrTIM7
    .word gmosPalIsrSDIO
    .word gmosPalIsrTIM5
    .word gmosPalIsrSPI3

    // STM32 peripheral interrupt vectors (block 4).
    .word gmosPalIsrUART4
    .word gmosPalIseUART5
    .word gmosPalIsrDMA2A
    .word gmosPalIsrDMA2B
    .word gmosPalIsrDMA2C
    .word gmosPalIsrDMA2D
    .word gmosPalIsrDMA2E
    .word gmosPalIsrAES
    .word gmosPalIsrCOMPAQ

.size gmosPalArmVectorTable, .-gmosPalArmVectorTable

/*
 * Provide weak aliases for unused ISR hooks. These will be overridden
 * if any drivers implement an ISR of the same name.
 */

    .weak      gmosPalArmNMI
    .thumb_set gmosPalArmNMI, gmosPalIsrUnused

    .weak      gmosPalArmFault
    .thumb_set gmosPalArmFault, gmosPalIsrUnused

    .weak      gmosPalMemManage
    .thumb_set gmosPalMemManage, gmosPalIsrUnused

    .weak      gmosPalBusFault
    .thumb_set gmosPalBusFault, gmosPalIsrUnused

    .weak      gmosPalUsageFault
    .thumb_set gmosPalUsageFault, gmosPalIsrUnused

    .weak      gmosPalArmSVC
    .thumb_set gmosPalArmSVC, gmosPalIsrUnused

    .weak      gmosPalArmPendSVC
    .thumb_set gmosPalArmPendSVC, gmosPalIsrUnused

    .weak      gmosPalArmSysTick
    .thumb_set gmosPalArmSysTick, gmosPalIsrUnused

    .weak      gmosPalIsrWatchdog
    .thumb_set gmosPalIsrWatchdog, gmosPalIsrUnused

    .weak      gmosPalIsrPVD
    .thumb_set gmosPalIsrPVD, gmosPalIsrUnused

    .weak      gmosPalIsrTamper
    .thumb_set gmosPalIsrTamper, gmosPalIsrUnused

    .weak      gmosPalIsrRTCWK
    .thumb_set gmosPalIsrRTCWK, gmosPalIsrUnused

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

    .weak      gmosPalIsrEXTID
    .thumb_set gmosPalIsrEXTID, gmosPalIsrUnused

    .weak      gmosPalIsrEXTIE
    .thumb_set gmosPalIsrEXTIE, gmosPalIsrUnused

    .weak      gmosPalIsrDMA1A
    .thumb_set gmosPalIsrDMA1A, gmosPalIsrUnused

    .weak      gmosPalIsrDMA1B
    .thumb_set gmosPalIsrDMA1B, gmosPalIsrUnused

    .weak      gmosPalIsrDMA1C
    .thumb_set gmosPalIsrDMA1C, gmosPalIsrUnused

    .weak      gmosPalIsrDMA1D
    .thumb_set gmosPalIsrDMA1D, gmosPalIsrUnused

    .weak      gmosPalIsrDMA1E
    .thumb_set gmosPalIsrDMA1E, gmosPalIsrUnused

    .weak      gmosPalIsrDMA1F
    .thumb_set gmosPalIsrDMA1F, gmosPalIsrUnused

    .weak      gmosPalIsrDMA1G
    .thumb_set gmosPalIsrDMA1G, gmosPalIsrUnused

    .weak      gmosPalIsrADC1
    .thumb_set gmosPalIsrADC1, gmosPalIsrUnused

    .weak      gmosPalIsrUSBH
    .thumb_set gmosPalIsrUSBH, gmosPalIsrUnused

    .weak      gmosPalIsrUSBL
    .thumb_set gmosPalIsrUSBL, gmosPalIsrUnused

    .weak      gmosPalIsrDAC
    .thumb_set gmosPalIsrDAC, gmosPalIsrUnused

    .weak      gmosPalIsrCOMPWK
    .thumb_set gmosPalIsrCOMPWK, gmosPalIsrUnused

    .weak      gmosPalIsrEXTIF
    .thumb_set gmosPalIsrEXTIF, gmosPalIsrUnused

    .weak      gmosPalIsrLCD
    .thumb_set gmosPalIsrLCD, gmosPalIsrUnused

    .weak      gmosPalIsrTIM9
    .thumb_set gmosPalIsrTIM9, gmosPalIsrUnused

    .weak      gmosPalIsrTIM10
    .thumb_set gmosPalIsrTIM10, gmosPalIsrUnused

    .weak      gmosPalIsrTIM11
    .thumb_set gmosPalIsrTIM11, gmosPalIsrUnused

    .weak      gmosPalIsrTIM2
    .thumb_set gmosPalIsrTIM2, gmosPalIsrUnused

    .weak      gmosPalIsrTIM3
    .thumb_set gmosPalIsrTIM3, gmosPalIsrUnused

    .weak      gmosPalIsrTIM4
    .thumb_set gmosPalIsrTIM4, gmosPalIsrUnused

    .weak      gmosPalIsrI2C1EV
    .thumb_set gmosPalIsrI2C1EV, gmosPalIsrUnused

    .weak      gmosPalIsrI2C1ER
    .thumb_set gmosPalIsrI2C1ER, gmosPalIsrUnused

    .weak      gmosPalIsrI2C2EV
    .thumb_set gmosPalIsrI2C2EV, gmosPalIsrUnused

    .weak      gmosPalIsrI2C2ER
    .thumb_set gmosPalIsrI2C2ER, gmosPalIsrUnused

    .weak      gmosPalIsrSPI1
    .thumb_set gmosPalIsrSPI1, gmosPalIsrUnused

    .weak      gmosPalIsrSPI2
    .thumb_set gmosPalIsrSPI2, gmosPalIsrUnused

    .weak      gmosPalIsrUSART1
    .thumb_set gmosPalIsrUSART1, gmosPalIsrUnused

    .weak      gmosPalIsrUSART2
    .thumb_set gmosPalIsrUSART2, gmosPalIsrUnused

    .weak      gmosPalIsrUSART3
    .thumb_set gmosPalIsrUSART3, gmosPalIsrUnused

    .weak      gmosPalIsrEXTIG
    .thumb_set gmosPalIsrEXTIG, gmosPalIsrUnused

    .weak      gmosPalIsrRTCAL
    .thumb_set gmosPalIsrRTCAL, gmosPalIsrUnused

    .weak      gmosPalIsrUSBWK
    .thumb_set gmosPalIsrUSBWK, gmosPalIsrUnused

    .weak      gmosPalIsrTIM6
    .thumb_set gmosPalIsrTIM6, gmosPalIsrUnused

    .weak      gmosPalIsrTIM7
    .thumb_set gmosPalIsrTIM7, gmosPalIsrUnused

    .weak      gmosPalIsrSDIO
    .thumb_set gmosPalIsrSDIO, gmosPalIsrUnused

    .weak      gmosPalIsrTIM5
    .thumb_set gmosPalIsrTIM5, gmosPalIsrUnused

    .weak      gmosPalIsrSPI3
    .thumb_set gmosPalIsrSPI3, gmosPalIsrUnused

    .weak      gmosPalIsrUART4
    .thumb_set gmosPalIsrUART4, gmosPalIsrUnused

    .weak      gmosPalIseUART5
    .thumb_set gmosPalIseUART5, gmosPalIsrUnused

    .weak      gmosPalIsrDMA2A
    .thumb_set gmosPalIsrDMA2A, gmosPalIsrUnused

    .weak      gmosPalIsrDMA2B
    .thumb_set gmosPalIsrDMA2B, gmosPalIsrUnused

    .weak      gmosPalIsrDMA2C
    .thumb_set gmosPalIsrDMA2C, gmosPalIsrUnused

    .weak      gmosPalIsrDMA2D
    .thumb_set gmosPalIsrDMA2D, gmosPalIsrUnused

    .weak      gmosPalIsrDMA2E
    .thumb_set gmosPalIsrDMA2E, gmosPalIsrUnused

    .weak      gmosPalIsrAES
    .thumb_set gmosPalIsrAES, gmosPalIsrUnused

    .weak      gmosPalIsrCOMPAQ
    .thumb_set gmosPalIsrCOMPAQ, gmosPalIsrUnused

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
