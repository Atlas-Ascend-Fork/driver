# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
add_library(commlib_obj OBJECT)

target_sources(commlib_obj PRIVATE
    drv_error_map.c
    drv_user_list.c
    atomic_lock.c
    atomic_ref.c
    drv_comm_intf.c
    drv_repair_fault.c
    drv_current.c
    drv_module_feature.c
)

target_compile_options(commlib_obj PRIVATE
    -fPIC
)

target_include_directories(commlib_obj PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${DRIVER_USER_DIR}/inc
    ${DRIVER_USER_DIR}/inc/trs
    ${DRIVER_USER_DIR}/inc/pbl
    ${DRIVER_HAL_INC_DIR}
    ${C_SEC_INCLUDE}
    ${DRIVER_USER_DIR}/msnpureport/inc/slog
    ${DRIVER_USER_DIR}/dms/common
    ${DRIVER_KERNEL_DIR}/inc
    ${DRIVER_KERNEL_DIR}/inc/ascend_platform
    ${DRIVER_KERNEL_DIR}/pbl/dev_urd/command/ioctl/
    ${DRIVER_KERNEL_DIR}/pbl/soc_resmng/command/ioctl
    ${DRIVER_KERNEL_DIR}/dms/command/ioctl
    ${DRIVER_KERNEL_DIR}/pbl/uda/command/ioctl
)

include(${FEATURE_CMAKE_PATH})
target_compile_definitions(commlib_obj PRIVATE
    ${CONFIG_DEFINES}
    $<$<STREQUAL:${ENABLE_BUILD_PRODUCT},TRUE>:ENABLE_BUILD_PRODUCT>
    $<$<STREQUAL:${ASCEND910_93_EX},TRUE>:ASCEND910_93_EX>
    $<$<STREQUAL:${PRODUCT},ascend950>:CFG_FEATURE_DEVICE_REPLACE>
    DRV_HOST
)
