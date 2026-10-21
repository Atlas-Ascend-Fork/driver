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
#include "ka_kernel_def_pub.h"
#include "ka_errno_pub.h"
#include "ka_fs_pub.h"
#include "ka_dfx_pub.h"
#include "hw_vdavinci.h"

bool hw_dvt_check_host_is_uvp(void);

#define PCI_VENDOR_ID_HUAWEI 0x19e5
#define HISI_EP_DEVICE_ID_MINIV1 0xd100
#define HISI_EP_DEVICE_ID_MINIV2 0xd500
#define HISI_EP_DEVICE_ID_CLOUD 0xd801
#define HISI_EP_DEVICE_ID_CLOUD_V2 0x802
#define DEVDRV_DIVERSITY_PCIE_VENDOR_ID 0xFFFF
#define UVP_VERSION_INFO_SIZE 256

static const ka_pci_device_id_t g_vmngh_tbl[] = {
    {KA_PCI_VDEVICE(HUAWEI, HISI_EP_DEVICE_ID_MINIV2), 0},
    {KA_PCI_VDEVICE(HUAWEI, HISI_EP_DEVICE_ID_CLOUD), 0},
    {KA_PCI_VDEVICE(HUAWEI, HISI_EP_DEVICE_ID_CLOUD_V2), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd805), 0},
    {DEVDRV_DIVERSITY_PCIE_VENDOR_ID, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {0x20C6, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {0x203F, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {0x20C6, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {0x203F, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {0x20E9, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {0x20E9, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {}};
KA_MODULE_DEVICE_TABLE(pci, g_vmngh_tbl);

struct vdavinci_mode_info g_vdev_mode = {0};
struct vdavinci_drv_ops vasd_virt_ops = {0};

int register_vdavinci_virtual_ops(struct vdavinci_drv_ops *ops)
{
    if (!ops) {
        ka_dfx_pr_err("Invalid para. ops is NULL\n");
        return -EINVAL;
    }

    vasd_virt_ops.vdavinci_init = ops->vdavinci_init;
    vasd_virt_ops.vdavinci_uninit = ops->vdavinci_uninit;
    vasd_virt_ops.vdavinci_hypervisor_inject_msix = ops->vdavinci_hypervisor_inject_msix;
    vasd_virt_ops.vdavinci_hypervisor_read_gpa = ops->vdavinci_hypervisor_read_gpa;
    vasd_virt_ops.vdavinci_hypervisor_write_gpa = ops->vdavinci_hypervisor_write_gpa;
    vasd_virt_ops.vdavinci_hypervisor_gfn_to_mfn = ops->vdavinci_hypervisor_gfn_to_mfn;
    vasd_virt_ops.vdavinci_hypervisor_dma_pool_init = ops->vdavinci_hypervisor_dma_pool_init;
    vasd_virt_ops.vdavinci_hypervisor_dma_pool_uninit = ops->vdavinci_hypervisor_dma_pool_uninit;
    vasd_virt_ops.vdavinci_hypervisor_dma_map_guest_page = ops->vdavinci_hypervisor_dma_map_guest_page;
    vasd_virt_ops.vdavinci_hypervisor_dma_unmap_guest_page = ops->vdavinci_hypervisor_dma_unmap_guest_page;
    vasd_virt_ops.vdavinci_hypervisor_dma_pool_active = ops->vdavinci_hypervisor_dma_pool_active;
    vasd_virt_ops.vdavinci_hypervisor_dma_map_guest_page_batch = ops->vdavinci_hypervisor_dma_map_guest_page_batch;
    vasd_virt_ops.vdavinci_hypervisor_dma_unmap_guest_page_batch = ops->vdavinci_hypervisor_dma_unmap_guest_page_batch;
    vasd_virt_ops.vdavinci_hypervisor_is_valid_gfn = ops->vdavinci_hypervisor_is_valid_gfn;
    vasd_virt_ops.vdavinci_hypervisor_mmio_get = ops->vdavinci_hypervisor_mmio_get;
    vasd_virt_ops.vdavinci_hypervisor_dma_alloc_coherent = ops->vdavinci_hypervisor_dma_alloc_coherent;
    vasd_virt_ops.vdavinci_hypervisor_dma_free_coherent = ops->vdavinci_hypervisor_dma_free_coherent;
    vasd_virt_ops.vdavinci_hypervisor_dma_map_single = ops->vdavinci_hypervisor_dma_map_single;
    vasd_virt_ops.vdavinci_hypervisor_dma_unmap_single = ops->vdavinci_hypervisor_dma_unmap_single;
    vasd_virt_ops.vdavinci_hypervisor_dma_map_page = ops->vdavinci_hypervisor_dma_map_page;
    vasd_virt_ops.vdavinci_hypervisor_dma_unmap_page = ops->vdavinci_hypervisor_dma_unmap_page;
    vasd_virt_ops.vdavinci_get_reserve_iova_for_check = ops->vdavinci_get_reserve_iova_for_check;
    vasd_virt_ops.vdavinci_is_vm_pfn_valid = ops->vdavinci_is_vm_pfn_valid;
    ka_dfx_pr_info("register virtual ops success!\n");
    return 0;
}
KA_EXPORT_SYMBOL_GPL(register_vdavinci_virtual_ops);

void unregister_vdavinci_virtual_ops(void)
{
    struct vdavinci_drv_ops ops = {0};
    vasd_virt_ops = ops;
}
KA_EXPORT_SYMBOL_GPL(unregister_vdavinci_virtual_ops);

struct vdavinci_drv_ops *get_vdavinci_virtual_ops(void)
{
    return &vasd_virt_ops;
}

#ifndef ENABLE_BUILD_PRODUCT
bool hw_dvt_check_host_is_uvp(void)
{
    char uvp_version_buf[UVP_VERSION_INFO_SIZE] = {0};
    ka_file_t *fp = NULL;
    loff_t pos = 0;
    ssize_t ret;

    fp = ka_fs_filp_open("/etc/uvp_version", KA_O_RDONLY, KA_S_IRUSR);
    if (KA_IS_ERR(fp)) {
        ka_dfx_pr_err("open uvp file failed, err = %ld.\n", KA_PTR_ERR(fp));
        return false;
    }

    ret = ka_fs_kernel_read(fp, uvp_version_buf, UVP_VERSION_INFO_SIZE, &pos);
    ka_fs_filp_close(fp, NULL);
    if ((ret <= 0) || (ret > UVP_VERSION_INFO_SIZE)) {
        ka_dfx_pr_err("get cpuset file content failed, ret = %ld.\n", ret);
        return false;
    }

    uvp_version_buf[UVP_VERSION_INFO_SIZE - 1] = '\0';
    if (ka_base_strstr(uvp_version_buf, "uvp_version") == NULL) {
        ka_dfx_pr_err("cannot find uvp_version in /etc/uvp_version.\n");
        return false;
    }

    return true;
}
#endif

int hw_dvt_set_mode(int mode)
{
    if (mode < 0 || mode >= VDAVINCI_MODE_MAX) {
        ka_dfx_pr_err("Invalid para. mode is %d\n", mode);
        return -EINVAL;
    }

#ifndef ENABLE_BUILD_PRODUCT
    if (mode == VDAVINCI_VM) {
        if (!hw_dvt_check_host_is_uvp()) {
            ka_dfx_pr_err("The system is not a UVP system, so VM mode is not supported.\n");
            return -EOPNOTSUPP;
        }
    }
#endif

    ka_task_down_write(&g_vdev_mode.rw_sem);
    g_vdev_mode.mode = mode;
    ka_task_up_write(&g_vdev_mode.rw_sem);
    ka_dfx_pr_info("set mode success! mode=%d\n", mode);
    return 0;
}
KA_EXPORT_SYMBOL_GPL(hw_dvt_set_mode);

int hw_dvt_get_mode(int *mode)
{
    if (mode == NULL) {
        ka_dfx_pr_err("Invalid para. mode is NULL\n");
        return -EINVAL;
    }

    ka_task_down_read(&g_vdev_mode.rw_sem);
    *mode = g_vdev_mode.mode;
    ka_task_up_read(&g_vdev_mode.rw_sem);
    return 0;
}
KA_EXPORT_SYMBOL_GPL(hw_dvt_get_mode);

bool hw_dvt_check_is_vm_mode(void)
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

int hw_dvt_init(void *vdavinci_priv)
{
    if (get_vdavinci_virtual_ops()->vdavinci_init != NULL) {
        return get_vdavinci_virtual_ops()->vdavinci_init(vdavinci_priv);
    }

    return 0;
}
KA_EXPORT_SYMBOL_GPL(hw_dvt_init);

int hw_dvt_uninit(void *vdavinci_priv)
{
    if (get_vdavinci_virtual_ops()->vdavinci_uninit != NULL) {
        return get_vdavinci_virtual_ops()->vdavinci_uninit(vdavinci_priv);
    }

    return 0;
}
KA_EXPORT_SYMBOL_GPL(hw_dvt_uninit);

KA_MODULE_LICENSE("GPL");
