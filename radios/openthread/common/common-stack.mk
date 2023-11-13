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
# This is the makefile fragment for building the common components of
# the OpenThread stack.
#

# List all the header directories that are required to build the common
# OpenThread components.
OPENTHREAD_HEADER_DIRS += \
	${GMOS_APP_DIR}/include \
	${GMOS_GIT_DIR}/common/include \
	${TARGET_PLATFORM_DIR}/include \
	${TARGET_PLATFORM_DIR}/vendor/include \
	${OPENTHREAD_COMMON_SRC_DIR}/include \
	${OPENTHREAD_TARGET_SRC_DIR}/include \
	${OPENTHREAD_IMPORT_PATH}/include \
	${OPENTHREAD_IMPORT_PATH}/src \
	${OPENTHREAD_IMPORT_PATH}/src/core

# List all the common component object files that need to be built.
OPENTHREAD_OBJ_FILE_NAMES = \
	gmos-openthread.o \
	gmos-openthread-join.o \
	gmos-openthread-resdir.o \
	gmos-openthread-sntp.o

# Add all the OpenThread API components to the build.
OPENTHREAD_API_FILE_NAMES = \
	$(shell cd ${OPENTHREAD_IMPORT_PATH}/src/core/api; ls *.cpp)
OPENTHREAD_OBJ_FILE_NAMES += \
	$(addprefix ot_core_api_, $(OPENTHREAD_API_FILE_NAMES:.cpp=.o))

# Add the OpenThread common components to the build. Exclude the
# extention example file which really shouldn't be in this directory.
OPENTHREAD_COMMON_FILE_NAMES = \
	$(shell cd ${OPENTHREAD_IMPORT_PATH}/src/core/common; ls *.cpp | grep -v ^extension_example.cpp)
OPENTHREAD_OBJ_FILE_NAMES += \
	$(addprefix ot_core_common_, $(OPENTHREAD_COMMON_FILE_NAMES:.cpp=.o))

# Add the OpenThread utility components to the build.
OPENTHREAD_UTILITY_FILE_NAMES = \
	$(shell cd ${OPENTHREAD_IMPORT_PATH}/src/core/utils; ls *.cpp)
OPENTHREAD_OBJ_FILE_NAMES += \
	$(addprefix ot_core_utils_, $(OPENTHREAD_UTILITY_FILE_NAMES:.cpp=.o))

# Add the OpenThread CLI support to the build.
OPENTHREAD_CLI_FILE_NAMES = \
	$(shell cd ${OPENTHREAD_IMPORT_PATH}/src/cli; ls *.cpp)
OPENTHREAD_OBJ_FILE_NAMES += \
	$(addprefix ot_cli_, $(OPENTHREAD_CLI_FILE_NAMES:.cpp=.o))

# Add all the OpenThread radio layer components to the build.
OPENTHREAD_RADIO_FILE_NAMES = \
	$(shell cd ${OPENTHREAD_IMPORT_PATH}/src/core/radio; ls *.cpp)
OPENTHREAD_OBJ_FILE_NAMES += \
	$(addprefix ot_core_radio_, $(OPENTHREAD_RADIO_FILE_NAMES:.cpp=.o))

# Add all the OpenThread MAC layer components to the build.
OPENTHREAD_MAC_FILE_NAMES = \
	$(shell cd ${OPENTHREAD_IMPORT_PATH}/src/core/mac; ls *.cpp)
OPENTHREAD_OBJ_FILE_NAMES += \
	$(addprefix ot_core_mac_, $(OPENTHREAD_MAC_FILE_NAMES:.cpp=.o))

# Add all the OpenThread IP network layer components to the build.
OPENTHREAD_NET_FILE_NAMES = \
	$(shell cd ${OPENTHREAD_IMPORT_PATH}/src/core/net; ls *.cpp)
OPENTHREAD_OBJ_FILE_NAMES += \
	$(addprefix ot_core_net_, $(OPENTHREAD_NET_FILE_NAMES:.cpp=.o))

# Add all the mesh network managment components to the build.
OPENTHREAD_MESHCOP_FILE_NAMES = \
	$(shell cd ${OPENTHREAD_IMPORT_PATH}/src/core/meshcop; ls *.cpp)
OPENTHREAD_OBJ_FILE_NAMES += \
	$(addprefix ot_core_meshcop_, $(OPENTHREAD_MESHCOP_FILE_NAMES:.cpp=.o))

# Add all the cryptography library components to the build.
OPENTHREAD_CRYPTO_FILE_NAMES = \
	$(shell cd ${OPENTHREAD_IMPORT_PATH}/src/core/crypto; ls *.cpp)
OPENTHREAD_OBJ_FILE_NAMES += \
	$(addprefix ot_core_crypto_, $(OPENTHREAD_CRYPTO_FILE_NAMES:.cpp=.o))

# Add all the CoAP protocol components to the build.
OPENTHREAD_COAP_FILE_NAMES = \
	$(shell cd ${OPENTHREAD_IMPORT_PATH}/src/core/coap; ls *.cpp)
OPENTHREAD_OBJ_FILE_NAMES += \
	$(addprefix ot_core_coap_, $(OPENTHREAD_COAP_FILE_NAMES:.cpp=.o))

# Add all the thread protocol components to the build.
OPENTHREAD_THREAD_FILE_NAMES = \
	$(shell cd ${OPENTHREAD_IMPORT_PATH}/src/core/thread; ls *.cpp)
OPENTHREAD_OBJ_FILE_NAMES += \
	$(addprefix ot_core_thread_, $(OPENTHREAD_THREAD_FILE_NAMES:.cpp=.o))

# Add all the backbone router components to the build.
OPENTHREAD_BACKBONE_ROUTER_FILE_NAMES = \
	$(shell cd ${OPENTHREAD_IMPORT_PATH}/src/core/backbone_router; ls *.cpp)
OPENTHREAD_OBJ_FILE_NAMES += \
	$(addprefix ot_core_backbone_router_, $(OPENTHREAD_BACKBONE_ROUTER_FILE_NAMES:.cpp=.o))

# Add all the third party TCP/IP libraries to be built.
# This still appears to be in development, so TCP is not normally used.
ifdef OPENTHREAD_USE_EXPERIMENTAL_TCP
OPENTHREAD_EXTRA_TCPIP_LIB_FILE_NAMES = \
	$(shell cd ${OPENTHREAD_IMPORT_PATH}/third_party/tcplp/lib; ls *.c)
OPENTHREAD_OBJ_FILE_NAMES += \
	$(addprefix ot_third_party_tcpip_, $(OPENTHREAD_EXTRA_TCPIP_LIB_FILE_NAMES:.c=.o))
OPENTHREAD_EXTRA_TCPIP_BSDTCP_FILE_NAMES = \
	$(shell cd ${OPENTHREAD_IMPORT_PATH}/third_party/tcplp/bsdtcp; ls *.c)
OPENTHREAD_OBJ_FILE_NAMES += \
	$(addprefix ot_third_party_tcpip_, $(OPENTHREAD_EXTRA_TCPIP_BSDTCP_FILE_NAMES:.c=.o))
OPENTHREAD_EXTRA_TCPIP_BSDTCP_CC_FILE_NAMES = \
	$(shell cd ${OPENTHREAD_IMPORT_PATH}/third_party/tcplp/bsdtcp/cc; ls *.c)
OPENTHREAD_OBJ_FILE_NAMES += \
	$(addprefix ot_third_party_tcpip_, $(OPENTHREAD_EXTRA_TCPIP_BSDTCP_CC_FILE_NAMES:.c=.o))
endif

# Specify additional OpenThread compiler options.
OTFLAGS = -DOPENTHREAD_CONFIG_CORE_USER_CONFIG_HEADER_ENABLE \
	-DMBEDTLS_CONFIG_FILE='"efr32-crypto-config.h"'

# Run the C compiler on the GubbinsMOS common source files.
${LOCAL_DIR}/%.o : ${OPENTHREAD_COMMON_SRC_DIR}/src/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${OTFLAGS} ${addprefix -I, ${OPENTHREAD_HEADER_DIRS}} -o $@ $<

# Build rule for core OpenThread API components.
${LOCAL_DIR}/ot_core_api_%.o : ${OPENTHREAD_IMPORT_PATH}/src/core/api/%.cpp | ${LOCAL_DIR}
	${CC} ${CPPFLAGS} ${OTFLAGS} ${addprefix -I, ${OPENTHREAD_HEADER_DIRS}} -o $@ $<

# Build rule for core OpenThread common components.
${LOCAL_DIR}/ot_core_common_%.o : ${OPENTHREAD_IMPORT_PATH}/src/core/common/%.cpp | ${LOCAL_DIR}
	${CC} ${CPPFLAGS} ${OTFLAGS} ${addprefix -I, ${OPENTHREAD_HEADER_DIRS}} -o $@ $<

# Build rule for core OpenThread utility components.
${LOCAL_DIR}/ot_core_utils_%.o : ${OPENTHREAD_IMPORT_PATH}/src/core/utils/%.cpp | ${LOCAL_DIR}
	${CC} ${CPPFLAGS} ${OTFLAGS} ${addprefix -I, ${OPENTHREAD_HEADER_DIRS}} -o $@ $<

# Build rule for OpenThread CLI components.
${LOCAL_DIR}/ot_cli_%.o : ${OPENTHREAD_IMPORT_PATH}/src/cli/%.cpp | ${LOCAL_DIR}
	${CC} ${CPPFLAGS} ${OTFLAGS} ${addprefix -I, ${OPENTHREAD_HEADER_DIRS}} -o $@ $<

# Build rule for OpenThread core radio components.
${LOCAL_DIR}/ot_core_radio_%.o : ${OPENTHREAD_IMPORT_PATH}/src/core/radio/%.cpp | ${LOCAL_DIR}
	${CC} ${CPPFLAGS} ${OTFLAGS} ${addprefix -I, ${OPENTHREAD_HEADER_DIRS}} -o $@ $<

# Build rule for OpenThread core MAC components.
${LOCAL_DIR}/ot_core_mac_%.o : ${OPENTHREAD_IMPORT_PATH}/src/core/mac/%.cpp | ${LOCAL_DIR}
	${CC} ${CPPFLAGS} ${OTFLAGS} ${addprefix -I, ${OPENTHREAD_HEADER_DIRS}} -o $@ $<

# Build rule for OpenThread core IP network protocol components.
${LOCAL_DIR}/ot_core_net_%.o : ${OPENTHREAD_IMPORT_PATH}/src/core/net/%.cpp | ${LOCAL_DIR}
	${CC} ${CPPFLAGS} ${OTFLAGS} ${addprefix -I, ${OPENTHREAD_HEADER_DIRS}} -o $@ $<

# Build rule for OpenThread core mesh network management components.
${LOCAL_DIR}/ot_core_meshcop_%.o : ${OPENTHREAD_IMPORT_PATH}/src/core/meshcop/%.cpp | ${LOCAL_DIR}
	${CC} ${CPPFLAGS} ${OTFLAGS} ${addprefix -I, ${OPENTHREAD_HEADER_DIRS}} -o $@ $<

# Build rule for OpenThread cryptography library components.
${LOCAL_DIR}/ot_core_crypto_%.o : ${OPENTHREAD_IMPORT_PATH}/src/core/crypto/%.cpp | ${LOCAL_DIR}
	${CC} ${CPPFLAGS} ${OTFLAGS} ${addprefix -I, ${OPENTHREAD_HEADER_DIRS}} -o $@ $<

# Build rule for OpenThread CoAP protocol components.
${LOCAL_DIR}/ot_core_coap_%.o : ${OPENTHREAD_IMPORT_PATH}/src/core/coap/%.cpp | ${LOCAL_DIR}
	${CC} ${CPPFLAGS} ${OTFLAGS} ${addprefix -I, ${OPENTHREAD_HEADER_DIRS}} -o $@ $<

# Build rule for OpenThread protocol components.
${LOCAL_DIR}/ot_core_thread_%.o : ${OPENTHREAD_IMPORT_PATH}/src/core/thread/%.cpp | ${LOCAL_DIR}
	${CC} ${CPPFLAGS} ${OTFLAGS} ${addprefix -I, ${OPENTHREAD_HEADER_DIRS}} -o $@ $<

# Build rule for OpenThread backbone router components.
${LOCAL_DIR}/ot_core_backbone_router_%.o : ${OPENTHREAD_IMPORT_PATH}/src/core/backbone_router/%.cpp | ${LOCAL_DIR}
	${CC} ${CPPFLAGS} ${OTFLAGS} ${addprefix -I, ${OPENTHREAD_HEADER_DIRS}} -o $@ $<

# Build rules for third party TCP/IP components, if required.
ifdef OPENTHREAD_USE_EXPERIMENTAL_TCP
${LOCAL_DIR}/ot_third_party_tcpip_%.o : ${OPENTHREAD_IMPORT_PATH}/third_party/tcplp/*/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${OTFLAGS} ${addprefix -I, ${OPENTHREAD_HEADER_DIRS}} -o $@ $<
${LOCAL_DIR}/ot_third_party_tcpip_%.o : ${OPENTHREAD_IMPORT_PATH}/third_party/tcplp/*/*/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${OTFLAGS} ${addprefix -I, ${OPENTHREAD_HEADER_DIRS}} -o $@ $<
endif
