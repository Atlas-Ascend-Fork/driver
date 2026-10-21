# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------

# 暂时设置为false，待修改build.sh传入
#set(BUILD_CH $ENV{BUILD_CH})
set(BUILD_CH false)
set(BUILD_FLAG COMMERCIAL_FLAG)
set(PROJECT_VERSION $ENV{PROJECT_VERSION})

message(STATUS "BUILD_CH: ${BUILD_CH}")

execute_process(COMMAND bash "-c" "${CMAKE_C_COMPILER} --version | head -n1 | cut -d ' ' -f4" OUTPUT_VARIABLE GCC_VERSION)
message(STATUS ${GCC_VERSION})

if("${BUILD_FLAG}" STREQUAL DT_FLAG OR "${BUILD_FLAG}" STREQUAL COMMERCIAL_FLAG)
    message(STATUS ${BUILD_FLAG})
    if (${GCC_VERSION} STRGREATER 4.9)
        add_compile_options(-Wall -fstack-protector-strong -fPIC -fno-strict-aliasing -D_FORTIFY_SOURCE=2 -O2)
    else()
        add_compile_options(-Wall -fstack-protector-all -fPIC -fno-strict-aliasing -D_FORTIFY_SOURCE=2 -O2)
    endif()
endif()

if (CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
    add_definitions("-DARM")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86")
    add_definitions("-DX86")
endif()

if("${BUILD_CH}" STREQUAL true)
    add_definitions("-DORIENT_CH")
endif()

set(CMAKE_EXE_LINKER_FLAGS "-Wl,-z,relro,-z,now,-z,noexecstack -pie -s ${CMAKE_EXE_LINKER_FLAGS}")
set(CMAKE_SHARED_LINKER_FLAGS "-Wl,-z,relro,-z,now -s ${CMAKE_SHARED_LINKER_FLAGS}")
set(CMAKE_SKIP_RPATH TRUE)
set(CMAKE_VERBOSE_MAKEFILE off)

add_subdirectory(dev_prod/user/dcmi)
if (PRODUCT STREQUAL "ascend950")
    add_subdirectory(dev_prod/user/npucli)
endif()
add_subdirectory(lqdrv/kernel)
add_subdirectory(lqdrv/user)
add_subdirectory(ndr)
add_subdirectory(ops_debug)
add_subdirectory(network)
add_subdirectory(tools)