# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------

message("Build third party library gtest")

unset(GTEST_FOUND CACHE)
unset(GTEST_INCLUDE CACHE)
unset(GTEST_STATIC_LIBRARY CACHE)
unset(GTEST_MAIN_STATIC_LIBRARY CACHE)
unset(GMOCK_STATIC_LIBRARY CACHE)
unset(GMOCK_MAIN_STATIC_LIBRARY CACHE)

set(GTEST_INSTALL_PATH ${TESTCASE_3RD_LIB_PATH}/gtest)

find_path(GTEST_INCLUDE
        NAMES gtest/gtest.h
        NO_CMAKE_SYSTEM_PATH
        NO_CMAKE_FIND_ROOT_PATH
        PATHS ${GTEST_INSTALL_PATH}/include)
find_library(GTEST_STATIC_LIBRARY
        NAMES libgtest.a
        NO_CMAKE_SYSTEM_PATH
        NO_CMAKE_FIND_ROOT_PATH
        PATHS ${GTEST_INSTALL_PATH}/lib)
find_library(GTEST_MAIN_STATIC_LIBRARY
        NAMES libgtest_main.a
        NO_CMAKE_SYSTEM_PATH
        NO_CMAKE_FIND_ROOT_PATH
        PATHS ${GTEST_INSTALL_PATH}/lib)
find_library(GMOCK_STATIC_LIBRARY
        NAMES libgmock.a
        NO_CMAKE_SYSTEM_PATH
        NO_CMAKE_FIND_ROOT_PATH
        PATHS ${GTEST_INSTALL_PATH}/lib)
find_library(GMOCK_MAIN_STATIC_LIBRARY
        NAMES libgmock_main.a
        NO_CMAKE_SYSTEM_PATH
        NO_CMAKE_FIND_ROOT_PATH
        PATHS ${GTEST_INSTALL_PATH}/lib)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(gtest
        FOUND_VAR
        GTEST_FOUND
        REQUIRED_VARS
        GTEST_INCLUDE
        GTEST_STATIC_LIBRARY
        GTEST_MAIN_STATIC_LIBRARY
        GMOCK_STATIC_LIBRARY
        GMOCK_MAIN_STATIC_LIBRARY
        )
message("gtest found:${GTEST_FOUND}")

if(GTEST_FOUND)
    message("gtest found in ${GTEST_INSTALL_PATH}")
else()
    if(EXISTS "${PROJECT_SOURCE_DIR}/../../llt/third_party/gtest/googletest-1.10.x")
        set(GTEST_URL "${PROJECT_SOURCE_DIR}/../../llt/third_party/gtest/googletest-1.10.x")
    else()
        set(GTEST_URL "https://gitcode.com/cann-src-third-party/googletest/releases/download/v1.14.0/googletest-1.14.0.tar.gz")
    endif()

    message("Downloading gtest from ${GTEST_URL}")

    set(GTEST_CXXFLAGS "-D_GLIBCXX_USE_CXX11_ABI=0 -O2 -D_FORTIFY_SOURCE=2 -fPIC -fstack-protector-all -Wl,-z,relro,-z,now,-z,noexecstack")
    include(ExternalProject)
    ExternalProject_Add(gtest
        URL ${GTEST_URL}
        DOWNLOAD_DIR ${CANN_3RD_LIB_PATH}
        CONFIGURE_COMMAND ${CMAKE_COMMAND}
        -DCMAKE_CXX_FLAGS=${GTEST_CXXFLAGS}
        -DCMAKE_INSTALL_PREFIX=${GTEST_INSTALL_PATH}
        <SOURCE_DIR>
        BUILD_COMMAND $(MAKE)
        INSTALL_COMMAND $(MAKE) install
    )
endif()

set(GTEST_INCLUDE ${GTEST_INSTALL_PATH}/include)


set(GTEST_LIBRARY
    ${GTEST_INSTALL_PATH}/lib/libgtest.a
    ${GTEST_INSTALL_PATH}/lib/libgtest_main.a
)