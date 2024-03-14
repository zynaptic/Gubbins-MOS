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
# This is the makefile fragment for setting up the compiler and build
# tool options for the Silicon Labs EFR32xG2x range of target devices
# using the ARM gcc cross compiler.
#

# Specifies the target device family to use. This should be one of the
# EFR series 2 device families.
ifndef GMOS_TARGET_DEVICE_FAMILY
GMOS_TARGET_DEVICE_FAMILY = EFR32MG24
endif

# The lower case family name is used in some of the SDK file names.
GMOS_TARGET_DEVICE_FAMILY_LC := $(shell echo $(GMOS_TARGET_DEVICE_FAMILY) | tr A-Z a-z)

# Specifies the target device variant to use. By default this is the
# device used on the xG24 +20dBm Pro development kit (xG24-PK6010A).
ifndef GMOS_TARGET_DEVICE_VARIANT
GMOS_TARGET_DEVICE_VARIANT = EFR32MG24B220F1536IM48
endif

# All EFR32xG2x devices use the Cortex-M33 core.
ARCH_NAME = cortex-m33

# Specify the location of the toolchain directory. By default this
# assumes the Silicon Labs recommended ARM toolchain located in the /opt
# directory.
ifndef ARM_GCC_TOOLCHAIN_DIR
ARM_GCC_TOOLCHAIN_DIR = /opt/arm-gnu-toolchain-12.2.rel1-x86_64-arm-none-eabi/
endif

# Specify the location of the Silicon Labs Gecko SDK directory. By
# default this assumes it has been downloaded to the Simplicity Studio
# working directory.
ifndef GMOS_GECKO_SDK_DIR
GMOS_GECKO_SDK_DIR = ${HOME}/SimplicityStudio/SDKs/gecko_sdk
endif

# Specify device specific SDK file locations.
GMOS_TARGET_DEVICE_FAMILY_DIR := \
	${GMOS_GECKO_SDK_DIR}/platform/Device/SiliconLabs/${GMOS_TARGET_DEVICE_FAMILY}

# Specify that the platform layer provides the PSA cryptography library
# and provide the path to the API header directory.
GMOS_PLATFORM_PSA_CRYPTO_API_DIR = ${GMOS_GECKO_SDK_DIR}/util/third_party/mbedtls/include

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
ASFLAGS += -MP

# Common C/C++ compiler options.
CXXFLAGS += -c
CXXFLAGS += -g3
CXXFLAGS += -gdwarf-2
CXXFLAGS += -mcpu=${ARCH_NAME}
CXXFLAGS += -mthumb
CXXFLAGS += -D${GMOS_TARGET_DEVICE_VARIANT}=1
CXXFLAGS += -Os
CXXFLAGS += -Wall
CXXFLAGS += -Wextra
CXXFLAGS += -ffunction-sections
CXXFLAGS += -fdata-sections
CXXFLAGS += -mfpu=fpv5-sp-d16
CXXFLAGS += -mfloat-abi=hard
CXXFLAGS += -mcmse
CXXFLAGS += --specs=nano.specs
CXXFLAGS += -MMD
CXXFLAGS += -MP

# C specific compiler options.
CFLAGS = ${CXXFLAGS}
CFLAGS += -std=c99

# C++ specific compiler options.
CPPFLAGS = ${CXXFLAGS}

# Linker options.
LDFLAGS += -g3
LDFLAGS += -gdwarf-2
LDFLAGS += -mcpu=${ARCH_NAME}
LDFLAGS += -mthumb
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -Wl,-Map=${GMOS_BUILD_DIR}/platform/target.map
LDFLAGS += -mfpu=fpv5-sp-d16
LDFLAGS += -mfloat-abi=hard
LDFLAGS += --specs=nano.specs
LDFLAGS += -T${GMOS_BUILD_DIR}/platform/target.ld
LDFLAGS += -L${GMOS_GECKO_SDK_DIR}/platform/emdrv/nvm3/lib

# Required linker library names.
LDLIBS += gcc
LDLIBS += c
LDLIBS += m
LDLIBS += nosys
LDLIBS += supc++
LDLIBS += nvm3_CM33_gcc
