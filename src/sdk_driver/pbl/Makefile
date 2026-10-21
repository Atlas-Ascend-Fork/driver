# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
ifeq ($(DAVINCI_HIAI_DKMS),y)
	DRIVER_SOURCE_DIR := $(HIAI_DKMS_DIR)
    ifeq ($(Driver_Install_Mode), vnpu_guest)
        MODULE_NAME := asdrv_vpbl
    else
        MODULE_NAME := asdrv_pbl
    endif
else
    DRIVER_SOURCE_DIR := $(DRIVER_KERNEL_DIR)
    ifeq ($(TARGET_TYPE), VPBL)
        MODULE_NAME := asdrv_vpbl
    else
        MODULE_NAME := asdrv_pbl
    endif
endif

obj-m += $(MODULE_NAME).o
ifneq ($(filter $(MODULE_NAME), asdrv_pbl asdrv_vpbl),)
    $(MODULE_NAME)-objs := pbl_module.o
endif

ifeq ($(DAVINCI_HIAI_DKMS),y)
    EXTRA_CFLAGS += -DCFG_ENV_HOST
    ifeq ($(Driver_Install_Mode),vnpu_guest)
        EXTRA_CFLAGS += -DCFG_FEATURE_VPBL
        include $(HIAI_DKMS_DIR)/pbl/drv_davinci_intf_host/drv_davinci_intf.mk
        include $(HIAI_DKMS_DIR)/pbl/log_host/log_host.mk
        include $(HIAI_DKMS_DIR)/pbl/uda/uda.mk
        include $(HIAI_DKMS_DIR)/pbl/runenv_config/runenv_config.mk
        include $(HIAI_DKMS_DIR)/pbl/dev_urd/dev_urd.mk
        include $(HIAI_DKMS_DIR)/pbl/mem_ops/mem_ops.mk
        include $(HIAI_DKMS_DIR)/pbl/ubmm/ubmm.mk
        include ${HIAI_DKMS_DIR}/pbl/msg_chan/msg_chan.mk
        include ${HIAI_DKMS_DIR}/pbl/chip_config/chip_config.mk
    else
        include $(HIAI_DKMS_DIR)/pbl/drv_davinci_intf_host/drv_davinci_intf.mk
        include $(HIAI_DKMS_DIR)/pbl/log_host/log_host.mk
        include $(HIAI_DKMS_DIR)/pbl/uda/uda.mk
        include $(HIAI_DKMS_DIR)/pbl/runenv_config/runenv_config.mk
        include $(HIAI_DKMS_DIR)/pbl/dev_urd/dev_urd.mk
        include $(HIAI_DKMS_DIR)/pbl/mem_ops/mem_ops.mk
        include $(HIAI_DKMS_DIR)/pbl/ubmm/ubmm.mk
        include ${HIAI_DKMS_DIR}/pbl/soc_resmng/soc_resmng.mk
        include ${HIAI_DKMS_DIR}/pbl/msg_chan/msg_chan.mk
        include ${HIAI_DKMS_DIR}/pbl/chip_config/chip_config.mk
    endif
else
    ifeq ($(TARGET_TYPE), VPBL)
        EXTRA_CFLAGS += -DCFG_FEATURE_VPBL
        include ${DRIVER_SOURCE_DIR}/pbl/drv_davinci_intf/drv_davinci_intf_host/drv_davinci_intf.mk
        include ${DRIVER_SOURCE_DIR}/pbl/log_host/log_host.mk
        include ${DRIVER_SOURCE_DIR}/pbl/uda/uda.mk
        include ${DRIVER_SOURCE_DIR}/pbl/runenv_config/runenv_config.mk
        include ${DRIVER_SOURCE_DIR}/pbl/dev_urd/dev_urd.mk
        include ${DRIVER_SOURCE_DIR}/pbl/mem_ops/mem_ops.mk
        include ${DRIVER_SOURCE_DIR}/pbl/ubmm/ubmm.mk
        include ${DRIVER_SOURCE_DIR}/pbl/msg_chan/msg_chan.mk
        include ${DRIVER_SOURCE_DIR}/pbl/chip_config/chip_config.mk
    else
        include ${DRIVER_SOURCE_DIR}/pbl/drv_davinci_intf/drv_davinci_intf_host/drv_davinci_intf.mk
        include ${DRIVER_SOURCE_DIR}/pbl/log_host/log_host.mk
        include ${DRIVER_SOURCE_DIR}/pbl/uda/uda.mk
        include ${DRIVER_SOURCE_DIR}/pbl/runenv_config/runenv_config.mk
        include ${DRIVER_SOURCE_DIR}/pbl/dev_urd/dev_urd.mk
        include ${DRIVER_SOURCE_DIR}/pbl/mem_ops/mem_ops.mk
        include ${DRIVER_SOURCE_DIR}/pbl/ubmm/ubmm.mk
        include ${DRIVER_SOURCE_DIR}/pbl/soc_resmng/soc_resmng.mk
        include ${DRIVER_SOURCE_DIR}/pbl/msg_chan/msg_chan.mk
        include ${DRIVER_SOURCE_DIR}/pbl/chip_config/chip_config.mk
    endif
    include ${DRIVER_SOURCE_DIR}/pbl/feature/feature.mk
    EXTRA_CFLAGS += -DCFG_ENV_HOST
endif

EXTRA_CFLAGS += -Wfloat-equal
EXTRA_CFLAGS += -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers -Wno-format-nonliteral -Wno-empty-body -Wno-type-limits
EXTRA_CFLAGS += -isystem $(shell $(CC) -print-file-name=include)

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
