#
# The Gubbins Microcontroller Operating System
#
# Copyright 2024-2025 Zynaptic Limited
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
# MQTT version 3.1.1 client library.
#

# Specify the source code directories to use.
MQTT_CLIENT_PATH = protocol/mqtt
MQTT_CLIENT_SRC_DIR = ${GMOS_GIT_DIR}/network/${MQTT_CLIENT_PATH}

# List all the header directories that are required to build the
# MQTT client code.
MQTT_HEADER_DIRS = \
	${GMOS_APP_DIR}/include \
	${GMOS_GIT_DIR}/common/include \
	${GMOS_GIT_DIR}/network/common/include \
	${TARGET_PLATFORM_DIR}/include \
	${TARGET_PLATFORM_DIR}/vendor/include \
	${MQTT_CLIENT_SRC_DIR}/include

# List all the application object files that need to be built.
MQTT_OBJ_FILE_NAMES = \
	gmos-mqtt-packet.o \
	gmos-mqtt-client.o \
	gmos-mqtt-keepalive.o \
	gmos-mqtt-publish.o \
	gmos-mqtt-subscribe.o

# Specify the local build directory.
LOCAL_DIR = ${GMOS_BUILD_DIR}/network/${MQTT_CLIENT_PATH}

# Specify the object files that need to be built.
MQTT_OBJ_FILES = ${addprefix ${LOCAL_DIR}/, ${MQTT_OBJ_FILE_NAMES}}

# Import generated dependency information if available.
-include $(MQTT_OBJ_FILES:.o=.d)

# Run the C compiler on the MQTT client source files.
${LOCAL_DIR}/%.o : ${MQTT_CLIENT_SRC_DIR}/src/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${addprefix -I, ${MQTT_HEADER_DIRS}} -o $@ $<

# Timestamp the application object files.
${LOCAL_DIR}/timestamp : ${MQTT_OBJ_FILES}
	touch $@

# Create the local build directory.
${LOCAL_DIR} :
	mkdir -p $@
