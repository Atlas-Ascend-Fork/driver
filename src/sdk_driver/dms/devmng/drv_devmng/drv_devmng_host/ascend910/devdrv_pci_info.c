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

#include "ka_kernel_def_pub.h"
#include "devdrv_manager.h"
#include "devdrv_manager_common.h"
#include "devdrv_pcie.h"
#include "dms_hotreset.h"
#include "adapter_api.h"
#include "comm_kernel_interface.h"
#include "devdrv_manager_ioctl.h"

static const ka_pci_device_id_t devdrv_driver_tbl[] = {
    {KA_PCI_VDEVICE(HUAWEI, 0xd100), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd105), 0},
    {KA_PCI_VDEVICE(HUAWEI, PCI_DEVICE_CLOUD), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd801), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd500), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd501), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd802), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd803), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd804), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd805), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd806), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd807), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd808), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd809), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd200), 0},
    {DEVDRV_DIVERSITY_PCIE_VENDOR_ID, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {0x20C6, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {0x203F, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {0x20E9, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {0x20C6, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {0x203F, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {0x20E9, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {}};
KA_MODULE_DEVICE_TABLE(pci, devdrv_driver_tbl);

int devdrv_manager_get_pci_info(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
#ifndef CFG_FEATURE_REFACTOR
    struct devdrv_platform_data *pdata = NULL;
#endif
    u32 dev_id = ASCEND_DEV_MAX_NUM + 1;
    struct devdrv_info *dev_info = NULL;
    struct devdrv_pci_info pci_info;
    u32 virt_id;
    u32 vfid = 0;
    int ret;
    unsigned long flags;

    if (copy_from_user_safe(&pci_info, (void *)(uintptr_t)arg, sizeof(struct devdrv_pci_info))) {
        return -EFAULT;
    }
    virt_id = pci_info.dev_id;
    ret = devdrv_manager_trans_and_check_id(virt_id, &dev_id, &vfid);
    if (ret != 0) {
        devdrv_drv_err("can't transform virt id %u,ret=%d\n", virt_id, ret);
        return ret;
    }

    if (dev_id >= ASCEND_DEV_MAX_NUM) {
        devdrv_drv_err("invalid dev_id(%u)\n", dev_id);
        return -ENODEV;
    }

    ka_task_spin_lock_irqsave(&dev_manager_info->spinlock, flags);
    dev_info = dev_manager_info->dev_info[dev_id];
    if (dev_info == NULL) {
        ka_task_spin_unlock_irqrestore(&dev_manager_info->spinlock, flags);
        devdrv_drv_err("dev_info[%u] is NULL\n", dev_id);
        return -EFAULT;
    }
#ifdef CFG_FEATURE_REFACTOR
    pci_info.bus_number = dev_info->pci_info.bus_number;
    pci_info.dev_number = dev_info->pci_info.dev_number;
    pci_info.function_number = dev_info->pci_info.function_number;
#else
    if (dev_info->pdata == NULL) {
        ka_task_spin_unlock_irqrestore(&dev_manager_info->spinlock, flags);
        devdrv_drv_err("dev_info[%u] pdata is NULL\n", dev_id);
        return -EFAULT;
    }
    pdata = dev_info->pdata;
    pci_info.bus_number = pdata->pci_info.bus_number;
    pci_info.dev_number = pdata->pci_info.dev_number;
    pci_info.function_number = pdata->pci_info.function_number;
#endif
    ka_task_spin_unlock_irqrestore(&dev_manager_info->spinlock, flags);

    if (copy_to_user_safe((void *)(uintptr_t)arg, &pci_info, sizeof(struct devdrv_pci_info))) {
        return -EFAULT;
    }

    return 0;
}

int devdrv_get_pcie_id(unsigned long arg)
{
    struct dmanage_pcie_id_info pcie_id_info = {0};
    struct devdrv_pcie_id_info id_info = {0};
    unsigned int dev_id, virt_id, vfid;
    int ret;

    ret = copy_from_user_safe(&pcie_id_info, (void *)((uintptr_t)arg), sizeof(struct dmanage_pcie_id_info));
    if (ret) {
        devdrv_drv_err("copy_from_user_safe failed.\n");
        return ret;
    }

    virt_id = pcie_id_info.davinci_id;
    ret = devdrv_manager_trans_and_check_id(virt_id, &dev_id, &vfid);
    if (ret != 0) {
        devdrv_drv_err("can't transform virt id %u, ret=%d\n", virt_id, ret);
        return ret;
    }

    ret = dms_hotreset_task_cnt_increase(dev_id);
    if (ret != 0) {
        devdrv_drv_err("Hotreset task cnt increase failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    ret = adap_get_pcie_id_info(dev_id, &id_info);
    if (ret) {
        devdrv_drv_err("devdrv_get_pcie_id failed.\n");
        dms_hotreset_task_cnt_decrease(dev_id);
        return ret;
    }

    pcie_id_info.venderid = id_info.venderid;
    pcie_id_info.subvenderid = id_info.subvenderid;
    pcie_id_info.deviceid = id_info.deviceid;
    pcie_id_info.subdeviceid = id_info.subdeviceid;
    pcie_id_info.bus = id_info.bus;
    pcie_id_info.device = id_info.device;
    pcie_id_info.fn = id_info.fn;
    pcie_id_info.domain = id_info.domain;

    ret = copy_to_user_safe((void *)(uintptr_t)arg, &pcie_id_info, sizeof(struct dmanage_pcie_id_info));
    if (ret) {
        devdrv_drv_err("copy_to_user_safe failed.\n");
        dms_hotreset_task_cnt_decrease(dev_id);
        return ret;
    }
    dms_hotreset_task_cnt_decrease(dev_id);
    return 0;
}

int devdrv_manager_get_dev_resource(struct devdrv_info *dev_info)
{
#ifndef CFG_FEATURE_REFACTOR
    struct devdrv_platform_data *pdata = NULL;
#endif
    struct devdrv_pci_dev_info dev_pci_info = {0};
    u32 dev_id = dev_info->pci_dev_id;

#ifndef CFG_FEATURE_UB
    if (adap_get_pci_dev_info(dev_id, &dev_pci_info)) {
        devdrv_drv_err("devdrv_get_pci_dev_info failed. dev_id(%u)\n", dev_id);
        return -EBUSY;
    }
#endif

#ifdef CFG_FEATURE_REFACTOR
    dev_info->pci_info.bus_number = dev_pci_info.bus_no;
    dev_info->pci_info.dev_number = dev_pci_info.device_no;
    dev_info->pci_info.function_number = dev_pci_info.function_no;
#else
    pdata = dev_info->pdata;
    if (pdata == NULL) {
        devdrv_drv_err("dev_info pdata is NULL\n");
        return -EFAULT;
    }
    pdata->pci_info.bus_number = dev_pci_info.bus_no;
    pdata->pci_info.dev_number = dev_pci_info.device_no;
    pdata->pci_info.function_number = dev_pci_info.function_no;
#endif
    devdrv_drv_debug("device(%u) ts doorbell addr.\n", dev_id);
    return 0;
}
