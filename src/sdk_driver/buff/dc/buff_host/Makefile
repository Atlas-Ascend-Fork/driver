# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
obj-m   += asdrv_buff.o
asdrv_buff-objs := xsmem_framework.o xsmem_ns_adapt.o xsmem_proc_fs.o xsmem_prop.o xsmem_res_idr.o xsmem_res_dispatch.o xsmem_algo_vma.o xsmem_algo_cache_vma.o multi_rbtree.o
asdrv_buff-objs += xsmem_modprobe_adapt.o xsmem_feature_check.o xsmem_framework_host.o

EXTRA_CFLAGS += -I$(DRIVER_HAL_INC_DIR)
EXTRA_CFLAGS += -I${C_SEC_INCLUDE}
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/pbl
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/buff/dc/buff_host
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/buff/dc/command/ioctl
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/kernel_adapt/include
EXTRA_CFLAGS += -isystem $(shell $(CC) -print-file-name=include)

EXTRA_CFLAGS += -DCFG_FEATURE_SHARE_LOG
EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
ccflags-y += -fno-common -fstack-protector-all -funsigned-char -pipe -s -Wall -Wcast-align -Wdate-time -Wfloat-equal -Wformat -Wstack-usage=2048 -Wstrict-prototypes -Wtrampolines -Wundef -Wunused -Wvla