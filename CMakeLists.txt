# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.16)
project(npu_driver LANGUAGES C CXX ASM)
if (ENABLE_TEST)
    enable_testing()
    add_subdirectory(test)
    return()
endif()

set(MAKE_CMD $(MAKE))
include(cmake/intf_pub_linux.cmake)
include(cmake/function.cmake)
include(cmake/driver_config.cmake)
include(cmake/feature_config.cmake)
include(cmake/create_ko_target.cmake)
include(cmake/external_dependencies.cmake)

# 提前拉取 driver-device，供编译阶段链接
if(BUILD_COMPONENT STREQUAL "DRIVER")
    include(cmake/third_party/driver_device_fetch.cmake)
    get_driver_device()
endif()

# path for so
set(DRIVER_HAL_INC_DIR ${CMAKE_CURRENT_LIST_DIR}/pkg_inc)
set(DRIVER_USER_DIR ${CMAKE_CURRENT_LIST_DIR}/src/ascend_hal)
set(DRIVER_KERNEL_DIR ${CMAKE_CURRENT_LIST_DIR}/src/sdk_driver)
set(DRIVER_NETWORK_DIR ${CMAKE_CURRENT_LIST_DIR}/src/custom/network)
set(DRIVER_CUST_INC_DIR ${CMAKE_CURRENT_LIST_DIR}/src/custom/include)
set(DRIVER_CUST_DIR ${CMAKE_CURRENT_LIST_DIR}/src/custom)

# path for ko
set(MAKE_ARGS
    DRIVER_HAL_INC_DIR=${CMAKE_CURRENT_LIST_DIR}/pkg_inc
    DRIVER_USER_DIR=${CMAKE_CURRENT_LIST_DIR}/src/ascend_hal
    DRIVER_KERNEL_DIR=${CMAKE_CURRENT_LIST_DIR}/src/sdk_driver
    DRIVER_CUST_INC_DIR=${CMAKE_CURRENT_LIST_DIR}/src/custom/include
    DRIVER_CUST_DIR=${CMAKE_CURRENT_LIST_DIR}/src/custom

    C_SEC_INCLUDE=${C_SEC_INCLUDE}
    #to be del
    PRODUCT_SIDE=host
)
add_subdirectory(src)

# build run package
if(ENABLE_PACKAGE)
    include(cmake/package.cmake)
    pack_built_in()
endif()
