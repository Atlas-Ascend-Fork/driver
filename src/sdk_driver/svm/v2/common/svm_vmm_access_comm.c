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
#include "devmm_dev.h"
#include "devmm_proc_mem_copy.h"
#include "svm_mem_mng.h"
#include "svm_phy_addr_blk_mng.h"
#include "svm_proc_mng.h"
#ifdef DRV_HOST
#include "svm_msg_client.h"
#else
#include "agentmm_proc_info.h"
#include "svm_msg_server.h"
#endif

#include "svm_vmm_access_comm.h"

int hugetlb_insert_hugepage_pte_by_pa(ka_mm_struct_t *mm, unsigned long addr, ka_mm_pgprot_t prot, unsigned long pa);
int hugetlb_insert_hugepage_pte_size(ka_mm_struct_t *mm, unsigned long addr, ka_mm_pgprot_t prot, ka_page_t *hpage,
                                     unsigned long page_size);

int svm_vmm_access_pa_query_local(struct devmm_svm_process *svm_proc, int phy_addr_blk_id, u64 offset,
                                  struct svm_pa_seg pa_seg[], u32 *seg_num)
{
    int ret = devmm_phy_addr_blk_query_pa(&svm_proc->phy_addr_blk_mng, phy_addr_blk_id, offset, pa_seg, seg_num);
    if (ret != 0) {
#ifndef EMU_ST
        devmm_drv_err("Query phy addr blk failed. (id=%d; offset=%llu; ret=%d)\n", phy_addr_blk_id, offset, ret);
        return ret;
#endif
    }
    return 0;
}

int svm_vmm_pa_ref_inc_local(struct devmm_svm_process *svm_proc, int phy_addr_blk_id)
{
    struct devmm_phy_addr_blk *blk = NULL;
    int ret;

    blk = devmm_phy_addr_blk_get(&svm_proc->phy_addr_blk_mng, phy_addr_blk_id);
    if (blk == NULL) {
#ifndef EMU_ST
        devmm_drv_err("Get phy addr blk failed. (id=%d)\n", phy_addr_blk_id);
        return -EINVAL;
#endif
    }

    ret = devmm_phy_addr_blk_occupy_inc(blk);
    devmm_phy_addr_blk_put(blk);
    return ret;
}

void svm_vmm_pa_ref_dec_local(struct devmm_svm_process *svm_proc, int phy_addr_blk_id)
{
    struct devmm_phy_addr_blk *blk = NULL;

    blk = devmm_phy_addr_blk_get(&svm_proc->phy_addr_blk_mng, phy_addr_blk_id);
    if (blk == NULL) {
#ifndef EMU_ST
        devmm_drv_warn("Get phy addr blk failed. (id=%d)\n", phy_addr_blk_id);
        return;
#endif
    }

    devmm_phy_addr_blk_occupy_dec(blk);
    devmm_phy_addr_blk_put(blk);
}

#ifdef DRV_HOST
int svm_vmm_pa_ref_op_remote(struct devmm_svm_process *svm_proc, const struct svm_vmm_pa_owner_desc *pa_owner_desc,
                             u32 op)
{
    struct svm_vmm_pa_ref_op_msg msg = {0};
    int ret;

    msg.head.msg_id = DEVMM_VMM_PA_REF_OP_ID;
    msg.head.process_id.hostpid = svm_proc->process_id.hostpid;
    msg.head.process_id.vfid = 0;
    msg.head.dev_id = (u16)pa_owner_desc->udevid;
    msg.pa_owner_desc = *pa_owner_desc;
    msg.op = op;

    ret = devmm_chan_msg_send(&msg, (u32)sizeof(msg), (u32)sizeof(msg));
    if (ret != 0) {
#ifndef EMU_ST
        devmm_drv_err("Operate remote pa ref failed. (devid=%u; id=%d; op=%u; ret=%d)\n", pa_owner_desc->udevid,
                      pa_owner_desc->phy_addr_blk_id, op, ret);
        return ret;
#endif
    }

    return 0;
}
#endif

static ka_vm_area_struct_t *svm_vmm_access_vma_get(struct devmm_svm_process *svm_proc, u64 va, u64 size)
{
    ka_vm_area_struct_t *vma = NULL;

    if ((size == 0) || (va + size < va)) {
#ifndef EMU_ST
        devmm_drv_err("Invalid access range. (va=0x%llx; size=%llu)\n", va, size);
        return NULL;
#endif
    }

    vma = devmm_find_vma(svm_proc, va);
    if ((vma == NULL) || !devmm_is_svm_vma_magic(ka_mm_get_vm_private_data(vma)) || (ka_mm_get_vm_start(vma) > va) ||
        (ka_mm_get_vm_end(vma) < va + size)) {
#ifndef EMU_ST
        devmm_drv_err("Invalid vma range. (va=0x%llx; size=%llu)\n", va, size);
        return NULL;
#endif
    }
    return vma;
}

static void svm_vmm_access_try_pin_page(u64 pa)
{
#ifndef EMU_ST
    if (ka_mm_mem_is_ram(pa)) {
        devmm_pin_page(ka_mm_pfn_to_page(KA_MM_PFN_DOWN(pa)));
    }
#endif
}

static void svm_vmm_access_try_unpin_page(u64 pa)
{
#ifndef EMU_ST
    if (ka_mm_mem_is_ram(pa)) {
        devmm_unpin_page(ka_mm_pfn_to_page(KA_MM_PFN_DOWN(pa)));
    }
#endif
}

static bool svm_vmm_access_page_size_match(u64 va, u64 pa, u64 size, u64 page_size)
{
    return KA_DRIVER_IS_ALIGNED(va, page_size) && KA_DRIVER_IS_ALIGNED(pa, page_size) &&
           KA_DRIVER_IS_ALIGNED(size, page_size);
}

static u64 svm_vmm_access_page_size_get(u64 va, u64 pa, u64 size)
{
    if (devmm_get_end_type() == DEVMM_END_HOST) {
        return KA_MM_PAGE_SIZE;
    } else {
        /* The giant-page mapping interface only supports local memory. */
        if (svm_vmm_access_page_size_match(va, pa, size, KA_GPAGE_SIZE) && ka_mm_mem_is_ram(pa)) {
            return KA_GPAGE_SIZE;
        }
        if (svm_vmm_access_page_size_match(va, pa, size, KA_HPAGE_SIZE)) {
            return KA_HPAGE_SIZE;
        }
        return KA_MM_PAGE_SIZE;
    }
}

#ifndef EMU_ST
#ifndef DRV_HOST
static int svm_vmm_access_pmd_entry_of_free_ptes(ka_pmd_t *pmd, u64 addr, u64 next, struct ka_pgwalk *walk)
{
    ka_vm_area_struct_t *vma = walk->vma;

#ifndef CFG_FEATURE_ENABLE_ASAN
    ka_pgtable_t pte = ka_mm_pmd_pgtable(*pmd);
#endif

    ka_mm_pmd_clear(pmd);
    ka_mm_flush_tlb_range(vma, addr, next);
#ifndef CFG_FEATURE_ENABLE_ASAN
    ka_mm_pte_free(vma->vm_mm, pte);
    ka_mm_dec_nr_ptes(vma->vm_mm);
#endif
    return 0;
}

static void svm_vmm_access_free_ptes(ka_vm_area_struct_t *vma, u64 start, u64 end)
{
    struct ka_pgwalk_ops ops = {NULL};

    ops.pmd_entry = svm_vmm_access_pmd_entry_of_free_ptes;
    (void)ka_walk_page_range(vma, start, end, &ops, NULL);
}

static int svm_vmm_access_pud_entry_of_free_pmds(ka_pud_t *pud, u64 addr, u64 next, struct ka_pgwalk *walk)
{
    ka_vm_area_struct_t *vma = walk->vma;

#ifndef CFG_FEATURE_ENABLE_ASAN
    ka_pmd_t *pmd = ka_mm_pud_pgtable(*pud);
#endif

    ka_mm_pud_clear(pud);
    ka_mm_flush_tlb_range(vma, addr, next);
#ifndef CFG_FEATURE_ENABLE_ASAN
    ka_mm_pmd_free(vma->vm_mm, pmd);
    ka_mm_dec_nr_ptes(vma->vm_mm);
#endif
    return 0;
}

static void svm_vmm_access_free_pmds(ka_vm_area_struct_t *vma, u64 start, u64 end)
{
    struct ka_pgwalk_ops ops = {NULL};

    ops.pud_entry_post = svm_vmm_access_pud_entry_of_free_pmds;
    ops.pmd_entry = svm_vmm_access_pmd_entry_of_free_ptes;
    (void)ka_walk_page_range(vma, start, end, &ops, NULL);
}
#endif
#endif
static int svm_vmm_access_remap_page(ka_vm_area_struct_t *vma, u64 va, u64 pa, u64 page_size, ka_pgprot_t page_prot)
{
#ifndef DRV_HOST
    if (page_size == KA_HPAGE_SIZE) {
#ifndef EMU_ST
        svm_vmm_access_free_ptes(vma, va, va + page_size);
#endif
        return hugetlb_insert_hugepage_pte_by_pa(vma->vm_mm, va, page_prot, pa);
    }
    if (page_size == KA_GPAGE_SIZE) {
#ifndef EMU_ST
        svm_vmm_access_free_pmds(vma, va, va + page_size);
#endif
        return hugetlb_insert_hugepage_pte_size(vma->vm_mm, va, page_prot, ka_mm_pfn_to_page(KA_MM_PFN_DOWN(pa)),
                                                page_size);
    }
#endif

    return ka_mm_remap_pfn_range(vma, va, KA_MM_PFN_DOWN(pa), page_size, page_prot);
}

static int svm_vmm_access_do_va_unmap_pte_entry(ka_pte_t *pte, u64 addr, u64 next, enum ka_pte_level level,
                                                struct ka_pgwalk *walk)
{
    u64 pa;

    pa = KA_MM_PFN_PHYS(ka_mm_pte_pfn(*pte));
    if (level == KA_FINAL_LEVEL) {
        devmm_zap_vma_ptes(walk->vma, addr, next - addr);
#ifndef DRV_HOST
    } else if (level == KA_PMD_LEVEL) {
        ka_mm_pmd_clear((ka_pmd_t *)pte);
    } else {
        ka_mm_pud_clear((ka_pud_t *)pte);
#endif
    }
    svm_vmm_access_try_unpin_page(pa);
    return 0;
}

static int svm_vmm_access_do_va_unmap(ka_vm_area_struct_t *vma, u64 start, u64 end)
{
#ifdef EMU_ST
    devmm_zap_vma_ptes(vma, start, end - start);
#else
    struct ka_pgwalk_ops ops = {NULL};
    int ret;

    ops.pte_entry = svm_vmm_access_do_va_unmap_pte_entry;
    ret = ka_walk_page_range(vma, start, end, &ops, NULL);
    if (ret != 0) {
        devmm_drv_err("Walk page table failed. (start=0x%llx; end=0x%llx; ret=%d)\n", start, end, ret);
        return ret;
    }
#ifndef DRV_HOST
    ka_mm_flush_tlb_range(vma, start, end);
#endif
#endif
    return 0;
}

static int _svm_vmm_access_do_va_map(ka_vm_area_struct_t *vma, u64 va, const struct svm_pa_seg *pa_seg,
                                     ka_pgprot_t page_prot)
{
    u64 page_size = svm_vmm_access_page_size_get(va, pa_seg->pa, pa_seg->size);
    u64 offset;

    if (!svm_vmm_access_page_size_match(va, pa_seg->pa, pa_seg->size, page_size)) {
#ifndef EMU_ST
        devmm_drv_err("Pa range cannot satisfy local page size. (va=0x%llx; size=%llu; page_size=%llu)\n", va,
                      pa_seg->size, page_size);
        return -EINVAL;
#endif
    }

    for (offset = 0; offset < pa_seg->size; offset += page_size) {
        u64 page_va = va + offset;
        u64 page_pa = pa_seg->pa + offset;
        int ret;

        svm_vmm_access_try_pin_page(page_pa);
        ret = svm_vmm_access_remap_page(vma, page_va, page_pa, page_size, page_prot);
        if (ret != 0) {
#ifndef EMU_ST
            devmm_drv_err("Remap pa failed. (va=0x%llx; page_size=%llu; ret=%d)\n", page_va, page_size, ret);
            svm_vmm_access_try_unpin_page(page_pa);
            if (offset != 0) {
                (void)svm_vmm_access_do_va_unmap(vma, va, va + offset);
            }
            return ret;
#endif
        }
    }
    return 0;
}

static int svm_vmm_access_do_va_map(ka_vm_area_struct_t *vma, u64 va, const struct svm_pa_seg pa_seg[], u32 seg_num)
{
    bool is_local_mem = ka_mm_mem_is_ram(pa_seg[0].pa);
    ka_pgprot_t page_prot = is_local_mem ? devmm_make_pgprot(0, false) : devmm_make_remote_pgprot(0);
    struct svm_pa_seg merged_seg;
    u64 mapped_size = 0;
    u32 i;
    int ret;

    merged_seg = pa_seg[0];
    for (i = 1; i <= seg_num; i++) {
        if ((i < seg_num) && ((merged_seg.pa + merged_seg.size) == pa_seg[i].pa)) {
            merged_seg.size += pa_seg[i].size;
            continue;
        }

        ret = _svm_vmm_access_do_va_map(vma, va + mapped_size, &merged_seg, page_prot);
        if (ret != 0) {
            if (mapped_size != 0) {
                (void)svm_vmm_access_do_va_unmap(vma, va, va + mapped_size);
            }
            return ret;
        }
        mapped_size += merged_seg.size;
        if (i < seg_num) {
            merged_seg = pa_seg[i];
        }
    }
    return 0;
}

static int svm_vmm_access_pa_seg_size_check(const struct svm_pa_seg pa_seg[], u32 seg_num, u64 size)
{
    u64 total_size = 0;
    u32 i;

    for (i = 0; i < seg_num; i++) {
        if (pa_seg[i].size > size - total_size) {
            return -EINVAL;
        }
        total_size += pa_seg[i].size;
    }

    return (total_size == size) ? 0 : -EINVAL;
}

static int _svm_vmm_access_va_map_local(struct devmm_svm_process *svm_proc, u64 va, u64 size,
                                        const struct svm_pa_seg pa_seg[], u32 seg_num)
{
    ka_vm_area_struct_t *vma = NULL;
    int ret;

    ret = svm_vmm_access_pa_seg_size_check(pa_seg, seg_num, size);
    if (ret != 0) {
#ifndef EMU_ST
        devmm_drv_err("Invalid pa seg size. (va=0x%llx; size=%llu; seg_num=%u)\n", va, size, seg_num);
        return ret;
#endif
    }

    vma = svm_vmm_access_vma_get(svm_proc, va, size);
    if (vma == NULL) {
        return -EINVAL;
    }

    ret = devmm_check_va_no_map(svm_proc, va, size);
    if (ret == 0) {
        return svm_vmm_access_do_va_map(vma, va, pa_seg, seg_num);
    }
    return ret;
}

int svm_vmm_access_va_map_local(struct devmm_svm_process *svm_proc, struct devmm_svm_heap *heap, u64 va, u64 size,
                                const struct svm_pa_seg pa_seg[], u32 seg_num)
{
    int ret;

    if (heap == NULL) {
#ifndef EMU_ST
        devmm_drv_err("Get heap failed. (va=0x%llx)\n", va);
        return -EINVAL;
#endif
    }

    ret = _svm_vmm_access_va_map_local(svm_proc, va, size, pa_seg, seg_num);
    if (ret == 0) {
        devmm_set_heap_used_status(heap, va, size);
    }
    return ret;
}

int svm_vmm_access_va_unmap_local(struct devmm_svm_process *svm_proc, u64 va, u64 size)
{
    ka_vm_area_struct_t *vma = NULL;

    vma = svm_vmm_access_vma_get(svm_proc, va, size);
    if (vma == NULL) {
        return -EINVAL;
    }

    return svm_vmm_access_do_va_unmap(vma, va, va + size);
}

void svm_vmm_access_heap_unmap_local(struct devmm_svm_process *svm_proc, struct devmm_svm_heap *heap)
{
    u32 used_num = (u32)(ka_base_round_up(heap->heap_size, HEAP_USED_PER_MASK_SIZE) / HEAP_USED_PER_MASK_SIZE);
    u32 nr;

    for (nr = 0; nr < used_num; nr++) {
        ka_vm_area_struct_t *vma = NULL;
        u64 start;
        u64 end;

        if (!ka_base_test_bit(nr, heap->used_mask)) {
            continue;
        }
        start = heap->start + (u64)nr * HEAP_USED_PER_MASK_SIZE;
        end = ka_base_min(start + HEAP_USED_PER_MASK_SIZE, heap->start + heap->heap_size);
        vma = devmm_find_vma(svm_proc, start);
        if (vma != NULL) {
            int ret = svm_vmm_access_do_va_unmap(vma, start, end);
            if (ret != 0) {
                devmm_drv_err("Unmap heap page table failed. (start=0x%llx; end=0x%llx; ret=%d)\n", start, end, ret);
            }
        }
        ka_base_clear_bit(nr, heap->used_mask);
    }
}

static int svm_vmm_access_pa_query_msg_send(struct devmm_svm_process *svm_proc,
                                            const struct svm_vmm_pa_owner_desc *pa_owner_desc, u64 offset,
                                            u32 query_flag, u32 seg_num, struct svm_vmm_access_pa_query_msg *msg)
{
    u32 out_msg_len = (u32)(sizeof(*msg) + sizeof(struct svm_pa_seg) * seg_num);

    msg->head.msg_id = DEVMM_VMM_ACCESS_PA_QUERY_ID;
    msg->head.process_id.hostpid = svm_proc->process_id.hostpid;
    msg->head.process_id.vfid = 0;
    msg->head.extend_num = (u16)seg_num;
    msg->pa_owner_desc = *pa_owner_desc;
    msg->query_flag = query_flag;
    msg->offset = offset;

#ifdef DRV_HOST
    msg->head.dev_id = pa_owner_desc->udevid;
    return devmm_chan_msg_send(msg, (u32)sizeof(*msg), out_msg_len);
#else
    if (pa_owner_desc->udevid == uda_get_host_id()) {
        msg->head.dev_id = svm_proc->process_id.devid;
        return devmm_chan_msg_send(msg, (u32)sizeof(*msg), out_msg_len);
    } else {
        u32 ack_len = out_msg_len;

        msg->head.dev_id = pa_owner_desc->udevid;
        return devmm_p2p_msg_send(svm_proc->process_id.devid, msg, out_msg_len, (u32)sizeof(*msg), &ack_len);
    }
#endif
}

int svm_vmm_access_pa_query_remote(struct devmm_svm_process *svm_proc,
                                   const struct svm_vmm_pa_owner_desc *pa_owner_desc, u64 offset,
                                   struct svm_pa_seg pa_seg[], u32 *seg_num, u32 query_flag)
{
    struct svm_vmm_access_pa_query_msg *msg = NULL;
    u32 out_msg_len = (u32)(sizeof(struct svm_vmm_access_pa_query_msg) + sizeof(struct svm_pa_seg) * (*seg_num));
    u32 input_seg_num = *seg_num;
    u32 i;
    int ret;

    msg = devmm_kvzalloc_ex(out_msg_len, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (msg == NULL) {
#ifndef EMU_ST
        devmm_drv_err("Alloc remote pa query msg failed. (seg_num=%u)\n", input_seg_num);
        return -ENOMEM;
#endif
    }

    ret = svm_vmm_access_pa_query_msg_send(svm_proc, pa_owner_desc, offset, query_flag, input_seg_num, msg);
    if (ret != 0) {
#ifndef EMU_ST
        devmm_drv_err("Query remote pa failed. (devid=%u; offset=%llu; phy_blk_id=%d; ret=%d; seg_num=%u)\n",
                      pa_owner_desc->udevid, offset, pa_owner_desc->phy_addr_blk_id, ret, input_seg_num);
        devmm_kvfree_ex(msg);
        return ret;
#endif
    }

    if ((msg->head.extend_num == 0) || (msg->head.extend_num > input_seg_num)) {
#ifndef EMU_ST
        devmm_drv_err("Invalid remote pa seg num. (devid=%u; input_seg_num=%u; output_seg_num=%u)\n",
                      pa_owner_desc->udevid, input_seg_num, msg->head.extend_num);
        devmm_kvfree_ex(msg);
        return -EINVAL;
#endif
    }

    *seg_num = msg->head.extend_num;
    for (i = 0; i < *seg_num; i++) {
        pa_seg[i] = msg->pa_seg[i];
    }

    devmm_kvfree_ex(msg);
    return ret;
}
