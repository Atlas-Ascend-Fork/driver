# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
target_sources(devdrv_obj PRIVATE
    devdrv_hccl.c
    devdrv_pcie.c
    devdrv_manager_ai_core.c
    devdrv_manager_dev_info_api.c
    devdrv_manager_fd.c
    devdrv_manager_bbox.c
    devdrv_manager_chip.c
    devdrv_manager_host_pid.c
    devdrv_manager_lp.c
    devdrv_manager_resource.c
    devdrv_manager_system_info.c
    devdrv_manager_topology.c
    devdrv_manager_run_info.c
    devdrv_container.c
    devdrv_common.c
    devdrv_vdev.c
    devdrv_host_flush_cache.c
)

target_include_directories(devdrv_obj PRIVATE
    ${DRIVER_HAL_INC_DIR}
    ${DRIVER_USER_DIR}/msnpureport/inc/slog
    ${DRIVER_HAL_INC_DIR}/depends/hiss
    ${DRIVER_USER_DIR}/inc
    ${DRIVER_USER_DIR}/inc/trs
    ${DRIVER_USER_DIR}/inc/mmpa_sdk
    ${DRIVER_USER_DIR}/tsdrv/
    $<$<STREQUAL:${PRODUCT},ascend910B>:${DRIVER_USER_DIR}/tsdrv/ascend910>
    $<$<STREQUAL:${PRODUCT},ascend950>:${DRIVER_USER_DIR}/tsdrv/ascend910>
    ${DRIVER_USER_DIR}/dms/include
    ${DRIVER_USER_DIR}/dms/common
    ${DRIVER_USER_DIR}/dms/hbm
    ${MODULE_SRC_DIR}/inc
    ${MODULE_SRC_DIR}/ascend910
    $<$<STREQUAL:${PRODUCT},ascend910B>:${MODULE_SRC_DIR}/ascend910/ascd910>
    ${MODULE_SRC_DIR}/dc
    ${DRIVER_KERNEL_DIR}/inc
    ${DRIVER_KERNEL_DIR}/inc/trs
    ${DRIVER_KERNEL_DIR}/pbl/dev_urd/command/ioctl
    ${DRIVER_KERNEL_DIR}/pbl/uda/command/ioctl
    ${DRIVER_KERNEL_DIR}/inc/ascend_platform
    ${DRIVER_KERNEL_DIR}/dpa/apm/command/ioctl
    ${DRIVER_KERNEL_DIR}/dms/command/ioctl
    $<$<STREQUAL:${ENABLE_UBE},true>:/usr/include/ub/umdk/urma>
)

target_link_libraries(devdrv_obj PRIVATE
    $<BUILD_INTERFACE:c_sec_headers>
)

target_compile_options(devdrv_obj PRIVATE
    -fno-common
    -Wfloat-equal
    -Wextra
)

add_subdirectory(devdrv_dsmi)

if(${PRODUCT} STREQUAL ascend910B OR ${PRODUCT} STREQUAL ascend950)
   add_subdirectory(ascd910)
else()
   message(FATAL_ERROR "Cannot find product: ${PRODUCT}.")
endif()
