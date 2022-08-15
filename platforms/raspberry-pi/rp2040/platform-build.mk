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
# This is the makefile fragment for building the platform specific
# source files for the Raspberry Pi RP2040 range of target devices.
#

# Build the host-side tools.
include ${TARGET_PLATFORM_DIR}/host-tools-build.mk

# Generate the stage 2 bootloader for inclusion in the firmware image.
include ${TARGET_PLATFORM_DIR}/bootloader-build.mk

# List all the header directories that are required to build the
# platform source code.
PLATFORM_HEADER_DIRS = \
	${GMOS_APP_DIR}/include \
	${GMOS_GIT_DIR}/common/include \
	${TARGET_PLATFORM_DIR}/include \
	${GMOS_PICO_SDK_DIR}/src/common/pico_base/include \
	${GMOS_PICO_SDK_DIR}/src/common/pico_util/include \
	${GMOS_PICO_SDK_DIR}/src/common/pico_binary_info/include \
	${GMOS_PICO_SDK_DIR}/src/common/pico_sync/include \
	${GMOS_PICO_SDK_DIR}/src/common/pico_time/include \
	${GMOS_PICO_SDK_DIR}/src/rp2_common/pico_mem_ops/include \
	${GMOS_PICO_SDK_DIR}/src/rp2_common/pico_bootrom/include \
	${GMOS_PICO_SDK_DIR}/src/rp2_common/pico_platform/include \
	${GMOS_PICO_SDK_DIR}/src/rp2_common/pico_printf/include \
	${GMOS_PICO_SDK_DIR}/src/rp2_common/hardware_base/include \
	${GMOS_PICO_SDK_DIR}/src/rp2_common/hardware_clocks/include \
	${GMOS_PICO_SDK_DIR}/src/rp2_common/hardware_irq/include \
	${GMOS_PICO_SDK_DIR}/src/rp2_common/hardware_dma/include \
	${GMOS_PICO_SDK_DIR}/src/rp2_common/hardware_pll/include \
	${GMOS_PICO_SDK_DIR}/src/rp2_common/hardware_xosc/include \
	${GMOS_PICO_SDK_DIR}/src/rp2_common/hardware_gpio/include \
	${GMOS_PICO_SDK_DIR}/src/rp2_common/hardware_uart/include \
	${GMOS_PICO_SDK_DIR}/src/rp2_common/hardware_spi/include \
	${GMOS_PICO_SDK_DIR}/src/rp2_common/hardware_rtc/include \
	${GMOS_PICO_SDK_DIR}/src/rp2_common/hardware_resets/include \
	${GMOS_PICO_SDK_DIR}/src/rp2_common/hardware_sync/include \
	${GMOS_PICO_SDK_DIR}/src/rp2_common/hardware_timer/include \
	${GMOS_PICO_SDK_DIR}/src/rp2_common/hardware_claim/include \
	${GMOS_PICO_SDK_DIR}/src/rp2_common/hardware_watchdog/include \
	${GMOS_PICO_SDK_DIR}/src/rp2_common/hardware_divider/include \
	${GMOS_PICO_SDK_DIR}/src/rp2040/hardware_regs/include \
	${GMOS_PICO_SDK_DIR}/src/rp2040/hardware_structs/include \
	${GMOS_PICO_SDK_DIR}/src/boards/include

# List all the platform object files that need to be built.
PLATFORM_OBJ_FILE_NAMES = \
	gmos-platform.o \
	pico-device.o \
	pico-timer.o \
	pico-console-simple.o \
	pico-driver-gpio.o \
	pico-driver-timer.o \
	pico-driver-spi.o \
	pico-driver-rtc.o \
	boot2.o \
	pico_standard_link/crt0.o \
	pico_mem_ops/mem_ops.o \
	pico_mem_ops/mem_ops_aeabi.o \
	pico_bootrom/bootrom.o \
	pico_platform/platform.o \
	pico_printf/printf.o \
	pico_runtime/runtime.o \
	pico_divider/divider.o \
	pico_sync/mutex.o \
	pico_sync/lock_core.o \
	pico_sync/critical_section.o \
	pico_time/time.o \
	hardware_pll/pll.o \
	hardware_irq/irq.o \
	hardware_irq/irq_handler_chain.o \
	hardware_dma/dma.o \
	hardware_gpio/gpio.o \
	hardware_uart/uart.o \
	hardware_spi/spi.o \
	hardware_rtc/rtc.o \
	hardware_sync/sync.o \
	hardware_xosc/xosc.o \
	hardware_clocks/clocks.o \
	hardware_timer/timer.o \
	hardware_claim/claim.o \
	hardware_watchdog/watchdog.o \
	hardware_divider/divider.o

# Specify the local build directory.
LOCAL_DIR = ${GMOS_BUILD_DIR}/platform

# Specify the object files that need to be built.
PLATFORM_OBJ_FILES = ${addprefix ${LOCAL_DIR}/, ${PLATFORM_OBJ_FILE_NAMES}}

# Import generated dependency information if available.
-include $(PLATFORM_OBJ_FILES:.o=.d)

# Run the assembler on SDK sources with the standard options.
${LOCAL_DIR}/%.o : ${GMOS_PICO_SDK_DIR}/src/*common/%.S | ${LOCAL_DIR}
	${AS} -x assembler-with-cpp ${ASFLAGS} \
		${addprefix -I, ${PLATFORM_HEADER_DIRS}} $< -o $@

# Run the assembler on generated stage 2 bootloader file with the
# standard options.
${LOCAL_DIR}/%.o : ${GMOS_BUILD_DIR}/bootloader/%.S | ${LOCAL_DIR}
	${AS} -x assembler-with-cpp ${ASFLAGS} \
		${addprefix -I, ${PLATFORM_HEADER_DIRS}} $< -o $@

# Run the C compiler on SDK sources with the standard options.
${LOCAL_DIR}/%.o : ${GMOS_PICO_SDK_DIR}/src/*common/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${addprefix -I, ${PLATFORM_HEADER_DIRS}} -o $@ $<

# Run the C compiler on GubbinsMOS sources with the standard options.
${LOCAL_DIR}/%.o : ${TARGET_PLATFORM_DIR}/src/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${addprefix -I, ${PLATFORM_HEADER_DIRS}} -o $@ $<

# Timestamp the target platform object files.
${LOCAL_DIR}/timestamp : ${PLATFORM_OBJ_FILES}
	touch $@

# Create the local build directories.
${LOCAL_DIR} :
	mkdir -p $@/pico_standard_link
	mkdir -p $@/pico_mem_ops
	mkdir -p $@/pico_bootrom
	mkdir -p $@/pico_platform
	mkdir -p $@/pico_printf
	mkdir -p $@/pico_runtime
	mkdir -p $@/pico_divider
	mkdir -p $@/pico_sync
	mkdir -p $@/pico_time
	mkdir -p $@/hardware_pll
	mkdir -p $@/hardware_irq
	mkdir -p $@/hardware_dma
	mkdir -p $@/hardware_gpio
	mkdir -p $@/hardware_uart
	mkdir -p $@/hardware_spi
	mkdir -p $@/hardware_rtc
	mkdir -p $@/hardware_sync
	mkdir -p $@/hardware_xosc
	mkdir -p $@/hardware_clocks
	mkdir -p $@/hardware_timer
	mkdir -p $@/hardware_claim
	mkdir -p $@/hardware_watchdog
	mkdir -p $@/hardware_divider

# Generate the firmware binary in Intel hex format.
${GMOS_BUILD_DIR}/firmware.hex : ${GMOS_BUILD_DIR}/firmware.elf
	${OC} -S -O ihex $< $@

# Generate the firmware binary as a raw binary file.
${GMOS_BUILD_DIR}/firmware.bin : ${GMOS_BUILD_DIR}/firmware.elf
	${OC} -S -O binary $< $@

# Generate the firmware binary as a UF2 format file. This requires the
# appropriate host-side conversion tool to have been built.
${GMOS_BUILD_DIR}/firmware.uf2 : ${GMOS_BUILD_DIR}/firmware.elf \
		${GMOS_BUILD_DIR}/host-tools/elf2uf2
	${GMOS_BUILD_DIR}/host-tools/elf2uf2 \
		${GMOS_BUILD_DIR}/firmware.elf $@

# Add the UF2 target to the 'all' target.
all : ${GMOS_BUILD_DIR}/firmware.uf2
