# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
##########################################################################
# 1. asdrv_esched.ko in host
###########################################################################
obj-m += asdrv_esched.o
asdrv_esched-objs := common/esched_fops.o \
                     common/esched.o \
                     common/esched_sysfs.o \
                     common/esched_feature_check.o \
                     esched_adapt.o \
                     esched_host_msg.o \
                     esched_host_adapt.o
asdrv_esched-objs += esched_drv_adapt.o \
                     soc/topic_sched_v1.o \
                     soc/topic_sched_v2.o \
                     common/hard_sched/esched_drv.o

EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/esched/command/ioctl/
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/esched/command/msg/
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/esched/dc/
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/esched/dc/host_adapt/
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/esched/dc/host_adapt/common
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/esched/dc/host_adapt/common/hard_sched
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/trs
EXTRA_CFLAGS += -I${C_SEC_INCLUDE}/
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/pbl
EXTRA_CFLAGS += -I$(DRIVER_HAL_INC_DIR)
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/esched/dc/host_adapt/soc/
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/esched/dc/host_adapt/common/hard_sched/
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/esched/dc/host_adapt/common/hard_sched/soc/

EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/trsdrv/trs/inc/
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/trsdrv/trsbase/inc/
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/trsdrv/trs/trs_core/command/msg/
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/trsdrv/trs/trs_core/command/ioctl/
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/dms/command/ioctl
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/esched/dc/host_adapt/host_comm_adapt/
EXTRA_CFLAGS += -isystem $(shell $(CC) -print-file-name=include)

ifeq ($(DAVINCI_HIAI_DKMS), y)
	EXTRA_CFLAGS += -I$(HIAI_DKMS_DIR)/kernel_adapt/include
else
	EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/kernel_adapt/include
endif

EXTRA_CFLAGS += -DCFG_ENV_HOST -DCFG_FEATURE_SHARE_LOG -DCFG_FEATURE_HOST_LOG -DCFG_FEATURE_KA_ALLOC_INTERFACE
EXTRA_CFLAGS += -DCFG_FEATURE_IDENTIFY_CP -DCFG_FEATURE_EXTERNAL_CDEV -DCFG_FEATURE_REMOTE_SUBMIT -DCFG_FEATURE_HARDWARE_SCHED \
	-DCFG_FEATURE_HARDWARE_MIA

ifneq ($(filter $(PRODUCT), ascend910B),)
	EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/esched/common/
	EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/esched/dc/host_adapt/common/hard_sched/soc/topic_sched_v1/
	EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/esched/command/msg/sched_event_msg_len_v1/
	asdrv_esched-objs += common/hard_sched/soc/topic_sched_v1/topic_sched_drv.o
	asdrv_esched-objs += esched_host_topic_v1.o
endif

ifneq ($(filter $(PRODUCT), ascend950),)
	EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/esched/host/soc/
	EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/esched/dc/host_adapt/common/hard_sched/soc/topic_sched_v2/
	EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/esched/command/msg/sched_event_msg_len_v2/
	EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD_V4
	asdrv_esched-objs += common/hard_sched/soc/topic_sched_v2/topic_sched_drv.o
	asdrv_esched-objs += esched_host_topic_v2.o
endif
ccflags-y += -fno-common -fstack-protector-all -funsigned-char -pipe -s -Wall -Wcast-align -Wdate-time -Wfloat-equal -Wformat -Wstack-usage=2048 -Wstrict-prototypes -Wtrampolines -Wundef -Wunused -Wvla
