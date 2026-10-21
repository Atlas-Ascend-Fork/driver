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
#include "ka_task_pub.h"
#include "ka_system_pub.h"
#include "ka_memory_pub.h"
#include "ka_common_pub.h"
#include "ka_sched_pub.h"

#include "pbl_runenv_config.h"
#include "pbl_uda.h"
#include "dpa_kernel_interface.h"

#include "kernel_version_adapt.h"
#include "svm_mm.h"
#include "svm_gfp.h"
#include "svm_slab.h"
#include "svm_kern_log.h"
#include "svm_addr_desc.h"

#include "svm_pgtable.h"

struct svm_pa_info {
    u64 pa;
    u8 pa_local_flag;
};

#define GPAGE_SHIFT 30ULL
#define GPAGE_SIZE (1ULL << GPAGE_SHIFT)

static struct svm_page_table_ops *pgtbl_ops[SVM_PAGE_GRAN_MAX] = {
    NULL,
};
static struct svm_page_table_externed_ops *pgtbl_externed_ops = NULL;

void svm_register_page_table_ops(enum svm_page_granularity gran, const struct svm_page_table_ops *ops)
{
    pgtbl_ops[gran] = (struct svm_page_table_ops *)ops;
}

void svm_get_page_table_ops(enum svm_page_granularity gran, struct svm_page_table_ops **ops)
{
    *ops = pgtbl_ops[gran];
}

void svm_register_page_table_externed_ops(const struct svm_page_table_externed_ops *ops)
{
    pgtbl_externed_ops = (struct svm_page_table_externed_ops *)ops;
}

static void svm_pre_remap(ka_vm_area_struct_t *vma, u64 va, u64 size)
{
    if (pgtbl_externed_ops != NULL) {
        pgtbl_externed_ops->pre_remap(vma, va, size);
    }
}

static void svm_remap_cancle(ka_vm_area_struct_t *vma, u64 va, u64 size)
{
    if (pgtbl_externed_ops != NULL) {
        pgtbl_externed_ops->remap_cancle(vma, va, size);
    }
}

static void svm_post_remap(ka_vm_area_struct_t *vma, u64 va, u64 size)
{
    if (pgtbl_externed_ops != NULL) {
        pgtbl_externed_ops->post_remap(vma, va, size);
    }
}

static void svm_post_unmap(ka_vm_area_struct_t *vma, u64 va, u64 size)
{
    if (pgtbl_externed_ops != NULL) {
        pgtbl_externed_ops->post_unmap(vma, va, size);
    }
}

#ifndef EMU_ST /* Simulation ST ignore pte */
int svm_pte_to_pfn(ka_pte_t *pte, u64 *pfn)
{
    if (pgtbl_externed_ops != NULL) {
        return pgtbl_externed_ops->pte_to_pfn(pte, pfn);
    }

    *pfn = (u64)ka_mm_pte_pfn(*pte);
    return 0;
}

struct svm_pgwalk_data_of_va_to_pfn {
    u64 pfn;
    u64 page_size;
};

static int svm_pte_hole_of_va_to_pfn(u64 addr, u64 next, enum ka_pte_level level, struct ka_pgwalk *walk)
{
    return -EFAULT;
}

static int svm_pte_entry_of_va_to_pfn(ka_pte_t *pte, u64 addr, u64 next, enum ka_pte_level level,
                                      struct ka_pgwalk *walk)
{
    struct svm_pgwalk_data_of_va_to_pfn *data = (struct svm_pgwalk_data_of_va_to_pfn *)walk->priv;
    u64 page_size;

    page_size = ka_pte_level_to_page_size(level);
    if (page_size == 0ULL) {
        return -EFAULT;
    }

    if (svm_pte_to_pfn(pte, &data->pfn) != 0) {
        return -EFAULT;
    }

    data->page_size = page_size;
    return 0;
}

static int svm_va_to_pfn(ka_vm_area_struct_t *vma, u64 va, u64 *pfn, u64 *page_size)
{
    struct svm_pgwalk_data_of_va_to_pfn data = {0};
    struct ka_pgwalk_ops ops = {NULL};
    int ret;

    ops.pte_hole = svm_pte_hole_of_va_to_pfn;
    ops.pte_entry = svm_pte_entry_of_va_to_pfn;

    ret = ka_walk_page_range(vma, va, va + 1ULL, &ops, (void *)&data);
    if (ret == 0) {
        *pfn = data.pfn;
        *page_size = data.page_size;
    }
    return ret;
}
#endif

int svm_va_to_pa(ka_vm_area_struct_t *vma, u64 va, u64 *pa, u64 *page_size)
{
    u64 pfn;
    int ret;

    ret = svm_va_to_pfn(vma, va, &pfn, page_size);
    if (ret != 0) {
        return ret;
    }
    *pa = (unsigned long)KA_MM_PFN_PHYS(pfn);
    *pa += (va - ka_base_round_down(va, *page_size));
    return 0;
}

u64 svm_va_to_page_size(u64 tgid, u64 va)
{
    ka_vm_area_struct_t *vma = NULL;
    ka_mm_struct_t *mm = NULL;
    u64 page_size = 0;
    u64 pfn;

    mm = svm_mm_get(tgid, false);
    if (mm == NULL) {
        return 0;
    }

    vma = ka_mm_find_vma(mm, va);
    if (vma == NULL) {
        svm_mm_put(mm, false);
        return 0;
    }

    (void)svm_va_to_pfn(vma, va, &pfn, &page_size);

    svm_mm_put(mm, false);
    return page_size;
}

u64 svm_task_va_to_page_size(ka_task_struct_t *task, u64 va)
{
    ka_mm_struct_t *mm = ka_task_get_mm(task);
    ka_vm_area_struct_t *vma = NULL;
    u64 page_size = 0;
    u64 pfn;

    ka_task_down_read(get_mmap_sem(mm));

    vma = ka_mm_find_vma(mm, va);
    if (vma == NULL) {
        ka_task_up_read(get_mmap_sem(mm));
        return 0;
    }

    (void)svm_va_to_pfn(vma, va, &pfn, &page_size);

    ka_task_up_read(get_mmap_sem(mm));
    return page_size;
}

u64 svm_page_size_to_page_shift(u64 page_size)
{
    switch (page_size) {
        case KA_MM_PAGE_SIZE:
            return KA_MM_PAGE_SHIFT;
        case KA_HPAGE_SIZE:
            return KA_MM_HPAGE_SHIFT;
        case SVM_GPAGE_SIZE:
            return SVM_GPAGE_SHIFT;
        default:
            return 0;
    }
}

ka_page_t *svm_pa_to_page(u64 pa)
{
    return ka_mm_pfn_to_page((unsigned long)KA_MM_PFN_DOWN(pa));
}

static inline u64 svm_make_pgprot_val(bool is_noncache, bool is_rdonly)
{
    if (dbl_get_deployment_mode() == DBL_HOST_DEPLOYMENT) {
        return is_rdonly ? ka_pgprot_val(KA_PAGE_READONLY) : ka_pgprot_val(KA_PAGE_SHARED);
    } else {
        u64 prot_val = 0;

#ifndef DRV_HOST /* sc warn */
        /* PTE_RDONLY will case page_fault by smmu, which will affect performance */
        if (is_rdonly) {
            prot_val = ka_pgprot_val(KA_PAGE_READONLY);
        } else {
            /* _EXEC will cause ESL performance degradation */
            prot_val = (ka_pgprot_val(KA_PAGE_SHARED) & (~PTE_RDONLY)) | PTE_DIRTY;
        }

        prot_val = is_noncache ? (prot_val | PROT_NORMAL_NC) : prot_val;
#endif
        return prot_val;
    }
}

static ka_mm_pgprot_t svm_make_pgprot(struct svm_pgtlb_attr *attr, bool is_local_mem)
{
    ka_mm_pgprot_t prot;

    if (attr->is_writecombine) {
        prot = ka_mm_pgprot_writecombine(__ka_pgprot(svm_make_pgprot_val(attr->is_noncache, attr->is_rdonly)));
    } else {
        prot = __ka_pgprot(svm_make_pgprot_val(attr->is_noncache, attr->is_rdonly));
    }

    if (!run_in_confidential_virtual_mach()) {
        return prot;
    }

    /* Local RAM is private in a confidential VM, while peer memory must remain shared. */
    return is_local_mem ? ka_mm_pgprot_encrypted(prot) : ka_mm_pgprot_decrypted(prot);
}

#ifndef EMU_ST /* Simulation ST ignore pte */
struct svm_pgwalk_data_of_query_pages {
    u64 page_size;
    u64 queried_page_num;
    u64 page_num;
    ka_page_t **pages;
};

static int svm_pte_entry_of_query_pages(ka_pte_t *pte, u64 addr, u64 next, enum ka_pte_level level,
                                        struct ka_pgwalk *walk)
{
    struct svm_pgwalk_data_of_query_pages *data = (struct svm_pgwalk_data_of_query_pages *)walk->priv;
    ka_page_t *page = NULL;
    u64 page_size, pa, pfn;

    page_size = ka_pte_level_to_page_size(level);
    if (page_size == 0ULL) {
        return -EFAULT;
    }

    if (data->queried_page_num == 0ULL) {
        data->page_size = page_size;
    }

    /* page size is same */
    if (page_size != data->page_size) {
        return -EFAULT;
    }

    if (svm_pte_to_pfn(pte, &pfn) != 0) {
        return -EFAULT;
    }

    pa = (unsigned long)KA_MM_PFN_PHYS(pfn);
    pa += (addr - ka_base_round_down(addr, page_size));

    /* only support local mem */
    if (!svm_pa_is_local_mem(pa)) {
        return -EFAULT;
    }

    if (!SVM_IS_ALIGNED(pa, page_size)) {
        svm_err("Pa not page align. (tmp_va=0x%llx; page_size=0x%llx)\n", addr, page_size);
        return -EFAULT;
    }

    page = svm_pa_to_page(pa);

    if ((data->queried_page_num + 1ULL) <= data->page_num) {
        data->pages[data->queried_page_num++] = page;
    }

    if (data->queried_page_num == data->page_num) {
        return -ENOBUFS;
    }

    return 0;
}

static int svm_pte_hole_of_query_pfn(u64 addr, u64 next, enum ka_pte_level level, struct ka_pgwalk *walk)
{
    return -EFAULT;
}

u64 svm_query_pages(ka_vm_area_struct_t *vma, u64 va, u64 size, ka_page_t **pages, u64 *page_num)
{
    struct svm_pgwalk_data_of_query_pages data = {0};
    struct ka_pgwalk_ops ops = {NULL};

    data.pages = pages;
    data.page_num = *page_num;
    ops.pte_hole = svm_pte_hole_of_query_pfn;
    ops.pte_entry = svm_pte_entry_of_query_pages;

    (void)ka_walk_page_range(vma, va, va + size, &ops, (void *)&data);

    *page_num = data.queried_page_num;
    return (data.page_size * data.queried_page_num);
}

struct svm_pgwalk_data_of_query_phys {
    bool is_local_mem;
    u64 queried_seg_num;
    u64 queried_seg_size;
    u64 seg_num;
    struct svm_pa_seg *pa_seg;
};

static int svm_pte_entry_of_query_phys(ka_pte_t *pte, u64 addr, u64 next, enum ka_pte_level level,
                                       struct ka_pgwalk *walk)
{
    struct svm_pgwalk_data_of_query_phys *data = (struct svm_pgwalk_data_of_query_phys *)walk->priv;
    u64 page_size, pa, pfn;

    page_size = ka_pte_level_to_page_size(level);
    if (page_size == 0ULL) {
        return -EFAULT;
    }

    if (svm_pte_to_pfn(pte, &pfn) != 0) {
        return -EFAULT;
    }

    pa = (unsigned long)KA_MM_PFN_PHYS(pfn);
    pa += (addr - ka_base_round_down(addr, page_size));

    if (data->queried_seg_num == 0ULL) {
        data->is_local_mem = svm_pa_is_local_mem(pa);
    } else {
        /* query pa is not mixed local and remote */
        if (data->is_local_mem != svm_pa_is_local_mem(pa)) {
            return -EFAULT;
        }
    }

    if ((data->queried_seg_num + 1ULL) <= data->seg_num) {
        data->pa_seg[data->queried_seg_num].pa = pa;
        data->pa_seg[data->queried_seg_num].size = next - addr;
        data->queried_seg_size += data->pa_seg[data->queried_seg_num].size;
        data->queried_seg_num++;
    }

    if (data->queried_seg_num == data->seg_num) {
        return -ENOBUFS;
    }

    return 0;
}

u64 svm_query_phys(ka_vm_area_struct_t *vma, u64 va, u64 size, struct svm_pa_seg pa_seg[], u64 *seg_num)
{
    struct svm_pgwalk_data_of_query_phys data = {0};
    struct ka_pgwalk_ops ops = {NULL};

    data.pa_seg = pa_seg;
    data.seg_num = *seg_num;
    ops.pte_hole = svm_pte_hole_of_query_pfn;
    ops.pte_entry = svm_pte_entry_of_query_phys;

    /* cannot return fail, because pmm_mem_recycle_single_seg may get part of size */
    (void)ka_walk_page_range(vma, va, va + size, &ops, (void *)&data);

    *seg_num = data.queried_seg_num;
    return data.queried_seg_size;
}

static int svm_pte_entry_of_check_va_not_map(ka_pte_t *pte, u64 addr, u64 next, enum ka_pte_level level,
                                             struct ka_pgwalk *walk)
{
    u64 pfn;

    if (svm_pte_to_pfn(pte, &pfn) != 0) {
        return 0;
    }

    svm_err("Va has been mapped. (start=0x%llx; end=%llu; level=%u)\n", addr, next, level);
    return -EFAULT;
}

static bool svm_is_va_range_not_map(ka_vm_area_struct_t *vma, u64 start, u64 end)
{
    struct ka_pgwalk_ops ops = {NULL};
    int ret;

    ops.pte_entry = svm_pte_entry_of_check_va_not_map;
    ret = ka_walk_page_range(vma, start, end, &ops, NULL);
    return (ret == 0);
}
#else
bool svm_is_va_range_not_map(ka_vm_area_struct_t *vma, u64 start, u64 end);
#endif

static u64 svm_get_continuous_page_num(ka_page_t **pages, u64 page_num)
{
    unsigned long stamp = (unsigned long)ka_jiffies;
    u64 pre_pfn = ka_mm_page_to_pfn(pages[0]);
    u64 continuous_num, i;

    for (i = 1; i < page_num; i++) {
        u64 post_pfn = ka_mm_page_to_pfn(pages[i]);
        if ((pre_pfn + 1) != post_pfn) {
            break;
        }
        pre_pfn = post_pfn;
        ka_try_cond_resched(&stamp);
    }
    continuous_num = i;
    return continuous_num;
}

int svm_remap_pages(ka_vm_area_struct_t *vma, u64 va, ka_page_t **pages, u64 page_num,
                    struct svm_pgtlb_attr *pgtlb_attr)
{
    enum svm_page_granularity gran = svm_page_size_to_page_gran(pgtlb_attr->page_size);
    u64 page_size = pgtlb_attr->page_size;
    u64 total_size = page_num * page_size;
    ka_mm_pgprot_t prot = svm_make_pgprot(pgtlb_attr, true);
    unsigned long stamp = ka_jiffies;
    u64 i;

    if ((gran >= SVM_PAGE_GRAN_MAX) || (pgtbl_ops[gran] == NULL)) {
        svm_err("Not support size. (page_size=0x%llx; gran=%u)\n", pgtlb_attr->page_size, gran);
        return -EINVAL;
    }

    if (!svm_is_va_range_not_map(vma, va, va + (page_num * page_size))) {
        return -EFAULT;
    }

    svm_pre_remap(vma, va, total_size);

    for (i = 0; i < page_num;) {
        u64 continuous_num = (page_size == KA_MM_PAGE_SIZE) ? svm_get_continuous_page_num(&pages[i], page_num - i) : 1;
        int ret = pgtbl_ops[gran]->remap(vma, va + i * page_size, ka_mm_page_to_phys(pages[i]), continuous_num, prot);
        if (ret != 0) {
            if (i > 0) {
                pgtbl_ops[gran]->unmap(vma, va, i);
            }

            svm_remap_cancle(vma, va, total_size);
            return ret;
        }
        i += continuous_num;
        ka_try_cond_resched(&stamp);
    }

    svm_post_remap(vma, va, total_size);

    return 0;
}

static u64 svm_get_continuous_phys_size(struct svm_pa_seg pa_seg[], u64 seg_num, u64 *continuous_size)
{
    unsigned long stamp = (unsigned long)ka_jiffies;
    u64 pre_pa = pa_seg[0].pa;
    u64 pre_size = pa_seg[0].size;
    u64 continuous_num, i;

    *continuous_size = pa_seg[0].size;
    for (i = 1; i < seg_num; i++) {
        u64 post_pa = pa_seg[i].pa;
        u64 post_size = pa_seg[i].size;
        if ((pre_pa + pre_size) != post_pa) {
            break;
        }
        pre_pa = post_pa;
        pre_size = post_size;
        *continuous_size += pa_seg[i].size;
        ka_try_cond_resched(&stamp);
    }
    continuous_num = i;
    return continuous_num;
}

int svm_remap_phys(ka_vm_area_struct_t *vma, u64 va, struct svm_pa_seg pa_seg[], u64 seg_num,
                   struct svm_pgtlb_attr *pgtlb_attr)
{
    enum svm_page_granularity gran = svm_page_size_to_page_gran(pgtlb_attr->page_size);
    u64 page_size = pgtlb_attr->page_size;
    ka_mm_pgprot_t prot = svm_make_pgprot(pgtlb_attr, svm_pa_is_local_mem(pa_seg[0].pa));
    unsigned long stamp = ka_jiffies;
    u64 start, i, total_size;

    if ((gran >= SVM_PAGE_GRAN_MAX) || (pgtbl_ops[gran] == NULL)) {
        svm_err("Not support size. (page_size=%llu)\n", pgtlb_attr->page_size);
        return -EINVAL;
    }

    total_size = 0;
    for (i = 0; i < seg_num; i++) {
        total_size += pa_seg[i].size;
    }

    if (!svm_is_va_range_not_map(vma, va, va + total_size)) {
        return -EFAULT;
    }

    svm_pre_remap(vma, va, total_size);

    start = va;
    for (i = 0; i < seg_num;) {
        u64 continuous_size = pa_seg[i].size;
        u64 continuous_num = (page_size == KA_MM_PAGE_SIZE) ?
                                 svm_get_continuous_phys_size(&pa_seg[i], seg_num - i, &continuous_size) :
                                 1;
        int ret = pgtbl_ops[gran]->remap(vma, start, pa_seg[i].pa, continuous_size / page_size, prot);
        if (ret != 0) {
            if (i > 0) {
                pgtbl_ops[gran]->unmap(vma, va, (start - va) / page_size);
            }

            svm_remap_cancle(vma, va, total_size);
            return ret;
        }
        start += continuous_size;
        i += continuous_num;
        ka_try_cond_resched(&stamp);
    }

    svm_post_remap(vma, va, total_size);

    return 0;
}

int svm_unmap_addr(ka_vm_area_struct_t *vma, u64 va, u64 size, u64 page_size)
{
    enum svm_page_granularity gran = svm_page_size_to_page_gran(page_size);
    if ((gran >= SVM_PAGE_GRAN_MAX) || (pgtbl_ops[gran] == NULL)) {
        svm_err("Not support size. (page_size=%llu)\n", page_size);
        return -EINVAL;
    }

    pgtbl_ops[gran]->unmap(vma, va, size / page_size);
    svm_post_unmap(vma, va, size);
    return 0;
}
