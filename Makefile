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
# This is the toplevel makefile for building applications based on the
# Gubbins microcontroller operating system. The build configuration
# options can be set by assigning the appropriate environment variables
# prior to initiating the build process.
#

# Gets the location of this makefile as the root of the GubbinsMOS
# source code directory.
GMOS_GIT_DIR := ${abspath ${CURDIR}/$(dir $(firstword $(MAKEFILE_LIST)))}

# Specifies the location of the gateway build directory if not defined
# by an environment variable.
ifndef GMOS_BUILD_DIR
GMOS_BUILD_DIR = /tmp/gmos_build
endif

# Specifies the path to the target platform directory. By default this
# is the STM32L0XX target.
ifndef TARGET_PLATFORM
TARGET_PLATFORM = st-micro/stm32l0xx
endif
TARGET_PLATFORM_DIR = ${GMOS_GIT_DIR}/platforms/${TARGET_PLATFORM}

# Compile the platform specific demo application by default.
ifndef GMOS_APP_DIR
GMOS_APP_DIR = ${TARGET_PLATFORM_DIR}/demo
endif

# Specifies the target device to use. By default this is the STM32L010RB
# device.
ifndef TARGET_DEVICE
TARGET_DEVICE = STM32L010RB
endif

# Include the platform setup makefile fragment. This defines the
# platform specific compiler options.
include ${TARGET_PLATFORM_DIR}/platform-setup.mk

# Generate the firmware binary.
${GMOS_BUILD_DIR}/firmware.bin : ${GMOS_BUILD_DIR}/firmware.elf
	${OC} -S -O binary $< $@
	${OS} $<

# Link all the generated object files. Note that 'shell ls' is used to
# get the list of object files instead of 'wildcard', since the wildcard
# expansion can occur before the timestamp dependencies are met.
${GMOS_BUILD_DIR}/firmware.elf : \
		${GMOS_BUILD_DIR}/app/timestamp \
		${GMOS_BUILD_DIR}/common/timestamp \
		${GMOS_BUILD_DIR}/platform/timestamp
	${LD} -o $@ ${LDFLAGS} -T${GMOS_BUILD_DIR}/platform/target.ld \
		$(shell ls -xw0 ${GMOS_BUILD_DIR}/app/*.o) \
		$(shell ls -xw0 ${GMOS_BUILD_DIR}/common/*.o) \
		$(shell ls -xw0 ${GMOS_BUILD_DIR}/platform/*.o) \
		$(addprefix -l, ${LDLIBS})

# Include the application source files makefile fragment.
include ${GMOS_APP_DIR}/app-build.mk

# Include the common source files makefile fragment.
include ${GMOS_GIT_DIR}/common/common-build.mk

# Include the platform build makefile fragment. This defines the
# platform specific source build process.
include ${TARGET_PLATFORM_DIR}/platform-build.mk

# Remove all build files.
clean :
	rm -rf ${GMOS_BUILD_DIR}
