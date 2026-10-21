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

#ifndef DEVDRV_SMMU_H
#define DEVDRV_SMMU_H

#include "comm_kernel_interface.h"
#include "devdrv_pci.h"
#include "comm_cmd_msg.h"

#define DEVDRV_PEH_PHY_ADDR_MAX_VALUE 0x800000000000 /* 128T */

/* cpu0's addr range */
#define DEVDRV_PEH_HOST_CPU0_PA0_START 0x40000000
#define DEVDRV_PEH_HOST_CPU0_PA0_END 0x80000000
#define DEVDRV_PEH_HOST_CPU0_PA1_START 0x22000000000
#define DEVDRV_PEH_HOST_CPU0_PA1_END 0x40000000000
/* cpu1's addr range */
#define DEVDRV_PEH_HOST_CPU1_PA_START 0xA2000000000
#define DEVDRV_PEH_HOST_CPU1_PA_END 0xC0000000000
/* cpu2's addr range */
#define DEVDRV_PEH_HOST_CPU2_PA_START 0x122000000000
#define DEVDRV_PEH_HOST_CPU2_PA_END 0x140000000000
/* cpu3's addr range */
#define DEVDRV_PEH_HOST_CPU3_PA_START 0x1A2000000000
#define DEVDRV_PEH_HOST_CPU3_PA_END 0x1C0000000000

#define DEVDRV_SMMU_NON_TRANS_MSG_DESC_SIZE 0x200

#define DEVDRV_PEH_HOST_VA_TO_PA 0
#define DEVDRV_PEH_MSI_TABLE_REFRESH 1
#define DEVDRV_PEH_VF_MSI_TABLE_REFRESH 2
#define DEVDRV_PEH_VA_TO_PA_MAX_CMD 3 /* must be last one */

#define DEVDRV_NPU_SID_START 0x9D00 /* npu0's die0 sid */
#define DEVDRV_NPU_CHIP_SID_OFFSET 0x400
#define DEVDRV_NPU_DIE_SID_OFFSET 0x200
#define DEVDRV_DIE_NUM_OF_ONE_CHIP 2

void devdrv_pdev_sid_init(struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_pdev_sid_no_init(struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_pdev_sid_uninit(struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_pdev_sid_no_uninit(struct devdrv_pci_ctrl *pci_ctrl);
int devdrv_smmu_init_msg_chan(struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_smmu_uninit_msg_chan(struct devdrv_pci_ctrl *pci_ctrl);
int devdrv_smmu_iova_to_phys_proc(struct devdrv_pci_ctrl *pci_ctrl, ka_dma_addr_t *va, u32 va_cnt, phys_addr_t *pa);
int devdrv_peh_smmu_iova_to_phys(struct devdrv_pci_ctrl *pci_ctrl, ka_dma_addr_t *va, u32 va_cnt, phys_addr_t *pa);
#endif
