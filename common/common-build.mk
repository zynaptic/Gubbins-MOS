#
# The Gubbins Microcontroller Operating System
#
# Copyright 2020-2024 Zynaptic Limited
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
# This is the makefile fragment for building the common source files
# for the Gubbins microcontroller operating system.
#

# List all the header directories that are required to build the
# common source code.
COMMON_HEADER_DIRS = \
	${GMOS_APP_DIR}/include \
	${TARGET_PLATFORM_DIR}/include \
	${GMOS_GIT_DIR}/common/include

# List all the common object files that need to be built.
COMMON_OBJ_FILE_NAMES = \
	gmos-scheduler.o \
	gmos-mempool.o \
	gmos-random.o \
	gmos-streams.o \
	gmos-buffers.o \
	gmos-events.o \
	gmos-format-cbor-enc.o \
	gmos-format-cbor-dec.o \
	gmos-driver-iic.o \
	gmos-driver-spi.o \
	gmos-driver-rtc.o \
	gmos-driver-rtc-sw.o \
	gmos-driver-lcd.o \
	gmos-driver-touch.o \
	gmos-driver-flash.o \
	gmos-driver-flash-sfdp.o \
	gmos-driver-eeprom.o \
	gmos-driver-eeprom-sw.o

# Specify the local build directory.
LOCAL_DIR = ${GMOS_BUILD_DIR}/common

# Specify the object files that need to be built.
COMMON_OBJ_FILES = ${addprefix ${LOCAL_DIR}/, ${COMMON_OBJ_FILE_NAMES}}

# Import generated dependency information if available.
-include $(COMMON_OBJ_FILES:.o=.d)

# Run the C compiler with the standard options.
${LOCAL_DIR}/%.o : ${GMOS_GIT_DIR}/common/src/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${addprefix -I, ${COMMON_HEADER_DIRS}} -o $@ $<

# Timestamp the common object files.
${LOCAL_DIR}/timestamp : ${COMMON_OBJ_FILES}
	touch $@

# Create the local build directory.
${LOCAL_DIR} :
	mkdir -p $@
