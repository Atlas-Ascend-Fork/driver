# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
add_library(drvaibuff_base_obj OBJECT)

set(BUFF_BASE_COMM_TARGET_SOURCES
    ascend_hal_base/buff_manage_kernel_api.c
)
target_sources(drvaibuff_base_obj PRIVATE
    ${BUFF_BASE_COMM_TARGET_SOURCES}
)

target_include_directories(drvaibuff_base_obj PRIVATE
    ${DRIVER_USER_DIR}/buff/dc/ascend_hal_base
    ${DRIVER_USER_DIR}/buff/dc/comm/common
    ${DRIVER_USER_DIR}/buff/dc/comm
    ${DRIVER_USER_DIR}/buff/dc/comm/lib
    ${C_SEC_INCLUDE}
    ${DRIVER_HAL_INC_DIR}
    ${DRIVER_USER_DIR}/inc
    ${DRIVER_USER_DIR}/inc/pbl
    ${DRIVER_USER_DIR}/pbl/commlib
    ${DRIVER_KERNEL_DIR}/buff/dc/command/ioctl
    ${DRIVER_USER_DIR}/msnpureport/inc/slog
)

target_compile_definitions(drvaibuff_base_obj PRIVATE
    DRV_HOST
)

target_link_libraries(drvaibuff_base_obj PRIVATE
    $<BUILD_INTERFACE:intf_pub>
)

add_library(drvaibuff_obj OBJECT)

set(BUFF_COMM_TARGET_SOURCES
    comm/buff_event.c
    comm/buff_query_grp.c
    comm/buff_range.c
    comm/buff_cache.c
    # comm/pool_drv.c
    comm/buff_memzone_adapt.c
    comm/buff_mempool_adapt.c
    comm/buff_large_buf_adapt.c
    comm/lib/bitmap.c
    comm/drv_buff_adp.c
    comm/drv_buff_list.c
    comm/drv_buff_maintain.c
    comm/drv_buff_mbuf.c
    comm/drv_buff_memzone.c
    comm/drv_usr_buff_mempool.c
    comm/common/drv_buff_common_mempool.c
    comm/common/drv_buff_unibuff.c
    comm/common/buff_feature.c
    comm/buff_recycle/buff_recycle.c
    comm/buff_recycle/buff_recycle_ctx.c
)

target_sources(drvaibuff_obj PRIVATE
    ${BUFF_COMM_TARGET_SOURCES}
    share_fd_adp/buff_mng.c
    share_fd_adp/grp_mng.c
    comm/buff_host_adapt.c
)

set(BUFF_PRODUCT_SIDE_ADP_INC
    ${DRIVER_USER_DIR}/buff/dc/share_fd_adp
)

target_include_directories(drvaibuff_obj PRIVATE
    ${DRIVER_USER_DIR}/buff/dc/ascend_hal_base
    ${DRIVER_USER_DIR}/buff/dc/comm
    ${DRIVER_USER_DIR}/buff/dc/comm/buff_recycle
    ${DRIVER_USER_DIR}/buff/dc/comm/common
    ${DRIVER_USER_DIR}/buff/dc/comm/lib
    ${C_SEC_INCLUDE}
    ${DRIVER_HAL_INC_DIR}
    ${DRIVER_USER_DIR}/inc
    ${DRIVER_USER_DIR}/inc/pbl
    ${DRIVER_USER_DIR}/pbl/commlib
    ${DRIVER_KERNEL_DIR}/buff/dc/command/ioctl
    ${BUFF_PRODUCT_SIDE_ADP_INC}
    ${DRIVER_USER_DIR}/msnpureport/inc/slog
    ${DRIVER_KERNEL_DIR}/dms/command/ioctl
)

target_compile_definitions(drvaibuff_obj PRIVATE
    DRV_HOST
)

target_link_libraries(drvaibuff_obj PRIVATE
    $<BUILD_INTERFACE:intf_pub>
)
