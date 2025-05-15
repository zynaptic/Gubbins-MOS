#
# The Gubbins Microcontroller Operating System
#
# Copyright 2022-2025 Zynaptic Limited
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
# the Zigbee stack.
#

# List all the header directories that are required to build the common
# Zigbee stack components.
ZIGBEE_HEADER_DIRS += \
	${GMOS_APP_DIR}/include \
	${GMOS_GIT_DIR}/common/include \
	${TARGET_PLATFORM_DIR}/include \
	${TARGET_PLATFORM_DIR}/vendor/include \
	${ZIGBEE_COMMON_SRC_DIR}/include \
	${ZIGBEE_TARGET_SRC_DIR}/include

# List all the common component object files that need to be built.
ZIGBEE_OBJ_FILE_NAMES = \
	gmos-zigbee-stack.o \
	gmos-zigbee-aps.o \
	gmos-zigbee-endpoint.o \
	gmos-zigbee-zdo-common.o \
	gmos-zigbee-zdo-client.o \
	gmos-zigbee-zdo-server.o \
	gmos-zigbee-zcl-core.o \
	gmos-zigbee-zcl-core-local.o \
	gmos-zigbee-zcl-core-remote.o \
	gmos-zigbee-zcl-general-basic.o

# Run the C compiler on the GubbinsMOS common source files.
${LOCAL_DIR}/%.o : ${ZIGBEE_COMMON_SRC_DIR}/src/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${OTFLAGS} ${addprefix -I, ${ZIGBEE_HEADER_DIRS}} -o $@ $<
