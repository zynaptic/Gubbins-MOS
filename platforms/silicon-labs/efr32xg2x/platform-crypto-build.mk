#
# The Gubbins Microcontroller Operating System
#
# Copyright 2023-2024 Zynaptic Limited
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
# This is the makefile fragment for building the platform specific PSA
# cryptography libraries for the Silicon Labs EFR32xG2x range of target
# devices.
#

# List all the header directories that are required to build the
# platform cryptography library source code.
PLATFORM_CRYPTO_HEADER_DIRS = \
	${TARGET_PLATFORM_DIR}/include \
	${GMOS_GECKO_SDK_DIR}/util/third_party/mbedtls/include \
	${GMOS_GECKO_SDK_DIR}/util/third_party/mbedtls/library \
	${GMOS_GECKO_SDK_DIR}/platform/common/inc \
	${GMOS_GECKO_SDK_DIR}/platform/emlib/inc \
	${GMOS_GECKO_SDK_DIR}/platform/emdrv/common/inc \
	${GMOS_GECKO_SDK_DIR}/platform/emdrv/nvm3/inc \
	${GMOS_GECKO_SDK_DIR}/platform/emdrv/nvm3/config/s2 \
	${GMOS_GECKO_SDK_DIR}/platform/security/sl_component/se_manager/inc \
	${GMOS_GECKO_SDK_DIR}/platform/security/sl_component/sl_psa_driver/inc \
	${GMOS_GECKO_SDK_DIR}/platform/security/sl_component/sl_mbedtls_support/inc \
	${GMOS_GECKO_SDK_DIR}/platform/security/sl_component/sl_mbedtls_support/config \
	${GMOS_GECKO_SDK_DIR}/platform/CMSIS/Core/Include \
	${GMOS_TARGET_DEVICE_FAMILY_DIR}/Include

# List all the platform cryptography object files that need to be built.
PLATFORM_CRYPTO_OBJ_FILE_NAMES = \
	crypto-sl_entropy_hardware.o \
	crypto-sl_se_manager.o \
	crypto-sl_se_manager_util.o \
	crypto-sl_se_manager_entropy.o \
	crypto-sl_se_manager_cipher.o \
	crypto-sl_se_manager_hash.o \
	crypto-sl_se_manager_signature.o \
	crypto-sl_se_manager_key_handling.o \
	crypto-sl_se_manager_key_derivation.o \
	crypto-sl_psa_its_nvm3.o \
	crypto-sl_psa_crypto.o \
	crypto-sli_psa_trng.o \
	crypto-sli_psa_crypto.o \
	crypto-sli_psa_driver_init.o \
	crypto-sli_se_driver_aead.o \
	crypto-sli_se_driver_cipher.o \
	crypto-sli_se_driver_key_management.o \
	crypto-sli_se_driver_key_derivation.o \
	crypto-sli_se_driver_mac.o \
	crypto-sli_se_driver_signature.o \
	crypto-sli_se_opaque_driver_aead.o \
	crypto-sli_se_opaque_driver_cipher.o \
	crypto-sli_se_opaque_driver_mac.o \
	crypto-sli_se_opaque_key_derivation.o \
	crypto-sli_se_transparent_driver_aead.o \
	crypto-sli_se_transparent_driver_cipher.o \
	crypto-sli_se_transparent_driver_hash.o \
	crypto-sli_se_transparent_driver_mac.o \
	crypto-sli_se_transparent_key_derivation.o \
	crypto-mbedtls-platform.o \
	crypto-mbedtls-platform_util.o \
	crypto-mbedtls-debug.o \
	crypto-mbedtls-bignum.o \
	crypto-mbedtls-bignum_core.o \
	crypto-mbedtls-constant_time.o \
	crypto-mbedtls-entropy.o \
	crypto-mbedtls-md.o \
	crypto-mbedtls-cipher.o \
	crypto-mbedtls-cipher_wrap.o \
	crypto-mbedtls-sha256.o \
	crypto-mbedtls-aes.o \
	crypto-mbedtls-ccm.o \
	crypto-mbedtls-cmac.o \
	crypto-mbedtls-hmac_drbg.o \
	crypto-mbedtls-ecp.o \
	crypto-mbedtls-ecp_curves.o \
	crypto-mbedtls-ecdh.o \
	crypto-mbedtls-ecdsa.o \
	crypto-mbedtls-ecjpake.o \
	crypto-mbedtls-pk.o \
	crypto-mbedtls-pk_wrap.o \
	crypto-mbedtls-pkparse.o \
	crypto-mbedtls-oid.o \
	crypto-mbedtls-asn1parse.o \
	crypto-mbedtls-asn1write.o \
	crypto-mbedtls-ctr_drbg.o \
	crypto-mbedtls-ssl_tls.o \
	crypto-mbedtls-ssl_msg.o \
	crypto-mbedtls-ssl_client.o \
	crypto-mbedtls-ssl_tls12_client.o \
	crypto-mbedtls-ssl_ciphersuites.o \
	crypto-mbedtls-ssl_debug_helpers_generated.o \
	crypto-mbedtls-psa_util.o \
	crypto-mbedtls-psa_crypto.o \
	crypto-mbedtls-psa_crypto_cipher.o \
	crypto-mbedtls-psa_crypto_hash.o \
	crypto-mbedtls-psa_crypto_mac.o \
	crypto-mbedtls-psa_crypto_aead.o \
	crypto-mbedtls-psa_crypto_ecp.o \
	crypto-mbedtls-psa_crypto_client.o \
	crypto-mbedtls-psa_crypto_storage.o \
	crypto-mbedtls-psa_crypto_slot_management.o \
	crypto-mbedtls-psa_crypto_driver_wrappers_no_static.o

# Specify the object files that need to be built. The local build
# directory should already be defined.
PLATFORM_CRYPTO_OBJ_FILES = ${addprefix ${LOCAL_DIR}/, ${PLATFORM_CRYPTO_OBJ_FILE_NAMES}}

# Import generated dependency information if available.
-include $(PLATFORM_CRYPTO_OBJ_FILES:.o=.d)

# Run the C compiler on the SDK platform security files.
${LOCAL_DIR}/crypto-%.o : ${GMOS_GECKO_SDK_DIR}/platform/security/*/*/src/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} -DMBEDTLS_CONFIG_FILE='"efr32-crypto-config.h"' \
	${addprefix -I, ${PLATFORM_CRYPTO_HEADER_DIRS}} -o $@ $<

# Run the C compiler on the MBedTLS source files.
${LOCAL_DIR}/crypto-mbedtls-%.o : ${GMOS_GECKO_SDK_DIR}/util/third_party/mbedtls/library/%.c | ${LOCAL_DIR}
	${CC} ${CFLAGS} -DMBEDTLS_CONFIG_FILE='"efr32-crypto-config.h"' \
	${addprefix -I, ${PLATFORM_CRYPTO_HEADER_DIRS}} -o $@ $<
