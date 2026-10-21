# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------

EXTRA_CFLAGS += -isystem $(shell $(CC) -print-file-name=include)
EXTRA_CFLAGS += -Iinclude/linux

SVM_KERNEL_DIR := ${DRIVER_KERNEL_DIR}/svm/v3

EXTRA_CFLAGS += -I${DRIVER_KERNEL_DIR}/inc
EXTRA_CFLAGS += -I${DRIVER_KERNEL_DIR}/inc/pbl
EXTRA_CFLAGS += -I${DRIVER_KERNEL_DIR}/inc/comm
EXTRA_CFLAGS += -I${DRIVER_KERNEL_DIR}/kernel_adapt/include
EXTRA_CFLAGS += -I${DRIVER_HAL_INC_DIR}
EXTRA_CFLAGS += -I$(C_SEC_INCLUDE)

EXTRA_CFLAGS += -DDRV_HOST

EXTRA_CFLAGS += -I${SVM_KERNEL_DIR}/command/msg
EXTRA_CFLAGS += -I${SVM_KERNEL_DIR}/command/ioctl
EXTRA_CFLAGS += -I${SVM_KERNEL_DIR}/command/ioctl/def
EXTRA_CFLAGS += -I${SVM_KERNEL_DIR}/command/ioctl/uk_msg

EXTRA_CFLAGS += -I${SVM_KERNEL_DIR}/inc
EXTRA_CFLAGS += -I${SVM_KERNEL_DIR}/inc/framework
EXTRA_CFLAGS += -I${SVM_KERNEL_DIR}/inc/kern_lib
EXTRA_CFLAGS += -I${SVM_KERNEL_DIR}/inc/share

SVM_KERNEL_MASTER_SRCS := $(subst $(SVM_KERNEL_DIR)/,,$(wildcard $(SVM_KERNEL_DIR)/framework/*.c))
SVM_KERNEL_MASTER_SRCS += $(subst $(SVM_KERNEL_DIR)/,,$(wildcard $(SVM_KERNEL_DIR)/host_dev/*.c))
SVM_KERNEL_MASTER_SRCS += $(subst $(SVM_KERNEL_DIR)/,,$(wildcard $(SVM_KERNEL_DIR)/dbi/core/*.c))
SVM_KERNEL_MASTER_SRCS += $(subst $(SVM_KERNEL_DIR)/,,$(wildcard $(SVM_KERNEL_DIR)/um_handle/*.c))
SVM_KERNEL_MASTER_SRCS += $(subst $(SVM_KERNEL_DIR)/,,$(wildcard $(SVM_KERNEL_DIR)/um_handle_adapt/host/*.c))
SVM_KERNEL_MASTER_SRCS += $(subst $(SVM_KERNEL_DIR)/,,$(wildcard $(SVM_KERNEL_DIR)/cmd_handle/host/*.c))
SVM_KERNEL_MASTER_SRCS += $(subst $(SVM_KERNEL_DIR)/,,$(wildcard $(SVM_KERNEL_DIR)/kern_lib/*.c))
SVM_KERNEL_MASTER_SRCS += $(subst $(SVM_KERNEL_DIR)/,,$(wildcard $(SVM_KERNEL_DIR)/kern_lib/normal_page/*.c))
SVM_KERNEL_MASTER_SRCS += $(subst $(SVM_KERNEL_DIR)/,,$(wildcard $(SVM_KERNEL_DIR)/kern_lib/compound_large_page/*.c))
SVM_KERNEL_MASTER_SRCS += $(subst $(SVM_KERNEL_DIR)/,,$(wildcard $(SVM_KERNEL_DIR)/k2umc/*.c))
SVM_KERNEL_MASTER_SRCS += $(subst $(SVM_KERNEL_DIR)/,,$(wildcard $(SVM_KERNEL_DIR)/kmc/*.c))
SVM_KERNEL_MASTER_SRCS += $(subst $(SVM_KERNEL_DIR)/,,$(wildcard $(SVM_KERNEL_DIR)/kmc/h2d/*.c))
SVM_KERNEL_MASTER_SRCS += $(subst $(SVM_KERNEL_DIR)/,,$(wildcard $(SVM_KERNEL_DIR)/mpl/*.c))
SVM_KERNEL_MASTER_SRCS += $(subst $(SVM_KERNEL_DIR)/,,$(wildcard $(SVM_KERNEL_DIR)/share/*.c))
SVM_KERNEL_MASTER_SRCS += $(subst $(SVM_KERNEL_DIR)/,,$(wildcard $(SVM_KERNEL_DIR)/share/repair/*.c))
SVM_KERNEL_MASTER_SRCS += $(subst $(SVM_KERNEL_DIR)/,,$(wildcard $(SVM_KERNEL_DIR)/share/smm/*.c))
SVM_KERNEL_MASTER_SRCS += $(subst $(SVM_KERNEL_DIR)/,,$(wildcard $(SVM_KERNEL_DIR)/share/um_handle/*.c))
SVM_KERNEL_MASTER_SRCS += $(subst $(SVM_KERNEL_DIR)/,,$(wildcard $(SVM_KERNEL_DIR)/pmm/*.c))
SVM_KERNEL_MASTER_SRCS += $(subst $(SVM_KERNEL_DIR)/,,$(wildcard $(SVM_KERNEL_DIR)/pmq/*.c))
SVM_KERNEL_MASTER_SRCS += $(subst $(SVM_KERNEL_DIR)/,,$(wildcard $(SVM_KERNEL_DIR)/pmq_adapt/agent/*.c))
SVM_KERNEL_MASTER_SRCS += $(subst $(SVM_KERNEL_DIR)/,,$(wildcard $(SVM_KERNEL_DIR)/pmq_adapt/agent/host/*.c))
SVM_KERNEL_MASTER_SRCS += $(subst $(SVM_KERNEL_DIR)/,,$(wildcard $(SVM_KERNEL_DIR)/mwl_adapt/agent/*.c))
SVM_KERNEL_MASTER_SRCS += $(subst $(SVM_KERNEL_DIR)/,,$(wildcard $(SVM_KERNEL_DIR)/pmq_adapt/client/*.c))
SVM_KERNEL_MASTER_SRCS += $(subst $(SVM_KERNEL_DIR)/,,$(wildcard $(SVM_KERNEL_DIR)/pmq_adapt/client/host/*.c))
SVM_KERNEL_MASTER_SRCS += $(subst $(SVM_KERNEL_DIR)/,,$(wildcard $(SVM_KERNEL_DIR)/memcpy/*.c))
SVM_KERNEL_MASTER_SRCS += $(subst $(SVM_KERNEL_DIR)/,,$(wildcard $(SVM_KERNEL_DIR)/memcpy/async/*.c))
SVM_KERNEL_MASTER_SRCS += $(subst $(SVM_KERNEL_DIR)/,,$(wildcard $(SVM_KERNEL_DIR)/memcpy/copy_ctrl/*.c))
SVM_KERNEL_MASTER_SRCS += $(subst $(SVM_KERNEL_DIR)/,,$(wildcard $(SVM_KERNEL_DIR)/memcpy/dma_desc_ops/*.c))
SVM_KERNEL_MASTER_SRCS += $(subst $(SVM_KERNEL_DIR)/,,$(wildcard $(SVM_KERNEL_DIR)/dma_map/*.c))
SVM_KERNEL_MASTER_SRCS += $(subst $(SVM_KERNEL_DIR)/,,$(wildcard $(SVM_KERNEL_DIR)/pcie_th_adapt/agent/*.c))
SVM_KERNEL_MASTER_SRCS += $(subst $(SVM_KERNEL_DIR)/,,$(wildcard $(SVM_KERNEL_DIR)/smp/*.c))
SVM_KERNEL_MASTER_SRCS += $(subst $(SVM_KERNEL_DIR)/,,$(wildcard $(SVM_KERNEL_DIR)/mwl/*.c))
SVM_KERNEL_MASTER_SRCS += $(subst $(SVM_KERNEL_DIR)/,,$(wildcard $(SVM_KERNEL_DIR)/casm/*.c))
ifeq ($(ENABLE_UBE),true)
    SVM_KERNEL_MASTER_SRCS += $(subst $(SVM_KERNEL_DIR)/,,$(wildcard $(SVM_KERNEL_DIR)/ubmm/*.c))
endif
SVM_KERNEL_MASTER_SRCS += $(subst $(SVM_KERNEL_DIR)/,,$(wildcard $(SVM_KERNEL_DIR)/user_mem_show/*.c))
SVM_KERNEL_MASTER_SRCS += $(subst $(SVM_KERNEL_DIR)/,,$(wildcard $(SVM_KERNEL_DIR)/casm_adapt/cross_server/*.c))
SVM_KERNEL_MASTER_SRCS += $(subst $(SVM_KERNEL_DIR)/,,$(wildcard $(SVM_KERNEL_DIR)/casm_adapt/ubmem/*.c))
SVM_KERNEL_MASTER_SRCS += $(subst $(SVM_KERNEL_DIR)/,,$(wildcard $(SVM_KERNEL_DIR)/ubmem_adapt/client/*.c))
SVM_KERNEL_MASTER_SRCS += $(subst $(SVM_KERNEL_DIR)/,,$(wildcard $(SVM_KERNEL_DIR)/share_log/*.c))
SVM_KERNEL_MASTER_SRCS += $(subst $(SVM_KERNEL_DIR)/,,$(wildcard $(SVM_KERNEL_DIR)/ksvmm/*.c))
SVM_KERNEL_MASTER_SRCS += $(subst $(SVM_KERNEL_DIR)/,,$(wildcard $(SVM_KERNEL_DIR)/pma/*.c))
SVM_KERNEL_MASTER_SRCS += $(subst $(SVM_KERNEL_DIR)/,,$(wildcard $(SVM_KERNEL_DIR)/mms/*.c))
SVM_KERNEL_MASTER_SRCS += $(subst $(SVM_KERNEL_DIR)/,,$(wildcard $(SVM_KERNEL_DIR)/kapi_stats/*.c))
SVM_KERNEL_MASTER_SRCS += $(subst $(SVM_KERNEL_DIR)/,,$(wildcard $(SVM_KERNEL_DIR)/soma/host/*.c))
SVM_KERNEL_MASTER_SRCS += $(subst $(SVM_KERNEL_DIR)/,,$(wildcard $(SVM_KERNEL_DIR)/dma_map/master/*.c))

EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/framework/
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/host_dev/
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/dbi/core/
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/dbi/
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/um_handle/
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/um_handle_adapt/host/
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/cmd_handle/
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/cmd_handle/host/
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/kern_lib/
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/kern_lib/normal_page/
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/kern_lib/compound_large_page/
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/k2umc/
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/kmc/
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/kmc/h2d/
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/mpl/
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/share/
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/share/repair
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/share/smm
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/share/um_handle
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/pmm
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/pmq
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/pmq_adapt
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/pmq_adapt/agent
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/pmq_adapt/agent/host
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/mwl_adapt/agent
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/pmq_adapt/client
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/pmq_adapt/client/host
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/memcpy
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/memcpy/async
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/memcpy/copy_ctrl
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/memcpy/dma_desc_ops
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/dma_map
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/pcie_th_adapt/agent
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/pcie_th_adapt
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/smp
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/mwl
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/casm
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/ubmm
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/user_mem_show
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/casm_adapt/cross_server
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/casm_adapt/ubmem
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/ubmem_adapt/client
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/ubmem_adapt
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/share_log
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/ksvmm
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/pma
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/mms
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/kapi_stats
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/soma/host
EXTRA_CFLAGS += -I$(SVM_KERNEL_DIR)/dma_map/master

EXTRA_CFLAGS += -DCFG_FEATURE_SHARE_LOG
EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
EXTRA_CFLAGS += -DCFG_FEATURE_NOT_SUPPORT_PMA_UB
ifneq ($(filter $(PRODUCT), ascend950),)
    EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
endif

asdrv_svm-objs := $(patsubst %.c,%.o,$(SVM_KERNEL_MASTER_SRCS))

obj-m := asdrv_svm.o
