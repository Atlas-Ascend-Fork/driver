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
#include "ka_fs_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_pci_pub.h"
#include "dvt.h"
#include "kvmdt.h"
#include "vfio_ops.h"

#define MAX_NAME_LEN 32

STATIC ka_kref_t debugfs_ref;
STATIC KA_TASK_DEFINE_MUTEX(debugfs_vascend_lock);
STATIC ka_dentry_t *vascend_debugfs_root = NULL;

STATIC int vdavinci_msix_count_show(struct seq_file *s, void *unused)
{
    int i = 0;
    struct hw_vdavinci *vdavinci = s->private;

    ka_fs_seq_printf(s, "%-16s %-16s\n", "Vector", "Count");
    if (vdavinci->debugfs.msix_count == NULL) {
        return 0;
    }
    for (i = 0; i < vdavinci->msix_nvec; i++) {
        ka_fs_seq_printf(s, "%-16d %-16llu\n", i, vdavinci->debugfs.msix_count[i]);
    }

    return 0;
}

static int seq_file_msix_count_open(struct inode *inode, struct file *file)
{
    return ka_fs_single_open(file, &vdavinci_msix_count_show, inode->i_private);
}

static const ka_file_operations_t vdavinci_msix_count_fops = {
    .owner = KA_THIS_MODULE,
    .open = seq_file_msix_count_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

/**
 * hw_dvt_debugfs_add_vdavinci - register debugfs entries for a vdavinci
 */
void hw_dvt_debugfs_add_vdavinci(struct hw_vdavinci *vdavinci)
{
    int ret;
    char *name = NULL;

    if (vdavinci->dvt->debugfs_root == NULL) {
        return;
    }

    name = ka_mm_kzalloc(MAX_NAME_LEN, KA_GFP_KERNEL);
    if (!name) {
        ret = -ENOMEM;
        vascend_err(vdavinci_to_dev(vdavinci),
                    "add debugfs failed, "
                    "malloc name failed, vid: %u, ret: %d\n",
                    vdavinci->id, ret);
        return;
    }

    if (vdavinci->dvt->dvt_dev.dev_num > 1) {
        ret = snprintf_s(name, MAX_NAME_LEN, MAX_NAME_LEN - 1, "vascend_p%u_%u", vdavinci->dev.dev_index, vdavinci->id);
    } else {
        ret = snprintf_s(name, MAX_NAME_LEN, MAX_NAME_LEN - 1, "vascend%u", vdavinci->id);
    }
    if (ret < 0) {
        vascend_err(vdavinci_to_dev(vdavinci),
                    "add debugfs failed, "
                    "vid: %u, ret: %d\n",
                    vdavinci->id, ret);
        goto out;
    }

    vdavinci->debugfs.debugfs = vdavinci_debugfs_create_dir(name, vdavinci->dvt->debugfs_root);
    if (vdavinci->debugfs.debugfs == NULL) {
        goto out;
    }

    ka_debugfs_create_u64("notify_count", 0400, vdavinci->debugfs.debugfs, &vdavinci->debugfs.notify_count);
    ka_fs_debugfs_create_file("msix_count", 0400, vdavinci->debugfs.debugfs, vdavinci, &vdavinci_msix_count_fops);
out:
    ka_mm_kfree(name);
}

/**
 * hw_dvt_debugfs_remove_vdavinci - remove debugfs entries of a vdavinci
 */
void hw_dvt_debugfs_remove_vdavinci(struct hw_vdavinci *vdavinci)
{
    vdavinci_debugfs_remove(vdavinci->debugfs.debugfs);
    vdavinci->debugfs.debugfs = NULL;
}

STATIC void hw_dvt_debugfs_release(ka_kref_t *ref)
{
    vdavinci_debugfs_remove(vascend_debugfs_root);
    vascend_debugfs_root = NULL;
}

/**
 * hw_dvt_debugfs_init - register dvt debugfs root entry
 */
int hw_dvt_debugfs_init(struct hw_dvt *dvt)
{
    int ret;
    char *name = NULL;
    struct vdavinci_priv *vdavinci_priv = dvt->vdavinci_priv;
    ka_pci_dev_t *pdev = ka_container_of(vdavinci_priv->dev, ka_pci_dev_t, dev);

    ka_task_mutex_lock(&debugfs_vascend_lock);
    if (vascend_debugfs_root == NULL) {
        vascend_debugfs_root = vdavinci_debugfs_create_dir("vascend", NULL);
        if (vascend_debugfs_root == NULL) {
            ka_task_mutex_unlock(&debugfs_vascend_lock);
            return -ENODEV;
        }
        ka_base_kref_init(&debugfs_ref);
    } else {
        ka_base_kref_get(&debugfs_ref);
    }
    ka_task_mutex_unlock(&debugfs_vascend_lock);

    name = ka_mm_kzalloc(MAX_NAME_LEN, KA_GFP_KERNEL);
    if (!name) {
        ret = -ENOMEM;
        vascend_err(vdavinci_priv->dev, "debugfs init failed, malloc name failed. (ret=%d)\n", ret);
        goto err_put_kref;
    }

    ret = snprintf_s(name, MAX_NAME_LEN, MAX_NAME_LEN - 1, "vascend_%02x_%02x_%u", pdev->bus->number,
                     KA_PCI_SLOT(pdev->devfn), KA_PCI_FUNC(pdev->devfn));
    if (ret < 0) {
        vascend_err(vdavinci_priv->dev, "debugfs init failed, format name failed. (ret=%d)\n", ret);
        goto err_free_name;
    }

    dvt->debugfs_root = vdavinci_debugfs_create_dir(name, vascend_debugfs_root);
    if (dvt->debugfs_root == NULL) {
        ret = -ENODEV;
        vascend_err(vdavinci_priv->dev, "debugfs init failed, create dir failed. (ret=%d)\n", ret);
        goto err_free_name;
    }
    ka_mm_kfree(name);

    return 0;

err_free_name:
    ka_mm_kfree(name);
err_put_kref:
    ka_task_mutex_lock(&debugfs_vascend_lock);
    (void)ka_base_kref_put(&debugfs_ref, hw_dvt_debugfs_release);
    ka_task_mutex_unlock(&debugfs_vascend_lock);
    return ret;
}

/**
 * hw_dvt_debugfs_clean - remove debugfs entries
 */
void hw_dvt_debugfs_clean(struct hw_dvt *dvt)
{
    if (dvt->debugfs_root == NULL) {
        return;
    }

    vdavinci_debugfs_remove(dvt->debugfs_root);
    dvt->debugfs_root = NULL;

    ka_task_mutex_lock(&debugfs_vascend_lock);
    (void)ka_base_kref_put(&debugfs_ref, hw_dvt_debugfs_release);
    ka_task_mutex_unlock(&debugfs_vascend_lock);
}
