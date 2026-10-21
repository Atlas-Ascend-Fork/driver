# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------

message("Build third party library mockcpp")

unset(MOCKCPP_FOUND CACHE)
unset(MOCKCPP_INCLUDE CACHE)
unset(MOCKCPP_STATIC_LIBRARY CACHE)

set(MOCKCPP_INSTALL_PATH ${TESTCASE_3RD_LIB_PATH}/mockcpp)

find_path(MOCKCPP_INCLUDE
        NAMES mockcpp/mockcpp.hpp
        NO_CMAKE_SYSTEM_PATH
        NO_CMAKE_FIND_ROOT_PATH
        PATHS ${MOCKCPP_INSTALL_PATH}/include)
find_library(MOCKCPP_STATIC_LIBRARY
        NAMES libmockcpp.a
        NO_CMAKE_SYSTEM_PATH
        NO_CMAKE_FIND_ROOT_PATH
        PATHS ${MOCKCPP_INSTALL_PATH}/lib)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(mockcpp
        FOUND_VAR
        MOCKCPP_FOUND
        REQUIRED_VARS
        MOCKCPP_INCLUDE
        MOCKCPP_STATIC_LIBRARY
        )
message("mockcpp found:${MOCKCPP_FOUND} ${MOCKCPP_INCLUDE} ${MOCKCPP_STATIC_LIBRARY}")

if(MOCKCPP_FOUND)
    message("mockcpp found in ${MOCKCPP_INSTALL_PATH}")
else()
    if(EXISTS "${PROJECT_SOURCE_DIR}/../../llt/third_party/mockcpp_src")
        set(MOCKCPP_URL "${PROJECT_SOURCE_DIR}/../../llt/third_party/mockcpp_src")
    else()
        set(MOCKCPP_URL "https://gitcode.com/cann-src-third-party/mockcpp/releases/download/v2.7-h1/mockcpp-2.7.tar.gz")
    endif()

    message("Downloading mockcpp from ${MOCKCPP_URL}")

    set(MOCKCPP_CXXFLAGS "-D_GLIBCXX_USE_CXX11_ABI=0 -O2 -D_FORTIFY_SOURCE=2 -fPIC -D_BUILD_32_BIT_TARGET_BY_64_BIT_COMPILER=OFF -fstack-protector-all -Wl,-z,relro,-z,now,-z,noexecstack")
    include(ExternalProject)
    ExternalProject_Add(mockcpp
        URL ${MOCKCPP_URL}
        DOWNLOAD_DIR ${CANN_3RD_LIB_PATH}
        CONFIGURE_COMMAND ${CMAKE_COMMAND}
        -DCMAKE_CXX_FLAGS=${MOCKCPP_CXXFLAGS}
        -DCMAKE_INSTALL_PREFIX=${MOCKCPP_INSTALL_PATH}
        -DBUILD_32_BIT_TARGET_BY_64_BIT_COMPILER=OFF
        <SOURCE_DIR>
        BUILD_COMMAND $(MAKE)
        INSTALL_COMMAND $(MAKE) install
    )
endif()

set(MOCKCPP_INCLUDE
    ${MOCKCPP_INSTALL_PATH}/include
    ${MOCKCPP_INSTALL_PATH}/include/3rdparty
)

set(MOCKCPP_LIBRARY
    ${MOCKCPP_INSTALL_PATH}/lib/libmockcpp.a
)