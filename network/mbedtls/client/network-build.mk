#
# The Gubbins Microcontroller Operating System
#
# Copyright 2025 Zynaptic Limited
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
# This is the makefile fragment for building the platform independent
# TLS client support using MbedTLS. It does not build the MbedTLS
# library itself, since this will either be compiled as part of the
# platform component from the silicon vendors's SDK or as a 'standalone'
# build using the standard MbedTLS distribution.
#

# Specify the source code directories to use.
MBEDTLS_CLIENT_PATH = mbedtls/client
MBEDTLS_CLIENT_SRC_DIR = ${GMOS_GIT_DIR}/network/${MBEDTLS_CLIENT_PATH}

# List all the header directories that are required to build the
# MbedTLS client code.
MBEDTLS_CLIENT_HEADER_DIRS = \
	${GMOS_APP_DIR}/include \
	${GMOS_GIT_DIR}/common/include \
	${GMOS_GIT_DIR}/network/common/include \
	${TARGET_PLATFORM_DIR}/include \
	${TARGET_PLATFORM_DIR}/vendor/include \
	${MBEDTLS_CLIENT_SRC_DIR}/include

# Add the PSA API headers location.
# TODO: Add support for standalone build.
ifdef GMOS_PLATFORM_PSA_CRYPTO_API_DIRS
MBEDTLS_CLIENT_HEADER_DIRS += ${GMOS_PLATFORM_PSA_CRYPTO_API_DIRS}
endif

# Add the MbedTLS API headers location.
# TODO: Add support for standalone build.
ifdef GMOS_PLATFORM_MBEDTLS_API_DIRS
MBEDTLS_CLIENT_HEADER_DIRS += ${GMOS_PLATFORM_MBEDTLS_API_DIRS}
endif

# List all the application object files that need to be built.
MBEDTLS_CLIENT_OBJ_FILE_NAMES = \
	gmos-mbedtls-client.o \
	gmos-mbedtls-config.o \
	gmos-mbedtls-support.o \
	gmos-mbedtls-certs.o

# Specify the local build directory.
LOCAL_DIR = ${GMOS_BUILD_DIR}/network/${MBEDTLS_CLIENT_PATH}

# Specify the object files that need to be built.
MBEDTLS_CLIENT_OBJ_FILES = ${addprefix ${LOCAL_DIR}/, ${MBEDTLS_CLIENT_OBJ_FILE_NAMES}}

# Import generated dependency information if available.
-include $(MBEDTLS_CLIENT_OBJ_FILES:.o=.d)

# Run the C compiler on the MbedTLS client source files.
${LOCAL_DIR}/%.o : ${MBEDTLS_CLIENT_SRC_DIR}/src/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} -DMBEDTLS_CONFIG_FILE='"${GMOS_PLATFORM_MBEDTLS_CONFIG_FILE}"' \
	${addprefix -I, ${MBEDTLS_CLIENT_HEADER_DIRS}} -o $@ $<

# Timestamp the application object files.
${LOCAL_DIR}/timestamp : ${MBEDTLS_CLIENT_OBJ_FILES}
	touch $@

# Create the local build directory.
${LOCAL_DIR} :
	mkdir -p $@
