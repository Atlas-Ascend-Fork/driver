/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
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
#include "ka_base_pub.h"

#include "devmm_common.h"
#include "svm_msg_client.h"

#include "svm_dynamic_addr.h"
#include "svm_pa_trans.h"
#include "svm_vmm_access_comm.h"
#include "svm_vmm_access_master.h"
#include "svm_vmm_access_msg.h"
#include "svm_master_mem_map.h"
#include "svm_mem_map.h"
#include "svm_proc_mng.h"

#define SVM_VMM_D2H_ACCESS_MAX_SEG_NUM DEVMM_PAGE_NUM_PER_MSG
#define SVM_VMM_H2D_ACCESS_MAX_SEG_NUM DEVMM_PAGE_NUM_PER_MSG
#define SVM_VMM_D2D_ACCESS_MAX_SEG_NUM DEVMM_P2P_PAGE_MAX_NUM_QUERY_MSG

enum svm_vmm_access_dir {
    SVM_VMM_ACCESS_DIR_H2D,
    SVM_VMM_ACCESS_DIR_D2H,
    SVM_VMM_ACCESS_DIR_D2D,
    SVM_VMM_ACCESS_DIR_INVALID
};

static enum svm_vmm_access_dir svm_vmm_get_access_dir(const struct devmm_vmma_struct *vmma, u32 access_udevid)
{
    if (access_udevid == vmma->info.devid) {
        return SVM_VMM_ACCESS_DIR_INVALID;
    }
    if (access_udevid == uda_get_host_id()) {
        return SVM_VMM_ACCESS_DIR_H2D;
    }
    if (vmma->info.devid == uda_get_host_id()) {
        return SVM_VMM_ACCESS_DIR_D2H;
    }
    return SVM_VMM_ACCESS_DIR_D2D;
}

static u64 svm_vmm_get_set_access_max_single_size(struct devmm_vmma_struct *vmma, u32 access_udevid)
{
    enum svm_vmm_access_dir dir = svm_vmm_get_access_dir(vmma, access_udevid);

    switch (dir) {
        case SVM_VMM_ACCESS_DIR_H2D:
            return (vmma->info.pa_pg_size * SVM_VMM_H2D_ACCESS_MAX_SEG_NUM);
        case SVM_VMM_ACCESS_DIR_D2H:
            return (vmma->info.pa_pg_size * SVM_VMM_D2H_ACCESS_MAX_SEG_NUM);
        case SVM_VMM_ACCESS_DIR_D2D:
            return (vmma->info.pa_pg_size * SVM_VMM_D2D_ACCESS_MAX_SEG_NUM);
        default:
            return 0;
    }
}

static int svm_vmm_access_va_map_remote(struct devmm_svm_process *svm_proc, u32 access_udevid, drv_mem_access_type type,
                                        u64 va, u64 size, const struct svm_pa_seg pa_seg[], u32 seg_num)
{
    struct svm_vmm_access_va_map_msg *msg = NULL;
    u32 in_msg_len = (u32)(sizeof(struct svm_vmm_access_va_map_msg) + sizeof(struct svm_pa_seg) * seg_num);
    u32 i;
    int ret;

    msg = devmm_kvzalloc_ex(in_msg_len, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (msg == NULL) {
#ifndef EMU_ST
        devmm_drv_err("Alloc remote va map msg failed. (seg_num=%u)\n", seg_num);
        return -ENOMEM;
#endif
    }

    msg->head.msg_id = DEVMM_VMM_ACCESS_VA_MAP_ID;
    msg->head.process_id.hostpid = svm_proc->process_id.hostpid;
    msg->head.process_id.vfid = 0;
    msg->head.dev_id = (u16)access_udevid;
    msg->head.extend_num = (u16)seg_num;
    msg->access_type = (u32)type;
    msg->va = va;
    msg->size = size;
    for (i = 0; i < seg_num; i++) {
        msg->pa_seg[i] = pa_seg[i];
    }

    ret = devmm_chan_msg_send(msg, in_msg_len, (u32)sizeof(struct svm_vmm_access_va_map_msg));
    if (ret != 0) {
#ifndef EMU_ST
        devmm_drv_err("Map remote va failed. (devid=%u; va=0x%llx; size=%llu; ret=%d)\n", access_udevid, va, size, ret);
        devmm_kvfree_ex(msg);
        return ret;
#endif
    }

    devmm_kvfree_ex(msg);
    return 0;
}

static int svm_vmm_access_va_unmap_remote(struct devmm_svm_process *svm_proc, u32 access_udevid, u64 va, u64 size)
{
    struct svm_vmm_access_va_unmap_msg msg = {0};
    int ret;

    msg.head.msg_id = DEVMM_VMM_ACCESS_VA_UNMAP_ID;
    msg.head.process_id.hostpid = svm_proc->process_id.hostpid;
    msg.head.process_id.vfid = 0;
    msg.head.dev_id = (u16)access_udevid;
    msg.va = va;
    msg.size = size;

    ret = devmm_chan_msg_send(&msg, sizeof(msg), sizeof(msg));
    if (ret != 0) {
#ifndef EMU_ST
        devmm_drv_err("Unmap remote va failed. (devid=%u; va=0x%llx; size=%llu; ret=%d)\n", access_udevid, va, size,
                      ret);
        return ret;
#endif
    }

    return 0;
}

static int svm_vmm_set_d2d_access_remote(struct devmm_svm_process *svm_proc, u32 access_udevid, u32 access_type, u64 va,
                                         u64 size, const struct svm_vmm_pa_owner_desc *pa_owner_desc, u64 pa_pg_size,
                                         u64 pa_offset)
{
    struct svm_vmm_set_d2d_access_msg msg = {0};
    int ret;

    msg.head.msg_id = DEVMM_VMM_SET_D2D_ACCESS_ID;
    msg.head.process_id.hostpid = svm_proc->process_id.hostpid;
    msg.head.process_id.vfid = 0;
    msg.head.dev_id = (u16)access_udevid;
    msg.pa_owner_desc = *pa_owner_desc;
    msg.access_type = access_type;
    msg.pa_offset = pa_offset;
    msg.pa_pg_size = pa_pg_size;
    msg.va = va;
    msg.size = size;

    ret = devmm_chan_msg_send(&msg, (u32)sizeof(msg), (u32)sizeof(msg));
    if (ret != 0) {
#ifndef EMU_ST
        devmm_drv_err("Set remote d2d access failed. (devid=%u; va=0x%llx; size=%llu; ret=%d)\n", access_udevid, va,
                      size, ret);
        return ret;
#endif
    }

    return 0;
}

static int svm_vmm_unset_access_remote(struct devmm_svm_process *svm_proc, u32 access_udevid, u64 va, u64 size,
                                       u64 page_size)
{
    u64 max_single_size = (u64)SVM_VMM_D2H_ACCESS_MAX_SEG_NUM * page_size;
    u64 finished_size = 0;
    int result = 0;

    while (finished_size < size) {
        u64 single_va = va + finished_size;
        u64 single_size = ka_base_min(size - finished_size, max_single_size);
        int ret;

        ret = svm_vmm_access_va_unmap_remote(svm_proc, access_udevid, single_va, single_size);
        if (ret != 0) {
#ifndef EMU_ST
            devmm_drv_err("Remote access unmap failed. (va=0x%llx; size=%llu; devid=%u; ret=%d)\n", single_va,
                          single_size, access_udevid, ret);
            result = ret;
#endif
        }
        finished_size += single_size;
    }
    return result;
}

static int svm_vmm_set_h2d_access(struct devmm_svm_process *svm_proc, struct devmm_vmma_struct *vmma,
                                  drv_mem_access_type type, u64 offset, u64 size)
{
    struct svm_vmm_pa_owner_desc pa_owner_desc;
    struct devmm_svm_heap *heap = NULL;
    struct svm_pa_seg *pa_seg = NULL;
    u32 seg_num = (u32)(size / vmma->info.pa_pg_size);
    u64 pa_offset = vmma->info.offset_pg_num * vmma->info.pa_pg_size + offset;
    u64 va = vmma->info.va + offset;
    int ret;

    (void)type;
    heap = devmm_svm_get_heap(svm_proc, va);
    if (heap == NULL) {
#ifndef EMU_ST
        devmm_drv_err("Get heap failed. (va=0x%llx)\n", va);
        return -EINVAL;
#endif
    }

    pa_seg = devmm_kvzalloc_ex(sizeof(*pa_seg) * seg_num, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (pa_seg == NULL) {
#ifndef EMU_ST
        devmm_drv_err("Alloc pa seg failed. (seg_num=%u)\n", seg_num);
        return -ENOMEM;
#endif
    }

    svm_vmm_pa_owner_desc_pack(svm_proc->process_id.hostpid, vmma->info.devid, vmma->info.phy_addr_blk_id,
                               &pa_owner_desc);
    ret = svm_vmm_access_query_raw_pa_remote(svm_proc, &pa_owner_desc, pa_offset, pa_seg, &seg_num);
    if (ret != 0) {
        goto free_pa_seg;
    }

    ret = svm_pa_seg_d2h(vmma->info.devid, pa_seg, seg_num);
    if (ret != 0) {
        goto free_pa_seg;
    }

    ret = svm_vmm_access_va_map_local(svm_proc, heap, va, size, pa_seg, seg_num);

free_pa_seg:
    devmm_kvfree_ex(pa_seg);
    return ret;
}

static int svm_vmm_set_d2ph_access(struct devmm_svm_process *svm_proc, struct devmm_vmma_struct *vmma,
                                   u32 access_udevid, drv_mem_access_type type, u64 offset, u64 size)
{
    struct svm_pa_seg *pa_seg = NULL;
    u32 seg_num = (u32)(size / vmma->info.pa_pg_size);
    u64 pa_offset = vmma->info.offset_pg_num * vmma->info.pa_pg_size + offset;
    u64 va = vmma->info.va + offset;
    int ret;

    pa_seg = devmm_kvzalloc_ex(sizeof(*pa_seg) * seg_num, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (pa_seg == NULL) {
        devmm_drv_err("Alloc pa seg failed. (seg_num=%u)\n", seg_num);
        return -ENOMEM;
    }

    ret = svm_vmm_access_pa_query_local(svm_proc, vmma->info.phy_addr_blk_id, pa_offset, pa_seg, &seg_num);
    if (ret != 0) {
        goto free_pa_seg;
    }

    ret = svm_vmm_access_va_map_remote(svm_proc, access_udevid, type, va, size, pa_seg, seg_num);
free_pa_seg:
    devmm_kvfree_ex(pa_seg);
    return ret;
}
#ifndef EMU_ST
static int svm_vmm_set_d2vh_access(struct devmm_svm_process *svm_proc, struct devmm_vmma_struct *vmma,
                                   u32 access_udevid, drv_mem_access_type type, u64 offset, u64 size)
{
    struct svm_pa_seg *vm_pa_seg = NULL;
    struct svm_pa_seg *pm_pa_seg = NULL;
    u32 vm_seg_num = (u32)(size / vmma->info.pa_pg_size);
    u32 pm_seg_num = (u32)(size / KA_MM_PAGE_SIZE);
    u32 mapped_seg_num = 0;
    u64 pa_offset = vmma->info.offset_pg_num * vmma->info.pa_pg_size + offset;
    u64 va = vmma->info.va + offset;
    u64 mapped_size = 0;
    int ret;

    vm_pa_seg = devmm_kvzalloc_ex(sizeof(*vm_pa_seg) * vm_seg_num, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (vm_pa_seg == NULL) {
        devmm_drv_err("Alloc vm pa segment failed. (seg_num=%u)\n", vm_seg_num);
        return -ENOMEM;
    }

    pm_pa_seg = devmm_kvzalloc_ex(sizeof(*pm_pa_seg) * pm_seg_num, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (pm_pa_seg == NULL) {
        devmm_drv_err("Alloc pm pa segment failed. (seg_num=%u)\n", pm_seg_num);
        ret = -ENOMEM;
        goto free_vm_pa_seg;
    }

    ret = svm_vmm_access_pa_query_local(svm_proc, vmma->info.phy_addr_blk_id, pa_offset, vm_pa_seg, &vm_seg_num);
    if (ret != 0) {
        goto free_pm_pa_seg;
    }

    ret = svm_pa_seg_v2p(access_udevid, vm_pa_seg, vm_seg_num, pm_pa_seg, &pm_seg_num);
    if (ret != 0) {
        goto free_pm_pa_seg;
    }
    if ((u64)pm_seg_num * KA_MM_PAGE_SIZE != size) {
        devmm_drv_err("Invalid pm pa segment size. (seg_num=%u; expected_size=%llu)\n", pm_seg_num, size);
        ret = -EINVAL;
        goto free_pm_pa_seg;
    }

    while (mapped_seg_num < pm_seg_num) {
        u32 single_seg_num = ka_base_min_t(u32, pm_seg_num - mapped_seg_num, SVM_VMM_D2H_ACCESS_MAX_SEG_NUM);
        u64 single_size = (u64)single_seg_num * KA_MM_PAGE_SIZE;

        ret = svm_vmm_access_va_map_remote(svm_proc, access_udevid, type, va + mapped_size, single_size,
                                           &pm_pa_seg[mapped_seg_num], single_seg_num);
        if (ret != 0) {
            if (mapped_size != 0) {
                (void)svm_vmm_unset_access_remote(svm_proc, access_udevid, va, mapped_size, KA_MM_PAGE_SIZE);
            }
            goto free_pm_pa_seg;
        }
        mapped_seg_num += single_seg_num;
        mapped_size += single_size;
    }

free_pm_pa_seg:
    devmm_kvfree_ex(pm_pa_seg);
free_vm_pa_seg:
    devmm_kvfree_ex(vm_pa_seg);
    return ret;
}
#endif
static int svm_vmm_set_d2h_access(struct devmm_svm_process *svm_proc, struct devmm_vmma_struct *vmma, u32 access_udevid,
                                  drv_mem_access_type type, u64 offset, u64 size)
{
    bool is_in_hccs_vm = false;
    int ret;

    ret = devmm_get_if_run_in_hccs_vm(access_udevid, &is_in_hccs_vm);
    if (ret != 0) {
        return ret;
    }

    if (is_in_hccs_vm) {
#ifndef EMU_ST
        return svm_vmm_set_d2vh_access(svm_proc, vmma, access_udevid, type, offset, size);
#endif
    } else {
        return svm_vmm_set_d2ph_access(svm_proc, vmma, access_udevid, type, offset, size);
    }
}

static int svm_vmm_set_d2d_access(struct devmm_svm_process *svm_proc, struct devmm_vmma_struct *vmma, u32 access_udevid,
                                  drv_mem_access_type type, u64 offset, u64 size)
{
    struct svm_vmm_pa_owner_desc pa_owner_desc;
    u64 pa_offset = vmma->info.offset_pg_num * vmma->info.pa_pg_size + offset;
    u64 va = vmma->info.va + offset;
    u64 pa_pg_size = vmma->info.pa_pg_size;

    svm_vmm_pa_owner_desc_pack(svm_proc->process_id.hostpid, vmma->info.devid, vmma->info.phy_addr_blk_id,
                               &pa_owner_desc);

    return svm_vmm_set_d2d_access_remote(svm_proc, access_udevid, type, va, size, &pa_owner_desc, pa_pg_size,
                                         pa_offset);
}

static int svm_vmm_unset_h2d_access(struct devmm_svm_process *svm_proc, u64 va, u64 size)
{
    return svm_vmm_access_va_unmap_local(svm_proc, va, size);
}

static int svm_vmm_unset_d2h_access(struct devmm_svm_process *svm_proc, u32 access_udevid, u64 va, u64 size,
                                    u64 page_size)
{
    return svm_vmm_unset_access_remote(svm_proc, access_udevid, va, size, page_size);
}

static int svm_vmm_unset_d2d_access(struct devmm_svm_process *svm_proc, u32 access_udevid, u64 va, u64 size,
                                    u64 page_size)
{
    return svm_vmm_unset_access_remote(svm_proc, access_udevid, va, size, page_size);
}

static int svm_vmm_single_set_access(struct devmm_svm_process *svm_proc, struct devmm_vmma_struct *vmma,
                                     u32 access_udevid, drv_mem_access_type type, u64 offset, u64 size)
{
    enum svm_vmm_access_dir dir = svm_vmm_get_access_dir(vmma, access_udevid);

    switch (dir) {
        case SVM_VMM_ACCESS_DIR_H2D:
            return svm_vmm_set_h2d_access(svm_proc, vmma, type, offset, size);
        case SVM_VMM_ACCESS_DIR_D2H:
            return svm_vmm_set_d2h_access(svm_proc, vmma, access_udevid, type, offset, size);
        case SVM_VMM_ACCESS_DIR_D2D:
            return svm_vmm_set_d2d_access(svm_proc, vmma, access_udevid, type, offset, size);
        default:
            return -EINVAL;
    }
}

static int svm_vmm_unset_owner_access(struct devmm_svm_process *svm_proc, struct devmm_vmma_struct *vmma)
{
    if (vmma->info.side == MEM_HOST_SIDE) {
        svm_set_da_status(svm_proc, vmma->info.va, vmma->info.size, DEVMM_DA_STATUS_UNMAPING);
        devmm_mem_unmap(svm_proc, &vmma->info);
        svm_set_da_status(svm_proc, vmma->info.va, vmma->info.size, DEVMM_DA_STATUS_NONE);
        return 0;
    }
    return devmm_msg_to_agent_mem_unmap(svm_proc, &vmma->info);
}

static int _svm_vmm_unset_access(struct devmm_svm_process *svm_proc, struct devmm_vmma_struct *vmma, u32 access_udevid,
                                 u64 size)
{
    enum svm_vmm_access_dir dir = svm_vmm_get_access_dir(vmma, access_udevid);
    u64 va = vmma->info.va;

    if (size == 0) {
        return 0;
    }

    switch (dir) {
        case SVM_VMM_ACCESS_DIR_H2D:
            return svm_vmm_unset_h2d_access(svm_proc, va, size);
        case SVM_VMM_ACCESS_DIR_D2H:
            return svm_vmm_unset_d2h_access(svm_proc, access_udevid, va, size, vmma->info.pg_size);
        case SVM_VMM_ACCESS_DIR_D2D:
            return svm_vmm_unset_d2d_access(svm_proc, access_udevid, va, size, vmma->info.pg_size);
        default:
            return -EINVAL;
    }
}

static int svm_vmm_split_set_access(struct devmm_svm_process *svm_proc, struct devmm_vmma_struct *vmma,
                                    u32 access_udevid, drv_mem_access_type type)
{
    u64 max_single_size = svm_vmm_get_set_access_max_single_size(vmma, access_udevid);
    u64 total_size = vmma->info.size;
    u64 finished_size = 0;

    if (max_single_size == 0) {
#ifndef EMU_ST
        devmm_drv_err("Invalid max single size. (access_udevid=%u; owner_udevid=%u; size=%llu; max_single_size=%llu)\n",
                      access_udevid, vmma->info.devid, (u64)vmma->info.size, (u64)max_single_size);
        return -EINVAL;
#endif
    }

    while (finished_size < total_size) {
        u64 single_size = ka_base_min(total_size - finished_size, max_single_size);
        int ret;

        ret = svm_vmm_single_set_access(svm_proc, vmma, access_udevid, type, finished_size, single_size);
        if (ret != 0) {
            (void)_svm_vmm_unset_access(svm_proc, vmma, access_udevid, finished_size);
            return ret;
        }
        finished_size += single_size;
    }

    return 0;
}

static int svm_vmm_set_access_capability_check(const struct devmm_vmma_struct *vmma, u32 access_udevid)
{
#ifndef EMU_ST
    enum svm_vmm_access_dir dir = svm_vmm_get_access_dir(vmma, access_udevid);
    if ((dir == SVM_VMM_ACCESS_DIR_D2H) && !devmm_is_hccs_connect(access_udevid)) {
        return -EOPNOTSUPP;
    }
#endif
    return 0;
}

int svm_vmm_set_access(struct devmm_svm_process *svm_proc, struct devmm_vmma_struct *vmma, u32 access_udevid,
                       drv_mem_access_type type)
{
    int ret;

    ret = svm_vmm_set_access_capability_check(vmma, access_udevid);
    if (ret != 0) {
        return ret;
    }
    return svm_vmm_split_set_access(svm_proc, vmma, access_udevid, type);
}

int svm_vmm_unset_access(struct devmm_svm_process *svm_proc, struct devmm_vmma_struct *vmma, u32 access_udevid)
{
    if (access_udevid == vmma->info.devid) {
        return svm_vmm_unset_owner_access(svm_proc, vmma);
    }
    return _svm_vmm_unset_access(svm_proc, vmma, access_udevid, vmma->info.size);
}
