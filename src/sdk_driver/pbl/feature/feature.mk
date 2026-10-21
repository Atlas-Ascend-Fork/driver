# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------

$(MODULE_NAME)-objs += feature/feature_framework.o
$(MODULE_NAME)-objs += feature/pbl_module_feature.o

EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/pbl
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/kernel_adapt/include
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/pbl/feature
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/pbl/dev_urd
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/pbl/uda
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/pbl/dev_urd/command/ioctl
EXTRA_CFLAGS += -isystem $(shell $(CC) -print-file-name=include)
