#!/usr/bin/env python3

#
# The Gubbins Microcontroller Operating System
#
# Copyright 2020 Zynaptic Limited
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
# a specific STM32L0XX device.
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
        + "for specific STM32L0XX devices.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        "--device", default="STM32L010RB", help="the name of the target device"
    )
    parser.add_argument(
        "--output", default=None, help="the name of the generated output file"
    )
    args = parser.parse_args()
    return args


#
# Define the parameter lookup table for the different STM32L0XX devices.
#
deviceParameterTable = {
    "STM32L010RB": {
        "TARGET_DEVICE": "STM32L010RB",
        "FLASH_SIZE": "128K",
        "RAM_SIZE": "20K",
        "RAM_ADDR_TOP": "0x20005000",
        "RAM_RESERVED": "0x1000",
    },
    "STM32L072CZ": {
        "TARGET_DEVICE": "STM32L072CZ",
        "FLASH_SIZE": "192K",
        "RAM_SIZE": "20K",
        "RAM_ADDR_TOP": "0x20005000",
        "RAM_RESERVED": "0x1000",
    },
}

#
# Define the linker script template.
#
linkerTemplateString = """
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
 * Automatically generated GubbinsMOS linker script for ${TARGET_DEVICE}.
 */

/* Specify the start of the descending application stack */
_estack = ${RAM_ADDR_TOP};

/* Specify the amount of reserved memory for the application stack */
_reservedRamSize = ${RAM_RESERVED};

/* Specify the flash and RAM physical memory */
MEMORY
{
    FLASH ( rx )      : ORIGIN = 0x08000000, LENGTH = ${FLASH_SIZE}
    RAM ( rxw )       : ORIGIN = 0x20000000, LENGTH = ${RAM_SIZE}
}

/* Map the logical memory sections */
SECTIONS
{
    /* Place the vector table at the start of flash memory */
    .gmosPalArmVectors 0x08000000 :
    {
        KEEP (*(.gmosPalArmVectors))
    } >FLASH

    /* Append the text section containing the main program code */
    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        . = ALIGN(4);
    } >FLASH

    /* Append the read only constant data */
    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    } >FLASH

    /* Append the ARM library data */
    .ARM.extab   : {
        . = ALIGN(4);
        *(.ARM.extab* .gnu.linkonce.armextab.*)
        . = ALIGN(4);
    } >FLASH

    .ARM : {
        . = ALIGN(4);
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
        . = ALIGN(4);
    } >FLASH

    /* Allocate the data section in RAM for initialised static data */
    _sidata = .;
    .data : AT(_sidata)
    {
        . = ALIGN(16);
        _sdata = .;
        *(.data)
        *(.data*)
        . = ALIGN(16);
        _edata = .;
    } >RAM

    /* Allocate the bss section for zero initialised static data */
    .bss :
    {
        . = ALIGN(16);
        _sbss = .;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(16);
        _ebss = .;
    } >RAM

    /* Mark the end of statically allocated RAM */
    end = .;

    /* Allocate the reserved RAM section for application stack */
    .reserved :
    {
        . = ALIGN(16);
        _sreserved = .;
        . = . + _reservedRamSize;
        . = ALIGN(16);
        _ereserved = .;
    } >RAM

    /* Remove information from the compiler libraries */
    /DISCARD/ :
    {
        libc.a ( * )
        libm.a ( * )
        libgcc.a ( * )
    }

    .ARM.attributes 0 : { *(.ARM.attributes) }
}

"""

#
# Provide main entry point.
#
def main(params, out):
    deviceParams = deviceParameterTable[params.device]
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
