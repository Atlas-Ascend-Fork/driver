# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
# build libascend_hal.so

if(${PRODUCT} STREQUAL ascend950)
    if(DEFINED CMAKE_BUILD_TYPE AND NOT ${CMAKE_BUILD_TYPE} STREQUAL Debug)
        set(_hal_visibility_opt -fvisibility=hidden)
    endif()
endif()

add_subdirectory(build)

add_subdirectory(bbox)
add_subdirectory(esched)
add_subdirectory(hdc)
add_subdirectory(queue)
add_subdirectory(buff)
add_subdirectory(dmc)
add_subdirectory(pbl)
add_subdirectory(dms)
add_subdirectory(svm)
add_subdirectory(dpa)
add_subdirectory(mmpa)
add_subdirectory(trs)
add_subdirectory(comm)
add_subdirectory(msnpureport)

if(${PRODUCT} STREQUAL ascend910B)
    add_subdirectory(roce)
endif()

if(_hal_visibility_opt)
    foreach(_hal_obj IN ITEMS
        urd_obj
        devdrv_obj
        dms_obj
        prof_obj
        eventsched_obj
        hdc_obj
        queue_obj
        commlib_obj
        uda_obj
        uda_base_obj
        trs_drv_obj
        rbtree_obj
        drv_hash_obj
        devmm_obj
        drvaibuff_obj
        drvaibuff_base_obj
        apm_obj
        rmo_obj
        mem_prof_obj
        drv_log_obj
        drv_log_base_obj
        queryfeature_obj
        ubmem_dev_obj
        dp_proc_mng_obj
        drv_event_obj
        ascend_urma_adapt_obj)
        if(TARGET ${_hal_obj})
            target_compile_options(${_hal_obj} PRIVATE ${_hal_visibility_opt})
        endif()
    endforeach()
endif()