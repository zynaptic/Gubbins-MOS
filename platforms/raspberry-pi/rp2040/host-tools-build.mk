#
# The Gubbins Microcontroller Operating System
#
# Copyright 2022 Zynaptic Limited
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
# This is the makefile fragment for building the host-side tools used
# for the Raspberry Pi Pico range of target devices.
#

# Specify the location of the host toolchain directory. By default this
# assumes the standard Ubuntu package installation directory.
ifndef HOST_GCC_TOOLCHAIN_DIR
HOST_GCC_TOOLCHAIN_DIR = /usr
endif
HOST_CCPP = $(HOST_GCC_TOOLCHAIN_DIR)/bin/g++

# List all the header directories that are required to build the
# platform source code.
HOST_TOOLS_HEADER_DIRS = \
	${GMOS_PICO_SDK_DIR}/tools/elf2uf2 \
	${GMOS_PICO_SDK_DIR}/src/common/boot_uf2/include

# Specify the local build directory.
LOCAL_DIR = ${GMOS_BUILD_DIR}/host-tools

# Import generated dependency information if available.
-include ${LOCAL_DIR}/elf2uf2.d

# Compile the ELF to UF2 conversion tool.
${LOCAL_DIR}/elf2uf2 : ${GMOS_PICO_SDK_DIR}/tools/elf2uf2/main.cpp | ${LOCAL_DIR}
	${HOST_CCPP} -MMD ${addprefix -I, ${HOST_TOOLS_HEADER_DIRS}} -o $@ $<

# Create the local build directory.
${LOCAL_DIR} :
	mkdir -p $@
