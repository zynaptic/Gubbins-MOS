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
# This is the makefile fragment for setting up the compiler and build
# tool options for the ST STM32L0XX range of target devices using the
# ARM gcc cross compiler.
#

ARCH_NAME = cortex-m0plus

# Specify the location of the toolchain directory. By default this
# assumes the standard Ubuntu package installation directory.
ifndef ARM_GCC_TOOLCHAIN_DIR
ARM_GCC_TOOLCHAIN_DIR = /usr
endif

CC = $(ARM_GCC_TOOLCHAIN_DIR)/bin/arm-none-eabi-gcc
AS = $(ARM_GCC_TOOLCHAIN_DIR)/bin/arm-none-eabi-gcc
LD = $(ARM_GCC_TOOLCHAIN_DIR)/bin/arm-none-eabi-gcc
OC = $(ARM_GCC_TOOLCHAIN_DIR)/bin/arm-none-eabi-objcopy
OD = $(ARM_GCC_TOOLCHAIN_DIR)/bin/arm-none-eabi-objdump
OS = $(ARM_GCC_TOOLCHAIN_DIR)/bin/arm-none-eabi-size

# Assembler options.
ASFLAGS += -c
ASFLAGS += -mcpu=${ARCH_NAME}
ASFLAGS += -mthumb
ASFLAGS += -Wall
ASFLAGS += -MMD

# C compiler options.
CFLAGS += -c
CFLAGS += -mcpu=${ARCH_NAME}
CFLAGS += -mthumb
CFLAGS += -mfloat-abi=soft
CFLAGS += -Wall
CFLAGS += -g
CFLAGS += -ffunction-sections
CFLAGS += -fdata-sections
CFLAGS += -nostdlib
CFLAGS += -lgcc
CFLAGS += -DTARGET_DEVICE=${TARGET_DEVICE}
CFLAGS += -MMD

# Linker options.
LDFLAGS += -Wl,-L${TARGET_PLATFORM_DIR}/vendor/lib/gcc
LDFLAGS += -mcpu=${ARCH_NAME}
LDFLAGS += -mthumb
LDFLAGS += -mfloat-abi=soft
LDFLAGS += -nostdlib
LDFLAGS += -Wl,--gc-sections

# Required linker library names.
LDLIBS += gcc
LDLIBS += arm_cortexM0l_math
