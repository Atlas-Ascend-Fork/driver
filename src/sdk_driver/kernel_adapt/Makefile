# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
ifeq ($(ENABLE_OPEN_SRC), y) # for build.sh script build
    TARGET_SRC_DIR := $(DRIVER_KERNEL_DIR)
    EXTRA_CFLAGS += -I$(C_SEC_INCLUDE)
else
    ifeq ($(DAVINCI_HIAI_DKMS),y) # for dkms
        KERNEL_DIR :=$(srctree)
        # HIAI_DKMS_DIR = driver/kernel
        DRIVER_SRC_BASE_DIR := $(HIAI_DKMS_DIR)
        TARGET_SRC_DIR := $(HIAI_DKMS_DIR)
        EXTRA_CFLAGS += -I$(TARGET_SRC_DIR)/libc_sec/include/
    else # for ci
        TARGET_SRC_DIR := $(DRIVER_KERNEL_DIR)
        EXTRA_CFLAGS += -I$(C_SEC_INCLUDE)
    endif
endif

EXTRA_CFLAGS += -I$(TARGET_SRC_DIR)/kernel_adapt/
EXTRA_CFLAGS += -I$(TARGET_SRC_DIR)/kernel_adapt/include
EXTRA_CFLAGS += -I$(TARGET_SRC_DIR)/kernel_adapt/base
EXTRA_CFLAGS += -I$(TARGET_SRC_DIR)/kernel_adapt/dfx
EXTRA_CFLAGS += -I$(TARGET_SRC_DIR)/kernel_adapt/driver
EXTRA_CFLAGS += -I$(TARGET_SRC_DIR)/kernel_adapt/fs
EXTRA_CFLAGS += -I$(TARGET_SRC_DIR)/kernel_adapt/memory
EXTRA_CFLAGS += -I$(TARGET_SRC_DIR)/kernel_adapt/net
EXTRA_CFLAGS += -I$(TARGET_SRC_DIR)/kernel_adapt/pci
EXTRA_CFLAGS += -I$(TARGET_SRC_DIR)/kernel_adapt/system
EXTRA_CFLAGS += -I$(TARGET_SRC_DIR)/kernel_adapt/task
EXTRA_CFLAGS += -I$(TARGET_SRC_DIR)/kernel_adapt/sched
EXTRA_CFLAGS += -I$(TARGET_SRC_DIR)/inc/pbl

EXTRA_CFLAGS += -isystem $(shell $(CC) -print-file-name=include)
EXTRA_CFLAGS += -Wno-type-limits # for kernel 5.15
ifeq ($(PRODUCT_SIDE), host)
    EXTRA_CFLAGS += -DDRV_HOST
endif
ccflags-y += -Wno-implicit-function-declaration

# check if gcc support -Wno-error=cast-function-type
CHECK_FLAG := $(shell echo "int main() { return 0; }" | ${CC} -x c -Wno-error=cast-function-type -o /dev/null - 2>/dev/null && echo supported || echo unsupported)

# add cflags -Wno-error=cast-function-type
ifeq ($(CHECK_FLAG),supported)
    EXTRA_CFLAGS += -Wno-error=cast-function-type
endif

ifeq ($(TARGET_BUILD_TYPE),debug)
    EXTRA_CFLAGS += -DCFG_BUILD_DEBUG
endif
ifneq ($(NOT_SUPPORT_SP), y)
    EXTRA_CFLAGS += -fstack-protector-all
endif

ifneq ($(filter $(ARCH), arm64 x86), )
$(info ARCH is: $(ARCH))
else
$(error NOT supported ARCH: $(ARCH))
endif

EXTRA_CFLAGS += -Wall -Werror

$(info $(EXTRA_CFLAGS))

ccflags-y += -Wall -Werror -Wtrampolines $(WDATE_TIME) -Wfloat-equal -Wvla -Wundef -funsigned-char -Wformat=2 -Wstack-usage=2048 -Wcast-align -Wextra
ccflags-y += -Wno-unused-parameter -O2 -Wno-missing-field-initializers -Wno-sign-compare -Wno-format-nonliteral

# check if gcc support -Wno-implicit-fallthrough
# -Wno-implicit-fallthrough was first added in gcc-7.
CHECK_FLAG := $(shell echo "int main() { return 0; }" | ${CC} -x c -Wno-implicit-fallthrough -o /dev/null - 2>/dev/null && echo supported || echo unsupported)
# add cflags -Wno-implicit-fallthrough
ifeq ($(CHECK_FLAG),supported)
    ccflags-y += -Wno-implicit-fallthrough
endif

obj-m += ascend_kernel_open_adapt.o

ascend_kernel_open_adapt-objs := ka_module_init.o  \
                                 task/ka_task.o \
                                 fs/ka_fs.o \
                                 system/ka_system.o \
                                 base/ka_base.o \
                                 driver/ka_driver.o \
                                 memory/ka_mem.o \
                                 memory/ka_pgwalk.o \
                                 pci/ka_pci.o \
                                 net/ka_net.o \
                                 dfx/ka_dfx.o \
                                 dfx/event_notify_proc.o \
                                 sched/ka_sched.o
