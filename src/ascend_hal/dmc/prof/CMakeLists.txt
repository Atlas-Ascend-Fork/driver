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
set(product_support_prof_v4 ascend950 ascend350)
string(REPLACE ";" "$<SEMICOLON>" product_support_prof_v4_gen "${product_support_prof_v4}")

if(${PRODUCT} IN_LIST product_support_prof_v4)
    add_subdirectory(mem_prof)
endif ()

if (${PRODUCT} IN_LIST product_support_prof_v4)
    if(${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL aarch64)
        if ((${ENABLE_UBE}))
            set(product_support_urma_gen ${product_support_prof_v4_gen})
        endif ()
    endif ()
endif ()

add_library(prof_obj OBJECT)

file(GLOB PROF_TS_SAMPLE_SRC_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/../prof_sample/ts_sample/*.c
)

target_sources(prof_obj PRIVATE
    prof_interface.c
    ./core/prof_core.c
    ./core/prof_poll.c
    ./core/prof_chan.c
    ./core/prof_chan_list.c
    ./adapt/prof_adapt.c
    ./adapt/prof_buff.c
    ./adapt/user/prof_adapt_user.c
    $<$<AND:$<STREQUAL:${PRODUCT_SIDE},host>,$<NOT:$<IN_LIST:${PRODUCT},${product_support_prof_v4_gen}>>>:${CMAKE_CURRENT_SOURCE_DIR}/adapt/prof_host_adapt.c>
    $<$<AND:$<STREQUAL:${PRODUCT_SIDE},host>,$<IN_LIST:${PRODUCT},${product_support_prof_v4_gen}>>:${CMAKE_CURRENT_SOURCE_DIR}/adapt/prof_host_adapt_v4.c>
    ./comm/prof_communication.c
    $<$<NOT:$<IN_LIST:${PRODUCT},${product_not_support_event}>>:${CMAKE_CURRENT_SOURCE_DIR}/comm/event/prof_event_master.c>
    $<$<NOT:$<IN_LIST:${PRODUCT},${product_not_support_event}>>:${CMAKE_CURRENT_SOURCE_DIR}/comm/event/prof_event_comm.c>
    ${CMAKE_CURRENT_SOURCE_DIR}/adapt/kernel/prof_adapt_h2d.c
    ${CMAKE_CURRENT_SOURCE_DIR}/adapt/kernel/prof_adapt_hdc.c
    $<$<IN_LIST:${PRODUCT},${product_support_urma_gen}>:${CMAKE_CURRENT_SOURCE_DIR}/adapt/kernel/prof_adapt_urma.c>
    ${CMAKE_CURRENT_SOURCE_DIR}/comm/hdc/prof_hdc_comm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/comm/hdc/prof_hdc.c
    $<$<IN_LIST:${PRODUCT},${product_support_urma_gen}>:${CMAKE_CURRENT_SOURCE_DIR}/comm/urma/prof_urma_comm.c>
    $<$<IN_LIST:${PRODUCT},${product_support_urma_gen}>:${CMAKE_CURRENT_SOURCE_DIR}/comm/urma/prof_urma.c>
    $<$<STREQUAL:${PRODUCT_SIDE},device>:${CMAKE_CURRENT_SOURCE_DIR}/adapt/user/prof_adapt_user_ioctl.c>
    $<$<STREQUAL:${PRODUCT_SIDE},device>:${CMAKE_CURRENT_SOURCE_DIR}/adapt/kernel/prof_adapt_ioctl.c>
    $<$<STREQUAL:${PRODUCT_SIDE},device>:${CMAKE_CURRENT_SOURCE_DIR}/comm/ioctl/prof_ioctl.c>
    $<$<AND:$<STREQUAL:${PRODUCT_SIDE},device>,$<NOT:$<IN_LIST:${PRODUCT},${product_not_support_event}>>>:${CMAKE_CURRENT_SOURCE_DIR}/comm/event/prof_event_agent.c>
    $<$<IN_LIST:${PRODUCT},${product_support_prof_v4_gen}>:${CMAKE_CURRENT_SOURCE_DIR}/adapt/host_sample/prof_adapt_host.c>
    $<$<IN_LIST:${PRODUCT},${product_support_prof_v4_gen}>:${CMAKE_CURRENT_SOURCE_DIR}/../prof_sample/prof_sample.c>
    # Keep the generator expression quoted, as PROF_TS_SAMPLE_SRC_FILES may contain multiple files separated by ';'.
    "$<$<IN_LIST:${PRODUCT},${product_support_prof_v4_gen}>:${PROF_TS_SAMPLE_SRC_FILES}>"
    $<$<IN_LIST:${PRODUCT},${product_support_prof_v4_gen}>:${CMAKE_CURRENT_SOURCE_DIR}/../prof_sample/prof_sample_aicpu_channel.c>
)

target_include_directories(prof_obj PRIVATE
    ${BASE_DIR}
    ${BASE_DIR}/core/
    ${BASE_DIR}/adapt/
    ${BASE_DIR}/adapt/user/
    ${BASE_DIR}/adapt/kernel/
    ${BASE_DIR}/comm/
    ${BASE_DIR}/comm/event/
    ${BASE_DIR}/command/
    ${BASE_DIR}/comm/hdc/
    $<$<IN_LIST:${PRODUCT},${product_support_urma_gen}>:${BASE_DIR}/comm/urma/>
    $<$<IN_LIST:${PRODUCT},${product_support_urma_gen}>:/usr/include/ub/umdk/urma>
    $<$<STREQUAL:${PRODUCT_SIDE},device>:${BASE_DIR}/comm/ioctl/>
    ${DRIVER_HAL_INC_DIR}
    ${C_SEC_INCLUDE}
    ${DRIVER_USER_DIR}/inc
    ${DRIVER_KERNEL_DIR}/dmc/prof/command/ioctl
    ${DRIVER_KERNEL_DIR}/dmc/prof/command/msg
    ${DRIVER_KERNEL_DIR}/dms/command/ioctl
    $<$<STREQUAL:${IN_BRANCH},trunk_ai>:${DRIVER_USER_DIR}/dms/drv_devmng/ascend910/>
    ${DRIVER_USER_DIR}/inc/dms
    ${DRIVER_USER_DIR}/inc/slog
)

target_compile_definitions(prof_obj PRIVATE
    DRV_HOST
    $<$<IN_LIST:${PRODUCT},${product_support_prof_v4_gen}>:CFG_SOC_PLATFORM_CLOUD_V4>
    $<$<AND:$<IN_LIST:${PRODUCT},${product_support_prof_v4_gen}>,$<STREQUAL:${CMAKE_HOST_SYSTEM_PROCESSOR},aarch64>>:SSAPI_USE_MAMI>
)

target_link_libraries(prof_obj PRIVATE
    $<BUILD_INTERFACE:intf_pub>
)