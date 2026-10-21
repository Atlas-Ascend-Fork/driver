# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------

set(MAKESELF_NAME "makeself")
set(MAKESELF_PATH "${CMAKE_BINARY_DIR}/${MAKESELF_NAME}")

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

set(MAKESELF_SEARCH_PATHS
    ${CANN_3RD_LIB_PATH}/open_source/${MAKESELF_NAME}       # compile with ci
    ${CANN_3RD_LIB_PATH}/${MAKESELF_NAME}
)

find_path(MAKESELF_SH_PATH NAMES makeself.sh PATHS ${MAKESELF_SEARCH_PATHS})
if(NOT MAKESELF_SH_PATH)
    if (EXISTS "${CANN_3RD_LIB_PATH}/makeself-release-2.5.0-patch1.tar.gz")
        set(MAKESELF_URL "file://${CANN_3RD_LIB_PATH}/makeself-release-2.5.0-patch1.tar.gz")
    else()
        set(MAKESELF_URL "https://gitcode.com/cann-src-third-party/makeself/releases/download/\
release-2.5.0-patch1.0/makeself-release-2.5.0-patch1.tar.gz")
    endif()
    message(STATUS "Downloading ${MAKESELF_NAME} from ${MAKESELF_URL}")

    include(FetchContent)
    FetchContent_Declare(
        ${MAKESELF_NAME}
        URL ${MAKESELF_URL}
        URL_HASH SHA256=bfa730a5763cdb267904a130e02b2e48e464986909c0733ff1c96495f620369a
        DOWNLOAD_DIR ${EXTERN_DEPEND_DOWNLOAD_DIR}
        SOURCE_DIR "${MAKESELF_PATH}"
    )
    FetchContent_MakeAvailable(${MAKESELF_NAME})
    execute_process(
        COMMAND chmod 700 "${CMAKE_BINARY_DIR}/makeself/makeself.sh"
        COMMAND chmod 700 "${CMAKE_BINARY_DIR}/makeself/makeself-header.sh"
        -E env
        CMAKE_TLS_VERIFY=0
        RESULT_VARIABLE CHMOD_RESULT
        ERROR_VARIABLE CHMOD_ERROR
    )
else()
    file(MAKE_DIRECTORY "${MAKESELF_PATH}")
    execute_process(
        COMMAND bash -c "cp ${MAKESELF_SH_PATH}/makeself-header.sh ${MAKESELF_PATH}/makeself-header.sh"
        COMMAND bash -c "cp ${MAKESELF_SH_PATH}/makeself.sh ${MAKESELF_PATH}/makeself.sh"
    )
endif()
