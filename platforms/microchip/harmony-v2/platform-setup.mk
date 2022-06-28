#
# The Gubbins Microcontroller Operating System
#
# Copyright 2020-2022 Zynaptic Limited
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
# This is the makefile fragment for setting up the compiler and build
# tool options for the Microchip range of PIC32 target devices supported
# by the Harmony-V2 framework and XC32 compiler.
#

# Specifies the target device to use. By default this is the
# PIC32MZ2048EFM100 device.
ifndef GMOS_TARGET_DEVICE
GMOS_TARGET_DEVICE = 32MZ2048EFM100
endif

# Specifies the size of the 'C' language heap for dynamic memory
# management.
ifndef GMOS_CONFIG_HEAP_SIZE
GMOS_CONFIG_HEAP_SIZE = 393216
endif

# Specifies the location of the toolchain directory. By default this
# assumes the standard XC32 installation directory.
ifndef XC32_GCC_TOOLCHAIN_DIR
XC32_GCC_TOOLCHAIN_DIR = /opt/microchip/xc32/v2.20
endif

# Specifies the location of the MPLAB tools distribution directory.
ifndef MPLAB_DISTRIBUTION_DIR
MPLAB_DISTRIBUTION_DIR = /opt/microchip/mplabx/v5.20
endif

# Specifies the type of device programmer to be used.
ifndef MPLAB_PROGRAMMER_TYPE
MPLAB_PROGRAMMER_TYPE = PKOB
endif

# Set up the command line tools.
CC = $(XC32_GCC_TOOLCHAIN_DIR)/bin/xc32-gcc
AS = $(XC32_GCC_TOOLCHAIN_DIR)/bin/xc32-gcc
LD = $(XC32_GCC_TOOLCHAIN_DIR)/bin/xc32-gcc
OC = $(XC32_GCC_TOOLCHAIN_DIR)/bin/xc32-objcopy
OD = $(XC32_GCC_TOOLCHAIN_DIR)/bin/xc32-objdump
OS = $(XC32_GCC_TOOLCHAIN_DIR)/bin/xc32-size

# Assembler options.
ASFLAGS += -c
ASFLAGS += -mprocessor=${GMOS_TARGET_DEVICE}
ASFLAGS += -Wall
ASFLAGS += -MMD

# C compiler options.
CFLAGS += -c
# CFLAGS += -std=c99
CFLAGS += -mprocessor=${GMOS_TARGET_DEVICE}
CFLAGS += -Wall
CFLAGS += -O2
CFLAGS += -G 2
CFLAGS += -ffunction-sections
CFLAGS += -fdata-sections
CFLAGS += -no-legacy-libc
CFLAGS += -DTARGET_DEVICE=${GMOS_TARGET_DEVICE}
CFLAGS += -DGMOS_CONFIG_HEAP_SIZE=${GMOS_CONFIG_HEAP_SIZE}
CFLAGS += -MMD

# Linker options.
LDFLAGS += -mprocessor=${GMOS_TARGET_DEVICE}
LDFLAGS += -no-legacy-libc
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -Wl,--no-code-in-dinit,--no-dinit-in-serial-mem
LDFLAGS += -Wl,--defsym=_min_heap_size=${GMOS_CONFIG_HEAP_SIZE}

# Specify the location of the Harmony-V2 distribution directory. By
# default this assumes the standard location.
ifndef HARMONY_DISTRIBUTION_DIR
HARMONY_DISTRIBUTION_DIR = /opt/microchip/harmony/v2_06
endif

# Specify the location of the Harmony-V2 generated code directory.
# By default this assumes a local development directory at the same
# level as the GubbinsMOS repository.
ifndef HARMONY_CODEGEN_DIR
HARMONY_CODEGEN_DIR = ${abspath ../Harmony-V2-Dev}
endif

# Set the Harmony framework directory location.
HARMONY_FRAMEWORK_SRC_DIR = ${abspath ${HARMONY_DISTRIBUTION_DIR}/framework}

# Add the Harmony framework include paths that may be used by common
# GubbinsMOS files.
CFLAGS += -I${HARMONY_FRAMEWORK_SRC_DIR}
CFLAGS += -I${HARMONY_CODEGEN_DIR}/firmware/src/system_config/default
