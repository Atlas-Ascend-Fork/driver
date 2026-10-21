# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
set(SRC_LIST
    ${MODULE_SRC_DIR}/dsmi_common/dsmi_dmp_command.c
    ${MODULE_SRC_DIR}/dsmi_common/dsmi_upgrade_proc.c
    ${MODULE_SRC_DIR}/dsmi_common/dsmi_common.c
    ${MODULE_SRC_DIR}/dsmi_common/dsmi_common_interface.c
    ${MODULE_SRC_DIR}/dsmi_common/fault/dsmi_fault_event.c
    dsmi_adapt.c
)

target_sources(drvdsmi_host PRIVATE ${SRC_LIST})
target_include_directories(drvdsmi_host PRIVATE
    ${DRIVER_HAL_INC_DIR}
    ${DRIVER_USER_DIR}/msnpureport/inc/slog
    ${DRIVER_USER_DIR}/dsmi/dc/
    ${DRIVER_USER_DIR}/dmc/device_monitor/dc
    ${DRIVER_USER_DIR}/dmc/device_monitor/include
    ${DRIVER_USER_DIR}/dmc/device_monitor/lib/include
    ${DRIVER_USER_DIR}/dmc/feature/host
    ${DRIVER_USER_DIR}/inc
    ${DRIVER_USER_DIR}/inc/mmpa_sdk
    ${DRIVER_USER_DIR}/dms/fault
    ${DRIVER_USER_DIR}/dms/include
    ${MODULE_SRC_DIR}/dc
    ${MODULE_SRC_DIR}/include
    ${MODULE_SRC_DIR}/dsmi_product
    ${DRIVER_KERNEL_DIR}/inc
    ${DRIVER_KERNEL_DIR}/inc/ascend_platform
    ${DRIVER_KERNEL_DIR}/pbl/dev_urd/command/ioctl/
    ${DRIVER_KERNEL_DIR}/dms/command/ioctl
    ${DRIVER_KERNEL_DIR}/pbl/uda/command/ioctl
    $<$<STREQUAL:${PRODUCT},ascend910B>:${DRIVER_USER_DIR}/inc/config/user_config/cloudv2>
    $<$<STREQUAL:${PRODUCT},ascend950>:${DRIVER_USER_DIR}/inc/config/user_config/cloudv4>
    ${DRIVER_USER_DIR}/dms/hbm
    ${DRIVER_USER_DIR}/src/comm/ascend_urma_adapt
    ${TOP_DIR}/ubengine/ssapi/userspace/umdk/src/urma/lib/urma/core/include
    ${TOP_DIR}/ubengine/ssapi/userspace/umdk/src/urma/lib/urma/core
)

set(support_ssh_config_host_list "ascend910B" "ascend950")
set(support_p2p_config_host_list "ascend910B" "ascend950")
set(support_ccpu_usr_cert_config_host_list "ascend910B" "ascend950")
set(product_910b "ascend910B")
set(product_950_series "ascend950")
set(support_roce_network_list "ascend910B" "ascend950")
set(support_hbm_ecc_host_list "ascend910B" "ascend950")
set(dms_architecture_v1 "ascend910B")
set(support_auth_enable_sign_host_list "ascend910B" "ascend950")

include(${FEATURE_CMAKE_PATH})

target_compile_definitions(drvdsmi_host PRIVATE
    CFG_FEATURE_DMP_HDC
    CFG_EDGE_HOST
    UPGRADE_HOST
    DRV_HOST
    ${CONFIG_DEFINES}
    $<$<STREQUAL:${PRODUCT},ascend910B>:CFG_SOC_PLATFORM_CLOUD>
    $<$<IN_LIST:${PRODUCT},${support_hbm_ecc_host_list}>:CFG_FEATURE_ECC_HBM_INFO>
    $<$<STREQUAL:${PRODUCT},ascend910B>:CFG_FEATURE_SRIOV>
    $<$<STREQUAL:${PRODUCT},ascend910B>:CFG_FEATURE_POWER>
    $<$<STREQUAL:${PRODUCT},ascend950>:CFG_FEATURE_POWER>
    $<$<STREQUAL:${PRODUCT},ascend910B>:CFG_FEATURE_FIRMWARE_TYPE_STRONG_MATCH>
    $<$<STREQUAL:${PRODUCT},ascend910B>:CFG_FEATURE_UPGRADE_PATCH_CONFIG>
    $<$<STREQUAL:${PRODUCT},ascend950>:CFG_FEATURE_UPGRADE_PATCH_CONFIG>
    $<$<STREQUAL:${PRODUCT},ascend950>:CFG_FEATURE_UPGRADE_MAMI_PATCH_CONFIG>
    $<$<IN_LIST:${PRODUCT},${support_ssh_config_host_list}>:CFG_FEATURE_SUPPORT_SSH_CONFIG>
    $<$<IN_LIST:${PRODUCT},${support_p2p_config_host_list}>:CFG_FEATURE_SUPPORT_P2P_CONFIG>
    $<$<IN_LIST:${PRODUCT},${support_ccpu_usr_cert_config_host_list}>:CFG_FEATURE_SUPPORT_CCPU_USR_CERT_CONFIG>
    $<$<IN_LIST:${PRODUCT},${product_910b}>:CFG_FEATURE_NEW_EVENT_CODE>
    $<$<IN_LIST:${PRODUCT},${product_950_series}>:CFG_FEATURE_NEW_EVENT_CODE>
    $<$<IN_LIST:${PRODUCT},${product_950_series}>:CFG_FEATURE_NETWORK_UNIC>
    $<$<IN_LIST:${PRODUCT},${support_roce_network_list}>:CFG_FEATURE_NETWORK_ROCE>
    $<$<IN_LIST:${PRODUCT},${dms_architecture_v1}>:CFG_FEATURE_DMS_ARCH_V1>
    $<$<STREQUAL:${PRODUCT},ascend910B>:CFG_FEATURE_PCIE_LINK_ERROR_INFO>
    $<$<STREQUAL:${PRODUCT},ascend950>:CFG_ENV_FPGA>
    $<$<IN_LIST:${PRODUCT},${product_950_series}>:CFG_FEATURE_PCIE_LINK_ERROR_INFO>
    $<$<IN_LIST:${PRODUCT},${product_950_series}>:CFG_FEATURE_PCIE_LINK_INFO>
    $<$<IN_LIST:${PRODUCT},${product_950_series}>:CFG_FEATURE_FAULT_EVENT>
    $<$<STREQUAL:${PRODUCT},ascend950>:CFG_FEATURE_BIST>
    $<$<STREQUAL:${PRODUCT},ascend960>:CFG_FEATURE_BIST>
    $<$<STREQUAL:${PRODUCT},ascend960esl>:CFG_FEATURE_BIST>
    $<$<IN_LIST:${PRODUCT},${support_auth_enable_sign_host_list}>:CFG_FEATURE_SUPPORT_AUTH_ENABLE_SIGN>
    $<$<STREQUAL:${PRODUCT},ascend910B>:CFG_FEATURE_SUPPORT_UDIS>
    $<$<STREQUAL:${PRODUCT},ascend950>:CFG_FEATURE_SUPPORT_UDIS>
    $<$<STREQUAL:${PRODUCT},ascend960>:CFG_FEATURE_SUPPORT_UDIS>
    $<$<STREQUAL:${PRODUCT},ascend960esl>:CFG_FEATURE_SUPPORT_UDIS>
    $<$<STREQUAL:${PRODUCT},ascend910B>:CFG_FEATURE_UDIS_UTIL_FROM_TS>
    $<$<STREQUAL:${PRODUCT},ascend950>:CFG_FEATURE_UDIS_UTIL_FROM_TS>
    $<$<STREQUAL:${PRODUCT},ascend960>:CFG_FEATURE_UDIS_UTIL_FROM_TS>
)

target_compile_options(drvdsmi_host PRIVATE
    -fvisibility=hidden
)

target_link_libraries(drvdsmi_host PRIVATE
     $<BUILD_INTERFACE:intf_pub>
    -Wl,--no-as-needed
    PUBLIC c_sec
    ascend_hal
    -Wl,--as-needed
    -ldl
)

install(TARGETS drvdsmi_host
    LIBRARY DESTINATION ${INSTALL_LIBRARY_DIR} OPTIONAL
)