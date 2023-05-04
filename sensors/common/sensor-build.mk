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
# This is the makefile fragment for building the common GubbinsMOS
# sensor library components.
#

# Specify the source code directories to use.
SENSOR_COMMON_DIR = ${GMOS_GIT_DIR}/sensors/common

# List all the header directories that are required to build the
# common sensor library code.
SENSOR_HEADER_DIRS = \
	${GMOS_APP_DIR}/include \
	${GMOS_GIT_DIR}/common/include \
	${TARGET_PLATFORM_DIR}/include \
	${SENSOR_COMMON_DIR}/include

# List all the application object files that need to be built.
SENSOR_OBJ_FILE_NAMES = \
	gmos-sensor-feeds.o

# Specify the local build directory.
LOCAL_DIR = ${GMOS_BUILD_DIR}/sensors/common

# Specify the object files that need to be built.
SENSOR_OBJ_FILES = ${addprefix ${LOCAL_DIR}/, ${SENSOR_OBJ_FILE_NAMES}}

# Import generated dependency information if available.
-include $(SENSOR_OBJ_FILES:.o=.d)

# Run the C compiler on the common sensor files.
${LOCAL_DIR}/%.o : ${SENSOR_COMMON_DIR}/src/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${addprefix -I, ${SENSOR_HEADER_DIRS}} -o $@ $<

# Timestamp the sensor library object files.
${LOCAL_DIR}/timestamp : ${SENSOR_OBJ_FILES}
	touch $@

# Create the local build directory.
${LOCAL_DIR} :
	mkdir -p $@
