# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------
asdrv_dpa-objs += apm/apm_init.o apm/apm_fops.o apm/apm_proc_fs.o apm/apm_kern_log.o
asdrv_dpa-objs += apm/msg/apm_msg.o
asdrv_dpa-objs += apm/task_group/apm_master_domain.o apm/task_group/apm_master.o apm/task_group/apm_slave_domain.o apm/task_group/apm_slave.o apm/task_group/apm_task_exit.o
asdrv_dpa-objs += apm/res_map/apm_res_map.o apm/res_map/apm_res_map_ctx.o apm/res_map/apm_res_map_ops.o
asdrv_dpa-objs += apm/slave_ssid/apm_slave_ssid.o
asdrv_dpa-objs += apm/task_group/apm_slave_meminfo.o apm/task_group/apm_proc_mem_query_handle.o
asdrv_dpa-objs += apm/msg/host/apm_host_msg.o
asdrv_dpa-objs += apm/task_group/proxy_adapt/host/apm_device_slave_proxy_domain.o apm/task_group/proxy_adapt/host/apm_host_proxy.o
asdrv_dpa-objs += apm/res_map/proxy_adapt/host/apm_res_map_host.o
asdrv_dpa-objs += apm/slave_ssid/adapt/host/apm_remote_slave_ssid.o
asdrv_dpa-objs += apm/apm_dev.o

DRIVER_SRC_DPA_BASE_DIR := $(DRIVER_KERNEL_DIR)/dpa
EXTRA_CFLAGS += -I$(C_SEC_INCLUDE)
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/pbl
EXTRA_CFLAGS += -I$(DRIVER_HAL_INC_DIR)
EXTRA_CFLAGS += -I$(DRIVER_SRC_DPA_BASE_DIR)/apm
EXTRA_CFLAGS += -I$(DRIVER_SRC_DPA_BASE_DIR)/apm/msg
EXTRA_CFLAGS += -I$(DRIVER_SRC_DPA_BASE_DIR)/apm/task_group
EXTRA_CFLAGS += -I$(DRIVER_SRC_DPA_BASE_DIR)/apm/task_group/proxy_adapt
EXTRA_CFLAGS += -I$(DRIVER_SRC_DPA_BASE_DIR)/apm/res_map
EXTRA_CFLAGS += -I$(DRIVER_SRC_DPA_BASE_DIR)/apm/res_map/proxy_adapt
EXTRA_CFLAGS += -I$(DRIVER_SRC_DPA_BASE_DIR)/apm/slave_ssid
EXTRA_CFLAGS += -I$(DRIVER_SRC_DPA_BASE_DIR)/apm/command/ioctl
EXTRA_CFLAGS += -I$(DRIVER_SRC_DPA_BASE_DIR)/apm/command/msg
EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc
EXTRA_CFLAGS += -I$(DRIVER_SRC_DPA_BASE_DIR)/apm/msg/host
EXTRA_CFLAGS += -I$(DRIVER_SRC_DPA_BASE_DIR)/apm/task_group/proxy_adapt/host
EXTRA_CFLAGS += -I$(DRIVER_SRC_DPA_BASE_DIR)/apm/res_map/proxy_adapt/host
EXTRA_CFLAGS += -I$(DRIVER_SRC_DPA_BASE_DIR)/apm/slave_ssid/adapt/host

EXTRA_CFLAGS += -DCFG_FEATURE_SHARE_LOG
EXTRA_CFLAGS += -DDRV_HOST
#Print info and warning logs to memory, not to printk
EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
EXTRA_CFLAGS += -isystem $(shell $(CC) -print-file-name=include)
EXTRA_CFLAGS += -Iinclude/linux

ifneq ($(filter $(PRODUCT), ascend950),)
    EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
endif

ccflags-y += -fno-common -fstack-protector-all -funsigned-char -pipe -s -Wall -Wcast-align -Wdate-time -Werror -Wfloat-equal -Wformat -Wstack-usage=2048 -Wstrict-prototypes -Wtrampolines -Wundef -Wunused -Wvla
