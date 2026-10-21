# ----------------------------------------------------------------------------
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0(the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------

if (PRODUCT STREQUAL "ascend910B")
    add_custom_target(extract_driver_device_custom ALL
        COMMAND echo "Copy custom files to ${DRIVER_DEVICE_INSTALL_DIR}."
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/lib/lib64/driver
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/lib/lib64/stub
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/lib/cert/ca
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/tools/npu-smi ${CMAKE_BINARY_DIR}/lib/tools/
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/tools/kmsagent ${CMAKE_BINARY_DIR}/lib/tools/
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/cert/ca/Computing_RSA-PSS_Equipment_CA_2099.pem ${CMAKE_BINARY_DIR}/lib/cert/ca/
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/cert/ca/Equipment_Root_CA_2041.pem ${CMAKE_BINARY_DIR}/lib/cert/ca/
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/cert/ca/Computing_RSA-PSS_Equipment_CA_2071.pem ${CMAKE_BINARY_DIR}/lib/cert/ca/
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/cert/ca/RSA_Equipment_Root_CA_2071.pem ${CMAKE_BINARY_DIR}/lib/cert/ca/
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/cert/ca/RSA-PSS_Equipment_Root_CA_2099.pem ${CMAKE_BINARY_DIR}/lib/cert/ca/
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/cert/ca/IT_Product_CA_2041.pem ${CMAKE_BINARY_DIR}/lib/cert/ca/
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/lib64/driver/libsecurec.so ${CMAKE_BINARY_DIR}/lib/lib64/driver/
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/lib64/driver/libkmc.so.25 ${CMAKE_BINARY_DIR}/lib/lib64/driver/
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/lib64/driver/libsdp.so.25 ${CMAKE_BINARY_DIR}/lib/lib64/driver/
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/lib64/driver/libcmscbb.so ${CMAKE_BINARY_DIR}/lib/lib64/driver/
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/lib64/driver/libtls_product.so ${CMAKE_BINARY_DIR}/lib/lib64/driver/
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/lib64/driver/libmbedcrypto.so.16 ${CMAKE_BINARY_DIR}/lib/lib64/driver/
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/lib64/driver/libmbedtls.so.21 ${CMAKE_BINARY_DIR}/lib/lib64/driver/
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/lib64/driver/libmbedx509.so.7 ${CMAKE_BINARY_DIR}/lib/lib64/driver/
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/lib64/driver/libaivaulttee.so ${CMAKE_BINARY_DIR}/lib/lib64/driver/
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/lib64/driver/libaivault.so ${CMAKE_BINARY_DIR}/lib/lib64/driver/
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/lib64/driver/kmsagent.conf ${CMAKE_BINARY_DIR}/lib/lib64/driver/
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/lib64/stub/libcrypto.so ${CMAKE_BINARY_DIR}/lib/lib64/stub
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/lib64/stub/libssl.so ${CMAKE_BINARY_DIR}/lib/lib64/stub
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/script/kmsagent_backup_conf.sh ${CMAKE_BINARY_DIR}/lib/script/
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/script/kmsagent_install.sh ${CMAKE_BINARY_DIR}/lib/script/
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/script/kmsagent_restore_conf.sh ${CMAKE_BINARY_DIR}/lib/script/
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/script/kmsagent_uninstall.sh ${CMAKE_BINARY_DIR}/lib/script/
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/script/kmsagent.service ${CMAKE_BINARY_DIR}/lib/script/
        DEPENDS extract_driver_device
        WORKING_DIRECTORY ${DRIVER_DEVICE_INSTALL_DIR}
    )
elseif (PRODUCT STREQUAL "ascend950")
    add_custom_target(extract_driver_device_custom ALL
        COMMAND echo "Copy custom files to ${DRIVER_DEVICE_INSTALL_DIR}."
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/lib/lib64/driver
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/lib/lib64/inner
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/lib/cert/ca
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/cert/ca/Computing_RSA-PSS_Equipment_CA_2071.pem ${CMAKE_BINARY_DIR}/lib/cert/ca/
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/cert/ca/Computing_RSA-PSS_Equipment_CA_2099.pem ${CMAKE_BINARY_DIR}/lib/cert/ca/
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/cert/ca/Equipment_Root_CA_2041.pem ${CMAKE_BINARY_DIR}/lib/cert/ca/
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/cert/ca/IT_Product_CA_2041.pem ${CMAKE_BINARY_DIR}/lib/cert/ca/
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/cert/ca/RSA_Equipment_Root_CA_2071.pem ${CMAKE_BINARY_DIR}/lib/cert/ca/
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/cert/ca/RSA-PSS_Equipment_Root_CA_2099.pem ${CMAKE_BINARY_DIR}/lib/cert/ca/
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/lib64/driver/kmsagent.conf ${CMAKE_BINARY_DIR}/lib/lib64/driver/
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/lib64/driver/libaivault.so ${CMAKE_BINARY_DIR}/lib/lib64/driver/
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/lib64/driver/libaivaulttee.so ${CMAKE_BINARY_DIR}/lib/lib64/driver/
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/lib64/driver/libcmscbb.so ${CMAKE_BINARY_DIR}/lib/lib64/driver/
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/lib64/driver/libkmc.so.25 ${CMAKE_BINARY_DIR}/lib/lib64/driver/
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/lib64/driver/libhccntls.so ${CMAKE_BINARY_DIR}/lib/lib64/driver/
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/lib64/driver/libmbedcrypto.so.16 ${CMAKE_BINARY_DIR}/lib/lib64/driver/
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/lib64/driver/libmbedtls.so.21 ${CMAKE_BINARY_DIR}/lib/lib64/driver/
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/lib64/driver/libmbedx509.so.7 ${CMAKE_BINARY_DIR}/lib/lib64/driver/
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/lib64/driver/libsdp.so.25 ${CMAKE_BINARY_DIR}/lib/lib64/driver/
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/lib64/driver/libsecurec.so ${CMAKE_BINARY_DIR}/lib/lib64/driver/
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/lib64/inner/libcrypto.so ${CMAKE_BINARY_DIR}/lib/lib64/inner
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/lib64/inner/libssl.so ${CMAKE_BINARY_DIR}/lib/lib64/inner
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/script/kmsagent_backup_conf.sh ${CMAKE_BINARY_DIR}/lib/script/
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/script/kmsagent_install.sh ${CMAKE_BINARY_DIR}/lib/script/
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/script/kmsagent_restore_conf.sh ${CMAKE_BINARY_DIR}/lib/script/
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/script/kmsagent_uninstall.sh ${CMAKE_BINARY_DIR}/lib/script/
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/script/kmsagent.service ${CMAKE_BINARY_DIR}/lib/script/
        COMMAND ${CMAKE_COMMAND} -E copy ${DRV_DEV_ARCHIVE_PREFIX}/tools/kmsagent ${CMAKE_BINARY_DIR}/lib/tools/
        COMMAND ${CMAKE_COMMAND} -E create_symlink libmbedcrypto.so.16 ${CMAKE_BINARY_DIR}/lib/lib64/driver/libmbedcrypto.so
        COMMAND ${CMAKE_COMMAND} -E create_symlink libmbedtls.so.21 ${CMAKE_BINARY_DIR}/lib/lib64/driver/libmbedtls.so
        COMMAND ${CMAKE_COMMAND} -E create_symlink libmbedx509.so.7 ${CMAKE_BINARY_DIR}/lib/lib64/driver/libmbedx509.so
        DEPENDS extract_driver_device
        WORKING_DIRECTORY ${DRIVER_DEVICE_INSTALL_DIR}
    )
endif()
