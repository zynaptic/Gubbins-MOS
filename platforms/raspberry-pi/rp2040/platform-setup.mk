#
# The Gubbins Microcontroller Operating System
#
# Copyright 2022-2023 Zynaptic Limited
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
# tool options for the Raspberry Pi Pico range of target devices using
# the ARM GCC cross compiler.
#

# Specifies the target device to use. By default this is the
# RP2040 device.
ifndef GMOS_TARGET_DEVICE
GMOS_TARGET_DEVICE = RP2040
endif

# All RP2040 devices use the Cortex-M0+ core.
ARCH_NAME = cortex-m0plus

# Specify the location of the toolchain directory. By default this
# assumes the standard Ubuntu package installation directory.
ifndef ARM_GCC_TOOLCHAIN_DIR
ARM_GCC_TOOLCHAIN_DIR = /usr
endif

# Specify the location of the Raspberry Pi SDK directory. By default
# this assumes it has been downloaded to the 'imports' directory.
ifndef GMOS_PICO_SDK_DIR
GMOS_PICO_SDK_DIR = ${GMOS_GIT_DIR}/imports/pico-sdk
endif

# Specify the linkage to be used for the firmware image. This currently
# just implements the default setting.
ifndef GMOS_PICO_LINK_MODE
GMOS_PICO_LINK_MODE = PICO_LINK_DEFAULT
endif

# Specify the second stage bootloader to be used for the firware image.
# This currently only supports the Winbond W25Q16 device on the standard
# Raspberry Pi Pico board.
ifndef GMOS_PICO_BOOT_DEVICE
GMOS_PICO_BOOT_DEVICE = W25Q16
endif

# Specify the various standard command line tools to use.
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
CFLAGS += --specs=nano.specs
CFLAGS += -lgcc
CFLAGS += -DTARGET_DEVICE=${GMOS_TARGET_DEVICE}
CFLAGS += -MMD
CFLAGS += -O2

# Standard linker options.
LDFLAGS += -mcpu=${ARCH_NAME}
LDFLAGS += -mthumb
LDFLAGS += -mfloat-abi=soft
LDFLAGS += --specs=nano.specs
LDFLAGS += -Wl,--gc-sections

# Linking the bootloader does not use the standard linker script.
BLDFLAGS += -mcpu=${ARCH_NAME}
BLDFLAGS += -mthumb
BLDFLAGS += -mfloat-abi=soft
BLDFLAGS += -nostdlib

# Select the correct linker options for default execute in place
# operation.
ifeq (${GMOS_PICO_LINK_MODE}, PICO_LINK_DEFAULT)
LDFLAGS += -T${GMOS_PICO_SDK_DIR}/src/rp2_common/pico_standard_link/memmap_default.ld
endif

# Enable function wrappers for hardware divider.
LDWRAPPERS += \
	__aeabi_idiv \
	__aeabi_idivmod \
	__aeabi_ldivmod \
	__aeabi_uidiv \
	__aeabi_uidivmod \
	__aeabi_uldivmod

# Enable function wrappers for simplified printf support.
LDWRAPPERS += \
	snprintf \
	vsnprintf

# Add function wrappers to linker options.
LDFLAGS += $(foreach WRAPFN, ${LDWRAPPERS}, -Wl,--wrap=${WRAPFN})

# Required linker library names.
LDLIBS += gcc
LDLIBS += c
LDLIBS += m
LDLIBS += nosys
