# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
get_filename_component(BASE_DIR "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)

add_subdirectory(data_parser)

add_library(${LIB_BBOX_PROC} STATIC)

target_sources(${LIB_BBOX_PROC}
    PRIVATE
        utils/bbox_common.c
        utils/bbox_system_api.c
        utils/bbox_feature.c
)

target_include_directories(${LIB_BBOX_PROC}
    PRIVATE
        ${DRIVER_HAL_INC_DIR}
        ${TOP_DIR}/inc/toolchain
        ${BASE_DIR}/data_parser
        ${BASE_DIR}/include
        ${BASE_DIR}/utils
        ${PROJ_DIR}/config/${PROD_NAME}/
        ${DRIVER_USER_DIR}/inc/slog
        ${C_SEC_INCLUDE}
        ${DRIVER_USER_DIR}/inc/mmpa
        ${DRIVER_KERNEL_DIR}/pbl/dev_urd/command/ioctl/
        ${DRIVER_USER_DIR}/inc
        ${DRIVER_USER_DIR}/inc/pbl
        ${BASE_DIR}/../../../../pkg_inc
        ${DRIVER_KERNEL_DIR}/dms/command/ioctl/
        ${DRIVER_KERNEL_DIR}/pbl/uda/command/ioctl/
)

target_link_libraries(${LIB_BBOX_PROC} PRIVATE
    $<BUILD_INTERFACE:bbox_intf_pub>
    $<BUILD_INTERFACE:intf_pub>
)

set(INSTALL_FILE_LIST ${CMAKE_CURRENT_BINARY_DIR}/Debug/${LIB_BBOX_PROC}.pdb)
install(FILES ${INSTALL_FILE_LIST} DESTINATION ${INSTALL_LIBRARY_DIR} OPTIONAL)