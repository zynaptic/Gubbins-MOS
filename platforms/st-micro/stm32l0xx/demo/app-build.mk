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
# This is the makefile fragment for building the STM32L0XX demo
# application source files for the Gubbins microcontroller operating
# system.
#

# List all the header directories that are required to build the
# application code.
APP_HEADER_DIRS = \
	${GMOS_APP_DIR}/include \
	${GMOS_GIT_DIR}/common/include \
	${TARGET_PLATFORM_DIR}/include \
	${TARGET_PLATFORM_DIR}/vendor/include

# List all the application object files that need to be built.
APP_OBJ_FILE_NAMES = \
	gmos-app-init.o \
	demo-i2c-lm75b.o

# Specify the local build directory.
LOCAL_DIR = ${GMOS_BUILD_DIR}/app

# Specify the object files that need to be built.
APP_OBJ_FILES = ${addprefix ${LOCAL_DIR}/, ${APP_OBJ_FILE_NAMES}}

# Import generated dependency information if available.
-include $(APP_OBJ_FILES:.o=.d)

# Run the C compiler with the standard options.
${LOCAL_DIR}/%.o : ${GMOS_APP_DIR}/src/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${addprefix -I, ${APP_HEADER_DIRS}} -o $@ $<

# Timestamp the application object files.
${LOCAL_DIR}/timestamp : ${APP_OBJ_FILES}
	touch $@

# Create the local build directory.
${LOCAL_DIR} :
	mkdir -p $@
