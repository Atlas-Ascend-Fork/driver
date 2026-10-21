# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
target_sources(dms_obj PRIVATE
    udis_user_common.c
    $<$<STREQUAL:${PRODUCT},ascend910B>:${CMAKE_CURRENT_SOURCE_DIR}/udis_user.c>
    $<$<STREQUAL:${PRODUCT},ascend950>:${CMAKE_CURRENT_SOURCE_DIR}/udis_user.c>
    $<$<STREQUAL:${PRODUCT},ascend960>:${CMAKE_CURRENT_SOURCE_DIR}/udis_user.c>
    $<$<STREQUAL:${PRODUCT},ascend960esl>:${CMAKE_CURRENT_SOURCE_DIR}/udis_user.c>
)

target_include_directories(dms_obj PRIVATE
    ${DRIVER_HAL_INC_DIR}
    ${DRIVER_USER_DIR}/inc
    ${DRIVER_KERNEL_DIR}/pbl/dev_urd/command/ioctl
)