# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------

MODULE_NAME := asdrv_ub
obj-m :=$(MODULE_NAME).o
$(MODULE_NAME)-objs := common/asdrv_ub_main.o
$(MODULE_NAME)-objs += common/ub_res/asdrv_ub_jetty.o
$(MODULE_NAME)-objs += common/msg/asdrv_ub_admin_msg.o
$(MODULE_NAME)-objs += common/msg/asdrv_ub_urma_chan.o
$(MODULE_NAME)-objs += common/load_image/asdrv_ub_path.o
$(MODULE_NAME)-objs += common/msg/asdrv_ub_non_trans_chan.o
$(MODULE_NAME)-objs += common/msg/asdrv_ub_common_msg.o
$(MODULE_NAME)-objs += common/msg/asdrv_ub_msg.o
$(MODULE_NAME)-objs += common/unit/asdrv_ub_proc_fs.o
$(MODULE_NAME)-objs += common/unit/asdrv_ub_urd.o
$(MODULE_NAME)-objs += common/dev_online/asdrv_ub_link_chan.o
$(MODULE_NAME)-objs += common/dev_online/asdrv_ub_pair_dev_info.o
$(MODULE_NAME)-objs += common/unit/asdrv_ub_rao.o
$(MODULE_NAME)-objs += common/unit/asdrv_ub_mem_decoder.o
$(MODULE_NAME)-objs += host/dev_online/asdrv_ub_dev_online.o
$(MODULE_NAME)-objs += host/load_image/asdrv_ub_load.o
$(MODULE_NAME)-objs += host/msg/asdrv_ub_msg.o
$(MODULE_NAME)-objs += host/msg/asdrv_ub_non_trans_chan.o
$(MODULE_NAME)-objs += host/unit/asdrv_ub_hotreset.o
$(MODULE_NAME)-objs += host/unit/asdrv_ub_mem_decoder.o
$(MODULE_NAME)-objs += host/unit/asdrv_ub_proc_fs.o
$(MODULE_NAME)-objs += host/unit/asdrv_ub_rao.o
$(MODULE_NAME)-objs += host/unit/asdrv_ub_urd.o
$(MODULE_NAME)-objs += host/asdrv_ub_main_adapt.o
$(MODULE_NAME)-objs += common/ummu_adapt/asdrv_ub_ummu_adapt.o
$(MODULE_NAME)-objs += common/ubcore_adapt/asdrv_ub_ubcore_adapt.o

EXTRA_CFLAGS += -I$(DRIVER_HAL_INC_DIR)
EXTRA_CFLAGS += -I${C_SEC_INCLUDE}
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/uba
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/pbl/ub_adapt
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/kernel_adapt/include/
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/comm/ub/command/msg/
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/pbl/msg_chan/command/msg/
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/comm/ub/host/
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/pbl/dev_urd
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/pbl/dev_urd/dc
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/pbl/uda/command/ioctl
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/comm/ub/inc/
EXTRA_CFLAGS += -Iinclude/linux
EXTRA_CFLAGS += -Iinclude/ub/urma
EXTRA_CFLAGS += -Iinclude/ub/urma/udma
EXTRA_CFLAGS += -isystem $(shell $(CC) -print-file-name=include)

EXTRA_CFLAGS += -DDRV_HOST
EXTRA_CFLAGS += -DCFG_ENV_HOST
EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
EXTRA_CFLAGS += -DCFG_FEATURE_DRV_LOG
EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
EXTRA_CFLAGS += -DSSAPI_USE_MAMI
EXTRA_CFLAGS += -DCFG_FEATURE_CDMA_LOAD_IMAGE
EXTRA_CFLAGS += -DCFG_FEATURE_SUPPORT_RUN_INSTALL

# /* for chip macro */
include $(FEATURE_MK_PATH)
EXTRA_CFLAGS += $(CONFIG_DEFINES)

ifneq ($(NOT_SUPPORT_SP), y)
    EXTRA_CFLAGS += -fstack-protector-all
endif

ccflags-y += -fno-common -fstack-protector-all -funsigned-char -pipe -s -Wall -Wcast-align -Werror -Wfloat-equal
ccflags-y += -Wformat=2 -Wstack-usage=2048 -Wstrict-prototypes -Wtrampolines -Wundef -Wunused -Wvla -Wno-format-nonliteral
ccflags-y += $(WDATE_TIME) -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers -Wno-empty-body