# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
if (${PRODUCT} STREQUAL ascend950)
    if (${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL aarch64)
        if(${ENABLE_UBE})
            set(support_ub "ascend950")
        endif ()
    endif ()
endif ()

add_library(queue_obj OBJECT)
target_sources(queue_obj PRIVATE
    que_host_adapt.c
)
if (${PRODUCT} IN_LIST support_ub)
target_sources(queue_obj PRIVATE
    que_host_ub_adapt.c
)
endif ()

target_include_directories(queue_obj PRIVATE
    ${DRIVER_USER_DIR}/buff/dc/comm
    ${DRIVER_USER_DIR}/buff/dc/comm/lib
    ${DRIVER_USER_DIR}/buff/dc/comm/common
    ${DRIVER_USER_DIR}/pbl/commlib
    ${DRIVER_USER_DIR}/queue/command
    ${DRIVER_USER_DIR}/queue/dc/core
    ${DRIVER_USER_DIR}/queue/dc/interface
    ${DRIVER_USER_DIR}/queue/dc/agent/common
    ${DRIVER_USER_DIR}/queue/dc/
    ${C_SEC_INCLUDE}
    ${DRIVER_HAL_INC_DIR}
    ${DRIVER_USER_DIR}/inc
    ${DRIVER_USER_DIR}/inc/pbl
    ${DRIVER_KERNEL_DIR}/queue/command/ioctl
    ${DRIVER_KERNEL_DIR}/esched/command/ioctl
    ${DRIVER_KERNEL_DIR}/queue/command/msg
    ${DRIVER_KERNEL_DIR}/esched/command/msg
    ${DRIVER_USER_DIR}/buff/dc/share_fd_adp
    ${DRIVER_USER_DIR}/esched
    ${DRIVER_USER_DIR}/esched/comm
    ${DRIVER_USER_DIR}/esched/dc
    ${DRIVER_USER_DIR}/esched/command
    ${DRIVER_USER_DIR}/msnpureport/inc/slog
    ${DRIVER_KERNEL_DIR}/dms/command/ioctl
)

target_link_libraries(queue_obj PRIVATE
    $<BUILD_INTERFACE:intf_pub>
)

target_compile_definitions(queue_obj PRIVATE
    DRV_HOST
    CFG_FEATURE_HOST_SUPPORT_LOCAL_QUEUE
    $<$<IN_LIST:${PRODUCT},${support_ub}>:CFG_FEATURE_QUE_SUPPORT_UB>
    $<$<AND:$<STREQUAL:${PRODUCT},ascend950>,$<STREQUAL:${CMAKE_HOST_SYSTEM_PROCESSOR},aarch64>>:SSAPI_USE_MAMI>
)

add_subdirectory(core)
add_subdirectory(agent)
add_subdirectory(interface)