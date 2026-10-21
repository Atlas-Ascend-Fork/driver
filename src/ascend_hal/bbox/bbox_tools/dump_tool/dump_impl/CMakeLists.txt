# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
add_library(${OBJ_DUMP_COMMON} OBJECT)

get_filename_component(BASE_DIR "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)

target_sources(${OBJ_DUMP_COMMON}
    PRIVATE
        bbox_parse_interface.c
        bbox_dump_interface.c
        bbox_dump_process.c
        bbox_drv_adapter.c
        product/bbox_dump_process_${PROD_NAME}.c
        product/bbox_dump_${PROD_NAME}.c
)

target_include_directories(${OBJ_DUMP_COMMON}
    PUBLIC
        ${BASE_DIR}
        ${BASE_DIR}/../dump_lib
        ${PROJ_DIR}/${BBOX_HOST_DIR}/data_parser
        ${PROJ_DIR}/${BBOX_HOST_DIR}/data_parser/rdr_data_parser
        ${PROJ_DIR}/${BBOX_HOST_DIR}/data_parser/os_data_parser
        ${DRIVER_HAL_INC_DIR}
        ${DRIVER_KERNEL_DIR}/pbl/dev_urd/command/ioctl/
        ${DRIVER_KERNEL_DIR}/dms/command/ioctl
        ${DRIVER_USER_DIR}/inc
        ${DRIVER_USER_DIR}/inc/pbl
        ${DRIVER_USER_DIR}/inc/mmpa
        ${DRIVER_USER_DIR}/inc/adump
        ${C_SEC_INCLUDE}
    PRIVATE
        ${PROJ_DIR}/config/${PROD_NAME}/
)
target_link_libraries(${OBJ_DUMP_COMMON} PRIVATE ${OBJ_TOOL_COMMON})
target_link_libraries(${OBJ_DUMP_COMMON} PRIVATE $<BUILD_INTERFACE:${OBJ_DATA_PARSE}>)
