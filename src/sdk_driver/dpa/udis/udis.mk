# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
include $(FEATURE_MK_PATH)
EXTRA_CFLAGS += $(CONFIG_DEFINES)

EXTRA_CFLAGS += -I$(C_SEC_INCLUDE)
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/pbl
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/dpa/udis
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/common
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/ascend_platform
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/kernel_adapt/include
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/pbl/dev_urd
EXTRA_CFLAGS += -I$(DRIVER_HAL_INC_DIR)
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/dpa/inc
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/dpa/udis/command/msg

EXTRA_CFLAGS += -DDRV_HOST
EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
EXTRA_CFLAGS += -DCFG_FEATURE_MEMALLOC_MODULE_TYPE=4
EXTRA_CFLAGS += -DCFG_FEATURE_MEMALLOC_SUBMODULE_TYPE=2

ifneq ($(filter $(PRODUCT), ascend910B),)
    EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
endif

ifneq ($(filter $(PRODUCT), ascend950),)
    EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
endif

asdrv_dpa-objs += udis/udis_management.o udis/udis_timer.o udis/udis_cmd.o udis/udis_dfx.o
asdrv_dpa-objs += udis/host/udis_msg.o udis/host/udis_data.o udis/host/udis_init.o udis/host/udis_addr.o