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
# This is the makefile fragment for building the standard second stage
# bootloader for the Raspberry Pi RP2040 range of target devices.
#

# Select the bootloader source directory.
GMOS_PICO_BOOT2_DIR = ${GMOS_PICO_SDK_DIR}/src/rp2_common/boot_stage2

# List all the header directories that are required to build the
# bootloader source code.
BOOTLOADER_HEADER_DIRS = \
	${TARGET_PLATFORM_DIR}/include \
	${GMOS_PICO_BOOT2_DIR}/include \
	${GMOS_PICO_BOOT2_DIR}/asminclude \
	${GMOS_PICO_SDK_DIR}/src/common/pico_base/include \
	${GMOS_PICO_SDK_DIR}/src/rp2_common/pico_platform/include \
	${GMOS_PICO_SDK_DIR}/src/rp2040/hardware_regs/include \
	${GMOS_PICO_SDK_DIR}/src/boards/include

# Select the correct second stage bootloader source file to use with
# the specified boot flash memory.
ifeq (${GMOS_PICO_BOOT_DEVICE}, W25Q16)
BOOTLOADER_OBJ_FILE_NAMES += boot2_w25q080.o
endif

# Specify the local build directory.
LOCAL_DIR = ${GMOS_BUILD_DIR}/bootloader

# Specify the object files that need to be built.
BOOTLOADER_OBJ_FILES = ${addprefix ${LOCAL_DIR}/, ${BOOTLOADER_OBJ_FILE_NAMES}}

# Run the assembler on bootloader sources with the standard options.
${LOCAL_DIR}/%.o : ${GMOS_PICO_SDK_DIR}/src/*/*/%.S | ${LOCAL_DIR}
	${AS} -x assembler-with-cpp ${ASFLAGS} \
		${addprefix -I, ${BOOTLOADER_HEADER_DIRS}} $< -o $@

# Convert the stage two bootloader to a file that can be included in the
# main firmware build.
${LOCAL_DIR}/boot2.S : ${LOCAL_DIR}/boot2.bin
	${GMOS_PICO_BOOT2_DIR}/pad_checksum -s 0xffffffff $< $@

# Convert the bootloader to a binary image for further processing.
${LOCAL_DIR}/boot2.bin : ${LOCAL_DIR}/boot2.elf
	${OC} -S -O binary $< $@

# Link the bootloader object files.
${LOCAL_DIR}/boot2.elf : ${BOOTLOADER_OBJ_FILES}
	${LD} -o $@ ${BLDFLAGS} -T${GMOS_PICO_BOOT2_DIR}/boot_stage2.ld \
		$<

# Create the local build directory.
${LOCAL_DIR} :
	mkdir -p $@
