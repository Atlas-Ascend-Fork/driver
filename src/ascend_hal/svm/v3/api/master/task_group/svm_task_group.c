/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "ascend_hal.h"

#include "pbl_uda_user.h"

#include "svm_log.h"
#include "svm_sys_cmd.h"
#include "svm_vmm.h"
#include "svmm.h"
#include "malloc_mng.h"
#include "smm_client.h"
#include "svm_pipeline.h"
#include "svm_sub_event_type_uk_msg.h"
#include "svm_criu.h"
#include "task_grp_msg.h"
#include "svm_umc_client.h"
#include "svm_umc_server.h"
#include "svm_mem_repair.h"
#include "svm_addr_desc.h"
#include "svm_dbi.h"
#include "svm_apbi.h"
#include "cache_malloc.h"
#include "va_allocator.h"
#include "svm_register.h"

struct svm_sync_task_info {
    u32 devid;
    u32 task_type;
};

static u32 svm_task_grp_bitmap[SVM_MAX_DEV_AGENT_NUM] = {0};

static bool svm_is_need_task_group_sync(u32 devid)
{
    if (devid >= SVM_MAX_DEV_AGENT_NUM) {
        return false;
    }

    return (svm_task_grp_bitmap[devid] != 0);
}

static bool svm_is_need_vmm_task_group_sync(u64 svm_flag, u64 start, u64 size)
{
    return !svm_flag_cap_is_support_vmm_ipc_unmap(svm_flag) && !svm_flag_is_dev_cp_only(svm_flag) &&
           !svm_flag_is_vmm_map_owner_no_access(svm_flag) && !svm_is_in_dcache_va_range(start, size);
}

static int svm_task_grp_map(u32 devid, u64 start, struct svm_global_va *src_info, u32 *task_bitmap)
{
    struct svm_dst_va dst_info;
    u32 task_type, smm_flag;
    int ret;

    smm_flag = 0;
    for (task_type = PROCESS_CP1 + 1; task_type < PROCESS_CPTYPE_MAX; task_type++) {
        if ((svm_task_grp_bitmap[devid] & (0x1U << task_type)) == 0) {
            continue;
        }

        svm_dst_va_pack(devid, task_type, start, src_info->size, &dst_info);
        ret = svm_smm_client_map(&dst_info, src_info, smm_flag);
        if (ret == DRV_ERROR_NONE) {
            *task_bitmap |= 0x1U << (u32)task_type;
        } else if (ret == DRV_ERROR_NO_PROCESS) {
            svm_task_grp_bitmap[devid] &= (~(0x1U << task_type));
        } else {
            svm_err("Smm map failed. (devid=%u; va=0x%llx; task_type=%u)\n", devid, start, task_type);
            return ret;
        }
    }

    return 0;
}

static void svm_task_grp_unmap(u32 devid, u64 start, struct svm_global_va *src_info, u32 task_bitmap)
{
    struct svm_dst_va dst_info;
    u32 task_type, smm_flag;
    int ret;

    smm_flag = 0;
    for (task_type = PROCESS_CP1 + 1; task_type < PROCESS_CPTYPE_MAX; task_type++) {
        if ((task_bitmap & (0x1U << (u32)task_type)) == 0) {
            continue;
        }

        svm_dst_va_pack(devid, task_type, start, src_info->size, &dst_info);
        ret = svm_smm_client_unmap(&dst_info, src_info, smm_flag);
        if ((ret != DRV_ERROR_NONE) && (ret != DRV_ERROR_NO_PROCESS)) {
            svm_err("Smm unmap failed. (devid=%u; va=0x%llx; task_type=%u)\n", devid, start, task_type);
        }
    }
}

static int _svm_task_grp_post_malloc(void *va_handle, u64 start, struct svm_prop *prop)
{
    struct svm_global_va src_info;
    u32 task_bitmap;
    int ret;

    if (!svm_is_need_task_group_sync(prop->devid)) {
        return 0;
    }

    if (!svm_flag_cap_is_support_normal_free(prop->flag)) {
        return 0;
    }

    if (svm_flag_is_dev_cp_only(prop->flag)) {
        return 0;
    }

    if (svm_flag_attr_is_pg_rdonly(prop->flag)) {
        return 0;
    }

    if (svm_flag_is_va_only_without_populate(prop->flag)) {
        return 0;
    }

    svm_global_va_pack(0, prop->tgid, prop->start, prop->aligned_size, &src_info);
    ret = uda_get_udevid_by_devid_ex(prop->devid, &src_info.udevid);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Get udevid failed. (devid=%u; ret=%d)\n", prop->devid, ret);
        return ret;
    }

    task_bitmap = svm_get_task_bitmap(va_handle);
    ret = svm_task_grp_map(prop->devid, start, &src_info, &task_bitmap);
    svm_set_task_bitmap(va_handle, task_bitmap);

    return ret;
}

static int svm_task_grp_post_malloc(void *va_handle, u64 start, struct svm_prop *prop)
{
    if (svm_handle_mem_is_cache(va_handle)) {
        return 0;
    }

    return _svm_task_grp_post_malloc(va_handle, start, prop);
}

static void _svm_task_grp_pre_free(void *va_handle, u64 start, struct svm_prop *prop, u32 task_bitmap)
{
    struct svm_global_va src_info;
    int ret;

    if (!svm_is_need_task_group_sync(prop->devid)) {
        return;
    }

    if (!svm_flag_cap_is_support_normal_free(prop->flag)) {
        return;
    }

    if (svm_flag_is_dev_cp_only(prop->flag)) {
        return;
    }

    if (svm_flag_is_va_only_without_populate(prop->flag)) {
        return;
    }

    svm_global_va_pack(0, prop->tgid, prop->start, prop->aligned_size, &src_info);
    ret = uda_get_udevid_by_devid_ex(prop->devid, &src_info.udevid);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Get udevid failed. (devid=%u; ret=%d)\n", prop->devid, ret);
        return;
    }

    svm_task_grp_unmap(prop->devid, start, &src_info, task_bitmap);
    svm_set_task_bitmap(va_handle, task_bitmap & (0x1U << PROCESS_CP1));
}

static void svm_task_grp_pre_free(void *va_handle, u64 start, struct svm_prop *prop)
{
    if (svm_handle_mem_is_cache(va_handle)) {
        return;
    }

    _svm_task_grp_pre_free(va_handle, start, prop, svm_get_task_bitmap(va_handle));
}

static struct svm_mng_ops task_grp_mng_ops = {.post_malloc = svm_task_grp_post_malloc,
                                              .pre_free = svm_task_grp_pre_free};

static int svm_task_grp_post_expand(u32 devid, u64 start, u64 size)
{
    struct svm_global_va src_info;
    u32 task_bitmap;
    int tgid, ret;

    if ((devid == svm_get_host_devid()) || (!svm_is_need_task_group_sync(devid))) {
        return 0;
    }

    ret = svm_apbi_query_tgid(devid, DEVDRV_PROCESS_CP1, &tgid);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Get mem tgid failed. (ret=%d; devid=%u)\n", ret, devid);
        return ret;
    }

    svm_global_va_pack(0, tgid, start, size, &src_info);
    ret = uda_get_udevid_by_devid_ex(devid, &src_info.udevid);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Get udevid failed. (devid=%u; ret=%d)\n", devid, ret);
        return ret;
    }

    task_bitmap = svm_task_grp_bitmap[devid];
    return svm_task_grp_map(devid, start, &src_info, &task_bitmap);
}

static void svm_task_grp_pre_shrink(u32 devid, u64 start, u64 size)
{
    struct svm_global_va src_info;
    int tgid, ret;

    if ((devid == svm_get_host_devid()) || (!svm_is_need_task_group_sync(devid))) {
        return;
    }

    ret = svm_apbi_query_tgid(devid, DEVDRV_PROCESS_CP1, &tgid);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Get mem tgid failed. (ret=%d; devid=%u)\n", ret, devid);
        return;
    }

    svm_global_va_pack(0, tgid, start, size, &src_info);
    ret = uda_get_udevid_by_devid_ex(devid, &src_info.udevid);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Get udevid failed. (devid=%u; ret=%d)\n", devid, ret);
        return;
    }

    svm_task_grp_unmap(devid, start, &src_info, svm_task_grp_bitmap[devid]);
}

static struct svm_cache_ops task_grp_cache_ops = {.post_expand = svm_task_grp_post_expand,
                                                  .pre_shrink = svm_task_grp_pre_shrink};

static int svm_task_grp_register_map(struct svm_global_va *src_info, u32 devid, u64 dst_va, u32 register_to_peer_flag)
{
    struct svm_dst_va dst_info;
    struct svm_global_va aligned_src_info;
    u64 npage_size, aligned_size;
    u32 task_type = PROCESS_HCCP;
    u32 smm_flag = ((register_to_peer_flag & SVM_REGISTER_TO_PEER_FLAG_READ_ONLY) != 0) ? SVM_SMM_FLAG_PG_RDONLY : 0;
    int ret;

    if (devid >= SVM_MAX_DEV_AGENT_NUM) {
        return 0;
    }

    if ((svm_task_grp_bitmap[devid] & (0x1U << (u32)task_type)) == 0) {
        return 0;
    }

    if ((svm_get_device_connect_type(devid) != HOST_DEVICE_CONNECT_TYPE_UB) ||
        (src_info->udevid != svm_get_host_devid())) {
        return 0;
    }

    ret = svm_dbi_query_npage_size(devid, &npage_size);
    if (ret != 0) {
        svm_err("Get npage size failed. (devid=%u; ret=%d)\n", devid, ret);
        return ret;
    }
    svm_register_compute_aligned(src_info, npage_size, &aligned_src_info, &aligned_size);

    svm_dst_va_pack(devid, task_type, dst_va, aligned_size, &dst_info);
    ret = svm_smm_client_map(&dst_info, &aligned_src_info, smm_flag);
    if (ret == DRV_ERROR_NO_PROCESS) {
        svm_task_grp_bitmap[devid] &= (~(0x1U << task_type));
    } else if (ret != 0) {
        svm_err("Smm map failed. (ret=%d; devid=%u; va=0x%llx; task_type=%u)\n", ret, devid, dst_va, task_type);
        return ret;
    }
    return 0;
}

static void svm_task_grp_register_unmap(struct svm_global_va *src_info, u32 devid, u64 dst_va)
{
    struct svm_dst_va dst_info;
    u32 task_type = PROCESS_HCCP;
    u32 smm_flag = 0;
    int ret;

    if (devid >= SVM_MAX_DEV_AGENT_NUM) {
        return;
    }

    if ((svm_task_grp_bitmap[devid] & (0x1U << (u32)task_type)) == 0) {
        return;
    }

    if ((svm_get_device_connect_type(devid) != HOST_DEVICE_CONNECT_TYPE_UB) ||
        (src_info->udevid != svm_get_host_devid())) {
        return;
    }

    svm_dst_va_pack(devid, task_type, dst_va, src_info->size, &dst_info);
    ret = svm_smm_client_unmap(&dst_info, src_info, smm_flag);
    if ((ret != DRV_ERROR_NONE) && (ret != DRV_ERROR_NO_PROCESS)) {
        svm_err("Smm unmap failed. (devid=%u; va=0x%llx; task_type=%u)\n", devid, dst_va, task_type);
    }
}

static struct svm_register_ops task_grp_register_ops = {.post_map = svm_task_grp_register_map,
                                                        .pre_unmap = svm_task_grp_register_unmap};

static int svm_task_grp_post_map(u32 devid, u64 start, u64 svm_flag, struct svm_global_va *src_info)
{
    u32 task_bitmap = 0;

    if (!svm_is_need_task_group_sync(devid)) {
        return 0;
    }

    if (!svm_is_need_vmm_task_group_sync(svm_flag, start, src_info->size)) {
        return 0;
    }

    return svm_task_grp_map(devid, start, src_info, &task_bitmap);
}

static int svm_task_grp_pre_unmap(u32 devid, u64 start, u64 svm_flag, struct svm_global_va *src_info)
{
    if (!svm_is_need_task_group_sync(devid)) {
        return 0;
    }

    if (!svm_is_need_vmm_task_group_sync(svm_flag, start, src_info->size)) {
        return 0;
    }

    svm_task_grp_unmap(devid, start, src_info, svm_task_grp_bitmap[devid]);
    return 0;
}

static struct svm_vmm_ops task_grp_vmm_ops = {.post_map = svm_task_grp_post_map, .pre_unmap = svm_task_grp_pre_unmap};

int svm_task_grp_map_normal_pa(u64 va, struct svm_prop *prop)
{
    void *handle = svm_handle_get(va);
    int ret;

    if (handle == NULL) {
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = _svm_task_grp_post_malloc(handle, prop->start, prop);
    svm_handle_put(handle);
    return ret;
}

void svm_task_grp_unmap_normal_pa(u64 va, struct svm_prop *prop)
{
    void *handle = svm_handle_get(va);
    if (handle == NULL) {
        return;
    }
    if (prop->devid < SVM_MAX_DEV_AGENT_NUM) {
        _svm_task_grp_pre_free(handle, prop->start, prop, svm_task_grp_bitmap[prop->devid]);
    }
    svm_handle_put(handle);
}

void svm_task_grp_unmap_vmm_pa(u32 devid, u64 va, u64 svm_flag, struct svm_global_va *src_info)
{
    if (!svm_is_need_task_group_sync(devid)) {
        return;
    }

    if (!svm_is_need_vmm_task_group_sync(svm_flag, va, src_info->size)) {
        return;
    }

    svm_task_grp_unmap(devid, va, src_info, svm_task_grp_bitmap[devid]);
}

int svm_task_grp_map_vmm_pa(u32 devid, u64 va, u64 svm_flag, struct svm_global_va *src_info)
{
    u32 task_bitmap = 0;

    if (!svm_is_need_task_group_sync(devid)) {
        return 0;
    }

    if (!svm_is_need_vmm_task_group_sync(svm_flag, va, src_info->size)) {
        return 0;
    }

    return svm_task_grp_map(devid, va, src_info, &task_bitmap);
}

static struct svm_mem_repair_ops task_grp_repair_ops = {.normal_map = svm_task_grp_map_normal_pa,
                                                        .normal_unmap = svm_task_grp_unmap_normal_pa,
                                                        .vmm_map = svm_task_grp_map_vmm_pa,
                                                        .vmm_unmap = svm_task_grp_unmap_vmm_pa};

static int svm_sync_normal_addr_to_task(void *va_handle, u64 start, struct svm_prop *prop, u32 task_type)
{
    u32 devid = prop->devid;
    struct svm_dst_va dst_info;
    struct svm_global_va src_info;
    u32 task_bitmap, smm_flag;
    int ret;

    if (svm_handle_mem_is_cache(va_handle)) {
        return 0;
    }

    if (svm_flag_is_dev_cp_only(prop->flag)) {
        return 0;
    }

    if (svm_flag_attr_is_pg_rdonly(prop->flag)) {
        return 0;
    }

    svm_global_va_pack(0, prop->tgid, start, prop->aligned_size, &src_info);
    ret = uda_get_udevid_by_devid_ex(devid, &src_info.udevid);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Get udevid failed. (devid=%u; ret=%d)\n", devid, ret);
        return ret;
    }

    smm_flag = 0;
    svm_dst_va_pack(devid, task_type, start, prop->aligned_size, &dst_info);
    ret = svm_smm_client_map(&dst_info, &src_info, smm_flag);
    if (ret != 0) {
        svm_err("Smm map failed. (devid=%u; va=0x%llx; task_type=%u)\n", devid, start, task_type);
        return ret;
    }

    task_bitmap = svm_get_task_bitmap(va_handle);
    task_bitmap |= 0x1U << (u32)task_type;
    svm_set_task_bitmap(va_handle, task_bitmap);

    return 0;
}

static int svm_sync_vmm_map_addr_to_task(void *seg_handle, u64 start, struct svm_global_va *src_info, void *priv)
{
    struct svm_sync_task_info *task_info = (struct svm_sync_task_info *)priv;
    struct svm_global_va real_src_info = *src_info;
    u32 devid = task_info->devid;
    struct svm_dst_va dst_info;
    u32 smm_flag;
    u64 svm_flag = svm_svmm_get_seg_svm_flag(seg_handle);
    int ret;

    if (!svm_is_need_vmm_task_group_sync(svm_flag, start, src_info->size)) {
        return 0;
    }

    if (svm_svmm_get_seg_devid(seg_handle) != task_info->devid) {
        return 0;
    }

    vmm_restore_real_src_va(&real_src_info);

    smm_flag = 0;
    svm_dst_va_pack(devid, task_info->task_type, start, real_src_info.size, &dst_info);
    ret = svm_smm_client_map(&dst_info, &real_src_info, smm_flag);
    if (ret != 0) {
        svm_err("Smm map failed. (devid=%u; va=0x%llx; task_type=%u)\n", devid, start, task_info->task_type);
        return ret;
    }

    return 0;
}

static int svm_sync_vmm_addr_to_task(void *vmm_svmm, struct svm_sync_task_info *task_info)
{
    return svm_svmm_for_each_seg_handle(vmm_svmm, svm_sync_vmm_map_addr_to_task, (void *)task_info);
}

static int svm_try_sync_single_addr_to_task(void *va_handle, u64 start, struct svm_prop *prop, void *priv)
{
    struct svm_sync_task_info *task_info = (struct svm_sync_task_info *)priv;

    if (svm_flag_cap_is_support_normal_free(prop->flag)) {
        if (prop->devid != task_info->devid) {
            return 0;
        }

        return svm_sync_normal_addr_to_task(va_handle, start, prop, task_info->task_type);
    } else {
        void *vmm_svmm = vmm_get_svmm(va_handle);
        if (vmm_svmm == NULL) {
            return 0;
        }

        return svm_sync_vmm_addr_to_task(vmm_svmm, task_info);
    }
}

static int svm_sync_cache_to_task(u32 devid, u64 start, u64 size, u32 flag, void *priv)
{
    u32 task_type = ((struct svm_sync_task_info *)priv)->task_type;
    u32 smm_flag;
    struct svm_global_va src_info;
    struct svm_dst_va dst_info;
    int tgid, ret;

    SVM_UNUSED(flag);

    if (devid == svm_get_host_devid()) {
        return 0;
    }

    ret = svm_apbi_query_tgid(devid, DEVDRV_PROCESS_CP1, &tgid);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Get mem tgid failed. (ret=%d; devid=%u)\n", ret, devid);
        return ret;
    }

    svm_global_va_pack(0, tgid, start, size, &src_info);
    ret = uda_get_udevid_by_devid_ex(devid, &src_info.udevid);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Get udevid failed. (devid=%u; ret=%d)\n", devid, ret);
        return ret;
    }

    smm_flag = 0;
    svm_dst_va_pack(devid, task_type, start, src_info.size, &dst_info);
    ret = svm_smm_client_map(&dst_info, &src_info, smm_flag);
    if (ret != 0) {
        svm_err("Smm map failed. (devid=%u; va=0x%llx; task_type=%u)\n", devid, start, task_type);
        return ret;
    }
    return 0;
}

static int svm_sync_register_to_task(void *seg_handle, u64 start, struct svm_global_va *src_info, void *priv)
{
    u64 svm_flag = svm_svmm_get_seg_svm_flag(seg_handle);
    u32 register_to_peer_flag = svm_flag_attr_is_pg_rdonly(svm_flag) ? SVM_REGISTER_TO_PEER_FLAG_READ_ONLY : 0;
    u32 devid = ((struct svm_sync_task_info *)priv)->devid;

    return svm_task_grp_register_map(src_info, devid, start, register_to_peer_flag);
}

static int svm_sync_svm_addr_to_task(u32 devid, u32 task_type)
{
    struct svm_sync_task_info task_info = {.devid = devid, .task_type = task_type};
    int ret;

    ret = svm_cache_for_each_range(devid, svm_sync_cache_to_task, (void *)&task_info);
    if (ret != 0) {
        return ret;
    }

    ret = svm_for_each_valid_handle(svm_try_sync_single_addr_to_task, (void *)&task_info);
    if (ret != 0) {
        return ret;
    }

    if (task_type == PROCESS_HCCP) {
        ret = svm_register_for_each_seg(devid, svm_sync_register_to_task, (void *)&task_info);
    }

    return ret;
}

static int svm_task_grp_add_task(u32 devid, u32 task_type)
{
    int ret;

    ret = svm_apbi_update(devid, (int)task_type);
    if (ret != 0) {
        svm_err("Update apbi failed. (devid=%u; task_type=%u)\n", devid, task_type);
        return ret;
    }

    ret = svm_va_reserve_add_task(devid, (int)task_type);
    if (ret != 0) {
        if (ret == DRV_ERROR_INVALID_DEVICE) {
            /* device not opened */
            svm_task_grp_bitmap[devid] |= (0x1U << (u32)task_type);
            return 0;
        }

        svm_err("Reserve va failed. (devid=%u; task_type=%u)\n", devid, task_type);
        return ret;
    }

    /* Set bitmap before sync so that svm_task_grp_register_map can see the bit and
     * actually execute HCCP page table mapping. If bitmap is set after sync (as before),
     * svm_task_grp_register_map checks the bit, finds it 0, and silently skips the map,
     * leaving HCCP page table never built. */
    svm_task_grp_bitmap[devid] |= (0x1U << (u32)task_type);

    if (!svm_criu_is_device_reopen(devid)) { /* Task grp add task before criu restore. */
        ret = svm_sync_svm_addr_to_task(devid, task_type);
        if (ret != 0) {
            svm_task_grp_bitmap[devid] &= (~(0x1U << (u32)task_type));
            return ret;
        }
    }

    return 0;
}

static int svm_master_add_task_event_proc_func(u32 devid, const void *msg_in, void *msg_out)
{
    const struct svm_task_add_grp *add_grp_msg = (const struct svm_task_add_grp *)msg_in;
    int ret;
    SVM_UNUSED(msg_out);

    svm_occupy_pipeline();
    ret = svm_task_grp_add_task(devid, add_grp_msg->task_type);
    svm_release_pipeline();

    return ret;
}

SVM_EVENT_PROC_REGISTER(SVM_ADD_GRP_EVENT, svm_master_add_task_event_proc_func, (u64)sizeof(struct svm_task_add_grp),
                        0);

static int svm_task_group_dev_uninit(u32 devid)
{
    if (devid < SVM_MAX_DEV_AGENT_NUM) {
        svm_task_grp_bitmap[devid] = 0;
    }

    return 0;
}

static int svm_task_group_criu_reset(u32 devid, void *data)
{
    SVM_UNUSED(data);
    return svm_task_group_dev_uninit(devid);
}

static const struct svm_criu_ops g_task_group_criu_ops = {
    .name = "task_group",
    .reset = svm_task_group_criu_reset,
};

void __attribute__((constructor)) svm_task_group_init(void)
{
    svm_mng_set_ops(&task_grp_mng_ops);
    svm_vmm_set_ops(&task_grp_vmm_ops);
    svm_mem_repair_set_ops(&task_grp_repair_ops);
    svm_cache_set_ops(&task_grp_cache_ops);
    svm_register_set_ops(&task_grp_register_ops);
    (void)svm_register_ioctl_dev_uninit_pre_handle(svm_task_group_dev_uninit);
    if (svm_criu_register_ops(&g_task_group_criu_ops) != DRV_ERROR_NONE) {
        svm_err("Register CRIU ops failed.\n");
    }
}
