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
# This is the makefile fragment for building the platform specific
# source files for the Silicon Labs EFR32xG2x range of target devices.
#

# Specify SDK specific file locations.
GMOS_TARGET_DEVICE_FAMILY_DIR := \
	${GMOS_GECKO_SDK_DIR}/platform/Device/SiliconLabs/${GMOS_TARGET_DEVICE_FAMILY}

# List all the header directories that are required to build the
# platform source code.
PLATFORM_HEADER_DIRS = \
	${GMOS_APP_DIR}/include \
	${GMOS_GIT_DIR}/common/include \
	${GMOS_GIT_DIR}/imports/printf \
	${TARGET_PLATFORM_DIR}/include \
	${GMOS_GECKO_SDK_DIR}/platform/common/inc \
	${GMOS_GECKO_SDK_DIR}/platform/emlib/inc \
	${GMOS_GECKO_SDK_DIR}/platform/emdrv/common/inc \
	${GMOS_GECKO_SDK_DIR}/platform/emdrv/spidrv/inc \
	${GMOS_GECKO_SDK_DIR}/platform/emdrv/dmadrv/inc \
	${GMOS_GECKO_SDK_DIR}/platform/service/sleeptimer/inc \
	${GMOS_GECKO_SDK_DIR}/platform/CMSIS/Core/Include \
	${GMOS_TARGET_DEVICE_FAMILY_DIR}/Include

# List all the platform object files that need to be built.
PLATFORM_OBJ_FILE_NAMES = \
	printf.o \
	gmos-platform.o \
	efr32-device.o \
	efr32-timer.o \
	efr32-console-simple.o \
	efr32-driver-gpio.o \
	efr32-driver-timer.o \
	efr32-driver-spi.o \
	efr32-driver-iic.o \
	em_system.o \
	em_core.o \
	em_emu.o \
	em_cmu.o \
	em_burtc.o \
	em_gpio.o \
	em_ldma.o \
	em_usart.o \
	em_eusart.o \
	em_i2c.o \
	dmadrv.o \
	spidrv.o \
	sl_sleeptimer.o \
	sl_sleeptimer_hal_burtc.o \
	startup_${GMOS_TARGET_DEVICE_FAMILY_LC}.o \
	system_${GMOS_TARGET_DEVICE_FAMILY_LC}.o

# Specify the local build directory.
LOCAL_DIR = ${GMOS_BUILD_DIR}/platform

# Specify the object files that need to be built.
PLATFORM_OBJ_FILES = ${addprefix ${LOCAL_DIR}/, ${PLATFORM_OBJ_FILE_NAMES}}

# Import generated dependency information if available.
-include $(PLATFORM_OBJ_FILES:.o=.d)

# Generate the linker script for the specified target device. This
# currently copies over the default script from the SDK, which only
# supports large memory devices booting directly into the application.
# This should be updated with a configurable linker script template to
# support the full range of devices and bootloader options.
${LOCAL_DIR}/target.ld : ${GMOS_TARGET_DEVICE_FAMILY_DIR}/Source/GCC/${GMOS_TARGET_DEVICE_FAMILY_LC}.ld | ${LOCAL_DIR}
	cp $< $@

# Run the assembler with the standard options.
${LOCAL_DIR}/%.o : ${TARGET_PLATFORM_DIR}/src/%.s | ${LOCAL_DIR}
	${AS} -x assembler-with-cpp ${ASFLAGS} $< -o $@

# Run the C compiler with the standard options.
${LOCAL_DIR}/%.o : ${TARGET_PLATFORM_DIR}/src/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${addprefix -I, ${PLATFORM_HEADER_DIRS}} -o $@ $<

# Run the C compiler for the selected standard library support files.
${LOCAL_DIR}/%.o : ${GMOS_GIT_DIR}/imports/*/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} -DPRINTF_INCLUDE_CONFIG_H \
		${addprefix -I, ${PLATFORM_HEADER_DIRS}} -o $@ $<

# Run the C compiler on the SDK device family specific files.
${LOCAL_DIR}/%.o : ${GMOS_TARGET_DEVICE_FAMILY_DIR}/Source/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${addprefix -I, ${PLATFORM_HEADER_DIRS}} -o $@ $<

# Run the C compiler on the SDK device library specific files.
${LOCAL_DIR}/%.o : ${GMOS_GECKO_SDK_DIR}/platform/*/src/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${addprefix -I, ${PLATFORM_HEADER_DIRS}} -o $@ $<

# Run the C compiler on the SDK service library specific files.
${LOCAL_DIR}/%.o : ${GMOS_GECKO_SDK_DIR}/platform/*/*/src/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${addprefix -I, ${PLATFORM_HEADER_DIRS}} -o $@ $<

# Timestamp the target platform object files.
${LOCAL_DIR}/timestamp : ${PLATFORM_OBJ_FILES} ${LOCAL_DIR}/target.ld
	touch $@

# Create the local build directory.
${LOCAL_DIR} :
	mkdir -p $@

# Generate the firmware binary in Intel hex format.
${GMOS_BUILD_DIR}/firmware.hex : ${GMOS_BUILD_DIR}/firmware.elf
	${OC} -S -O ihex $< $@

# Generate the firmware binary as a raw binary file.
${GMOS_BUILD_DIR}/firmware.bin : ${GMOS_BUILD_DIR}/firmware.elf
	${OC} -S -O binary $< $@
