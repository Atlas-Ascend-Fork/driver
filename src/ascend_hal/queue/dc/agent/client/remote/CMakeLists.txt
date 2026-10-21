# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
target_sources(queue_obj PRIVATE
    que_clt_api.c
)

target_include_directories(queue_obj PRIVATE
    ${DRIVER_USER_DIR}/queue/dc/agent/client/remote/comm
    ${DRIVER_USER_DIR}/queue/dc/agent/client/remote/que_clt_pcie
    ${DRIVER_USER_DIR}/queue/dc/agent/common
)

add_subdirectory(que_clt_pcie)
if(${PRODUCT} IN_LIST support_ub)
    add_subdirectory(que_clt_ub)
endif ()