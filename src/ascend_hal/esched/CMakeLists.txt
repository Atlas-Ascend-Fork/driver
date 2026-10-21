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
add_library(eventsched_obj OBJECT)

target_sources(eventsched_obj PRIVATE
    comm/event_sched_app.c
    comm/event_sched.c
    comm/esched_feature.c
    esched_adapt.c
    esched_host_adapt.c
)

# The apm module providing halQueryMasterPidByDeviceSlave is only built for ascend950,
# so a stub is compiled in for other products to keep the link resolvable.
if (NOT ${PRODUCT} STREQUAL ascend950)
target_sources(eventsched_obj PRIVATE
    esched_host_adapt_before_v4.c
)
endif ()

if (${PRODUCT} STREQUAL ascend950)
    if( ${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL aarch64)
        if(${ENABLE_UBE})
            set(support_ub "ascend950")
        endif ()
    endif ()
endif ()

if (${PRODUCT} IN_LIST support_ub)
target_sources(eventsched_obj PRIVATE
    esched_topic_sqe.c
)
endif ()

target_include_directories(eventsched_obj PRIVATE
    ${BASE_DIR}
    ${BASE_DIR}/comm
    ${DRIVER_HAL_INC_DIR}
    ${DRIVER_USER_DIR}/inc
    ${DRIVER_USER_DIR}/inc/pbl
    ${DRIVER_KERNEL_DIR}/dpa/apm/command/ioctl
    ${DRIVER_KERNEL_DIR}/esched/command/ioctl
    ${DRIVER_KERNEL_DIR}/esched/command/msg
    ${DRIVER_USER_DIR}/esched/command
    ${C_SEC_INCLUDE}
    ${DRIVER_USER_DIR}/msnpureport/inc/slog
    ${DRIVER_KERNEL_DIR}/dms/command/ioctl
)

target_link_libraries(eventsched_obj PRIVATE
    $<BUILD_INTERFACE:intf_pub>
)

target_compile_definitions(eventsched_obj PRIVATE
    CFG_ENV_HOST
    DRV_HOST
    $<$<STREQUAL:${PRODUCT},ascend950>:CFG_SOC_PLATFORM_CLOUD_V4>
    CFG_FEATURE_EXTERNAL_CDEV
)

add_library(drv_event_obj OBJECT)

target_sources(drv_event_obj PRIVATE
    comm/drv_event_proc.c
)

target_include_directories(drv_event_obj PRIVATE
    ${C_SEC_INCLUDE}
    ${DRIVER_USER_DIR}/inc
    ${DRIVER_HAL_INC_DIR}
    ${DRIVER_KERNEL_DIR}/src/dpa/apm/command/ioctl
    ${DRIVER_KERNEL_DIR}/src/esched/command/ioctl
    ${DRIVER_KERNEL_DIR}/src/esched/command/msg
    ${DRIVER_USER_DIR}/msnpureport/inc/slog
)

target_link_libraries(drv_event_obj PRIVATE
    $<BUILD_INTERFACE:intf_pub>
)

target_compile_definitions(drv_event_obj PRIVATE
    DRV_HOST
)