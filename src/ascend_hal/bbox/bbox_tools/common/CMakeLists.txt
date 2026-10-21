# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
add_library(${OBJ_TOOL_COMMON} OBJECT)

get_filename_component(BASE_DIR "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)

target_sources(${OBJ_TOOL_COMMON}
    PRIVATE
        ${BASE_DIR}/bbox_tool_fs.c
        ${BASE_DIR}/bbox_tool_log.c
        ${PROJ_DIR}/${BBOX_HOST_DIR}/utils/bbox_common.c
        ${PROJ_DIR}/${BBOX_HOST_DIR}/utils/bbox_system_api.c
        ${PROJ_DIR}/${BBOX_HOST_DIR}/utils/bbox_utils.c
        ${PROJ_DIR}/${BBOX_HOST_DIR}/utils/bbox_feature.c
)

target_include_directories(${OBJ_TOOL_COMMON}
    PUBLIC
        ${BASE_DIR}
        ${PROJ_DIR}/${BBOX_HOST_DIR}/file_manage
        ${PROJ_DIR}/${BBOX_HOST_DIR}/include
        ${PROJ_DIR}/${BBOX_HOST_DIR}/utils
        ${DRIVER_USER_DIR}/inc/mmpa
        ${C_SEC_INCLUDE}
        ${DRIVER_USER_DIR}/inc/slog
        ${DRIVER_KERNEL_DIR}/pbl/dev_urd/command/ioctl/
        ${DRIVER_KERNEL_DIR}/dms/command/ioctl/
        ${DRIVER_KERNEL_DIR}/pbl/uda/command/ioctl/
        ${DRIVER_USER_DIR}/inc
        ${DRIVER_USER_DIR}/inc/pbl
        ${PROJ_DIR}/${BBOX_HOST_DIR}/../../../../pkg_inc/
)

target_compile_definitions(${OBJ_TOOL_COMMON} PUBLIC DRV_HOST)

target_link_libraries(${OBJ_TOOL_COMMON} PUBLIC
    $<BUILD_INTERFACE:bbox_intf_pub>
)