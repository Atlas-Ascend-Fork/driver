# ------------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ------------------------------------------------------------------------------------------------------------

    asdrv_svm-objs += master/pmaster/svm_shmem_node_pod.o
    asdrv_svm-objs += master/comm/svm_ref_server_occupier.o  master/comm/svm_recycle_thread.o
    asdrv_svm-objs += master/pmaster/msg/svm_msg_client.o master/pmaster/msg/svm_host_msg_client.o master/pmaster/msg/svm_device_msg_client.o
    asdrv_svm-objs += master/comm/svm_master_mem_mng.o master/comm/svm_master_proc_fs.o master/comm/svm_master_mem_stats.o master/comm/svm_master_feature_proc_fs.o
    asdrv_svm-objs += master/comm/devmm_proc_info.o master/comm/devmm_channel.o master/comm/svm_master_addr_map.o master/comm/svm_master_memset.o master/comm/svm_master_vma_ops.o master/comm/svm_master_mem_repair.o master/comm/svm_master_dev_capability.o master/comm/svm_master_phy_allocator.o
    asdrv_svm-objs += master/comm/svm_master_convert.o master/comm/svm_master_advise.o master/comm/svm_master_get_host_info.o master/comm/svm_master_cgroup.o master/comm/svm_master_proc_mng.o master/comm/svm_master_memcpy.o
    asdrv_svm-objs += master/comm/svm_dev_res_mng.o master/comm/svm_task_dev_res_mng.o master/comm/svm_master_dma_desc_mng.o master/comm/svm_master_mem_create.o master/comm/svm_master_mem_map.o master/comm/svm_master_addr_ref_ops.o master/comm/svm_master_mem_share.o
    asdrv_svm-objs += master/pmaster/svm_pa_trans_master.o master/pmaster/svm_vmm_access_master.o
    asdrv_svm-objs += master/pmaster/devmm_proc_mem_copy.o master/pmaster/svm_master_remote_map.o master/pmaster/svm_shmem_interprocess.o master/pmaster/svm_master_pm_proc_mng.o master/pmaster/svm_dma_prepare_pool.o
    asdrv_svm-objs += common/svm_dma.o common/svm_mem_query.o common/devmm_page_cache.o common/svm_proc_mng.o common/devmm_dev.o common/devmm_common.o common/svm_heap_mng.o common/svm_mmu_notifier.o common/svm_dynamic_addr.o
    asdrv_svm-objs += common/svm_pci_dev_tbl.o common/svm_module_ops.o common/svm_mem_mng.o common/svm_res_idr.o common/svm_version_adapt.o common/svm_proc_fs.o common/devmm_register_dma.o
    asdrv_svm-objs += common/svm_rbtree.o common/svm_srcu_work.o common/svm_page_cnt_stats.o common/svm_gfp.o  common/svm_dma_map.o common/svm_mem_split.o common/svm_log.o common/svm_kapi_stats.o
    asdrv_svm-objs += common/svm_proc_gfp.o common/svm_phy_addr_blk_mng.o common/svm_mem_create.o common/svm_mem_share.o common/svm_mem_map.o common/svm_vmma_mng.o common/svm_pa_trans_comm.o common/svm_vmm_access_comm.o common/devmm_addr_mng.o
    asdrv_svm-objs += master/pmaster/svm_master_query.o master/pmaster/devmm_pagefault_info.o
    asdrv_svm-objs += master/pmaster/svm_master_register_ops.o master/pmaster/svm_hot_reset.o master/pmaster/svm_master_process_status_mng.o master/pmaster/svm_pmaster_mem_map.o master/pmaster/svm_shmem_node.o master/pmaster/svm_shmem_procfs.o
    asdrv_svm-objs += master/comm/svm_master_soma_pool_mng.o master/comm/soma_master_vma_ops.o
ifndef UVM_OPEN
    asdrv_svm-objs += master/comm/uvm_master_vma_ops.o master/comm/uvm_channel.o master/comm/uvm_alloc_free.o master/comm/uvm_master_memset.o master/comm/uvm_advise.o master/comm/uvm_master_prefetch.o
    asdrv_svm-objs += common/uvm_async_task.o common/uvm_kthread_pool.o common/uvm_heap_mng.o
endif
    obj-m := asdrv_svm.o

    EXTRA_CFLAGS += -I$(DRIVER_HAL_INC_DIR)
    EXTRA_CFLAGS += -I$(C_SEC_INCLUDE)
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc/pbl
    EXTRA_CFLAGS += -I${DRIVER_KERNEL_DIR}/svm/v2/command/msg
    EXTRA_CFLAGS += -I${DRIVER_KERNEL_DIR}/svm/v2/command/ioctl
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/svm/v2/master
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/svm/v2/common
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/svm/v2/common/inc
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/svm/v2/master/comm
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/svm/v2/master/comm/inc
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/svm/v2/master/pmaster
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/svm/v2/master/pmaster/msg
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/svm/v2/master/pmaster/pm_adapt
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/dms/devmng/include
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/kernel_adapt/include
    EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/dms/command/ioctl
    EXTRA_CFLAGS += -isystem $(shell $(CC) -print-file-name=include)

    EXTRA_CFLAGS += -DDRV_HOST
    EXTRA_CFLAGS += -DCFG_FEATURE_SHARE_LOG
    EXTRA_CFLAGS += -DCFG_FEATURE_HOST_LOG
    EXTRA_CFLAGS += -DCFG_FEATURE_KA_ALLOC_INTERFACE
    EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD_HOST
    EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD
    EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_CLOUD_V2               #temporary

    ccflags-y += -fno-common -fstack-protector-all -funsigned-char -pipe -s -Wall -Wcast-align -Werror -Wdate-time -Wfloat-equal -Wformat -Wstack-usage=2048 -Wstrict-prototypes -Wtrampolines -Wundef -Wunused -Wvla
