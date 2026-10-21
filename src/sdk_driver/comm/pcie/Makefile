# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
MODULE_NAME := drv_pcie_host
obj-m += $(MODULE_NAME).o
$(MODULE_NAME)-objs := host/devdrv_feature.o
$(MODULE_NAME)-objs += host/devdrv_dma.o
$(MODULE_NAME)-objs += host/devdrv_msg.o
$(MODULE_NAME)-objs += host/devdrv_ctrl.o
$(MODULE_NAME)-objs += host/devdrv_sysfs.o
$(MODULE_NAME)-objs += host/devdrv_pci.o
$(MODULE_NAME)-objs += host/devdrv_common_msg.o
$(MODULE_NAME)-objs += host/devdrv_device_load.o
$(MODULE_NAME)-objs += host/devdrv_atu_interface.o
$(MODULE_NAME)-objs += host/devdrv_vpc.o
$(MODULE_NAME)-objs += host/devdrv_smmu.o
$(MODULE_NAME)-objs += host/devdrv_pasid_rbtree.o
$(MODULE_NAME)-objs += host/devdrv_pcie_link_info.o
$(MODULE_NAME)-objs += host/soc_adapt/res_drv.o
$(MODULE_NAME)-objs += host/soc_adapt/dma_adapt.o
$(MODULE_NAME)-objs += host/soc_adapt/nvme_adapt.o
$(MODULE_NAME)-objs += host/interfaces/devdrv_dma_interface.o
$(MODULE_NAME)-objs += dc/host/devdrv_adapt.o
$(MODULE_NAME)-objs += dc/host/cloud_v1/res_drv_cloud_v1.o
$(MODULE_NAME)-objs += dc/host/cloud_v2/res_drv_cloud_v2.o
$(MODULE_NAME)-objs += dc/host/cloud_v4/res_drv_cloud_v4.o
$(MODULE_NAME)-objs += dc/host/cloud_v6/res_drv_cloud_v6.o
$(MODULE_NAME)-objs += dc/host/mini_v2/res_drv_mini_v2.o
$(MODULE_NAME)-objs += dc/host/mini_v3/res_drv_mini_v3.o
$(MODULE_NAME)-objs += dc/host/mini_v5/res_drv_mini_v5.o
$(MODULE_NAME)-objs += host/devdrv_s2s_msg.o

EXTRA_CFLAGS += -I$(DRIVER_HAL_INC_DIR)
EXTRA_CFLAGS += -I${C_SEC_INCLUDE}/
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/kernel_adapt/include/
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/comm/pcie/command/msg/
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/comm/pcie/dc/host/
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/comm/pcie/dc/host/cloud_v1/
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/comm/pcie/dc/host/cloud_v2/
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/comm/pcie/dc/host/cloud_v4/
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/comm/pcie/dc/host/cloud_v6/
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/comm/pcie/dc/host/mini_v2/
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/comm/pcie/dc/host/mini_v3/
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/comm/pcie/dc/host/mini_v5/
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/comm/pcie/inc/
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/comm/pcie/host/
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/comm/pcie/host/soc_adapt/
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/kernel_adapt/include/
EXTRA_CFLAGS += -isystem $(shell $(CC) -print-file-name=include)
ifneq ($(filter $(PRODUCT), ascend950 ascend350),)
	EXTRA_CFLAGS += -Iinclude/linux
endif

EXTRA_CFLAGS += -DCFG_FEATURE_SRIOV
ifneq ($(filter $(PRODUCT), ascend910B),)
	EXTRA_CFLAGS += -DCFG_FEATURE_DMI
endif

ifneq ($(filter $(PRODUCT), ascend950 ascend350),)
	ifeq ($(ENABLE_EQUIP), true)
		EXTRA_CFLAGS += -DCFG_FEATURE_DMI
	endif
endif

EXTRA_CFLAGS += -DCFG_HOST
EXTRA_CFLAGS += -DDRV_HOST
EXTRA_CFLAGS += -DCFG_ENV_HOST
EXTRA_CFLAGS += -DCFG_FEATURE_SHARE_LOG
ifeq ($(ASCEND910_93_EX), TRUE)
	EXTRA_CFLAGS += -DASCEND910_93_EX
endif
ifeq ($(ENABLE_BUILD_PRODUCT), TRUE)
	EXTRA_CFLAGS += -DENABLE_BUILD_PRODUCT
endif

ccflags-y += -fno-common -fstack-protector-all -funsigned-char -pipe -s -Wall -Wcast-align -Werror -Wfloat-equal
ccflags-y += -Wformat=2 -Wstack-usage=2048 -Wstrict-prototypes -Wtrampolines -Wundef -Wunused -Wvla -Wno-format-nonliteral