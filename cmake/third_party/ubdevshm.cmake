# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in the compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
set(UBDEVSHM_NAME "ubdevshm")
set(UBDEVSHM_SEARCH_DIR
    ${CANN_3RD_LIB_PATH}/ubengine/ssapi/kernel # compile with ci
)

set(UBDEVSHM_SRC_DIR "")
set(UBDEVSHM_INCLUDE_DIR "")

foreach(_dir ${UBDEVSHM_SEARCH_DIR})
    if(EXISTS "${_dir}/drivers/ub/ubdevshm")
        set(UBENGINE_KERNEL_DIR ${_dir})
        set(UBDEVSHM_SRC_DIR ${_dir}/drivers/ub/ubdevshm)
        set(UBDEVSHM_INCLUDE_DIR ${_dir}/include/ub/ubdevshm)
        message(STATUS "Find ${UBDEVSHM_NAME} source code in ${UBENGINE_KERNEL_DIR}")
        break()
    endif()
endforeach()

if (UBDEVSHM_SRC_DIR STREQUAL "" OR UBDEVSHM_INCLUDE_DIR STREQUAL "")
    # ubdevshm is released by openeuler, and download openeuler first to get ubdevshm source code.
    set(OPENEULER_NAME "openeuler")
    set(OPENEULER_VERSION "6.6.0-156.0.0")
    if (EXISTS "${CANN_3RD_LIB_PATH}/${OPENEULER_VERSION}.tar.gz")
        set(OPENEULER_URL "file://${CANN_3RD_LIB_PATH}/${OPENEULER_VERSION}.tar.gz")
    else()
        set(OPENEULER_URL "https://raw.gitcode.com/openeuler/kernel/archive/refs/heads/${OPENEULER_VERSION}.tar.gz")
    endif()

    if(POLICY CMP0135)
        cmake_policy(SET CMP0135 NEW)
    endif()

    set(OPENEULER_INSTALL_DIR ${EXTERN_DEPEND_SOURCE_DIR}/openeuler)
    file(MAKE_DIRECTORY "${OPENEULER_INSTALL_DIR}")

    message(STATUS "Downloading openeuler from ${OPENEULER_URL}")

    include(FetchContent)
    FetchContent_Declare(
        ${OPENEULER_NAME}
        URL ${OPENEULER_URL}
        DOWNLOAD_DIR ${EXTERN_DEPEND_DOWNLOAD_DIR}
        SOURCE_DIR ${OPENEULER_INSTALL_DIR}
    )
    FetchContent_MakeAvailable(${OPENEULER_NAME})
    FetchContent_GetProperties(${OPENEULER_NAME})
    if(NOT ${OPENEULER_NAME}_POPULATED)
        message(FATAL_ERROR "Failed to download ${OPENEULER_NAME} from ${OPENEULER_URL}")
    endif()

    if(NOT EXISTS "${OPENEULER_INSTALL_DIR}/drivers/ub/ubdevshm")
        message(FATAL_ERROR "Failed to find ubdevshm source code in ${OPENEULER_INSTALL_DIR}/drivers/ub/ubdevshm/")
    endif()

    set(UBDEVSHM_SRC_DIR ${EXTERN_DEPEND_SOURCE_DIR}/ubdevshm)
    file(MAKE_DIRECTORY "${UBDEVSHM_SRC_DIR}")

    execute_process(
        COMMAND bash -c "cp ${OPENEULER_INSTALL_DIR}/drivers/ub/ubdevshm/*.c ${UBDEVSHM_SRC_DIR}/"
        COMMAND bash -c "cp ${OPENEULER_INSTALL_DIR}/drivers/ub/ubdevshm/*.h ${UBDEVSHM_SRC_DIR}/"
        COMMAND bash -c "cp ${OPENEULER_INSTALL_DIR}/drivers/ub/ubdevshm/Makefile ${UBDEVSHM_SRC_DIR}/Makefile"
        COMMAND bash -c "cp ${OPENEULER_INSTALL_DIR}/include/ub/ubdevshm/ubdevshm.h ${UBDEVSHM_SRC_DIR}/ubdevshm.h"
        COMMAND bash -c "cp ${PROJECT_SOURCE_DIR}/cmake/third_party/ubdevshm.patch ${UBDEVSHM_SRC_DIR}/ubdevshm.patch;patch -p1 < ubdevshm.patch"
        WORKING_DIRECTORY ${UBDEVSHM_SRC_DIR}
    )

    set(UBDEVSHM_MAKE_ARGS
        ${MAKE_ARGS}
        CONFIG_UB_UBDEVSHM=y
    )

    add_host_ko(LOCAL_MODULE ubdevshm
        KO_SRC_FOLDER ${UBDEVSHM_SRC_DIR}
        MAKE_ARGS ${UBDEVSHM_MAKE_ARGS})
else()
    add_host_ko(LOCAL_MODULE ubdevshm
        KO_SRC_FOLDER ${UBDEVSHM_SRC_DIR}
        MAKE_ARGS "UBENGINE_KERNEL_DIR=${UBENGINE_KERNEL_DIR}" "UBDEVSHM_KERNEL_INCLUDE_DIR=${UBDEVSHM_INCLUDE_DIR}" "UB_BUILD_OUT_OF_TREE=true"
        TARGETE_DPENDS "${ubdevshm_depends}")
endif()
