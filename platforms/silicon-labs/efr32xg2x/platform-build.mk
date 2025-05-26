#
# The Gubbins Microcontroller Operating System
#
# Copyright 2023-2025 Zynaptic Limited
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

# List all the header directories that are required to build the
# platform source code.
PLATFORM_HEADER_DIRS = \
	${GMOS_APP_DIR}/include \
	${GMOS_GIT_DIR}/common/include \
	${GMOS_GIT_DIR}/imports/printf \
	${TARGET_PLATFORM_DIR}/include \
	${GMOS_SIMPLICITY_SDK_DIR}/platform/common/inc \
	${GMOS_SIMPLICITY_SDK_DIR}/platform/common/config \
	${GMOS_SIMPLICITY_SDK_DIR}/platform/emlib/inc \
	${GMOS_SIMPLICITY_SDK_DIR}/platform/emdrv/common/inc \
	${GMOS_SIMPLICITY_SDK_DIR}/platform/emdrv/spidrv/inc \
	${GMOS_SIMPLICITY_SDK_DIR}/platform/emdrv/dmadrv/inc \
	${GMOS_SIMPLICITY_SDK_DIR}/platform/emdrv/dmadrv/inc/s2_signals \
	${GMOS_SIMPLICITY_SDK_DIR}/platform/emdrv/nvm3/inc \
	${GMOS_SIMPLICITY_SDK_DIR}/platform/emdrv/nvm3/config \
	${GMOS_SIMPLICITY_SDK_DIR}/platform/peripheral/inc \
	${GMOS_SIMPLICITY_SDK_DIR}/platform/driver/gpio/inc \
	${GMOS_SIMPLICITY_SDK_DIR}/platform/service/mpu/inc \
	${GMOS_SIMPLICITY_SDK_DIR}/platform/service/sleeptimer/inc \
	${GMOS_SIMPLICITY_SDK_DIR}/platform/service/device_manager/inc \
	${GMOS_SIMPLICITY_SDK_DIR}/platform/service/interrupt_manager/inc \
	${GMOS_SIMPLICITY_SDK_DIR}/platform/service/interrupt_manager/config \
	${GMOS_SIMPLICITY_SDK_DIR}/platform/service/clock_manager/inc \
	${GMOS_SIMPLICITY_SDK_DIR}/platform/security/sl_component/se_manager/inc \
	${GMOS_SIMPLICITY_SDK_DIR}/platform/CMSIS/Core/Include \
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
	efr32-driver-eeprom.o \
	efr32-driver-flash.o \
	sdk-em_system.o \
	sdk-em_core.o \
	sdk-em_emu.o \
	sdk-em_cmu.o \
	sdk-em_rmu.o \
	sdk-em_burtc.o \
	sdk-em_gpio.o \
	sdk-em_ldma.o \
	sdk-em_usart.o \
	sdk-em_eusart.o \
	sdk-em_i2c.o \
	sdk-em_msc.o \
	sdk-em_prs.o \
	sdk-dmadrv.o \
	sdk-spidrv.o \
	sdk-nvm3_default_common_linker.o \
	sdk-nvm3.o \
	sdk-nvm3_object.o \
	sdk-nvm3_utils.o \
	sdk-nvm3_page.o \
	sdk-nvm3_cache.o \
	sdk-nvm3_lock.o \
	sdk-nvm3_hal_flash.o \
	sdk-sl_mpu.o \
	sdk-sl_core_cortexm.o \
	sdk-sl_device_clock_${GMOS_TARGET_DEVICE_FAMILY_XLC}.o \
	sdk-sl_device_peripheral_hal_${GMOS_TARGET_DEVICE_FAMILY_XLC}.o \
	sdk-sl_interrupt_manager_cortexm.o \
	sdk-sl_clock_manager.o \
	sdk-sl_clock_manager_hal_s2.o \
	sdk-sl_hal_sysrtc.o \
	sdk-sl_sleeptimer.o \
	sdk-sl_sleeptimer_hal_burtc.o \
	sdk-sl_sleeptimer_hal_sysrtc.o \
	sdk-startup_${GMOS_TARGET_DEVICE_FAMILY_LC}.o \
	sdk-system_${GMOS_TARGET_DEVICE_FAMILY_LC}.o

# Specify the local build directory.
LOCAL_DIR = ${GMOS_BUILD_DIR}/platform

# Specify the object files that need to be built.
PLATFORM_OBJ_FILES = ${addprefix ${LOCAL_DIR}/, ${PLATFORM_OBJ_FILE_NAMES}}

# Import generated dependency information if available.
-include $(PLATFORM_OBJ_FILES:.o=.d)

# Generate the linker script for the specified target device.
${LOCAL_DIR}/target.ld : ${TARGET_PLATFORM_DIR}/tools/linker-script-gen.py | ${LOCAL_DIR}
	python3 $< --device ${GMOS_TARGET_DEVICE_VARIANT} --output $@

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
${LOCAL_DIR}/sdk-%.o : ${GMOS_TARGET_DEVICE_FAMILY_DIR}/Source/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${addprefix -I, ${PLATFORM_HEADER_DIRS}} -o $@ $<

# Run the C compiler on the SDK device library specific files.
${LOCAL_DIR}/sdk-%.o : ${GMOS_SIMPLICITY_SDK_DIR}/platform/*/src/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${addprefix -I, ${PLATFORM_HEADER_DIRS}} -o $@ $<

# Run the C compiler on the SDK service library specific files.
${LOCAL_DIR}/sdk-%.o : ${GMOS_SIMPLICITY_SDK_DIR}/platform/*/*/*/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${addprefix -I, ${PLATFORM_HEADER_DIRS}} -o $@ $<

# Run the C compiler on the Silicon Labs core utilities files.
${LOCAL_DIR}/sdk-%.o : ${GMOS_SIMPLICITY_SDK_DIR}/util/silicon_labs/*/*/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${addprefix -I, ${PLATFORM_HEADER_DIRS}} -o $@ $<

# Include the makefile fragment for building the platform specific
# cryptography library.
include ${TARGET_PLATFORM_DIR}/platform-crypto-build.mk

# Timestamp the target platform object files.
${LOCAL_DIR}/timestamp : ${PLATFORM_OBJ_FILES} \
	${PLATFORM_CRYPTO_OBJ_FILES} ${LOCAL_DIR}/target.ld
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
