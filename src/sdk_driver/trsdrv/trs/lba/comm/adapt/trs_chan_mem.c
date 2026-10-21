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
#include "ka_common_pub.h"
#include "ka_memory_pub.h"
#include "kernel_cgroup_mem_adapt.h"
#include "trs_chan.h"
#include "trs_rsv_mem.h"
#include "trs_chan_mem.h"

static void trs_chan_mem_make_exact(ka_page_t *p, u32 order, size_t size)
{
#ifndef CFG_FEATURE_HOST_ENV
    ka_page_t *cur_page = NULL;
    u64 alloced_num, page_num, i;

    if (order == 0) {
        return;
    }
    ka_mm_split_page(p, order);
    ka_split_page_memcg(p, order);
    alloced_num = 1ull << order;
    page_num = KA_MM_PAGE_ALIGN(size) / KA_MM_PAGE_SIZE;

    for (i = page_num; i < alloced_num; i++) {
        cur_page = ka_mm_nth_page(p, i);
        __ka_mm_free_page(cur_page);
    }
#endif
}

void *trs_chan_mem_alloc_ddr(struct trs_id_inst *inst, int nid, size_t size, phys_addr_t *paddr)
{
    u32 order = ka_mm_get_order(size);
    ka_page_t *p = NULL;

    if (nid == KA_NUMA_NO_NODE) {
        p = ka_mm_alloc_pages(__KA_GFP_ZERO | __KA_GFP_ACCOUNT | KA_GFP_HIGHUSER_MOVABLE | __KA_GFP_NOWARN, order);
    } else {
        p = ka_mm_alloc_pages_node(
            nid, __KA_GFP_THISNODE | __KA_GFP_ZERO | __KA_GFP_ACCOUNT | KA_GFP_HIGHUSER_MOVABLE | __KA_GFP_NOWARN,
            order);
    }
    if (p == NULL) {
        return NULL;
    }
    trs_chan_mem_make_exact(p, order, size);
    *paddr = ka_mm_page_to_phys(p);

    return (void *)ka_mm_page_address(p);
}

void trs_chan_mem_free_ddr(struct trs_id_inst *inst, void *vaddr, size_t size)
{
    if (vaddr != NULL) {
#ifdef CFG_FEATURE_HOST_ENV
        ka_mm_free_pages((unsigned long)(uintptr_t)vaddr, ka_mm_get_order(size));
#else
        ka_mm_free_pages_exact(vaddr, size);
#endif
    }
}

int trs_chan_mem_alloc_attr_lists(struct trs_chan_mem_attr *mem_attr, u32 pa_num)
{
    mem_attr->pa_wraper_list = (struct ka_pa_wraper *)trs_vzalloc(sizeof(struct ka_pa_wraper) * pa_num);
    if (mem_attr->pa_wraper_list == NULL) {
        trs_err("Alloc pa_wrapper_list failed.\n");
        return -ENOMEM;
    }

    mem_attr->vmap_pfn_handle = kzalloc(sizeof(struct ka_vmap_pfn_handle), GFP_KERNEL);
    if (mem_attr->vmap_pfn_handle == NULL) {
        trs_vfree(mem_attr->pa_wraper_list);
        mem_attr->pa_wraper_list = NULL;
        trs_err("Alloc vmap_pfn_handle failed.\n");
        return -ENOMEM;
    }

    return 0;
}

void trs_chan_mem_free_attr_lists(struct trs_chan_mem_attr *mem_attr)
{
    if (mem_attr->vmap_pfn_handle != NULL) {
        kfree(mem_attr->vmap_pfn_handle);
        mem_attr->vmap_pfn_handle = NULL;
    }
    if (mem_attr->pa_wraper_list != NULL) {
        trs_vfree(mem_attr->pa_wraper_list);
        mem_attr->pa_wraper_list = NULL;
    }
}

static void *trs_chan_mem_alloc_nids_ddr_mem(struct trs_id_inst *inst, u32 nids[], int node_num, size_t size,
                                             phys_addr_t *paddr)
{
    void *vaddr = NULL;
    int i;

    for (i = 0; i < node_num; i++) {
        vaddr = trs_chan_mem_alloc_ddr(inst, nids[i], size, paddr);
        if (vaddr != NULL) {
            return vaddr;
        }
    }
    return NULL;
}

static ka_page_t **trs_chan_mem_alloc_ddr_pages(struct trs_id_inst *inst, u32 nids[], int node_num, u32 page_num,
                                                struct trs_chan_mem_attr *mem_attr)
{
    ka_page_t **pages = NULL;
    void *vaddr = NULL;
    phys_addr_t paddr;
    u32 i;

    pages = (ka_page_t **)trs_vzalloc(sizeof(ka_page_t *) * page_num);
    if (pages == NULL) {
        trs_err("Alloc pages array failed.\n");
        return NULL;
    }

    for (i = 0; i < page_num; i++) {
        vaddr = trs_chan_mem_alloc_nids_ddr_mem(inst, nids, node_num, KA_MM_PAGE_SIZE, &paddr);
        if (vaddr == NULL) {
            for (; i > 0; i--) {
                trs_chan_mem_free_ddr(inst, ka_mm_page_address(pages[i - 1]), mem_attr->pa_wraper_list[i - 1].size);
            }
            trs_vfree(pages);
            return NULL;
        }
        mem_attr->pa_wraper_list[i].pa = paddr;
        mem_attr->pa_wraper_list[i].size = KA_MM_PAGE_SIZE;
        pages[i] = ka_mm_virt_to_page(vaddr);
    }

    return pages;
}

void *trs_chan_mem_alloc_ddr_phy_pages(struct trs_id_inst *inst, u32 nids[], int node_num, size_t size,
                                       struct trs_chan_mem_attr *mem_attr)
{
    u32 page_num = (size + KA_MM_PAGE_SIZE - 1) / KA_MM_PAGE_SIZE;
    ka_page_t **pages;

    if (trs_chan_mem_alloc_attr_lists(mem_attr, page_num) != 0) {
        return NULL;
    }

    pages = trs_chan_mem_alloc_ddr_pages(inst, nids, node_num, page_num, mem_attr);
    if (pages == NULL) {
        trs_chan_mem_free_attr_lists(mem_attr);
        return NULL;
    }

    mem_attr->pa_num = page_num;
    mem_attr->phy_addr = mem_attr->pa_wraper_list[0].pa;
    mem_attr->vmap_pfn_handle->kva = ka_mm_vmap(pages, page_num, 0, KA_PAGE_KERNEL);
    trs_vfree(pages); /* pages only used for vmap */
    if (mem_attr->vmap_pfn_handle->kva == NULL) {
        u32 i;
        trs_err("Vmap failed for ddr phy pages.\n");
        for (i = 0; i < page_num; i++) {
            trs_chan_mem_free_ddr(inst, ka_mm_phys_to_virt(mem_attr->pa_wraper_list[i].pa),
                                  mem_attr->pa_wraper_list[i].size);
        }
        trs_chan_mem_free_attr_lists(mem_attr);
        return NULL;
    }

    return mem_attr->vmap_pfn_handle->kva;
}

void trs_chan_mem_free_ddr_phy_pages(struct trs_id_inst *inst, struct trs_chan_mem_attr *mem_attr)
{
    u32 i;

    if (mem_attr->vmap_pfn_handle != NULL && mem_attr->vmap_pfn_handle->kva != NULL) {
        ka_mm_vunmap(mem_attr->vmap_pfn_handle->kva);
    }
    for (i = 0; i < mem_attr->pa_num; i++) {
        trs_chan_mem_free_ddr(inst, ka_mm_phys_to_virt(mem_attr->pa_wraper_list[i].pa),
                              mem_attr->pa_wraper_list[i].size);
    }
    trs_chan_mem_free_attr_lists(mem_attr);

    mem_attr->phy_addr = TRS_INVALID_PHYS_ADDR;
    mem_attr->pa_num = 0;
}

void *trs_chan_mem_alloc_rsv(struct trs_id_inst *inst, int type, size_t size, phys_addr_t *paddr, u32 flag)
{
    void *vaddr = NULL;
    int ret;

    vaddr = trs_rsv_mem_alloc(inst, type, size, flag);
    if (vaddr == NULL) {
#ifndef EMU_ST
        trs_debug("No rsv mem. (devid=%u; tsid=%u; type=%d; size=0x%lx)\n", inst->devid, inst->tsid, type, size);
#endif
        return NULL;
    }
    ret = trs_rsv_mem_v2p(inst, type, vaddr, paddr);
    if (ret != 0) {
        trs_rsv_mem_free(inst, type, vaddr, size);
#ifndef EMU_ST
        trs_err("Rcv mem v2p fail. (devid=%u; tsid=%u; type=%d; size=%lu; ret=%d)\n", inst->devid, inst->tsid, type,
                size, ret);
#endif
        return NULL;
    }

    return vaddr;
}

void trs_chan_mem_free_rsv(struct trs_id_inst *inst, int type, void *sq_addr, size_t size)
{
    trs_rsv_mem_free(inst, type, sq_addr, size);
}
