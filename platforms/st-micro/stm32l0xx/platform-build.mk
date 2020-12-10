#
# The Gubbins Microcontroller Operating System
#
# Copyright 2020 Zynaptic Limited
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
# implied. See the License for the specific language governing
# permissions and limitations under the License.
#

#
# This is the makefile fragment for building the platform specific
# source files for the ST STM32L0XX range of target devices.
#

# List all the header directories that are required to build the
# platform source code.
PLATFORM_HEADER_DIRS = \
	${GMOS_APP_DIR}/include \
	${GMOS_GIT_DIR}/common/include \
	${GMOS_GIT_DIR}/imports/printf \
	${TARGET_PLATFORM_DIR}/include \
	${TARGET_PLATFORM_DIR}/vendor/include

# List all the platform object files that need to be built.
PLATFORM_OBJ_FILE_NAMES = \
	vectors.o \
	startup.o \
	printf.o \
	gmos-platform.o \
	stm32-device.o \
	stm32-console-dma.o \
	stm32-console-simple.o \
	stm32-timer.o

# Specify the local build directory.
LOCAL_DIR = ${GMOS_BUILD_DIR}/platform

# Specify the object files that need to be built.
PLATFORM_OBJ_FILES = ${addprefix ${LOCAL_DIR}/, ${PLATFORM_OBJ_FILE_NAMES}}

# Import generated dependency information if available.
-include $(PLATFORM_OBJ_FILES:.o=.d)

# Generate the linker script for the specified target device.
${LOCAL_DIR}/target.ld : ${TARGET_PLATFORM_DIR}/tools/linker-script-gen.py | ${LOCAL_DIR}
	python3 $< --device ${TARGET_DEVICE} --output $@

# Run the assembler with the standard options.
${LOCAL_DIR}/%.o : ${TARGET_PLATFORM_DIR}/src/%.s | ${LOCAL_DIR}
	${AS} -x assembler-with-cpp ${ASFLAGS} $< -o $@

# Run the C compiler with the standard options.
${LOCAL_DIR}/%.o : ${TARGET_PLATFORM_DIR}/src/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${addprefix -I, ${PLATFORM_HEADER_DIRS}} -o $@ $<

# Run the C compiler for the selected standard library support files.
${LOCAL_DIR}/%.o : ${GMOS_GIT_DIR}/imports/*/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} -DPRINTF_INCLUDE_CONFIG_H \
		${addprefix -I, ${PLATFORM_HEADER_DIRS}} -o $@ $<

# Timestamp the target platform object files.
${LOCAL_DIR}/timestamp : ${PLATFORM_OBJ_FILES} ${LOCAL_DIR}/target.ld
	touch $@

# Create the local build directory.
${LOCAL_DIR} :
	mkdir -p $@
