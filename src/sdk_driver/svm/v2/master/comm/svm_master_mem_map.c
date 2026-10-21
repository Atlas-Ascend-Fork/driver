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

#include "ka_type.h"
#include "pbl_feature_loader.h"
#include "svm_kernel_msg.h"
#include "devmm_common.h"
#include "devmm_chan_handlers.h"
#include "svm_heap_mng.h"
#include "svm_mem_map.h"
#include "svm_rbtree.h"
#include "devmm_page_cache.h"
#include "svm_msg_client.h"
#include "svm_phy_addr_blk_mng.h"
#include "svm_vmma_mng.h"
#include "svm_master_query.h"
#include "svm_master_mem_map.h"
#include "svm_vmm_access_master.h"
#include "svm_vmm_access_msg.h"
#include "svm_master_mem_create.h"
#include "svm_master_mem_share.h"
#include "ka_compiler_pub.h"
#include "ka_base_pub.h"

static int svm_vmm_vmma_ref_op_msg_check(struct devmm_svm_heap *heap, struct svm_vmm_vmma_ref_op_msg *msg)
{
    if ((heap == NULL) || (heap->heap_sub_type != SUB_RESERVE_TYPE) || (msg->size == 0)) {
        return -EINVAL;
    }

    if ((msg->op != SVM_VMM_VMMA_REF_OP_GET) && (msg->op != SVM_VMM_VMMA_REF_OP_PUT)) {
        return -EINVAL;
    }

    return 0;
}

static int svm_vmm_vmma_ref_op_msg_handle(struct devmm_svm_process *svm_proc, struct devmm_svm_heap *heap, void *data,
                                          u32 *ack_len)
{
    struct svm_vmm_vmma_ref_op_msg *msg = (struct svm_vmm_vmma_ref_op_msg *)data;
    struct devmm_vmma_struct *vmma = NULL;
    u64 va = msg->va;
    u64 size = msg->size;
    u32 op = msg->op;
    int ret = 0;

    *ack_len = sizeof(msg->head);

    (void)svm_proc;
    ret = svm_vmm_vmma_ref_op_msg_check(heap, msg);
    if (ret != 0) {
        return ret;
    }

    vmma = devmm_vmma_get(&heap->vmma_mng, va);
    if (vmma == NULL) {
        return -EINVAL;
    }
    if (size > vmma->info.size - (va - vmma->info.va)) {
        devmm_vmma_put(vmma);
        return -EINVAL;
    }

    if (op == SVM_VMM_VMMA_REF_OP_GET) {
        ret = devmm_vmma_shared_set(&heap->vmma_mng, vmma);
    } else {
        devmm_vmma_shared_clear(vmma);
    }

    devmm_vmma_put(vmma);
    return ret;
}

static const struct devmm_chan_handlers_st g_svm_vmm_vmma_ref_op_handle = {
    svm_vmm_vmma_ref_op_msg_handle, sizeof(struct svm_vmm_vmma_ref_op_msg), 0, DEVMM_MSG_GET_HEAP_MASK};

int svm_vmm_vmma_ref_op_msg_register(void)
{
    svm_register_d2h_msg_handle(DEVMM_VMMA_REF_OP_ID, &g_svm_vmm_vmma_ref_op_handle);
    return 0;
}
DECLAER_FEATURE_AUTO_INIT(svm_vmm_vmma_ref_op_msg_register, FEATURE_LOADER_STAGE_3);

int devmm_access_munmap_all(struct devmm_svm_process *svm_proc, struct devmm_vmma_struct *vmma)
{
    u32 udevid;
    int result = 0;

    for (udevid = 0; udevid < DEVMM_MAX_ACCESS_DEVICE_NUM; udevid++) {
        int ret;

        if (vmma->device_access_type[udevid] == MEM_ACCESS_TYPE_NONE) {
            continue;
        }

        ret = svm_vmm_unset_access(svm_proc, vmma, udevid);
        if (ret != 0) {
            result = ret;
            continue;
        }
        vmma->device_access_type[udevid] = MEM_ACCESS_TYPE_NONE;
    }

    return result;
}

static int devmm_mem_map_pg_bitmap_state_set(struct devmm_svm_heap *heap, struct devmm_mem_map_para *para,
                                             u32 logic_devid, bool is_da_addr)
{
    u32 *page_bitmap = NULL;
    u64 pg_cnt = para->size / heap->chunk_page_size;
    u64 i, j;
    u32 flag, tmp_devid;
    int ret;

    flag = (para->side == MEM_HOST_SIDE) ? (DEVMM_PAGE_HOST_MAPPED_MASK | DEVMM_PAGE_LOCKED_HOST_MASK) :
                                           (DEVMM_PAGE_DEV_MAPPED_MASK | DEVMM_PAGE_LOCKED_DEVICE_MASK);
    tmp_devid = (para->side == MEM_HOST_SIDE) ? uda_get_host_id() : logic_devid;

    page_bitmap = devmm_get_page_bitmap_with_heap(heap, para->va);
    if (ka_unlikely(page_bitmap == NULL)) {
        devmm_drv_err("Unexpected, get pg_bitmap failed. (va=0x%llx)\n", para->va);
        return -EINVAL;
    }

    for (i = 0; i < pg_cnt; i++) {
        ret = devmm_page_bitmap_check_and_set_flag(page_bitmap + i, flag | DEVMM_PAGE_ADVISE_POPULATE_MASK);
        if (ret != 0) {
            devmm_drv_err("Already mapped. (already_mapped=%llu; va=0x%llx; page_cnt=%llu)\n", i, para->va, pg_cnt);
            ret = -EADDRINUSE;
            goto clear_bitmap;
        }
        devmm_page_bitmap_set_devid(page_bitmap + i, tmp_devid);
    }

    return 0;
clear_bitmap:
    for (j = 0; j < i; j++) {
        devmm_page_bitmap_clear_flag(page_bitmap + j, flag);
    }
    return ret;
}

static int devmm_mem_map_pg_bitmap_state_clear(struct devmm_svm_heap *heap, u64 va, u64 size, u32 side, bool is_da_addr)
{
    u32 *page_bitmap = NULL;
    u64 pg_cnt, i;
    u32 flag;

    pg_cnt = size / heap->chunk_page_size;
    flag = (side == MEM_HOST_SIDE) ? (DEVMM_PAGE_HOST_MAPPED_MASK | DEVMM_PAGE_LOCKED_HOST_MASK) :
                                     (DEVMM_PAGE_DEV_MAPPED_MASK | DEVMM_PAGE_LOCKED_DEVICE_MASK);

    page_bitmap = devmm_get_page_bitmap_with_heap(heap, va);
    if (ka_unlikely(page_bitmap == NULL)) {
        devmm_drv_err("Unexpected, get pg_bitmap failed. (va=0x%llx)\n", va);
        return -EINVAL;
    }

    for (i = 0; i < pg_cnt; i++) {
        devmm_page_bitmap_clear_flag(page_bitmap + i, flag | DEVMM_PAGE_ADVISE_POPULATE_MASK);
    }
    return 0;
}

int devmm_msg_to_agent_mem_unmap(struct devmm_svm_process *svm_proc, struct devmm_vmma_info *info)
{
    struct devmm_chan_mem_unmap msg = {{{0}}};
    u64 freed_num, tmp_num;
    int ret;

    msg.head.msg_id = DEVMM_CHAN_MEM_UNMAP_H2D_ID;
    msg.head.process_id.hostpid = svm_proc->process_id.hostpid;
    msg.head.process_id.vfid = (u16)info->vfid;
    msg.head.dev_id = (u16)info->devid;

    devmm_mem_free_preprocess_by_dev_and_va(svm_proc, info->devid, info->va, info->pg_num * info->pg_size);
    for (freed_num = 0; freed_num < info->pg_num; freed_num += tmp_num) {
        msg.va = info->va + freed_num * info->pg_size;
        ret = devmm_chan_msg_send(&msg, sizeof(struct devmm_chan_mem_unmap), sizeof(struct devmm_chan_mem_unmap));
        if (ret != 0) {
            devmm_drv_err("Send mem unmap msg failed. (ret=%d; va=0x%llx; freed_num=%llu)\n", ret, msg.va, freed_num);
            return ret;
        }
        /* Every agent map msg will create vmma, so map per page num should equal with unmap */
        tmp_num = ka_base_min((info->pg_num - freed_num), DEVMM_MEM_MAP_MAX_PAGE_NUM_PER_MSG);
        devmm_free_pages_cache(svm_proc, info->logic_devid, (u32)tmp_num, info->pg_size, msg.va, true);
    }

    return 0;
}

static int _devmm_ioctl_mem_unmap(struct devmm_svm_process *svm_proc, struct devmm_vmma_info *info)
{
    if (info->side == MEM_HOST_SIDE) {
        svm_set_da_status(svm_proc, info->va, info->size, DEVMM_DA_STATUS_UNMAPING);
        devmm_mem_unmap(svm_proc, info);
        svm_set_da_status(svm_proc, info->va, info->size, DEVMM_DA_STATUS_NONE);
        return 0;
    } else {
        return devmm_msg_to_agent_mem_unmap(svm_proc, info);
    }
}

int devmm_owner_unset_access(struct devmm_svm_process *svm_proc, struct devmm_vmma_struct *vmma)
{
    struct devmm_svm_heap *heap = NULL;
    u32 *page_bitmap = NULL;
    bool is_da_addr;
    int ret;

    if (!vmma->info.owner_mapped) {
        return 0;
    }

    heap = devmm_svm_get_heap(svm_proc, vmma->info.va);
    if (heap == NULL) {
        devmm_drv_err("Get heap failed. (va=0x%llx)\n", vmma->info.va);
        return -EINVAL;
    }

    page_bitmap = devmm_get_page_bitmap_with_heap(heap, vmma->info.va);
    if (page_bitmap == NULL) {
        devmm_drv_err("Get page bitmap failed. (va=0x%llx)\n", vmma->info.va);
        return -EINVAL;
    }

    devmm_svm_free_share_page_msg(svm_proc, heap, vmma->info.va, vmma->info.size, page_bitmap);
    ret = _devmm_ioctl_mem_unmap(svm_proc, &vmma->info);
    if (ret != 0) {
        return ret;
    }

    is_da_addr = svm_is_da_addr(svm_proc, vmma->info.va, vmma->info.size);
    (void)devmm_mem_map_pg_bitmap_state_clear(heap, vmma->info.va, vmma->info.size, vmma->info.side, is_da_addr);
    vmma->info.owner_mapped = false;
    vmma->info.type = MEM_ACCESS_TYPE_NONE;
    return 0;
}

static int devmm_mem_unmap_check_bitmap_state(struct devmm_svm_process *svm_proc, struct devmm_svm_heap *heap, u64 va,
                                              u64 size)
{
    u64 pg_cnt = size / heap->chunk_page_size;
    u32 *page_bitmap = NULL;
    bool is_translating;

    page_bitmap = devmm_get_page_bitmap_with_heap(heap, va);
    if (ka_unlikely(page_bitmap == NULL)) {
        devmm_drv_err("Unexpected, get pg_bitmap failed. (va=0x%llx)\n", va);
        return -EINVAL;
    }

    is_translating = devmm_check_is_translate(svm_proc, va, page_bitmap, heap->chunk_page_size, pg_cnt);
    return (is_translating) ? -EBUSY : 0;
}

int devmm_ioctl_mem_unmap(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_unmap_para *para = &arg->data.mem_unmap_para;
    struct devmm_vmma_struct *vmma = NULL;
    struct devmm_svm_heap *heap = NULL;
    int ret;

    heap = devmm_svm_get_heap(svm_proc, para->va);
    if (heap == NULL) {
        devmm_drv_err("Is idle addr. (va=0x%llx)\n", para->va);
        return -EINVAL;
    }

    vmma = devmm_vmma_get(&heap->vmma_mng, para->va);
    if (vmma == NULL) {
        /* The log cannot be modified, because in the failure mode library. */
        devmm_drv_err("Get vmma failed. (va=0x%llx)\n", para->va);
        return -EINVAL;
    }

    if (para->va != vmma->info.va) {
        devmm_drv_err("Va isn't head addr. (va=0x%llx; vmma->va=0x%llx)\n", para->va, vmma->info.va);
        devmm_vmma_put(vmma);
        return -EINVAL;
    }

    ret = devmm_vmma_exclusive_set(vmma);
    if (ret != 0) {
        devmm_vmma_put(vmma);
        return ret;
    }

    ret = devmm_mem_unmap_check_bitmap_state(svm_proc, heap, vmma->info.va, vmma->info.size);
    if (ret != 0) {
        goto exclusive_clear;
    }

    ret = devmm_access_munmap_all(svm_proc, vmma);
    if (ret != 0) {
        goto exclusive_clear;
    }

    ret = devmm_owner_unset_access(svm_proc, vmma);
    if (ret != 0) {
        goto exclusive_clear;
    }

    if (devmm_vmma_is_owner_no_access(&vmma->info)) {
        devmm_mem_map_occupy_dec(svm_proc, &vmma->info);
    }

    para->logic_devid = vmma->info.logic_devid;
    para->side = vmma->info.side;
    para->unmap_size = vmma->info.size;

    devmm_vmma_destroy(&heap->vmma_mng, vmma);

exclusive_clear:
    devmm_vmma_exclusive_clear(vmma);
    devmm_vmma_put(vmma);
    return ret;
}

static bool devmm_vmm_map_flag_is_owner_no_access(u64 vmm_map_flag)
{
    return (vmm_map_flag & DEVMM_VMM_MAP_FLAG_OWNER_NO_ACCESS) != 0;
}

static int devmm_mem_map_pa_pg_size_check(struct devmm_svm_process *svm_proc, const struct devmm_mem_map_para *para,
                                          u32 devid)
{
    struct devmm_share_id_map_node *map_node = NULL;
    struct devmm_phy_addr_blk *blk = NULL;
    u64 import_pa_pg_size = 0;
    bool is_import = false;
    int ret = 0;

    map_node = devmm_share_id_map_node_get(svm_proc, devid, para->id);
    if (map_node != NULL) {
        if (map_node->blk_type == SVM_PYH_ADDR_BLK_IMPORT_TYPE) {
            import_pa_pg_size = map_node->shid_map_node_info.pa_pg_size;
            is_import = true;
        }
        devmm_share_id_map_node_put(map_node);
    }

    if ((para->pa_pg_size == 0) || (ka_mm_is_power_of_2(para->pa_pg_size) == false) ||
        !KA_DRIVER_IS_ALIGNED(para->size, para->pa_pg_size) || ((para->size / para->pa_pg_size) > para->pg_num)) {
#ifndef EMU_ST
        devmm_drv_err("Invalid pa page size. (size=%llu; pa_pg_size=%llu; pg_num=%llu)\n", para->size, para->pa_pg_size,
                      para->pg_num);
        return -EINVAL;
#endif
    }

    if (is_import) {
        if (para->pa_pg_size != import_pa_pg_size) {
#ifndef EMU_ST
            devmm_drv_err("Import pa page size mismatch. (id=%d; input=%llu; actual=%llu)\n", para->id,
                          para->pa_pg_size, import_pa_pg_size);
            return -EINVAL;
#endif
        }
        return 0;
    }

    if (para->side != MEM_HOST_SIDE) {
        return 0;
    }

    blk = devmm_phy_addr_blk_get(&svm_proc->phy_addr_blk_mng, para->id);
    if (blk == NULL) {
#ifndef EMU_ST
        devmm_drv_err("Get phy addr blk failed. (id=%d)\n", para->id);
        return -EINVAL;
#endif
    }

    if (para->pa_pg_size != blk->pa_pg_size) {
#ifndef EMU_ST
        devmm_drv_err("Pa page size mismatch. (id=%d; input=%llu; actual=%llu)\n", para->id, para->pa_pg_size,
                      blk->pa_pg_size);
        ret = -EINVAL;
#endif
    }
    devmm_phy_addr_blk_put(blk);
    return ret;
}

static int devmm_mem_map_range_check(struct devmm_svm_heap *heap, const struct devmm_mem_map_para *para)
{
    int ret;

    if (heap->heap_sub_type != SUB_RESERVE_TYPE) {
        devmm_drv_err("Heap sub type could only be reserve type. (va=0x%llx; heap_sub_type=%u)\n", para->va,
                      heap->heap_sub_type);
        return -EINVAL;
    }

    if (para->size == 0) {
        devmm_drv_err("Size is zero.\n");
        return -EINVAL;
    }

    if (KA_DRIVER_IS_ALIGNED(para->va, heap->chunk_page_size) == false) {
        devmm_drv_err("Va should be aligned by pg_size. (pg_size=%u)\n", heap->chunk_page_size);
        return -EINVAL;
    }

    if (KA_DRIVER_IS_ALIGNED(para->size, heap->chunk_page_size) == false) {
        devmm_drv_err("Size should be aligned by pg_size. (size=%llu; pg_size=%u)\n", para->size,
                      heap->chunk_page_size);
        return -EINVAL;
    }

    ret = devmm_check_va_add_size_by_heap(heap, para->va, para->size);
    if (ret != 0) {
        devmm_drv_err("Addr out of heap range. (va=0x%llx; size=%llu)\n", para->va, para->size);
        return ret;
    }

    return 0;
}

static int devmm_mem_map_attr_check(struct devmm_svm_process *svm_proc, const struct devmm_mem_map_para *para,
                                    u32 devid)
{
    int ret;

    if ((para->side != MEM_HOST_SIDE) && (para->side != MEM_DEV_SIDE)) {
        devmm_drv_err("Invalid side. (side=%u)\n", para->side);
        return -EINVAL;
    }

    if (para->pg_type >= MEM_MAX_PAGE_TYPE) {
        devmm_drv_err("Invalid pg_type. (pg_type=%u)\n", para->pg_type);
        return -EINVAL;
    }

    ret = devmm_mem_map_pa_pg_size_check(svm_proc, para, devid);
    if (ret != 0) {
        return ret;
    }

    if ((para->side == MEM_HOST_SIDE) && (devid != uda_get_host_id())) {
        devmm_drv_err("Invalid side devid. (side=%u; devid=%u)\n", para->side, devid);
        return -EINVAL;
    }

    return 0;
}

static int devmm_mem_map_alloced_check(struct devmm_svm_heap *heap, const struct devmm_mem_map_para *para)
{
    u32 *page_bitmap = NULL;
    u64 alloced_size, pg_cnt, i;

    alloced_size = devmm_get_alloced_size_from_va(heap, para->va);
    if (para->size > alloced_size) {
        devmm_drv_err("Size out of range. (size=%llu; alloced_size=%llu)\n", para->size, alloced_size);
        return -EINVAL;
    }

    page_bitmap = devmm_get_page_bitmap_with_heap(heap, para->va);
    if (page_bitmap == NULL) {
        devmm_drv_err("Unexpected, get pg_bitmap failed. (va=0x%llx)\n", para->va);
        return -EINVAL;
    }

    pg_cnt = para->size / heap->chunk_page_size;
    for (i = 0; i < pg_cnt; i++) {
        if (devmm_page_bitmap_is_page_available(page_bitmap + i) == false) {
            devmm_drv_err("Addr hasn't been alloced. (va=0x%llx; size=%llu)\n", para->va, para->size);
            return -EINVAL;
        }
    }

    return 0;
}

static int devmm_ioctl_mem_map_para_check(struct devmm_svm_process *svm_proc, struct devmm_svm_heap *heap,
                                          const struct devmm_mem_map_para *para, u32 devid)
{
    int ret;

    ret = devmm_mem_map_range_check(heap, para);
    if (ret != 0) {
        return ret;
    }

    ret = devmm_mem_map_attr_check(svm_proc, para, devid);
    if (ret != 0) {
        return ret;
    }

    return devmm_mem_map_alloced_check(heap, para);
}

static int devmm_master_mem_map(struct devmm_svm_process *svm_proc, struct devmm_vmma_info *info)
{
    return devmm_mem_map(svm_proc, info, true);
}

static int _devmm_ioctl_mem_map(struct devmm_svm_process *svm_proc, struct devmm_vmma_info *info)
{
    if (info->side == MEM_HOST_SIDE) {
        return devmm_master_mem_map(svm_proc, info);
    } else {
        return devmm_msg_to_agent_mem_map(svm_proc, info);
    }
}

static void devmm_ioctl_vmma_info_pack(struct devmm_svm_process *svm_proc, struct devmm_svm_heap *heap,
                                       struct devmm_devid *devids, struct devmm_mem_map_para *para,
                                       struct devmm_vmma_info *info)
{
    u32 type = SVM_PYH_ADDR_BLK_NORMAL_TYPE;
    struct devmm_phy_addr_blk *blk = NULL;
    bool is_same_sys_share = true;

    if (para->side == MEM_HOST_SIDE) {
        blk = devmm_phy_addr_blk_get(&svm_proc->phy_addr_blk_mng, para->id);
        if (blk != NULL) {
            is_same_sys_share = blk->is_same_sys_share;
            type = blk->type;
            devmm_drv_debug("blk info. (is_same_sys_share=%u; type=%u)\n", is_same_sys_share, blk->type);
            devmm_phy_addr_blk_put(blk);
        }
    }

    info->pg_type = para->pg_type;
    if (para->pg_type == MEM_GIANT_PAGE_TYPE) {
        info->pg_type = MEM_GIANT_PAGE_TYPE;
        info->pg_size = SVM_MASTER_GIANT_PAGE_SIZE;
    } else if ((para->side == MEM_HOST_SIDE) && !is_same_sys_share && (type == SVM_PYH_ADDR_BLK_IMPORT_TYPE)) {
        info->pg_type = MEM_NORMAL_PAGE_TYPE;
        info->pg_size = KA_MM_PAGE_SIZE;
    } else {
        info->pg_type = (heap->heap_type == DEVMM_HEAP_HUGE_PAGE) ? MEM_HUGE_PAGE_TYPE : MEM_NORMAL_PAGE_TYPE;
        if (info->pg_type == MEM_HUGE_PAGE_TYPE) {
            info->pg_size = (para->side == MEM_HOST_SIDE) ? SVM_MASTER_HUGE_PAGE_SIZE : devmm_svm->device_hpage_size;
        } else {
            info->pg_size = (para->side == MEM_HOST_SIDE) ? KA_MM_PAGE_SIZE : devmm_svm->device_page_size;
        }
    }

    info->va = para->va;
    info->size = para->size;
    info->pg_num = info->size / info->pg_size;
    info->pa_pg_size = para->pa_pg_size;

    info->side = para->side;
    info->logic_devid = devids->logical_devid;
    info->devid = devids->devid;
    info->vfid = devids->vfid;
    info->page_insert_dev_id = devids->logical_devid;
    info->local_handle_flag = 1;
    info->vmm_map_flag = para->vmm_map_flag;
    info->owner_mapped = true;
    info->type = MEM_ACCESS_TYPE_READWRITE;

    info->module_id = para->module_id;
    info->phy_addr_blk_id = para->id;
    info->offset_pg_num = 0;
    info->phy_addr_blk_pg_num = para->pg_num;
}

static void devmm_mem_map_local_handle_flag_set(struct devmm_svm_process *svm_proc, u32 devid, int id,
                                                struct devmm_vmma_info *info)
{
    struct devmm_share_id_map_node *map_node = devmm_share_id_map_node_get(svm_proc, devid, id);

    if (map_node == NULL) {
        return;
    }

    if (map_node->blk_type == SVM_PYH_ADDR_BLK_IMPORT_TYPE) {
        info->local_handle_flag = 0;
    }
    devmm_share_id_map_node_put(map_node);
}

static int devmm_mem_map_info_prepare(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg,
                                      struct devmm_svm_heap **heap, struct devmm_vmma_info *info)
{
    struct devmm_mem_map_para *para = &arg->data.mem_map_para;
    int ret;

    *heap = devmm_svm_get_heap(svm_proc, para->va);
    if (*heap == NULL) {
        devmm_drv_err("Invalid addr, get heap failed. (va=0x%llx)\n", para->va);
        return -EINVAL;
    }

    ret = devmm_ioctl_mem_map_para_check(svm_proc, *heap, para, arg->head.devid);
    if (ret != 0) {
        devmm_drv_err("Check mem map para failed. (va=0x%llx; size=%llu; ret=%d)\n", para->va, para->size, ret);
        return ret;
    }

    devmm_ioctl_vmma_info_pack(svm_proc, *heap, &arg->head, para, info);
    devmm_mem_map_local_handle_flag_set(svm_proc, arg->head.devid, para->id, info);
    return 0;
}

static int devmm_mem_map_owner_access(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_map_para *para = &arg->data.mem_map_para;
    struct devmm_vmma_info info = {0};
    struct devmm_svm_heap *heap = NULL;
    bool is_da_addr = svm_is_da_addr(svm_proc, para->va, para->size);
    int ret;

    ret = devmm_mem_map_info_prepare(svm_proc, arg, &heap, &info);
    if (ret != 0) {
        devmm_drv_err("Prepare owner map info failed. (va=0x%llx; ret=%d)\n", para->va, ret);
        return ret;
    }

    ret = devmm_mem_map_pg_bitmap_state_set(heap, para, arg->head.logical_devid, is_da_addr);
    if (ret != 0) {
        devmm_drv_err("Set owner map bitmap failed. (va=0x%llx; ret=%d)\n", para->va, ret);
        return ret;
    }

    ret = _devmm_ioctl_mem_map(svm_proc, &info);
    if (ret != 0) {
        devmm_drv_err("Map owner va failed. (va=0x%llx; size=%llu; ret=%d)\n", para->va, para->size, ret);
        (void)devmm_mem_map_pg_bitmap_state_clear(heap, para->va, para->size, para->side, is_da_addr);
        return ret;
    }

    ret = devmm_vmma_create(&heap->vmma_mng, &info);
    if (ret != 0) {
        devmm_drv_err("Create owner vmma failed. (va=0x%llx; size=%llu; ret=%d)\n", para->va, para->size, ret);
        (void)_devmm_ioctl_mem_unmap(svm_proc, &info);
        (void)devmm_mem_map_pg_bitmap_state_clear(heap, para->va, para->size, para->side, is_da_addr);
        return ret;
    }

    return 0;
}

static int devmm_mem_map_owner_no_access(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_map_para *para = &arg->data.mem_map_para;
    struct devmm_vmma_info info = {0};
    struct devmm_svm_heap *heap = NULL;
    int ret;

    ret = devmm_mem_map_info_prepare(svm_proc, arg, &heap, &info);
    if (ret != 0) {
#ifndef EMU_ST
        devmm_drv_err("Prepare no-access map info failed. (va=0x%llx; ret=%d)\n", para->va, ret);
        return ret;
#endif
    }

    info.type = MEM_ACCESS_TYPE_NONE;
    info.owner_mapped = false;
    ret = devmm_mem_map_occupy_inc(svm_proc, &info);
    if (ret != 0) {
#ifndef EMU_ST
        devmm_drv_err("Occupy no-access phy addr blk failed. (id=%d; ret=%d)\n", info.phy_addr_blk_id, ret);
        return ret;
#endif
    }

    ret = devmm_vmma_create(&heap->vmma_mng, &info);
    if (ret != 0) {
#ifndef EMU_ST
        devmm_drv_err("Create no-access vmma failed. (va=0x%llx; size=%llu; ret=%d)\n", para->va, para->size, ret);
        devmm_mem_map_occupy_dec(svm_proc, &info);
        return ret;
#endif
    }

    return 0;
}

int devmm_ioctl_mem_map(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    if (devmm_vmm_map_flag_is_owner_no_access(arg->data.mem_map_para.vmm_map_flag)) {
        return devmm_mem_map_owner_no_access(svm_proc, arg);
    }

    return devmm_mem_map_owner_access(svm_proc, arg);
}

int devmm_ioctl_resv_addr_info_query(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_resv_addr_info_query_para *para = &arg->data.resv_addr_info_query_para;
    struct devmm_vmma_struct *vmma = NULL;
    struct devmm_svm_heap *heap = NULL;

    heap = devmm_svm_get_heap(svm_proc, para->va);
    if (heap == NULL) {
        devmm_drv_info("Is idle addr. (va=0x%llx)\n", para->va);
        return -EINVAL;
    }

    vmma = devmm_vmma_get(&heap->vmma_mng, para->va);
    if (vmma == NULL) {
        devmm_drv_info("Not in range. (va=0x%llx)\n", para->va);
        return -EINVAL;
    }
    para->start = vmma->info.va;
    para->end = vmma->info.va + vmma->info.size - 1;
    para->module_id = vmma->info.module_id;
    para->devid = vmma->info.logic_devid;
    devmm_vmma_put(vmma);

    return 0;
}

static struct devmm_vmma_struct *devmm_vmma_erase_one_by_devid(struct devmm_vmma_mng *mng, u32 devid)
{
    struct devmm_vmma_struct *vmma = NULL;
    ka_rb_node_t *rbnode = NULL;

    ka_task_down_write(&mng->rw_sem);
    rbnode = ka_base_rb_first(&mng->root);
    while (rbnode != NULL) {
        vmma = ka_base_rb_entry(rbnode, struct devmm_vmma_struct, rbnode);
        if ((vmma->info.side != DEVMM_SIDE_MASTER) && (vmma->info.devid == devid)) {
            (void)devmm_rb_erase(&mng->root, rbnode); /* Won't fail. */
            break;
        }
        rbnode = ka_base_rb_next(rbnode);
        vmma = NULL;
    }
    ka_task_up_write(&mng->rw_sem);

    return vmma;
}

void devmm_destroy_all_heap_vmmas_by_devid(struct devmm_svm_process *svm_proc, u32 devid)
{
    struct devmm_vmma_struct *vmma = NULL;
    struct devmm_svm_heap *heap = NULL;
    u32 stamp = (u32)ka_jiffies;
    u64 heap_idx;
    u32 max_num;
    u32 is_host_pin;

    for (is_host_pin = 0; is_host_pin <= 1; is_host_pin++) {
        max_num = (is_host_pin ? svm_proc->max_host_pin_heap_use : svm_proc->max_heap_use);
        for (heap_idx = 0; heap_idx < max_num; heap_idx++) {
            devmm_try_cond_resched(&stamp);
            heap = devmm_get_heap_by_idx(svm_proc, heap_idx, is_host_pin);
            if (devmm_check_heap_is_entity(heap) == false) {
                continue;
            }
            while ((vmma = devmm_vmma_erase_one_by_devid(&heap->vmma_mng, devid)) != NULL) {
                devmm_try_cond_resched(&stamp);
                (void)devmm_access_munmap_all(svm_proc, vmma);
                (void)devmm_owner_unset_access(svm_proc, vmma);
                if (devmm_vmma_is_owner_no_access(&vmma->info)) {
                    devmm_mem_map_occupy_dec(svm_proc, &vmma->info);
                }
                devmm_vmma_put(vmma);
            }

            heap_idx += (heap->heap_size >= DEVMM_HEAP_SIZE) ? (heap->heap_size / DEVMM_HEAP_SIZE - 1) : 0;
        }
    }
}

static struct devmm_vmma_struct *devmm_vmma_matched_get(struct devmm_svm_heap *heap, u64 va, u64 size)
{
    struct devmm_vmma_struct *vmma = NULL;

    vmma = devmm_vmma_get(&heap->vmma_mng, va);
    if (vmma == NULL) {
        devmm_drv_err("Get vmma failed. (va=0x%llx)\n", va);
        return NULL;
    }

    if ((va != vmma->info.va) || (size != vmma->info.size)) {
        devmm_drv_err("Not match para. (va=0x%llx; vmma->va=0x%llx; size=0x%llx; vmma->size=0x%llx)\n", va,
                      vmma->info.va, size, vmma->info.size);
        devmm_vmma_put(vmma);
        return NULL;
    }

    return vmma;
}

static struct devmm_vmma_struct *devmm_ioctl_vmma_matched_get(struct devmm_svm_process *svm_proc, u64 va, u64 size,
                                                              struct devmm_svm_heap **heap)
{
    *heap = devmm_svm_get_heap(svm_proc, va);
    if (*heap == NULL) {
        devmm_drv_err("Is idle addr. (va=0x%llx)\n", va);
        return NULL;
    }

    return devmm_vmma_matched_get(*heap, va, size);
}

static void devmm_vmma_owner_mmap_bitmap_clear(struct devmm_svm_process *svm_proc, struct devmm_vmma_struct *vmma,
                                               struct devmm_svm_heap *heap)
{
    bool is_da_addr = svm_is_da_addr(svm_proc, vmma->info.va, vmma->info.size);

    devmm_mem_map_pg_bitmap_state_clear(heap, vmma->info.va, vmma->info.size, vmma->info.side, is_da_addr);
}

static int devmm_vmma_owner_mmap(struct devmm_svm_process *svm_proc, struct devmm_vmma_struct *vmma)
{
    struct devmm_mem_map_para para = {0};
    struct devmm_svm_heap *heap = NULL;
    bool is_da_addr;
    int ret;

    heap = devmm_svm_get_heap(svm_proc, vmma->info.va);
    if (heap == NULL) {
#ifndef EMU_ST
        devmm_drv_err("Invalid addr, get heap failed. (va=0x%llx)\n", vmma->info.va);
        return -EINVAL;
#endif
    }

    para.va = vmma->info.va;
    para.size = vmma->info.size;
    para.id = vmma->info.phy_addr_blk_id;
    para.side = vmma->info.side;
    para.pg_type = vmma->info.pg_type;
    para.module_id = vmma->info.module_id;
    para.pg_num = vmma->info.phy_addr_blk_pg_num;

    is_da_addr = svm_is_da_addr(svm_proc, vmma->info.va, vmma->info.size);
    ret = devmm_mem_map_pg_bitmap_state_set(heap, &para, vmma->info.logic_devid, is_da_addr);
    if (ret != 0) {
#ifndef EMU_ST
        devmm_drv_err("Set owner mmap bitmap failed. (va=0x%llx; ret=%d)\n", vmma->info.va, ret);
        return ret;
#endif
    }

    ret = _devmm_ioctl_mem_map(svm_proc, &vmma->info);
    if (ret != 0) {
#ifndef EMU_ST
        devmm_drv_err("Map owner vmma failed. (va=0x%llx; size=%llu; ret=%d)\n", vmma->info.va, vmma->info.size, ret);
        devmm_vmma_owner_mmap_bitmap_clear(svm_proc, vmma, heap);
        return ret;
#endif
    }

    vmma->info.owner_mapped = true;
    return 0;
}

int devmm_ioctl_mem_query_owner(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_query_owner_para *para = &arg->data.mem_query_owner_para;
    struct devmm_svm_heap *heap = NULL;
    struct devmm_vmma_struct *vmma = NULL;

    vmma = devmm_ioctl_vmma_matched_get(svm_proc, para->va, para->size, &heap);
    if (vmma == NULL) {
        return -EINVAL;
    }

    para->devid = vmma->info.devid;
    para->local_handle_flag = vmma->info.local_handle_flag;
    devmm_vmma_put(vmma);

    return 0;
}

static bool devmm_is_mem_access_type_valid(drv_mem_access_type type)
{
    return ((type == MEM_ACCESS_TYPE_NONE) || (type == MEM_ACCESS_TYPE_READ) || (type == MEM_ACCESS_TYPE_READWRITE));
}

static int _devmm_vmm_set_non_owner_access(struct devmm_svm_process *svm_proc, struct devmm_vmma_struct *vmma,
                                           struct devmm_mem_set_access_para *para, u32 access_udevid)
{
    drv_mem_access_type current_type = vmma->device_access_type[access_udevid];
    int ret;

    if (current_type == para->type) {
        return 0;
    }

    if (para->type == MEM_ACCESS_TYPE_NONE) {
        ret = svm_vmm_unset_access(svm_proc, vmma, access_udevid);
        if (ret == 0) {
            vmma->device_access_type[access_udevid] = MEM_ACCESS_TYPE_NONE;
        }
        return ret;
    }

    if (current_type != MEM_ACCESS_TYPE_NONE) {
        devmm_drv_err("Repeat set access. (va=0x%llx; logic_devid=%u)\n", para->va, para->logic_devid);
        return -EEXIST;
    }

    ret = svm_vmm_set_access(svm_proc, vmma, access_udevid, para->type);
    if (ret != 0) {
        devmm_drv_err("Set mapped owner access failed. (va=0x%llx; devid=%u; ret=%d)\n", para->va, access_udevid, ret);
        return ret;
    }
    vmma->device_access_type[access_udevid] = para->type;
    return 0;
}

static int _devmm_vmm_set_owner_access(struct devmm_svm_process *svm_proc, struct devmm_vmma_struct *vmma,
                                       drv_mem_access_type type)
{
    int ret;

    if (vmma->info.type == type) {
        return 0;
    }
    if ((vmma->info.type != MEM_ACCESS_TYPE_NONE) && (type != MEM_ACCESS_TYPE_NONE)) {
        devmm_drv_err("Repeat set owner access. (va=0x%llx; current_type=%u; new_type=%u)\n", vmma->info.va,
                      vmma->info.type, type);
        return -EEXIST;
    }

    if (type == MEM_ACCESS_TYPE_NONE) {
        if (!vmma->info.owner_mapped) {
            vmma->info.type = MEM_ACCESS_TYPE_NONE;
            return 0;
        }
        return devmm_owner_unset_access(svm_proc, vmma);
    }

    if (!vmma->info.owner_mapped) {
        ret = devmm_vmma_owner_mmap(svm_proc, vmma);
        if (ret != 0) {
#ifndef EMU_ST
            devmm_drv_err("Map owner access failed. (va=0x%llx; ret=%d)\n", vmma->info.va, ret);
            return ret;
#endif
        }
    }
    vmma->info.type = type;
    return 0;
}

int devmm_ioctl_mem_set_access(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_set_access_para *para = &arg->data.mem_set_access_para;
    struct devmm_svm_heap *heap = NULL;
    struct devmm_vmma_struct *vmma = NULL;
    u32 udevid;
    int ret;

    ret = uda_devid_to_udevid_ex(para->logic_devid, &udevid);
    if (ret != 0) {
        devmm_drv_err("Get udevid failed. (logic_devid=%u)\n", para->logic_devid);
        return ret;
    }

    if ((para->logic_devid >= DEVMM_MAX_LOGIC_DEVICE_NUM) || (udevid >= DEVMM_MAX_ACCESS_DEVICE_NUM) ||
        (!devmm_is_mem_access_type_valid(para->type))) {
        devmm_drv_err("Invalid para. (logic_devid=%u; type=%u)\n", para->logic_devid, (u8)para->type);
        return -EINVAL;
    }

    vmma = devmm_ioctl_vmma_matched_get(svm_proc, para->va, para->size, &heap);
    if (vmma == NULL) {
        return -EINVAL;
    }

    ret = devmm_vmma_shared_set(&heap->vmma_mng, vmma);
    if (ret != 0) {
        devmm_vmma_put(vmma);
        return ret;
    }

    ka_task_mutex_lock(&vmma->mutex);
    if (udevid == vmma->info.devid) {
        ret = _devmm_vmm_set_owner_access(svm_proc, vmma, para->type);
    } else {
        ret = _devmm_vmm_set_non_owner_access(svm_proc, vmma, para, udevid);
    }
    ka_task_mutex_unlock(&vmma->mutex);

    devmm_vmma_shared_clear(vmma);
    devmm_vmma_put(vmma);
    return ret;
}

int devmm_ioctl_mem_get_access(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_get_access_para *para = &arg->data.mem_get_access_para;
    struct devmm_svm_heap *heap = NULL;
    struct devmm_vmma_struct *vmma = NULL;
    u32 udevid;
    int ret;

    ret = uda_devid_to_udevid_ex(para->logic_devid, &udevid);
    if (ret != 0) {
        devmm_drv_err("Get udevid failed. (logic_devid=%u)\n", para->logic_devid);
        return ret;
    }

    if ((para->logic_devid >= DEVMM_MAX_LOGIC_DEVICE_NUM) || (udevid >= DEVMM_MAX_ACCESS_DEVICE_NUM)) {
        devmm_drv_err("Invalid para. (logic_devid=%u)\n", para->logic_devid);
        return -EINVAL;
    }

    heap = devmm_svm_get_heap(svm_proc, para->va);
    if (heap == NULL) {
        devmm_drv_err("Is idle addr. (va=0x%llx)\n", para->va);
        return -EINVAL;
    }

    vmma = devmm_vmma_get(&heap->vmma_mng, para->va);
    if (vmma == NULL) {
        devmm_drv_err("Get vmma failed. (va=0x%llx)\n", para->va);
        return -EINVAL;
    }

    if ((para->va + para->size) > (vmma->info.va + vmma->info.size)) {
        devmm_drv_err("Invalid para. (va=0x%llx; vmma->va=0x%llx; size=0x%llx; vmma->size=0x%llx)\n", para->va,
                      vmma->info.va, para->size, vmma->info.size);
        devmm_vmma_put(vmma);
        return -EINVAL;
    }

    ka_task_mutex_lock(&vmma->mutex);

    para->size = vmma->info.size;
    if (udevid == vmma->info.devid) {
        para->type = vmma->info.type;
    } else {
        para->type = vmma->device_access_type[udevid];
    }

    ka_task_mutex_unlock(&vmma->mutex);

    devmm_vmma_put(vmma);

    return 0;
}
