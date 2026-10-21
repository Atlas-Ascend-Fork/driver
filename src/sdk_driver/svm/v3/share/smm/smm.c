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

#include "ka_ioctl_pub.h"
#include "ka_base_pub.h"
#include "ka_task_pub.h"
#include "ka_memory_pub.h"
#include "ka_system_pub.h"
#include "ka_common_pub.h"
#include "ka_compiler_pub.h"
#include "ka_sched_pub.h"

#include "kernel_version_adapt.h"
#include "pbl_feature_loader.h"
#include "dpa_kernel_interface.h"
#include "pbl_uda.h"

#include "svm_dev_topology.h"
#include "svm_addr_desc.h"
#include "svm_pgtable.h"
#include "svm_kern_log.h"
#include "framework_vma.h"
#include "framework_cmd.h"
#include "va_mng.h"
#include "svm_gfp.h"
#include "svm_ioctl_ex.h"
#include "svm_mm.h"
#include "svm_pub.h"
#include "svm_slab.h"
#include "smm_ctx.h"
#include "smm_ioctl.h"
#include "smm_flag.h"
#include "smm_ctrl.h"
#include "pmm.h"
#include "smm.h"

static int svm_munmap_check_dst(struct smm_ctx *ctx, u32 src_udevid, u64 dst_va, u64 dst_size);

static void (*smm_exterrnal_handle)(enum smm_external_op_type op_type, u32 udevid, int tgid, u64 va, u64 size) = NULL;
void svm_smm_register_external_handle(void (*handle)(enum smm_external_op_type op_type, u32 udevid, int tgid, u64 va,
                                                     u64 size))
{
    smm_exterrnal_handle = handle;
}

static void smm_external_handle_proc(enum smm_external_op_type op_type, u32 udevid, int tgid, u64 va, u64 size)
{
    if (smm_exterrnal_handle != NULL) {
        smm_exterrnal_handle(op_type, udevid, tgid, va, size);
    }
}

static int smm_slave_permission_check(u32 udevid, int tgid, struct svm_global_va *src_info)
{
    u32 remote_udevid, slave_udevid, master_tgid, proc_type_bitmap;
    int mode, slave_tgid, ret;

    ret = apm_query_master_info_by_slave(tgid, &master_tgid, &slave_udevid, &mode, &proc_type_bitmap);
    if (ret != 0) {
        svm_info("Not apm bind proc. (udevid=%u; tgid=%d)\n", udevid,
                 tgid); /* May failed in cp2/hccp sync pgtable, do not print err in this scene. */
        return -EPERM;
    }

    if (udevid != slave_udevid) {
        svm_err("Udevid not match. (udevid=%u; tgid=%d; slave_udevid=%d)\n", udevid, tgid, slave_udevid);
        return -EACCES;
    }

    /* cp and hccp is safe */
    if (((proc_type_bitmap & (0x1 << PROCESS_CP1)) != 0) || ((proc_type_bitmap & (0x1 << PROCESS_HCCP)) != 0)) {
        return 0;
    }

    ret = uda_dev_get_remote_udevid(udevid, &remote_udevid);
    if (ret != 0) {
        svm_err("uda_dev_get_remote_udevid failed. (ret=%d; udevid=%u)\n", ret, udevid);
        return ret;
    }

    if (remote_udevid != src_info->udevid) {
        svm_err("Remote_udevid not match. (remote_udevid=%u; tgid=%d; src_remote_udevid=%d)\n", remote_udevid, tgid,
                src_info->udevid);
        return -EACCES;
    }

    /* non cp, check src is cp */
    ret = hal_kernel_apm_query_slave_tgid_by_master(master_tgid, udevid, PROCESS_CP1, &slave_tgid);
    if (ret != 0) {
        svm_err("Not apm bind cp proc. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return -EACCES;
    }

    if (slave_tgid != src_info->tgid) {
        svm_err("Tgid not match. (udevid=%u; tgid=%d; slave_tgid=%d; src_tgid=%d)\n", udevid, tgid, slave_tgid,
                src_info->tgid);
        return -EACCES;
    }

    return 0;
}

static int smm_permission_check(u32 udevid, int tgid, struct svm_global_va *src_info)
{
    if (udevid != uda_get_host_id()) {
        return smm_slave_permission_check(udevid, tgid, src_info);
    }
    return 0;
}

static int smm_para_check(u32 udevid, int tgid, u64 dst_size, struct svm_global_va *src_info)
{
    int ret;

    if ((dst_size == 0) || (dst_size != src_info->size)) {
        svm_err("Invalid size. (dst_size=%llu; src_size=0x%llx)\n", dst_size, src_info->size);
        return -EINVAL;
    }

    if ((src_info->udevid >= smm_get_src_max_udev_num()) || (src_info->va == 0) || (src_info->size == 0) ||
        (src_info->tgid == 0)) {
        svm_err("Invalid src. (va=0x%llx; tgid=%d; size=0x%llx)\n", src_info->va, src_info->tgid, src_info->size);
        return -EINVAL;
    }

    /* does not need to check va, later check vma will check va and size */

    ret = smm_permission_check(udevid, tgid, src_info);
    if (ret != 0) {
        return ret;
    }

    return 0;
}

static void smm_align_pa_seg_boundary(struct svm_pa_seg *pa_seg, u64 seg_num)
{
    u64 pa = pa_seg[0].pa;

    pa_seg[0].pa = svm_align_down(pa, KA_MM_PAGE_SIZE);
    pa_seg[0].size = svm_align_up(pa % KA_MM_PAGE_SIZE + pa_seg[0].size, KA_MM_PAGE_SIZE);
    if (seg_num > 1) {
        pa_seg[seg_num - 1].size = svm_align_up(pa_seg[seg_num - 1].pa % KA_MM_PAGE_SIZE + pa_seg[seg_num - 1].size,
                                                KA_MM_PAGE_SIZE);
    }
}

static int smm_external_mmap(struct smm_ctx *ctx, ka_task_struct_t *task, u32 src_udevid, struct smm_addr *sa, u64 flag)
{
    struct smm_ops *ops = ctx->ops[src_udevid];
    unsigned long stamp = (unsigned long)ka_jiffies;
    struct svm_pa_seg *pa_seg = sa->src_pa_seg;
    u64 seg_num;
    u64 dst_va = svm_align_down(sa->dst_va, KA_MM_PAGE_SIZE);
    u64 dst_size = svm_align_up(sa->dst_va % KA_MM_PAGE_SIZE + sa->dst_size, KA_MM_PAGE_SIZE);
    u64 va = dst_va, remap_size = 0;
    u64 page_size;
    int tgid = ka_task_get_tgid_for_task(task);
    u64 i;

    smm_align_pa_seg_boundary(pa_seg, sa->seg_num);
    seg_num = svm_make_pa_continues(pa_seg, sa->seg_num);

    page_size = svm_get_page_size_by_pa_seg(dst_va, dst_size, pa_seg, seg_num);
    if (page_size == 0) {
        return -EINVAL;
    }

    for (i = 0; i < seg_num; i++) {
        int ret = ops->remap(ctx->udevid, tgid, va, pa_seg[i].pa, pa_seg[i].size, flag);
        if (ret != 0) {
            (void)ops->unmap(ctx->udevid, tgid, dst_va, remap_size);
            svm_err("Remap failed. (ret=%d; va=0x%llx; size=0x%llx; seg_num=%llu; remap_size=0x%llx)\n", ret, dst_va,
                    dst_size, seg_num, remap_size);
            return ret;
        }

        va += pa_seg[i].size;
        remap_size += pa_seg[i].size;
        ka_try_cond_resched(&stamp);
    }

    smm_external_handle_proc(SMM_EXTERNAL_POST_REMAP, ctx->udevid, tgid, dst_va, dst_size);
    sa->dst_page_size = page_size;
    return 0;
}

static int smm_external_munmap(struct smm_ctx *ctx, ka_task_struct_t *task, u32 src_udevid, struct smm_addr *sa)
{
    struct smm_ops *ops = ctx->ops[src_udevid];
    u64 dst_va = svm_align_down(sa->dst_va, KA_MM_PAGE_SIZE);
    u64 dst_size = svm_align_up(sa->dst_va % KA_MM_PAGE_SIZE + sa->dst_size, KA_MM_PAGE_SIZE);
    int tgid = ka_task_get_tgid_for_task(task);
    int ret;

    ret = ops->unmap(ctx->udevid, tgid, dst_va, dst_size);
    if (ret != 0) {
        return ret;
    }

    smm_external_handle_proc(SMM_EXTERNAL_POST_UNMAP, ctx->udevid, tgid, dst_va, dst_size);
    return 0;
}

static int smm_internal_mmap(struct smm_ctx *ctx, ka_task_struct_t *task, u32 src_udevid, u32 src_server_id,
                             struct smm_addr *sa, u64 flag)
{
    ka_vm_area_struct_t *vma = NULL;
    struct svm_pa_seg *pa_seg = sa->src_pa_seg;
    u64 seg_num = sa->seg_num;
    u64 dst_va = sa->dst_va;
    u64 dst_size = sa->dst_size;
    struct svm_pgtlb_attr attr;
    int ret;

    attr.is_noncache = flag & SVM_SMM_FLAG_PG_NC;
    attr.is_rdonly = flag & SVM_SMM_FLAG_PG_RDONLY;
    attr.is_writecombine = (smm_get_pa_location(ctx, src_udevid, src_server_id) != SMM_LOCAL_PA);
    attr.page_size = svm_get_page_size_by_pa_seg(dst_va, dst_size, pa_seg, seg_num);
    if (attr.page_size == 0) {
        return -EINVAL;
    }

    vma = ka_mm_find_vma(ka_task_get_mm(task), dst_va);
    if ((vma == NULL) || (svm_check_vma(vma, dst_va, dst_size) != 0)) {
        svm_err("Svm find vma failed. (va=0x%llx; size=0x%llx)\n", dst_va, dst_size);
        return -EINVAL;
    }

    ret = pmm_add_seg(ctx->udevid, dst_va, dst_size, 0);
    if (ret != 0) {
        svm_err("Add pmm seg failed. (ret=%d; va=0x%llx; size=0x%llx)\n", ret, dst_va, dst_size);
        return ret;
    }

    svm_set_vma_status(vma, VMA_STATUS_NORMAL_OP);
    ret = svm_remap_phys(vma, dst_va, pa_seg, seg_num, &attr);
    svm_set_vma_status(vma, VMA_STATUS_IDLE);
    if (ret != 0) {
        (void)pmm_del_seg(ctx->udevid, dst_va, dst_size, 0);
        svm_err("Remap failed. (ret=%d; va=0x%llx; size=0x%llx; seg_num=%llu)\n", ret, dst_va, dst_size, seg_num);
        return ret;
    }

    sa->dst_page_size = attr.page_size;
    return 0;
}

static int smm_unmap_addr_by_pa_seg(ka_vm_area_struct_t *vma, u64 va, struct svm_pa_seg *pa_seg, u64 seg_num)
{
    u64 cur_size, page_size, i;
    u64 cur_va = va;
    int ret;

    for (i = 0; i < seg_num;) {
        u64 cur_seg_num = svm_get_same_page_size_seg_num_by_pa_seg(cur_va, &pa_seg[i], seg_num - i, &cur_size,
                                                                   &page_size);
        if (cur_seg_num == 0) {
            svm_warn("Invalid pa seg. (va=0x%llx; size=0x%llx; cur_seg_num=%llu)\n", cur_va, pa_seg[i].size,
                     seg_num - i);
            return -EINVAL;
        }

        ret = svm_unmap_addr(vma, cur_va, cur_size, page_size);
        if (ret != 0) {
            return ret;
            ;
        }
        cur_va += cur_size;
        i += cur_seg_num;
    }

    return 0;
}
static int smm_internal_munmap(struct smm_ctx *ctx, ka_task_struct_t *task, u32 src_udevid, struct smm_addr *sa)
{
    ka_vm_area_struct_t *vma = NULL;
    struct svm_pa_seg *pa_seg = sa->src_pa_seg;
    u64 seg_num = sa->seg_num;
    u32 udevid = ctx->udevid;
    u64 dst_va = sa->dst_va;
    u64 dst_size = sa->dst_size;
    u64 query_size;
    int ret;

    /* vma query and unmap must be locked together */
    vma = ka_mm_find_vma(ka_task_get_mm(task), dst_va);
    if ((vma == NULL) || (svm_check_vma(vma, dst_va, dst_size) != 0)) {
        svm_err("Find vma failed. (udevid=%u; dst_va=0x%llx; dst_size=0x%llx)\n", udevid, dst_va, dst_size);
        return -EINVAL;
    }

    query_size = svm_query_phys(vma, dst_va, dst_size, pa_seg, &seg_num);
    if (query_size != dst_size) {
        svm_err("Smm query pa failed. (udevid=%u; dst_va=0x%llx; dst_size=0x%llx; query_size=0x%llx)\n", udevid, dst_va,
                dst_size, query_size);
        return -EINVAL;
    }

    sa->seg_num = seg_num; /* update for later pa put */

    svm_set_vma_status(vma, VMA_STATUS_NORMAL_OP);
    ret = smm_unmap_addr_by_pa_seg(vma, dst_va, pa_seg, seg_num);
    svm_set_vma_status(vma, VMA_STATUS_IDLE);
    if (ret == 0) {
        (void)pmm_del_seg(ctx->udevid, dst_va, dst_size, 0);
    }

    return ret;
}

static void smm_try_complement_pa_ref(struct svm_pa_seg *src_pa_seg, u64 seg_num, u64 dst_page_size)
{
#ifndef EMU_ST /* Simulation ST can not check local mem */
    unsigned long stamp = (unsigned long)ka_jiffies;
    bool is_local = svm_pa_is_local_mem(src_pa_seg[0].pa);
    ka_page_t *pg = NULL;
    u64 i, j, num;

    if (is_local) {
        for (i = 0; i < seg_num; i++) {
            num = src_pa_seg[i].size / dst_page_size;
            for (j = 1; j < num; j++) {
                pg = ka_mm_pfn_to_page(KA_MM_PFN_DOWN(src_pa_seg[i].pa + j * dst_page_size));
                ka_mm_get_page(pg);

                ka_try_cond_resched(&stamp);
            }

            ka_try_cond_resched(&stamp);
        }
    }
#endif
}

static int smm_single_mmap(struct smm_ctx *ctx, ka_task_struct_t *task, struct svm_global_va *src_info,
                           struct smm_addr *sa, u64 flag)
{
    int ret;

    ret = smm_pa_get(ctx, src_info, sa->src_pa_seg, &sa->seg_num, flag);
    if (ret != 0) {
        svm_err("Smm get pa failed. (ret=%d; udev=%u; src_udev=%u)\n", ret, ctx->udevid, src_info->udevid);
        return ret;
    }

    if (smm_is_ops_support_remap(ctx, src_info->udevid)) {
        ret = smm_external_mmap(ctx, task, src_info->udevid, sa, flag);
    } else {
        ka_task_down_write(get_mmap_sem(ka_task_get_mm(task)));
        ret = smm_internal_mmap(ctx, task, src_info->udevid, src_info->server_id, sa, flag);
        if (ret == 0) {
            /*
             * Complement the asymmetry ops of pa ref:
             * - smm_map->smm_pa_get() pin pages by src page_size.
             * - smm_munmap->smm_pa_put() unpin pages by dst page_size (<= src page_size).
             */
            smm_try_complement_pa_ref(sa->src_pa_seg, sa->seg_num, sa->dst_page_size);
        }
        ka_task_up_write(get_mmap_sem(ka_task_get_mm(task)));
    }
    if (ret != 0) {
        smm_pa_put(ctx, src_info, sa->src_pa_seg, sa->seg_num);
    }

    return ret;
}

static int smm_single_munmap(struct smm_ctx *ctx, ka_task_struct_t *task, struct svm_global_va *src_info,
                             struct smm_addr *sa, u64 flag)
{
    int ret;

    SVM_UNUSED(flag);

    ret = svm_munmap_check_dst(ctx, src_info->udevid, sa->dst_va, sa->dst_size);
    if (ret != 0) {
        return ret;
    }

    if (smm_is_ops_support_remap(ctx, src_info->udevid)) {
        ret = smm_external_munmap(ctx, task, src_info->udevid, sa);
    } else {
        ka_task_down_write(get_mmap_sem(ka_task_get_mm_for_task(task)));
        ret = smm_internal_munmap(ctx, task, src_info->udevid, sa);
        ka_task_up_write(get_mmap_sem(ka_task_get_mm_for_task(task)));
    }

    if (ret == 0) {
        smm_pa_put(ctx, src_info, sa->src_pa_seg, sa->seg_num);
    }

    return ret;
}

static int smm_split_mmap(struct smm_ctx *ctx, ka_task_struct_t *task, struct svm_global_va *src_info, u64 dst_va,
                          u64 flag, bool need_pa_seg)
{
    struct smm_split_target target = {.ctx = ctx, .task = task, .src_info = *src_info, .dst_va = dst_va, .flag = flag};
    struct smm_split_policy policy = {
        .min_page_size = 4ULL * SVM_BYTES_PER_KB, /* Get pa seg by src page size which may not local os PAGE_SIZE */
        .max_single_size = SVM_BYTES_PER_GB,
        .need_pa_seg = need_pa_seg};

    return smm_split_walk(&target, &policy, smm_single_mmap, smm_single_munmap);
}

static int smm_split_munmap(struct smm_ctx *ctx, ka_task_struct_t *task, struct svm_global_va *src_info, u64 dst_va,
                            u64 flag, bool need_pa_seg)
{
    struct smm_split_target target = {.ctx = ctx, .task = task, .src_info = *src_info, .dst_va = dst_va, .flag = flag};
    struct smm_split_policy policy = {
        .min_page_size = KA_MM_PAGE_SIZE, .max_single_size = SVM_BYTES_PER_GB, .need_pa_seg = need_pa_seg};

    return smm_split_walk(&target, &policy, smm_single_munmap, NULL);
}

static int _smm_mmap(struct smm_ctx *ctx, ka_task_struct_t *task, struct svm_global_va *src_info, u64 *dst_va, u64 flag)
{
    u64 dst_size = svm_align_up(src_info->va % KA_MM_PAGE_SIZE + src_info->size, KA_MM_PAGE_SIZE);
    u64 alloc_va = svm_align_down(*dst_va, KA_MM_PAGE_SIZE);
    u32 src_udevid = src_info->udevid;
    int tgid = ka_task_get_tgid_for_task(task);
    int ret;

    ret = smm_alloc_va(ctx, tgid, src_udevid, dst_size, &alloc_va);
    if (ret != 0) {
        svm_err("Malloc va failed. (ret=%d; src_udevid=%u; size=0x%llx)\n", ret, src_udevid, dst_size);
        return ret;
    }

    /* dst_va = alloc_va + offset, non-aligned */
    *dst_va = alloc_va + (src_info->va % KA_MM_PAGE_SIZE);

    ret = smm_split_mmap(ctx, task, src_info, *dst_va, flag, true);
    if (ret != 0) {
        svm_err("Split map failed. (ret=%d; src_udevid=%u; dst_size=0x%llx)\n", ret, src_udevid, dst_size);
        (void)smm_free_va(ctx, tgid, src_udevid, alloc_va, dst_size);
    }

    return ret;
}

static int _smm_munmap(struct smm_ctx *ctx, ka_task_struct_t *task, struct svm_global_va *src_info, u64 dst_va)
{
    u64 dst_size = svm_align_up(src_info->va % KA_MM_PAGE_SIZE + src_info->size, KA_MM_PAGE_SIZE);
    u64 alloc_va = svm_align_down(dst_va, KA_MM_PAGE_SIZE);
    u64 flag = 0;
    u32 src_udevid = src_info->udevid;
    bool need_pa_seg = !smm_is_ops_support_remap(ctx, src_udevid);
    int tgid = ka_task_get_tgid_for_task(task);
    int ret;

    ret = smm_split_munmap(ctx, task, src_info, dst_va, flag, need_pa_seg);
    if (ret != 0) {
        svm_warn("Smm try split munmap failed. (ret=%d; src_udevid=%u; dst_size=0x%llx)\n", ret, src_udevid, dst_size);
        return ret;
    }

    ret = smm_free_va(ctx, tgid, src_udevid, alloc_va, dst_size);
    if (ret != 0) {
        svm_warn("Free va failed. (va=0x%llx; size=0x%llx)\n", alloc_va, dst_size);
        ret = 0; /* ignore the error, return success */
    }

    return ret;
}

static int smm_mmap(u32 udevid, ka_task_struct_t *task, struct svm_global_va *src_info, u64 *dst_va, u64 flag)
{
    struct smm_ctx *ctx = NULL;
    int ret;

    ctx = smm_ctx_get(udevid);
    if (ctx == NULL) {
        svm_warn("Invalid udevid. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    ret = _smm_mmap(ctx, task, src_info, dst_va, flag);
    smm_ctx_put(ctx);

    return ret;
}

static int smm_munmap(u32 udevid, ka_task_struct_t *task, struct svm_global_va *src_info, u64 dst_va)
{
    struct smm_ctx *ctx = NULL;
    int ret;

    ctx = smm_ctx_get(udevid);
    if (ctx == NULL) {
        svm_warn("Invalid udevid. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    ret = _smm_munmap(ctx, task, src_info, dst_va);
    smm_ctx_put(ctx);

    return ret;
}

static int svm_smm_ioctl_mmap(u32 udevid, u32 cmd, unsigned long arg)
{
    struct svm_global_va *src_info = NULL;
    struct svm_smm_map_para *user_para = (struct svm_smm_map_para __ka_user *)(uintptr_t)arg;
    struct svm_smm_map_para para;
    int tgid = ka_task_get_current_tgid();
    u64 dst_va;
    int ret;

    ret = ka_base_copy_from_user(&para, user_para, sizeof(struct svm_smm_map_para));
    if (ret != 0) {
        svm_err("Smm copy arg from user failed. (ret=%d; arg=0x%lx)\n", ret, arg);
        return -EINVAL;
    }

    ret = svm_call_ioctl_pre_handler(udevid, cmd, (void *)&para);
    if (ret != 0) {
        svm_err("Pre handle failed. (udevid=%u; ret=%d)\n", udevid, ret);
        return ret;
    }

    src_info = &para.src_info;
    dst_va = para.dst_va;
    ret = smm_para_check(udevid, tgid, para.dst_size, src_info);
    if (ret != 0) {
        svm_call_ioctl_pre_cancle_handler(udevid, cmd, (void *)&para);
        return ret;
    }

    ret = smm_mmap(udevid, ka_task_get_current(), src_info, &dst_va, para.flag);
    if (ret != 0) {
        svm_err("Mmap failed. (udevid=%u; ret=%d)\n", udevid, ret);
        svm_call_ioctl_pre_cancle_handler(udevid, cmd, (void *)&para);
        return ret;
    }

    if (dst_va != para.dst_va) {
        ret = ka_base_put_user(dst_va, &user_para->dst_va);
        if (ret != 0) {
            svm_err("Put va failed. (ret=%d; va=0x%llx)\n", ret, dst_va);
            (void)smm_munmap(udevid, ka_task_get_current(), src_info, dst_va);
            svm_call_ioctl_pre_cancle_handler(udevid, cmd, (void *)&para);
            return ret;
        }
    }

    ret = svm_call_ioctl_post_handler(udevid, cmd, (void *)&para);
    if (ret != 0) {
        svm_err("Post handle failed. (udevid=%u; dst_va=0x%llx; size=%llu)\n", udevid, para.dst_va, para.dst_size);
        (void)smm_munmap(udevid, ka_task_get_current(), src_info, dst_va);
        svm_call_ioctl_pre_cancle_handler(udevid, cmd, (void *)&para);
        return ret;
    }

    return 0;
}

static int svm_get_munmap_page_size(u64 va, u64 size, u64 *page_size)
{
    int va_type;
    int ret;

    ret = svm_get_current_task_va_type(va, size, &va_type);
    if (ret != 0) {
        svm_err("Invalid addr. (va=0x%llx; size=0x%llx)\n", va, size);
        return ret;
    }

    if (va_type == VA_TYPE_SVM) {
        *page_size = svm_task_va_to_page_size(ka_task_get_current(), va);
        if (*page_size == 0) {
            svm_err("Svm va to page size failed. (va=0x%llx)\n", va);
            return -EINVAL;
        }
    } else {
        *page_size = KA_MM_PAGE_SIZE;
    }

    return 0;
}

static int svm_munmap_check_dst(struct smm_ctx *ctx, u32 src_udevid, u64 dst_va, u64 dst_size)
{
    u64 page_size = KA_MM_PAGE_SIZE;
    int ret = 0;

    if (!smm_is_ops_support_remap(ctx, src_udevid)) {
        ret = svm_get_munmap_page_size(dst_va, dst_size, &page_size);

        if ((SVM_IS_ALIGNED(dst_va, page_size) == 0) || (SVM_IS_ALIGNED(dst_size, page_size) == 0)) {
            svm_err("Smm va isn't aligned by page_size. (va=0x%llx; size=%llu; page_size=%llu)\n", dst_va, dst_size,
                    page_size);
            return -EINVAL;
        }
    }

    return ret;
}

static int svm_smm_ioctl_munmap(u32 udevid, u32 cmd, unsigned long arg)
{
    struct svm_global_va *src_info = NULL;
    struct svm_smm_unmap_para para;
    int tgid = ka_task_get_current_tgid();
    int ret;

    ret = ka_base_copy_from_user(&para, (void __ka_user *)(uintptr_t)arg, sizeof(struct svm_smm_unmap_para));
    if (ret != 0) {
        svm_err("Smm copy arg from user failed. (ret=%d; arg=0x%lx)\n", ret, arg);
        return -EINVAL;
    }

    if (para.dst_va == 0) {
        svm_err("Invalid para. (va=0x%llx; size=0x%llx)\n", para.dst_va, para.dst_size);
        return -EINVAL;
    }

    ret = svm_call_ioctl_pre_handler(udevid, cmd, (void *)&para);
    if (ret != 0) {
        svm_err("Pre handle failed. (udevid=%u; ret=%d)\n", udevid, ret);
        return ret;
    }

    src_info = &para.src_info;
    ret = smm_para_check(udevid, tgid, para.dst_size, src_info);
    if (ret != 0) {
        return ret;
    }

    ret = smm_munmap(udevid, ka_task_get_current(), src_info, para.dst_va);
    if (ret != 0) {
        return ret;
    }

    ret = svm_call_ioctl_post_handler(udevid, cmd, (void *)&para);
    if (ret != 0) {
        svm_err("Post handle failed. (udevid=%u; dst_va=0x%llx; size=%llu)\n", udevid, para.dst_va, para.dst_size);
        return ret;
    }

    return 0;
}

int svm_smm_feature_init(void)
{
    svm_register_ioctl_cmd_handle(_KA_IOC_NR(SVM_SMM_MAP), svm_smm_ioctl_mmap);
    svm_register_ioctl_cmd_handle(_KA_IOC_NR(SVM_SMM_UNMAP), svm_smm_ioctl_munmap);

    return 0;
}
DECLAER_FEATURE_AUTO_INIT(svm_smm_feature_init, FEATURE_LOADER_STAGE_6);
