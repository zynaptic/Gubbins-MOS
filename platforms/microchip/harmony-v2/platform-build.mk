#
# The Gubbins Microcontroller Operating System
#
# Copyright 2020-2022 Zynaptic Limited
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
# source files for the Microchip range of PIC32 target devices supported
# by the Harmony-V2 framework.
#

# List all the header directories that are required to build the
# platform source code.
PLATFORM_HEADER_DIRS = \
	${GMOS_APP_DIR}/include \
	${GMOS_GIT_DIR}/common/include \
	${GMOS_GIT_DIR}/imports/printf \
	${TARGET_PLATFORM_DIR}/include \
	${HARMONY_CODEGEN_DIR}/firmware/src/system_config/default/framework

# List all the platform object files that need to be built.
PLATFORM_OBJ_FILE_NAMES = \
	printf.o \
	gmos-platform.o \
	harmony-app-hooks.o \
	harmony-rtos-hooks.o \
	harmony-driver-gpio.o \
	harmony-driver-timer.o \
	harmony-driver-spi.o

# List all the common Harmony framework files that need to be built.
PLATFORM_OBJ_FILE_NAMES += \
	sys_int_pic32.o \
	sys_tmr.o \
	sys_random.o \
	drv_tmr.o \
	drv_spi.o \
	drv_spi_sys_queue_fifo.o \
	crypto.o \
	random.o \
	memory.o\
	sha256.o

# List of automatically generated Harmony source files.
# Note that this does not currently support FreeRTOS integration.
GENERATED_SRC_FILE_NAMES += \
	system_init.c \
	system_tasks.c \
	system_interrupt.c

# Scan the automatically generated Harmony system and driver source
# directories.
GENERATED_SRC_FILE_NAMES += \
	$(shell ls -R -w1 ${HARMONY_CODEGEN_DIR}/firmware/src/*/*/*/ | grep \\.c$)
GENERATED_ASM_FILE_NAMES += \
	$(shell ls -R -w1 ${HARMONY_CODEGEN_DIR}/firmware/src/*/*/*/ | grep \\.S$)

# Add list of automatically generated Harmony object files.
PLATFORM_OBJ_FILE_NAMES += $(GENERATED_SRC_FILE_NAMES:.c=.o)
PLATFORM_OBJ_FILE_NAMES += $(GENERATED_ASM_FILE_NAMES:.S=.o)

# Specify the local build directory.
LOCAL_DIR = ${GMOS_BUILD_DIR}/platform

# Specify the object files that need to be built.
PLATFORM_OBJ_FILES = ${addprefix ${LOCAL_DIR}/, ${PLATFORM_OBJ_FILE_NAMES}}

# Import generated dependency information if available.
-include $(PLATFORM_OBJ_FILES:.o=.d)

# Run the C compiler with the standard options.
${LOCAL_DIR}/%.o : ${TARGET_PLATFORM_DIR}/src/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${addprefix -I, ${PLATFORM_HEADER_DIRS}} -o $@ $<

# Run the C compiler for the selected standard library support files.
${LOCAL_DIR}/%.o : ${GMOS_GIT_DIR}/imports/*/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} -DPRINTF_INCLUDE_CONFIG_H \
		${addprefix -I, ${PLATFORM_HEADER_DIRS}} -o $@ $<

# Run the compiler for the main generated Harmony framework files.
${LOCAL_DIR}/%.o : ${HARMONY_CODEGEN_DIR}/firmware/src/*/*/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${addprefix -I, ${PLATFORM_HEADER_DIRS}} -o $@ $<

# Run the compiler for the generated Harmony framework module files.
${LOCAL_DIR}/%.o : ${HARMONY_CODEGEN_DIR}/firmware/src/*/*/*/*/*/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${addprefix -I, ${PLATFORM_HEADER_DIRS}} -o $@ $<

# Run the compiler for the generated Harmony framework module files.
${LOCAL_DIR}/%.o : ${HARMONY_CODEGEN_DIR}/firmware/src/*/*/*/*/*/*/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${addprefix -I, ${PLATFORM_HEADER_DIRS}} -o $@ $<

# Run the assembler for the generated Harmony framework module files.
${LOCAL_DIR}/%.o : ${HARMONY_CODEGEN_DIR}/firmware/src/*/*/%.S | ${LOCAL_DIR}
	${AS} ${ASFLAGS} ${addprefix -I, ${PLATFORM_HEADER_DIRS}} -o $@ $<

# Run the assembler for the generated Harmony framework module files.
${LOCAL_DIR}/%.o : ${HARMONY_CODEGEN_DIR}/firmware/src/*/*/*/*/*/*/%.S | ${LOCAL_DIR}
	${AS} ${ASFLAGS} ${addprefix -I, ${PLATFORM_HEADER_DIRS}} -o $@ $<

# Compile standard driver and system components.
${LOCAL_DIR}/%.o : ${HARMONY_FRAMEWORK_SRC_DIR}/*/src/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${addprefix -I, ${PLATFORM_HEADER_DIRS}} -o $@ $<

# Compile standard driver and system components.
${LOCAL_DIR}/%.o : ${HARMONY_FRAMEWORK_SRC_DIR}/*/*/src/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${addprefix -I, ${PLATFORM_HEADER_DIRS}} -o $@ $<

# Compile standard driver and system components.
${LOCAL_DIR}/%.o : ${HARMONY_FRAMEWORK_SRC_DIR}/*/*/src/*/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${addprefix -I, ${PLATFORM_HEADER_DIRS}} -o $@ $<

# Timestamp the target platform object files.
${LOCAL_DIR}/timestamp : ${PLATFORM_OBJ_FILES}
	touch $@

# Create the local build directory.
${LOCAL_DIR} :
	mkdir -p $@

# Generate the firmware binary in Intel hex format.
${GMOS_BUILD_DIR}/firmware.hex : ${GMOS_BUILD_DIR}/firmware.elf
	${XC32_GCC_TOOLCHAIN_DIR}/bin/xc32-bin2hex --sort $<

# Generate the firmware binary as a raw binary file.
${GMOS_BUILD_DIR}/firmware.bin : ${GMOS_BUILD_DIR}/firmware.elf
	${OC} -S -O binary $< $@

# Load the compiled image to flash.
JAVA_JVM != ls ${MPLAB_DISTRIBUTION_DIR}/sys/java/*/bin/java
install : ${GMOS_BUILD_DIR}/firmware.hex
	${JAVA_JVM} -jar ${MPLAB_DISTRIBUTION_DIR}/mplab_platform/mplab_ipe/ipecmd.jar \
		-M -TP${MPLAB_PROGRAMMER_TYPE} -P${GMOS_TARGET_DEVICE} -OL -F$<
