#!/usr/bin/env bash
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
# Fetches the stable release of the Raspberry Pi Pico SDK from the main
# repository. This must be done before running a GubbinsMOS build using
# the Raspberry Pi Pico as a target platform.
#

# Specify the SDK version to be used.
PICO_SDK_VERSION=1.4.0

# Always clone into the import directory.
LOCAL_DIR="$(dirname "${BASH_SOURCE[0]}")"
cd $LOCAL_DIR

# Clone the repo.
git clone --branch ${PICO_SDK_VERSION} --single-branch --depth 1 \
    https://github.com/raspberrypi/pico-sdk
