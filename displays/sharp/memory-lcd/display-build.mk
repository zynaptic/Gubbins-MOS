#
# The Gubbins Microcontroller Operating System
#
# Copyright 2023 Zynaptic Limited
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
# This is the makefile fragment for building the Sharp Memory LCD
# display driver.
#

# Specify the source code directories to use.
MEMLCD_DRIVER_PATH = sharp/memory-lcd
MEMLCD_DRIVER_SRC_DIR = ${GMOS_GIT_DIR}/displays/${MEMLCD_DRIVER_PATH}
DISPLAY_COMMON_DIR = ${GMOS_GIT_DIR}/displays/common

# List all the header directories that are required to build the
# display driver code.
MEMLCD_HEADER_DIRS = \
	${GMOS_APP_DIR}/include \
	${GMOS_GIT_DIR}/common/include \
	${TARGET_PLATFORM_DIR}/include \
	${DISPLAY_COMMON_DIR}/include \
	${MEMLCD_DRIVER_SRC_DIR}/include

# List all the application object files that need to be built.
MEMLCD_OBJ_FILE_NAMES = \
	gmos-display-memlcd.o

# Specify the local build directory.
LOCAL_DIR = ${GMOS_BUILD_DIR}/displays/${MEMLCD_DRIVER_PATH}

# Specify the object files that need to be built.
MEMLCD_OBJ_FILES = ${addprefix ${LOCAL_DIR}/, ${MEMLCD_OBJ_FILE_NAMES}}

# Import generated dependency information if available.
-include $(MEMLCD_OBJ_FILES:.o=.d)

# Run the C compiler on the driver specific files.
${LOCAL_DIR}/%.o : ${MEMLCD_DRIVER_SRC_DIR}/src/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${addprefix -I, ${MEMLCD_HEADER_DIRS}} -o $@ $<

# Timestamp the application object files.
${LOCAL_DIR}/timestamp : ${MEMLCD_OBJ_FILES}
	touch $@

# Create the local build directory.
${LOCAL_DIR} :
	mkdir -p $@
