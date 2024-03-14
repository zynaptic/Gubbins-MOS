#!/usr/bin/env python3

#
# The Gubbins Microcontroller Operating System
#
# Copyright 2023-2024 Zynaptic Limited
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
# implied. See the License for the specific language governing
# permissions and limitations under the License.
#

#
# This tool is used to generate a suitable GubbinsMOS linker script for
# a specific Silicon labs EFR32xG2x device. The vendor template for the
# linker script is provided as a Jinja template file, so this is
# processed using the appropriate parameters for the target device.
#

import sys
import argparse

from string import Template

#
# Extract the command line arguments.
#
def parseCommandLine():
    parser = argparse.ArgumentParser(
        description="This script is used to generate gcc linker scripts "
        + "for specific EFR32xG2x devices.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        "--device",
        default="EFR32MG24B220F1536IM48",
        help="the name of the target device",
    )
    parser.add_argument(
        "--output", default=None, help="the name of the generated output file"
    )
    args = parser.parse_args()
    return args


#
# Define the parameter lookup table for the different EFR32xG2x devices.
#
deviceParameterTable = {
    "EFR32MG24B220F1536IM48": {
        "TARGET_DEVICE": "EFR32MG24B220F1536IM48",
        "FLASH_MEMORY_BASE": 0x08000000,
        "FLASH_MEMORY_SIZE": 1536 * 1024,
        "FLASH_PAGE_SIZE": 8 * 1024,
        "RAM_MEMORY_SIZE": 256 * 1024,
        "NVM3_PAGE_ALLOCATION": 5,
    },
}

#
# Define the linker script template.
#
linkerTemplateString = """
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
 * Automatically generated GubbinsMOS linker script for ${TARGET_DEVICE}.
 */

MEMORY
{
  FLASH (rx) : ORIGIN = ${FLASH_IMAGE_BASE}, LENGTH = ${FLASH_IMAGE_SIZE}
  NVM (r)    : ORIGIN = ${FLASH_NVM_BASE}, LENGTH = ${FLASH_NVM_SIZE}
  RAM (rwx)  : ORIGIN = 0x20000000, LENGTH = ${RAM_MEMORY_SIZE}
}

ENTRY(Reset_Handler)

SECTIONS
{
  .text :
  {
    KEEP(*(.vectors))
    *(.text*)

    KEEP(*(.init))
    KEEP(*(.fini))

    /* .ctors */
    *crtbegin.o(.ctors)
    *crtbegin?.o(.ctors)
    *(EXCLUDE_FILE(*crtend?.o *crtend.o) .ctors)
    *(SORT(.ctors.*))
    *(.ctors)

    /* .dtors */
    *crtbegin.o(.dtors)
    *crtbegin?.o(.dtors)
    *(EXCLUDE_FILE(*crtend?.o *crtend.o) .dtors)
    *(SORT(.dtors.*))
    *(.dtors)

    *(.rodata*)

    KEEP(*(.eh_frame*))
  } > FLASH

  /*
   * TODO: SG veneers:
   * All SG veneers are placed in the special output section .gnu.sgstubs. Its start address
   * must be set, either with the command line option ‘--section-start’ or in a linker script,
   * to indicate where to place these veneers in memory.
   */
/*
  .gnu.sgstubs : ALIGN(32)
  {
    . = ALIGN(32);
    linker_sg_begin = .;
    KEEP(*(.gnu.sgstubs*))
    . = ALIGN(32);
  } > FLASH
  linker_sg_end = linker_sg_begin + SIZEOF(.gnu.sgstubs);
*/

  .ARM.extab :
  {
    *(.ARM.extab* .gnu.linkonce.armextab.*)
  } > FLASH

  __exidx_start = .;
  .ARM.exidx :
  {
    *(.ARM.exidx* .gnu.linkonce.armexidx.*)
  } > FLASH
  __exidx_end = .;

  .copy.table :
  {
    . = ALIGN(4);
    __copy_table_start__ = .;

    LONG (__etext)
    LONG (__data_start__)
    LONG ((__data_end__ - __data_start__) / 4)

    __copy_table_end__ = .;
  } > FLASH

  .zero.table :
  {
    . = ALIGN(4);
    __zero_table_start__ = .;
    __zero_table_end__ = .;
    __etext = ALIGN(4);
  } > FLASH

  .data : AT (__etext)
  {
    __data_start__ = .;
    *(vtable)
    *(.data*)
    . = ALIGN (4);
    PROVIDE (__ram_func_section_start = .);
    *(.ram)
    PROVIDE (__ram_func_section_end = .);

    . = ALIGN(4);
    /* preinit data */
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP(*(.preinit_array))
    PROVIDE_HIDDEN (__preinit_array_end = .);

    . = ALIGN(4);
    /* init data */
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array))
    PROVIDE_HIDDEN (__init_array_end = .);

    . = ALIGN(4);
    /* finit data */
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP(*(SORT(.fini_array.*)))
    KEEP(*(.fini_array))
    PROVIDE_HIDDEN (__fini_array_end = .);

    KEEP(*(.jcr*))
    . = ALIGN(4);
    /* All data end */
    __data_end__ = .;

  } > RAM

  .bss :
  {
    . = ALIGN(4);
    __bss_start__ = .;
    *(.bss)
    *(.bss.*)
    *(COMMON)
    . = ALIGN(4);
    __bss_end__ = .;
  } > RAM AT > RAM

  __ramfuncs_start__ = .;

  __vma_ramfuncs_start__ = .;
  __lma_ramfuncs_start__ = __etext + SIZEOF(.data);

  __text_application_ram_offset__ = . - __vma_ramfuncs_start__;
  text_application_ram . : AT(__lma_ramfuncs_start__ + __text_application_ram_offset__)
  {
    . = ALIGN(4);
    __text_application_ram_start__ = .;
    *(text_application_ram)
    . = ALIGN(4);
    __text_application_ram_end__ = .;
  } > RAM

  . = ALIGN(4);
  __vma_ramfuncs_end__ = .;
  __lma_ramfuncs_end__ = __lma_ramfuncs_start__ + __text_application_ram_offset__ + SIZEOF(text_application_ram);

  __ramfuncs_end__ = .;

  .heap (COPY):
  {
    __HeapBase = .;
    __end__ = .;
    end = __end__;
    _end = __end__;
    KEEP(*(.heap*))
    __HeapLimit = .;
  } > RAM

  /* TODO: ARMv8-M stack sealing:
     to use ARMv8-M stack sealing uncomment '.stackseal' section and KEEP(*(.stackseal*))
     in .stack_dummy section
   */
/*
  .stackseal (COPY) :
  {
    . = ALIGN(8);
    __StackSeal = .;
    . = . + 8;
    . = ALIGN(8);
  } > RAM
*/

  /* .stack_dummy section doesn't contains any symbols. It is only
   * used for linker to calculate size of stack sections, and assign
   * values to stack symbols later */
  .stack_dummy (COPY):
  {
    KEEP(*(.stack*))
 /* KEEP(*(.stackseal*))*/
  } > RAM

  /* Set stack top to end of RAM, and stack limit move down by
   * size of stack_dummy section */
  __StackTop = ORIGIN(RAM) + LENGTH(RAM);
  __StackLimit = __StackTop - SIZEOF(.stack_dummy);
  PROVIDE(__stack = __StackTop);

  /* Place reserved NVM pages at the top of the flash memory area. */
  linker_nvm_begin = ORIGIN(NVM);

  /* Check if data + heap + stack exceeds RAM limit */
  ASSERT(__StackLimit >= __HeapLimit, "region RAM overflowed with stack")

  /* Check if FLASH usage exceeds FLASH size */
  ASSERT( LENGTH(FLASH) >= (__etext + SIZEOF(.data)), "FLASH memory overflowed !")
}

"""

#
# Provide main entry point.
#
def main(params, out):

    # Generate derived device parameters.
    deviceParams = deviceParameterTable[params.device]

    # The reserved flash area is used for the NVM3 EEPROM emulation,
    # with the final page being reserved for permanent token and key
    # storage.
    # TODO: This will need to be updated to support bootloader based
    # images.
    flashMemoryTop = (
        deviceParams["FLASH_MEMORY_BASE"] + deviceParams["FLASH_MEMORY_SIZE"]
    )
    flashNvmSize = deviceParams["FLASH_PAGE_SIZE"] * (
        1 + deviceParams["NVM3_PAGE_ALLOCATION"]
    )
    flashNvmBase = flashMemoryTop - flashNvmSize
    flashImageSize = deviceParams["FLASH_MEMORY_SIZE"] - flashNvmSize
    flashImageBase = deviceParams["FLASH_MEMORY_BASE"]

    deviceParams["FLASH_IMAGE_SIZE"] = flashImageSize
    deviceParams["FLASH_IMAGE_BASE"] = flashImageBase
    deviceParams["FLASH_NVM_SIZE"] = flashNvmSize
    deviceParams["FLASH_NVM_BASE"] = flashNvmBase

    # Insert device parameters into the linker script template.
    linkerTemplate = Template(linkerTemplateString)
    linkerScript = linkerTemplate.substitute(deviceParams)
    out.write(linkerScript)


#
# Run the script with the provided command line options.
#
try:
    params = parseCommandLine()
    if params.output == None:
        main(params, sys.stdout)
    else:
        with open(params.output, "w") as f:
            main(params, f)
except KeyboardInterrupt as e:
    exit()
except Exception as e:
    print(e)
    exit()
