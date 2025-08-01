#
# The Gubbins Microcontroller Operating System
#
# Copyright 2024-2025 Zynaptic Limited
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
# This is a makefile fragment for building the portable GubbinsMOS
# Zigbee stack, using the Silicon Labs EmberZNet and RAIL support
# libraries as the low level radio interface.
#

# Specify the source code directories to use.
ZIGBEE_COMMON_SRC_DIR = ${GMOS_GIT_DIR}/radios/zigbee/common
ZIGBEE_COMMON_TEST_DIR = ${GMOS_GIT_DIR}/radios/zigbee/test
ZIGBEE_TARGET_PATH = zigbee/silicon-labs/efr32-rail
ZIGBEE_TARGET_SRC_DIR = ${GMOS_GIT_DIR}/radios/${ZIGBEE_TARGET_PATH}

# Add the precompiled Zigbee libraries to the link stage.
TSLIBS = \
	zigbee-pro-stack \
	zigbee-source-route

# Select the target specific RAIL libraries for EFR32MG24 SoC devices.
# Custom PA curve configurations need to be compiled for the standalone
# devices.
ifeq (${GMOS_TARGET_DEVICE_FAMILY}, EFR32MG24)
TSLIBS += rail_efr32xg24_gcc_release
TSFILES = sdk-pa_curves_efr32.o
endif

# Select the target specific RAIL libraries for MGM24 modules. Standard
# FCC/CA compliant PA curve configurations need to be used from the SDK
# libraries.
ifeq (${GMOS_TARGET_DEVICE_FAMILY}, MGM24)
TSLIBS += \
	rail_module_efr32xg24_gcc_release \
	rail_config_${GMOS_TARGET_DEVICE_VARIANT_LC}_gcc
endif

# Add the precompiled RAIL libraries to the link stage.
LDLIBS += ${TSLIBS}

# Add the path to the precompiled RAIL libraries.
LDFLAGS += -L${GMOS_BUILD_DIR}/radios/${ZIGBEE_TARGET_PATH}/lib

# List all the header directories that are required to build the
# platform specific Zigbee components.
ZIGBEE_TARGET_HEADER_DIRS = \
	${GMOS_TARGET_DEVICE_FAMILY_DIR}/Include \
	${GMOS_SIMPLICITY_SDK_DIR}/platform/common/inc \
	${GMOS_SIMPLICITY_SDK_DIR}/platform/emlib/inc \
	${GMOS_SIMPLICITY_SDK_DIR}/platform/CMSIS/Core/Include \
	${GMOS_SIMPLICITY_SDK_DIR}/platform/emdrv/common/inc \
	${GMOS_SIMPLICITY_SDK_DIR}/platform/emdrv/nvm3/inc \
	${GMOS_SIMPLICITY_SDK_DIR}/platform/driver/gpio/inc \
	${GMOS_SIMPLICITY_SDK_DIR}/platform/radio/mac \
	${GMOS_SIMPLICITY_SDK_DIR}/platform/radio/mac/config \
	${GMOS_SIMPLICITY_SDK_DIR}/platform/radio/rail_lib/common \
	${GMOS_SIMPLICITY_SDK_DIR}/platform/radio/rail_lib/chip/efr32/efr32xg2x \
	${GMOS_SIMPLICITY_SDK_DIR}/platform/radio/rail_lib/protocol/ieee802154 \
	${GMOS_SIMPLICITY_SDK_DIR}/platform/radio/rail_lib/plugin \
	${GMOS_SIMPLICITY_SDK_DIR}/platform/radio/rail_lib/plugin/pa-conversions \
	${GMOS_SIMPLICITY_SDK_DIR}/platform/radio/rail_lib/plugin/rail_util_pti \
	${GMOS_SIMPLICITY_SDK_DIR}/platform/radio/rail_lib/plugin/rail_util_ieee802154 \
	${GMOS_SIMPLICITY_SDK_DIR}/platform/service/sleeptimer/inc \
	${GMOS_SIMPLICITY_SDK_DIR}/platform/service/device_manager/inc \
	${GMOS_SIMPLICITY_SDK_DIR}/platform/service/power_manager/inc \
	${GMOS_SIMPLICITY_SDK_DIR}/platform/service/power_manager/config \
	${GMOS_SIMPLICITY_SDK_DIR}/platform/service/legacy_hal/inc \
	${GMOS_SIMPLICITY_SDK_DIR}/platform/service/token_manager/inc \
	${GMOS_SIMPLICITY_SDK_DIR}/platform/service/token_manager/config \
	${GMOS_SIMPLICITY_SDK_DIR}/protocol/zigbee \
	${GMOS_SIMPLICITY_SDK_DIR}/protocol/zigbee/stack \
	${GMOS_SIMPLICITY_SDK_DIR}/protocol/zigbee/stack/include \
	${GMOS_SIMPLICITY_SDK_DIR}/platform/security/sl_component/se_manager/inc \
	${GMOS_SIMPLICITY_SDK_DIR}/platform/security/sl_component/sl_psa_driver/inc \
	${GMOS_SIMPLICITY_SDK_DIR}/platform/security/sl_component/sl_mbedtls_support/inc \
	${GMOS_SIMPLICITY_SDK_DIR}/platform/security/sl_component/sl_mbedtls_support/config \
	${GMOS_SIMPLICITY_SDK_DIR}/util/plugin/security_manager \
	${GMOS_SIMPLICITY_SDK_DIR}/util/plugin/byte_utilities \
	${GMOS_SIMPLICITY_SDK_DIR}/util/silicon_labs/silabs_core \
	${GMOS_SIMPLICITY_SDK_DIR}/util/third_party/mbedtls/include \
	${GMOS_SIMPLICITY_SDK_DIR}/util/third_party/mbedtls/library \

# List all the Zigbee stack files that need to be built.
ZIGBEE_TARGET_OBJ_FILE_NAMES = \
	gmos-zigbee-ember-ral.o \
	gmos-zigbee-ember-utils.o \
	gmos-zigbee-ember-stubs.o \
	gmos-zigbee-ember-startup.o \
	gmos-zigbee-ember-active.o \
	gmos-zigbee-ember-form-network.o \
	gmos-zigbee-ember-join-network.o \
	gmos-zigbee-ember-coordinator.o \
	gmos-zigbee-ember-concentrator.o \
	gmos-zigbee-ember-aps-device.o \
	em-hal-base-replacement.o \
	em-hal-token_legacy.o \
	em-hal-random.o \
	em-hal-crc.o \
	em-hal-diagnostic.o \
	em-hal-ember-phy.o \
	em-stack-sl_zigbee_configuration.o \
	em-stack-sl_zigbee_configuration_access.o \
	em-stack-strong-random-api.o \
	em-stack-zigbee-security-manager.o \
	em-stack-zigbee-security-manager-vault-support.o \
	em-stack-zigbee-event-logger-stub-gen.o \
	em-stack-multi-pan-stub.o \
	em-stack-gp-stub.o \
	em-stack-aps-keys-full-stub.o \
	em-stack-cbke-crypto-engine-stub.o \
	em-stack-cbke-crypto-engine-163k1-stub.o \
	em-stack-cbke-crypto-engine-283k1-stub.o \
	em-stack-cbke-crypto-engine-dsa-sign-stub.o \
	em-stack-cbke-crypto-engine-dsa-verify-stub.o \
	em-stack-cbke-crypto-engine-dsa-verify-283k1-stub.o \
	em-stack-enhanced-beacon-request-stub.o \
	em-stack-zdo-r22-stub.o \
	em-stack-sl_zigbee_endpoint_stubs.o \
	em-stack-sl_zigbee_callback_stubs.o \
	em-stack-sl_zigbee_multi_network_stub.o \
	em-stack-sl_zigbee_r23_misc_support_stubs.o \
	em-stack-sl_zigbee_dynamic_commissioning_stubs.o \
	em-stack-sli_zigbee_zdo_cluster_filter_stubs.o \
	em-stack-mac-info-element-parsing-stub.o \
	em-stack-zll-stubs.o \
	em-stack-sl_token_def.o \
	em-stack-sl_token_manager.o \
	em-stack-sl_token_manufacturing.o \
	em-stack-message_baremetal_wrapper.o \
	em-stack-message_baremetal_callbacks.o \
	em-stack-raw-message-baremetal-callbacks.o \
	em-stack-bootload_baremetal_callbacks.o \
	em-stack-child_baremetal_callbacks.o \
	em-stack-stack-info-baremetal-wrapper.o \
	em-stack-stack-info-baremetal-callbacks.o \
	em-stack-trust-center-baremetal-wrapper.o \
	em-stack-trust-center-baremetal-callbacks.o \
	em-stack-security_baremetal_wrapper.o \
	em-stack-security_baremetal_callbacks.o \
	em-stack-sl_zigbee_random_api_baremetal_wrapper.o \
	em-stack-zigbee-security-manager-baremetal-wrapper.o \
	em-stack-network-formation-baremetal-wrapper.o \
	em-stack-network-formation-baremetal-callbacks.o \
	em-stack-security-address-cache.o \
	sdk-plugin-byte-utilities.o \
	sdk-plugin-security_manager.o \
	sdk-sl_rail_util_ant_div.o \
	sdk-sl_rail_util_pti.o \
	sdk-pa_conversions_efr32.o \
	sdk-coexistence-802154.o \
	${TSFILES}

#	em-stack-source-route-stub.o \

# Specify additional target specific compiler options.
TSFLAGS = \
	-DCONFIGURATION_HEADER='"${ZIGBEE_TARGET_SRC_DIR}/include/sl_zigbee_stack_config.h"' \
	-DPLATFORM_HEADER='"platform-header.h"'

# Specify additional SDK specific compiler options.
SDKFLAGS = \
	${TSFLAGS} \
	-DUSE_NVM3 \
	-DSTACK_TYPES_HEADER='"sl_zigbee_types.h"' \
	-DMBEDTLS_CONFIG_FILE='"efr32-crypto-config.h"' \
	-Wno-unused-parameter \
	-Wno-missing-field-initializers

# Specify the local build directory.
LOCAL_DIR = ${GMOS_BUILD_DIR}/radios/${ZIGBEE_TARGET_PATH}

# There are various common Zigbee components, so these are listed
# with their own build rules in an independent makefile fragment.
include ${ZIGBEE_COMMON_SRC_DIR}/common-stack.mk

# There are various common Zigbee test components, so these are listed
# with their own build rules in an independent makefile fragment.
ifdef GMOS_ZIGBEE_TEST_FRAMEWORK_ENABLE
include ${ZIGBEE_COMMON_TEST_DIR}/common-test.mk
endif

# Add the common Zigbee components to the build requirements.
ZIGBEE_TARGET_HEADER_DIRS += ${ZIGBEE_HEADER_DIRS}
ZIGBEE_TARGET_OBJ_FILE_NAMES += ${ZIGBEE_OBJ_FILE_NAMES}

# Specify the object files that need to be built.
ZIGBEE_TARGET_OBJ_FILES = ${addprefix ${LOCAL_DIR}/, ${ZIGBEE_TARGET_OBJ_FILE_NAMES}}

# Specify the library files that need to be copied.
ZIGBEE_TARGET_LIB_FILES = ${addsuffix .a, ${addprefix ${LOCAL_DIR}/lib/lib, ${TSLIBS}}}

# Import generated dependency information if available.
-include $(ZIGBEE_TARGET_OBJ_FILES:.o=.d)

# Run the C compiler on the target specific files.
${LOCAL_DIR}/%.o : ${ZIGBEE_TARGET_SRC_DIR}/src/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${TSFLAGS} ${addprefix -I, ${ZIGBEE_TARGET_HEADER_DIRS}} -o $@ $<

# Run the C compiler on the vendor Zigbee stack files.
${LOCAL_DIR}/em-hal-%.o : ${GMOS_SIMPLICITY_SDK_DIR}/platform/service/legacy_hal/src/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${SDKFLAGS} ${addprefix -I, ${ZIGBEE_TARGET_HEADER_DIRS}} -o $@ $<
${LOCAL_DIR}/em-stack-%.o : ${GMOS_SIMPLICITY_SDK_DIR}/protocol/zigbee/stack/*/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${SDKFLAGS} ${addprefix -I, ${ZIGBEE_TARGET_HEADER_DIRS}} -o $@ $<
${LOCAL_DIR}/em-stack-%.o : ${GMOS_SIMPLICITY_SDK_DIR}/protocol/zigbee/stack/*/*/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${SDKFLAGS} ${addprefix -I, ${ZIGBEE_TARGET_HEADER_DIRS}} -o $@ $<
${LOCAL_DIR}/em-stack-%.o : ${GMOS_SIMPLICITY_SDK_DIR}/protocol/zigbee/stack/*/*/*/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${SDKFLAGS} ${addprefix -I, ${ZIGBEE_TARGET_HEADER_DIRS}} -o $@ $<
${LOCAL_DIR}/em-stack-%.o : ${GMOS_SIMPLICITY_SDK_DIR}/platform/service/token_manager/src/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${SDKFLAGS} ${addprefix -I, ${ZIGBEE_TARGET_HEADER_DIRS}} -o $@ $<

# Run the C compiler on the vendor Zigbee application utility files.
${LOCAL_DIR}/em-util-%.o : ${GMOS_SIMPLICITY_SDK_DIR}/protocol/zigbee/app/util/*/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${SDKFLAGS} ${addprefix -I, ${ZIGBEE_TARGET_HEADER_DIRS}} -o $@ $<

# Run the C compiler on the additional SDK utility plugin files that are
# required by the Zigbee stack.
${LOCAL_DIR}/sdk-plugin-%.o : ${GMOS_SIMPLICITY_SDK_DIR}/util/plugin/*/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${SDKFLAGS} ${addprefix -I, ${ZIGBEE_TARGET_HEADER_DIRS}} -o $@ $<

# Run the C compiler on the additional SDK rail plugin files that are
# required by the Zigbee stack.
${LOCAL_DIR}/sdk-%.o : ${GMOS_SIMPLICITY_SDK_DIR}/platform/radio/rail_lib/plugin/*/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${SDKFLAGS} ${addprefix -I, ${ZIGBEE_TARGET_HEADER_DIRS}} -o $@ $<
${LOCAL_DIR}/sdk-%.o : ${GMOS_SIMPLICITY_SDK_DIR}/platform/radio/rail_lib/plugin/*/*/*/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${SDKFLAGS} ${addprefix -I, ${ZIGBEE_TARGET_HEADER_DIRS}} -o $@ $<

# Copy the required target libraries to the build directory.
${LOCAL_DIR}/lib/lib%.a : ${GMOS_SIMPLICITY_SDK_DIR}/protocol/zigbee/build/gcc/cortex-m33/*/release_singlenetwork/lib%.a | ${LOCAL_DIR}/lib
	cp $< $@
${LOCAL_DIR}/lib/lib%.a : ${GMOS_SIMPLICITY_SDK_DIR}/platform/radio/rail_lib/autogen/librail_release/lib%.a | ${LOCAL_DIR}/lib
	cp $< $@

# Timestamp the Zigbee stack object files.
${LOCAL_DIR}/timestamp : ${ZIGBEE_TARGET_OBJ_FILES} ${ZIGBEE_TARGET_LIB_FILES}
	touch $@

# Create the local build directories.
${LOCAL_DIR} :
	mkdir -p $@
${LOCAL_DIR}/lib :
	mkdir -p $@
