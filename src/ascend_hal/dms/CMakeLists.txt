# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
get_filename_component(MODULE_SRC_DIR "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)

add_library(dms_obj OBJECT)

target_include_directories(dms_obj PRIVATE
    ${DRIVER_HAL_INC_DIR}
    ${DRIVER_USER_DIR}/msnpureport/inc/slog
    ${DRIVER_USER_DIR}/dmc/device_monitor/include
    ${DRIVER_USER_DIR}/inc
    ${DRIVER_USER_DIR}/inc/mmpa_sdk
    ${DRIVER_KERNEL_DIR}/drv_memory/hbm_isolation
    ${DRIVER_KERNEL_DIR}/tsdrv/ts_drv/ts_drv_common
    ${DRIVER_KERNEL_DIR}/pbl/dev_urd/command/ioctl
    ${DRIVER_KERNEL_DIR}/dms/command/ioctl
    ${DRIVER_KERNEL_DIR}/pbl/uda/command/ioctl
    ${MODULE_SRC_DIR}/common
    ${MODULE_SRC_DIR}/include
    ${DRIVER_USER_DIR}/inc/dms
    ${DRIVER_USER_DIR}/inc/pbl
    ${DRIVER_USER_DIR}/dms/drv_devmng
    ${DRIVER_USER_DIR}/dms/drv_devmng/ascend910
    $<$<STREQUAL:${ENABLE_UBE},true>:/usr/include/ub/umdk/urma>
)

target_link_libraries(dms_obj PRIVATE
    $<BUILD_INTERFACE:c_sec_headers>
    $<BUILD_INTERFACE:intf_pub>
)

target_compile_options(dms_obj PRIVATE
    -fPIC
    -fno-common
    -Wfloat-equal
    -Wextra
)

target_compile_definitions(dms_obj PRIVATE
    __USE_GNU
    _GNU_SOURCE
)

add_subdirectory(bbox)
add_subdirectory(board)
add_subdirectory(chip)
add_subdirectory(common)
add_subdirectory(fault)
add_subdirectory(fault_inject)
add_subdirectory(feature/host)
add_subdirectory(time)
add_subdirectory(vdev)
add_subdirectory(power)
add_subdirectory(qos)
add_subdirectory(hbm)
add_subdirectory(lpm)
add_subdirectory(emmc)
add_subdirectory(pcie)
add_subdirectory(log)
add_subdirectory(sils)
add_subdirectory(ub)
add_subdirectory(p2p)
add_subdirectory(flash)
add_subdirectory(communication)
add_subdirectory(udis)
add_subdirectory(drv_devmng)
add_subdirectory(dc)

# configuring module feature
include(${MODULE_SRC_DIR}/feature/host/module.cmake)

# configuring product feature
include(${MODULE_SRC_DIR}/feature/host/${PRODUCT}.cmake)