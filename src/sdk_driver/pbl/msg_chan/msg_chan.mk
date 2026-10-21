# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
$(MODULE_NAME)-objs += msg_chan/common/msg_chan_dev_info_comm.o
$(MODULE_NAME)-objs += msg_chan/common/msg_chan_main_comm.o
$(MODULE_NAME)-objs += msg_chan/common/msg_chan_msg_comm.o
$(MODULE_NAME)-objs += msg_chan/common/msg_chan_urd_comm.o
$(MODULE_NAME)-objs += msg_chan/common/pcie_dma_interface.o
$(MODULE_NAME)-objs += msg_chan/host/msg_chan_common_msg.o
$(MODULE_NAME)-objs += msg_chan/host/msg_chan_dev_info.o
$(MODULE_NAME)-objs += msg_chan/host/msg_chan_msg.o
$(MODULE_NAME)-objs += msg_chan/host/msg_chan_main.o
$(MODULE_NAME)-objs += msg_chan/host/msg_chan_trans.o
$(MODULE_NAME)-objs += msg_chan/dc/common/msg_chan_init.o
$(MODULE_NAME)-objs += msg_chan/dc/common/msg_chan_urd_adapt.o
$(MODULE_NAME)-objs += msg_chan/dc/common/msg_chan_rao_adapt.o
$(MODULE_NAME)-objs += msg_chan/dc/host/msg_chan_urd.o
$(MODULE_NAME)-objs += msg_chan/dc/host/msg_chan_s2s.o
$(MODULE_NAME)-objs += msg_chan/dc/host/msg_chan_rao.o

ifeq ($(ENABLE_UBE),true)
	$(MODULE_NAME)-objs += ub_adapt/adapt_ops.o
	$(MODULE_NAME)-objs += ub_adapt/ummu_core_adapt.o
	$(MODULE_NAME)-objs += ub_adapt/ubcore_adapt.o
endif

EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/uba
EXTRA_CFLAGS += -I$(DRIVER_HAL_INC_DIR)
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/pbl
EXTRA_CFLAGS += -I${C_SEC_INCLUDE}
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/pbl/dev_urd
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/pbl/msg_chan/inc
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/pbl/msg_chan/host
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/pbl/ub_adapt

EXTRA_CFLAGS += -Iinclude/linux
EXTRA_CFLAGS += -Iinclude/ub/urma
EXTRA_CFLAGS += -Iinclude/ub/urma/udma
EXTRA_CFLAGS += -isystem $(shell $(CC) -print-file-name=include)

EXTRA_CFLAGS += -DDRV_HOST
EXTRA_CFLAGS += -DCFG_HOST_ENV
EXTRA_CFLAGS += -DCFG_FEATURE_SRIOV
EXTRA_CFLAGS += -DCFG_ENV_HOST
EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG

include $(FEATURE_MK_PATH)
EXTRA_CFLAGS += $(CONFIG_DEFINES)

ccflags-y += -fno-common -fstack-protector-all -funsigned-char -pipe -s -Wall -Wcast-align -Werror -Wfloat-equal
ccflags-y += -Wformat=2 -Wstack-usage=2048 -Wstrict-prototypes -Wtrampolines -Wundef -Wunused -Wvla -Wno-format-nonliteral