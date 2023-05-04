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
# This is the makefile fragment for building the Silicon Labs Si702x
# hygrometer and temperature sensor driver.
#

# Specify the source code directories to use.
SI702X_DRIVER_PATH = silicon-labs/si702x
SI702X_DRIVER_SRC_DIR = ${GMOS_GIT_DIR}/sensors/${SI702X_DRIVER_PATH}

# List all the header directories that are required to build the
# sensor driver code.
SI702X_HEADER_DIRS = \
	${GMOS_APP_DIR}/include \
	${GMOS_GIT_DIR}/common/include \
	${GMOS_GIT_DIR}/sensors/common/include \
	${TARGET_PLATFORM_DIR}/include \
	${SI702X_DRIVER_SRC_DIR}/include

# List all the application object files that need to be built.
SI702X_OBJ_FILE_NAMES = \
	gmos-sensor-si702x.o \

# Specify the local build directory.
LOCAL_DIR = ${GMOS_BUILD_DIR}/sensors/${SI702X_DRIVER_PATH}

# Specify the object files that need to be built.
SI702X_OBJ_FILES = ${addprefix ${LOCAL_DIR}/, ${SI702X_OBJ_FILE_NAMES}}

# Import generated dependency information if available.
-include $(SI702X_OBJ_FILES:.o=.d)

# Run the C compiler on the driver specific files.
${LOCAL_DIR}/%.o : ${SI702X_DRIVER_SRC_DIR}/src/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${addprefix -I, ${SI702X_HEADER_DIRS}} -o $@ $<

# Timestamp the application object files.
${LOCAL_DIR}/timestamp : ${SI702X_OBJ_FILES}
	touch $@

# Create the local build directory.
${LOCAL_DIR} :
	mkdir -p $@
