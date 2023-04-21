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
# This is the makefile fragment for building the common GubbinsMOS
# display library components.
#

# Specify the source code directories to use.
DISPLAY_COMMON_DIR = ${GMOS_GIT_DIR}/displays/common

# List all the header directories that are required to build the
# common display library code.
DISPLAY_HEADER_DIRS = \
	${GMOS_APP_DIR}/include \
	${GMOS_GIT_DIR}/common/include \
	${TARGET_PLATFORM_DIR}/include \
	${DISPLAY_COMMON_DIR}/include

# List all the application object files that need to be built.
DISPLAY_OBJ_FILE_NAMES = \
	gmos-display-raster.o \
	gmos-display-font.o \
	gmos-display-font-defs.o

# Specify the local build directory.
LOCAL_DIR = ${GMOS_BUILD_DIR}/displays/common

# Specify the object files that need to be built.
DISPLAY_OBJ_FILES = ${addprefix ${LOCAL_DIR}/, ${DISPLAY_OBJ_FILE_NAMES}}

# Import generated dependency information if available.
-include $(DISPLAY_OBJ_FILES:.o=.d)

# Run the C compiler on the common display files.
${LOCAL_DIR}/%.o : ${DISPLAY_COMMON_DIR}/src/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} ${addprefix -I, ${DISPLAY_HEADER_DIRS}} -o $@ $<

# Rebuild the font definition files if the contents of the font
# directory have changed from the standard distribution.
DISPLAY_COMMON_FONTS = $(shell ls -xw0 ${DISPLAY_COMMON_DIR}/fonts/*/*.bdf)
${DISPLAY_COMMON_DIR}/include/gmos-display-font-defs.h : \
		${DISPLAY_COMMON_DIR}/src/gmos-display-font-defs.c
	touch $@
${DISPLAY_COMMON_DIR}/src/gmos-display-font-defs.c : ${DISPLAY_COMMON_FONTS} \
		${DISPLAY_COMMON_DIR}/tools/gmos-display-fontgen.py
	${DISPLAY_COMMON_DIR}/tools/gmos-display-fontgen.py \
		--font_dir ${DISPLAY_COMMON_DIR}/fonts \
		--code_file ${DISPLAY_COMMON_DIR}/src/gmos-display-font-defs.c \
		--header_file ${DISPLAY_COMMON_DIR}/include/gmos-display-font-defs.h

# Timestamp the display library object files.
${LOCAL_DIR}/timestamp : ${DISPLAY_OBJ_FILES}
	touch $@

# Create the local build directory.
${LOCAL_DIR} :
	mkdir -p $@
