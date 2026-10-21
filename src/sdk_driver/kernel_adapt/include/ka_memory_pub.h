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

#ifndef KA_MEMORY_PUB_H
#define KA_MEMORY_PUB_H

#include <linux/version.h>
#include <asm/barrier.h>
#include <asm/io.h>
#include <uapi/linux/sysinfo.h>
#include <linux/types.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>
#include <linux/version.h>
#include <linux/nodemask.h>
#include <linux/gfp.h>
#include <linux/pagemap.h>
#include <linux/string.h>
#include <linux/thread_info.h>
#include <linux/iommu.h>
#include <linux/kconfig.h>

#include <linux/mmzone.h>
#include <linux/rwsem.h>
#include <linux/mm_types.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0))
#include <linux/sched/mm.h>
#else
#include <linux/sched.h>
#endif
#include <linux/memcontrol.h>
#include <asm/pgalloc.h>
#include <asm/pgtable.h>
#include <linux/page-flags.h>
#include <linux/mmu_notifier.h>
#include <linux/hugetlb.h>

#include <conftest.h>
#include "ka_common_pub.h"
#include "ka_base_pub.h"

#define __KA_GFP_DMA __GFP_DMA
#define __KA_GFP_HIGHMEM __GFP_HIGHMEM
#define __KA_GFP_DMA32 __GFP_DMA32
#define __KA_GFP_MOVABLE __GFP_MOVABLE
#define __KA_GFP_RECLAIMABLE __GFP_RECLAIMABLE
#define __KA_GFP_HIGH __GFP_HIGH
#define __KA_GFP_IO __GFP_IO
#define __KA_GFP_FS __GFP_FS
#define __KA_GFP_WRITE __GFP_WRITE
#define __KA_GFP_NOWARN __GFP_NOWARN
#define __KA_GFP_NOFAIL __GFP_NOFAIL
#define __KA_GFP_NORETRY __GFP_NORETRY
#define __KA_GFP_MEMALLOC __GFP_MEMALLOC
#define __KA_GFP_COMP __GFP_COMP
#define __KA_GFP_ZERO __GFP_ZERO
#define __KA_GFP_NOMEMALLOC __GFP_NOMEMALLOC
#define __KA_GFP_HARDWALL __GFP_HARDWALL
#define __KA_GFP_THISNODE __GFP_THISNODE
#define __KA_GFP_RECLAIM __GFP_RECLAIM

#ifndef __GFP_ACCOUNT
#ifdef __GFP_KMEMCG
#define __GFP_ACCOUNT __GFP_KMEMCG /* for linux version 3.10 */
#endif
#ifdef __GFP_NOACCOUNT
#define __GFP_ACCOUNT 0 /* for linux version 4.1 */
#endif
#endif
#define __KA_GFP_ACCOUNT __GFP_ACCOUNT

#define _KA_PAGE_RW _PAGE_RW
#define KA_PTE_WRITE PTE_WRITE
#define KA_PTE_RDONLY PTE_RDONLY

/* Useful GFP flag combinations */
#define KA_GFP_ATOMIC GFP_ATOMIC
#define KA_GFP_KERNEL GFP_KERNEL
#define KA_GFP_NOWAIT GFP_NOWAIT
#define KA_GFP_NOIO GFP_NOIO
#define KA_GFP_NOFS GFP_NOFS
#define KA_GFP_USER GFP_USER
#define KA_GFP_DMA GFP_DMA
#define KA_GFP_DMA32 GFP_DMA32
#define KA_GFP_HIGHUSER GFP_HIGHUSER
#define KA_GFP_HIGHUSER_MOVABLE GFP_HIGHUSER_MOVABLE
#define KA_GFP_TRANSHUGE_LIGHT GFP_TRANSHUGE_LIGHT
#define KA_GFP_TRANSHUGE GFP_TRANSHUGE
#define KA_GFP_KERNEL_ACCOUNT GFP_KERNEL_ACCOUNT

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0))
#define __KA_GFP_RETRY_MAYFAIL __GFP_RETRY_MAYFAIL
#else
#define __KA_GFP_RETRY_MAYFAIL __GFP_REPEAT
#endif

#define __KA_GFP_DIRECT_RECLAIM __ka_get_mem_gfp_direct_reclaim()
#define __KA_GFP_KSWAPD_RECLAIM __ka_get_mem_gfp_kswapd_reclaim()
#define __KA_GFP_NOLOCKDEP __ka_get_mem_gfp_nolockdep()
#define __KA_GFP_KMEMCG __ka_get_mem_gfp_kmemcg()
#define __KA_GFP_NOACCOUNT __ka_get_mem_gfp_noaccount()

#ifndef HUGETLB_ALLOC_NONE
#define HUGETLB_ALLOC_NONE 0x00
#endif
#ifndef HUGETLB_ALLOC_NORMAL
#define HUGETLB_ALLOC_NORMAL 0x01
#endif
#ifndef HUGETLB_ALLOC_BUDDY
#define HUGETLB_ALLOC_BUDDY 0x02
#endif
#ifndef HUGETLB_ALLOC_NORECLAIM
#define HUGETLB_ALLOC_NORECLAIM 0x04
#endif
#ifndef HUGETLB_ALLOC_GIANT
#define HUGETLB_ALLOC_GIANT 0x08
#endif
#define KA_HUGETLB_ALLOC_NONE HUGETLB_ALLOC_NONE
#define KA_HUGETLB_ALLOC_NORMAL HUGETLB_ALLOC_NORMAL
#define KA_HUGETLB_ALLOC_BUDDY HUGETLB_ALLOC_BUDDY
#define KA_HUGETLB_ALLOC_NORECLAIM HUGETLB_ALLOC_NORECLAIM
#define KA_HUGETLB_ALLOC_GIANT HUGETLB_ALLOC_GIANT

#define __ka_mm_iomem __iomem

#define KA_MM_PAGE_SIZE PAGE_SIZE
#define KA_MM_PAGE_MASK PAGE_MASK
#define KA_MM_HPAGE_SHIFT HPAGE_SHIFT
#define KA_MM_PAGE_ALIGN(addr) PAGE_ALIGN(addr)
#define KA_MM_IS_ALIGNED(x, a) IS_ALIGNED(x, a)
#define KA_MM_PAGE_ALIGNED(addr) PAGE_ALIGNED(addr)

#define KA_PROT_READ PROT_READ   /* page can be read */
#define KA_PROT_WRITE PROT_WRITE /* page can be written */
#define KA_FOLL_WRITE FOLL_WRITE
#define KA_FOLL_LONGTERM FOLL_LONGTERM

#define KA_VM_PFNMAP VM_PFNMAP
#define KA_VM_LOCKED VM_LOCKED
#define KA_VM_WRITE VM_WRITE
#define KA_VM_MAYWRITE VM_MAYWRITE

#define KA_VM_IO VM_IO
#define KA_VM_SHARED VM_SHARED
#define KA_VM_DONTCOPY VM_DONTCOPY
#define KA_VM_DONTEXPAND VM_DONTEXPAND
#define KA_VM_DONTDUMP VM_DONTDUMP

#define KA_GPAGE_SHIFT PUD_SHIFT
#define KA_GPAGE_SIZE (1ULL << KA_GPAGE_SHIFT)
#define KA_HPAGE_SIZE HPAGE_SIZE
#define KA_VM_HUGETLB VM_HUGETLB
#define KA_VM_FAULT_SIGBUS VM_FAULT_SIGBUS
#define KA_VM_FAULT_SIGSEGV VM_FAULT_SIGSEGV

#define KA_MIGRATE_MOVABLE MIGRATE_MOVABLE
#define KA_IOMMU_READ IOMMU_READ
#define KA_IOMMU_WRITE IOMMU_WRITE
#define KA_IOMMU_CACHE IOMMU_CACHE

/*
 * linux kernel < 3.11 not defined VM_FAULT_SIGSEGV,
 * euler LINUX_VERSION_CODE 3.10 defined VM_FAULT_SIGSEGV
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)
#define KA_MM_FAULT_ERROR VM_FAULT_SIGSEGV
#else
#ifdef VM_FAULT_SIGSEGV
#define KA_MM_FAULT_ERROR VM_FAULT_SIGSEGV
#else
#define KA_MM_FAULT_ERROR VM_FAULT_SIGBUS
#endif
#endif

#define KA_MM_GIANT_PAGE_SIZE 0x40000000ULL

#define KA_MAP_FIXED MAP_FIXED

#define __ka_pgprot(x) __pgprot(x)
#define ka_pgprot_t pgprot_t
#define __ka_pgprot(x) __pgprot(x)
#define ka_pgprot_val(x) pgprot_val(x)

#define KA_PAGE_NONE PAGE_NONE
#define KA_PAGE_SHARED PAGE_SHARED
#define KA_PAGE_SHARED_EXEC PAGE_SHARED_EXEC
#define KA_PAGE_READONLY PAGE_READONLY
#define KA_PAGE_READONLY_EXEC PAGE_READONLY_EXEC
#define KA_PAGE_KERNEL PAGE_KERNEL

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0)
#define ka_mm_pgprot_device(prot) pgprot_device(prot)
#else
#define ka_mm_pgprot_device(prot) pgprot_noncached(prot)
#endif
#define ka_mm_pgprot_writecombine(prot) pgprot_writecombine(prot)
#define ka_mm_pgprot_noncached(prot) pgprot_noncached(prot)

#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 16, 0)
typedef int ka_vm_fault_t;
#else
#define ka_vm_fault_t vm_fault_t
#endif

#define ka_pmd_t pmd_t
#define ka_pgd_t pgd_t
#define ka_pte_t pte_t
#define ka_pud_t pud_t
#define ka_pgtable_t pgtable_t
#define ka_nodemask_t nodemask_t
#define ka_mm_nodes_clear(dst) nodes_clear(dst)
#define ka_mm_node_set(node, dst) node_set(node, dst)
typedef struct dma_pool ka_dma_pool_t;
typedef struct kmem_cache ka_kmem_cache_t;
typedef struct mm_struct ka_mm_struct_t;
typedef struct address_space ka_address_space_t;
typedef struct vm_fault ka_vm_fault_struct_t;
typedef struct mempolicy ka_mempolicy_t;
typedef struct mmu_notifier ka_mmu_notifier_t;
typedef struct mmu_notifier_ops ka_mmu_notifier_ops_t;
typedef struct vm_operations_struct ka_vm_operations_struct_t;
typedef struct vm_area_struct ka_vm_area_struct_t;
typedef struct mmu_notifier_range ka_mmu_notifier_range_t;
typedef enum dma_data_direction ka_dma_data_direction_t;
typedef struct iommu_sva ka_iommu_sva_t;
typedef struct iommu_group ka_iommu_group_t;
typedef struct iommu_resv_region ka_iommu_resv_region_t;

static inline unsigned long ka_mm_get_vm_fault_address(ka_vm_fault_struct_t *vmf)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
    return vmf->address;
#else
    return 0;
#endif
}

#define PXD_JUDGE(pxd) (((pxd) == NULL) || (pxd##_none(*(pxd##_t *)(pxd)) != 0) || (pxd##_bad(*(pxd##_t *)(pxd)) != 0))
#define PMD_JUDGE(pmd) \
    (((pmd) == NULL) || (ka_mm_pmd_none(*(ka_pmd_t *)(pmd)) != 0) || (ka_mm_pmd_bad(*(ka_pmd_t *)(pmd)) != 0))

#if defined(__arm__) || defined(__aarch64__)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0)
#define PMD_HUGE(pmd)                                               \
    (((pmd) != NULL) && (ka_mm_pmd_val(*(ka_pmd_t *)(pmd)) != 0) && \
     ((ka_mm_pmd_val(*(ka_pmd_t *)(pmd)) & PMD_TABLE_BIT) == 0))
#define PUD_GIANT(pud) \
    (((pud) != NULL) && (pud_val(*(ka_pud_t *)(pud)) != 0) && ((pud_val(*(ka_pud_t *)(pud)) & PUD_TABLE_BIT) == 0))
#else
#define PMD_HUGE(pmd) \
    (((pmd) != NULL) && (ka_mm_pmd_none(*(ka_pmd_t *)(pmd)) == 0) && (ka_mm_pte_huge(*(ka_pte_t *)(pmd)) != 0))
#define PUD_GIANT(pud) \
    (((pud) != NULL) && (ka_mm_pud_none(*(ka_pud_t *)(pud)) == 0) && (ka_mm_pte_huge(*(ka_pte_t *)(pud)) != 0))
#endif
#else
#define PMD_HUGE(pmd) 0
#define PUD_GIANT(pud) 0
#endif

#define ka_mm_mmu_notifier_range_init(range, event, flags, vma, mm, start, end) \
    mmu_notifier_range_init(range, event, flags, vma, mm, start, end)
#define ka_mm_mmu_notifier_invalidate_range_start(range) mmu_notifier_invalidate_range_start(range)
#define ka_mm_mmu_notifier_invalidate_range_end(range) mmu_notifier_invalidate_range_end(range)
#define ka_mm_mmu_notifier_get_locked(ops, mm) mmu_notifier_get_locked(ops, mm)

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
#define ka_mm_mmu_notifier_get(ops, mm) mmu_notifier_get(ops, mm)
#define ka_mm_mmu_notifier_put(mm) mmu_notifier_put(mm)
#else
/* mmu_notifier_get no supported */
static inline ka_mmu_notifier_t *ka_mm_mmu_notifier_get(ka_mmu_notifier_ops_t *ops, ka_mm_struct_t *mm)
{
    (void)ops;
    (void)mm;
    return NULL;
}
/* mmu_notifier_put no supported */
#define ka_mm_mmu_notifier_put(mm) ((void)mm)
#endif

#define ka_mm_mmu_notifier_register(subscription, mm) mmu_notifier_register(subscription, mm)

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 2, 0))
#define ka_mm_mmu_notifier_range_blockable(range) mmu_notifier_range_blockable(range)
#else
#define ka_mm_mmu_notifier_range_blockable(range) (range->blockable)
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
#define ka_mm_mmu_notifier_unregister_no_release(mn, mm) mmu_notifier_unregister_no_release(mn, mm)
#else
static inline void ka_mm_mmu_notifier_unregister_no_release(ka_mmu_notifier_t *mn, ka_mm_struct_t *mm)
{
    (void)mm;
    if (mn != NULL) {
        mmu_notifier_put(mn);
    }
}
#endif
#define ka_mm_mmu_notifier_unregister(mn, mm) mmu_notifier_unregister(mn, mm)
#define __ka_mm_mmu_notifier_register(mn, mm) __mmu_notifier_register(mn, mm)

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#define KA_MM_MMU_NOTIFIER_OPS_INIT_ALLOC_NOTIFIER(alloc_notifier_fn) .alloc_notifier = alloc_notifier_fn,
#define KA_MM_MMU_NOTIFIER_OPS_INIT_FREE_NOTIFIER(free_notifier_fn) .free_notifier = free_notifier_fn,
#else
#define KA_MM_MMU_NOTIFIER_OPS_INIT_ALLOC_NOTIFIER(alloc_notifier_fn)
#define KA_MM_MMU_NOTIFIER_OPS_INIT_FREE_NOTIFIER(free_notifier_fn)
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0))
#define KA_DEFINE_MMU_NOTIFIER_INVALIDATE_RANGE_START_FN(func_name)                        \
    static int ka_##func_name(ka_mmu_notifier_t *mn, const ka_mmu_notifier_range_t *range) \
    {                                                                                      \
        bool blockable = ka_mm_mmu_notifier_range_blockable(range);                        \
                                                                                           \
        return func_name(mn, range->mm, range->start, range->end, blockable);              \
    }
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0)
#define KA_DEFINE_MMU_NOTIFIER_INVALIDATE_RANGE_START_FN(func_name)                                              \
    static int ka_##func_name(ka_mmu_notifier_t *mn, ka_mm_struct_t *mm, unsigned long start, unsigned long end, \
                              bool blockable)                                                                    \
    {                                                                                                            \
        return func_name(mn, mm, start, end, blockable);                                                         \
    }
#else
#define KA_DEFINE_MMU_NOTIFIER_INVALIDATE_RANGE_START_FN(func_name)                                               \
    static void ka_##func_name(ka_mmu_notifier_t *mn, ka_mm_struct_t *mm, unsigned long start, unsigned long end) \
    {                                                                                                             \
        (void)func_name(mn, mm, start, end, true);                                                                \
    }
#endif

#define KA_MM_MMU_NOTIFIER_OPS_INIT_INVALIDATE_RANGE_START(range_start_fn) \
    .invalidate_range_start = ka_##range_start_fn,

typedef struct ka_sysinfo {
    unsigned long totalram;
    unsigned long freeram;
    unsigned long sharedram;
} ka_sysinfo_t;

#define KA_DMA_ATTR_SKIP_CPU_SYNC DMA_ATTR_SKIP_CPU_SYNC
#define KA_DMA_ATTR_WRITE_COMBINE DMA_ATTR_WRITE_COMBINE
#define KA_DMA_ATTR_NO_KERNEL_MAPPING DMA_ATTR_NO_KERNEL_MAPPING

#define ka_dma_addr_t dma_addr_t
#define KA_DMA_BIDIRECTIONAL DMA_BIDIRECTIONAL
#define KA_DMA_TO_DEVICE DMA_TO_DEVICE
#define KA_DMA_FROM_DEVICE DMA_FROM_DEVICE
#define KA_DMA_NONE DMA_NONE
#define KA_DMA_BIT_MASK DMA_BIT_MASK

#ifndef DMA_MAPPING_ERROR
#define KA_DMA_MAPPING_ERROR (~(ka_dma_addr_t)0)
#else
#define KA_DMA_MAPPING_ERROR DMA_MAPPING_ERROR
#endif
#define ka_mm_pgprot_t pgprot_t

#define KA_MM_PAGE_SHIFT PAGE_SHIFT
#define KA_MM_PFN_ALIGN(x) PFN_ALIGN(x)
#define KA_MM_PFN_UP(x) PFN_UP(x)
#define KA_MM_PFN_DOWN(x) PFN_DOWN(x)
#define KA_MM_PHYS_PFN(x) PHYS_PFN(x)
#define KA_MM_PFN_PHYS(x) PFN_PHYS(x)

#define KA_MODULE_TYPE_BIT 16U
#define ka_mm_get_module_id(module_type, sub_module_type) ((module_type << KA_MODULE_TYPE_BIT) | sub_module_type)

#define ka_mm_kmalloc(size, flags) kmalloc(size, flags | __KA_GFP_ACCOUNT)
#define ka_mm_kzalloc(size, flags) kzalloc(size, flags | __KA_GFP_ACCOUNT)
#define ka_mm_kcalloc(n, size, flags) kcalloc(n, size, flags | __KA_GFP_ACCOUNT)
#define ka_mm_kzalloc_node(size, flags, node) kzalloc_node(size, flags | __KA_GFP_ACCOUNT, node)
#define ka_mm_kfree(addr) kfree(addr)

#define ka_mm_vmalloc(size) vmalloc(size)
#define ka_mm_vzalloc(size) vzalloc(size)

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
#define __ka_mm_vmalloc(size, gfp_mask, prot) __vmalloc(size, gfp_mask | __KA_GFP_ACCOUNT)
#else
#define __ka_mm_vmalloc(size, gfp_mask, prot) __vmalloc(size, gfp_mask | __KA_GFP_ACCOUNT, prot)
#endif

#define ka_mm_vfree(addr) vfree(addr)
#define ka_mm_get_free_pages(gfp_mask, order) __get_free_pages(gfp_mask | __KA_GFP_ACCOUNT, order)
#define ka_mm_alloc_pages(gfp_mask, order) alloc_pages(gfp_mask | __KA_GFP_ACCOUNT, order)
#define ka_mm_free_pages(addr, order) free_pages(addr, order)
#define __ka_mm_free_page(page) __free_page(page)
#define ka_mm_alloc_pages_exact(size, gfp_mask) alloc_pages_exact(size, gfp_mask | __KA_GFP_ACCOUNT)
#define ka_mm_free_pages_exact(virt, size) free_pages_exact(virt, size)
#define ka_mm_alloc_pages_node(nid, gfp_mask, order) alloc_pages_node(nid, gfp_mask | __KA_GFP_ACCOUNT, order)
#define __ka_mm_free_pages(page, order) __free_pages(page, order)

#define ka_mm_kvmalloc_node(size, gfp_flags, node) kvmalloc_node(size, gfp_flags | __KA_GFP_ACCOUNT, node)
#define ka_mm_kvzalloc(size, gfp_flags) kvzalloc(size, gfp_flags | __KA_GFP_ACCOUNT)
#define ka_mm_kvmalloc(size, gfp_flags) kvmalloc(size, gfp_flags | __KA_GFP_ACCOUNT)
#define ka_mm_kvfree(addr) kvfree(addr)

#define ka_mm_page_count(page) page_count(page)
#define ka_mm_get_page(page) get_page(page)
#define ka_mm_vmalloc_node(size, node) vmalloc_node((size), node)
#define ka_mm_vzalloc_node(size, node) vzalloc_node(size, node)
#define ka_mm_vmalloc_32(size) vmalloc_32(size)
#define ka_mm_vmalloc_32_user(size) vmalloc_32_user(size)

#define ka_mm_dma_alloc_coherent(dev, size, dma_handle, gfp) \
    dma_alloc_coherent(dev, size, dma_handle, gfp | __KA_GFP_ACCOUNT)
#define ka_mm_dma_free_coherent(dev, size, cpu_addr, dma_handle) dma_free_coherent(dev, size, cpu_addr, dma_handle)
#define ka_mm_put_page(page) put_page(page)
#define ka_mm_dma_map_page(d, p, o, s, r) dma_map_page(d, p, o, s, r)
#define ka_mm_dma_unmap_page(d, a, s, r) dma_unmap_page(d, a, s, r)

#ifdef RHEL_RELEASE_CODE
#if RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(8, 0)
#define ka_mm_dma_map_page_attrs(d, p, o, s, r, a) dma_map_page_attrs(d, p, o, s, r, a)
#define ka_mm_dma_unmap_page_attrs(d, a, s, r, t) dma_unmap_page_attrs(d, a, s, r, t)
#define ka_mm_dma_map_resource(dev, phys_addr, size, dir, attrs) dma_map_resource(dev, phys_addr, size, dir, attrs)
#define ka_mm_dma_unmap_resource(dev, addr, size, dir, attrs) dma_unmap_resource(dev, addr, size, dir, attrs)
#elif RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7, 5)
#define ka_mm_dma_map_page_attrs(d, p, o, s, r, a) dma_map_page_attrs(d, p, o, s, r, (struct dma_attrs *)a)
#define ka_mm_dma_unmap_page_attrs(d, p, o, s, r) dma_unmap_page_attrs(d, p, o, s, (struct dma_attrs *)r)
#define ka_mm_dma_map_resource(dev, phys_addr, size, dir, attrs) \
    dma_map_resource(dev, phys_addr, size, dir, (struct dma_attrs *)attrs)
#define ka_mm_dma_unmap_resource(dev, addr, size, dir, attrs) \
    dma_unmap_resource(dev, addr, size, dir, (struct dma_attrs *)attrs)
#else // RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7, 5)
#define ka_mm_dma_map_page_attrs(d, p, o, s, r, a) 0
#define ka_mm_dma_unmap_page_attrs(d, p, o, s, r) (~(ka_dma_addr_t)0)
#define ka_mm_dma_map_resource(dev, phys_addr, size, dir, attrs) (~(ka_dma_addr_t)0)
#define ka_mm_dma_unmap_resource(dev, addr, size, dir, attrs)
#endif // RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(8, 0)
#else  // RHEL_RELEASE_CODE
#define ka_mm_dma_map_page_attrs(d, p, o, s, r, a) dma_map_page_attrs(d, p, o, s, r, a)
#define ka_mm_dma_unmap_page_attrs(d, a, s, r, t) dma_unmap_page_attrs(d, a, s, r, t)
#define ka_mm_dma_map_resource(dev, phys_addr, size, dir, attrs) dma_map_resource(dev, phys_addr, size, dir, attrs)
#define ka_mm_dma_unmap_resource(dev, addr, size, dir, attrs) dma_unmap_resource(dev, addr, size, dir, attrs)
#endif // RHEL_RELEASE_CODE

#define ka_mm_sg_dma_len(sg) sg_dma_len(sg)
#define ka_mm_sg_dma_address(sg) sg_dma_address(sg)
#define ka_dma_unmap_sg(d, s, n, r) dma_unmap_sg(d, s, n, r)
#define ka_dma_map_sg(d, s, n, r) dma_map_sg(d, s, n, r)

#define ka_mm_kmem_cache_create(name, size, align, flags, ctor) kmem_cache_create(name, size, align, flags, ctor)
#define ka_mm_kmem_cache_destroy(cache) kmem_cache_destroy(cache)
#define ka_mm_kmem_cache_alloc(ka_cache, gfp) kmem_cache_alloc(ka_cache, gfp | __KA_GFP_ACCOUNT)
#define ka_mm_kmem_cache_free(cache, x) kmem_cache_free(cache, x)
#define ka_mm_kmem_cache_alloc_node(s, gfpflags, node) kmem_cache_alloc_node(s, gfpflags | __KA_GFP_ACCOUNT, node)

#define ka_mm_lock_page(page) lock_page(page)
#define ka_mm_unlock_page(page) unlock_page(page)
#define ka_mm_follow_pfn(ka_vma, address, pfn) follow_pfn(ka_vma, address, pfn)

#define ka_mm_vmalloc_to_page(vmalloc_addr) vmalloc_to_page(vmalloc_addr)
#define ka_mm_vmalloc_to_pfn(vmalloc_addr) vmalloc_to_pfn(vmalloc_addr)
#define ka_mm_find_vma(mm, addr) find_vma(mm, addr)
#define ka_mm_io_remap_pfn_range(vma, addr, pfn, size, prot) io_remap_pfn_range(vma, addr, pfn, size, prot)
#define ka_mm_remap_pfn_range(vma, addr, pfn, size, ka_prot) remap_pfn_range(vma, addr, pfn, size, ka_prot)
#define ka_mm_vm_munmap(addr, len) vm_munmap(addr, len)
#define ka_mm_remap_vmalloc_range(vma, addr, pgoff) remap_vmalloc_range(vma, addr, pgoff)
#define ka_mm_can_do_mlock(void) can_do_mlock(void)
#define ka_mm_kstrdup(s, gfp_flag) kstrdup(s, gfp_flag | __KA_GFP_ACCOUNT)
#define ka_mm_kstrdup_const(s, gfp_flag) kstrdup_const(s, gfp_flag | __KA_GFP_ACCOUNT)
#define ka_mm_kfree_const(addr) kfree_const(addr)
#define ka_mm_kmemdup(src, len, gfp_flag) kmemdup(src, len, gfp_flag | __KA_GFP_ACCOUNT)
#define ka_mm_memdup_user(src, len) memdup_user(src, len)
#define ka_mm_page_mapped(page) page_mapped(page)
#define ka_mm_set_page_dirty(page) set_page_dirty(page)
#define ka_mm_set_page_dirty_lock(page) set_page_dirty_lock(page)
#define ka_mm_unmap_mapping_range(mapping, holebegin, holelen, even_cows) \
    unmap_mapping_range(mapping, holebegin, holelen, even_cows)
#define ka_mm_memcpy_fromio(to, from, count) memcpy_fromio(to, from, count)
#define ka_mm_memcpy_toio(to, from, count) memcpy_toio(to, from, count)
#define ka_mm_memset_io(dst, c, count) memset_io(dst, c, count)
#define ka_mm_ioremap(addr, size) ioremap(addr, size)
#define ka_mm_ioremap_nocache(addr, size) ioremap_nocache(addr, size)
#define ka_mm_pci_iomap(dev, bar, maxlen) pci_iomap(dev, bar, maxlen)
#define ka_mm_pci_iounmap(dev, p) pci_iounmap(dev, p)

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
#define ioremap_nocache ioremap
#endif

#define ka_mm_ioremap_wc(addr, size) ioremap_wc(addr, size)
#define ka_mm_ioremap_cache(phys_addr, size) ioremap_cache(phys_addr, size)
#define ka_mm_iounmap(va) iounmap(va)
#define ka_mm_dma_set_coherent_mask(dev, mask) dma_set_coherent_mask(dev, mask)
#define ka_mm_dma_set_mask(dev, mask) dma_set_mask(dev, mask)
#define ka_mm_dma_map_single(dev, ptr, size, direction) dma_map_single(dev, ptr, size, direction)
#define ka_mm_dma_mapping_error(dev, addr) dma_mapping_error(dev, addr)
#define ka_mm_dma_unmap_single(dev, dma_addr, size, direction) dma_unmap_single(dev, dma_addr, size, direction)
#define ka_mm_dma_sync_single_for_device(dev, addr, size, dir) dma_sync_single_for_device(dev, addr, size, dir)
#define ka_mm_dma_sync_single_for_cpu(dev, addr, size, dir) dma_sync_single_for_cpu(dev, addr, size, dir)
#define ka_mm_writel(b, addr) writel(b, addr)
#define ka_mm_readl(addr) readl(addr)
#define ka_mm_readl_relaxed(addr) readl_relaxed(addr)
#define ka_mm_writeq(b, addr) writeq(b, addr)
#define ka_mm_writel(b, addr) writel(b, addr)
#define ka_mm_writew(b, addr) writew(b, addr)
#define ka_mm_writeb(b, addr) writeb(b, addr)
#define ka_mm_readq(addr) readq(addr)
#define ka_mm_readl(addr) readl(addr)
#define ka_mm_readw(addr) readw(addr)
#define ka_mm_readb(addr) readb(addr)
#define ka_mm_writel_relaxed(value, addr) writel_relaxed(value, addr)
#define ka_mm_writeq_relaxed(value, addr) writeq_relaxed(value, addr)

#define ka_mm_page_align(addr) page_align(addr)
#define ka_mm_page_to_pfn(page) page_to_pfn(page)
#define ka_mm_pfn_to_page(pfn) pfn_to_page(pfn)
#define ka_mm_pfn_to_online_page(pfn) pfn_to_online_page(pfn)
#define ka_mm_page_to_phys(page) page_to_phys(page)
#define ka_mm_virt_to_phys(address) virt_to_phys(address)
#define ka_mm_phys_to_pfn(addr) phys_to_pfn(addr)
#define ka_mm_is_power_of_2(pg_num) is_power_of_2(pg_num)
#define ka_mm_phys_to_virt(addr) phys_to_virt(addr)
#define ka_mm_pfn_valid(pfn) pfn_valid(pfn)
#define ka_mm_is_power_of_2(pg_num) is_power_of_2(pg_num)
#define ka_mm_split_page(page, order) split_page(page, order)
#define ka_mm_nth_page(addr, n) nth_page(addr, n)
#define ka_mm_get_order(size) get_order(size)
#define ka_mm_free_page(page) free_page(page)
#define ka_mm_mmput(mm) mmput(mm)
#define ka_mm_mmgrab(mm) mmgrab(mm)
#define ka_mm_mmdrop(mm) mmdrop(mm)
#define ka_mm_is_vmalloc_addr(addr) is_vmalloc_addr(addr)
#define ka_mm_page_address(page) page_address(page)
#define ka_mm_pfn_to_nid(pfn) pfn_to_nid(pfn)
#define __ka_mm_phys_to_pfn(paddr) __phys_to_pfn(paddr)
#define __ka_mm_pfn_to_phys(pfn) __pfn_to_phys(pfn)
#define ka_mm_page_is_ram(pfn) page_is_ram(pfn)
#define ka_mm_next_online_node(nid) next_online_node(nid)
#define ka_mm_num_online_nodes() num_online_nodes()
#define ka_mm_first_online_node first_online_node
#define ka_mm_iommu_map(domain, iova, paddr, size, prot, gfp) iommu_map(domain, iova, paddr, size, prot, gfp)
#define ka_mm_iommu_unmap(domain, iova, size) iommu_unmap(domain, iova, size)
#define ka_mm_iommu_get_domain_for_dev(dev) iommu_get_domain_for_dev(dev)
#define ka_mm_iommu_domain_alloc(bus) iommu_domain_alloc(bus)
#define ka_mm_iommu_group_get(dev) iommu_group_get(dev)
#define ka_mm_iommu_group_put(group) iommu_group_put(group)
#define ka_mm_iommu_detach_group(domain, group) iommu_detach_group(domain, group)
#define ka_mm_iommu_domain_free(domain) iommu_domain_free(domain)
#define ka_mm_put_iova_domain(iovad) put_iova_domain(iovad)
#define ka_mm_iommu_get_group_resv_regions(group, head) iommu_get_group_resv_regions(group, head)
#define ka_mm_iommu_attach_group(domain, group) iommu_attach_group(domain, group)
#define ka_mm_iommu_get_msi_cookie(domain, base) iommu_get_msi_cookie(domain, base)
#define ka_mm_mmget_not_zero(mm) mmget_not_zero(mm)

#define ka_mm_flush_tlb_range(vma, start, end) flush_tlb_range(vma, start, end)

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 17, 0)
#define ka_mm_PagePoisoned(page) PagePoisoned(page)
#else
static inline int ka_mm_PagePoisoned(ka_page_t *page)
{
    return 0;
}
#endif

#define ka_mm_PageHWPoison(page) PageHWPoison(page)
#define ka_mm_PageHuge(page) PageHuge(page)
#define ka_mm_PageCompound(page) PageCompound(page)
#define ka_mm_compound_head(page) compound_head(page)
#define ka_mm_dec_nr_ptes(mm) mm_dec_nr_ptes(mm)
#define ka_mm_compound_order(page) compound_order(page)
#define ka_mm_page_to_nid(page) page_to_nid(page)

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
#define ka_mm_page_size(page) page_size(page)
#else
#define ka_mm_page_size(page) (PAGE_SIZE << compound_order(page))
#endif

/*
 * ka_vmap_pfn_handle: opaque handle for physical memory mapping.
 * - kva: contiguous kernel virtual address (valid when PFNs contiguous, or >= 5.10 vmap_pfn)
 * - kva_list/size_list/count: per-page ioremap'd addresses for non-contiguous PFNs (< 5.10)
 *
 * For DDR phy pages mapped via ka_mm_vmap, only kva is populated (kva_list/size_list/count remain NULL/0).
 * For physical memory, created via ka_mm_vmap_pfn_create, all fields populated as needed.
 */
struct ka_vmap_pfn_handle {
    void *kva; /* Contiguous kva (contiguous PFNs, >= 5.10, or ka_mm_vmap) */
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0) || !(IS_ENABLED(CONFIG_VMAP_PFN))
    void **kva_list;          /* Per-page ioremap'd addresses (non-contiguous < 5.10) */
    unsigned long *size_list; /* Per-page sizes in bytes (non-contiguous < 5.10) */
    unsigned int count;       /* Number of pages in kva_list (non-contiguous < 5.10) */
#endif
};

struct ka_vmap_pfn_handle *ka_mm_vmap_pfn_create(unsigned long *pfns, unsigned int count, pgprot_t prot);
void ka_mm_vmap_pfn_destroy(struct ka_vmap_pfn_handle *handle);
void *ka_mm_vmap_pfn_kva(struct ka_vmap_pfn_handle *handle);
void ka_mm_vmap_pfn_memcpy_toio(struct ka_vmap_pfn_handle *handle, unsigned long offset, const void *src, size_t len);
void ka_mm_vmap_pfn_memcpy_fromio(struct ka_vmap_pfn_handle *handle, unsigned long offset, void *dst, size_t len);

#define ka_mm_vmap(pages, count, flags, prot) vmap(pages, count, flags, prot)
#define ka_mm_vunmap(addr) vunmap(addr)
#ifdef FUNC___PTE_MAP_PRESENT
#define __ka_mm_pte_map(pmd, addr) __pte_map(pmd, addr)
#endif
#ifdef FUNC_PTE_LOCKPTR_PRESENT
#define ka_mm_pte_lockptr(mm, pmd) pte_lockptr(mm, pmd)
#endif
#define ka_mm_pte_offset_map(dir, address) pte_offset_map(dir, address)
#define ka_mm_pte_offset_map_lock(mm, pmd, address, ptlp) pte_offset_map_lock(mm, pmd, address, ptlp)
#define ka_mm_pte_unmap(pte) pte_unmap(pte)
#define ka_mm_pte_unmap_unlock(pte, ptl) pte_unmap_unlock(pte, ptl)
#define ka_mm_pte_none(pte) pte_none(pte)
#define ka_mm_pte_present(pte) pte_present(pte)
#define ka_mm_pte_pfn(pte) pte_pfn(pte)
#define ka_mm_pte_huge(pte) pte_huge(pte)
#define ka_mm_pmd_none(pmd) pmd_none(pmd)
#define ka_mm_pmd_same(pmd_a, pmd_b) pmd_same(pmd_a, pmd_b)
#define ka_mm_pmd_trans_huge(pmd) pmd_trans_huge(pmd)
#define ka_mm_pmd_devmap(pmd) pmd_devmap(pmd)
#define ka_mm_pmd_ERROR(pmd) pmd_ERROR(pmd)
#define ka_mm_pmd_clear(pmd) pmd_clear(pmd)
#define ka_mm_pmd_offset(pud, va) pmd_offset(pud, va)
#define ka_mm_pmd_addr_end(va, end) pmd_addr_end(va, end)
#define ka_mm_pmd_val(x) pmd_val(x)
#define ka_mm_pmdp_get_lockless(pmdp) pmdp_get_lockless(pmdp)
#define ka_mm_pte_free(mm, pte) pte_free(mm, pte)
#define ka_mm_pmd_huge(pmd) pmd_huge(pmd)
#define ka_mm_pmd_free(mm, pmd) pmd_free(mm, pmd)
#define ka_mm_pmd_pgtable(pmd) pmd_pgtable(pmd)
#define ka_mm_is_pmd_migration_entry(pmd) is_pmd_migration_entry(pmd)
#define ka_mm_pmd_bad(pmd) pmd_bad(pmd)
#define ka_mm_pgd_offset(mm, address) pgd_offset(mm, address)
#define ka_mm_pgd_addr_end(address, end) pgd_addr_end(address, end)
#define ka_mm_p4d_offset(pgd, address) p4d_offset(pgd, address)
#define ka_mm_pud_offset pud_offset
#define ka_mm_pud_none(pud) pud_none(pud)
#define ka_mm_pud_clear(pud) pud_clear(pud)
#define ka_mm_pud_pgtable(pud) pud_pgtable(pud)
#define ka_mm_pte_wrprotect(pte) pte_wrprotect(pte)
#define ka_mm_pte_mkdirty(pte) pte_mkdirty(pte)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
#define ka_mm_pte_mkwrite(pte, vma) pte_mkwrite(pte, vma)
#else
#define ka_mm_pte_mkwrite(pte, vma) pte_mkwrite(pte)
#endif
#define ka_mm_pte_write(pte) pte_write(pte)

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
#define ka_mm_assert_fault_locked(vmf) assert_fault_locked(vmf)
#define ka_mm_release_fault_lock(vmf) release_fault_lock(vmf)
static inline void ka_mm_acquire_fault_locked(ka_mm_struct_t *mm, ka_vm_fault_struct_t *vmf)
{
    (void)vmf; /* can not use vmf->vma, because we dose not task mmap_lock */
    mmap_read_lock(mm);
}
#endif

unsigned int __ka_get_mem_gfp_account(void);
void ka_si_meminfo(struct ka_sysinfo *val);
#define ka_mm_memdup_user_nul(src, len) memdup_user_nul(src, len)
#define ka_mm_get_mem_cgroup_from_mm(mm) get_mem_cgroup_from_mm(mm)
#define ka_mm_mem_cgroup_put(memcg) mem_cgroup_put(memcg)
#define ka_mm_pgd_addr_end(address, end) pgd_addr_end(address, end)
ka_rw_semaphore_t *ka_mm_get_mmap_sem(ka_mm_struct_t *mm);
long ka_mm_get_user_pages_remote(ka_task_struct_t *tsk, ka_mm_struct_t *mm, unsigned long long va, int write,
                                 unsigned int num, ka_page_t **pages);
long ka_mm_pin_user_pages_remote(ka_task_struct_t *tsk, ka_mm_struct_t *mm, unsigned long start, unsigned long nr_pages,
                                 unsigned long gup_flags, ka_page_t **pages, int *locked);
#define ka_mm_put_devmap_managed_page(page) put_devmap_managed_page(page)
#define ka_mm_dma_set_mask_and_coherent(dev, mask) dma_set_mask_and_coherent(dev, mask)
#define ka_mm_memcpy_flushcache(dst, src, cnt) memcpy_flushcache(dst, src, cnt)
#define ka_mm_get_user_pages_fast(start, nr_pages, gup_flags, pages) \
    get_user_pages_fast(start, nr_pages, gup_flags, pages)
#define ka_mm_numa_mem_id() numa_mem_id()

int ka_mm_alloc_contig_range(unsigned long start, unsigned long end, unsigned migratetype, gfp_t gfp_mask);
void ka_mm_free_contig_range(unsigned long pfn, unsigned long nr_pages);

bool ka_mm_mem_is_ram(u64 pa);

void *ka_mm_page_to_virt(ka_page_t *page);
unsigned long ka_mm_get_vm_flags(ka_vm_area_struct_t *vma);
void ka_mm_set_vm_flags(ka_vm_area_struct_t *vma, unsigned long flags);
void ka_mm_vm_flags_clear(ka_vm_area_struct_t *vma, unsigned long flags);
void ka_mm_set_vm_private_data(ka_vm_area_struct_t *vma, void *private_data);
void *ka_mm_get_vm_private_data(ka_vm_area_struct_t *vma);
void ka_mm_set_vm_start(ka_vm_area_struct_t *vma, unsigned long start);
unsigned long ka_mm_get_vm_start(ka_vm_area_struct_t *vma);
void ka_mm_set_vm_end(ka_vm_area_struct_t *vma, unsigned long end);
unsigned long ka_mm_get_vm_end(ka_vm_area_struct_t *vma);
ka_mm_pgprot_t *ka_mm_get_vm_pgprot(ka_vm_area_struct_t *vma);
void ka_mm_set_vm_pgprot(ka_vm_area_struct_t *vma);
ka_mm_pgprot_t ka_mm_pgprot_encrypted(ka_mm_pgprot_t prot);
ka_mm_pgprot_t ka_mm_pgprot_decrypted(ka_mm_pgprot_t prot);
void ka_mm_set_vm_ops(ka_vm_area_struct_t *vma, const ka_vm_operations_struct_t *vm_ops);
const ka_vm_operations_struct_t *ka_mm_get_vm_ops(ka_vm_area_struct_t *vma);
int ka_mm_zap_vma_ptes(ka_vm_area_struct_t *vma, unsigned long address, unsigned long size);
int ka_mm_pin_user_pages_fast(unsigned long start, int nr_pages, unsigned int gup_flags, ka_page_t **pages);
void ka_mm_unpin_user_page(ka_page_t *page);
void ka_mm_mmget(ka_mm_struct_t *mm);

typedef ka_vm_fault_t (*vmf_fault_func)(ka_vm_fault_struct_t *);
typedef int (*vm_fault_func)(ka_vm_area_struct_t *, ka_vm_fault_struct_t *);
void ka_mm_set_vm_fault_host(ka_vm_operations_struct_t *ops_managed, vmf_fault_func vmf_func, vm_fault_func vm_func);
void ka_mm_set_vm_mremap(ka_vm_operations_struct_t *ops_managed, int (*mremap_func)(ka_vm_area_struct_t *));
bool ka_mm_is_svm_addr(struct vm_area_struct *vma, u64 addr);
#define ka_mm_get_ds() get_ds()

#ifndef CFG_FEATURE_KA_ALLOC_INTERFACE
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
#define ka_alloc_hugetlb_folio_size(nid, size, module_id) alloc_hugetlb_folio_size(nid, size)
#endif
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
struct folio *ka_alloc_hugetlb_folio_size(int nid, unsigned long size, unsigned int module_id);
#endif
#endif

/* init ka_vm_operations_struct_t */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 3, 0)
#define ka_vm_ops_init_mremap(ops_mremap) .mremap = ops_mremap,
#else
#define ka_vm_ops_init_mremap(ops_mremap)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)
#define ka_vm_ops_init_may_split(may_split_fn) .may_split = may_split_fn,
#else
#define ka_vm_ops_init_may_split(may_split_fn)
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0)) && (LINUX_VERSION_CODE < KERNEL_VERSION(5, 11, 0))
#define ka_vm_ops_init_split(split_fn) .split = split_fn,
#else
#define ka_vm_ops_init_split(split_fn)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
#define HUGEPAGE_2M_ORDER 9
#define KA_DEFINE_VM_OPS_HUGE_FAULT_FUNC(func_name)                                    \
    static ka_vm_fault_t ka_##func_name(ka_vm_fault_struct_t *vmf, unsigned int order) \
    {                                                                                  \
        ka_vm_fault_t ret = KA_MM_FAULT_ERROR;                                         \
                                                                                       \
        if (order == HUGEPAGE_2M_ORDER) {                                              \
            ret = func_name(vmf->vma, vmf, 1);                                         \
        }                                                                              \
                                                                                       \
        return ret;                                                                    \
    }
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 17, 0)
#define KA_DEFINE_VM_OPS_HUGE_FAULT_FUNC(func_name)                                              \
    static ka_vm_fault_t ka_##func_name(ka_vm_fault_struct_t *vmf, enum page_entry_size pe_size) \
    {                                                                                            \
        ka_vm_fault_t ret = KA_MM_FAULT_ERROR;                                                   \
                                                                                                 \
        if (pe_size == PE_SIZE_PMD) {                                                            \
            ret = func_name(vmf->vma, vmf, 1);                                                   \
        }                                                                                        \
                                                                                                 \
        return ret;                                                                              \
    }
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#define KA_DEFINE_VM_OPS_HUGE_FAULT_FUNC(func_name)                                    \
    static int ka_##func_name(ka_vm_fault_struct_t *vmf, enum page_entry_size pe_size) \
    {                                                                                  \
        int ret = KA_MM_FAULT_ERROR;                                                   \
                                                                                       \
        if (pe_size == PE_SIZE_PMD) {                                                  \
            ret = func_name(vmf->vma, vmf, 1);                                         \
        }                                                                              \
                                                                                       \
        return ret;                                                                    \
    }
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#define ka_vm_ops_init_huge_fault(huge_fault_fn) .huge_fault = ka_##huge_fault_fn,
#else
#define ka_vm_ops_init_huge_fault(huge_fault_fn)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#define KA_DEFINE_VM_OPS_FAULT_FUNC(func_name)                     \
    static ka_vm_fault_t ka_##func_name(ka_vm_fault_struct_t *vmf) \
    {                                                              \
        return (ka_vm_fault_t)func_name(vmf->vma, vmf);            \
    }
#else
#define KA_DEFINE_VM_OPS_FAULT_FUNC(func_name)                                     \
    static int ka_##func_name(ka_vm_area_struct_t *vma, ka_vm_fault_struct_t *vmf) \
    {                                                                              \
        return (ka_vm_fault_t)func_name(vma, vmf);                                 \
    }
#endif

#define ka_vm_ops_init_fault(fault_fn) .fault = ka_##fault_fn,

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 17, 0)
#define KA_DEFINE_VM_OPS_PFN_MKWRITE_FUNC(func_name)            \
    static vm_fault_t ka_##func_name(ka_vm_fault_struct_t *vmf) \
    {                                                           \
        return (ka_vm_fault_t)func_name(vmf->vma, vmf);         \
    }
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#define KA_DEFINE_VM_OPS_PFN_MKWRITE_FUNC(func_name)     \
    static int ka_##func_name(ka_vm_fault_struct_t *vmf) \
    {                                                    \
        return (ka_vm_fault_t)func_name(vmf->vma, vmf);  \
    }
#else
#define KA_DEFINE_VM_OPS_PFN_MKWRITE_FUNC(func_name)                \
    static int ka_##func_name(void *vma, ka_vm_fault_struct_t *vmf) \
    {                                                               \
        return (ka_vm_fault_t)func_name(vma, vmf);                  \
    }
#endif

#define ka_vm_ops_init_pfn_mkwrite(pfn_mkwrite_fn) .pfn_mkwrite = ka_##pfn_mkwrite_fn,
/* init ka_vm_operations_struct_t end */

static inline bool ka_mm_is_support_pin_user_memory(void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 19, 0)
    return false;
#else
    return true;
#endif
}

static inline bool ka_mm_is_support_obmm(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
    return true;
#else
    return false;
#endif
}

static inline bool ka_mm_is_support_mmu_notifier_get_put(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
    return true;
#else
    return false;
#endif
}

#define ka_mm_num_possible_nodes() num_possible_nodes()
#define KA_MM_NODE_DATA(nid) NODE_DATA(nid)
#define ka_mm_node_online(x) node_online(x)

#ifndef UVM_OPEN
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
#define ka_mm_mmap_read_lock(x) mmap_read_lock(x)
#define ka_mm_mmap_read_unlock(x) mmap_read_unlock(x)
#define ka_mm_mmap_write_lock(x) mmap_write_lock(x)
#define ka_mm_mmap_write_unlock(x) mmap_write_unlock(x)
#else
#define ka_mm_mmap_read_lock(x) down_read(&(x)->mmap_sem)
#define ka_mm_mmap_read_unlock(x) up_read(&(x)->mmap_sem)
#define ka_mm_mmap_write_lock(x) down_write(&(x)->mmap_sem)
#define ka_mm_mmap_write_unlock(x) up_write(&(x)->mmap_sem)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
#define KA_MM_UVM_FAULT_ERROR VM_FAULT_DONE_COW
#else
#define KA_MM_UVM_FAULT_ERROR DEVMM_FAULT_OK
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
#define KA_MM_UVM_RESET_PTES_OF_HUGEPAGE_HOST(svm_proc, vma, addr, is_read_only, need_lock) \
    uvm_reset_ptes_of_hugepage_host(svm_proc, vma, addr, is_read_only, need_lock)
#else
#define KA_MM_UVM_RESET_PTES_OF_HUGEPAGE_HOST(svm_proc, vma, addr, is_read_only, need_lock) 0
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#define KA_UVM_DEFINE_VM_OPS_PFN_MKWRITE_FUNC(func_name)           \
    static ka_vm_fault_t ka_##func_name(ka_vm_fault_struct_t *vmf) \
    {                                                              \
        return (ka_vm_fault_t)func_name(vmf->vma, vmf);            \
    }
#else
#define KA_UVM_DEFINE_VM_OPS_PFN_MKWRITE_FUNC(func_name)                           \
    static int ka_##func_name(ka_vm_area_struct_t *vma, ka_vm_fault_struct_t *vmf) \
    {                                                                              \
        return (ka_vm_fault_t)func_name(vma, vmf);                                 \
    }
#endif
#endif

void *ka_mm_get_pte(const ka_vm_area_struct_t *vma, u64 va, u64 *kpg_size);
int ka_mm_va_to_pa_pgd_range(ka_pgd_t *pgd, u64 start, u64 end, u64 *pas, u64 *num);
ka_pmd_t *ka_mm_get_va_to_pmd(const ka_vm_area_struct_t *vma, unsigned long va);

struct ka_pgwalk {
    ka_vm_area_struct_t *vma;
    struct ka_pgwalk_ops *ops;
    void *priv;
};

enum ka_pte_level {
    KA_PGD_LEVEL = 0U,
    KA_P4D_LEVEL,
    KA_PUD_LEVEL,
    KA_PMD_LEVEL,
    KA_FINAL_LEVEL,
    KA_MAX_LEVEL
};

/**
 * struct ka_pgwalk_ops - callbacks for ka_walk_page_range
 * @pud_entry_post: if set, called for each non-empty PUD entry after walk next level PD
 * @pmd_entry:      if set, called for each non-empty PMD entry
 * @pte_entry:      if set, called for each non-empty PTE entry
 * @pte_hole:       if set, called for each hole at all levels
 */
struct ka_pgwalk_ops {
    int (*pud_entry_post)(ka_pud_t *pud, u64 addr, u64 next, struct ka_pgwalk *walk);
    int (*pmd_entry)(ka_pmd_t *pmd, u64 addr, u64 next, struct ka_pgwalk *walk);
    int (*pte_entry)(ka_pte_t *pte, u64 addr, u64 next, enum ka_pte_level level, struct ka_pgwalk *walk);
    int (*pte_hole)(u64 addr, u64 next, enum ka_pte_level level, struct ka_pgwalk *walk);
};

static inline u64 ka_pte_level_to_page_size(enum ka_pte_level level)
{
    u64 pte_level_to_page_size[KA_MAX_LEVEL] = {[KA_PGD_LEVEL] = 0ULL,
                                                [KA_P4D_LEVEL] = 0ULL,
                                                [KA_PUD_LEVEL] = KA_GPAGE_SIZE,
                                                [KA_PMD_LEVEL] = KA_HPAGE_SIZE,
                                                [KA_FINAL_LEVEL] = KA_MM_PAGE_SIZE};

    return pte_level_to_page_size[level];
}

int ka_walk_page_range(ka_vm_area_struct_t *vma, u64 start, u64 end, struct ka_pgwalk_ops *ops, void *priv);

#define ka_mm_virt_to_page(kaddr) virt_to_page(kaddr)
#define ka_mm_offset_in_page(p) offset_in_page(p)

#endif
