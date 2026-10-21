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

#include <linux/slab.h>
#include <linux/swap.h>
#include <linux/swapops.h>
#include <linux/compiler.h>
#include <linux/version.h>
#include <linux/mmzone.h>
#include <linux/kernel.h>
#if defined(CONFIG_X86_64) && defined(CONFIG_AMD_MEM_ENCRYPT)
#include <asm/mem_encrypt.h>
#endif

#include "pbl_range_rbtree.h"
#include "securec.h"

#include "ka_kernel_def_pub.h"
#include "ka_memory_pub.h"
#include "ka_task_pub.h"
#include "ka_system_pub.h"
#include "kernel_adapt_init.h"
#include "ka_mem.h"

#if !defined(EMU_ST)
#ifndef DRV_HOST
#include "linux/share_pool.h"
#endif
#endif

#pragma message(PRINT_MACRO(LINUX_VERSION_CODE))

void ka_si_meminfo(struct ka_sysinfo *val)
{
    struct sysinfo sys_info = {0};
    si_meminfo(&sys_info);
    val->totalram = sys_info.totalram;
    val->freeram = sys_info.freeram;
    val->sharedram = sys_info.sharedram;
}
EXPORT_SYMBOL(ka_si_meminfo);

ka_rw_semaphore_t *ka_mm_get_mmap_sem(ka_mm_struct_t *mm)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
    return &mm->mmap_lock;
#else
    return &mm->mmap_sem;
#endif
}
EXPORT_SYMBOL(ka_mm_get_mmap_sem);

long ka_mm_pin_user_pages_remote(ka_task_struct_t *tsk, ka_mm_struct_t *mm, unsigned long start, unsigned long nr_pages,
                                 unsigned long gup_flags, ka_page_t **pages, int *locked)
{
    long got_num;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 5, 0)
    got_num = pin_user_pages_remote(mm, start, nr_pages, gup_flags, pages, locked);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
    got_num = pin_user_pages_remote(mm, start, nr_pages, gup_flags, pages, NULL, locked);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0)
    got_num = get_user_pages_remote(mm, start, nr_pages, gup_flags, pages, NULL, locked);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
    got_num = get_user_pages_remote(tsk, mm, start, nr_pages, gup_flags, pages, NULL, locked);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0)
    got_num = get_user_pages_remote(tsk, mm, start, nr_pages, gup_flags, pages, locked);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 6, 0)
    got_num = get_user_pages_remote(tsk, mm, start, nr_pages, gup_flags, 0, pages, locked);
#else
    got_num = get_user_pages_locked(tsk, mm, start, nr_pages, gup_flags, 0, pages, locked);
#endif
    return got_num;
}
EXPORT_SYMBOL(ka_mm_pin_user_pages_remote);

long ka_mm_get_user_pages_remote(ka_task_struct_t *tsk, ka_mm_struct_t *mm, unsigned long long va, int write,
                                 unsigned int num, ka_page_t **pages)
{
    return ka_mm_pin_user_pages_remote(tsk, mm, va, num, (write != 0) ? FOLL_WRITE : 0, pages, NULL);
}
EXPORT_SYMBOL(ka_mm_get_user_pages_remote);

void *ka_mm_page_to_virt(ka_page_t *page)
{
#ifdef page_to_virt
    return page_to_virt(page);
#else
    return __va(((phys_addr_t)(page_to_pfn(page)) << PAGE_SHIFT));
#endif
}
EXPORT_SYMBOL(ka_mm_page_to_virt);

unsigned long ka_mm_get_vm_flags(ka_vm_area_struct_t *vma)
{
    return vma->vm_flags;
}
EXPORT_SYMBOL(ka_mm_get_vm_flags);

void ka_mm_set_vm_flags(ka_vm_area_struct_t *vma, unsigned long flags)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0))
    vm_flags_set(vma, (vm_flags_t)flags);
#else
    vma->vm_flags |= flags;
#endif
}
EXPORT_SYMBOL(ka_mm_set_vm_flags);

void ka_mm_vm_flags_clear(ka_vm_area_struct_t *vma, unsigned long flags)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)
    vm_flags_clear(vma, (vm_flags_t)flags);
#else
    vma->vm_flags &= ~flags;
#endif
}
EXPORT_SYMBOL(ka_mm_vm_flags_clear);

void ka_mm_set_vm_private_data(ka_vm_area_struct_t *vma, void *private_data)
{
    vma->vm_private_data = private_data;
}
EXPORT_SYMBOL(ka_mm_set_vm_private_data);

void *ka_mm_get_vm_private_data(ka_vm_area_struct_t *vma)
{
    return vma->vm_private_data;
}
EXPORT_SYMBOL(ka_mm_get_vm_private_data);

void ka_mm_set_vm_start(ka_vm_area_struct_t *vma, unsigned long start)
{
    vma->vm_start = start;
}
EXPORT_SYMBOL(ka_mm_set_vm_start);

unsigned long ka_mm_get_vm_start(ka_vm_area_struct_t *vma)
{
    return vma->vm_start;
}
EXPORT_SYMBOL(ka_mm_get_vm_start);

void ka_mm_set_vm_end(ka_vm_area_struct_t *vma, unsigned long end)
{
    vma->vm_end = end;
}
EXPORT_SYMBOL(ka_mm_set_vm_end);

unsigned long ka_mm_get_vm_end(ka_vm_area_struct_t *vma)
{
    return vma->vm_end;
}
EXPORT_SYMBOL(ka_mm_get_vm_end);

void ka_mm_set_vm_pgprot(ka_vm_area_struct_t *vma)
{
    vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
}
EXPORT_SYMBOL(ka_mm_set_vm_pgprot);

ka_mm_pgprot_t *ka_mm_get_vm_pgprot(ka_vm_area_struct_t *vma)
{
    return &(vma->vm_page_prot);
}
EXPORT_SYMBOL(ka_mm_get_vm_pgprot);

ka_mm_pgprot_t ka_mm_pgprot_encrypted(ka_mm_pgprot_t prot)
{
#if defined(CONFIG_X86_64) && defined(CONFIG_AMD_MEM_ENCRYPT)
    /*
     * This conversion only follows the AMD SME/SEV C-bit definition.
     * Setting the bit makes a mapping private and encrypted; clearing it makes the mapping shared and decrypted.
     * Other CC architectures may use the opposite mask semantics and need a dedicated implementation.
     */
    return __pgprot(pgprot_val(prot) | sme_me_mask);
#else
    return prot;
#endif
}
KA_EXPORT_SYMBOL_GPL(ka_mm_pgprot_encrypted);

ka_mm_pgprot_t ka_mm_pgprot_decrypted(ka_mm_pgprot_t prot)
{
#if defined(CONFIG_X86_64) && defined(CONFIG_AMD_MEM_ENCRYPT)
    /* See ka_mm_pgprot_encrypted() for the AMD-specific C-bit semantics. */
    return __pgprot(pgprot_val(prot) & ~sme_me_mask);
#else
    return prot;
#endif
}
KA_EXPORT_SYMBOL_GPL(ka_mm_pgprot_decrypted);

int ka_mm_zap_vma_ptes(ka_vm_area_struct_t *vma, unsigned long address, unsigned long size)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 18, 0)
    zap_vma_ptes(vma, address, size);
    return 0;
#else
    return zap_vma_ptes(vma, address, size);
#endif
}
EXPORT_SYMBOL(ka_mm_zap_vma_ptes);

void ka_mm_set_vm_ops(ka_vm_area_struct_t *vma, const ka_vm_operations_struct_t *vm_ops)
{
    vma->vm_ops = vm_ops;
}
EXPORT_SYMBOL(ka_mm_set_vm_ops);

const ka_vm_operations_struct_t *ka_mm_get_vm_ops(ka_vm_area_struct_t *vma)
{
    return vma->vm_ops;
}
EXPORT_SYMBOL(ka_mm_get_vm_ops);

void ka_mm_mmget(ka_mm_struct_t *mm)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0))
    mmget(mm);
#else
    atomic_inc(&mm->mm_users);
#endif
}
EXPORT_SYMBOL(ka_mm_mmget);

int ka_mm_pin_user_pages_fast(unsigned long start, int nr_pages, unsigned int gup_flags, ka_page_t **pages)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
    return pin_user_pages_fast(start, nr_pages, gup_flags, pages);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 2, 0)
    return get_user_pages_fast(start, nr_pages, gup_flags, pages);
#else
    int write = (int)gup_flags;
    return get_user_pages_fast(start, nr_pages, write, pages);
#endif
}
EXPORT_SYMBOL(ka_mm_pin_user_pages_fast);

void ka_mm_unpin_user_page(ka_page_t *page)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
    unpin_user_page(page);
#else
    put_page(page);
#endif
}
EXPORT_SYMBOL(ka_mm_unpin_user_page);

void ka_mm_set_vm_fault_host(ka_vm_operations_struct_t *ops_managed, vmf_fault_func vmf_func, vm_fault_func vm_func)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
    ops_managed->fault = vmf_func;
#else
    ops_managed->fault = vm_func;
#endif
}
EXPORT_SYMBOL_GPL(ka_mm_set_vm_fault_host);

void ka_mm_set_vm_mremap(ka_vm_operations_struct_t *ops_managed, int (*mremap_func)(ka_vm_area_struct_t *))
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 3, 0)
    ops_managed->mremap = mremap_func;
#endif
    return;
}
EXPORT_SYMBOL_GPL(ka_mm_set_vm_mremap);

#ifndef EMU_ST
bool ka_mm_is_svm_addr(struct vm_area_struct *vma, u64 addr)
{
#if LINUX_VERSION_CODE > KERNEL_VERSION(5, 2, 0)
#ifndef DRV_HOST
    if (mg_is_sharepool_addr(addr)) {
        return false;
    }
#endif
#endif
    return (((vma->vm_flags) & VM_PFNMAP) != 0);
}
EXPORT_SYMBOL_GPL(ka_mm_is_svm_addr);
#endif

#ifndef EMU_ST
#if defined(FUNC___PTE_MAP_PRESENT) && defined(FUNC___PTE_OFFSET_MAP_PRESENT) &&                                  \
    defined(FUNC_PMDP_GET_LOCKLESS_PRESENT) && defined(FUNC_IS_PMD_MIGRATION_ENTRY_PRESENT) &&                    \
    defined(FUNC_PMD_NONE_PRESENT) && defined(FUNC_PMD_TRANS_HUGE_PRESENT) && defined(FUNC_PMD_DEVMAP_PRESENT) && \
    defined(FUNC_PMD_BAD_PRESENT) && defined(FUNC_PMD_ERROR_PRESENT) && defined(FUNC_PMD_CLEAR_PRESENT)
#define KA_MEM_PTE_OFFSET_MAP_CAPABLE
#endif

#ifdef KA_MEM_PTE_OFFSET_MAP_CAPABLE
static ka_pte_t *ka_mm_inner_pte_offset_map(ka_pmd_t *pmd, unsigned long addr, ka_pmd_t *pmdvalp)
{
    ka_pmd_t pmdval;

    /* rcu_read_lock() to be added later */
    pmdval = ka_mm_pmdp_get_lockless(pmd);
    if (pmdvalp) {
        *pmdvalp = pmdval;
    }
    if (unlikely(ka_mm_pmd_none(pmdval) || ka_mm_is_pmd_migration_entry(pmdval))) {
        goto nomap;
    }
    if (unlikely(ka_mm_pmd_trans_huge(pmdval) || ka_mm_pmd_devmap(pmdval))) {
        goto nomap;
    }
    if (unlikely(ka_mm_pmd_bad(pmdval))) {
        ka_mm_pmd_ERROR(*pmd);
        ka_mm_pmd_clear(pmd);
        goto nomap;
    }
    return __ka_mm_pte_map(&pmdval, addr);

nomap:
    /* rcu_read_unlock() to be added later */
    return NULL;
}
#endif

/**
 * @brief get the page table entry of the va
 * @attention
 * kpg_size=KA_MM_PAGE_SIZE(4K) -> pte
 * kpg_size=KA_HPAGE_SIZE(2M) -> pmd
 * kpg_size=PUD_SIZE(1G) -> pud
 * @param [in] vma: ka_vm_area_struct_t
 * @param [in] va: va
 * @param [out] kpg_size: real page size from kernel
 * @return NULL for fail, others for success, means pte pointer
 */
void *ka_mm_get_pte(const ka_vm_area_struct_t *vma, u64 va, u64 *kpg_size)
{
    ka_pgd_t *pgd = NULL;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
    p4d_t *p4d = NULL;
#endif
    ka_pud_t *pud = NULL;
    ka_pmd_t *pmd = NULL;
    ka_pte_t *pte = NULL;

    if ((vma == NULL) || (vma->vm_mm == NULL) || (kpg_size == NULL)) {
        return NULL;
    }
    /* too much log, not print */
    pgd = ka_mm_pgd_offset(vma->vm_mm, va);
    if (PXD_JUDGE(pgd) != 0) {
        return NULL;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
    p4d = ka_mm_p4d_offset(pgd, va);
    if (PXD_JUDGE(p4d) != 0) {
        return NULL;
    }

    /* if kernel version is above 4.11.0,then 5 level pt arrived.
    pud_offset(pgd,va) changed to pud_offset(p4d,va) for x86
    but not changed in arm64 */
    pud = ka_mm_pud_offset(p4d, va);
#else
    pud = ka_mm_pud_offset(pgd, va);
#endif
    if (PUD_GIANT(pud) != 0) {
        *kpg_size = KA_MM_GIANT_PAGE_SIZE;
        return pud;
    }
    if (PXD_JUDGE(pud) != 0) {
        return NULL;
    }

    pmd = ka_mm_pmd_offset(pud, va);
    /* huge page pmd can not judge bad flag */
    if (PMD_HUGE(pmd) != 0) {
        *kpg_size = KA_HPAGE_SIZE;
        return pmd;
    }
    if (PMD_JUDGE(pmd) != 0) {
        return NULL;
    }

#ifdef KA_MEM_PTE_OFFSET_MAP_CAPABLE
    pte = ka_mm_inner_pte_offset_map(pmd, va, NULL);
#else
    pte = ka_mm_pte_offset_map(pmd, va);
#endif
    if ((pte == NULL) || (ka_mm_pte_none(*pte) != 0) || (ka_mm_pte_present(*pte) == 0)) {
        return NULL;
    }
    *kpg_size = KA_MM_PAGE_SIZE;
    return pte;
}
EXPORT_SYMBOL_GPL(ka_mm_get_pte);

ka_pmd_t *ka_mm_get_va_to_pmd(const ka_vm_area_struct_t *vma, unsigned long va) /* To be deleted */
{
    ka_pgd_t *pgd = NULL;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
    p4d_t *p4d = NULL;
#endif
    ka_pud_t *pud = NULL;
    ka_pmd_t *pmd = NULL;

    if ((vma == NULL) || (vma->vm_mm == NULL)) {
        return NULL;
    }
    /* too much log, not print */
    pgd = ka_mm_pgd_offset(vma->vm_mm, va);
    if (PXD_JUDGE(pgd)) {
        return NULL;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
    p4d = ka_mm_p4d_offset(pgd, va);
    if (PXD_JUDGE(p4d) != 0) {
        return NULL;
    }

    /* if kernel version is above 4.11.0,then 5 level pt arrived.
    pud_offset(pgd,va) changed to pud_offset(p4d,va) for x86
    but not changed in arm64 */
    pud = ka_mm_pud_offset(p4d, va);
    if (PXD_JUDGE(pud) != 0) {
        return NULL;
    }
#else
    pud = ka_mm_pud_offset(pgd, va);
    if (PXD_JUDGE(pud) != 0) {
        return NULL;
    }
#endif

    pmd = ka_mm_pmd_offset(pud, va);
    return pmd;
}
EXPORT_SYMBOL_GPL(ka_mm_get_va_to_pmd);

static int ka_mm_va_to_pa_pmd_range(ka_pmd_t *pmd, u64 start, u64 end, u64 *pas, u64 *num)
{
    ka_pte_t *pte = NULL;
    u64 got_num = 0;
    u64 va = start;

#ifdef KA_MEM_PTE_OFFSET_MAP_CAPABLE
    pte = ka_mm_inner_pte_offset_map(pmd, va, NULL);
#else
    pte = ka_mm_pte_offset_map(pmd, va);
#endif
    if (pte == NULL) {
        return -ERANGE;
    }

    for (; va != end; pte++, va += KA_MM_PAGE_SIZE) {
        if ((ka_mm_pte_none(*pte) != 0) || (ka_mm_pte_present(*pte) == 0)) {
            return -ERANGE;
        }

        pas[got_num] = KA_MM_PFN_PHYS(ka_mm_pte_pfn(*pte));
        got_num++;
    }

    *num = got_num;
    return 0;
}

static int ka_mm_va_to_pa_pud_range(ka_pud_t *pud, u64 start, u64 end, u64 *pas, u64 *num)
{
    ka_pmd_t *pmd = NULL;
    u64 got_num = 0;
    u64 va = start;
    u64 next;

    pmd = ka_mm_pmd_offset(pud, va);
    for (; va != end; pmd++, va = next) {
        int ret;
        u64 n;

        if (PXD_JUDGE(pmd) != 0) {
            return -EDOM;
        }

        next = ka_mm_pmd_addr_end(va, end);
        ret = ka_mm_va_to_pa_pmd_range(pmd, va, next, &pas[got_num], &n);
        if (ret != 0) {
            return ret;
        }
        got_num += n;
    }

    *num = got_num;
    return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
static int ka_mm_va_to_pa_p4d_range(p4d_t *p4d, u64 start, u64 end, u64 *pas, u64 *num)
{
    ka_pud_t *pud = NULL;
    u64 got_num = 0;
    u64 va = start;
    u64 next;

    pud = ka_mm_pud_offset(p4d, va);
    for (; va != end; pud++, va = next) {
        int ret;
        u64 n = 0;

        if (PXD_JUDGE(pud) != 0) {
            return -EDOM;
        }

        next = pud_addr_end(va, end);
        ret = ka_mm_va_to_pa_pud_range(pud, va, next, &pas[got_num], &n);
        if (ret != 0) {
            return ret;
        }
        got_num += n;
    }

    *num = got_num;
    return 0;
}

int ka_mm_va_to_pa_pgd_range(ka_pgd_t *pgd, u64 start, u64 end, u64 *pas, u64 *num)
{
    p4d_t *p4d = NULL;
    u64 got_num = 0;
    u64 va = start;
    u64 next;

    if (pgd == NULL || pas == NULL || num == NULL) {
        return -EINVAL;
    }
    p4d = ka_mm_p4d_offset(pgd, va);
    for (; va != end; p4d++, va = next) {
        int ret;
        u64 n;

        if (PXD_JUDGE(p4d) != 0) {
            return -EDOM;
        }

        next = p4d_addr_end(va, end);
        ret = ka_mm_va_to_pa_p4d_range(p4d, va, next, &pas[got_num], &n);
        if (ret != 0) {
            return ret;
        }
        got_num += n;
    }

    *num = got_num;
    return 0;
}
#else
int ka_mm_va_to_pa_pgd_range(ka_pgd_t *pgd, u64 start, u64 end, u64 *pas, u64 *num)
{
    ka_pud_t *pud = NULL;
    u64 got_num = 0;
    u64 va = start;
    u64 next;

    if (pgd == NULL || pas == NULL || num == NULL) {
        return -EINVAL;
    }
    pud = ka_mm_pud_offset(pgd, va);
    for (; va != end; pud++, va = next) {
        int ret;
        u64 n;

        if (PXD_JUDGE(pud) != 0) {
            return -EDOM;
        }

        next = pud_addr_end(va, end);
        ret = ka_mm_va_to_pa_pud_range(pud, va, next, &pas[got_num], &n);
        if (ret != 0) {
            return ret;
        }
        got_num += n;
    }

    *num = got_num;
    return 0;
}
#endif
EXPORT_SYMBOL_GPL(ka_mm_va_to_pa_pgd_range);
#endif

int ka_mm_alloc_contig_range(unsigned long start, unsigned long end, unsigned migratetype, gfp_t gfp_mask)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
    return alloc_contig_range(start, end, migratetype, gfp_mask);
#else
    return -ENOSYS;
#endif
}
EXPORT_SYMBOL_GPL(ka_mm_alloc_contig_range);

void ka_mm_free_contig_range(unsigned long pfn, unsigned long nr_pages)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
    free_contig_range(pfn, nr_pages);
#endif
}
EXPORT_SYMBOL_GPL(ka_mm_free_contig_range);

static struct range_rbtree g_ka_ram_record_tree;
static KA_TASK_DEFINE_RWLOCK(g_ka_ram_record_lock);

static int ka_ram_record_walk_cb(struct resource *res, void *arg)
{
    struct range_rbtree *tree = (struct range_rbtree *)arg;
    struct range_rbtree_node *node = NULL;

    node = ka_mm_kzalloc(sizeof(struct range_rbtree_node), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (node == NULL) {
        return -ENOMEM;
    }

    node->start = res->start;
    node->size = res->end - res->start + 1;

    /* call ka_ram_record_walk_cb when insmod ko, so not need lock */
    if (range_rbtree_insert(tree, node) != 0) {
        ka_mm_kfree(node);
        node = NULL;
    }

    return 0;
}

int ka_mm_ram_record_init(void)
{
    int ret;

    range_rbtree_init(&g_ka_ram_record_tree);

    ret = walk_iomem_res_desc(IORES_DESC_NONE, IORESOURCE_SYSTEM_RAM | IORESOURCE_BUSY, 0, ULLONG_MAX,
                              &g_ka_ram_record_tree, ka_ram_record_walk_cb);
    if ((ret != 0) && (ret != -ENOMEM)) {
        ka_mm_ram_record_uninit();
        return ret;
    }

    ka_info("Ram walk finish, ret=%d, node_num=%u\n", ret, g_ka_ram_record_tree.node_num);
    return 0;
}

void ka_mm_ram_record_uninit(void)
{
    struct range_rbtree_node *node = NULL;

    do {
        ka_task_write_lock(&g_ka_ram_record_lock);
        node = range_rbtree_erase_one(&g_ka_ram_record_tree);
        ka_task_write_unlock(&g_ka_ram_record_lock);

        if (node == NULL) {
            break;
        }
        ka_mm_kfree(node);
    } while (node != NULL);
}

/* use read_lock, could not call in interrupt */
bool ka_mm_mem_is_ram(u64 pa)
{
    bool is_ram;

    ka_task_read_lock(&g_ka_ram_record_lock);
    is_ram = range_rbtree_check_exist(&g_ka_ram_record_tree, pa, 1);
    ka_task_read_unlock(&g_ka_ram_record_lock);

    if (!is_ram) {
        is_ram = page_is_ram(KA_MM_PFN_DOWN(pa));
    }

    return is_ram;
}
EXPORT_SYMBOL_GPL(ka_mm_mem_is_ram);

/*
 * ka_mm_vmap_pfn: Map physical addresses (with or without struct page) to
 * kernel virtual address space via an opaque handle.
 *
 * Internally handles both contiguous and non-contiguous physical addresses:
 * - Contiguous PFNs: single ioremap, handle->kva is contiguous (all kernels)
 * - Non-contiguous PFNs, >= 5.10: kernel vmap_pfn, handle->kva is contiguous
 * - Non-contiguous PFNs, < 5.10: per-page ioremap, kva_list based access
 *
 * ka_mm_vmap_pfn_kva() returns contiguous kva when available (contiguous or
 * >= 5.10), NULL when kva is non-contiguous (< 5.10 non-contiguous PFNs).
 * ka_mm_vmap_pfn_memcpy_toio/fromio() work for all cases transparently.
 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0) || !(IS_ENABLED(CONFIG_VMAP_PFN))
/*
 * Check if PFNs are contiguous: pfn[i] == pfn[0] + i for all i.
 * If contiguous, a single ioremap of the whole range gives a contiguous kva.
 */
static bool pfns_are_contiguous(unsigned long *pfns, unsigned int count)
{
    unsigned int i;
    for (i = 1; i < count; i++) {
        if (pfns[i] != pfns[0] + i)
            return false;
    }
    return true;
}

static struct ka_vmap_pfn_handle *vmap_pfn_manual(unsigned long *pfns, unsigned int count, pgprot_t prot)
{
    struct ka_vmap_pfn_handle *handle;
    unsigned int i;

    if (pfns == NULL || count == 0)
        return NULL;

    handle = kzalloc(sizeof(struct ka_vmap_pfn_handle), GFP_KERNEL);
    if (handle == NULL)
        return NULL;

    if (pfns_are_contiguous(pfns, count)) {
        /* Contiguous PFNs: single ioremap gives contiguous kva */
        handle->kva = ioremap(pfns[0] << PAGE_SHIFT, (unsigned long)count << PAGE_SHIFT);
        if (handle->kva == NULL) {
            kfree(handle);
            return NULL;
        }
        return handle;
    }

    /* Non-contiguous PFNs: per-page ioremap, kva not contiguous */
    handle->kva_list = kcalloc(count, sizeof(void *), GFP_KERNEL);
    if (handle->kva_list == NULL) {
        kfree(handle);
        return NULL;
    }

    handle->size_list = kcalloc(count, sizeof(unsigned long), GFP_KERNEL);
    if (handle->size_list == NULL) {
        kfree(handle->kva_list);
        kfree(handle);
        return NULL;
    }

    handle->count = count;
    handle->kva = NULL;

    for (i = 0; i < count; i++) {
        handle->kva_list[i] = ioremap(pfns[i] << PAGE_SHIFT, PAGE_SIZE);
        if (handle->kva_list[i] == NULL) {
            unsigned int j;
            for (j = 0; j < i; j++)
                iounmap(handle->kva_list[j]);
            kfree(handle->size_list);
            kfree(handle->kva_list);
            kfree(handle);
            return NULL;
        }
        handle->size_list[i] = PAGE_SIZE;
    }

    return handle;
}
#endif

struct ka_vmap_pfn_handle *ka_mm_vmap_pfn_create(unsigned long *pfns, unsigned int count, pgprot_t prot)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0) && IS_ENABLED(CONFIG_VMAP_PFN)
    struct ka_vmap_pfn_handle *handle;
    void *kva;

    if (pfns == NULL || count == 0)
        return NULL;

    kva = vmap_pfn(pfns, count, prot);
    if (kva == NULL)
        return NULL;

    handle = kzalloc(sizeof(struct ka_vmap_pfn_handle), GFP_KERNEL);
    if (handle == NULL) {
        vunmap(kva);
        return NULL;
    }

    handle->kva = kva;
    return handle;
#else
    return vmap_pfn_manual(pfns, count, prot);
#endif
}
EXPORT_SYMBOL_GPL(ka_mm_vmap_pfn_create);

void ka_mm_vmap_pfn_destroy(struct ka_vmap_pfn_handle *handle)
{
    if (handle == NULL)
        return;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0) && IS_ENABLED(CONFIG_VMAP_PFN)
    if (handle->kva != NULL)
        vunmap(handle->kva);
#else
    if (handle->kva != NULL) {
        /* Contiguous kva from single ioremap */
        iounmap(handle->kva);
    } else {
        /* Per-page ioremap'd kva_list */
        unsigned int i;
        for (i = 0; i < handle->count; i++) {
            if (handle->kva_list[i] != NULL)
                iounmap(handle->kva_list[i]);
        }
        kfree(handle->size_list);
        kfree(handle->kva_list);
    }
#endif
    kfree(handle);
}
EXPORT_SYMBOL_GPL(ka_mm_vmap_pfn_destroy);

void *ka_mm_vmap_pfn_kva(struct ka_vmap_pfn_handle *handle)
{
    if (handle == NULL)
        return NULL;
    return handle->kva;
}
EXPORT_SYMBOL_GPL(ka_mm_vmap_pfn_kva);

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0) || !(IS_ENABLED(CONFIG_VMAP_PFN))
static void vmap_pfn_manual_memcpy(struct ka_vmap_pfn_handle *handle, unsigned long offset, void *buf, size_t len,
                                   bool to_device)
{
    unsigned long remaining = len;
    unsigned long pos = offset;
    unsigned int seg;
    unsigned long seg_offset, seg_len;
    void *seg_kva;

    /* Find starting segment */
    for (seg = 0; seg < handle->count; seg++) {
        if (pos < handle->size_list[seg])
            break;
        pos -= handle->size_list[seg];
    }
    if (seg >= handle->count) {
        return;
    }

    while (remaining > 0 && seg < handle->count) {
        seg_offset = pos;
        seg_len = min(remaining, handle->size_list[seg] - seg_offset);
        seg_kva = handle->kva_list[seg] + seg_offset;

        if (to_device)
            memcpy_toio(seg_kva, buf, seg_len);
        else
            memcpy_fromio(buf, seg_kva, seg_len);

        buf += seg_len;
        remaining -= seg_len;
        pos = 0; /* Next segment starts at offset 0 */
        seg++;
    }
}
#endif

void ka_mm_vmap_pfn_memcpy_toio(struct ka_vmap_pfn_handle *handle, unsigned long offset, const void *src, size_t len)
{
    if (handle == NULL || src == NULL || len == 0)
        return;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0) && IS_ENABLED(CONFIG_VMAP_PFN)
    memcpy_toio(handle->kva + offset, src, len);
#else
    if (handle->kva != NULL) {
        memcpy_toio(handle->kva + offset, src, len);
    } else {
        vmap_pfn_manual_memcpy(handle, offset, (void *)src, len, true);
    }
#endif
}
EXPORT_SYMBOL_GPL(ka_mm_vmap_pfn_memcpy_toio);

void ka_mm_vmap_pfn_memcpy_fromio(struct ka_vmap_pfn_handle *handle, unsigned long offset, void *dst, size_t len)
{
    if (handle == NULL || dst == NULL || len == 0)
        return;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0) && IS_ENABLED(CONFIG_VMAP_PFN)
    memcpy_fromio(dst, handle->kva + offset, len);
#else
    if (handle->kva != NULL) {
        memcpy_fromio(dst, handle->kva + offset, len);
    } else {
        vmap_pfn_manual_memcpy(handle, offset, dst, len, false);
    }
#endif
}
EXPORT_SYMBOL_GPL(ka_mm_vmap_pfn_memcpy_fromio);
