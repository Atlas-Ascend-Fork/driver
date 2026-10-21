# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------

set(C_SEC_NAME "secure_c")
message(STATUS "Build third party library secure_c")

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

set(C_SEC_HEAD_SEARCH_PATHS
    ${CANN_3RD_LIB_PATH}/abl/libc_sec/include             # compile with ci
    ${CANN_3RD_LIB_PATH}/libboundscheck-v1.1.16/include
)

find_path(C_SEC_INCLUDE NAMES securec.h PATHS ${C_SEC_HEAD_SEARCH_PATHS} NO_DEFAULT_PATH)
if(C_SEC_INCLUDE)
    get_filename_component(CANN_3RD_SECURE_C_DIR ${C_SEC_INCLUDE} DIRECTORY)
    file(MAKE_DIRECTORY "${EXTERN_DEPEND_SOURCE_DIR}/libc_sec")
    execute_process(
        COMMAND bash -c "cp -r ${CANN_3RD_SECURE_C_DIR}/include ${EXTERN_DEPEND_SOURCE_DIR}/libc_sec/include \
&& cp -r ${CANN_3RD_SECURE_C_DIR}/src ${EXTERN_DEPEND_SOURCE_DIR}/libc_sec/src"
        RESULT_VARIABLE _cp_result
    )
    if(NOT _cp_result EQUAL 0)
        message(FATAL_ERROR "Failed to copy secure_c source from ${CANN_3RD_SECURE_C_DIR} (ret=${_cp_result})")
    endif()
else()
    if(EXISTS "${CANN_3RD_LIB_PATH}/libboundscheck-v1.1.16.tar.gz")
        set(C_SEC_URL "file://${CANN_3RD_LIB_PATH}/libboundscheck-v1.1.16.tar.gz")
    else()
        set(C_SEC_URL "https://gitcode.com/cann-src-third-party/libboundscheck/releases/download/v1.1.16/\
libboundscheck-v1.1.16.tar.gz")
    endif()

    message(STATUS "Downloading ${C_SEC_NAME} from ${C_SEC_URL}")

    include(FetchContent)
    FetchContent_Declare(
        ${C_SEC_NAME}
        URL ${C_SEC_URL}
        DOWNLOAD_DIR ${EXTERN_DEPEND_DOWNLOAD_DIR}
        SOURCE_DIR ${EXTERN_DEPEND_SOURCE_DIR}/libc_sec
    )
    FetchContent_MakeAvailable(${C_SEC_NAME})
endif()

set(C_SEC_INCLUDE ${EXTERN_DEPEND_SOURCE_DIR}/libc_sec/include)
get_filename_component(SECURE_C_DIR ${C_SEC_INCLUDE} DIRECTORY)

configure_file(
    ${PROJECT_SOURCE_DIR}/cmake/config/c_sec_config/c_sec.cmake
    ${SECURE_C_DIR}/CMakeLists.txt
    COPYONLY
)

execute_process(
    COMMAND bash -c "cp ${PROJECT_SOURCE_DIR}/src/sdk_driver/seclib/Makefile ${SECURE_C_DIR}/Makefile \
&& cp ${PROJECT_SOURCE_DIR}/src/sdk_driver/seclib/securecmodule.c ${SECURE_C_DIR}/src/securecmodule.c"
    RESULT_VARIABLE _cp_result
)
if(NOT _cp_result EQUAL 0)
    message(FATAL_ERROR "Failed to copy Makefile/securecmodule.c to ${SECURE_C_DIR} (ret=${_cp_result})")
endif()

include(ExternalProject)
ExternalProject_Add(secure_c
    SOURCE_DIR ${SECURE_C_DIR}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}
)

link_directories(${CMAKE_CURRENT_BINARY_DIR}/lib)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)

ExternalProject_Get_Property(secure_c BINARY_DIR)
add_library(c_sec SHARED IMPORTED)
add_dependencies(c_sec secure_c)
set_target_properties(c_sec PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${C_SEC_INCLUDE}"
    IMPORTED_LOCATION             "${BINARY_DIR}/libc_sec.so"
)

add_library(c_sec_headers INTERFACE IMPORTED)
target_include_directories(c_sec_headers INTERFACE ${C_SEC_INCLUDE})

if("${BUILD_COMPONENT}" STREQUAL "DRIVER")
    add_host_ko(LOCAL_MODULE drv_seclib_host
        KO_SRC_FOLDER ${SECURE_C_DIR}
        MAKE_ARGS ${MAKE_ARGS})
endif()
