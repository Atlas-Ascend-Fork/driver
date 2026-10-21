# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
add_library(hdc_obj OBJECT)

get_filename_component(BASE_DIR "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
target_include_directories(hdc_obj PRIVATE
    ${BASE_DIR}
    ${C_SEC_INCLUDE}/
    ${DRIVER_HAL_INC_DIR}
    ${DRIVER_USER_DIR}/msnpureport/inc/slog/
    ${DRIVER_USER_DIR}/pbl/commlib
    ${DRIVER_USER_DIR}/inc
    ${DRIVER_USER_DIR}/inc/dmc
    ${DRIVER_USER_DIR}/inc/mmpa_sdk
    ${DRIVER_USER_DIR}/inc/common
    ${DRIVER_USER_DIR}/buff/dc/comm/lib/
    ${DRIVER_USER_DIR}/hdc/dc
    ${DRIVER_KERNEL_DIR}/hdc/command/ioctl
    ${DRIVER_KERNEL_DIR}/inc/
    ${DRIVER_USER_DIR}/hdc/inc/
    ${DRIVER_USER_DIR}/hdc/common/
    ${DRIVER_USER_DIR}/hdc/pcie/
    ${DRIVER_USER_DIR}/inc/dms
    ${DRIVER_KERNEL_DIR}/dms/command/ioctl
    ${DRIVER_USER_DIR}/hdc/common/hdc_file/host
    $<$<STREQUAL:${ENABLE_UBE},true>:/usr/include/ub/umdk/urma>
)

target_sources(hdc_obj PRIVATE
    pcie/hdc_pcie_epoll.c
    pcie/hdc_pcie_drv.c
    sock/hdc_sock_epoll.c
    sock/hdc_ppc.c
    common/hdc_file/host/hdc_file.c
)

target_compile_definitions(hdc_obj PRIVATE
    DRV_HOST
    HDC_HOST
    CFG_ENV_HOST
    CFG_FEATURE_SHARE_LOG
    CFG_FEATURE_SRIOV
    $<$<STREQUAL:${PRODUCT},ascend910B>:CFG_FEATURE_PM_MONITOR>

    $<$<AND:$<STREQUAL:${PRODUCT},ascend950>,$<STREQUAL:${ENABLE_UBE},true>,$<STREQUAL:${CMAKE_HOST_SYSTEM_PROCESSOR},aarch64>>:CFG_FEATURE_SUPPORT_UB>
    $<$<AND:$<STREQUAL:${PRODUCT},ascend950>,$<STREQUAL:${CMAKE_HOST_SYSTEM_PROCESSOR},aarch64>>:SSAPI_USE_MAMI>
)

target_link_options(hdc_obj PRIVATE
    $<$<CONFIG:Release>:-Wl,--build-id=none>
    $<$<CONFIG:Release>:-s>
)

target_link_libraries(hdc_obj PRIVATE
    $<BUILD_INTERFACE:intf_pub>
)

add_subdirectory(common)
add_subdirectory(dc)

if(${ENABLE_UBE})
    add_subdirectory(ub)
endif()
