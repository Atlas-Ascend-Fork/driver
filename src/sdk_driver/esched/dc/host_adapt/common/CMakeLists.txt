# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
add_library(asdrv_esched_obj OBJECT)
set(DEVICE_LOCAL_MODULE asdrv_esched)
set(TARGET_KO_DIRECTORY ${CMAKE_INSTALL_PREFIX}/lib)
target_sources(asdrv_esched_obj PRIVATE
    esched.c
    esched_fops.c
    esched_sysfs.c
    esched_feature_check.c
    ../bind_cpu/drv_cpu_type.c
)

target_compile_definitions(asdrv_esched_obj PRIVATE CFG_FEATURE_MORE_PID_PRIORITY)
target_compile_definitions(asdrv_esched_obj PRIVATE CFG_FEATURE_SCHEDULE_REALTIME)
target_compile_definitions(asdrv_esched_obj PRIVATE CFG_FEATURE_TIMEOUT_PROCESS)
target_compile_definitions(asdrv_esched_obj PRIVATE CFG_FEATURE_TRACE_RECOED)
target_compile_definitions(asdrv_esched_obj PRIVATE CFG_FEATURE_THREAD_SWAPOUT)

target_link_libraries(asdrv_esched_obj PRIVATE $<BUILD_INTERFACE:utils_intf_pub>)

target_include_directories(asdrv_esched_obj PRIVATE
    ${DRIVER_KERNEL_DIR}/src/event_sched/device/comm/
    ${DRIVER_KERNEL_DIR}/src/event_sched/device/bind_cpu/
    ${DRIVER_KERNEL_DIR}/inc/
    ${DRIVER_KERNEL_DIR}/inc/trs
    ${DRIVER_KERNEL_DIR}/src/esched/command/ioctl/
    ${DRIVER_KERNEL_DIR}/src/esched/command/msg/
    ${TOP_DIR}/inc/driver/
    ${TOP_DIR}/inc/driver/errcode/
    ${TOP_DIR}/inc/common/
    ${TOP_DIR}/abl/libc_sec/include
    ${TOP_DIR}/driver/driver/src/sdk_driver/kernel_adapt/include
)

add_custom_command(
    OUTPUT ${DEVICE_LOCAL_MODULE}.ko
    COMMAND ${CMAKE_LINKER} -r $<TARGET_OBJECTS:asdrv_esched_obj> -o ${DEVICE_LOCAL_MODULE}.ko
    DEPENDS asdrv_esched_obj
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND_EXPAND_LISTS
)
add_custom_target(asdrv_esched ALL DEPENDS asdrv_esched_obj ${DEVICE_LOCAL_MODULE}.ko)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${DEVICE_LOCAL_MODULE}.ko
            DESTINATION ${CMAKE_INSTALL_PREFIX}/lib OPTIONAL)
