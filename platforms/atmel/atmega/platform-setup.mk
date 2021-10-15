#
# The Gubbins Microcontroller Operating System
#
# Copyright 2020-2021 Zynaptic Limited
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
# tool options for the Microchip/Atmel ATMEGA range of target devices
# using the AVR gcc cross compiler.
#

# Specify the location of the toolchain directory. By default this
# assumes the standard Ubuntu package installation directory.
ifndef AVR_GCC_TOOLCHAIN_DIR
AVR_GCC_TOOLCHAIN_DIR = /usr
endif

CC = $(AVR_GCC_TOOLCHAIN_DIR)/bin/avr-gcc
AS = $(AVR_GCC_TOOLCHAIN_DIR)/bin/avr-gcc
LD = $(AVR_GCC_TOOLCHAIN_DIR)/bin/avr-gcc
OC = $(AVR_GCC_TOOLCHAIN_DIR)/bin/avr-objcopy
OD = $(AVR_GCC_TOOLCHAIN_DIR)/bin/avr-objdump
OS = $(AVR_GCC_TOOLCHAIN_DIR)/bin/avr-size

# Assembler options.
ASFLAGS += -c
ASFLAGS += -mmcu=${TARGET_DEVICE}
ASFLAGS += -Wall

# C compiler options.
CFLAGS += -c
CFLAGS += -mmcu=${TARGET_DEVICE}
CFLAGS += -Wall
CFLAGS += -Os
CFLAGS += -ffunction-sections
CFLAGS += -fdata-sections
CFLAGS += -nodevicelib
CFLAGS += -DTARGET_DEVICE=${TARGET_DEVICE}

# Linker options.
LDFLAGS += -Wl,-L${TARGET_PLATFORM_DIR}/vendor/lib/gcc
LDFLAGS += -mmcu=${TARGET_DEVICE}
LDFLAGS += -nodevicelib
LDFLAGS += -Wl,--gc-sections

# Required linker library names.
LDLIBS += gcc
