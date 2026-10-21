# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------

# configuring module feature
ifeq ($(TARGET_BUILD_TYPE),debug)
	EXTRA_CFLAGS += -DCFG_BUILD_DEBUG
endif

EXTRA_CFLAGS += -DCFG_HOST_ENV
EXTRA_CFLAGS += -DCFG_EDGE_HOST
EXTRA_CFLAGS += -DCFG_FEATURE_MEMALLOC_MODULE_TYPE=3
EXTRA_CFLAGS += -DCFG_FEATURE_MEMALLOC_SUBMODULE_TYPE=0

asdrv_dms-y += feature/host/dms_feature.o
asdrv_dms-y += feature/host/dmc_feature.o
