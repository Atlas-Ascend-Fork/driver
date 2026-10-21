# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
#########libmsn_adcore
set(adcoreHeaders
    ${CMAKE_CURRENT_LIST_DIR}/../../inc/adump
    ${CMAKE_CURRENT_LIST_DIR}/../inc/adump
    ${CMAKE_CURRENT_LIST_DIR}/../inc/slog
    ${CMAKE_CURRENT_LIST_DIR}/
    ${CMAKE_CURRENT_LIST_DIR}/epoll
    ${CMAKE_CURRENT_LIST_DIR}/commopts
    ${CMAKE_CURRENT_LIST_DIR}/common
    ${CMAKE_CURRENT_LIST_DIR}/protocol
    ${CMAKE_CURRENT_LIST_DIR}/component
    ${CMAKE_CURRENT_LIST_DIR}/device
    ${CMAKE_CURRENT_LIST_DIR}/log
    ${CMAKE_CURRENT_LIST_DIR}/hdc

    ${LIBC_SEC_HEADER}
    ${CMAKE_CURRENT_LIST_DIR}/../../../../pkg_inc
)

set(adcoreSrcList
    ${CMAKE_CURRENT_LIST_DIR}/hdc/hdc_api.cpp
    ${CMAKE_CURRENT_LIST_DIR}/adcore_api.cpp
    ${CMAKE_CURRENT_LIST_DIR}/device/adx_dsmi.cpp
    ${CMAKE_CURRENT_LIST_DIR}/device/adx_device.cpp
    ${CMAKE_CURRENT_LIST_DIR}/device/adx_hdc_device.cpp
    ${CMAKE_CURRENT_LIST_DIR}/component/adx_server_manager.cpp
    ${CMAKE_CURRENT_LIST_DIR}/component/adx_common_component.cpp
    ${CMAKE_CURRENT_LIST_DIR}/component/adx_component_api_c.cpp
    ${CMAKE_CURRENT_LIST_DIR}/epoll/adx_hdc_epoll.cpp
    ${CMAKE_CURRENT_LIST_DIR}/commopts/adx_comm_opt_manager.cpp
    ${CMAKE_CURRENT_LIST_DIR}/commopts/hdc_comm_opt.cpp
    ${CMAKE_CURRENT_LIST_DIR}/common/thread.cpp
    ${CMAKE_CURRENT_LIST_DIR}/common/thread_comm.cpp
    ${CMAKE_CURRENT_LIST_DIR}/common/memory_utils.cpp
    ${CMAKE_CURRENT_LIST_DIR}/protocol/adx_msg_proto.cpp
    ${CMAKE_CURRENT_LIST_DIR}/common/file_utils.cpp
    ${CMAKE_CURRENT_LIST_DIR}/common/string_utils.cpp
    ${CMAKE_CURRENT_LIST_DIR}/adx_api.cpp
    ${CMAKE_CURRENT_LIST_DIR}/common/create_func.cpp
    ${CMAKE_CURRENT_LIST_DIR}/component/server_register.cpp
)

add_library(msn_adcore STATIC
    ${adcoreSrcList}
)

target_include_directories(msn_adcore PRIVATE ${adcoreHeaders})

target_compile_options(msn_adcore PRIVATE
    $<$<NOT:$<STREQUAL:${TARGET_SYSTEM_NAME},Windows>>:-Werror>
    $<$<NOT:$<STREQUAL:${TARGET_SYSTEM_NAME},Windows>>:-Wextra>
    $<$<NOT:$<STREQUAL:${TARGET_SYSTEM_NAME},Windows>>:-Wfloat-equal>
    -std=c++11
    -fstack-protector-strong
    -fno-common
    -fno-strict-aliasing
    -fvisibility=hidden
    $<$<AND:$<STREQUAL:${TARGET_SYSTEM_NAME},Windows>,$<STREQUAL:${CMAKE_CONFIGURATION_TYPES},Debug>>:/MTd>
    $<$<AND:$<STREQUAL:${TARGET_SYSTEM_NAME},Windows>,$<STREQUAL:${CMAKE_CONFIGURATION_TYPES},Release>>:/MT>
    $<$<CONFIG:Debug>:-D_FORTIFY_SOURCE=2 -Os -ftrapv>
    $<$<CONFIG:Release>:-D_FORTIFY_SOURCE=2 -Os>
)

target_link_options(msn_adcore PRIVATE
    -Wl,-z,relro,-z,now,-z,noexecstack
    -Wl,-Bsymbolic
    -Wl,--exclude-libs,ALL
)

target_compile_definitions(msn_adcore PRIVATE
    $<IF:$<STREQUAL:${PRODUCT_SIDE},host>,ADX_LIB_HOST_DRV,ADX_LIB>
    $<$<STREQUAL:${PRODUCT_SIDE},device>:ADX_LIB_DEVICE_DRV>
    $<$<NOT:$<STREQUAL:${TARGET_SYSTEM_NAME},Windows>>:OS_TYPE=0>
    $<$<STREQUAL:${TARGET_SYSTEM_NAME},Windows>:OS_TYPE=1>
    $<$<STREQUAL:${TARGET_SYSTEM_NAME},Windows>:SECUREC_USING_STD_SECURE_LIB=0>
)

if(${TARGET_SYSTEM_NAME} STREQUAL "Windows")
target_link_libraries(msn_adcore PRIVATE
    $<BUILD_INTERFACE:intf_pub>
    # $<BUILD_INTERFACE:mmpa_headers>
    # $<BUILD_INTERFACE:adump_headers>
    # $<BUILD_INTERFACE:slog_headers>
    -Wl,--no-as-needed
    c_sec
    mmpa
    -Wl,--as-needed
    ascend_hal
)
else()
target_link_libraries(msn_adcore PRIVATE
    $<BUILD_INTERFACE:intf_pub>
    # $<BUILD_INTERFACE:mmpa_headers>
    # $<BUILD_INTERFACE:adump_headers>
    # $<BUILD_INTERFACE:slog_headers>
    -Wl,--no-as-needed
    c_sec
    mmpa
    -Wl,--as-needed
    $<$<STREQUAL:${PRODUCT_SIDE},host>:ascend_hal>
    $<$<STREQUAL:${PRODUCT_SIDE},device>:ascend_hal>
    $<$<STREQUAL:${PRODUCT_SIDE},device>:unified_dlog>
    $<$<AND:$<NOT:$<BOOL:${PRODUCT}>>,$<STREQUAL:${PRODUCT_SIDE},host>>:unified_dlog>
)
endif()
