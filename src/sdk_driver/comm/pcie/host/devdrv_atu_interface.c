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
#include "devdrv_atu_interface.h"
#include "devdrv_atu.h"
#include "devdrv_util.h"
#include "devdrv_ctrl.h"
#include "pbl/pbl_uda.h"

int devdrv_get_atu_info(struct devdrv_pci_ctrl *pci_ctrl, int atu_type, struct devdrv_iob_atu **atu, u64 *host_phy_base)
{
    int ret = 0;
    switch (atu_type) {
        case ATU_TYPE_RX_MEM:
            *atu = pci_ctrl->mem_rx_atu;
            *host_phy_base = pci_ctrl->mem_phy_base;
            break;
        default:
            devdrv_err("Not support atu_type. (dev_id=%d; atu_type=%d)\n", pci_ctrl->dev_id, atu_type);
            ret = -EINVAL;
            break;
    }

    return ret;
}

int devdrv_atu_base_to_target(struct devdrv_pci_ctrl *pci_ctrl, struct devdrv_iob_atu atu[], int num, u64 base_addr,
                              u64 *target_addr)
{
    u64 bar_base;
    int i;

    for (i = 0; i < num; i++) {
        if (atu[i].valid == ATU_INVALID) {
            continue;
        }

        bar_base = atu[i].base_addr;
        if (!devdrv_is_pdev_main_davinci_dev(pci_ctrl)) {
            bar_base -= pci_ctrl->mem_bar_offset;
        }

        if ((bar_base <= base_addr) && ((bar_base + atu[i].size) > base_addr)) {
            *target_addr = base_addr - bar_base + atu[i].target_addr;
            return 0;
        }
    }
    return -EINVAL;
}

int devdrv_atu_target_to_base(struct devdrv_pci_ctrl *pci_ctrl, struct devdrv_iob_atu atu[], int num, u64 target_addr,
                              u64 *base_addr)
{
    u64 bar_base;
    int i;

    for (i = 0; i < num; i++) {
        if (atu[i].valid == ATU_INVALID) {
            continue;
        }

        if ((atu[i].target_addr <= target_addr) && ((atu[i].target_addr + atu[i].size) > target_addr)) {
            bar_base = atu[i].base_addr;
            if (!devdrv_is_pdev_main_davinci_dev(pci_ctrl)) {
                bar_base -= pci_ctrl->mem_bar_offset;
            }

            *base_addr = target_addr - atu[i].target_addr + bar_base;
            return 0;
        }
    }

    return -EINVAL;
}

int devdrv_devmem_addr_d2h(u32 udevid, phys_addr_t device_phy_addr, phys_addr_t *host_bar_addr)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    struct devdrv_iob_atu *atu = NULL;
    u64 host_phy_base = 0;
    u32 index_id;
    int ret;

    ret = uda_udevid_to_add_id(udevid, &index_id);
    if (ret != 0) {
        devdrv_err("Udevid to index_id failed. (udevid=%u; index_id=%u)\n", udevid, index_id);
        return -EINVAL;
    }

    if (host_bar_addr == NULL) {
        devdrv_err("host_bar_addr is null. (udevid=%u; index_id=%u)\n", udevid, index_id);
        return -EINVAL;
    }

    pci_ctrl = devdrv_get_bottom_half_pci_ctrl_by_id(index_id);
    if (pci_ctrl == NULL) {
        devdrv_err("Get pci_ctrl failed. (udevid=%u; index_id=%u)\n", udevid, index_id);
        return -EINVAL;
    }

    /* Without PA transformation, can't access bar addr on the same chip, but Virtual pass-through must use bar */
    if ((pci_ctrl->connect_protocol == CONNECT_PROTOCOL_HCCS) && (ka_pci_get_is_physfn(pci_ctrl->pdev) != 0)) {
        *host_bar_addr = device_phy_addr;
        return 0;
    }

    if (devdrv_get_atu_info(pci_ctrl, ATU_TYPE_RX_MEM, &atu, &host_phy_base) != 0) {
        devdrv_err("Find atu failed. (udevid=%u; index_id=%u)\n", udevid, index_id);
        return -EINVAL;
    }

    if (devdrv_atu_target_to_base(pci_ctrl, atu, DEVDRV_MAX_RX_ATU_NUM, (u64)device_phy_addr, (u64 *)host_bar_addr) !=
        0) {
        devdrv_warn("device_phy_addr not found. (udevid=%u; index_id=%u)\n", udevid, index_id);
        return -EFAULT;
    }

    *host_bar_addr += host_phy_base;

    return 0;
}
KA_EXPORT_SYMBOL(devdrv_devmem_addr_d2h);

int devdrv_devmem_addr_h2d(u32 udevid, phys_addr_t host_bar_addr, phys_addr_t *device_phy_addr)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    struct devdrv_iob_atu *atu = NULL;
    u64 host_phy_base = 0;
    u32 index_id;
    int ret;

    ret = uda_udevid_to_add_id(udevid, &index_id);
    if (ret != 0) {
        devdrv_err("Udevid to index_id failed. (udevid=%u; index_id=%u)\n", udevid, index_id);
        return -EINVAL;
    }

    if (device_phy_addr == NULL) {
        devdrv_err("device_phy_addr is null. (udevid=%u; index_id=%u)\n", udevid, index_id);
        return -EINVAL;
    }

    pci_ctrl = devdrv_get_bottom_half_pci_ctrl_by_id(index_id);
    if (pci_ctrl == NULL) {
        devdrv_err("Get pci_ctrl failed. (udevid=%u; index_id=%u)\n", udevid, index_id);
        return -EINVAL;
    }

    if (devdrv_get_atu_info(pci_ctrl, ATU_TYPE_RX_MEM, &atu, &host_phy_base) != 0) {
        devdrv_err("Find atu failed. (udevid=%u; index_id=%u)\n", udevid, index_id);
        return -EINVAL;
    }

    if (host_bar_addr < host_phy_base) {
        devdrv_err("host_bar_addr is smaller than host_phy_base. (udevid=%u; index_id=%u)\n", udevid, index_id);
        return -EINVAL;
    }

    if (devdrv_atu_base_to_target(pci_ctrl, atu, DEVDRV_MAX_RX_ATU_NUM, (u64)(host_bar_addr - host_phy_base),
                                  (u64 *)device_phy_addr) != 0) {
        devdrv_err("host_bar_addr not found. (udevid=%u; index_id=%u)\n", udevid, index_id);
        return -EINVAL;
    }

    return 0;
}
KA_EXPORT_SYMBOL(devdrv_devmem_addr_h2d);
