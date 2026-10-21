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

#include "ka_pci_pub.h"
#include "ka_memory_pub.h"
#include "dvt.h"
#include "priv_ops.h"

#define SHIFT 8
#define DEVFN_MASK 0xff

STATIC int sriov_virtfn_bus(ka_pci_dev_t *dev, unsigned int vf_id)
{
    if (!dev->is_physfn) {
        return -EINVAL;
    }
    return (int)(dev->bus->number + ((dev->devfn + dev->sriov->offset + dev->sriov->stride * vf_id) >> SHIFT));
}

STATIC int sriov_virtfn_devfn(ka_pci_dev_t *dev, unsigned int vf_id)
{
    if (!dev->is_physfn) {
        return -EINVAL;
    }
    return (int)((dev->devfn + dev->sriov->offset + dev->sriov->stride * vf_id) & DEVFN_MASK);
}

STATIC void hw_dvt_sriov_uninit_vf_array(struct hw_dvt *dvt)
{
    unsigned int i = 0;

    if (dvt == NULL) {
        return;
    }
    if (dvt->sriov.vf_array == NULL) {
        return;
    }
    for (i = 0; i < dvt->sriov.vf_num; i++) {
        /* for ka_pci_get_domain_bus_and_slot */
        ka_pci_dev_put(dvt->sriov.vf_array[i].vf);
        dvt->sriov.vf_array[i].vf = NULL;
    }
    ka_mm_kfree(dvt->sriov.vf_array);
    dvt->sriov.vf_array = NULL;
}

STATIC void hw_dvt_clean_vdavinci_vfs(struct hw_dvt *dvt)
{
    hw_dvt_sriov_uninit_vf_array(dvt);
    dvt->sriov.vf_num = 0;
    dvt->sriov.vf_used = 0;
    dvt->is_sriov_enabled = false;
}

STATIC int hw_dvt_sriov_init_vf_array(struct hw_dvt *dvt, ka_pci_dev_t *dev, int num_vfs)
{
    unsigned int i = 0, j = 0;
    ka_device_t *kdev = NULL;

    if (dvt == NULL || dev == NULL) {
        return -EINVAL;
    }
    kdev = &dev->dev;
    dvt->sriov.vf_array = ka_mm_kzalloc(num_vfs * sizeof(struct vf_used_map), KA_GFP_KERNEL);
    if (dvt->sriov.vf_array == NULL) {
        vascend_err(kdev, "Failed to allocate vf array\n");
        return -ENOMEM;
    }
    for (i = 0; i < (unsigned int)num_vfs; i++) {
        int bus = sriov_virtfn_bus(dev, i);
        int devfn = sriov_virtfn_devfn(dev, i);

        if (bus < 0 || devfn < 0) {
            vascend_err(kdev, "Failed to get virtfn bus/devfn\n");
            goto out_free_vf;
        }
        dvt->sriov.vf_array[i].vf = ka_pci_get_domain_bus_and_slot(ka_pci_domain_nr(dev->bus), (unsigned int)bus,
                                                                   (unsigned int)devfn);
        if (dvt->sriov.vf_array[i].vf == NULL) {
            vascend_err(kdev, "Failed to enable vf\n");
            goto out_free_vf;
        }
        dvt->sriov.vf_array[i].vdavinci = NULL;
        dvt->sriov.vf_array[i].used = false;
    }

    return 0;
out_free_vf:
    for (j = 0; j < i; j++) {
        ka_pci_dev_put(dvt->sriov.vf_array[j].vf);
        dvt->sriov.vf_array[j].vf = NULL;
    }
    ka_mm_kfree(dvt->sriov.vf_array);
    dvt->sriov.vf_array = NULL;
    return -EINVAL;
}

STATIC int hw_dvt_sriov_init_pf(struct hw_dvt *dvt, ka_pci_dev_t *dev, int num_vfs)
{
    int ret = 0;
    ka_device_t *kdev = NULL;

    if (dvt == NULL || dev == NULL) {
        return -EINVAL;
    }
    kdev = &dev->dev;
    ka_task_mutex_lock(&dvt->lock);
    ret = hw_dvt_init_dev_pf_info(dvt);
    if (ret != 0) {
        vascend_err(kdev, "init dev pf info failed.\n");
        goto un_lock;
    }
    ret = hw_dvt_sriov_init_vf_array(dvt, dev, num_vfs);
    if (ret != 0) {
        vascend_err(kdev, "init dev pf info failed.\n");
        goto pf_info;
    }
    dvt->sriov.vf_used = 0;
    dvt->sriov.vf_num = (unsigned int)num_vfs;
    dvt->is_sriov_enabled = true;
    ret = hw_dvt_set_mmio_ops(dvt, vdavinci_mmio_vf_devices_ops);
    if (ret != 0) {
        vascend_err(kdev, "Failed to set mmio ops\n");
        goto vf_array;
    }
    ka_task_mutex_unlock(&dvt->lock);
    return 0;

vf_array:
    hw_dvt_sriov_uninit_vf_array(dvt);
pf_info:
    hw_dvt_uninit_dev_pf_info(dvt);
un_lock:
    ka_task_mutex_unlock(&dvt->lock);
    return ret;
}

STATIC int hw_dvt_sriov_disable(ka_pci_dev_t *dev)
{
    ka_device_t *kdev = &dev->dev;
    struct hw_dvt *dvt = NULL;
    int ret = 0;

    dvt = kdev_to_davinci(kdev)->dvt;
    if (!dvt) {
        return -EINVAL;
    }

    ka_task_mutex_lock(&dvt->lock);
    /* Check vf_used inside the lock: a concurrent mdev create (also under
     * dvt->lock) must not slip in between the check and the vf_array teardown. */
    if (!hw_vdavinci_vf_used_num_zero(dvt)) {
        vascend_err(kdev, "the count of mdev has not been reset to zero\n");
        ka_task_mutex_unlock(&dvt->lock);
        return -EINVAL;
    }
    ret = vdavinci_priv_vdev_enable_sriov(dvt->vdavinci_priv, dev, 0);
    if (ret != 0) {
        vascend_err(kdev, "Failed to disable sriov\n");
        ka_task_mutex_unlock(&dvt->lock);
        return ret;
    }
    hw_dvt_clean_vdavinci_vfs(dvt);
    hw_dvt_uninit_dev_pf_info(dvt);
    ret = hw_dvt_set_mmio_ops(dvt, vdavinci_mmio_pf_devices_ops);
    if (ret) {
        vascend_err(kdev, "Failed to set mmio ops\n");
        ka_task_mutex_unlock(&dvt->lock);
        return -EINVAL;
    }
    vascend_info(kdev, "Sriov disable success\n");
    ka_task_mutex_unlock(&dvt->lock);
    return 0;
}

int hw_dvt_sriov_enable(ka_pci_dev_t *dev, int num_vfs)
{
    ka_device_t *kdev = &dev->dev;
    struct hw_dvt *dvt;
    int ret = 0;

    dvt = kdev_to_davinci(kdev)->dvt;
    if (dvt == NULL) {
        return -EINVAL;
    }
    if (num_vfs > VDAVINCI_VF_MAX || num_vfs < 0) {
        vascend_err(kdev, "Unsupported vf num:%u\n", num_vfs);
        return -EINVAL;
    }
    if (!hw_vdavinci_vf_used_num_zero(dvt)) {
        vascend_err(kdev, "the count of mdev has not been reset to zero\n");
        return -EINVAL;
    }
    if (num_vfs == 0) {
        return hw_dvt_sriov_disable(dev);
    }
    ret = vdavinci_priv_vdev_enable_sriov(dvt->vdavinci_priv, dev, num_vfs);
    if (ret != 0) {
        vascend_err(kdev, "Failed to enable sriov\n");
        return ret;
    }
    ret = hw_dvt_sriov_init_pf(dvt, dev, num_vfs);
    if (ret != 0) {
        vascend_err(kdev, "Failed to init pf's sriov\n");
        goto disable_sriov;
    }
    return num_vfs;

disable_sriov:
    (void)vdavinci_priv_vdev_enable_sriov(dvt->vdavinci_priv, dev, 0);
    return ret;
}
