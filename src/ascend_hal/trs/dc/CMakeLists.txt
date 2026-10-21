# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
get_filename_component(BASE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
add_library(trs_drv_obj OBJECT)

set(TRS_SRC_FILES
    ${BASE_DIR}/core/trs_cb_event.c
    ${BASE_DIR}/core/trs_dev_drv.c
    ${BASE_DIR}/core/trs_res.c
    ${BASE_DIR}/core/trs_sqcq.c
    ${BASE_DIR}/core/trs_interface.c
    ${BASE_DIR}/shr_id/trs_shr_id_fd.c
    ${BASE_DIR}/shr_id/trs_shr_id_user.c
)

list(APPEND TRS_SRC_FILES
    $<$<OR:$<STREQUAL:${PRODUCT},ascend910B>,$<STREQUAL:${PRODUCT},ascend950>>:${BASE_DIR}/shr_id/trs_shr_id_spod_user.c>
    $<$<OR:$<STREQUAL:${PRODUCT},ascend910B>,$<STREQUAL:${PRODUCT},ascend950>>:${BASE_DIR}/remote/master/trs_master_event.c>
    $<$<STREQUAL:${PRODUCT},ascend950>:${BASE_DIR}/remote/master/trs_sq_mem_raw.c>
    $<$<STREQUAL:${PRODUCT},ascend950>:${BASE_DIR}/remote/master/trs_sq_mem_cache.c>
    $<$<STREQUAL:${PRODUCT},ascend950>:${BASE_DIR}/remote/master/trs_sqcq_mem.c>
)

if (${ENABLE_UBE})
    list(APPEND TRS_SRC_FILES
        $<$<STREQUAL:${PRODUCT},ascend950>:${BASE_DIR}/core/urma/trs_urma.c>
        $<$<STREQUAL:${PRODUCT},ascend950>:${BASE_DIR}/core/urma/master/trs_master_urma.c>
        $<$<STREQUAL:${PRODUCT},ascend950>:${BASE_DIR}/core/urma/master/trs_master_async.c>
    )
endif()

target_sources(trs_drv_obj PRIVATE
    ${TRS_SRC_FILES}
)

target_include_directories(trs_drv_obj PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${BASE_DIR}
    ${BASE_DIR}/inc
    ${BASE_DIR}/core
    ${BASE_DIR}/shr_id
    ${BASE_DIR}/remote/master
    ${BASE_DIR}/core/urma/master
    ${BASE_DIR}/core/urma
    ${DRIVER_USER_DIR}/inc
    ${DRIVER_USER_DIR}/inc/pbl
    ${DRIVER_USER_DIR}/inc/trs
    ${DRIVER_USER_DIR}/pbl/commlib
    ${DRIVER_USER_DIR}/buff/dc/comm/lib
    ${DRIVER_USER_DIR}/msnpureport/inc/slog/
    ${C_SEC_INCLUDE}/
    ${DRIVER_HAL_INC_DIR}

    ${DRIVER_KERNEL_DIR}/trsdrv/trs/shr_id/command/ioctl
    ${DRIVER_KERNEL_DIR}/trsdrv/trs/trs_core/command/ioctl
    ${DRIVER_KERNEL_DIR}/pbl/dev_urd/command/ioctl
    ${DRIVER_KERNEL_DIR}/dms/command/ioctl
    ${DRIVER_USER_DIR}/inc/dms
    ${DRIVER_USER_DIR}/trs/core/command/msg
    ${DRIVER_USER_DIR}/buff/dc/comm/lib
    ${DRIVER_USER_DIR}/dmc/device_monitor/lib/include

    $<$<STREQUAL:${ENABLE_UBE},true>:/usr/include/ub/umdk/urma>
    $<$<STREQUAL:${ENABLE_UBE},true>:/usr/include/ub/umdk/urma/udma>
)

target_link_libraries(trs_drv_obj PRIVATE
    $<BUILD_INTERFACE:intf_pub>
    -ldl
    -Wl,--Bsymbolic
)

target_compile_definitions(trs_drv_obj PRIVATE
    $<$<STREQUAL:${CMAKE_BUILD_TYPE},Debug>:CFG_BUILD_DEBUG>
    $<$<STREQUAL:${PRODUCT_SIDE},host>:DRV_HOST>
    $<$<STREQUAL:${PRODUCT_SIDE},host>:PCIE_HOST>
    $<$<STREQUAL:${PRODUCT_SIDE},host>:CFG_MANAGER_HOST_ENV>
    $<$<STREQUAL:${PRODUCT},ascend910B>:CFG_SOC_PLATFORM_CLOUD>
    $<$<STREQUAL:${PRODUCT},ascend910B>:CFG_SOC_PLATFORM_CLOUD_V2>
    $<$<STREQUAL:${PRODUCT},ascend910B>:CFG_FEATURE_RUNTIME_NO_THREAD>
    $<$<STREQUAL:${PRODUCT},ascend910B>:CFG_FEATURE_CALLBACK_EVENT>
    $<$<STREQUAL:${PRODUCT},ascend910B>:CFG_FEATURE_SUPPORT_STREAM_TASK>
    # 950
    $<$<STREQUAL:${PRODUCT},ascend950>:CFG_SOC_PLATFORM_CLOUD>
    $<$<STREQUAL:${PRODUCT},ascend950>:CFG_SOC_PLATFORM_CLOUD_V4>
    $<$<STREQUAL:${PRODUCT},ascend950>:CFG_FEATURE_EVENT_ESCHED_ACTIVE_STREAM>
    $<$<STREQUAL:${PRODUCT},ascend950>:CFG_FEATURE_SQCQ_RESET>
    $<$<STREQUAL:${PRODUCT},ascend950>:CFG_FEATURE_SQ_SEND_LOCK>
    $<$<STREQUAL:${PRODUCT},ascend950>:CFG_FEATURE_RUNTIME_NO_THREAD>
    $<$<STREQUAL:${PRODUCT},ascend950>:CFG_FEATURE_CALLBACK_EVENT>
    $<$<STREQUAL:${PRODUCT},ascend950>:SSAPI_USE_MAMI>
    $<$<STREQUAL:${PRODUCT},ascend950>:CFG_FEATURE_SUPPORT_STREAM_TASK>
    $<$<STREQUAL:${ENABLE_UBE},true>:CFG_FEATURE_SUPPORT_UB>
)
