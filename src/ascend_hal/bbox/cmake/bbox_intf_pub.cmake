# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
add_library(bbox_intf_pub INTERFACE)

target_compile_definitions(bbox_intf_pub INTERFACE
    $<$<STREQUAL:${TARGET_SYSTEM_NAME},Linux>:OS_TYPE=0>
    $<$<STREQUAL:${PRODUCT},ascend910B>:CFG_SOC_PLATFORM_CLOUD_V2>
    $<$<STREQUAL:${PRODUCT},ascend950>:CFG_SOC_PLATFORM_CLOUD_V4>
    $<$<STREQUAL:${PRODUCT},ascend950esl>:CFG_SOC_PLATFORM_CLOUD_V4>
    $<$<STREQUAL:${PRODUCT},ascend910_55esl>:CFG_SOC_PLATFORM_CLOUD_V4>
    $<$<STREQUAL:${PRODUCT},ascend910_55>:CFG_SOC_PLATFORM_CLOUD_V4>
    $<$<STREQUAL:${PRODUCT},ascend310Brc>:CFG_SOC_PLATFORM_RC>
    $<$<STREQUAL:${PRODUCT},ascend310Brcesl>:CFG_SOC_PLATFORM_RC>
    $<$<STREQUAL:${PRODUCT},ascend310Brcemu>:CFG_SOC_PLATFORM_RC>
    $<$<STREQUAL:${PRODUCT},ascend960>:CFG_FEATURE_NOT_SUPPORT_EROS>
    $<$<STREQUAL:${PRODUCT},ascend960esl>:CFG_FEATURE_NOT_SUPPORT_EROS>
)
