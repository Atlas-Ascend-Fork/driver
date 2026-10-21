# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------

ifneq ($(NOT_SUPPORT_SP), y)
	EXTRA_CFLAGS += -fstack-protector-all
endif

ccflags-y += -Wall -funsigned-char -Wextra -Werror -Wformat=2 -Wfloat-equal -Wcast-align -Wundef $(WDATE_TIME)
ccflags-y += -Wno-unused-parameter -Wno-sign-compare  -Wno-missing-field-initializers -Wno-format-nonliteral -Wno-empty-body -Wno-missing-prototypes -Wno-missing-declarations
ccflags-y += -fno-common -fstack-protector-all -pipe -s -Wstack-usage=2048 -Wstrict-prototypes -Wtrampolines -Wunused -Wvla

include $(DRIVER_MODULE_DEVMNG_DIR)/drv_devmng/drv_devmng_host/drv_devmng_host.mk
include $(DRIVER_MODULE_DEVMNG_DIR)/dc/dms.mk
include $(DRIVER_MODULE_DEVMNG_DIR)/config/dms_config.mk
include $(DRIVER_MODULE_DEVMNG_DIR)/product/dms_product.mk

EXTRA_CFLAGS += -I$(C_SEC_INCLUDE)
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/trs
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/pbl
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/ascend_platform
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/common
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/src/kernel_adapt/include
EXTRA_CFLAGS += -I$(DRIVER_SOURCE_DIR)/pbl/dev_urd
EXTRA_CFLAGS += -I$(DRIVER_SOURCE_DIR)/fms/smf/event
EXTRA_CFLAGS += -I$(DRIVER_SOURCE_DIR)/fms/smf/sensor
EXTRA_CFLAGS += -I$(DRIVER_SOURCE_DIR)/fms/smf/sensor/config
EXTRA_CFLAGS += -I$(DRIVER_SOURCE_DIR)/fms/soft_fault
EXTRA_CFLAGS += -I$(DRIVER_MODULE_DEVMNG_DIR)
EXTRA_CFLAGS += -I$(DRIVER_MODULE_DEVMNG_DIR)/include
EXTRA_CFLAGS += -I$(DRIVER_MODULE_DEVMNG_DIR)/drv_devmng/drv_devmng_inc
EXTRA_CFLAGS += -I$(DRIVER_HAL_INC_DIR)
EXTRA_CFLAGS += -I$(DRIVER_SOURCE_DIR)/pbl/dev_urd/dc
EXTRA_CFLAGS += -I$(DRIVER_SOURCE_DIR)/pbl/dev_urd/command/ioctl
EXTRA_CFLAGS += -I$(DRIVER_SOURCE_DIR)/dms/command/ioctl
EXTRA_CFLAGS += -I$(DRIVER_SOURCE_DIR)/dms/include

asdrv_dms-y += devmng/drv_devmng/drv_devmng_common/devdrv_spod_info.o
asdrv_dms-y += devmng/dc/status/dms_spod_info.o
