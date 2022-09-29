#
# The Gubbins Microcontroller Operating System
#
# Copyright 2022 Zynaptic Limited
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
# TCP/IP source files for the WIZnet W5500 TCP/IP offload device.
#

# Specify the source code directories to use.
WIZNET_DRIVER_PATH = tcpip/wiznet/w5500
WIZNET_DRIVER_SRC_DIR = ${GMOS_GIT_DIR}/network/${WIZNET_DRIVER_PATH}
TCPIP_COMMON_SRC_DIR = ${GMOS_GIT_DIR}/network/tcpip/common

# List all the header directories that are required to build the
# TCP/IP stack code.
WIZNET_HEADER_DIRS = \
	${GMOS_APP_DIR}/include \
	${GMOS_GIT_DIR}/common/include \
	${TARGET_PLATFORM_DIR}/include \
	${TARGET_PLATFORM_DIR}/vendor/include \
	${TCPIP_COMMON_SRC_DIR}/include \
	${WIZNET_DRIVER_SRC_DIR}/include

# List all the application object files that need to be built.
WIZNET_OBJ_FILE_NAMES = \
	gmos-tcpip-dhcp.o \
	wiznet-driver-core.o \
	wiznet-driver-socket.o \
	wiznet-driver-socket-udp.o \
	wiznet-driver-socket-tcp.o \
	wiznet-driver-socket-util.o \
	wiznet-spi-adaptor.o

# Specify the local build directory.
LOCAL_DIR = ${GMOS_BUILD_DIR}/network/${WIZNET_DRIVER_PATH}

# Specify the object files that need to be built.
WIZNET_OBJ_FILES = ${addprefix ${LOCAL_DIR}/, ${WIZNET_OBJ_FILE_NAMES}}

# Import generated dependency information if available.
-include $(WIZNET_OBJ_FILES:.o=.d)

# Run the C compiler on the TCP/IP stack common files.
${LOCAL_DIR}/%.o : ${TCPIP_COMMON_SRC_DIR}/src/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${addprefix -I, ${WIZNET_HEADER_DIRS}} -o $@ $<

# Run the C compiler on the vendor stack specific files.
${LOCAL_DIR}/%.o : ${WIZNET_DRIVER_SRC_DIR}/src/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${addprefix -I, ${WIZNET_HEADER_DIRS}} -o $@ $<

# Timestamp the application object files.
${LOCAL_DIR}/timestamp : ${WIZNET_OBJ_FILES}
	touch $@

# Create the local build directory.
${LOCAL_DIR} :
	mkdir -p $@
