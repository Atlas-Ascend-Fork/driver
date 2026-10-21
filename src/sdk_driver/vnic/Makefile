# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
MODULE_NAME := asdrv_vnic
obj-m += $(MODULE_NAME).o
$(MODULE_NAME)-objs := host/pcivnic_main.o host/pcivnic_common_adapt.o host/pcivnic_host.o \
					   host/pcivnic_host_adapt.o host/pcivnic_init.o host/pcivnic_adapt.o

EXTRA_CFLAGS += -I$(DRIVER_HAL_INC_DIR)
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/pbl
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/vnic/inc
EXTRA_CFLAGS += -I${C_SEC_INCLUDE}
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/vnic/host
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/vnic/command/msg
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/kernel_adapt/include
EXTRA_CFLAGS += -isystem $(shell $(CC) -print-file-name=include)
ifneq ($(filter $(PRODUCT), ascend950),)
	EXTRA_CFLAGS += -Iinclude/linux
endif

EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG

ifneq ($(filter $(PRODUCT), ascend910B),)
	EXTRA_CFLAGS += -DCFG_FEATURE_SRIOV
	EXTRA_CFLAGS += -DCFG_FEATURE_AGENT_SMMU
	EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
endif

ccflags-y += -fno-common -fstack-protector-all -funsigned-char -pipe -s -Wall -Wcast-align -Werror -Wfloat-equal
ccflags-y += -Wformat=2 -Wstack-usage=2048 -Wstrict-prototypes -Wtrampolines -Wundef -Wunused -Wvla -Wno-format-nonliteral