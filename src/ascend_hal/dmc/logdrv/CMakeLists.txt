# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
get_filename_component(BASE_DIR "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)

add_library(drv_log_obj OBJECT)

target_sources(drv_log_obj PRIVATE
    drv_log_user.c
)

target_compile_options(drv_log_obj PRIVATE
    -fPIC
)

target_compile_definitions(drv_log_obj PRIVATE
    DRV_HOST
    __USE_GNU
    _GNU_SOURCE
)

target_include_directories(drv_log_obj PRIVATE
    ${BASE_DIR}
    ${C_SEC_INCLUDE}
    ${DRIVER_HAL_INC_DIR}
    ${DRIVER_USER_DIR}/inc
    ${DRIVER_USER_DIR}/inc/slog
)

add_library(drv_log_base_obj OBJECT)

target_sources(drv_log_base_obj PRIVATE
    drv_log_user_kernel_api.c
    drv_log_user_common.c
    drv_share_log.c
    $<$<STREQUAL:${ENABLE_UBE},true>:drv_urma_log.c>
)

target_compile_definitions(drv_log_base_obj PRIVATE
    DRV_HOST
)

target_include_directories(drv_log_base_obj PRIVATE
    ${BASE_DIR}
    ${C_SEC_INCLUDE}
    ${DRIVER_HAL_INC_DIR}
    ${DRIVER_USER_DIR}/pbl/commlib
    ${DRIVER_USER_DIR}/inc
    ${DRIVER_USER_DIR}/inc/slog
    $<IF:$<STREQUAL:${ENABLE_UBE},true>,/usr/include/ub/umdk/urma,${DRIVER_NETWORK_DIR}/inc/ascend950/urma/>
)
