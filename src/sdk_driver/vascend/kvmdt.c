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

#include "ka_compiler_pub.h"
#include "ka_task_pub.h"
#include "ka_kvm_pub.h"
#include "ka_errno_pub.h"
#include "ka_driver_pub.h"
#include "ka_system_pub.h"
#include "ka_pci_pub.h"
#include "hw_vdavinci.h"
#include "dma_pool.h"
#include "log.h"
#include "vfio_ops.h"
#include "dvt_sysfs.h"
#include "dvt.h"
#include "vdavinci.h"
#include "schedule.h"
#include "kvmdt.h"

#define HW_VDAVINCI_READ_SUPPORT_TYPES 3
#define HW_VDAVINCI_WRITE_SUPPORT_TYPES 2

#define HW_VDAVINCI_IRQ_BYPASS_REGISTERED 0

enum {
    IO_REGION_INDEX,
    MEM_REGION_INDEX,
    NUM_REGION_INDEX
};

static unsigned int g_vdavinci_rw_support_size[] = {1, 2, 4, 8};

STATIC void hw_vdavinci_release_work(ka_work_struct_t *work);
STATIC int hw_vdavinci_get_irq_count(struct hw_vdavinci *vdavinci, unsigned int type);

struct vdavinci_priv *kdev_to_davinci(ka_device_t *kdev)
{
    ka_pci_dev_t *pdev;
    pdev = ka_container_of(kdev, ka_pci_dev_t, dev);
    return (struct vdavinci_priv *)ka_pci_get_drvdata(pdev);
}

STATIC struct hw_vdavinci_type *hw_vdavinci_find_available_type(ka_device_t *dev, const char *name)
{
    struct hw_vdavinci_type *type = NULL;
    struct hw_dvt *dvt = kdev_to_davinci(dev)->dvt;

    type = g_hw_vdavinci_ops.dvt_find_vdavinci_type(dvt, name);
    if (type == NULL) {
        vascend_err(dev, "failed to find type: %s to create\n", name);
        return NULL;
    }

    if (type->avail_instance == 0) {
        vascend_err(dev, "insufficient instance of type: %s\n", name);
        return NULL;
    }

    return type;
}

/**
 * check if the driver is in vm mode
 */
STATIC bool hw_vdavinci_check_is_vm_mode(void)
{
    int ret;
    int mode;

    ret = hw_dvt_get_mode(&mode);
    if (ret != 0) {
        ka_dfx_pr_err("hw_dvt_get_mode fail, ret: %d\n", ret);
        return false;
    }

    if (mode == VDAVINCI_VM) {
        return true;
    }

    return false;
}

bool hw_vdavinci_is_enabled(struct hw_dvt *dvt)
{
    if (!hw_vdavinci_check_is_vm_mode()) {
        return false;
    }

    if (hw_vdavinci_sriov_support(dvt) && !dvt->is_sriov_enabled) {
        return false;
    }

    return true;
}

bool hw_vdavinci_priv_callback_check(struct vdavinci_priv *vdavinci_priv)
{
    struct vdavinci_priv_ops *ops = NULL;

    if (vdavinci_priv == NULL) {
        return false;
    }
    if (vdavinci_priv->ops == NULL || vdavinci_priv->dev == NULL) {
        return false;
    }
    ops = vdavinci_priv->ops;
    if (ops->vdavinci_create == NULL || ops->vdavinci_destroy == NULL || ops->vdavinci_release == NULL ||
        ops->vdavinci_reset == NULL || ops->vdavinci_notify == NULL || ops->davinci_getdevnum == NULL ||
        ops->davinci_getdevinfo == NULL) {
        return false;
    }

    return true;
}

STATIC void hw_vdavinci_init_vdev(struct hw_vdavinci *vdavinci, ka_mdev_device_t *mdev)
{
    vdavinci->vdev.data = vdavinci;
    vdavinci->vdev.mdev = mdev;
    vdavinci->vdev.iommu_notify = vdavinci_iommu_notify;
    vdavinci->vdev.group_notify = vdavinci_group_notify;
    vdavinci->vdev.hw_vdev_remove = hw_vdavinci_remove;
    vdavinci->vdev.hw_vdev_open = hw_vdavinci_open;
    vdavinci->vdev.hw_vdev_release = hw_vdavinci_release;
    vdavinci->vdev.hw_vdev_read = hw_vdavinci_read;
    vdavinci->vdev.hw_vdev_write = hw_vdavinci_write;
    vdavinci->vdev.hw_vdev_mmap = hw_vdavinci_mmap;
    vdavinci->vdev.hw_vdev_ioctl = hw_vdavinci_ioctl;
    vdavinci->vdev.vfio_dma_unmap = vdavinci_vfio_dma_unmap;
    KA_TASK_INIT_WORK(&vdavinci->vdev.release_work, hw_vdavinci_release_work);
}

ka_vdev *hw_vdavinci_create(struct kobject *kobj, ka_mdev_device_t *mdev)
{
    int ret = 0;
    struct hw_vdavinci *vdavinci = NULL;
    struct hw_vdavinci_type *type = NULL;
    ka_device_t *pdev;
    struct hw_dvt *dvt;
    struct hw_pf_info *pf_info;
    ka_uuid_le_t uuid = ka_get_uuid(mdev);

    pdev = ka_get_mdev_parent(mdev);
    dvt = kdev_to_davinci(pdev)->dvt;
    if (!hw_vdavinci_is_enabled(dvt)) {
        vascend_err(pdev, "driver is not in vm mode or device's sriov is not enabled\n");
        return NULL;
    }

    ka_task_mutex_lock(&dvt->lock);
    type = hw_vdavinci_find_available_type(pdev, ka_base_kobject_name(kobj));
    if (type == NULL) {
        goto unlock;
    }

    vascend_info(pdev, "enter create vdavinci, type: %s\n", ka_base_kobject_name(kobj));
    vdavinci = g_hw_vdavinci_ops.vdavinci_create(dvt, type, uuid);
    if (KA_IS_ERR_OR_NULL(vdavinci)) {
        ret = vdavinci == NULL ? -EFAULT : KA_PTR_ERR(vdavinci);
        vascend_err(pdev, "failed to create vdavinci: %d\n", ret);
        goto unlock;
    }

    type->vf_num++;

    pf_info = &dvt->pf[vdavinci->dev.dev_index];

    pf_info->reserved_aicpu_num = hw_dvt_get_used_aicpu_num(dvt, pf_info->dev_index);

    pf_info->reserved_aicore_num -= type->aicore_num;
    pf_info->reserved_jpegd_num -= type->jpegd_num;
    pf_info->reserved_mem_size -= type->mem_size;
    pf_info->instance_num++;
    hw_dvt_update_vdavinci_types(dvt, pf_info->dev_index);
    ka_task_mutex_unlock(&dvt->lock);

    vdavinci->vfg_id = type->vfg_id;
    hw_vdavinci_init_vdev(vdavinci, mdev);
    ka_set_mdev_drvdata(ka_driver_mdev_dev(mdev), &vdavinci->vdev);
    vascend_info(pdev, "leave create vdavinci, vid: %u\n", vdavinci->id);
    return &vdavinci->vdev;

unlock:
    ka_task_mutex_unlock(&dvt->lock);
    return NULL;
}

int hw_vdavinci_remove(ka_mdev_device_t *mdev)
{
    struct hw_vdavinci *vdavinci = get_vdavinci_from_mdev(mdev);
    struct hw_dvt *dvt = NULL;
    struct hw_vdavinci_type *type = NULL;
    struct hw_pf_info *pf_info = NULL;
    u32 vid = 0;
    ka_device_t *dev = NULL;

    if (vdavinci == NULL || vdavinci->dvt == NULL || vdavinci->type == NULL || vdavinci->dvt->vdavinci_priv == NULL) {
        return -EINVAL;
    }
    vid = vdavinci->id;
    dvt = vdavinci->dvt;
    type = vdavinci->type;
    pf_info = &dvt->pf[vdavinci->dev.dev_index];
    dev = vdavinci->dvt->vdavinci_priv->dev;

    ka_task_mutex_lock(&dvt->lock);
    /* The active/opened check and vdavinci_destroy must share one critical
     * section: hw_vdavinci_open claims the device under the same lock, so a
     * concurrent open cannot slip in between the check and the destroy (UAF). */
    if (hw_dvt_is_vdavinci_active(vdavinci) || ka_base_atomic_read(&vdavinci->opened) != 0) {
        ka_task_mutex_unlock(&dvt->lock);
        return -EBUSY;
    }
    vascend_info(vdavinci_to_dev(vdavinci), "enter remove vdavinci, vid: %u\n", vid);

    g_hw_vdavinci_ops.vdavinci_destroy(vdavinci);

    type->vf_num--;
    pf_info->reserved_aicpu_num = hw_dvt_get_used_aicpu_num(dvt, pf_info->dev_index);

    pf_info->reserved_aicore_num += type->aicore_num;
    pf_info->reserved_jpegd_num += type->jpegd_num;
    pf_info->reserved_mem_size += type->mem_size;
    pf_info->instance_num--;
    hw_dvt_update_vdavinci_types(dvt, pf_info->dev_index);
    ka_task_mutex_unlock(&dvt->lock);
    vascend_info(dev, "leave remove vdavinci, vid: %u\n", vid);

    return 0;
}

STATIC int hw_vdavinci_open_check(struct hw_vdavinci *vdavinci)
{
    if (vdavinci == NULL || vdavinci->dvt == NULL) {
        return -EINVAL;
    }
    if (!hw_vdavinci_priv_callback_check(vdavinci->dvt->vdavinci_priv)) {
        vascend_err(vdavinci_to_dev(vdavinci), "vdavinci's priv callback is null\n");
        return -EINVAL;
    }
    if (hw_dvt_is_vdavinci_active(vdavinci)) {
        vascend_err(vdavinci_to_dev(vdavinci), "vdavinci is already active, vid: %u\n", vdavinci->id);
        return -EEXIST;
    }

    return 0;
}

/* Claim the device for open, atomic with a concurrent hw_vdavinci_remove.
 * The critical section only wraps the cmpxchg so opens of other vdavincis are
 * not serialized. Caller must ensure vdavinci and vdavinci->dvt are valid. */
STATIC int hw_vdavinci_open_claim(struct hw_vdavinci *vdavinci)
{
    int ret = 0;

    ka_task_mutex_lock(&vdavinci->dvt->lock);
    if (ka_base_atomic_cmpxchg(&vdavinci->opened, 0, 1) != 0) {
        ret = -EBUSY;
    }
    ka_task_mutex_unlock(&vdavinci->dvt->lock);
    if (ret != 0) {
        vascend_err(vdavinci_to_dev(vdavinci), "vdavinci is already opened, vid: %u\n", vdavinci->id);
    }

    return ret;
}

/* Symmetric uninit of hw_vdavinci_open_claim. No dvt->lock needed: the caller
 * is the exclusive owner of the latch (the successful claimer on the open
 * failure path, or the single release guarded by vdev.released) and clears it
 * as its last vdavinci access, so a reader observing 0 can safely proceed.
 * The release store orders all teardown writes before the clear and pairs
 * with the full barrier of the next claim's cmpxchg. */
STATIC void hw_vdavinci_close_claim(struct hw_vdavinci *vdavinci)
{
    ka_base_atomic_set_release(&vdavinci->opened, 0);
}

/* Take the vfio device reference and register the vfio group notifiers.
 * On failure the vfio device reference taken here is released. */
STATIC int hw_vdavinci_open_vfio(struct hw_vdavinci *vdavinci)
{
    int ret = 0;

    ret = vdavinci_get_vfio_device(vdavinci);
    if (ret != 0) {
        return ret;
    }
    ret = vdavinci_register_vfio_group(vdavinci);
    if (ret != 0) {
        /* register_vfio_group rolls the notifiers back itself on failure. */
        vdavinci_put_vfio_device(vdavinci);
    }

    return ret;
}

/* Symmetric uninit of hw_vdavinci_open_vfio. */
STATIC void hw_vdavinci_close_vfio(struct hw_vdavinci *vdavinci)
{
    vdavinci_unregister_vfio_group(vdavinci);
    vdavinci_put_vfio_device(vdavinci);
}

/* Check the KVM handle of the opener and hold a reference to it.
 * The ref is released in __hw_vdavinci_release via hw_vdavinci_close_kvm. */
STATIC int hw_vdavinci_open_kvm(struct hw_vdavinci *vdavinci)
{
    ka_kvm_t *kvm = vdavinci->vdev.kvm;

    if (kvm == NULL || kvm->mm != ka_task_get_current_mm()) {
        vascend_err(vdavinci_to_dev(vdavinci), "KVM is required to use huawei vdavinci, vid: %u\n", vdavinci->id);
        return -ESRCH;
    }
    kvm_get_kvm(kvm);

    return 0;
}

/* Symmetric uninit of hw_vdavinci_open_kvm. */
STATIC void hw_vdavinci_close_kvm(struct hw_vdavinci *vdavinci)
{
    kvm_put_kvm(vdavinci->vdev.kvm);
}

/* Hold a reference to the opener's mm. The ref is released in
 * __hw_vdavinci_release via hw_vdavinci_close_mm. */
STATIC void hw_vdavinci_open_mm(struct hw_vdavinci *vdavinci)
{
    ka_mm_mmgrab(vdavinci->mm);
}

/* Symmetric uninit of hw_vdavinci_open_mm. */
STATIC void hw_vdavinci_close_mm(struct hw_vdavinci *vdavinci)
{
    ka_mm_mmdrop(vdavinci->mm);
    vdavinci->mm = NULL;
}

int hw_vdavinci_open(ka_mdev_device_t *mdev)
{
    struct hw_vdavinci *vdavinci = get_vdavinci_from_mdev(mdev);
    int ret = 0;

    if (vdavinci == NULL || vdavinci->dvt == NULL) {
        return -EINVAL;
    }

    ret = hw_vdavinci_open_claim(vdavinci);
    if (ret != 0) {
        return ret;
    }

    ret = hw_vdavinci_open_check(vdavinci);
    if (ret != 0) {
        goto undo_open;
    }
    vascend_info(vdavinci_to_dev(vdavinci), "enter open vdavinci, vid: %u\n", vdavinci->id);
    vdavinci->qemu_task = ka_task_get_current()->group_leader;
    vdavinci->mm = ka_task_get_current()->mm;
    ka_base_cpumask_clear(&vdavinci->vm_cpus_mask);

    ret = hw_vdavinci_open_vfio(vdavinci);
    if (ret != 0) {
        goto undo_open;
    }

    ret = hw_vdavinci_open_kvm(vdavinci);
    if (ret != 0) {
        goto undo_vfio;
    }
    hw_vdavinci_open_mm(vdavinci);

    ka_base_atomic_set(&vdavinci->vdev.released, 0);
    g_hw_vdavinci_ops.vdavinci_activate(vdavinci);
    vascend_info(vdavinci_to_dev(vdavinci), "leave open vdavinci, vid: %u\n", vdavinci->id);
    return 0;

undo_vfio:
    hw_vdavinci_close_vfio(vdavinci);
undo_open:
    hw_vdavinci_close_claim(vdavinci);
    return ret;
}

STATIC void hw_vdavinci_release_trigger(ka_vdev_pci_irq_ctx *ctx)
{
#if (IS_VDAVINCI_KERNEL_VERSION_SUPPORT || (defined(DRV_UT)))
    struct hw_vdavinci *vdavinci = NULL;
    ka_eventfd_ctx_t *trigger = NULL;
    unsigned long flags = 0;

    if (ctx == NULL) {
        return;
    }
    hw_vdavinci_cancel_work_sync(&ctx->work);
    if (ctx->trigger == NULL) {
        return;
    }
    ka_task_spin_lock_irqsave(&ctx->msix_lock, flags);
    trigger = ctx->trigger;
    ctx->trigger = NULL;
    ka_task_spin_unlock_irqrestore(&ctx->msix_lock, flags);
    if (!ka_irq_bypass_producer_token_is_err(&ctx->producer) && ctx->data != NULL) {
        vdavinci = ctx->data;
        ctx->data = NULL;
        ka_irq_bypass_unregister_producer(&ctx->producer);
        ka_irq_bypass_producer_set_token(&ctx->producer, NULL);
        ctx->producer.irq = -1;
        vascend_info(vdavinci_to_dev(vdavinci), "vdavinci unregister irq success, irq: %d\n", ctx->irq);
    }
    ka_eventfd_ctx_put(trigger);
    ctx->irq = -1;
#endif
}

STATIC void hw_vdavinci_release_msix_triggers(struct hw_vdavinci *vdavinci)
{
    int i, nvec;

    if (vdavinci->vdev.ctx == NULL) {
        return;
    }

    nvec = hw_vdavinci_get_irq_count(vdavinci, VFIO_PCI_MSIX_IRQ_INDEX);

    for (i = 0; i < nvec; i++) {
        hw_vdavinci_release_trigger(&(vdavinci->vdev.ctx[i]));
    }
}

STATIC void hw_vdavinci_free_msix_ctx(struct hw_vdavinci *vdavinci)
{
    ka_vdev_pci_irq_ctx *ctx = NULL;
    int nvec = 0, i = 0;

    ctx = vdavinci->vdev.ctx;
    KA_WRITE_ONCE(vdavinci->vdev.ctx, NULL);
    ka_system_synchronize_rcu();

    if (ctx == NULL) {
        return;
    }

    nvec = vdavinci->msix_nvec;
    for (i = 0; i < nvec; i++) {
        hw_vdavinci_cancel_work_sync(&ctx[i].work);
    }
    ka_mm_kfree(ctx);
}

STATIC void __hw_vdavinci_release(struct hw_vdavinci *vdavinci)
{
    struct vdavinci_ioeventfd *ioeventfd = NULL;
    struct vdavinci_ioeventfd *ioeventfd_tmp = NULL;

    if (vdavinci == NULL) {
        return;
    }
    vascend_info(vdavinci_to_dev(vdavinci), "enter release vdavinci, vid: %u\n", vdavinci->id);
    /* active is set only at the end of a successful open: a release reaching
     * a vdavinci whose open never completed must not run the cleanup chain. */
    if (!hw_dvt_is_vdavinci_active(vdavinci)) {
        return;
    }
    if (ka_base_atomic_cmpxchg(&vdavinci->vdev.released, 0, 1)) {
        return;
    }
    g_hw_vdavinci_ops.vdavinci_release(vdavinci);
    g_hw_vdavinci_ops.vdavinci_deactivate(vdavinci);
    hw_vdavinci_release_msix_triggers(vdavinci);
    /* kvmdt_gfn_to_hva and kvmdt_is_valid_gfn access vdev.kvm in RCU read-side
     * critical sections without taking a kvm reference. Their safety relies on
     * the unconditional synchronize_rcu() inside hw_vdavinci_free_msix_ctx,
     * which drains all RCU readers that saw active != 0. Keep kvm_put_kvm and
     * the teardown below strictly after hw_vdavinci_free_msix_ctx. */
    hw_vdavinci_free_msix_ctx(vdavinci);
    hw_vdavinci_unregister_iodev(vdavinci);
    hw_vdavinci_close_kvm(vdavinci);
    hw_vdavinci_dma_pool_uninit(vdavinci);
    hw_vdavinci_close_vfio(vdavinci);

    hw_vdavinci_close_mm(vdavinci);
    vdavinci->vdev.kvm = NULL;

    ka_list_for_each_entry_safe(ioeventfd, ioeventfd_tmp, &vdavinci->ioeventfds_list, next)
    {
#if IS_VDAVINCI_KERNEL_VERSION_SUPPORT
        hw_vdavinci_ioeventfd_deactive(vdavinci, ioeventfd);
#endif
    }

    /* Allow the next open only after the whole teardown chain completed. */
    hw_vdavinci_close_claim(vdavinci);
    vascend_info(vdavinci_to_dev(vdavinci), "leave release vdavinci, vid: %u\n", vdavinci->id);
}

void hw_vdavinci_release(ka_mdev_device_t *mdev)
{
    struct hw_vdavinci *vdavinci = get_vdavinci_from_mdev(mdev);

    if (vdavinci == NULL) {
        return;
    }
    /* Cancel only from this process-context path: cancel_work_sync from the
     * work handler itself would deadlock waiting for its own completion. */
    hw_vdavinci_cancel_work_sync(&vdavinci->vdev.release_work);

    __hw_vdavinci_release(vdavinci);
}

STATIC void hw_vdavinci_release_work(ka_work_struct_t *work)
{
    struct hw_vdavinci *vdavinci = ka_container_of(work, struct hw_vdavinci, vdev.release_work);

    __hw_vdavinci_release(vdavinci);
}

ssize_t hw_vdavinci_read(ka_mdev_device_t *mdev, char __ka_user *buf, size_t count, loff_t *ppos)
{
    int i = 0;
    size_t done = 0;
    unsigned int size = 0;
    size_t filled;
    u64 val;
    char *pos = buf;
    size_t count_left = count;
    struct hw_vdavinci *vdavinci = get_vdavinci_from_mdev(mdev);

    if (buf == NULL || ppos == NULL) {
        return -EINVAL;
    }
    while (count_left > 0) {
        filled = 0;
        val = 0;

        for (i = HW_VDAVINCI_READ_SUPPORT_TYPES; i >= 0; i--) {
            size = g_vdavinci_rw_support_size[i];
            if (count_left >= size && (*ppos % size) == 0) {
                if (hw_vdavinci_rw(vdavinci, (char *)&val, size, ppos, false) < 0) {
                    return -EFAULT;
                }

                if (ka_base_copy_to_user(pos, &val, size) != 0) {
                    return -EFAULT;
                }

                filled = size;
                break;
            }
        }

        count_left -= filled;
        done += filled;
        *ppos += filled;
        pos += filled;
    }

    return done;
}

size_t hw_vdavinci_write(ka_mdev_device_t *mdev, const char __ka_user *buf, size_t count, loff_t *ppos)
{
    /* The return type stays size_t to match ka_vdev.hw_vdev_write. Error
     * paths return (size_t)-EFAULT: the bit pattern survives the size_t ->
     * ssize_t conversion in ka_hw_vdev_write_ops, so the negative errno is
     * correctly propagated to the caller. */
    int i = 0;
    size_t done = 0;
    unsigned int size = 0;
    size_t filled;
    u64 val;
    const char *pos = buf;
    size_t count_left = count;
    struct hw_vdavinci *vdavinci = get_vdavinci_from_mdev(mdev);

    if (buf == NULL || ppos == NULL) {
        return -EINVAL;
    }
    while (count_left > 0) {
        filled = 0;
        val = 0;

        for (i = HW_VDAVINCI_WRITE_SUPPORT_TYPES; i >= 0; i--) {
            size = g_vdavinci_rw_support_size[i];
            if (count_left >= size && (*ppos % size) == 0) {
                if (ka_base_copy_from_user(&val, pos, size) != 0) {
                    return -EFAULT;
                }

                if (hw_vdavinci_rw(vdavinci, (char *)&val, size, ppos, true) <= 0) {
                    return -EFAULT;
                }

                filled = size;
                break;
            }
        }

        count_left -= filled;
        done += filled;
        *ppos += filled;
        pos += filled;
    }

    return done;
}

STATIC struct vdavinci_bar_map *hw_vdavinci_find_bar_map(struct vdavinci_mapinfo *mmio_map_info, unsigned long offset)
{
    u64 i = 0;
    struct vdavinci_bar_map *map = NULL;

    for (i = 0; i < mmio_map_info->num; i++) {
        map = &mmio_map_info->map_info[i];
        if (map->offset <= offset && offset < map->offset + map->size) {
            return map;
        }
    }

    return NULL;
}

STATIC int hw_vdavinci_mmap_check(const ka_vm_area_struct_t *vma)
{
    unsigned long region_index = 0;

    region_index = vma->vm_pgoff >> (VFIO_PCI_OFFSET_SHIFT - KA_MM_PAGE_SHIFT);
    if (region_index >= VFIO_PCI_ROM_REGION_INDEX) {
        return -EINVAL;
    }
    if ((vma->vm_flags & KA_VM_SHARED) == 0 || vma->vm_end < vma->vm_start) {
        return -EINVAL;
    }

    return 0;
}

STATIC struct vdavinci_mapinfo *hw_vdavinci_get_bar_sparse(struct hw_vdavinci *vdavinci, unsigned long index)
{
    switch (index) {
        case VFIO_PCI_BAR0_REGION_INDEX:
            return &vdavinci->mmio.bar0_sparse;
        case VFIO_PCI_BAR2_REGION_INDEX:
            return &vdavinci->mmio.bar2_sparse;
        case VFIO_PCI_BAR4_REGION_INDEX:
            return &vdavinci->mmio.bar4_sparse;
        default:
            return NULL;
    }
}

STATIC int hw_vdavinci_remap(struct hw_vdavinci *vdavinci, ka_vm_area_struct_t *vma, struct vdavinci_bar_map *map)
{
    int ret = 0;
    unsigned long size = vma->vm_end - vma->vm_start;

    switch (map->map_type) {
        case MAP_TYPE_BACKEND:
            ret = ka_mm_remap_vmalloc_range(vma, map->vaddr, 0);
            break;
        case MAP_TYPE_PASSTHROUGH: {
            if (hw_vdavinci_is_support_normal_nc()) {
                vma->vm_page_prot = ka_mm_pgprot_writecombine(vma->vm_page_prot);
#if IS_VDAVINCI_NORMAL_NC_SUPPORT
                ka_mm_set_vm_flags(vma, ka_mm_get_vm_flags(vma) | VM_ALLOW_ANY_UNCACHED);
#endif
                vascend_info(vdavinci_to_dev(vdavinci), "page's prot set to writecombine\n");
            } else {
                vma->vm_page_prot = ka_mm_pgprot_noncached(vma->vm_page_prot);
            }
            vma->vm_pgoff = map->paddr >> KA_MM_PAGE_SHIFT;
            ret = ka_mm_remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, size, vma->vm_page_prot);
            break;
        }
        default:
            ret = -EINVAL;
            break;
    }
    if (ret != 0) {
        vascend_err(vdavinci_to_dev(vdavinci), "vdavinci remap error, map_type: %d, ret: %d", map->map_type, ret);
    }

    return ret;
}

int hw_vdavinci_mmap(ka_mdev_device_t *mdev, ka_vm_area_struct_t *vma)
{
    int ret = 0;
    unsigned long pgoff = 0, index = 0, size = 0;
    struct hw_vdavinci *vdavinci = get_vdavinci_from_mdev(mdev);
    struct vdavinci_bar_map *map = NULL;
    struct vdavinci_mapinfo *mmio_map_info = NULL;

    if (vma == NULL || vdavinci == NULL) {
        return -EINVAL;
    }
    ret = hw_vdavinci_mmap_check(vma);
    if (ret != 0) {
        return ret;
    }

    index = vma->vm_pgoff >> (VFIO_PCI_OFFSET_SHIFT - KA_MM_PAGE_SHIFT);
    pgoff = vma->vm_pgoff & ((1U << (VFIO_PCI_OFFSET_SHIFT - KA_MM_PAGE_SHIFT)) - 1);

    mmio_map_info = hw_vdavinci_get_bar_sparse(vdavinci, index);
    if (mmio_map_info == NULL) {
        return -EINVAL;
    }
    size = vma->vm_end - vma->vm_start;
    map = hw_vdavinci_find_bar_map(mmio_map_info, pgoff << KA_MM_PAGE_SHIFT);
    if (map == NULL || map->size == 0 || size != map->size) {
        vascend_err(vdavinci_to_dev(vdavinci), "find no bar map for pgoff:0x%lx\n", pgoff);
        return -EINVAL;
    }
    ret = hw_vdavinci_remap(vdavinci, vma, map);
    if (ret != 0) {
        vascend_err(vdavinci_to_dev(vdavinci), "vdavinci mmap error, index: %lu, ret: %d\n", index, ret);
    }

    return ret;
}

STATIC int hw_vdavinci_get_irq_count(struct hw_vdavinci *vdavinci, unsigned int type)
{
    u16 flags = 0;
    const unsigned int byte_count = 2;

    if (type == VFIO_PCI_INTX_IRQ_INDEX) {
        return 1;
    } else if (type == VFIO_PCI_MSIX_IRQ_INDEX) {
        g_hw_vdavinci_ops.emulate_cfg_read(vdavinci, DAVINCI_PCI_MSIX_FLAGS, &flags, byte_count);
        return (flags & KA_PCI_MSIX_FLAGS_QSIZE) + 1;
    } else if (type == VFIO_PCI_MSI_IRQ_INDEX || type == VFIO_PCI_ERR_IRQ_INDEX || type == VFIO_PCI_REQ_IRQ_INDEX) {
        return 0;
    }

    return 0;
}

STATIC int hw_vdavinci_set_intx_mask(struct hw_vdavinci *vdavinci, const ka_vfio_irq_set_t *hdr, void *data)
{
    return 0;
}

STATIC int hw_vdavinci_set_intx_unmask(struct hw_vdavinci *vdavinci, const ka_vfio_irq_set_t *hdr, void *data)
{
    return 0;
}

STATIC int hw_vdavinci_set_intx_trigger(struct hw_vdavinci *vdavinci, const ka_vfio_irq_set_t *hdr, void *data)
{
    return 0;
}

static void hw_vdavinci_put_msix_trigger(struct hw_vdavinci *vdavinci, unsigned int end, unsigned int start)
{
    unsigned int i;

    if (vdavinci == NULL || vdavinci->vdev.ctx == NULL) {
        return;
    }
    for (i = start; i < end; i++) {
        hw_vdavinci_release_trigger(&(vdavinci->vdev.ctx[i]));
    }
}

STATIC void vdavinci_irq_work_handler(ka_work_struct_t *work)
{
#if (IS_VDAVINCI_KERNEL_VERSION_SUPPORT || (defined(DRV_UT)))
    int ret = 0;
    struct hw_vdavinci *vdavinci = NULL;
    ka_vdev_pci_irq_ctx *ctx = ka_container_of(work, ka_vdev_pci_irq_ctx, work);

    if (ctx == NULL || ctx->data == NULL || ctx->trigger == NULL) {
        return;
    }
    vdavinci = ctx->data;
    if (ctx->irq <= 0 || ctx->irq >= nr_irqs) {
        ka_irq_bypass_producer_set_token(&ctx->producer, KA_ERR_PTR(-EINVAL));
        vascend_warn(vdavinci_to_dev(vdavinci), "illegal irq: %d\n", ctx->irq);
        return;
    }
    ka_irq_bypass_producer_set_token(&ctx->producer, ctx->trigger);
    ctx->producer.irq = ctx->irq;
    ret = ka_irq_bypass_register_producer(&ctx->producer);
    if (ret != 0) {
        ka_irq_bypass_producer_set_token(&ctx->producer, KA_ERR_PTR(ret));
        vascend_warn(vdavinci_to_dev(vdavinci), "vdavinci register irq error: %d, irq: %d\n", ret, ctx->irq);
        return;
    }

    vascend_info(vdavinci_to_dev(vdavinci), "vdavinci register irq success, irq: %d\n", ctx->irq);
#endif
}

STATIC int hw_vdavinci_check_msix(struct hw_vdavinci *vdavinci, unsigned int start, unsigned int count)
{
    unsigned int nnvec = 0;
    int nvec = 0, i = 0;

    nvec = hw_vdavinci_get_irq_count(vdavinci, VFIO_PCI_MSIX_IRQ_INDEX);
    nnvec = (unsigned int)nvec;

    if (start >= nnvec || start + count > nnvec || start > nnvec - count) {
        return -EINVAL;
    }
    if (vdavinci->msix_nvec != 0 && vdavinci->msix_nvec != nvec) {
        return -EINVAL;
    }
    if (vdavinci->vdev.ctx == NULL) {
        vdavinci->vdev.ctx = ka_mm_kcalloc(nvec, sizeof(ka_vdev_pci_irq_ctx), KA_GFP_KERNEL);
        if (vdavinci->vdev.ctx == NULL) {
            return -ENOMEM;
        }
        for (i = 0; i < nvec; i++) {
            ka_task_spin_lock_init(&vdavinci->vdev.ctx[i].msix_lock);
            KA_TASK_INIT_WORK(&vdavinci->vdev.ctx[i].work, vdavinci_irq_work_handler);
        }
    }
    vdavinci->msix_nvec = nvec;

    /* msix_count is for debugfs statistics only: an allocation failure must
     * not block the msix eventfd setup, so degrade to a warning here. */
    if (!vdavinci->debugfs.msix_count) {
        vdavinci->debugfs.msix_count = ka_mm_kcalloc(nvec, sizeof(unsigned long long), KA_GFP_KERNEL);
        if (!vdavinci->debugfs.msix_count) {
            vascend_warn(vdavinci_to_dev(vdavinci), "msix_count is unavailable for debugfs. (nvec=%d)\n", nvec);
        }
    }

    return 0;
}

STATIC int hw_vdavinci_set_msix_trigger(struct hw_vdavinci *vdavinci, const ka_vfio_irq_set_t *hdr, void *data)
{
    int fd, ret;
    unsigned int i = 0, j = 0, start = hdr->start, count = hdr->count;
    u32 flags = hdr->flags;
    unsigned long msix_flags = 0;

    ret = hw_vdavinci_check_msix(vdavinci, start, count);
    if (ret) {
        return ret;
    }

    if ((flags & KA_VFIO_IRQ_SET_DATA_EVENTFD) != 0) {
        u_int32_t *fds = data;
        ka_eventfd_ctx_t *trigger = NULL;

        for (i = start, j = 0; i < start + count; i++, j++) {
            hw_vdavinci_release_trigger(&(vdavinci->vdev.ctx[i]));

            fd = fds ? (int)fds[j] : -1;
            if (fd < 0) {
                continue;
            }

            trigger = ka_eventfd_ctx_fdget(fd);
            if (KA_IS_ERR(trigger)) {
                vascend_err(vdavinci_to_dev(vdavinci),
                            "eventfd_ctx_fdget_failed, "
                            "vid: %u, vector: %u 's eventfd can't be %d\n",
                            vdavinci->id, i, fd);
                ret = KA_PTR_ERR(trigger);
                goto release_eventfd;
            }
            ka_task_spin_lock_irqsave(&vdavinci->vdev.ctx[i].msix_lock, msix_flags);
            vdavinci->vdev.ctx[i].trigger = trigger;
            ka_task_spin_unlock_irqrestore(&vdavinci->vdev.ctx[i].msix_lock, msix_flags);
            vdavinci->vdev.ctx[i].flags = KA_BASE_BIT(HW_VDAVINCI_IRQ_BYPASS_REGISTERED);
        }
    } else if ((flags & KA_VFIO_IRQ_SET_DATA_NONE) != 0 && count == 0) {
        hw_vdavinci_release_msix_triggers(vdavinci);
    }

    return 0;

release_eventfd:
    hw_vdavinci_put_msix_trigger(vdavinci, i, start);

    return ret;
}

STATIC int hw_vdavinci_set_irqs(struct hw_vdavinci *vdavinci, ka_vfio_irq_set_t *hdr, void *data)
{
    int (*func)(struct hw_vdavinci *vdavinci, const ka_vfio_irq_set_t *hdr, void *data) = NULL;

    switch (hdr->index) {
        case VFIO_PCI_INTX_IRQ_INDEX:
            switch (hdr->flags & KA_VFIO_IRQ_SET_ACTION_TYPE_MASK) {
                case KA_VFIO_IRQ_SET_ACTION_MASK:
                    func = hw_vdavinci_set_intx_mask;
                    break;
                case KA_VFIO_IRQ_SET_ACTION_UNMASK:
                    func = hw_vdavinci_set_intx_unmask;
                    break;
                case KA_VFIO_IRQ_SET_ACTION_TRIGGER:
                    func = hw_vdavinci_set_intx_trigger;
                    break;
                default:
                    return -ENOTTY;
            }
            break;
        case VFIO_PCI_MSIX_IRQ_INDEX:
            switch (hdr->flags & KA_VFIO_IRQ_SET_ACTION_TYPE_MASK) {
                case KA_VFIO_IRQ_SET_ACTION_TRIGGER:
                    func = hw_vdavinci_set_msix_trigger;
                    break;
                default:
                    return -ENOTTY;
            }
            break;
        default:
            return -ENOTTY;
    }

    if (func == NULL) {
        return -ENOTTY;
    }

    return func(vdavinci, hdr, data);
}

STATIC long _hw_vdavinci_device_get_info(uintptr_t arg, struct hw_vdavinci *vdavinci)
{
    ka_vfio_device_info_t info;
    unsigned long minsz = ka_offsetofend(ka_vfio_device_info_t, num_irqs);

    if (!arg) {
        return -EINVAL;
    }

    if (ka_base_copy_from_user(&info, (void __ka_user *)arg, minsz) != 0) {
        return -EFAULT;
    }

    if (info.argsz < minsz) {
        return -EINVAL;
    }

    info.flags = KA_VFIO_DEVICE_FLAGS_PCI;
    info.flags |= KA_VFIO_DEVICE_FLAGS_RESET;
    info.num_irqs = VFIO_PCI_NUM_IRQS;
    info.num_regions = VFIO_PCI_NUM_REGIONS + vdavinci->vdev.num_regions;

    if (ka_base_copy_to_user((void __ka_user *)arg, &info, minsz) != 0) {
        return -EFAULT;
    }

    return 0;
}

STATIC int _hw_vdavinci_device_get_cap(int cap_type_id, ka_vfio_region_info_cap_sparse_mmap_t *sparse,
                                       ka_vfio_region_info_t *info, uintptr_t arg)
{
    int ret = 0;
#if IS_VDAVINCI_KERNEL_VERSION_SUPPORT
    size_t size = 0;
    ka_vfio_info_cap_t caps = {.buf = NULL, .size = 0};

    if (cap_type_id != KA_VFIO_REGION_INFO_CAP_SPARSE_MMAP || sparse == NULL) {
        return -EINVAL;
    }
    size = sizeof(*sparse) + (sparse->nr_areas * sizeof(*sparse->areas));
    ret = ka_driver_vfio_info_add_capability(&caps, &sparse->header, size);
    if (ret != 0) {
        return ret;
    }
    if (caps.size == 0) {
        goto out;
    }
    info->flags |= KA_VFIO_REGION_INFO_FLAG_CAPS;
    if (info->argsz < sizeof(*info) + caps.size) {
        info->argsz = sizeof(*info) + caps.size;
        info->cap_offset = 0;
        goto out;
    }
    ka_driver_vfio_info_cap_shift(&caps, sizeof(*info));
    if (ka_base_copy_to_user((void __ka_user *)(arg + sizeof(*info)), caps.buf, caps.size) != 0) {
        ret = -EFAULT;
        goto out;
    }
    info->cap_offset = sizeof(*info);
out:
    ka_mm_kfree(caps.buf);
#endif
    return ret;
}

STATIC ka_vfio_region_info_cap_sparse_mmap_t *hw_vdavinci_device_get_sparse_info(struct vdavinci_mapinfo *mmio_map_info,
                                                                                 unsigned int map_num)
{
    ka_vfio_region_info_cap_sparse_mmap_t *sparse = NULL;
    u64 i = 0, j = 0;
    unsigned int nr_areas = map_num;
    struct vdavinci_bar_map *map;

    if (nr_areas == 0) {
        return NULL;
    }

    sparse = ka_mm_kzalloc(sizeof(*sparse) + sizeof(sparse->areas[0]) * nr_areas, KA_GFP_KERNEL);
    if (sparse == NULL) {
        return NULL;
    }

    sparse->header.id = KA_VFIO_REGION_INFO_CAP_SPARSE_MMAP;
    sparse->header.version = 1;
    sparse->nr_areas = nr_areas;

    for (i = 0; i < mmio_map_info->num; i++) {
        map = &mmio_map_info->map_info[i];
        if (map->map_type == MAP_TYPE_BACKEND || map->map_type == MAP_TYPE_PASSTHROUGH) {
            sparse->areas[j].offset = map->offset;
            sparse->areas[j].size = map->size;
            j++;
        }
    }

    return sparse;
}

STATIC long hw_vdavinci_device_get_bar_info(struct hw_vdavinci *vdavinci, ka_vfio_region_info_t *info)
{
    switch (info->index) {
        case VFIO_PCI_CONFIG_REGION_INDEX:
            info->offset = VFIO_PCI_INDEX_TO_OFFSET(info->index);
            info->size = vdavinci->dvt->device_info.cfg_space_size;
            info->flags = KA_VFIO_REGION_INFO_FLAG_READ | KA_VFIO_REGION_INFO_FLAG_WRITE;
            break;
        case VFIO_PCI_BAR0_REGION_INDEX:
        case VFIO_PCI_BAR2_REGION_INDEX:
        case VFIO_PCI_BAR4_REGION_INDEX:
            info->offset = VFIO_PCI_INDEX_TO_OFFSET(info->index);
            info->size = vdavinci->cfg_space.bar[info->index].size;
            info->flags = KA_VFIO_REGION_INFO_FLAG_READ | KA_VFIO_REGION_INFO_FLAG_WRITE;
            break;
        case VFIO_PCI_BAR1_REGION_INDEX:
        case VFIO_PCI_BAR3_REGION_INDEX:
        case VFIO_PCI_BAR5_REGION_INDEX:
        case VFIO_PCI_ROM_REGION_INDEX:
        case VFIO_PCI_VGA_REGION_INDEX:
            info->offset = VFIO_PCI_INDEX_TO_OFFSET(info->index);
            info->size = 0;
            info->flags = 0;
            break;
        default:
            return -EINVAL;
    }

    return 0;
}

STATIC int hw_vdavinci_get_vfio_region_info(uintptr_t arg, ka_vfio_region_info_t *info,
                                            struct vdavinci_mapinfo *mmio_map_info)
{
    u64 i = 0;
    int ret = 0, cap_type_id = 0;
    unsigned int map_num = 0;
    struct vdavinci_bar_map *map = NULL;
    ka_vfio_region_info_cap_sparse_mmap_t *sparse = NULL;

    for (i = 0; i < mmio_map_info->num; i++) {
        map = &mmio_map_info->map_info[i];
        if (map->map_type == MAP_TYPE_BACKEND || map->map_type == MAP_TYPE_PASSTHROUGH) {
            map_num++;
        }
    }

    if (map_num > 0) {
        info->flags = info->flags | KA_VFIO_REGION_INFO_FLAG_MMAP;
        sparse = hw_vdavinci_device_get_sparse_info(mmio_map_info, map_num);
        if (sparse) {
            cap_type_id = KA_VFIO_REGION_INFO_CAP_SPARSE_MMAP;
            info->flags = info->flags | KA_VFIO_REGION_INFO_FLAG_CAPS;
        }
    }

    if ((info->flags & KA_VFIO_REGION_INFO_FLAG_CAPS) != 0) {
        ret = _hw_vdavinci_device_get_cap(cap_type_id, sparse, info, arg);
    }

    if (sparse) {
        ka_mm_kfree(sparse);
    }

    return ret;
}

STATIC long _hw_vdavinci_device_get_region_info(uintptr_t arg, struct hw_vdavinci *vdavinci)
{
    int ret = 0;
    ka_vfio_region_info_t info;
    struct vdavinci_mapinfo *mmio_map_info = NULL;
    unsigned long minsz = ka_offsetofend(ka_vfio_region_info_t, offset);

    if (!arg) {
        return -EINVAL;
    }

    if (ka_base_copy_from_user(&info, (void __ka_user *)arg, minsz) != 0) {
        return -EFAULT;
    }

    if (info.argsz < minsz) {
        return -EINVAL;
    }

    if (hw_vdavinci_device_get_bar_info(vdavinci, &info)) {
        return -EINVAL;
    }

    mmio_map_info = hw_vdavinci_get_bar_sparse(vdavinci, info.index);
    if (!mmio_map_info) {
        return ka_base_copy_to_user((void __ka_user *)arg, &info, minsz) != 0 ? -EFAULT : 0;
    }

    ret = hw_vdavinci_get_vfio_region_info(arg, &info, mmio_map_info);
    if (ret) {
        return ret;
    }

    return ka_base_copy_to_user((void __ka_user *)arg, &info, minsz) ? -EFAULT : 0;
}

STATIC long _hw_vdavinci_device_get_irq_info(uintptr_t arg, struct hw_vdavinci *vdavinci)
{
    ka_vfio_irq_info_t info;
    u32 index = 0;
    unsigned long minsz = ka_offsetofend(ka_vfio_irq_info_t, count);

    if (!arg) {
        return -EINVAL;
    }

    if (ka_base_copy_from_user(&info, (void __ka_user *)arg, minsz) != 0) {
        return -EFAULT;
    }
    index = info.index;
    if (info.argsz < minsz || index >= VFIO_PCI_NUM_IRQS) {
        return -EINVAL;
    }

    if (index != VFIO_PCI_INTX_IRQ_INDEX && index != VFIO_PCI_MSIX_IRQ_INDEX) {
        return -EINVAL;
    }

    info.flags = KA_VFIO_IRQ_INFO_EVENTFD;
    info.count = (unsigned int)hw_vdavinci_get_irq_count(vdavinci, index);

    if (index == VFIO_PCI_INTX_IRQ_INDEX) {
        info.flags |= (VFIO_IRQ_INFO_MASKABLE | KA_VFIO_IRQ_INFO_AUTOMASKED);
    } else {
        info.flags |= KA_VFIO_IRQ_INFO_NORESIZE;
    }

    if (ka_base_copy_to_user((void __ka_user *)arg, &info, minsz) != 0) {
        return -EFAULT;
    }

    return 0;
}

#if (IS_VDAVINCI_KERNEL_VERSION_SUPPORT || (defined(DRV_UT)))
STATIC long _hw_vdavinci_device_set_ioeventfd(uintptr_t arg, struct hw_vdavinci *vdavinci)
{
    ka_vfio_device_ioeventfd_t efd;
    unsigned long minsz = ka_offsetofend(ka_vfio_device_ioeventfd_t, fd);
    int count = 0;

    if (!arg) {
        return -EINVAL;
    }

    if (ka_base_copy_from_user(&efd, (void __ka_user *)arg, minsz)) {
        return -EFAULT;
    }

    if (efd.argsz < minsz || (efd.flags & ~KA_VFIO_DEVICE_IOEVENTFD_SIZE_MASK)) {
        return -EINVAL;
    }

    count = efd.flags & KA_VFIO_DEVICE_IOEVENTFD_SIZE_MASK;

    if (ka_hweight8(count) != 1 || efd.fd < -1) {
        return -EINVAL;
    }

    return hw_vdavinci_set_ioeventfd(vdavinci, efd.offset, efd.data, count, efd.fd);
}
#endif

STATIC long _hw_vdavinci_device_set_irqs(uintptr_t arg, struct hw_vdavinci *vdavinci)
{
    ka_vfio_irq_set_t hdr;
    unsigned long minsz = ka_offsetofend(ka_vfio_irq_set_t, count);
    size_t data_size = 0;
    u8 *data = NULL;
    int ret = 0, max = 0;

    if (!arg) {
        return -EINVAL;
    }

    if (ka_base_copy_from_user(&hdr, (void __ka_user *)arg, minsz) != 0) {
        return -EFAULT;
    }

    if ((hdr.flags & KA_VFIO_IRQ_SET_DATA_NONE) == 0) {
        max = hw_vdavinci_get_irq_count(vdavinci, hdr.index);

        ret = ka_driver_vfio_set_irqs_validate_and_prepare(&hdr, max, VFIO_PCI_NUM_IRQS, &data_size);
        if (ret != 0) {
            return -EINVAL;
        }
        if (data_size != 0) {
            data = ka_mm_memdup_user((void __ka_user *)(arg + minsz), data_size);
            if (KA_IS_ERR(data)) {
                return KA_PTR_ERR(data);
            }
        }
    }

    ka_task_mutex_lock(&vdavinci->vdavinci_lock);
    ret = hw_vdavinci_set_irqs(vdavinci, &hdr, data);
    ka_task_mutex_unlock(&vdavinci->vdavinci_lock);

    ka_mm_kfree(data);

    return ret;
}

STATIC long _hw_vdavinci_device_reset(struct hw_vdavinci *vdavinci)
{
    return g_hw_vdavinci_ops.vdavinci_reset(vdavinci);
}

long hw_vdavinci_ioctl(ka_mdev_device_t *mdev, unsigned int cmd, unsigned long arg)
{
    struct hw_vdavinci *vdavinci = get_vdavinci_from_mdev(mdev);
    uintptr_t arg_uptr = arg;

    if (!vdavinci) {
        return -EINVAL;
    }

    switch (cmd) {
        case KA_VFIO_DEVICE_GET_INFO:
            return _hw_vdavinci_device_get_info(arg_uptr, vdavinci);
        case KA_VFIO_DEVICE_GET_REGION_INFO:
            return _hw_vdavinci_device_get_region_info(arg_uptr, vdavinci);
        case KA_VFIO_DEVICE_GET_IRQ_INFO:
            return _hw_vdavinci_device_get_irq_info(arg_uptr, vdavinci);
        case KA_VFIO_DEVICE_SET_IRQS:
            return _hw_vdavinci_device_set_irqs(arg_uptr, vdavinci);
        case KA_VFIO_DEVICE_RESET:
            return _hw_vdavinci_device_reset(vdavinci);
#if ((IS_VDAVINCI_KERNEL_VERSION_SUPPORT) || (defined(DRV_UT)))
        case KA_VFIO_DEVICE_IOEVENTFD:
            return _hw_vdavinci_device_set_ioeventfd(arg_uptr, vdavinci);
#endif
        default:
            return -ENOTTY;
    }
}

STATIC int kvmdt_register_mdev(ka_device_t *dev, struct hw_dvt *dvt)
{
    dvt->dvt_dev.drv = &ka_hw_vdev_mdev_driver;
    return vdavinci_register_device(dev, &dvt->dvt_dev, VDAVINCI_NAME);
}

STATIC void kvmdt_unregister_mdev(ka_device_t *dev, struct hw_dvt *dvt)
{
    vdavinci_unregister_device(dev, &dvt->dvt_dev);
    dvt->dvt_dev.drv = NULL;
}

STATIC int kvmdt_inject_msix(struct hw_vdavinci *vdavinci, u32 vector, int irq)
{
    unsigned long flags = 0;

    if (ka_unlikely(vdavinci == NULL || !hw_dvt_is_vdavinci_active(vdavinci))) {
        return -ESRCH;
    }
    if (ka_unlikely(vector >= (u32)vdavinci->msix_nvec)) {
        vascend_err(vdavinci_to_dev(vdavinci),
                    "inject msix failed, "
                    "wrong msix data: %d, vid: %u\n",
                    vector, vdavinci->id);
        return -EINVAL;
    }
    if (ka_unlikely(vdavinci->vdev.ctx == NULL || vdavinci->vdev.ctx[vector].trigger == NULL)) {
        return 0;
    }
    if (irq > 0 && ka_test_and_clear_bit(HW_VDAVINCI_IRQ_BYPASS_REGISTERED, &vdavinci->vdev.ctx[vector].flags)) {
        vdavinci->vdev.ctx[vector].irq = irq;
        vdavinci->vdev.ctx[vector].data = vdavinci;
        ka_task_schedule_work(&vdavinci->vdev.ctx[vector].work);
    }
    ka_task_spin_lock_irqsave(&vdavinci->vdev.ctx[vector].msix_lock, flags);
    if (ka_likely(vdavinci->vdev.ctx[vector].trigger != NULL)) {
        if (ka_likely(ka_eventfd_signal(vdavinci->vdev.ctx[vector].trigger, 1)) != 0 &&
            vdavinci->debugfs.msix_count != NULL) {
            vdavinci->debugfs.msix_count[vector]++;
        }
    }
    ka_task_spin_unlock_irqrestore(&vdavinci->vdev.ctx[vector].msix_lock, flags);

    return 0;
}

STATIC int kvmdt_rw_gpa_common(struct hw_vdavinci *vdavinci, unsigned long gpa, void *buf, unsigned long len,
                               bool write)
{
    int ret = 0;

    if (vdavinci == NULL) {
        return -ESRCH;
    }
    ka_task_mutex_lock(&vdavinci->vdavinci_lock);
    if (!hw_dvt_is_vdavinci_active(vdavinci)) {
        ka_task_mutex_unlock(&vdavinci->vdavinci_lock);
        return -ESRCH;
    }
    ret = vdavinci_rw_gpa(vdavinci, gpa, buf, len, write);
    ka_task_mutex_unlock(&vdavinci->vdavinci_lock);
    return ret;
}

STATIC int kvmdt_read_gpa(struct hw_vdavinci *vdavinci, unsigned long gpa, void *buf, unsigned long len)
{
    return kvmdt_rw_gpa_common(vdavinci, gpa, buf, len, false);
}

STATIC int kvmdt_write_gpa(struct hw_vdavinci *vdavinci, unsigned long gpa, void *buf, unsigned long len)
{
    return kvmdt_rw_gpa_common(vdavinci, gpa, buf, len, true);
}

#if (IS_VDAVINCI_KERNEL_VERSION_SUPPORT || (defined(DRV_UT)))
STATIC unsigned long kvmdt_gfn_to_mfn_2m(struct page_info_list *dma_page_list, unsigned long gfn)
{
    struct page_info_entry *dma_page_info = NULL;

    if (ka_unlikely(dma_page_list == NULL || ka_list_empty(&dma_page_list->head))) {
        return KVMDT_ERROR_PFN;
    }
    ka_list_for_each_entry(dma_page_info, &(dma_page_list->head), list)
    {
        if (gfn >= dma_page_info->gfn && gfn < dma_page_info->gfn + dma_page_info->length) {
            return ka_mm_page_to_pfn(dma_page_info->page) + gfn - dma_page_info->gfn;
        }
    }

    return KVMDT_ERROR_PFN;
}

STATIC unsigned long kvmdt_gfn_to_mfn_range(struct hw_vdavinci *vdavinci, unsigned long gfn)
{
    const int cut = 2;
    unsigned long size = 0;
    struct vm_dom_info *vm_dom;
    struct ram_range_info *ram_info = NULL;
    struct dma_info_2m *node = NULL;
    int left = 0, right = 0, mid = 0;

    if (ka_unlikely(vdavinci == NULL)) {
        return KVMDT_ERROR_PFN;
    }
    vm_dom = (struct vm_dom_info *)vdavinci->vdev.domain;
    if (ka_unlikely(vm_dom == NULL)) {
        return KVMDT_ERROR_PFN;
    }
    ram_info = get_ram_range_by_gfn(vm_dom, gfn, KA_MM_PAGE_SIZE);
    if (ka_unlikely(ram_info == NULL)) {
        return KVMDT_ERROR_PFN;
    }
    left = 0;
    right = ram_info->dma_array_len - 1;
    while (left <= right) {
        mid = left + (right - left) / cut;
        node = ram_info->dma_array[mid];
        size = node->size / KA_MM_PAGE_SIZE;

        if (gfn < node->gfn) {
            right = mid - 1;
        } else if (gfn >= node->gfn + size) {
            left = mid + 1;
        } else {
            return kvmdt_gfn_to_mfn_2m(&(node->dma_page_list), gfn);
        }
    }

    return KVMDT_ERROR_PFN;
}
#endif

/* Acquire RCU read lock and return kvm pointer if vdavinci is active.
 * Caller must call ka_task_rcu_read_unlock() after use. */
STATIC ka_kvm_t *kvmdt_kvm_rcu_lock(struct hw_vdavinci *vdavinci)
{
    if (vdavinci == NULL) {
        return NULL;
    }

    ka_task_rcu_read_lock();
    if (!hw_dvt_is_vdavinci_active(vdavinci) || vdavinci->vdev.kvm == NULL) {
        ka_task_rcu_read_unlock();
        return NULL;
    }
    return vdavinci->vdev.kvm;
}

STATIC unsigned long kvmdt_gfn_to_mfn(struct hw_vdavinci *vdavinci, unsigned long gfn)
{
    unsigned long pfn = KVMDT_ERROR_PFN;
#if (IS_VDAVINCI_KERNEL_VERSION_SUPPORT || (defined(DRV_UT)))
    bool kthread = (ka_task_get_current_mm() == NULL);
    ka_kvm_t *kvm = kvmdt_kvm_rcu_lock(vdavinci);

    if (kvm == NULL) {
        return KVMDT_ERROR_PFN;
    }
    kvm_get_kvm(kvm);
    pfn = kvmdt_gfn_to_mfn_range(vdavinci, gfn);
    ka_task_rcu_read_unlock();

    if (ka_likely(pfn != KVMDT_ERROR_PFN)) {
        kvm_put_kvm(kvm);
        return pfn;
    }

    ka_task_might_sleep();
    if (kthread) {
        if (!ka_mm_mmget_not_zero(kvm->mm)) {
            kvm_put_kvm(kvm);
            return KVMDT_ERROR_PFN;
        }
        ka_use_mm(kvm->mm);
    }

    pfn = ka_gfn_to_pfn(kvm, gfn);

    if (kthread) {
        ka_unuse_mm(kvm->mm);
        ka_mm_mmput(kvm->mm);
    }

    kvm_put_kvm(kvm);

    if (ka_is_error_noslot_pfn(pfn)) {
        return KVMDT_ERROR_PFN;
    }
    ka_kvm_release_pfn_clean(pfn);
#endif
    return pfn;
}

unsigned long kvmdt_gfn_to_hva(struct hw_vdavinci *vdavinci, unsigned long gfn)
{
    unsigned long hva = KVMDT_ERROR_PFN;
#if (IS_VDAVINCI_KERNEL_VERSION_SUPPORT || (defined(DRV_UT)))
    bool kthread = (ka_task_get_current_mm() == NULL);
    ka_kvm_t *kvm = kvmdt_kvm_rcu_lock(vdavinci);

    if (kvm == NULL) {
        return KVMDT_ERROR_PFN;
    }
    if (kthread) {
        if (!ka_mm_mmget_not_zero(kvm->mm)) {
            ka_task_rcu_read_unlock();
            return KVMDT_ERROR_PFN;
        }
        ka_use_mm(kvm->mm);
    }
    hva = ka_gfn_to_hva(kvm, gfn);
    if (kthread) {
        ka_unuse_mm(kvm->mm);
        /* Safe in RCU read-side: kvm is alive (protected by synchronize_rcu in release),
         * so kvm holds an mm refcount and mmput will not reach zero here. */
        ka_mm_mmput(kvm->mm);
    }
    ka_task_rcu_read_unlock();

    if (ka_kvm_is_error_hva(hva)) {
        return KVMDT_ERROR_PFN;
    }
#endif
    return hva;
}

void hw_vdavinci_release_vm(struct hw_vdavinci *vdavinci, struct vm_dom_info *vm_dom)
{
    hw_vdavinci_unpin_pages(vdavinci, vm_dom);
    vdavinci_check(vdavinci);
}

STATIC bool kvmdt_is_valid_gfn(struct hw_vdavinci *vdavinci, unsigned long gfn)
{
    bool ret = false;
    int idx;
    ka_kvm_t *kvm = NULL;

    if (vdavinci == NULL || !hw_dvt_is_vdavinci_active(vdavinci) || vdavinci->vdev.kvm == NULL) {
        return false;
    }
    kvm = vdavinci->vdev.kvm;

    idx = ka_task_srcu_read_lock(&kvm->srcu);
    ret = ka_kvm_is_visible_gfn(kvm, gfn);
    ka_task_srcu_read_unlock(&kvm->srcu, idx);

    return ret;
}

STATIC int kvmdt_get_mmio_info(void **dst, int *size, void *_vdavinci, int bar)
{
    struct hw_vdavinci *vdavinci = (struct hw_vdavinci *)_vdavinci;
    struct hw_dvt *dvt = vdavinci->dvt;

    if (bar == IO_REGION_INDEX) {
        *dst = vdavinci->mmio.io_base;
        *size = vdavinci->type->bar2_size;
        if (hw_vdavinci_sriov_support(dvt)) {
            *size = VF_BAR0_VPC_SIZE;
        }
    } else if (bar == MEM_REGION_INDEX) {
        *dst = vdavinci->mmio.mem_base;
        *size = vdavinci->type->bar4_size;
        if (hw_vdavinci_sriov_support(dvt)) {
            *size = VF_BAR0_DVPP_SIZE;
        }
    } else {
        return -EINVAL;
    }

    return 0;
}

struct hw_kvmdt_ops g_hw_kvmdt_ops = {
    .register_mdev = kvmdt_register_mdev,
    .unregister_mdev = kvmdt_unregister_mdev,
    .inject_msix = kvmdt_inject_msix,
    .read_gpa = kvmdt_read_gpa,
    .write_gpa = kvmdt_write_gpa,
    .gfn_to_mfn = kvmdt_gfn_to_mfn,
    .is_valid_gfn = kvmdt_is_valid_gfn,
    .mmio_get = kvmdt_get_mmio_info,
    .dma_pool_init = hw_vdavinci_dma_pool_init,
    .dma_pool_uninit = hw_vdavinci_dma_pool_uninit,
    .dma_get_iova = hw_vdavinci_get_iova,
    .dma_put_iova = hw_vdavinci_put_iova,
    .dma_get_iova_batch = hw_vdavinci_get_iova_batch,
    .is_pfn_valid = is_vm_pfn_valid,
};
