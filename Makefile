#
# The Gubbins Microcontroller Operating System
#
# Copyright 2020-2023 Zynaptic Limited
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

# Specifies the path to the target platform directory. By default this
# is the STM32L0XX target.
ifndef GMOS_TARGET_PLATFORM
GMOS_TARGET_PLATFORM = st-micro/stm32l0xx
endif
TARGET_PLATFORM_DIR = ${GMOS_GIT_DIR}/platforms/${GMOS_TARGET_PLATFORM}

# Specifies the location of the GubbinsMOS build directory if not
# defined by an environment variable.
ifndef GMOS_BUILD_DIR
GMOS_BUILD_DIR = /tmp/gmos_build/${GMOS_TARGET_PLATFORM}
endif

# Compile the platform specific demo application by default.
ifndef GMOS_APP_DIR
GMOS_APP_DIR = ${TARGET_PLATFORM_DIR}/demo
endif

# Include the platform setup makefile fragment. This defines the
# platform specific compiler options.
include ${TARGET_PLATFORM_DIR}/platform-setup.mk

# Optionally include the application setup makefile fragment. This
# allows application specific options to be applied if required.
ifneq ($(wildcard ${GMOS_APP_DIR}/app-setup.mk),)
include ${GMOS_APP_DIR}/app-setup.mk
endif

# Generate both binary and hex format files.
all : \
	${GMOS_BUILD_DIR}/firmware.elf \
	${GMOS_BUILD_DIR}/firmware.hex \
	${GMOS_BUILD_DIR}/firmware.bin

# Specify the common component timestamps and object files.
COMPONENT_TIMESTAMPS = \
	${GMOS_BUILD_DIR}/app/timestamp \
	${GMOS_BUILD_DIR}/common/timestamp \
	${GMOS_BUILD_DIR}/platform/timestamp

COMPONENT_OBJECT_FILES = \
	${GMOS_BUILD_DIR}/app/*.o \
	${GMOS_BUILD_DIR}/common/*.o \
	${GMOS_BUILD_DIR}/platform/*.o \
	${GMOS_BUILD_DIR}/platform/*/*.o

# If one or more network directories have been specified, add them
# to the set of common components.
ifdef GMOS_TARGET_NETWORK_DIRS
NETWORK_BUILD_PATH = ${GMOS_BUILD_DIR}/network/$(DIR)
COMPONENT_TIMESTAMPS += \
	$(foreach DIR, ${GMOS_TARGET_NETWORK_DIRS}, $(NETWORK_BUILD_PATH)/timestamp)
COMPONENT_OBJECT_FILES += \
	$(foreach DIR, ${GMOS_TARGET_NETWORK_DIRS}, $(NETWORK_BUILD_PATH)/*.o)
endif

# If one or more target radio directories have been specified, add them
# to the set of common components.
ifdef GMOS_TARGET_RADIO_DIRS
RADIO_BUILD_PATH = ${GMOS_BUILD_DIR}/radios/$(DIR)
COMPONENT_TIMESTAMPS += \
	$(foreach DIR, ${GMOS_TARGET_RADIO_DIRS}, $(RADIO_BUILD_PATH)/timestamp)
COMPONENT_OBJECT_FILES += \
	$(foreach DIR, ${GMOS_TARGET_RADIO_DIRS}, $(RADIO_BUILD_PATH)/*.o)
endif

# If one or more target sensor directories have been specified, add them
# to the set of common components.
ifdef GMOS_TARGET_SENSOR_DIRS
GMOS_TARGET_SENSOR_DIRS += common
SENSOR_BUILD_PATH = ${GMOS_BUILD_DIR}/sensors/$(DIR)
COMPONENT_TIMESTAMPS += \
	$(foreach DIR, ${GMOS_TARGET_SENSOR_DIRS}, $(SENSOR_BUILD_PATH)/timestamp)
COMPONENT_OBJECT_FILES += \
	$(foreach DIR, ${GMOS_TARGET_SENSOR_DIRS}, $(SENSOR_BUILD_PATH)/*.o)
endif

# If one or more target display directories have been specified, add
# them to the set of common components.
ifdef GMOS_TARGET_DISPLAY_DIRS
GMOS_TARGET_DISPLAY_DIRS += common
DISPLAY_BUILD_PATH = ${GMOS_BUILD_DIR}/displays/$(DIR)
COMPONENT_TIMESTAMPS += \
	$(foreach DIR, ${GMOS_TARGET_DISPLAY_DIRS}, $(DISPLAY_BUILD_PATH)/timestamp)
COMPONENT_OBJECT_FILES += \
	$(foreach DIR, ${GMOS_TARGET_DISPLAY_DIRS}, $(DISPLAY_BUILD_PATH)/*.o)
endif

# Link all the generated object files. Note that 'shell ls' is used to
# get the list of object files instead of 'wildcard', since the wildcard
# expansion can occur before the timestamp dependencies are met.
${GMOS_BUILD_DIR}/firmware.elf : ${COMPONENT_TIMESTAMPS}
	${LD} -o $@ ${LDFLAGS} \
		$(shell ls -xw0 ${COMPONENT_OBJECT_FILES}) \
		$(addprefix -l, ${LDLIBS})
	${OS} $@

# Include the application source files makefile fragment.
include ${GMOS_APP_DIR}/app-build.mk

# Include the common source files makefile fragment.
include ${GMOS_GIT_DIR}/common/common-build.mk

# Include the platform build makefile fragment. This defines the
# platform specific source build process.
include ${TARGET_PLATFORM_DIR}/platform-build.mk

# Include the network build makefile fragments if required.
ifdef GMOS_TARGET_NETWORK_DIRS
NETWORK_SOURCE_PATH = ${GMOS_GIT_DIR}/network/$(DIR)
include $(foreach DIR, ${GMOS_TARGET_NETWORK_DIRS}, $(NETWORK_SOURCE_PATH)/network-build.mk)
endif

# Include the target radio makefile fragments if specified.
ifdef GMOS_TARGET_RADIO_DIRS
RADIO_SOURCE_PATH = ${GMOS_GIT_DIR}/radios/$(DIR)
include $(foreach DIR, ${GMOS_TARGET_RADIO_DIRS}, $(RADIO_SOURCE_PATH)/radio-build.mk)
endif

# Include the target sensor makefile fragments if specified.
ifdef GMOS_TARGET_SENSOR_DIRS
SENSOR_SOURCE_PATH = ${GMOS_GIT_DIR}/sensors/$(DIR)
include $(foreach DIR, ${GMOS_TARGET_SENSOR_DIRS}, $(SENSOR_SOURCE_PATH)/sensor-build.mk)
endif

# Include the target display makefile fragments if specified.
ifdef GMOS_TARGET_DISPLAY_DIRS
DISPLAY_SOURCE_PATH = ${GMOS_GIT_DIR}/displays/$(DIR)
include $(foreach DIR, ${GMOS_TARGET_DISPLAY_DIRS}, $(DISPLAY_SOURCE_PATH)/display-build.mk)
endif

# Remove all build files.
clean :
	rm -rf ${GMOS_BUILD_DIR}
