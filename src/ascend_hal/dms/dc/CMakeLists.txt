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
    dms_lpm_adapt.c
    dms_pcie_info_adapt.c
    dms_user_common_adapt.c
    dms_soc_adapt.c
    devdrv_manager_adapter.c
)

target_include_directories(dms_obj PRIVATE
    ${TOP_DIR}/inc/driver
    ${DRIVER_USER_DIR}/inc
    ${BASE_DIR}
    ${DRIVER_USER_DIR}/src/dms/common/
)