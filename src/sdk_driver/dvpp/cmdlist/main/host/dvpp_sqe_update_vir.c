/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "dvpp_sqe_update_vir.h"

#include "ka_memory_pub.h"

#include "securec.h"
#include "hw_vdavinci.h"
#include "comm_kernel_interface.h"
#include "vmng_kernel_interface.h"

#include "dvpp_dma_mem_ctrl.h"
#include "dvpp_tlv_decoder.h"
#include "dvpp_cmdlist.h"
#include "dvpp_cmdlist_log.h"
#include "dvpp_cmdlist_context.h"
#include "dvpp_cmdlist_util.h"
#include "dvpp_sqe_update_comm.h"
#include "dvpp_share_mem_ctrl.h"

static void resource_destroy(dvpp_comm_para *comm_para, dvpp_save_para *save_para, dvpp_dma_mem *dma_mem,
                             uint64_t *tlv_stru_addr)
{
    dvpp_dma_free(comm_para->pf_id, save_para->cmdbuf_size, (void *)(dma_mem->vir_addr), dma_mem->phy_addr);
    dma_mem->vir_addr = 0;

    ka_mm_vfree((void *)(*tlv_stru_addr));
    *tlv_stru_addr = 0;
}

static uint64_t transfer_to_pm_share_mem_addr(uint64_t vm_share_mem_addr, uint32_t devid)
{
    return vm_share_mem_addr - g_share_mem_pool[devid]->addr + (uintptr_t)(g_share_mem_pool[devid]) +
           sizeof(dvpp_share_mem_pool);
}

static int32_t get_pf_vf_id(uint32_t dev_id, uint32_t *pf_id, uint32_t *vf_id)
{
    int32_t ret = devdrv_get_pfvf_id_by_devid(dev_id, pf_id, vf_id);
    if (ret != 0) {
        DVPP_CMDLIST_LOG_ERROR("devdrv_get_pfvf_id_by_devid fail, ret:%d, dev_id:%u\n", ret, dev_id);
        return -1;
    }

    DVPP_CMDLIST_LOG_DEBUG("dev_id:%d, pf_id:%u, vf_id:%u\n", dev_id, *pf_id, *vf_id);

    return 0;
}

static int32_t dvpp_drv_ready(dvpp_sqe_args *sqe_args, dvpp_comm_para *para, uint32_t devid)
{
    int32_t ret = 0;
    dvpp_dma_mem dma_mem;
    dvpp_share_blk *blk = NULL;
    dvpp_svm_pa_info *pa_info = NULL;
    dvpp_save_para save_para;
    uint64_t tlv_stru_addr = 0;
    uint64_t cmdbuf_size_used = 0;
    uint64_t blk_addr = 0;
    uint64_t cache_blk_addr = 0;
    // 虚拟机下把相对于共享内存起始地址的偏移存放在args_addr中
    uint64_t offset = sqe_args->args_addr;
    if (offset >= g_share_mem_pool[devid]->size) {
        DVPP_CMDLIST_LOG_ERROR("blk offset %llu beyond max value %llu.\n", offset, g_share_mem_pool[devid]->size);
        goto FAIL0;
    }
    blk = (dvpp_share_blk *)(offset + (uintptr_t)(g_share_mem_pool[devid]));

    tlv_stru_addr = (uint64_t)ka_mm_vmalloc(sizeof(dvpp_tlv_type_struct) + blk->size);
    if (tlv_stru_addr == 0) {
        DVPP_CMDLIST_LOG_ERROR("alloc tlv struct addr fail.\n");
        goto FAIL0;
    }
    blk_addr = transfer_to_pm_share_mem_addr(blk->addr, devid);
    cache_blk_addr = tlv_stru_addr + sizeof(dvpp_tlv_type_struct);
    ret = memcpy_s((void *)(cache_blk_addr), blk->size, (void *)(blk_addr), blk->size);
    if (ret != EOK) {
        DVPP_CMDLIST_LOG_ERROR("cache share blk addr fail.\n");
        goto FAIL1;
    }
    sqe_args->args_addr = cache_blk_addr;
    pa_info = (dvpp_svm_pa_info *)(sqe_args->args_addr + sqe_args->args_size);
    pa_info->pa_list = (uint64_t *)((uint64_t)(uintptr_t)(pa_info->pa_list) - blk->addr + cache_blk_addr);
    pa_info->size_list = (uint64_t *)((uint64_t)(uintptr_t)(pa_info->size_list) - blk->addr + cache_blk_addr);

    // 用pf的能力做dma内存申请
    dma_mem.vir_addr = (uint64_t)dvpp_dma_alloc(para->pf_id, sqe_args->cmdbuf_size, &dma_mem.phy_addr, KA_GFP_ATOMIC);
    if (dma_mem.vir_addr == 0) {
        DVPP_CMDLIST_LOG_ERROR("alloc host dma memory fail, size:%d\n", sqe_args->cmdbuf_size);
        goto FAIL1;
    }
    sqe_args->cmdbuf_kva = dma_mem.vir_addr;
    // 这里保存是因为生成sqe的时候可能会修改下面的值，后续释放内存就会出错
    save_para.cmdbuf_size = sqe_args->cmdbuf_size;
    if (dvpp_comm_drv_ready(sqe_args, tlv_stru_addr, &cmdbuf_size_used) != 0) {
        DVPP_CMDLIST_LOG_ERROR("dvpp drv comm ready fail.\n");
        goto FAIL2;
    }

    // 用pf的能力做pcie拷贝
    ret = dvpp_dma_sync_link_copy(para->pf_id, &dma_mem, pa_info);
    if (ret != 0) {
        DVPP_CMDLIST_LOG_ERROR("pcie copy host dma memory to device fail. ret:%d\n", ret);
        goto FAIL2;
    }

    dvpp_put_share_mem_blk_to_pool(blk);
    resource_destroy(para, &save_para, &dma_mem, &tlv_stru_addr);
    return 0;

FAIL2:
    dvpp_dma_free(para->pf_id, save_para.cmdbuf_size, (void *)(dma_mem.vir_addr), dma_mem.phy_addr);
    dma_mem.vir_addr = 0;
FAIL1:
    ka_mm_vfree((void *)(tlv_stru_addr));
    tlv_stru_addr = 0;
FAIL0:
    dvpp_put_share_mem_blk_to_pool(blk);
    return -1;
}

static dvpp_share_mem_pool *dvpp_get_share_mem_pool(uint32_t pf_id, uint32_t vf_id, dvpp_sqe_args *sqe_args)
{
    int32_t ret = 0;
    void *vdavinci;
    uint64_t addr;
    int32_t size;
    const int32_t BAR_INDEX = 1;

    vdavinci = vmngh_get_vdavinci_by_id(pf_id, vf_id);
    if (vdavinci == NULL) {
        return NULL;
    }

    ret = hw_dvt_hypervisor_mmio_get((void **)(&addr), &size, vdavinci, BAR_INDEX);
    if (ret != 0) {
        DVPP_CMDLIST_LOG_ERROR("get dvpp share mem addr fail, ret:%d, size:%d\n", ret, size);
        return NULL;
    }

    DVPP_CMDLIST_LOG_DEBUG("dvpp share mem size:%d\n", size);

    return (dvpp_share_mem_pool *)(addr);
}

// 虚拟机场景：args内存已是内核态地址，并在后半段保存了svm pa地址信息
int32_t dvpp_sqe_update_vir_mach(uint32_t devid, int pid, void *sqe)
{
    int32_t ret = 0;
    dvpp_sqe_args *sqe_args = NULL;
    dvpp_comm_para comm_para;

    sqe_args = (dvpp_sqe_args *)sqe;

    ret = get_pf_vf_id(devid, &comm_para.pf_id, &comm_para.vf_id);
    if (ret != 0) {
        return -1;
    }

    if (g_share_mem_pool[devid] == NULL) {
        ka_task_spin_lock(&g_share_mem_pool_lock[devid]);
        if (g_share_mem_pool[devid] == NULL) {
            g_share_mem_pool[devid] = dvpp_get_share_mem_pool(comm_para.pf_id, comm_para.vf_id, sqe_args);
        }
        ka_task_spin_unlock(&g_share_mem_pool_lock[devid]);
    }

    if (g_share_mem_pool[devid] == NULL) {
        DVPP_CMDLIST_LOG_ERROR("share mem base addr is null.\n");
        return -1;
    }

    comm_para.devid = devid;
    comm_para.pid = pid;
    if (dvpp_drv_ready(sqe_args, &comm_para, devid) != 0) {
        DVPP_CMDLIST_LOG_ERROR("dvpp drv ready fail.\n");
        return -1;
    }

    return 0;
}
