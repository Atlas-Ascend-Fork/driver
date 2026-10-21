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
#include "ka_base_pub.h"
#include "ka_task_pub.h"
#include "ka_common_pub.h"
#include "ka_memory_pub.h"
#include "ka_system_pub.h"
#include "ka_sched_pub.h"
#include "ka_ioctl_pub.h"

#include "comm_kernel_interface.h"
#include "comm_msg_chan.h"
#include "pbl_feature_loader.h"
#include "pbl_uda.h"
#include "pbl_spod_info.h"
#include "dpa_kernel_interface.h"
#include "securec.h"

#include "svm_gfp.h"
#include "svm_gup.h"
#include "svm_pgtable.h"
#include "svm_slab.h"
#include "svm_mm.h"
#include "svm_smp.h"
#include "svm_kern_log.h"
#include "dbi_kern.h"
#include "pmq.h"
#include "pmq_client.h"
#include "svm_ioctl_ex.h"
#include "framework_cmd.h"
#include "framework_vma.h"
#include "dma_map_flag.h"
#include "dma_map_ctx.h"
#include "ksvmm.h"
#include "dma_map_msg_send.h"
#include "dma_map_ops.h"

static bool svm_is_host_va(struct svm_global_va *dst_va)
{
    return (dst_va->udevid == uda_get_host_id());
}

static int _svm_dma_map_query_host_io_addr(ka_mm_struct_t *mm, u64 va, u64 size, struct svm_pa_seg pa_seg[],
                                           u64 *seg_num)
{
    ka_vm_area_struct_t *vma = NULL;
    u64 query_size;
    int ret;

    vma = ka_mm_find_vma(mm, va);
    if (vma == NULL) {
        svm_err("Find vma failed. (va=0x%llx; size=%llu)\n", va, size);
        return -ENOSYS;
    }

    ret = svm_check_va_range(va, size, ka_mm_get_vm_start(vma), ka_mm_get_vm_end(vma));
    if (ret != 0) {
        svm_err("Vma is invalid. (va=0x%llx; size=0x%llx; vma_start=0x%lx; vma_end=0x%lx)\n", va, size,
                ka_mm_get_vm_start(vma), ka_mm_get_vm_end(vma));
        return -EFAULT;
    }

    if ((ka_mm_get_vm_flags(vma) & KA_VM_PFNMAP) == 0) {
        svm_err("Vma is not pfn-mapped. (va=0x%llx; size=%llu)\n", va, size);
        return -EINVAL;
    }

    query_size = svm_query_phys(vma, va, size, pa_seg, seg_num);
    if (query_size != size) {
        svm_err("Query pa failed. (va=0x%llx; size=0x%llx; query_size=0x%llx)\n", va, size, query_size);
        return -EFAULT;
    }

    /* Only check the first page, svm_query_phys will check mixed mappings. */
    if (svm_pa_is_local_mem(pa_seg[0].pa)) {
        svm_err("Pa shouldn't be local memory. (va=0x%llx; size=%llu)\n", va, size);
        return -EINVAL;
    }

    return 0;
}

static int svm_dma_map_query_host_io_addr(int tgid, u64 va, u64 size, struct svm_pa_seg pa_seg[], u64 *seg_num)
{
    ka_mm_struct_t *mm = NULL;
    int ret;

    mm = svm_mm_get(tgid, false);
    if (mm == NULL) {
        return -ENOSYS;
    }

    ret = _svm_dma_map_query_host_io_addr(mm, va, size, pa_seg, seg_num);
    svm_mm_put(mm, false);
    return ret;
}

static int svm_dma_map_pin_host_pa(struct dma_map_node *map_node)
{
    struct svm_global_va *dst_va = &map_node->align_dst_va;
    u64 page_num = dst_va->size / KA_MM_PAGE_SIZE;
    u32 gup_flag = 0;
    int ret;

    map_node->pages = (ka_page_t **)svm_kvzalloc(page_num * sizeof(ka_page_t *), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (map_node->pages == NULL) {
        svm_err("Malloc pages mng failed. (page_num=%llu)\n", page_num);
        return -ENOMEM;
    }

    gup_flag |= map_node->is_write ? SVM_GUP_FLAG_ACCESS_WRITE : 0;
    gup_flag |= map_node->is_svm_va ? 0 : SVM_GUP_FLAG_CHECK_PA_LOCAL; /* svm va will check local by smp, ksvmm */
    ret = svm_pin_uva_npages(dst_va->va, page_num, gup_flag, map_node->pages, &map_node->is_pfn_map);
    if (ret != 0) {
        svm_err("Pin host addr failed. (ret=%d; va=0x%llx; size=0x%llx)\n", ret, dst_va->va, dst_va->size);
        svm_kvfree(map_node->pages);
        map_node->pages = NULL;
        return ret;
    }

    return 0;
}

static void svm_dma_map_unpin_host_pa(struct dma_map_node *map_node)
{
    struct svm_global_va *dst_va = &map_node->align_dst_va;
    u64 page_num = dst_va->size / KA_MM_PAGE_SIZE;

    svm_unpin_uva_npages(map_node->is_pfn_map, map_node->pages, page_num, page_num);
    svm_kvfree(map_node->pages);
    map_node->pages = NULL;
}

static bool svm_is_local_map(struct svm_global_va *src_info, u32 dst_server_id, u32 dst_udevid, u32 dst_tgid)
{
    return ((src_info->server_id == SVM_INVALID_SERVER_ID) || (src_info->server_id == dst_server_id)) &&
           (src_info->udevid == dst_udevid) && (src_info->tgid == dst_tgid);
}

static void svm_ksvmm_range_unpin_mem(u32 udevid, int tgid, u64 va, u64 size)
{
    struct svm_global_va src_info;
    unsigned long stamp = (unsigned long)ka_jiffies;
    u64 seg_start, cur_va;
    int ret;

    for (cur_va = va; cur_va < (va + size);) {
        ka_try_cond_resched(&stamp);
        ret = ksvmm_get_seg(udevid, tgid, cur_va, &seg_start, &src_info);
        if (ret != 0) {
            return;
        }

        (void)ksvmm_unpin_seg(udevid, tgid, cur_va, 1ULL);
        cur_va = seg_start + src_info.size;
    }
}

static int _svm_ksvmm_range_pin_local_map_mem(u32 udevid, int tgid, u64 va, u64 size, u32 local_server_id, int dst_tgid)
{
    struct svm_global_va src_info;
    unsigned long stamp = (unsigned long)ka_jiffies;
    u64 seg_start, cur_va;
    int ret;

    for (cur_va = va; cur_va < (va + size);) {
        ka_try_cond_resched(&stamp);
        ret = ksvmm_pin_seg(udevid, tgid, cur_va, 1ULL);
        if (ret != 0) {
            svm_err("Pin ksvmm seg failed. (ret=%d; udevid=%u; tgid=%d; va=0x%llx)\n", ret, udevid, tgid, cur_va);
            svm_ksvmm_range_unpin_mem(udevid, tgid, va, cur_va - va);
            return ret;
        }

        ret = ksvmm_get_seg(udevid, tgid, cur_va, &seg_start, &src_info);
        if (ka_unlikely(ret != 0)) {
            svm_err("Get ksvmm seg failed. (ret=%d; va=0x%llx)\n", ret, cur_va);
            (void)ksvmm_unpin_seg(udevid, tgid, cur_va, 1ULL);
            svm_ksvmm_range_unpin_mem(udevid, tgid, va, cur_va - va);
            return ret;
        }

        if (ka_unlikely(!svm_is_local_map(&src_info, local_server_id, udevid, dst_tgid))) {
            svm_err("Isn't local map mem.\n");
            (void)ksvmm_unpin_seg(udevid, tgid, cur_va, 1ULL);
            svm_ksvmm_range_unpin_mem(udevid, tgid, va, cur_va - va);
            return -EINVAL;
        }

        cur_va = seg_start + src_info.size;
    }

    return 0;
}

static int svm_ksvmm_range_pin_local_map_mem(u32 udevid, int tgid, u64 va, u64 size)
{
    struct spod_info info;
    u32 local_server_id = SVM_INVALID_SERVER_ID;
    int ret, dst_tgid;

    ret = dbl_get_spod_info(udevid, &info);
    if (ret != 0) {
        svm_debug("Get server id failed. (udevid=%u)\n", udevid);
    } else {
        local_server_id = (u32)info.server_id;
    }

    if (udevid == uda_get_host_id()) {
        dst_tgid = tgid;
    } else {
        ret = hal_kernel_apm_query_slave_tgid_by_master(tgid, udevid, PROCESS_CP1, &dst_tgid);
        if (ret != 0) {
            svm_err("Apm get cp1 tgid failed. (ret=%d; udevid=%u)\n", ret, udevid);
            return ret;
        }
    }

    return _svm_ksvmm_range_pin_local_map_mem(udevid, tgid, va, size, local_server_id, dst_tgid);
}

static int svm_dma_map_pin_svm_pa(struct dma_map_node *map_node)
{
    struct svm_global_va *dst_va = &map_node->align_dst_va;
    int tgid = map_node->tgid;
    int ret;

    map_node->is_mpl_va = true;
    ret = svm_smp_pin_mem(dst_va->udevid, tgid, dst_va->va, dst_va->size, false); /* For mpl mem. */
    if (ret != 0) {
        ret = svm_ksvmm_range_pin_local_map_mem(dst_va->udevid, tgid, dst_va->va,
                                                dst_va->size); /* For smm local map. */
        if (ret == 0) {
            map_node->is_mpl_va = false;
        }
    }
    if (ret != 0) {
        svm_err("Pin mem failed. (udevid=%u; start=0x%llx; size=0x%llx)\n", dst_va->udevid, dst_va->va, dst_va->size);
        return ret;
    }
    map_node->is_local_mem = true;

    if (svm_is_host_va(dst_va)) {
        ret = svm_dma_map_pin_host_pa(map_node);
        if (ret != 0) {
            if (map_node->is_mpl_va) {
                (void)svm_smp_unpin_mem(dst_va->udevid, tgid, dst_va->va, dst_va->size, false);
            } else {
                svm_ksvmm_range_unpin_mem(dst_va->udevid, tgid, dst_va->va, dst_va->size);
            }
            return ret;
        }
    }

    return 0;
}

static int svm_dma_map_unpin_svm_pa(struct dma_map_node *map_node)
{
    struct svm_global_va *dst_va = &map_node->align_dst_va;
    int tgid = map_node->tgid;
    int ret;

    if (svm_is_host_va(dst_va)) {
        svm_dma_map_unpin_host_pa(map_node);
    }

    if (map_node->is_mpl_va) {
        ret = svm_smp_unpin_mem(dst_va->udevid, tgid, dst_va->va, dst_va->size, false);
        if (ret != 0) {
            svm_debug("Smp unpin mem check. (udevid=%u; start=0x%llx; size=0x%llx)\n", dst_va->udevid, dst_va->va,
                      dst_va->size);
            return ret;
        }
    } else {
        svm_ksvmm_range_unpin_mem(dst_va->udevid, tgid, dst_va->va, dst_va->size);
    }

    return 0;
}

static int svm_dma_map_pin_non_svm_pa(struct dma_map_node *map_node)
{
    struct svm_global_va *dst_va = &map_node->align_dst_va;

    if (svm_is_host_va(dst_va) == false) {
        svm_err("Device only supports svm addr. (udevid=%u; start=0x%llx; size=0x%llx)\n", dst_va->udevid, dst_va->va,
                dst_va->size);
        return -EINVAL;
    }

    if (svm_dma_map_flag_is_va_io_map(map_node->flag)) {
        map_node->is_local_mem = false;
        return 0;
    }
    map_node->is_local_mem = true;

    return svm_dma_map_pin_host_pa(map_node);
}

static int svm_dma_map_unpin_non_svm_pa(struct dma_map_node *map_node)
{
    if (map_node->is_local_mem) {
        svm_dma_map_unpin_host_pa(map_node);
    }

    return 0;
}

static int svm_dma_map_pin_pa(struct dma_map_node *map_node)
{
    struct svm_global_va *dst_va = &map_node->align_dst_va;
    int va_type = VA_TYPE_SVM;
    int ret;

    if (dst_va->udevid == uda_get_host_id()) {
        ret = svm_get_current_task_va_type(dst_va->va, dst_va->size, &va_type);
        if (ret != 0) {
            svm_err("Invalid addr. (udevid=%u; va=0x%llx; size=%llu)\n", dst_va->udevid, dst_va->va, dst_va->size);
            return ret;
        }
    }

    if (va_type == VA_TYPE_SVM) {
        map_node->is_svm_va = true;
        return svm_dma_map_pin_svm_pa(map_node);
    } else {
        map_node->is_svm_va = false;
        return svm_dma_map_pin_non_svm_pa(map_node);
    }
}

static int svm_dma_map_unpin_pa(struct dma_map_node *map_node)
{
    if (map_node->is_svm_va) {
        return svm_dma_map_unpin_svm_pa(map_node);
    } else {
        return svm_dma_map_unpin_non_svm_pa(map_node);
    }
}

static void svm_dma_map_query_host_pa(struct dma_map_node *map_node, struct svm_pa_seg *pa_seg, u64 *seg_num)
{
    u64 i;

    for (i = 0; i < *seg_num; i++) {
        pa_seg[i].pa = ka_mm_page_to_phys(map_node->pages[i]);
        pa_seg[i].size = KA_MM_PAGE_SIZE;
    }
}

static int svm_dma_map_query_host_bar_from_pa_cache(struct svm_global_va *dst_va, struct svm_pa_seg *pa_seg,
                                                    u64 *seg_num)
{
    struct uda_mia_dev_para mia_para = {0};
    u64 tmp_seg_num = *seg_num;
    u32 phy_devid = 0;
    int ret;
    u64 i;

    ret = svm_smp_query_mem_pa(dst_va->udevid, ka_task_get_current_tgid(), dst_va->va, dst_va->size, pa_seg,
                               &tmp_seg_num);
    if (ret != 0) {
        svm_warn("Pa cache not exist. (ret=%d; udevid=%u; tgid=%d; va=0x%llx; size=0x%llx)\n", ret, dst_va->udevid,
                 ka_task_get_current_tgid(), dst_va->va, dst_va->size);
        return ret;
    }

    /* phy_devid handle del later */
    if (uda_is_phy_dev(dst_va->udevid)) {
        phy_devid = dst_va->udevid;
    } else {
        ret = uda_udevid_to_mia_devid(dst_va->udevid, &mia_para);
        if (ret != 0) {
            svm_warn("Get mia devid not succ. (ret=%d; udevid=%u)\n", ret, dst_va->udevid);
            goto out;
        }
        phy_devid = mia_para.phy_devid;
    }

    for (i = 0; i < tmp_seg_num; i++) {
        ret = devdrv_devmem_addr_d2h(phy_devid, pa_seg[i].pa, &pa_seg[i].pa);
        if (ret != 0) {
            svm_warn("Pa to host bar not succ. (ret=%d; phy_devid=%u; dst_va=0x%llx; size=%llu)\n", ret, phy_devid,
                     dst_va->va, dst_va->size);
            goto out;
        }
    }

    *seg_num = tmp_seg_num;
    return 0;

out:
    (void)memset_s(pa_seg, tmp_seg_num * sizeof(struct svm_pa_seg), 0, tmp_seg_num * sizeof(struct svm_pa_seg));
    return ret;
}

static int svm_dma_map_query_host_bar(struct svm_global_va *dst_va, struct svm_pa_seg *pa_seg, u64 *seg_num)
{
    int ret;

    ret = svm_dma_map_query_host_bar_from_pa_cache(dst_va, pa_seg, seg_num);
    if ((ret == 0) || (ret == -EOWNERDEAD)) { /* ret = -EOWNERDEAD : va is free, not send msg retry */
        return ret;
    }

    return svm_pmq_client_host_bar_query(uda_get_host_id(), dst_va, pa_seg, seg_num);
}

static int svm_dma_map_query_device_pa(u32 udevid, struct svm_global_va *dst_va, struct svm_pa_seg *pa_seg,
                                       u64 *seg_num)
{
    if (dst_va->udevid == udevid) {
        enum svm_smmu_mode smmu_mode;
        int ret;

        ret = svm_dbi_kern_query_smmu_mode(dst_va->udevid, &smmu_mode);
        if (ret != 0) {
            return ret;
        }

        if (smmu_mode == SVM_SMMU_MODE_IOVA) {
            /* Agent IOVA is the final same-device DMA address; pa_seg is only a unified intermediate carrier. */
            return svm_dma_map_addr_query_remote(udevid, dst_va, pa_seg, *seg_num, seg_num);
        }

        /* Use pa as dma addr, for esl. */
        return svm_pmq_client_pa_query(uda_get_host_id(), dst_va, pa_seg, seg_num);
    }

    return svm_dma_map_query_host_bar(dst_va, pa_seg, seg_num);
}

static int svm_dma_map_query_svm_pa(u32 udevid, struct dma_map_node *map_node, struct svm_pa_seg *pa_seg, u64 *seg_num)
{
    struct svm_global_va *dst_va = &map_node->align_dst_va;
    int ret = 0;

    if (svm_is_host_va(dst_va)) {
        svm_dma_map_query_host_pa(map_node, pa_seg, seg_num);
    } else {
        ret = svm_dma_map_query_device_pa(udevid, dst_va, pa_seg, seg_num);
    }
    if (ret != 0) {
        svm_err("Query pa failed. (start=0x%llx; size=0x%llx)\n", dst_va->va, dst_va->size);
        return ret;
    }

    return ret;
}

static int svm_dma_map_query_non_svm_pa(u32 udevid, struct dma_map_node *map_node, struct svm_pa_seg *pa_seg,
                                        u64 *seg_num)
{
    if (map_node->is_local_mem) {
        svm_dma_map_query_host_pa(map_node, pa_seg, seg_num);
        return 0;
    } else {
        struct svm_global_va *dst_va = &map_node->align_dst_va;
        return svm_dma_map_query_host_io_addr(map_node->tgid, dst_va->va, dst_va->size, pa_seg, seg_num);
    }
}

static int svm_dma_map_query_pa(u32 udevid, struct dma_map_node *map_node, struct svm_pa_seg *pa_seg, u64 *seg_num)
{
    if (map_node->is_svm_va) {
        return svm_dma_map_query_svm_pa(udevid, map_node, pa_seg, seg_num);
    } else {
        return svm_dma_map_query_non_svm_pa(udevid, map_node, pa_seg, seg_num);
    }
}

static void svm_host_dma_unmap_proc(u32 udevid, struct dma_map_node *map_node)
{
    ka_device_t *dev = uda_get_device(udevid);
    u64 seg_num = map_node->addr_info.dma_addr_seg_num;
    unsigned long stamp = (unsigned long)ka_jiffies;
    u32 i;

    for (i = 0; i < seg_num; i++) {
        struct svm_dma_addr_seg *dma_seg = &map_node->addr_info.seg[i];
        if (dma_seg->size == 0) {
            break;
        }

        ka_try_cond_resched(&stamp);
        if (map_node->is_local_mem) {
            hal_kernel_devdrv_dma_unmap_page_attrs(
                dev, (ka_dma_addr_t)dma_seg->dma_addr, dma_seg->size, KA_DMA_BIDIRECTIONAL,
                KA_DMA_ATTR_SKIP_CPU_SYNC | KA_DMA_ATTR_WRITE_COMBINE | KA_DMA_ATTR_NO_KERNEL_MAPPING);
        } else {
            devdrv_dma_unmap_resource(dev, (ka_dma_addr_t)dma_seg->dma_addr, dma_seg->size, KA_DMA_BIDIRECTIONAL, 0);
        }
        dma_seg->size = 0;
    }
}

static int svm_host_dma_map_proc(u32 udevid, struct dma_map_node *map_node, struct svm_pa_seg *pa_seg)
{
    ka_device_t *dev = uda_get_device(udevid);
    u64 seg_num = map_node->addr_info.dma_addr_seg_num;
    unsigned long stamp = (unsigned long)ka_jiffies;
    u32 i;

    for (i = 0; i < seg_num; i++) {
        ka_dma_addr_t dma_addr;
        ka_page_t *page = NULL;

        ka_try_cond_resched(&stamp);
        if (map_node->is_local_mem) {
            page = svm_pa_to_page(pa_seg[i].pa);
            dma_addr = hal_kernel_devdrv_dma_map_page_attrs(
                dev, page, 0, pa_seg[i].size, KA_DMA_BIDIRECTIONAL,
                KA_DMA_ATTR_SKIP_CPU_SYNC | KA_DMA_ATTR_WRITE_COMBINE | KA_DMA_ATTR_NO_KERNEL_MAPPING);
        } else {
            dma_addr = devdrv_dma_map_resource(dev, (phys_addr_t)pa_seg[i].pa, pa_seg[i].size, KA_DMA_BIDIRECTIONAL, 0);
        }
        if (ka_mm_dma_mapping_error(dev, dma_addr) != 0) {
            svm_host_dma_unmap_proc(udevid, map_node);
            svm_err("Dma mapping error. (error=%d)\n", ka_mm_dma_mapping_error(dev, dma_addr));
            return ka_mm_dma_mapping_error(dev, dma_addr);
        }

        map_node->addr_info.seg[i].dma_addr = (u64)dma_addr;
        map_node->addr_info.seg[i].size = pa_seg[i].size;
    }

    return 0;
}

static void svm_device_dma_unmap_single(struct dma_map_node *map_node, u64 seg_idx)
{
    map_node->addr_info.seg[seg_idx].size = 0;
}

static void svm_device_dma_unmap_proc(u32 udevid, struct dma_map_node *map_node)
{
    u64 seg_num = map_node->addr_info.dma_addr_seg_num;
    u64 i;

    for (i = 0; i < seg_num; i++) {
        svm_device_dma_unmap_single(map_node, i);
    }
}

static int svm_device_dma_map_single(u32 udevid, struct dma_map_node *map_node, struct svm_pa_seg *pa_seg, u64 seg_idx)
{
    u32 dst_udevid = map_node->align_dst_va.udevid;
    int ret;

    if (dst_udevid != udevid) {
        if ((uda_is_pf_dev(udevid) == false) || (uda_is_pf_dev(dst_udevid) == false)) {
            svm_err("Vf not support. (src_udevid=%u; dst_udevid=%u)\n", udevid, dst_udevid);
            return -EINVAL;
        }
    }

    if (dst_udevid != udevid) {
        ret = devdrv_devmem_addr_bar_to_dma(udevid, dst_udevid, (phys_addr_t)pa_seg->pa,
                                            (ka_dma_addr_t *)&map_node->addr_info.seg[seg_idx].dma_addr);
        if (ret != 0) {
            svm_err("Bar to dma failed. (src_udevid=%u; dst_udevid=%u; ret=%d)\n", udevid, dst_udevid, ret);
            return ret;
        }
    } else {
        map_node->addr_info.seg[seg_idx].dma_addr = pa_seg->pa;
    }

    map_node->addr_info.seg[seg_idx].size = pa_seg->size;
    return 0;
}

static int svm_device_dma_map_proc(u32 udevid, struct dma_map_node *map_node, struct svm_pa_seg *pa_seg)
{
    u64 seg_num = map_node->addr_info.dma_addr_seg_num;
    u64 i;
    int ret;

    for (i = 0; i < seg_num; i++) {
        ret = svm_device_dma_map_single(udevid, map_node, &pa_seg[i], i);
        if (ret != 0) {
            return ret;
        }
    }

    return 0;
}

static int svm_dma_map_proc(u32 udevid, struct dma_map_node *map_node, struct svm_pa_seg *pa_seg)
{
    struct svm_global_va *dst_va = &map_node->align_dst_va;

    if (svm_is_host_va(dst_va)) {
        return svm_host_dma_map_proc(udevid, map_node, pa_seg);
    } else {
        return svm_device_dma_map_proc(udevid, map_node, pa_seg);
    }
}

static void svm_dma_unmap_proc(u32 udevid, struct dma_map_node *map_node)
{
    struct svm_global_va *dst_va = &map_node->align_dst_va;

    if (svm_is_host_va(dst_va)) {
        svm_host_dma_unmap_proc(udevid, map_node);
    } else {
        svm_device_dma_unmap_proc(udevid, map_node);
    }
}

static int svm_dma_map_check_remap_pa_segs(struct dma_map_node *map_node, struct svm_pa_seg *pa_seg, u64 start_seg_idx,
                                           u64 remap_seg_num)
{
    u64 i;

    for (i = 0; i < remap_seg_num; i++) {
        if (pa_seg[i].size != map_node->addr_info.seg[start_seg_idx + i].size) {
            return -EINVAL;
        }
    }

    return 0;
}

static int svm_dma_map_remap_device_segs(u32 udevid, struct dma_map_node *map_node, struct svm_pa_seg *pa_seg,
                                         u64 start_seg_idx, u64 remap_seg_num)
{
    struct svm_dma_addr_seg *old_seg = NULL;
    u64 i;
    int ret = 0;

    old_seg = svm_kvzalloc(remap_seg_num * sizeof(*old_seg), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (old_seg == NULL) {
        svm_err("Alloc old dma seg failed. (seg_num=%llu)\n", remap_seg_num);
        return -ENOMEM;
    }

    for (i = 0; i < remap_seg_num; i++) {
        old_seg[i] = map_node->addr_info.seg[start_seg_idx + i];
        svm_device_dma_unmap_single(map_node, start_seg_idx + i);
    }

    for (i = 0; i < remap_seg_num; i++) {
        ret = svm_device_dma_map_single(udevid, map_node, &pa_seg[i], start_seg_idx + i);
        if (ret != 0) {
            break;
        }
    }
    if (ret != 0) {
        for (i = 0; i < remap_seg_num; i++) {
            map_node->addr_info.seg[start_seg_idx + i] = old_seg[i];
        }
    }
    svm_kvfree(old_seg);
    return ret;
}

static int svm_dma_map_master_do_remap(u32 udevid, struct dma_map_node *map_node, struct svm_global_va *remap_info,
                                       u64 start_seg_idx, u64 remap_seg_num)
{
    struct svm_pa_seg *pa_seg = NULL;
    u64 seg_num;
    int ret;

    if (svm_is_host_va(&map_node->align_dst_va) || !map_node->is_svm_va) {
        return -EOPNOTSUPP;
    }

    pa_seg = svm_kvzalloc(remap_seg_num * sizeof(*pa_seg), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (pa_seg == NULL) {
        svm_err("Alloc remap pa seg failed. (seg_num=%llu)\n", remap_seg_num);
        return -ENOMEM;
    }

    seg_num = remap_seg_num;
    ret = svm_dma_map_query_device_pa(udevid, remap_info, pa_seg, &seg_num);
    if (ret != 0) {
        svm_err("Query remap pa failed. (ret=%d; va=0x%llx; size=0x%llx)\n", ret, remap_info->va, remap_info->size);
        goto out;
    }
    if (seg_num != remap_seg_num) {
        svm_err("Invalid remap pa seg num. (seg_num=%llu; expected_num=%llu; va=0x%llx; size=0x%llx)\n", seg_num,
                remap_seg_num, remap_info->va, remap_info->size);
        ret = -EINVAL;
        goto out;
    }

    ret = svm_dma_map_check_remap_pa_segs(map_node, pa_seg, start_seg_idx, remap_seg_num);
    if (ret != 0) {
        svm_err("Invalid remap pa seg size. (va=0x%llx; size=0x%llx)\n", remap_info->va, remap_info->size);
        goto out;
    }

    ret = svm_dma_map_remap_device_segs(udevid, map_node, pa_seg, start_seg_idx, remap_seg_num);
out:
    svm_kvfree(pa_seg);
    return ret;
}

static int svm_dma_map_master_do_map(u32 udevid, struct dma_map_node *map_node, u64 dst_page_size)
{
    struct svm_global_va *dst_va = &map_node->align_dst_va;
    struct svm_pa_seg *pa_seg = NULL;
    u64 seg_num;
    int ret;

    if (dst_page_size == 0ULL) {
        svm_err("Invalid page size. (page_size=%llu)\n", dst_page_size);
        return -EINVAL;
    }

    if (!svm_is_host_va(dst_va)) {
        if (udevid != dst_va->udevid) {
            if (!uda_proc_can_access_udevid(map_node->tgid, dst_va->udevid)) {
                svm_err("Can not access udevid. (udevid=%u)\n", dst_va->udevid);
                return -EINVAL;
            }
        }
    }

    ret = svm_dma_map_pin_pa(map_node);
    if (ret != 0) {
        svm_err("Pin pa failed. (start=0x%llx; size=0x%llx)\n", dst_va->va, dst_va->size);
        return ret;
    }

    seg_num = dst_va->size / dst_page_size;
    pa_seg = svm_kvzalloc(seg_num * sizeof(struct svm_pa_seg), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (pa_seg == NULL) {
        (void)svm_dma_map_unpin_pa(map_node);
        svm_err("Malloc pa seg failed. (seg_num=%llu)\n", seg_num);
        return -ENOMEM;
    }

    ret = svm_dma_map_query_pa(udevid, map_node, pa_seg, &seg_num);
    if (ret != 0) {
        svm_kvfree(pa_seg);
        (void)svm_dma_map_unpin_pa(map_node);
        svm_err("Query pa failed. (start=0x%llx; size=0x%llx)\n", dst_va->va, dst_va->size);
        return ret;
    }

    /* Keep device segments as independent remap units; host segments can still be merged. */
    map_node->addr_info.dma_addr_seg_num = svm_is_host_va(dst_va) ? svm_make_pa_continues(pa_seg, seg_num) : seg_num;
    map_node->addr_info.first_seg_offset = map_node->raw_va - dst_va->va;
    map_node->addr_info.last_seg_len = (pa_seg[map_node->addr_info.dma_addr_seg_num - 1].size -
                                        (dst_va->size - map_node->raw_size - map_node->addr_info.first_seg_offset));
    if (map_node->addr_info.dma_addr_seg_num == 1) {
        map_node->addr_info.last_seg_len -= map_node->addr_info.first_seg_offset;
    }

    map_node->addr_info.seg = svm_kvzalloc(map_node->addr_info.dma_addr_seg_num * sizeof(struct svm_dma_addr_seg),
                                           KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (map_node->addr_info.seg == NULL) {
        svm_kvfree(pa_seg);
        (void)svm_dma_map_unpin_pa(map_node);
        svm_err("Malloc dma seg failed. (seg_num=%llu)\n", seg_num);
        return -ENOMEM;
    }

    ret = svm_dma_map_proc(udevid, map_node, pa_seg);
    svm_kvfree(pa_seg); /* end use pa seg */
    if (ret != 0) {
        svm_kvfree(map_node->addr_info.seg);
        map_node->addr_info.seg = NULL;
        (void)svm_dma_map_unpin_pa(map_node);
        svm_err("Dma map failed. (start=0x%llx; size=0x%llx)\n", dst_va->va, dst_va->size);
        return ret;
    }

    return 0;
}

static int svm_dma_map_master_do_unmap(struct dma_map_node *map_node)
{
    int ret;

    svm_dma_unmap_proc(map_node->udevid, map_node);
    svm_kvfree(map_node->addr_info.seg);
    map_node->addr_info.seg = NULL;
    ret = svm_dma_map_unpin_pa(map_node);

    return ret;
}

static const struct svm_dma_map_ops g_svm_dma_map_master_ops = {
    .do_dma_map = svm_dma_map_master_do_map,
    .do_dma_unmap = svm_dma_map_master_do_unmap,
    .do_dma_remap = svm_dma_map_master_do_remap,
};

static int svm_dst_va_to_global_va(struct svm_dst_va *dst_va, struct svm_global_va *global_va)
{
    int ret;

    svm_global_va_pack(0, 0, dst_va->va, dst_va->size, global_va);

    ret = uda_devid_to_udevid_ex(dst_va->devid, &global_va->udevid);
    if (ret != 0) {
        svm_err("Get udevid failed. (devid=%u)\n", dst_va->devid);
        return ret;
    }

    if (global_va->udevid != uda_get_host_id()) {
        ret = hal_kernel_apm_query_slave_tgid_by_master(ka_task_get_current_tgid(), global_va->udevid,
                                                        dst_va->task_type, &global_va->tgid);
        if (ret != 0) {
            svm_err("Get slave tgid failed. (udevid=%u; task_type=%u)\n", global_va->udevid, dst_va->task_type);
            return ret;
        }
    } else {
        global_va->tgid = ka_task_get_current_tgid();
    }

    return 0;
}

static int svm_ioctl_dma_map_addr(u32 udevid, u32 cmd, unsigned long arg)
{
    struct svm_dma_map_para para;
    struct svm_global_va global_va;
    u64 dst_page_size;
    int ret;

    ret = ka_base_copy_from_user(&para, (void __ka_user *)(uintptr_t)arg, sizeof(para));
    if (ret != 0) {
        svm_err("Copy_from_user fail.\n");
        return -EINVAL;
    }

    ret = svm_dst_va_to_global_va(&para.dst_va, &global_va);
    if (ret != 0) {
        return ret;
    }

    ret = svm_dbi_kern_query_npage_size(global_va.udevid, &dst_page_size);
    if (ret != 0) {
        svm_err("Get page size failed. (udevid=%u)\n", global_va.udevid);
        return ret;
    }

    return svm_dma_map_addr(udevid, ka_task_get_current_tgid(), &global_va, dst_page_size, para.flag, NULL);
}

static int svm_ioctl_dma_unmap_addr(u32 udevid, u32 cmd, unsigned long arg)
{
    struct svm_dma_unmap_para para;
    struct svm_global_va global_va;
    int ret;

    ret = ka_base_copy_from_user(&para, (void __ka_user *)(uintptr_t)arg, sizeof(para));
    if (ret != 0) {
        svm_err("Copy_from_user fail.\n");
        return -EINVAL;
    }

    ret = svm_dst_va_to_global_va(&para.dst_va, &global_va);
    if (ret != 0) {
        return ret;
    }

    return svm_dma_unmap_addr(udevid, ka_task_get_current_tgid(), &global_va);
}

int svm_dma_map_master_register_ops(void);
int svm_dma_map_master_register_ops(void)
{
    svm_register_ioctl_cmd_handle(_KA_IOC_NR(SVM_DMA_MAP), svm_ioctl_dma_map_addr);
    svm_register_ioctl_cmd_handle(_KA_IOC_NR(SVM_DMA_UNMAP), svm_ioctl_dma_unmap_addr);
    return svm_dma_map_register_ops(&g_svm_dma_map_master_ops);
}
DECLAER_FEATURE_AUTO_INIT(svm_dma_map_master_register_ops, FEATURE_LOADER_STAGE_4);
