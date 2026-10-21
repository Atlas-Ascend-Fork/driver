# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
ifeq ($(ENABLE_OPEN_SRC), y)
    LOCAL_PATH := $(DRIVER_KERNEL_DIR)/dvpp/cmdlist

    obj-m   += vdrv_dvpp_cmdlist.o

    vdrv_dvpp_cmdlist-objs := dvpp_vdrv_init.o dvpp_trs_ops.o
    vdrv_dvpp_cmdlist-objs += vdvpp_sqe_para_check.o
    vdrv_dvpp_cmdlist-objs += vdvpp_share_mem_ctrl.o
    vdrv_dvpp_cmdlist-objs += vdvpp_svm_mem_ctrl.o
    vdrv_dvpp_cmdlist-objs += vdvpp_vcm_agent.o
    vdrv_dvpp_cmdlist-objs += vdvpp_cmdlist_ioctl.o

    EXTRA_CFLAGS += -I$(LOCAL_PATH)/main/host \
                    -I$(LOCAL_PATH)/main/host/mem_ctrl \
                    -I$(LOCAL_PATH)/main/comm \
                    -I$(LOCAL_PATH)/base \
                    -I$(LOCAL_PATH)/cmdlist \
                    -I$(LOCAL_PATH)/proc \
                    -I$(LOCAL_PATH)/context \
                    -I$(LOCAL_PATH)/vdrv \
                    -I$(LOCAL_PATH)/decoder \
                    -I$(LOCAL_PATH)/vmng \
                    -I$(LOCAL_PATH)/command/ioctl

    EXTRA_CFLAGS += -I$(DRIVER_HAL_INC_DIR)
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/svm/v2/common
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/svm/v2/command/ioctl
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/ts_agent/inc
    EXTRA_CFLAGS += -I${DRIVER_HAL_INC_DIR}/depends/libc_sec
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/kernel_adapt/include
    EXTRA_CFLAGS += -I${C_SEC_INCLUDE}

    EXTRA_CFLAGS += -Iinclude/linux
else
    ifeq ($(DAVINCI_HIAI_DKMS), y)
        LOCAL_PATH := $(HIAI_DKMS_DIR)/dvpp_cmdlist
        include $(LOCAL_PATH)/vdrv/Makefile_dkms
    else
        LOCAL_PATH := $(DRIVER_KERNEL_DIR)/dvpp/cmdlist

        obj-m   += vdrv_dvpp_cmdlist.o

        vdrv_dvpp_cmdlist-objs := dvpp_vdrv_init.o dvpp_trs_ops.o
        vdrv_dvpp_cmdlist-objs += vdvpp_sqe_para_check.o
        vdrv_dvpp_cmdlist-objs += vdvpp_share_mem_ctrl.o
        vdrv_dvpp_cmdlist-objs += vdvpp_svm_mem_ctrl.o
        vdrv_dvpp_cmdlist-objs += vdvpp_vcm_agent.o
        vdrv_dvpp_cmdlist-objs += vdvpp_cmdlist_ioctl.o

        EXTRA_CFLAGS += -I$(LOCAL_PATH)/main/host \
                        -I$(LOCAL_PATH)/main/host/mem_ctrl \
                        -I$(LOCAL_PATH)/main/comm \
                        -I$(LOCAL_PATH)/base \
                        -I$(LOCAL_PATH)/cmdlist \
                        -I$(LOCAL_PATH)/proc \
                        -I$(LOCAL_PATH)/context \
                        -I$(LOCAL_PATH)/vdrv \
                        -I$(LOCAL_PATH)/decoder \
                        -I$(LOCAL_PATH)/vmng \
                        -I$(LOCAL_PATH)/command/ioctl

        EXTRA_CFLAGS += -I$(DRIVER_HAL_INC_DIR)
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/svm/v2/common
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/ts_agent/inc
        EXTRA_CFLAGS += -I${DRIVER_HAL_INC_DIR}/depends/libc_sec
        EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/kernel_adapt/include
    endif
endif
