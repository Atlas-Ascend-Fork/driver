# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
EXTRA_CFLAGS += -I$(DRIVER_SOURCE_DIR)/pbl/mem_ops
EXTRA_CFLAGS += -I$(DRIVER_SOURCE_DIR)/pbl/mem_ops/timer

ifeq ($(DAVINCI_HIAI_DKMS),y)
    KERNEL_DIR :=$(srctree)
    # for dkms
    DRIVER_SRC_BASE_DIR := $(HIAI_DKMS_DIR)
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/dev_inc/inc/pbl
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/inc/driver
    EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/libc_sec/include
    EXTRA_CFLAGS += -I$(srctree)/include/linux
else
    EXTRA_CFLAGS += -I$(DRIVER_SOURCE_DIR)/inc
    EXTRA_CFLAGS += -I$(DRIVER_SOURCE_DIR)/inc/pbl
    EXTRA_CFLAGS += -I$(DRIVER_SOURCE_DIR)/libc_sec
    EXTRA_CFLAGS += -I$(DRIVER_HAL_INC_DIR)
    EXTRA_CFLAGS += -I$(srctree)/include/linux
endif

ifeq ($(TARGET_BUILD_TYPE),debug)
    EXTRA_CFLAGS += -DCFG_BUILD_DEBUG
endif

EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE

ifneq ($(NOT_SUPPORT_SP), y)
    EXTRA_CFLAGS += -fstack-protector-all
endif

EXTRA_CFLAGS += -Wall -Werror

ccflags-y += -Wall -Werror -Wtrampolines $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char -Wformat=2 -Wstack-usage=2048 -Wcast-align -Wextra
ccflags-y += -Wno-unused-parameter -O2 -Wno-missing-field-initializers -Wno-sign-compare

$(MODULE_NAME)-objs += mem_ops/ka_module_init.o mem_ops/timer/ka_timer.o
$(MODULE_NAME)-objs += mem_ops/ka_memory_mng.o mem_ops/ka_rbtree.o mem_ops/ka_memory.o mem_ops/ka_memory_query.o
$(MODULE_NAME)-objs += mem_ops/ka_proc_fs.o
