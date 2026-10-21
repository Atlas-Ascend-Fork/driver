# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------

set(EXTERN_DEPEND_SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/third_party)
set(EXTERN_DEPEND_DOWNLOAD_DIR ${CMAKE_CURRENT_BINARY_DIR}/third_party/download)
file(MAKE_DIRECTORY "${EXTERN_DEPEND_SOURCE_DIR}")
file(MAKE_DIRECTORY "${EXTERN_DEPEND_DOWNLOAD_DIR}")

if(NOT CANN_3RD_LIB_PATH)
    set(_DEFAULT_CANN_3RD_LIB_PATH ${PROJECT_SOURCE_DIR}/../..)
    get_filename_component(_DEFAULT_CANN_3RD_LIB_PATH_ABS "${_DEFAULT_CANN_3RD_LIB_PATH}" ABSOLUTE)
    if(EXISTS "${_DEFAULT_CANN_3RD_LIB_PATH_ABS}" AND NOT "${_DEFAULT_CANN_3RD_LIB_PATH_ABS}" STREQUAL "/")
        set(CANN_3RD_LIB_PATH "${_DEFAULT_CANN_3RD_LIB_PATH_ABS}")
        message(STATUS "CANN_3RD_LIB_PATH not set, using default: ${CANN_3RD_LIB_PATH}")
    else()
        set(CANN_3RD_LIB_PATH "${EXTERN_DEPEND_SOURCE_DIR}")
        message(STATUS "CANN_3RD_LIB_PATH not set and default (${_DEFAULT_CANN_3RD_LIB_PATH}) is invalid. "
                       "Redirecting to build dir: ${CANN_3RD_LIB_PATH}. Dependencies will be fetched from remote.")
    endif()
    unset(_DEFAULT_CANN_3RD_LIB_PATH)
    unset(_DEFAULT_CANN_3RD_LIB_PATH_ABS)
endif()

include(${PROJECT_SOURCE_DIR}/cmake/third_party/secure_c.cmake)

if("${PRODUCT}" STREQUAL "ascend910B")
    include(${PROJECT_SOURCE_DIR}/cmake/third_party/rdma-core.cmake)
endif()

if("${PRODUCT}" STREQUAL "ascend950" AND "${BUILD_COMPONENT}" STREQUAL "DRIVER")
    include(${PROJECT_SOURCE_DIR}/cmake/third_party/ubdevshm.cmake)
endif()
