#
# The Gubbins Microcontroller Operating System
#
# Copyright 2026 Zynaptic Limited
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
# This is the makefile fragment for building the driver files needed to
# use the ST25DVXX series of dynamic NFC tags.
#

# Specify the source code directories to use.
ST25DVXX_DRIVER_PATH = nfctag/st-micro/st25dvxx
ST25DVXX_DRIVER_SRC_DIR = ${GMOS_GIT_DIR}/radios/${ST25DVXX_DRIVER_PATH}
NFCTAG_COMMON_DRIVER_SRC_DIR = ${GMOS_GIT_DIR}/radios/nfctag/common

# List all the header directories that are required to build the
# NFC tag driver code.
ST25DVXX_HEADER_DIRS = \
	${GMOS_APP_DIR}/include \
	${GMOS_GIT_DIR}/common/include \
	${TARGET_PLATFORM_DIR}/include \
	${NFCTAG_COMMON_DRIVER_SRC_DIR}/include \
	${ST25DVXX_DRIVER_SRC_DIR}/include

# List all the object files that need to be built.
ST25DVXX_OBJ_FILE_NAMES = \
	gmos-driver-nfctag.o \
	gmos-driver-nfctag-type5.o \
	gmos-driver-nfctag-ndef-enc.o \
	gmos-driver-nfctag-ndef-dec.o \
	st25dv-driver-nfctag.o \

# Specify the local build directory.
LOCAL_DIR = ${GMOS_BUILD_DIR}/radios/${ST25DVXX_DRIVER_PATH}

# Specify the object files that need to be built.
ST25DVXX_OBJ_FILES = ${addprefix ${LOCAL_DIR}/, ${ST25DVXX_OBJ_FILE_NAMES}}

# Import generated dependency information if available.
-include $(ST25DVXX_OBJ_FILES:.o=.d)

# Run the C compiler on the driver specific files.
${LOCAL_DIR}/%.o : ${ST25DVXX_DRIVER_SRC_DIR}/src/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${addprefix -I, ${ST25DVXX_HEADER_DIRS}} -o $@ $<

# Run the C compiler on the GubbinsMOS common source files.
${LOCAL_DIR}/%.o : ${NFCTAG_COMMON_DRIVER_SRC_DIR}/src/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${addprefix -I, ${ST25DVXX_HEADER_DIRS}} -o $@ $<

# Timestamp the driver object files.
${LOCAL_DIR}/timestamp : ${ST25DVXX_OBJ_FILES}
	touch $@

# Create the local build directory.
${LOCAL_DIR} :
	mkdir -p $@
