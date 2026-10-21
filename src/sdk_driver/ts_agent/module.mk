# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := ts_agent

LOCAL_KO_SRC_FOLDER := $(LOCAL_PATH)
ifeq ($(PRODUCT_SIDE), host)
LOCAL_DEPEND_KO := drv_devdrv_host asdrv_dms asdrv_pbl
else
    ifneq ($(filter ascend310B ascend310Brc ascend910B as31xm1, $(PRODUCT)), )
        LOCAL_DEPEND_KO := asdrv_dms asdrv_pbl
    else
        LOCAL_DEPEND_KO := drv_devdrv asdrv_dms asdrv_pbl
    endif
endif
LOCAL_INSTALLED_KO_FILES := ts_agent.ko

ifeq ($(PRODUCT_SIDE), host)
include $(BUILD_HOST_KO)
else
include $(BUILD_DEVICE_KO)
endif
