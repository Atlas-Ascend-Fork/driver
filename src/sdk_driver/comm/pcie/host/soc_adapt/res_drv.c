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
#include "res_drv.h"
#include "comm_kernel_interface.h"
#include "devdrv_adapt.h"
#include "devdrv_mem_alloc.h"

int devdrv_res_init(struct devdrv_pci_ctrl *pci_ctrl)
{
    int ret = -EINVAL;

    if (pci_ctrl->chip_type < HISI_CHIP_NUM) {
        ret = devdrv_res_init_func[pci_ctrl->chip_type](pci_ctrl);
    }

    return ret;
}

void devdrv_res_uninit(struct devdrv_pci_ctrl *pci_ctrl)
{
    pci_ctrl->shr_para = NULL;

    if (pci_ctrl->io_base != NULL) {
        ka_mm_iounmap(pci_ctrl->io_base);
        pci_ctrl->io_base = NULL;
    }

    if (pci_ctrl->msi_base != NULL) {
        ka_mm_iounmap(pci_ctrl->msi_base);
        pci_ctrl->msi_base = NULL;
    }

    if (pci_ctrl->mem_base != NULL) {
        ka_mm_iounmap(pci_ctrl->mem_base);
        pci_ctrl->mem_base = NULL;
    }

    if (pci_ctrl->local_reserve_mem_base != NULL) {
        ka_mm_iounmap(pci_ctrl->local_reserve_mem_base);
        pci_ctrl->local_reserve_mem_base = NULL;
    }

    if (pci_ctrl->ops.peh != NULL) {
        devdrv_kfree(pci_ctrl->ops.peh);
        pci_ctrl->ops.peh = NULL;
    }
}
