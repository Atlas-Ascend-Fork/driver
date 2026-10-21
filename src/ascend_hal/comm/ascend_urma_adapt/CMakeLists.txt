# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
# Define common val
get_filename_component(ASCEND_URMA_ADAPT_DIR "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)

set(ASCEND_URMA_ADAPT_SRC_FILES)
set(ASCEND_URMA_ADAPT_SRC_INC_DIRS)
set(ASCEND_URMA_ADAPT_COMPILE_DEFINITIONS)

# Collect srcs, dirs, compile_definitions
if(${PRODUCT_SIDE} STREQUAL host)
    if(${ENABLE_UBE})
        file(GLOB ASCEND_URMA_ADAPT_SRC_FILES
            ${ASCEND_URMA_ADAPT_DIR}/*.c
        )

        list(APPEND ASCEND_URMA_ADAPT_SRC_INC_DIRS
            /usr/include/ub/umdk/urma/

            ${DRIVER_USER_DIR}/inc/

            ${ASCEND_URMA_ADAPT_DIR}/
        )
    else ()

    endif ()
else ()
    file(GLOB ASCEND_URMA_ADAPT_SRC_FILES
        ${ASCEND_URMA_ADAPT_DIR}/*.c
    )

    list(APPEND ASCEND_URMA_ADAPT_SRC_INC_DIRS
        ${TOP_DIR}/ubengine/ssapi/kernelspace/urma/code/include
        ${TOP_DIR}/ubengine/ssapi/kernelspace/urma/code/lib/urma/include

        ${DRIVER_USER_DIR}/inc/

        ${ASCEND_URMA_ADAPT_DIR}/
    )
endif ()

# Create obj
add_library(ascend_urma_adapt_obj OBJECT)

target_sources(ascend_urma_adapt_obj PRIVATE
    ${ASCEND_URMA_ADAPT_SRC_FILES}
)

target_include_directories(ascend_urma_adapt_obj PRIVATE
    ${C_SEC_INCLUDE}/
    ${DRIVER_HAL_INC_DIR}
    ${DRIVER_USER_DIR}/msnpureport/inc/slog

    ${DRIVER_USER_DIR}/inc
    ${DRIVER_USER_DIR}/inc/common
    ${DRIVER_USER_DIR}/inc/
    ${DRIVER_USER_DIR}/dmc/device_monitor/lib/include
    ${DRIVER_USER_DIR}/pbl/commlib
    ${DRIVER_USER_DIR}/inc/dms
    ${DRIVER_KERNEL_DIR}/dms/command/ioctl

    ${ASCEND_URMA_ADAPT_SRC_INC_DIRS}
)

target_compile_definitions(ascend_urma_adapt_obj PRIVATE
    $<$<STREQUAL:${PRODUCT_SIDE},host>:DRV_HOST>                     # For drv_user_log.h
    ${ASCEND_URMA_ADAPT_COMPILE_DEFINITIONS}
    $<$<AND:$<STREQUAL:${PRODUCT},ascend960>,$<STREQUAL:${CMAKE_HOST_SYSTEM_PROCESSOR},aarch64>>:SSAPI_USE_MAMI>
    $<$<AND:$<STREQUAL:${PRODUCT},ascend960esl>,$<STREQUAL:${CMAKE_HOST_SYSTEM_PROCESSOR},aarch64>>:SSAPI_USE_MAMI>
    $<$<AND:$<STREQUAL:${PRODUCT},ascend950>,$<STREQUAL:${CMAKE_HOST_SYSTEM_PROCESSOR},aarch64>>:SSAPI_USE_MAMI>
)

target_link_libraries(ascend_urma_adapt_obj PRIVATE
    $<BUILD_INTERFACE:intf_pub>
)

target_compile_options(ascend_urma_adapt_obj PRIVATE
    -Werror
    -funsigned-char
    -Wextra
    -Wformat=2
    -Wstack-usage=2048
    -Wcast-align
    -Wfloat-equal
    -Wswitch-default
    -Wshadow
    -Wvla
    -Wundef
    -Wno-unused-parameter
    -Wno-missing-field-initializers
    $<$<VERSION_GREATER:${CMAKE_C_COMPILER_VERSION},4.8.5>: -Wdate-time>
)

set(INSTALL_RUNTIME_DIR bin)

