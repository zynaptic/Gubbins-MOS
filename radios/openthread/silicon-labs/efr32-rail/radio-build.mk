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
# This is a makefile fragment for building the OpenThread stack from
# source, using the Silicon Labs RAIL support library as the low level
# radio interface.
#

# Specify the OpenThread distribution source code directory to use.
# By default this uses the version of the OpenThread stack shipped with
# the Silicon Labs SDK.
ifndef OPENTHREAD_IMPORT_PATH
OPENTHREAD_IMPORT_PATH = ${GMOS_GECKO_SDK_DIR}/util/third_party/openthread
endif

# Specify the source code directories to use.
OPENTHREAD_COMMON_SRC_DIR = ${GMOS_GIT_DIR}/radios/openthread/common
OPENTHREAD_TARGET_PATH = openthread/silicon-labs/efr32-rail
OPENTHREAD_TARGET_SRC_DIR = ${GMOS_GIT_DIR}/radios/${OPENTHREAD_TARGET_PATH}
OPENTHREAD_EFR32_PLATFORM_DIR = ${GMOS_GECKO_SDK_DIR}/protocol/openthread

# Add the path to the precompiled RAIL library.
LDFLAGS += -L${GMOS_GECKO_SDK_DIR}/platform/radio/rail_lib/autogen/librail_release

# Add the precompiled RAIL library to the link stage.
LDLIBS += rail_efr32xg24_gcc_release

# List all the header directories that are required to build the
# platform independent OpenThread components.
OPENTHREAD_HEADER_DIRS = \
	${GMOS_TARGET_DEVICE_FAMILY_DIR}/Include \
	${GMOS_GECKO_SDK_DIR}/platform/common/inc \
	${GMOS_GECKO_SDK_DIR}/platform/emlib/inc \
	${GMOS_GECKO_SDK_DIR}/platform/CMSIS/Core/Include \
	${OPENTHREAD_EFR32_PLATFORM_DIR}/platform-abstraction/efr32 \
	${OPENTHREAD_EFR32_PLATFORM_DIR}/platform-abstraction/include \
	${GMOS_GECKO_SDK_DIR}/util/third_party/mbedtls/include \
	${GMOS_GECKO_SDK_DIR}/platform/service/device_init/inc \
	${GMOS_GECKO_SDK_DIR}/platform/security/sl_component/se_manager/inc \
	${GMOS_GECKO_SDK_DIR}/platform/security/sl_component/sl_psa_driver/inc \
	${GMOS_GECKO_SDK_DIR}/platform/security/sl_component/sl_mbedtls_support/inc \
	${GMOS_GECKO_SDK_DIR}/platform/security/sl_component/sl_mbedtls_support/config

# List all the header directories that are required to build the
# platform specific OpenThread components.
OPENTHREAD_TARGET_HEADER_DIRS = \
	${GMOS_APP_DIR}/include \
	${GMOS_GIT_DIR}/common/include \
	${TARGET_PLATFORM_DIR}/include \
	${TARGET_PLATFORM_DIR}/vendor/include \
	${OPENTHREAD_COMMON_SRC_DIR}/include \
	${OPENTHREAD_TARGET_SRC_DIR}/include \
	${OPENTHREAD_EFR32_PLATFORM_DIR}/config \
	${OPENTHREAD_IMPORT_PATH}/include \
	${OPENTHREAD_IMPORT_PATH}/src/core \
	${OPENTHREAD_IMPORT_PATH}/examples/platforms \
	${GMOS_GECKO_SDK_DIR}/platform/emdrv/common/inc \
	${GMOS_GECKO_SDK_DIR}/platform/emdrv/nvm3/inc \
	${GMOS_GECKO_SDK_DIR}/platform/service/mpu/inc \
	${GMOS_GECKO_SDK_DIR}/platform/service/sleeptimer/inc \
	${GMOS_GECKO_SDK_DIR}/platform/radio/rail_lib/common \
	${GMOS_GECKO_SDK_DIR}/platform/radio/rail_lib/chip/efr32/efr32xg2x \
	${GMOS_GECKO_SDK_DIR}/platform/radio/rail_lib/protocol/ieee802154 \
	${GMOS_GECKO_SDK_DIR}/platform/radio/rail_lib/plugin/pa-conversions \
	${GMOS_GECKO_SDK_DIR}/platform/security/sl_component/se_manager/inc \
	${GMOS_GECKO_SDK_DIR}/platform/security/sl_component/se_manager/src \
	${GMOS_GECKO_SDK_DIR}/platform/security/sl_component/sl_psa_driver/inc \
	${GMOS_GECKO_SDK_DIR}/platform/security/sl_component/sl_mbedtls_support/inc \
	${GMOS_GECKO_SDK_DIR}/platform/security/sl_component/sl_mbedtls_support/config \
	${GMOS_GECKO_SDK_DIR}/platform/security/sl_component/sl_protocol_crypto/src \
	${GMOS_GECKO_SDK_DIR}/util/plugin/security_manager \
	${GMOS_GECKO_SDK_DIR}/util/silicon_labs/silabs_core/memory_manager

# List all the OpenThread object files that need to be built.
OPENTHREAD_TARGET_OBJ_FILE_NAMES = \
	gmos-openthread-ral.o \
	gmos-openthread-cli.o \
	ot-efr32-system.o \
	ot-efr32-misc.o \
	ot-efr32-alarm.o \
	ot-efr32-entropy.o \
	ot-efr32-crypto.o \
	ot-efr32-radio.o \
	ot-efr32-mac_frame.o \
	ot-efr32-flash.o \
	ot-efr32-soft_source_match_table.o \
	ot-efr32-ieee802154-packet-utils.o \
	sdk-pa_conversions_efr32.o \
	sdk-pa_curves_efr32.o \
	sdk-sli_protocol_crypto_radioaes.o \
	sdk-sli_radioaes_management.o \
	sdk-plugin-security_manager.o

# Specify the local build directory.
LOCAL_DIR = ${GMOS_BUILD_DIR}/radios/${OPENTHREAD_TARGET_PATH}

# There are a lot of common OpenThread components, so these are listed
# with their own build rules in an independent makefile fragment.
include ${OPENTHREAD_COMMON_SRC_DIR}/common-stack.mk

# Add the common OpenThread components to the build requirements.
OPENTHREAD_TARGET_HEADER_DIRS += ${OPENTHREAD_HEADER_DIRS}
OPENTHREAD_TARGET_OBJ_FILE_NAMES += ${OPENTHREAD_OBJ_FILE_NAMES}

# Specify the object files that need to be built.
OPENTHREAD_TARGET_OBJ_FILES = ${addprefix ${LOCAL_DIR}/, ${OPENTHREAD_TARGET_OBJ_FILE_NAMES}}

# Import generated dependency information if available.
-include $(OPENTHREAD_TARGET_OBJ_FILES:.o=.d)

# Run the C compiler on the target specific files.
${LOCAL_DIR}/%.o : ${OPENTHREAD_TARGET_SRC_DIR}/src/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${OTFLAGS} ${addprefix -I, ${OPENTHREAD_TARGET_HEADER_DIRS}} -o $@ $<

# Run the C compiler on the standard EFR32 platform files. Ensure that
# the correct configuration file for the compiled library is included.
${LOCAL_DIR}/ot-efr32-%.o : ${OPENTHREAD_EFR32_PLATFORM_DIR}/platform-abstraction/efr32/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${OTFLAGS} ${addprefix -I, ${OPENTHREAD_TARGET_HEADER_DIRS}} -o $@ $<

# Run the C++ compiler on the standard EFR32 platform files. Ensure that
# the correct configuration file for the compiled library is included.
${LOCAL_DIR}/ot-efr32-%.o : ${OPENTHREAD_EFR32_PLATFORM_DIR}/platform-abstraction/efr32/%.cpp | ${LOCAL_DIR}
	${CC} ${CPPFLAGS} ${OTFLAGS} ${addprefix -I, ${OPENTHREAD_TARGET_HEADER_DIRS}} -o $@ $<

# Run the C++ compiler on the MAC utilities, which are taken from the
# OpenThread platform examples directory.
${LOCAL_DIR}/ot-efr32-%.o : ${OPENTHREAD_IMPORT_PATH}/examples/platforms/utils/%.cpp | ${LOCAL_DIR}
	${CC} ${CPPFLAGS} ${OTFLAGS} ${addprefix -I, ${OPENTHREAD_TARGET_HEADER_DIRS}} -o $@ $<

# Run the C compiler on the additional SDK radio library specific files
# that are required by the OpenThread stack.
${LOCAL_DIR}/sdk-%.o : ${GMOS_GECKO_SDK_DIR}/platform/radio/*/*/*/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${addprefix -I, ${OPENTHREAD_TARGET_HEADER_DIRS}} -o $@ $<

# Run the C compiler on the additional SDK security library specific
# files that are required by the OpenThread stack.
${LOCAL_DIR}/sdk-%.o : ${GMOS_GECKO_SDK_DIR}/platform/security/*/*/src/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} -DMBEDTLS_CONFIG_FILE='"efr32-crypto-config.h"' \
	${addprefix -I, ${OPENTHREAD_TARGET_HEADER_DIRS}} -o $@ $<

# Run the C compiler on the additional SDK utility plugin files that are
# required by the OpenThread stack.
${LOCAL_DIR}/sdk-plugin-%.o : ${GMOS_GECKO_SDK_DIR}/util/plugin/*/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} -DMBEDTLS_CONFIG_FILE='"efr32-crypto-config.h"' \
	${addprefix -I, ${OPENTHREAD_TARGET_HEADER_DIRS}} -o $@ $<

# Timestamp the local build object files.
${LOCAL_DIR}/timestamp : ${OPENTHREAD_TARGET_OBJ_FILES}
	touch $@

# Create the local build directory.
${LOCAL_DIR} :
	mkdir -p $@
