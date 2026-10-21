# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
MODULE_NAME := drv_pcie_hdc_host
obj-m += $(MODULE_NAME).o
$(MODULE_NAME)-objs := common/hdc_init.o
$(MODULE_NAME)-objs += common/hdcdrv_feature.o
$(MODULE_NAME)-objs += dc/hdcdrv_feature_adapt.o
$(MODULE_NAME)-objs += pcie/host/hdcdrv_adapt.o pcie/host/hdcdrv_host.o pcie/host/hdcdrv_host_adapt.o
$(MODULE_NAME)-objs += pcie/common/hdcdrv_core.o pcie/common/hdcdrv_core_com.o pcie/common/hdcdrv_mem.o
$(MODULE_NAME)-objs += pcie/common/hdcdrv_epoll.o pcie/common/hdcdrv_sysfs.o pcie/common/hdcdrv_mem_com.o
$(MODULE_NAME)-objs += pcie/common/hdcdrv_mem_fast.o

ifneq ($(filter $(PRODUCT), ascend950),)
	$(MODULE_NAME)-objs += ub/common/hdcdrv_core_ub.o ub/common/hdcdrv_proc_fs_ub.o ub/common/hdcdrv_mem_pool_ub.o
	$(MODULE_NAME)-objs += ub/common/hdcdrv_mem_com_ub.o ub/host/hdcdrv_adapt_ub.o ub/common/hdcdrv_ctrl_msg.o
endif

EXTRA_CFLAGS += -I$(DRIVER_HAL_INC_DIR)
EXTRA_CFLAGS += -I${C_SEC_INCLUDE}
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/hdc/inc
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/hdc/dc
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/hdc/ub/inc
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/hdc/ub/host
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/hdc/command/msg
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/hdc/command/ioctl
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/hdc/pcie/inc
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/hdc/pcie/host
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/dms/command/ioctl
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/kernel_adapt/include
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/pbl
EXTRA_CFLAGS += -isystem $(shell $(CC) -print-file-name=include)

EXTRA_CFLAGS += -DDRV_HOST
EXTRA_CFLAGS += -DCFG_EDGE_HOST
EXTRA_CFLAGS += -DCFG_ENV_HOST
EXTRA_CFLAGS += -DCFG_FEATURE_SRIOV
EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
ifneq ($(filter $(PRODUCT), ascend910B),)
	EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
endif

ifneq ($(filter $(PRODUCT), ascend950),)
	EXTRA_CFLAGS += -Iinclude/linux
endif
EXTRA_CFLAGS += -DCFG_FEATURE_SHARE_LOG

ccflags-y += -fno-common -fstack-protector-all -funsigned-char -pipe -s -Wall -Wcast-align -Werror -Wfloat-equal
ccflags-y += -Wformat=2 -Wstack-usage=2048 -Wstrict-prototypes -Wtrampolines -Wundef -Wunused -Wvla -Wno-format-nonliteral