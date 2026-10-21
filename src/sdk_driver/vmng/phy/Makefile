# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
MODULE_NAME := asdrv_vmng
obj-m += ${MODULE_NAME}.o
${MODULE_NAME}-objs := common/module_host_init.o
${MODULE_NAME}-objs += host/virtmnghost_pci.o host/virtmnghost_ctrl.o host/virtmnghost_unit.o host/virtmnghost_feature.o
${MODULE_NAME}-objs += host/virtmnghost_external.o host/virtmnghost_sysfs.o host/virtmnghost_proc_fs.o
${MODULE_NAME}-objs += host/soc_adapt/virtmng_res_drv.o host/soc_adapt/virtmng_res_cloud_v1.o host/soc_adapt/virtmng_res_cloud_v2.o
${MODULE_NAME}-objs += host/soc_adapt/virtmng_res_mini_v2.o host/soc_adapt/virtmng_res_cloud_v4.o host/soc_adapt/virtmng_res_comm.o

EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc
EXTRA_CFLAGS += -I${C_SEC_INCLUDE}
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/vmng/command/msg
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/vmng/inc
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/vpc/common
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/vpc/phy
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/vmng/phy/host
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/vmng/phy/host/soc_adapt
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/vmng/phy/vascend_drv_stub
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/pbl/dev_urd
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/pbl/dev_urd/dc
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/pbl/uda/command/ioctl
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/kernel_adapt/include
EXTRA_CFLAGS += -I$(DRIVER_HAL_INC_DIR)
EXTRA_CFLAGS += -isystem $(shell $(CC) -print-file-name=include)

EXTRA_CFLAGS += -DVIRTMNG_HOST
EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
EXTRA_CFLAGS += -DDRV_HOST
EXTRA_CFLAGS += -DCFG_ENV_HOST

include $(FEATURE_MK_PATH)
EXTRA_CFLAGS += $(CONFIG_DEFINES)

ccflags-y += -fno-common -fstack-protector-all -funsigned-char -pipe -s -Wall -Wcast-align -Werror -Wfloat-equal
ccflags-y += -Wformat=2 -Wstack-usage=2048 -Wstrict-prototypes -Wtrampolines -Wundef -Wunused -Wvla -Wno-format-nonliteral