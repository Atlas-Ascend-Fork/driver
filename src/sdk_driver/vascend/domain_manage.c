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

#include "ka_task_pub.h"
#include "ka_list_pub.h"
#include "hw_vdavinci.h"
#include "dvt.h"
#include "dma_pool.h"
#include "domain_manage.h"
#include "vfio_ops.h"

#define START(map) ((map)->start)
#define LAST(map) ((map)->end - 1)

KA_INTERVAL_TREE_DEFINE(struct vm_pfn_node, rb, unsigned long, subtree_last, START, LAST, ka_maybe_unused static,
                        pfn_tree)

KA_LIST_HEAD(g_vm_domains);
KA_TASK_DEFINE_MUTEX(g_vm_domains_lock);

ka_list_head_t *get_vm_domains_list(void)
{
    return &g_vm_domains;
}

ka_mutex_t *get_vm_domains_lock(void)
{
    return &g_vm_domains_lock;
}

void dev_dom_release(ka_kref_t *ref)
{
    struct dev_dom_info *dev_dom = ka_container_of(ref, typeof(*dev_dom), ref);
    struct ram_range_info *ram_info = NULL;

    ka_list_del(&dev_dom->list);

    ka_list_for_each_entry(ram_info, &dev_dom->vm_dom->ram_info_list->head, list)
    {
        dev_dom->ops.dev_dma_unmap_ram_range(dev_dom->vdavinci, ram_info);
    }
    ka_mm_kfree(dev_dom);
}

struct dev_dom_info *dev_dom_info_find(struct vm_dom_info *vm_dom, struct hw_vdavinci *vdavinci)
{
    struct dev_dom_info *dev_dom = NULL;

    ka_list_for_each_entry(dev_dom, &vm_dom->dev_dom_list_head, list)
    {
        if (dev_dom->vdavinci == vdavinci) {
            return dev_dom;
        }
    }

    return NULL;
}

struct dev_dom_info *dev_dom_info_new(struct vm_dom_info *vm_dom, struct hw_vdavinci *vdavinci)
{
    struct dev_dom_info *dev_dom = (struct dev_dom_info *)ka_mm_kzalloc(sizeof(struct dev_dom_info), KA_GFP_KERNEL);
    if (!dev_dom) {
        return NULL;
    }

    dev_dom->vm_dom = vm_dom;
    dev_dom->vdavinci = vdavinci;
    dev_dom->status = DOMAIN_MAP_STATUS_INVALID;
    ka_base_kref_init(&dev_dom->ref);
    ka_list_add_tail(&(dev_dom->list), &(vm_dom->dev_dom_list_head));

    return dev_dom;
}

/* hold vm_dom_info->rw_semaphore before call this function */
struct dev_dom_info *dev_dom_info_get(struct vm_dom_info *vm_dom, struct hw_vdavinci *vdavinci)
{
    struct dev_dom_info *dev_dom = NULL;

    dev_dom = dev_dom_info_find(vm_dom, vdavinci);
    if (dev_dom) {
        ka_base_kref_get(&dev_dom->ref);
        return dev_dom;
    }

    return dev_dom_info_new(vm_dom, vdavinci);
}

/* hold vm_dom_info->rw_semaphore before call this function */
void dev_dom_info_put(struct dev_dom_info *dev_dom, struct hw_vdavinci *vdavinci)
{
    if (ka_base_kref_put(&dev_dom->ref, dev_dom_release)) {
        if (vdavinci->is_passthrough) {
            hw_vdavinci_iommu_detach_group(vdavinci);
        }
    }
}

STATIC void vm_remove_pfn(struct vm_dom_info *vm_dom)
{
#if (IS_VDAVINCI_KERNEL_VERSION_SUPPORT || (defined(DRV_UT)))
    struct vm_pfn_node *node;
    ka_rb_root_cached_t *pfn_root;

    if (vm_dom == NULL) {
        return;
    }
    pfn_root = &vm_dom->pfn_root;
    while (true) {
        node = pfn_tree_iter_first(pfn_root, 0, ULONG_MAX);
        if (node == NULL) {
            break;
        }
        pfn_tree_remove(node, pfn_root);
        ka_mm_kfree(node);
    }
#endif
}

/* Last kref release callback: caller must not hold g_vm_domains_lock. */
void vm_dom_info_release(ka_kref_t *ref)
{
    ka_mutex_t *g_vm_domains_lock = get_vm_domains_lock();
    struct vm_dom_info *vm_dom = ka_container_of(ref, typeof(*vm_dom), ref);

    /* Drain down_read holders (hw_vdavinci_get_iova drops cache_lock before
     * finishing with vm_dom). up_write before kfree is safe: no reader can
     * reach this semaphore after the caller cleared vdev.domain. */
    ka_task_down_write(&vm_dom->sem);
    ka_task_mutex_lock(g_vm_domains_lock);
    vm_remove_pfn(vm_dom);
    ka_mm_put_iova_domain(&vm_dom->iovad);
    ka_list_del(&vm_dom->node);
    ka_mm_kfree(vm_dom->ram_info_list);
    ka_task_mutex_unlock(g_vm_domains_lock);
    ka_task_up_write(&vm_dom->sem);
    ka_mm_kfree(vm_dom);
}

struct vm_dom_info *vm_dom_info_find(const ka_kvm_t *kvm)
{
    struct vm_dom_info *vm_dom = NULL;

    ka_list_for_each_entry(vm_dom, &g_vm_domains, node)
    {
        if (vm_dom->kvm == kvm) {
            return vm_dom;
        }
    }

    return NULL;
}

struct vm_dom_info *vm_dom_info_new(ka_kvm_t *kvm)
{
    struct vm_dom_info *vm_dom = NULL;
#if (IS_VDAVINCI_KERNEL_VERSION_SUPPORT || (defined(DRV_UT)))
    vm_dom = ka_mm_kzalloc(sizeof(struct vm_dom_info), KA_GFP_KERNEL);
    if (!vm_dom) {
        return NULL;
    }

    vm_dom->ram_info_list = (struct ram_range_info_list *)ka_mm_kzalloc(sizeof(struct ram_range_info_list),
                                                                        KA_GFP_KERNEL);
    if (!vm_dom->ram_info_list) {
        ka_mm_kfree(vm_dom);
        return NULL;
    }
    vm_dom->pfn_root = KA_RB_ROOT_CACHED;
    vm_dom->kvm = kvm;
    ka_base_atomic_set(&vm_dom->status, DOMAIN_PIN_STATUS_INVALID);
    ka_task_init_rwsem(&vm_dom->sem);
    ka_base_kref_init(&vm_dom->ref);
    KA_INIT_LIST_HEAD(&(vm_dom->ram_info_list->head));
    KA_INIT_LIST_HEAD(&(vm_dom->dev_dom_list_head));
    ka_list_add_tail(&(vm_dom->node), &g_vm_domains);

    ka_init_iova_domain(&vm_dom->iovad);
#endif
    return vm_dom;
}

struct vm_dom_info *vm_dom_info_get(ka_kvm_t *kvm)
{
    struct vm_dom_info *vm_dom = NULL;
    ka_mutex_t *g_vm_domains_lock = get_vm_domains_lock();

    if (kvm == NULL) {
        return NULL;
    }
    ka_task_mutex_lock(g_vm_domains_lock);
    vm_dom = vm_dom_info_find(kvm);
    if (vm_dom != NULL) {
        ka_base_kref_get(&vm_dom->ref);
        goto out;
    }
    vm_dom = vm_dom_info_new(kvm);

out:
    ka_task_mutex_unlock(g_vm_domains_lock);
    return vm_dom;
}

int vm_add_and_merge_pfn(struct vm_dom_info *vm_dom, unsigned long pfn, unsigned long size)
{
#if (IS_VDAVINCI_KERNEL_VERSION_SUPPORT || (defined(DRV_UT)))
    struct vm_pfn_node *node;
    ka_rb_root_cached_t *pfn_root;
    unsigned long start = pfn;
    unsigned long end = pfn + size;

    if (vm_dom == NULL) {
        return -EINVAL;
    }
    pfn_root = &vm_dom->pfn_root;
    while (true) {
        node = pfn_tree_iter_first(pfn_root, (start > 0) ? start - 1 : 0, end);
        if (node == NULL) {
            break;
        }
        start = ka_base_min(start, node->start);
        end = ka_base_max(end, node->end);

        pfn_tree_remove(node, pfn_root);
        ka_mm_kfree(node);
    }
    node = ka_mm_kzalloc(sizeof(struct vm_pfn_node), KA_GFP_KERNEL);
    if (node == NULL) {
        return -ENOMEM;
    }
    node->start = start;
    node->end = end;
    pfn_tree_insert(node, pfn_root);
#endif
    return 0;
}

bool is_vm_pfn_managed(struct vm_dom_info *vm_dom, unsigned long pfn, unsigned long size)
{
#if (IS_VDAVINCI_KERNEL_VERSION_SUPPORT || (defined(DRV_UT)))
    ka_rb_root_cached_t *pfn_root;
    struct vm_pfn_node *node;

    if (ka_unlikely(vm_dom == NULL || size == 0)) {
        return false;
    }
    if (ka_unlikely(pfn + size < pfn)) {
        return false;
    }
    pfn_root = &vm_dom->pfn_root;
    node = pfn_tree_iter_first(pfn_root, pfn, pfn + size - 1);
    if (ka_unlikely(node == NULL)) {
        return false;
    }

    return (node->start <= pfn && (pfn + size) <= node->end);
#else
    return false;
#endif
}
