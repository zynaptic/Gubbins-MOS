#
# The Gubbins Microcontroller Operating System
#
# Copyright 2023-2026 Zynaptic Limited
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
# This is the makefile fragment for packaging the compiled application for
# use with the Silicon Labs EFR32xG2x range of target devices.
#

# Temporary bootloader file name.
ifndef GMOS_BOOTLOADER_FILE
GMOS_BOOTLOADER_FILE = ${TARGET_PLATFORM_DIR}/bootloader/mgm240-bootloader-storage-internal-single-1536k.s37
endif

# Specify the location of the Silicon Labs firmware image signing certificate
# and keys directory.
ifndef GMOS_APP_SIGNING_CERT_DIR
GMOS_APP_SIGNING_CERT_DIR = ${TARGET_PLATFORM_DIR}/demo-certs
endif

# Ensure that the application certificates directory exists.
${GMOS_APP_SIGNING_CERT_DIR} :
	mkdir -p $@

# Create a symmetric firmware image encryption key if required. This should
# never overwrite an existing encryption key.
ifeq (,$(wildcard ${GMOS_APP_SIGNING_CERT_DIR}/aes_key.txt))
${GMOS_APP_SIGNING_CERT_DIR}/aes_key.txt : ${GMOS_APP_SIGNING_CERT_DIR}
	${GMOS_SIMPLICITY_COMMANDER_DIR}/commander util genkey --type aes-ccm --outfile $@
endif

# Create a bootloader signing key pair if required. This should never
# overwrite an existing bootloader signing private key.
ifeq (,$(wildcard ${GMOS_APP_SIGNING_CERT_DIR}/sign_key.pem))
${GMOS_APP_SIGNING_CERT_DIR}/sign_key.pem ${GMOS_APP_SIGNING_CERT_DIR}/sign_pubkey.pem &: \
		${GMOS_APP_SIGNING_CERT_DIR}
	${GMOS_SIMPLICITY_COMMANDER_DIR}/commander util genkey --type ecc-p256 \
		--privkey ${GMOS_APP_SIGNING_CERT_DIR}/sign_key.pem \
		--pubkey ${GMOS_APP_SIGNING_CERT_DIR}/sign_pubkey.pem \
		--tokenfile ${GMOS_APP_SIGNING_CERT_DIR}/sign_pubkey.txt
endif

# Create a bootloader key pair if required. This should never overwrite an
# existing bootloader private key.
ifeq (,$(wildcard ${GMOS_APP_SIGNING_CERT_DIR}/bl_cert_key.pem))
${GMOS_APP_SIGNING_CERT_DIR}/bl_cert_key.pem ${GMOS_APP_SIGNING_CERT_DIR}/bl_cert_pubkey.pem &: \
		${GMOS_APP_SIGNING_CERT_DIR}
	${GMOS_SIMPLICITY_COMMANDER_DIR}/commander util genkey --type ecc-p256 \
		--privkey ${GMOS_APP_SIGNING_CERT_DIR}/bl_cert_key.pem \
		--pubkey ${GMOS_APP_SIGNING_CERT_DIR}/bl_cert_pubkey.pem \
		--tokenfile ${GMOS_APP_SIGNING_CERT_DIR}/bl_cert_pubkey.txt
endif

# Create an application key pair if required. This should never overwite an
# existing application private key.
ifeq (,$(wildcard ${GMOS_APP_SIGNING_CERT_DIR}/app_cert_key.pem))
${GMOS_APP_SIGNING_CERT_DIR}/app_cert_key.pem ${GMOS_APP_SIGNING_CERT_DIR}/app_cert_pubkey.pem &: \
		${GMOS_APP_SIGNING_CERT_DIR}
	${GMOS_SIMPLICITY_COMMANDER_DIR}/commander util genkey --type ecc-p256 \
		--privkey ${GMOS_APP_SIGNING_CERT_DIR}/app_cert_key.pem \
		--pubkey ${GMOS_APP_SIGNING_CERT_DIR}/app_cert_pubkey.pem \
		--tokenfile ${GMOS_APP_SIGNING_CERT_DIR}/app_cert_pubkey.txt
endif

# Create a bootloader certificate file if required. This should never
# overwrite an existing certificate file.
ifeq (,$(wildcard ${GMOS_APP_SIGNING_CERT_DIR}/bl_cert.bin))
${GMOS_APP_SIGNING_CERT_DIR}/bl_cert.bin : \
		${GMOS_APP_SIGNING_CERT_DIR}/bl_cert_pubkey.pem \
		${GMOS_APP_SIGNING_CERT_DIR}/sign_key.pem
	${GMOS_SIMPLICITY_COMMANDER_DIR}/commander util gencert \
		--cert-type secureboot --cert-version 1 \
		--cert-pubkey ${GMOS_APP_SIGNING_CERT_DIR}/bl_cert_pubkey.pem \
		--sign ${GMOS_APP_SIGNING_CERT_DIR}/sign_key.pem --outfile $@
endif

# Create an application certificate file if required. This should never
# overwrite an existing certificate file.
ifeq (,$(wildcard ${GMOS_APP_SIGNING_CERT_DIR}/app_cert.bin))
${GMOS_APP_SIGNING_CERT_DIR}/app_cert.bin : \
		${GMOS_APP_SIGNING_CERT_DIR}/app_cert_pubkey.pem \
		${GMOS_APP_SIGNING_CERT_DIR}/bl_cert_key.pem
	${GMOS_SIMPLICITY_COMMANDER_DIR}/commander util gencert \
		--cert-type secureboot --cert-version 1 \
		--cert-pubkey ${GMOS_APP_SIGNING_CERT_DIR}/app_cert_pubkey.pem \
		--sign ${GMOS_APP_SIGNING_CERT_DIR}/bl_cert_key.pem --outfile $@
endif

# Add the bootloader CRC to the Intel hex format file.
${GMOS_BUILD_DIR}/firmware-crc.hex : ${GMOS_BUILD_DIR}/firmware.hex
	${GMOS_SIMPLICITY_COMMANDER_DIR}/commander convert $< --crc \
		--outfile $@

# Add the application certificate to the Intel hex format file.
${GMOS_BUILD_DIR}/firmware-signed.hex : ${GMOS_BUILD_DIR}/firmware.hex \
		${GMOS_APP_SIGNING_CERT_DIR}/app_cert.bin \
		${GMOS_APP_SIGNING_CERT_DIR}/app_cert_key.pem
	${GMOS_SIMPLICITY_COMMANDER_DIR}/commander convert $< --secureboot \
		--certificate ${GMOS_APP_SIGNING_CERT_DIR}/app_cert.bin \
		--keyfile ${GMOS_APP_SIGNING_CERT_DIR}/app_cert_key.pem \
		--outfile $@

# Add the bootloader certificate to the Intel hex format file.
${GMOS_BUILD_DIR}/bootloader-signed.hex : ${GMOS_BOOTLOADER_FILE} \
		${GMOS_APP_SIGNING_CERT_DIR}/bl_cert.bin \
		${GMOS_APP_SIGNING_CERT_DIR}/bl_cert_key.pem \
		${GMOS_APP_SIGNING_CERT_DIR}/aes_key.txt
	${GMOS_SIMPLICITY_COMMANDER_DIR}/commander convert $< --secureboot \
		--certificate ${GMOS_APP_SIGNING_CERT_DIR}/bl_cert.bin \
		--keyfile ${GMOS_APP_SIGNING_CERT_DIR}/bl_cert_key.pem \
		--aeskey ${GMOS_APP_SIGNING_CERT_DIR}/aes_key.txt \
		--outfile $@

# Build the GBL3 file for the CRC protected firmware image.
${GMOS_BUILD_DIR}/firmware-crc.gbl : ${GMOS_BUILD_DIR}/firmware-crc.hex
	${GMOS_SIMPLICITY_COMMANDER_DIR}/commander gbl create $@ \
		--app $<

# Build the GBL3 file for the CRC protected firmware image (compressed).
${GMOS_BUILD_DIR}/firmware-crc-lzma.gbl : ${GMOS_BUILD_DIR}/firmware-crc.hex
	${GMOS_SIMPLICITY_COMMANDER_DIR}/commander gbl create $@ --compress lzma \
		--app $<

# Build the GBL3 file for the signed firmware image.
${GMOS_BUILD_DIR}/firmware-signed.gbl : ${GMOS_BUILD_DIR}/firmware-signed.hex
	${GMOS_SIMPLICITY_COMMANDER_DIR}/commander gbl create $@ \
		--certificate ${GMOS_APP_SIGNING_CERT_DIR}/app_cert.bin \
		--sign ${GMOS_APP_SIGNING_CERT_DIR}/app_cert_key.pem \
		--app $<

# Build the GBL3 file for the signed firmware image (compressed).
${GMOS_BUILD_DIR}/firmware-signed-lzma.gbl : ${GMOS_BUILD_DIR}/firmware-signed.hex
	${GMOS_SIMPLICITY_COMMANDER_DIR}/commander gbl create $@ --compress lzma \
		--certificate ${GMOS_APP_SIGNING_CERT_DIR}/app_cert.bin \
		--sign ${GMOS_APP_SIGNING_CERT_DIR}/app_cert_key.pem \
		--app $<

# Build the GBL3 file for the signed firmware image (compressed and encrypted).
${GMOS_BUILD_DIR}/firmware-signed-aes.gbl : ${GMOS_BUILD_DIR}/firmware-signed.hex \
		${GMOS_APP_SIGNING_CERT_DIR}/aes_key.txt
	${GMOS_SIMPLICITY_COMMANDER_DIR}/commander gbl create $@ --compress lzma \
		--encrypt ${GMOS_APP_SIGNING_CERT_DIR}/aes_key.txt \
		--certificate ${GMOS_APP_SIGNING_CERT_DIR}/app_cert.bin \
		--sign ${GMOS_APP_SIGNING_CERT_DIR}/app_cert_key.pem \
		--app $<

# Build the GBL3 file for the signed bootloader image.
${GMOS_BUILD_DIR}/bootloader-signed.gbl : ${GMOS_BUILD_DIR}/bootloader-signed.hex
	${GMOS_SIMPLICITY_COMMANDER_DIR}/commander gbl create $@ \
		--certificate ${GMOS_APP_SIGNING_CERT_DIR}/app_cert.bin \
		--sign ${GMOS_APP_SIGNING_CERT_DIR}/app_cert_key.pem \
		--bootloader $<

# Build the GBL3 file for the signed bootloader image (compressed).
${GMOS_BUILD_DIR}/bootloader-signed-lzma.gbl : ${GMOS_BUILD_DIR}/bootloader-signed.hex
	${GMOS_SIMPLICITY_COMMANDER_DIR}/commander gbl create $@ --compress lzma \
		--certificate ${GMOS_APP_SIGNING_CERT_DIR}/app_cert.bin \
		--sign ${GMOS_APP_SIGNING_CERT_DIR}/app_cert_key.pem \
		--bootloader $<

# Build the GBL3 file for the signed bootloader image (compressed and encrypted).
${GMOS_BUILD_DIR}/bootloader-signed-aes.gbl : ${GMOS_BUILD_DIR}/bootloader-signed.hex \
		${GMOS_APP_SIGNING_CERT_DIR}/aes_key.txt
	${GMOS_SIMPLICITY_COMMANDER_DIR}/commander gbl create $@ --compress lzma \
		--encrypt ${GMOS_APP_SIGNING_CERT_DIR}/aes_key.txt \
		--certificate ${GMOS_APP_SIGNING_CERT_DIR}/app_cert.bin \
		--sign ${GMOS_APP_SIGNING_CERT_DIR}/app_cert_key.pem \
		--bootloader $<

# Build the GBL3 file for the signed firmware and bootloader image (compressed and encrypted).
${GMOS_BUILD_DIR}/firmware-bootloader-signed-aes.gbl : \
		${GMOS_BUILD_DIR}/bootloader-signed.hex \
		${GMOS_BUILD_DIR}/firmware-signed.hex \
		${GMOS_APP_SIGNING_CERT_DIR}/aes_key.txt
	${GMOS_SIMPLICITY_COMMANDER_DIR}/commander gbl create $@ --compress lzma \
		--encrypt ${GMOS_APP_SIGNING_CERT_DIR}/aes_key.txt \
		--certificate ${GMOS_APP_SIGNING_CERT_DIR}/app_cert.bin \
		--sign ${GMOS_APP_SIGNING_CERT_DIR}/app_cert_key.pem \
		--bootloader ${GMOS_BUILD_DIR}/bootloader-signed.hex \
		--app ${GMOS_BUILD_DIR}/firmware-signed.hex

# Include extended build artifact list.
all : \
	${GMOS_BUILD_DIR}/firmware-crc.gbl \
	${GMOS_BUILD_DIR}/firmware-crc-lzma.gbl \
	${GMOS_BUILD_DIR}/firmware-signed.gbl \
	${GMOS_BUILD_DIR}/firmware-signed-lzma.gbl \
	${GMOS_BUILD_DIR}/firmware-signed-aes.gbl \
	${GMOS_BUILD_DIR}/bootloader-signed.gbl \
	${GMOS_BUILD_DIR}/bootloader-signed-lzma.gbl \
	${GMOS_BUILD_DIR}/bootloader-signed-aes.gbl \
	${GMOS_BUILD_DIR}/firmware-bootloader-signed-aes.gbl
