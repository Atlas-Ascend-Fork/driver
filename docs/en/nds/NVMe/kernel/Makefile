# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------

# Kbuild Makefile for the npu_nds demo kernel module.
# The following environment variables are expected to be exported by the
# driver's setenv.bash (see run.sh):
#   KDIR                - kernel build tree (/lib/modules/$(uname -r)/build)
#   DRIVER_HAL_INC_DIR  - path to ascend_kernel_hal.h
#   DRIVER_KERNEL_DIR   - path to the kernel adapt inc dir (for ka_* headers)
#   DRIVER_CUST_INC_DIR - path to custom include dir
#   C_SEC_INCLUDE       - path to libc_sec headers
# Any of them may be empty on a given install; missing ones are skipped.

EXTRA_CFLAGS += -I$(NDS_SRC_DIR)
EXTRA_CFLAGS += -I$(DRIVER_HAL_INC_DIR)
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/kernel_adapt/include
EXTRA_CFLAGS += -I$(DRIVER_CUST_INC_DIR)
EXTRA_CFLAGS += -I$(C_SEC_INCLUDE)
EXTRA_CFLAGS += -isystem $(shell $(CC) -print-file-name=include)
EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
EXTRA_CFLAGS += -fstack-protector-all

obj-m += npu_nds.o
npu_nds-y := npu_nds_kern.o
