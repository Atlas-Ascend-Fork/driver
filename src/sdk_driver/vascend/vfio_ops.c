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

#include "ka_kvm_pub.h"
#include "ka_mm_pub.h"
#include "ka_task_pub.h"
#include "ka_fs_pub.h"
#include "ka_pci_pub.h"
#include "ka_errno_pub.h"
#include "ka_dfx_pub.h"
#include "ka_memory_pub.h"
#include "ka_system_pub.h"
#include "hw_vdavinci.h"
#include "dvt.h"
#include "dvt_sysfs.h"
#include "domain_manage.h"
#include "dma_pool.h"
#include "kvmdt.h"
#include "log.h"
#include "vfio_ops.h"

struct vdavinci_monitor {
    ka_atomic64_t map_count;
    ka_atomic64_t pin_count;
};

#if IS_VDAVINCI_VPMEM_SUPPORT
static bool (*pfn_is_vpmem_fn)(unsigned long pfn) = NULL;
#endif

STATIC struct vdavinci_monitor monitor = {
    .map_count = KA_BASE_ATOMIC64_INIT(0),
    .pin_count = KA_BASE_ATOMIC64_INIT(0),
};

void vdavinci_iommu_unmap(ka_device_t *dev, unsigned long iova, size_t size)
{
    size_t unmapped;
    ka_iommu_domain_t *domain = NULL;

    domain = ka_pci_iommu_get_domain_for_dev(dev);
    if (domain == NULL) {
        vascend_err(dev, "iommu domain is null\n");
        return;
    }
    ka_mm_dma_sync_single_for_cpu(dev, iova, size, KA_DMA_BIDIRECTIONAL);
    unmapped = ka_iommu_unmap(domain, iova, size);
    KA_WARN_ON(unmapped != size);
    ka_base_atomic64_sub((long)unmapped, &monitor.map_count);
}

int vdavinci_iommu_map(ka_device_t *dev, unsigned long iova, phys_addr_t paddr, size_t size, int prot)
{
    int ret = 0;
    ka_iommu_domain_t *domain = ka_pci_iommu_get_domain_for_dev(dev);

    if (domain == NULL) {
        vascend_err(dev, "iommu domain is null\n");
        return -ENODEV;
    }
    ret = ka_iommu_map(domain, iova, paddr, size, prot, KA_GFP_KERNEL);
    if (ret == 0 && !ka_is_dev_dma_coherent(dev)) {
        ka_mm_dma_sync_single_for_device(dev, iova, size, KA_DMA_BIDIRECTIONAL);
    }
    if (ret == 0) {
        ka_base_atomic64_add((long)size, &monitor.map_count);
    }
    return ret;
}

STATIC int vdavinci_pin_vpmem_pages(struct hw_vdavinci *vdavinci, ka_pin_info *pin_info)
{
    unsigned long pfn = 0;
    int i = 0;

    if (vdavinci == NULL || pin_info == NULL) {
        return -EINVAL;
    }
    if (pin_info->npage <= 0 || pin_info->pages == NULL) {
        return -EINVAL;
    }
    for (i = 0; i < pin_info->npage; i++) {
        /* Call kvmdt directly: the pin path runs under the vm_dom->sem
         * write side, taking the read side in the interface layer would
         * self-deadlock on the non-recursive rwsem. */
        pfn = g_hw_kvmdt_ops.gfn_to_mfn(vdavinci, pin_info->gfn + i);
        if (pfn == KVMDT_ERROR_PFN) {
            vascend_err(vdavinci_to_dev(vdavinci), "error pfn: 0x%lx, gfn: 0x%lx\n", pfn, pin_info->gfn + i);
            return -EFAULT;
        }
        pin_info->pages[i] = ka_mm_pfn_to_page(pfn);
    }

    return pin_info->npage;
}

STATIC void vdavinci_unpin_vpmem_pages(struct hw_vdavinci *vdavinci, ka_pin_info *pin_info)
{
    int i = 0;

    if (pin_info == NULL || pin_info->pages == NULL) {
        return;
    }
    for (i = 0; i < pin_info->npage; i++) {
        pin_info->pages[i] = NULL;
    }
}

STATIC int vdavinci_pin_hva_pages(struct hw_vdavinci *vdavinci, ka_pin_info *pin_info)
{
    int pins = -EFAULT;
#if IS_VDAVINCI_PIN_HVA_SUPPORT
    unsigned long hva = 0;
    int i = 0;
    struct mm_struct *mm = 0;

    if (vdavinci == NULL || vdavinci->mm == NULL || vdavinci->qemu_task == NULL) {
        return -EINVAL;
    }
    if (pin_info == NULL || pin_info->npage <= 0 || pin_info->pages == NULL) {
        return -EINVAL;
    }
    hva = kvmdt_gfn_to_hva(vdavinci, pin_info->gfn);
    if (hva == KVMDT_ERROR_PFN) {
        vascend_err(vdavinci_to_dev(vdavinci), "error hva: 0x%lx, gfn: 0x%lx\n", hva, pin_info->gfn);
        return -EFAULT;
    }
    mm = vdavinci->mm;
    if (!ka_mm_mmget_not_zero(mm)) {
        return -ENODEV;
    }
    ka_mm_mmap_read_lock(mm);
    pins = ka_mm_pin_user_pages_remote(vdavinci->qemu_task, mm, hva, pin_info->npage, KA_FOLL_WRITE | KA_FOLL_LONGTERM,
                                       pin_info->pages, NULL);
    ka_mm_mmap_read_unlock(mm);
    ka_mm_mmput(mm);
    if (pins < 0) {
        return pins;
    }
    if (pins != pin_info->npage) {
        vascend_err(vdavinci_to_dev(vdavinci), "pin partial fail: total=%d, success=%d\n", pin_info->npage, pins);
        for (i = 0; i < pins; i++) {
            ka_mm_unpin_user_page(pin_info->pages[i]);
        }
        return -EFAULT;
    }
#endif
    return pins;
}

STATIC void vdavinci_unpin_hva_pages(struct hw_vdavinci *vdavinci, ka_pin_info *pin_info)
{
#if IS_VDAVINCI_PIN_HVA_SUPPORT
    int i;

    if (vdavinci == NULL || pin_info == NULL) {
        return;
    }
    if (pin_info->pages == NULL || pin_info->npage <= 0) {
        return;
    }
    for (i = 0; i < pin_info->npage; i++) {
        if (pin_info->pages[i] == NULL || is_error_page(pin_info->pages[i])) {
            continue;
        }
        ka_mm_unpin_user_page(pin_info->pages[i]);
        pin_info->pages[i] = NULL;
    }
#endif
}

void vdavinci_unpin_pages(struct hw_vdavinci *vdavinci, ka_pin_info *pin_info)
{
    if (vdavinci == NULL || pin_info == NULL) {
        return;
    }
    if (pin_info->pages == NULL || pin_info->npage <= 0) {
        vascend_err(vdavinci_to_dev(vdavinci), "pages is null\n");
        return;
    }
    if (vdavinci_pfn_is_vpmem(ka_mm_page_to_pfn(pin_info->pages[0]))) {
        vdavinci_unpin_vpmem_pages(vdavinci, pin_info);
    } else if (IS_ENABLED(IS_VDAVINCI_PIN_HVA_SUPPORT)) {
        vdavinci_unpin_hva_pages(vdavinci, pin_info);
    } else {
        ka_vfio_unpin_pages(vdavinci->vdev.vfio_device, pin_info);
    }
    ka_base_atomic64_sub(pin_info->npage, &monitor.pin_count);
}

int vdavinci_pin_pages(struct hw_vdavinci *vdavinci, ka_pin_info *pin_info)
{
    int ret = 0;

    if (vdavinci == NULL || pin_info == NULL) {
        return -EINVAL;
    }
    if (vdavinci_gfn_is_vpmem(vdavinci, pin_info->gfn)) {
        ret = vdavinci_pin_vpmem_pages(vdavinci, pin_info);
    } else if (IS_ENABLED(IS_VDAVINCI_PIN_HVA_SUPPORT)) {
        ret = vdavinci_pin_hva_pages(vdavinci, pin_info);
    } else {
        ret = ka_vfio_pin_pages(vdavinci->vdev.vfio_device, pin_info);
    }
    if (ret > 0) {
        ka_base_atomic64_add(ret, &monitor.pin_count);
    }
    return ret;
}

struct hw_vdavinci *get_vdavinci_from_dev(ka_device_t *dev)
{
    ka_vdev *vdev = NULL;

    vdev = ka_get_mdev_drvdata(dev);
    if (vdev == NULL) {
        return NULL;
    }
    return vdev->data;
}

struct hw_vdavinci *get_vdavinci_from_mdev(ka_mdev_device_t *mdev)
{
    return get_vdavinci_from_dev(ka_driver_mdev_dev(mdev));
}

STATIC int vdavinci_init_type_ops(ka_dvt_dev *dvt)
{
    ka_mdev_type_ops *ops = NULL;

    ops = ka_mm_kzalloc(sizeof(ka_mdev_type_ops), KA_GFP_KERNEL);
    if (ops == NULL) {
        return -ENOMEM;
    }
    ops->get_available = available_instances_ops;
    ops->show_description = description_ops;
    ops->device_api_ops = device_api_ops;
    ops->store_vfg_id = vfg_id_store_ops;
    ops->hw_vdev_create = hw_vdavinci_create;
    dvt->type_ops = ops;

    return 0;
}

STATIC void vdavinci_clean_type_ops(ka_dvt_dev *dvt)
{
    ka_mm_kfree(dvt->type_ops);
    dvt->type_ops = NULL;
}

int vdavinci_register_device(ka_device_t *dev, ka_dvt_dev *dvt, const char *name)
{
    int ret = 0;

    ret = vdavinci_init_type_ops(dvt);
    if (ret != 0) {
        return ret;
    }
    ret = ka_mdev_register_type_ops(dev, dvt->type_ops);
    if (ret != 0) {
        goto free_type_ops;
    }
    ret = ka_vdev_init_type_groups(dvt);
    if (ret != 0) {
        vascend_err(dev, "vdavinci init type groups error: %d\n", ret);
        goto clean_mdev_ops;
    }
    ret = ka_vdev_set_device_ops(dvt, get_hw_vdavinci_groups());
    if (ret != 0) {
        vascend_err(dev, "vdavinci set device's ops error: %d\n", ret);
        goto clean_group;
    }
    ret = ka_vdev_register_mdev_device(dev, dvt, name);
    if (ret != 0) {
        vascend_err(dev, "vdavinci register mdev device error: %d\n", ret);
        goto clean_ops;
    }

    return 0;

clean_ops:
    ka_vdev_clean_device_ops(dvt);
clean_group:
    ka_vdev_cleanup_type_groups(dvt);
clean_mdev_ops:
    ka_mdev_unregister_type_ops(dev);
free_type_ops:
    vdavinci_clean_type_ops(dvt);
    return ret;
}

void vdavinci_unregister_device(ka_device_t *dev, ka_dvt_dev *dvt)
{
    ka_vdev_unregister_mdev_device(dev, dvt);
    ka_vdev_clean_device_ops(dvt);
    ka_vdev_cleanup_type_groups(dvt);
    ka_mdev_unregister_type_ops(dev);
    vdavinci_clean_type_ops(dvt);
}

int vdavinci_iommu_notify(void *data, unsigned long iova, unsigned long size)
{
    struct hw_vdavinci *vdavinci = data;
    unsigned long start_gfn;

    if (vdavinci == NULL) {
        return KA_NOTIFY_BAD;
    }
    start_gfn = iova >> KA_MM_PAGE_SHIFT;
    ka_task_mutex_lock(&vdavinci->vdev.cache_lock);
    hw_vdavinci_unplug_ram(vdavinci, start_gfn, size);
    ka_task_mutex_unlock(&vdavinci->vdev.cache_lock);

    return KA_NOTIFY_OK;
}

int vdavinci_group_notify(void *data, void *group_data)
{
    struct vm_dom_info *vm_dom = NULL;
    struct hw_vdavinci *vdavinci = data;

    if (vdavinci == NULL) {
        return KA_NOTIFY_BAD;
    }
    if (group_data == NULL) {
        ka_task_schedule_work(&vdavinci->vdev.release_work);
    } else {
        vm_dom = vm_dom_info_get(vdavinci->vdev.kvm);
        if (vm_dom == NULL) {
            vascend_err(vdavinci_to_dev(vdavinci), "vnpu init domain failed.\n");
            return KA_NOTIFY_BAD;
        }
        vdavinci->vdev.domain = vm_dom;
    }

    return KA_NOTIFY_OK;
}

void vdavinci_vfio_dma_unmap(void *data, u64 iova, u64 length)
{
    unsigned long start_gfn = iova >> KA_MM_PAGE_SHIFT;
    struct hw_vdavinci *vdavinci = data;

    if (vdavinci == NULL) {
        return;
    }
    ka_task_mutex_lock(&vdavinci->vdev.cache_lock);
    hw_vdavinci_unplug_ram(vdavinci, start_gfn, length);
    ka_task_mutex_unlock(&vdavinci->vdev.cache_lock);
}

int vdavinci_register_vfio_group(struct hw_vdavinci *vdavinci)
{
    int ret = 0;

    vdavinci->vdev.domain = NULL;
    ret = ka_vdev_register_vfio_group(&vdavinci->vdev);
    if (ret != 0) {
        vascend_err(vdavinci_to_dev(vdavinci),
                    "vfio register iommu notifier failed, "
                    "vid: %u, ret: %d\n",
                    vdavinci->id, ret);
        return ret;
    }
    if (vdavinci->vdev.domain != NULL) {
        return 0;
    }
    if (vdavinci->vdev.kvm == NULL) {
        vascend_err(vdavinci_to_dev(vdavinci), "kvm is null\n");
        goto unregister_group;
    }
    vdavinci->vdev.domain = vm_dom_info_get(vdavinci->vdev.kvm);
    if (vdavinci->vdev.domain == NULL) {
        vascend_err(vdavinci_to_dev(vdavinci), "get vm's domain failed\n");
        goto unregister_group;
    }

    return 0;

unregister_group:
    /* Roll back the notifiers registered above, otherwise a failed open
     * leaks them past the vdavinci destruction. */
    ka_vdev_unregister_vfio_group(&vdavinci->vdev);
    return -EINVAL;
}

void vdavinci_unregister_vfio_group(struct hw_vdavinci *vdavinci)
{
    ka_mutex_t *g_vm_domains_lock = get_vm_domains_lock();
    struct vm_dom_info *vm_dom = NULL;

    if (vdavinci == NULL) {
        return;
    }
    vm_dom = vdavinci->vdev.domain;
    /* Unregister the notifiers first: their callbacks write vdev.domain, and
     * none can run once vfio_unregister_notifier has returned. */
    ka_vdev_unregister_vfio_group(&vdavinci->vdev);
    /* Clear domain under cache_lock before dropping the vm_dom reference:
     * readers get vm_dom and take vm_dom->sem inside cache_lock. Once the
     * last ref is dropped, down_write in vm_dom_info_release drains all
     * remaining readers. */
    ka_task_mutex_lock(&vdavinci->vdev.cache_lock);
    vdavinci->vdev.domain = NULL;
    ka_task_mutex_unlock(&vdavinci->vdev.cache_lock);

    if (vm_dom != NULL && ka_refcount_mutex_lock(&vm_dom->ref, g_vm_domains_lock)) {
        ka_task_mutex_unlock(g_vm_domains_lock);

        hw_vdavinci_release_vm(vdavinci, vm_dom);

        vm_dom_info_release(&vm_dom->ref);
    }
}

void vdavinci_put_vfio_device(struct hw_vdavinci *vdavinci)
{
    if (KA_IS_ERR_OR_NULL(vdavinci->vdev.vfio_device)) {
        return;
    }
    if (ka_put_vfio_device(vdavinci->vdev.vfio_device)) {
        vdavinci->vdev.vfio_device = NULL;
    }
}

int vdavinci_get_vfio_device(struct hw_vdavinci *vdavinci)
{
    if (!KA_IS_ERR_OR_NULL(vdavinci->vdev.vfio_device)) {
        return 0;
    }
    vdavinci->vdev.vfio_device = ka_get_vfio_device(ka_driver_mdev_dev(vdavinci->vdev.mdev));
    if (KA_IS_ERR_OR_NULL(vdavinci->vdev.vfio_device)) {
        vascend_err(vdavinci_to_dev(vdavinci), "failed to get vfio device");
        return -ENODEV;
    }

    return 0;
}

int vdavinci_rw_gpa(struct hw_vdavinci *vdavinci, unsigned long gpa, void *buf, unsigned long len, bool write)
{
    ka_vfio_gpa info_gpa = {0};

    info_gpa.kvm = vdavinci->vdev.kvm;
    info_gpa.group = vdavinci->vdev.vfio_group;
    info_gpa.device = vdavinci->vdev.vfio_device;

    return ka_vfio_rw_gpa(&info_gpa, gpa, buf, len, write);
}

ka_dentry_t *vdavinci_debugfs_create_dir(const char *name, ka_dentry_t *parent)
{
    ka_dentry_t *node = NULL;

    node = ka_fs_debugfs_create_dir(name, parent);
    if (KA_IS_ERR_OR_NULL(node)) {
        return NULL;
    }

    return node;
}

void vdavinci_debugfs_remove(ka_dentry_t *dentry)
{
    if (dentry == NULL) {
        return;
    }
    ka_fs_debugfs_remove_recursive(dentry);
}

void vdavinci_check(struct hw_vdavinci *vdavinci)
{
    long pin_count = ka_base_atomic64_read(&monitor.pin_count);
    long map_count = ka_base_atomic64_read(&monitor.map_count);

    ka_base_atomic64_set(&monitor.pin_count, 0);
    ka_base_atomic64_set(&monitor.map_count, 0);
    if (pin_count == 0 && map_count == 0) {
        return;
    }
    vascend_warn(vdavinci_to_dev(vdavinci), "check warning: pin count: %ld, map count: %ld\n", pin_count, map_count);
}

bool vdavinci_pfn_is_vpmem(unsigned long pfn)
{
    bool ret = false;
#if IS_VDAVINCI_VPMEM_SUPPORT
    if (pfn_is_vpmem_fn == NULL) {
        return false;
    }
    ret = pfn_is_vpmem_fn(pfn);
#endif
    return ret;
}

bool vdavinci_gfn_is_vpmem(struct hw_vdavinci *vdavinci, unsigned long gfn)
{
    unsigned long pfn = 0;

    if (vdavinci == NULL) {
        return false;
    }
    /* Same as vdavinci_pin_vpmem_pages: called under the sem write side. */
    pfn = g_hw_kvmdt_ops.gfn_to_mfn(vdavinci, gfn);
    if (pfn == KVMDT_ERROR_PFN) {
        return false;
    }

    return vdavinci_pfn_is_vpmem(pfn);
}

int vfio_ops_module_init(void)
{
#if IS_VDAVINCI_VPMEM_SUPPORT
    pfn_is_vpmem_fn = __ka_system_symbol_get("pfn_is_vpmem");
    if (pfn_is_vpmem_fn == NULL) {
        pr_info("vascend: vpmem is not found\n");
    }
#endif
    return 0;
}

void vfio_ops_module_exit(void)
{
#if IS_VDAVINCI_VPMEM_SUPPORT
    if (pfn_is_vpmem_fn == NULL) {
        return;
    }
    ka_system_symbol_put_addr(pfn_is_vpmem_fn);
    pfn_is_vpmem_fn = NULL;
#endif
}