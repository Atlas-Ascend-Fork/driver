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
#include "ka_memory_pub.h"
#include "ka_common_pub.h"
#include "ka_kernel_def_pub.h"

#include "ka_memory_mng.h"
#include "pbl_ka_memory.h"

void *ka_kmalloc(size_t size, ka_gfp_t flags, unsigned int module_id)
{
    void *va = ka_mm_kmalloc(size, flags);
    ka_mem_alloc_stat_add(module_id, size, (unsigned long)(uintptr_t)va);
    return va;
}
KA_EXPORT_SYMBOL_GPL(ka_kmalloc);

void *ka_kzalloc(size_t size, ka_gfp_t flags, unsigned int module_id)
{
    void *va = ka_mm_kzalloc(size, flags);
    ka_mem_alloc_stat_add(module_id, size, (unsigned long)(uintptr_t)va);
    return va;
}
KA_EXPORT_SYMBOL_GPL(ka_kzalloc);

void *ka_kcalloc(unsigned long n, size_t size, ka_gfp_t flags, unsigned int module_id)
{
    void *va = ka_mm_kcalloc(n, size, flags);
    ka_mem_alloc_stat_add(module_id, n * size, (unsigned long)(uintptr_t)va);
    return va;
}
KA_EXPORT_SYMBOL_GPL(ka_kcalloc);

void *ka_kzalloc_node(size_t size, ka_gfp_t flags, unsigned int node, unsigned int module_id)
{
    void *va = ka_mm_kzalloc_node(size, flags, node);
    ka_mem_alloc_stat_add(module_id, size, (unsigned long)(uintptr_t)va);
    return va;
}
KA_EXPORT_SYMBOL_GPL(ka_kzalloc_node);

void ka_kfree(const void *addr, unsigned int module_id)
{
    ka_mem_alloc_stat_del((unsigned long)(uintptr_t)addr, module_id);
    ka_mm_kfree(addr);
}
KA_EXPORT_SYMBOL_GPL(ka_kfree);

void *ka_vzalloc(size_t size, unsigned int module_id)
{
    void *va = ka_mm_vzalloc(size);
    ka_mem_alloc_stat_add(module_id, size, (unsigned long)(uintptr_t)va);
    return va;
}
KA_EXPORT_SYMBOL_GPL(ka_vzalloc);

void *__ka_vmalloc(size_t size, ka_gfp_t gfp_mask, ka_mm_pgprot_t prot, unsigned int module_id)
{
    void *va = ka_vmalloc_ex(size, gfp_mask, prot);
    ka_mem_alloc_stat_add(module_id, size, (unsigned long)(uintptr_t)va);
    return va;
}
KA_EXPORT_SYMBOL_GPL(__ka_vmalloc);

void ka_vfree(const void *addr, unsigned int module_id)
{
    ka_mem_alloc_stat_del((unsigned long)(uintptr_t)addr, module_id);
    ka_mm_vfree(addr);
}
KA_EXPORT_SYMBOL_GPL(ka_vfree);

unsigned long ka_get_free_pages(ka_gfp_t gfp_mask, unsigned int order, unsigned int module_id)
{
    unsigned long va = ka_mm_get_free_pages(gfp_mask, order);
    ka_mem_alloc_stat_add(module_id, KA_MM_PAGE_SIZE << order, va);
    return va;
}
KA_EXPORT_SYMBOL_GPL(ka_get_free_pages);

ka_page_t *ka_alloc_pages(ka_gfp_t gfp_mask, unsigned int order, unsigned int module_id)
{
    void *va = ka_mm_alloc_pages(gfp_mask, order);
    ka_mem_alloc_stat_add(module_id, KA_MM_PAGE_SIZE << order, (unsigned long)(uintptr_t)va);
    return va;
}
KA_EXPORT_SYMBOL_GPL(ka_alloc_pages);

void ka_free_pages(unsigned long addr, unsigned int order, unsigned int module_id)
{
    ka_mem_alloc_stat_del(addr, module_id);
    ka_mm_free_pages(addr, order);
}
KA_EXPORT_SYMBOL_GPL(ka_free_pages);

void *ka_alloc_pages_exact(size_t size, ka_gfp_t gfp_mask, unsigned int module_id)
{
    void *va = ka_mm_alloc_pages_exact(size, gfp_mask);
    ka_mem_alloc_stat_add(module_id, size, (unsigned long)(uintptr_t)va);
    return va;
}
KA_EXPORT_SYMBOL_GPL(ka_alloc_pages_exact);

void ka_free_pages_exact(void *virt, size_t size, unsigned int module_id)
{
    ka_mem_alloc_stat_del((unsigned long)(uintptr_t)virt, module_id);
    ka_mm_free_pages_exact(virt, size);
}
KA_EXPORT_SYMBOL_GPL(ka_free_pages_exact);

ka_page_t *__ka_alloc_pages_node(int nid, ka_gfp_t gfp_mask, unsigned int order)
{
    return ka_mm_alloc_pages_node(nid, gfp_mask, order);
}
KA_EXPORT_SYMBOL_GPL(__ka_alloc_pages_node);

ka_page_t *ka_alloc_pages_node(int nid, ka_gfp_t gfp_mask, unsigned int order, unsigned int module_id)
{
    void *va = __ka_alloc_pages_node(nid, gfp_mask, order);
    ka_mem_alloc_stat_add(module_id, KA_MM_PAGE_SIZE << order, (unsigned long)(uintptr_t)va);
    return va;
}
KA_EXPORT_SYMBOL_GPL(ka_alloc_pages_node);

void __ka_free_pages(ka_page_t *page, unsigned int order, unsigned int module_id)
{
    ka_mem_alloc_stat_del((unsigned long)(uintptr_t)page, module_id);
    __ka_mm_free_pages(page, order);
}
KA_EXPORT_SYMBOL_GPL(__ka_free_pages);

void *ka_kvzalloc(size_t size, ka_gfp_t flags, unsigned int module_id)
{
    void *va = ka_mm_kvzalloc(size, flags);
    ka_mem_alloc_stat_add(module_id, size, (unsigned long)(uintptr_t)va);
    return va;
}
KA_EXPORT_SYMBOL_GPL(ka_kvzalloc);

void *ka_kvmalloc_node(size_t size, ka_gfp_t flags, int node, unsigned int module_id)
{
    void *va = ka_mm_kvmalloc_node(size, flags, node);
    ka_mem_alloc_stat_add(module_id, size, (unsigned long)(uintptr_t)va);
    return va;
}
KA_EXPORT_SYMBOL_GPL(ka_kvmalloc_node);

void *ka_kvmalloc(size_t size, ka_gfp_t flags, unsigned int module_id)
{
    void *va = ka_mm_kvmalloc(size, flags);
    ka_mem_alloc_stat_add(module_id, size, (unsigned long)(uintptr_t)va);
    return va;
}
KA_EXPORT_SYMBOL_GPL(ka_kvmalloc);

void ka_kvfree(const void *addr, unsigned int module_id)
{
    ka_mem_alloc_stat_del((unsigned long)(uintptr_t)addr, module_id);
    ka_mm_kvfree(addr);
}
KA_EXPORT_SYMBOL_GPL(ka_kvfree);

void *ka_dma_alloc_coherent(ka_device_t *dev, size_t size, ka_dma_addr_t *dma_handle, ka_gfp_t gfp,
                            unsigned int module_id)
{
    void *va = ka_mm_dma_alloc_coherent(dev, size, dma_handle, gfp);
    ka_mem_alloc_stat_add(module_id, size, (unsigned long)(uintptr_t)va);
    return va;
}
KA_EXPORT_SYMBOL_GPL(ka_dma_alloc_coherent);

void ka_dma_free_coherent(ka_device_t *dev, size_t size, void *cpu_addr, ka_dma_addr_t dma_handle,
                          unsigned int module_id)
{
    ka_mem_alloc_stat_del((unsigned long)(uintptr_t)cpu_addr, module_id);
    ka_mm_dma_free_coherent(dev, size, cpu_addr, dma_handle);
}
KA_EXPORT_SYMBOL_GPL(ka_dma_free_coherent);
