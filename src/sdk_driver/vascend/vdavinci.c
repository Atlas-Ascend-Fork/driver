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
#include "ka_list_pub.h"
#include "ka_errno_pub.h"
#include "dvt.h"
#include "hw_vdavinci.h"
#include "mmio.h"
#include "domain_manage.h"
#include "vfio_ops.h"
#include "priv_ops.h"
#include "schedule.h"
#include "vdavinci.h"

#if KA_IS_DAVINCI_XARRAY_SUPPORT
static KA_DEFINE_XARRAY(g_vdavinci_xa);
#else
static KA_LIST_HEAD(g_vdavinci_list);

STATIC ka_list_head_t *get_vdavinci_list(void)
{
    return &g_vdavinci_list;
}
#endif

static void hw_vdavinci_reset_debugfs_info(struct hw_vdavinci *vdavinci)
{
    int ret = 0;
    size_t destmax_count = (size_t)vdavinci->msix_nvec * sizeof(unsigned long long);

    if (vdavinci->debugfs.msix_count) {
        ret = memset_s(vdavinci->debugfs.msix_count, destmax_count, 0, destmax_count);
        if (ret)
            vascend_err(vdavinci_to_dev(vdavinci),
                        "vdavinci reset msix_count failed, "
                        "ret: %d\n",
                        ret);
    }

    vdavinci->debugfs.notify_count = 0;
}

STATIC void hw_dvt_vdavinci_init(struct hw_vdavinci *vdavinci, struct hw_dvt *dvt, struct hw_vdavinci_type *type,
                                 unsigned int id)
{
    vdavinci->id = id;
    vdavinci->dvt = dvt;
    vdavinci->type = type;
    vdavinci->dev.dev = dvt->vdavinci_priv->dev;
    vdavinci->dev.dev_index = type->dev_index;
    vdavinci->dev.fid = vdavinci->id;
    vdavinci->dev.resource_dev = dvt->vdavinci_priv->dev;
    vdavinci->vf_index = -1;
    ka_task_mutex_init(&vdavinci->vdavinci_lock);
    KA_INIT_LIST_HEAD(&(vdavinci->vdev.dev_dma_info_list_head));
}

#if KA_IS_DAVINCI_XARRAY_SUPPORT
struct hw_vdavinci *find_vdavinci(ka_device_t *dev)
{
    if (ka_unlikely(dev == NULL)) {
        return NULL;
    }

    return ka_xa_load(&g_vdavinci_xa, (uintptr_t)dev);
}
#else
struct hw_vdavinci *find_vdavinci(ka_device_t *dev)
{
    struct hw_vdavinci *vdavinci = NULL, *next = NULL;
    ka_list_head_t *head = get_vdavinci_list();

    if (dev == NULL) {
        return NULL;
    }
    ka_list_for_each_entry_safe(vdavinci, next, head, list)
    {
        if (vdavinci->dev.resource_dev == dev) {
            return vdavinci;
        }
    }
    return NULL;
}
#endif

#if KA_IS_DAVINCI_XARRAY_SUPPORT
STATIC void unregister_vdavinci(struct hw_vdavinci *vdavinci)
{
    if (ka_unlikely(vdavinci == NULL)) {
        return;
    }

    (void)ka_xa_erase(&g_vdavinci_xa, (uintptr_t)vdavinci->dev.resource_dev);
}
#else
STATIC void unregister_vdavinci(struct hw_vdavinci *vdavinci)
{
    struct hw_vdavinci *vdavinci_node = NULL, *next = NULL;
    ka_list_head_t *head = get_vdavinci_list();

    ka_list_for_each_entry_safe(vdavinci_node, next, head, list)
    {
        if (vdavinci_node == vdavinci) {
            ka_list_del(&vdavinci_node->list);
        }
    }
}
#endif

#if KA_IS_DAVINCI_XARRAY_SUPPORT
STATIC int register_vdavinci(struct hw_vdavinci *vdavinci)
{
    void *old = NULL;

    if (ka_unlikely(vdavinci == NULL)) {
        return -EINVAL;
    }
    old = ka_xa_store(&g_vdavinci_xa, (uintptr_t)vdavinci->dev.resource_dev, vdavinci, KA_GFP_KERNEL);
    if (ka_unlikely(ka_xa_is_err(old))) {
        return ka_xa_err(old);
    }

    return 0;
}
#else
STATIC int register_vdavinci(struct hw_vdavinci *vdavinci)
{
    ka_list_head_t *head = get_vdavinci_list();

    ka_list_add_tail(&(vdavinci->list), head);

    return 0;
}
#endif

STATIC int hw_get_available_vf(struct hw_vdavinci *vdavinci)
{
    struct hw_dvt *dvt = vdavinci->dvt;
    unsigned int i;
    for (i = 0; i < dvt->sriov.vf_num; i++) {
        if (!dvt->sriov.vf_array[i].used) {
            vdavinci->vf_index = (int)i;
            dvt->sriov.vf_array[i].used = true;
            dvt->sriov.vf_array[i].vdavinci = vdavinci;
            return i;
        }
    }

    return -EINVAL;
}

STATIC int hw_dvt_alloc_vf(struct hw_vdavinci *vdavinci)
{
    struct hw_dvt *dvt = vdavinci->dvt;
    ka_device_t *dev = dvt->vdavinci_priv->dev;
    int index = 0, ret = 0;

    if (dvt->sriov.vf_used >= dvt->sriov.vf_num) {
        vascend_err(dev, "No available VFs.\n");
        return -ENODEV;
    }
    index = hw_get_available_vf(vdavinci);
    if (index < 0) {
        vascend_err(dev, "No available VFs.\n");
        return -ENODEV;
    }
    vdavinci->vf.pdev = dvt->sriov.vf_array[index].vf;
    vdavinci->dev.resource_dev = &vdavinci->vf.pdev->dev;
    dvt->sriov.vf_used += 1;
    vdavinci->vf.irq_type = VFIO_PCI_NUM_IRQS;
    vdavinci->is_passthrough = true;
    ret = register_vdavinci(vdavinci);
    if (ret != 0) {
        vascend_err(dev, "register vdavinci error: %d\n", ret);
        goto out;
    }

    return 0;
out:
    vdavinci->is_passthrough = false;
    vdavinci->vf.irq_type = -1;
    dvt->sriov.vf_used -= 1;
    vdavinci->dev.resource_dev = NULL;
    vdavinci->vf.pdev = NULL;
    dvt->sriov.vf_array[index].used = false;
    dvt->sriov.vf_array[index].vdavinci = NULL;
    return ret;
}

STATIC int hw_dvt_reclaim_vf(struct hw_vdavinci *vdavinci)
{
    struct hw_dvt *dvt = vdavinci->dvt;
    int index;
    index = vdavinci->vf_index;
    if (index < 0) {
        return 0;
    }
    dvt->sriov.vf_array[index].vdavinci = NULL;
    dvt->sriov.vf_array[index].used = false;
    dvt->sriov.vf_used -= 1;
    unregister_vdavinci(vdavinci);
    vdavinci->dev.resource_dev = NULL;
    vdavinci->vf.pdev = NULL;
    vdavinci->is_passthrough = false;
    return 0;
}

int init_vdavinci_type(struct hw_vdavinci_type *type, struct vdavinci_type *tp)
{
    int ret = 0;

    if (type == NULL || tp == NULL) {
        return -EINVAL;
    }
    ret = snprintf_s(tp->template_name, HW_DVT_MAX_TYPE_NAME, HW_DVT_MAX_TYPE_NAME - 1, "%s", type->template_name);
    if (ret < 0) {
        ka_dfx_pr_err("vdavinci type init failed, ret: %d\n", ret);
        return ret;
    }
    tp->type = type->type;
    tp->bar0_size = type->bar0_size;
    tp->bar2_size = type->bar2_size;
    tp->bar4_size = type->bar4_size;
    tp->aicore_num = type->aicore_num;
    tp->mem_size = type->mem_size;
    tp->aicpu_num = type->aicpu_num;
    tp->vpc_num = type->vpc_num;
    tp->jpegd_num = type->jpegd_num;
    tp->jpege_num = type->jpege_num;
    tp->venc_num = type->venc_num;
    tp->vdec_num = type->vdec_num;
    tp->share = type->share;
    tp->ddrmem_size = type->ddrmem_size;
    tp->hbmmem_size = type->hbmmem_size;

    return 0;
}

STATIC int hw_dvt_ops_create_vdavinci(struct hw_vdavinci *vdavinci, struct hw_vdavinci_type *type, ka_uuid_le_t uuid)
{
    struct vdavinci_type tp;
    int ret = 0;

    ret = init_vdavinci_type(type, &tp);
    if (ret != 0) {
        vascend_err(vdavinci_to_dev(vdavinci), "init vdavinci type failed\n");
        return ret;
    }
    ret = vdavinci_priv_vdev_create(vdavinci, &tp, uuid);
    if (ret != 0) {
        vascend_err(vdavinci_to_dev(vdavinci),
                    "create vdavinci failed, call vdavinci_create failed, "
                    "pf : %u, vid: %u, ret: %d\n",
                    vdavinci->dev.dev_index, vdavinci->id, ret);
        return ret;
    }

    return 0;
}

struct hw_vdavinci *hw_dvt_create_vdavinci(struct hw_dvt *dvt, struct hw_vdavinci_type *type, ka_uuid_le_t uuid)
{
    struct hw_vdavinci *vdavinci;
    int ret;
    struct hw_pf_info *pf_info = &dvt->pf[type->dev_index];

    vdavinci = ka_mm_vzalloc(sizeof(*vdavinci));
    if (vdavinci == NULL) {
        return KA_ERR_PTR(-ENOMEM);
    }

    ret = ka_base_idr_alloc(&pf_info->vdavinci_idr, vdavinci, 0, DVT_MAX_VDAVINCI, KA_GFP_KERNEL);
    if (ret < 0)
        goto free_vdavinci;

    hw_dvt_vdavinci_init(vdavinci, dvt, type, (u32)ret);

    if (dvt->is_sriov_enabled) {
        ret = hw_dvt_alloc_vf(vdavinci);
        if (ret) {
            goto clean_idr;
        }
    }

    ret = dvt->mmio_init(vdavinci);
    if (ret) {
        goto put_vf;
    }

    hw_vdavinci_init_cfg_space(vdavinci);
    ret = hw_dvt_ops_create_vdavinci(vdavinci, type, uuid);
    if (ret != 0) {
        goto mmio_uninit;
    }
    hw_dvt_debugfs_add_vdavinci(vdavinci);
    ka_task_mutex_init(&vdavinci->ioeventfds_lock);
    ka_task_mutex_init(&vdavinci->vdev.cache_lock);
    KA_INIT_LIST_HEAD(&vdavinci->ioeventfds_list);

    return vdavinci;

mmio_uninit:
    dvt->mmio_uninit(vdavinci);
put_vf:
    hw_dvt_reclaim_vf(vdavinci);
clean_idr:
    ka_base_idr_remove(&pf_info->vdavinci_idr, vdavinci->id);
free_vdavinci:
    ka_mm_vfree(vdavinci);
    return KA_ERR_PTR(ret);
}

void hw_dvt_destroy_vdavinci(struct hw_vdavinci *vdavinci)
{
    struct hw_dvt *dvt = vdavinci->dvt;
    struct hw_pf_info *pf_info = &dvt->pf[vdavinci->dev.dev_index];

    /* Drain the release work: SET_KVM(NULL) can re-queue it while the
     * release path runs, before the notifier unregistration. */
    hw_vdavinci_cancel_work_sync(&vdavinci->vdev.release_work);

    vdavinci_priv_vdev_destroy(vdavinci);

    dvt->mmio_uninit(vdavinci);
    hw_dvt_reclaim_vf(vdavinci);
    ka_base_idr_remove(&pf_info->vdavinci_idr, vdavinci->id);
    ka_task_mutex_destroy(&vdavinci->vdev.cache_lock);
    ka_task_mutex_destroy(&vdavinci->ioeventfds_lock);
    ka_task_mutex_destroy(&vdavinci->vdavinci_lock);
    hw_dvt_debugfs_remove_vdavinci(vdavinci);
    if (vdavinci->debugfs.msix_count) {
        ka_mm_kfree(vdavinci->debugfs.msix_count);
        vdavinci->debugfs.msix_count = NULL;
    }
    ka_mm_vfree(vdavinci);
}

void hw_dvt_release_vdavinci(struct hw_vdavinci *vdavinci)
{
    ka_task_mutex_lock(&vdavinci->vdavinci_lock);
    vdavinci_priv_vdev_release(vdavinci);
    ka_task_mutex_unlock(&vdavinci->vdavinci_lock);
}

int hw_dvt_reset_vdavinci(struct hw_vdavinci *vdavinci)
{
    int ret = -1;

    if (ka_unlikely(vdavinci == NULL)) {
        return -EINVAL;
    }
    vascend_info(vdavinci_to_dev(vdavinci), "enter reset vdavinci, pf : %u, vid: %u\n", vdavinci->dev.dev_index,
                 vdavinci->id);
    ka_task_mutex_lock(&vdavinci->vdavinci_lock);
    ret = vdavinci_priv_vdev_reset(vdavinci);
    if (ka_unlikely(ret != 0)) {
        vascend_err(vdavinci_to_dev(vdavinci),
                    "reset vdavinci failed, call vdavinci_reset failed, "
                    "pf : %u, vid: %u, ret: %d\n",
                    vdavinci->dev.dev_index, vdavinci->id, ret);
        goto out;
    }

    hw_vdavinci_reset_mmio(vdavinci);
    hw_vdavinci_reset_cfg_space(vdavinci);
    hw_vdavinci_reset_debugfs_info(vdavinci);
    vdavinci->msix_injection_allowed = false;

    vascend_info(vdavinci_to_dev(vdavinci), "leave reset vdavinci, pf : %u, vid: %u\n", vdavinci->dev.dev_index,
                 vdavinci->id);

out:
    ka_task_mutex_unlock(&vdavinci->vdavinci_lock);
    return ret;
}

/* active transitions are serialized against rw_gpa's active check by taking
 * vdavinci_lock inside the transition helpers, so callers cannot forget it. */
void hw_dvt_activate_vdavinci(struct hw_vdavinci *vdavinci)
{
    ka_task_mutex_lock(&vdavinci->vdavinci_lock);
    ka_base_atomic_set(&vdavinci->active, 1);
    ka_task_mutex_unlock(&vdavinci->vdavinci_lock);
}

void hw_dvt_deactivate_vdavinci(struct hw_vdavinci *vdavinci)
{
    ka_task_mutex_lock(&vdavinci->vdavinci_lock);
    ka_base_atomic_set(&vdavinci->active, 0);
    ka_task_mutex_unlock(&vdavinci->vdavinci_lock);
}

void vdavinci_module_exit(void)
{
#if KA_IS_DAVINCI_XARRAY_SUPPORT
    ka_xa_destroy(&g_vdavinci_xa);
#endif
}
