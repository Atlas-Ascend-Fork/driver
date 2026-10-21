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
#include "ka_compiler_pub.h"
#include "devdrv_ctrl.h"
#include "devdrv_util.h"
#include "devdrv_msg.h"
#include "devdrv_mem_alloc.h"
#include "devdrv_smmu.h"
#include "pbl/pbl_uda.h"

STATIC inline int devdrv_smmu_host_pa_range_check(phys_addr_t pa)
{
    /* host valid phy address segment */
    if (((pa >= DEVDRV_PEH_HOST_CPU0_PA0_START) && (pa < DEVDRV_PEH_HOST_CPU0_PA0_END)) ||
        ((pa >= DEVDRV_PEH_HOST_CPU0_PA1_START) && (pa < DEVDRV_PEH_HOST_CPU0_PA1_END)) ||
        ((pa >= DEVDRV_PEH_HOST_CPU1_PA_START) && (pa < DEVDRV_PEH_HOST_CPU1_PA_END)) ||
        ((pa >= DEVDRV_PEH_HOST_CPU2_PA_START) && (pa < DEVDRV_PEH_HOST_CPU2_PA_END)) ||
        ((pa >= DEVDRV_PEH_HOST_CPU3_PA_START) && (pa < DEVDRV_PEH_HOST_CPU3_PA_END))) {
        return 0;
    } else {
        return -ENOMEM;
    }
}

int devdrv_smmu_iova_to_phys_proc(struct devdrv_pci_ctrl *pci_ctrl, ka_dma_addr_t *va, u32 va_cnt, phys_addr_t *pa)
{
    struct devdrv_host_dma_addr_to_pa_cmd *cmd_data = NULL;
    u32 data_len;
    int ret;
    int i;

    if (pci_ctrl->connect_protocol != CONNECT_PROTOCOL_HCCS) {
        for (i = 0; i < va_cnt; i++) {
            pa[i] = va[i];
        }
        return 0;
    }

    data_len = sizeof(struct devdrv_host_dma_addr_to_pa_cmd) + sizeof(u64) * va_cnt;
    cmd_data = (struct devdrv_host_dma_addr_to_pa_cmd *)devdrv_kzalloc(data_len, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (cmd_data == NULL) {
        devdrv_err("Alloc cmd_data fail. (dev_id=%u)\n", pci_ctrl->dev_id);
        return -EINVAL;
    }

    cmd_data->sub_cmd = DEVDRV_PEH_HOST_VA_TO_PA;
    cmd_data->host_devid = pci_ctrl->dev_id;
    cmd_data->cnt = va_cnt;
    for (i = 0; i < va_cnt; i++) {
        cmd_data->dma_addr[i] = va[i];
    }

    ret = devdrv_admin_msg_chan_send(pci_ctrl->msg_dev, DEVDRV_HCCS_HOST_DMA_ADDR_MAP, cmd_data, data_len, cmd_data,
                                     data_len);
    if (ret != 0) {
        devdrv_kfree(cmd_data);
        devdrv_err("Dma addr to pa fail(va_cnt=%u, devid=%u, ret=%d, data_len=%u\n", va_cnt, pci_ctrl->dev_id, ret,
                   data_len);
        return -ENOMEM;
    }
    for (i = 0; i < va_cnt; i++) {
        pa[i] = cmd_data->dma_addr[i];
        if (ka_unlikely(devdrv_smmu_host_pa_range_check(pa[i]) != 0)) {
            devdrv_warn("Agent smmu va to pa is abnormal. (dev_id=%u)\n", pci_ctrl->dev_id);
        }
    }
    devdrv_kfree(cmd_data);

    return 0;
}

int devdrv_peh_smmu_iova_to_phys(struct devdrv_pci_ctrl *pci_ctrl, ka_dma_addr_t *va, u32 va_cnt, phys_addr_t *pa)
{
    int ret = 0;
    if (pci_ctrl->ops.peh != NULL) {
        ret = pci_ctrl->ops.peh->smmu_iova_to_phys_proc(pci_ctrl, va, va_cnt, pa);
    }
    return ret;
}

int devdrv_smmu_iova_to_phys(u32 dev_id, ka_dma_addr_t *va, u32 va_cnt, phys_addr_t *pa)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    int ret;
    u32 index_id;

    (void)uda_udevid_to_add_id(dev_id, &index_id);
    pci_ctrl = devdrv_get_bottom_half_pci_ctrl_by_id(index_id);
    if (pci_ctrl == NULL) {
        devdrv_err("Get pci_ctrl failed. (dev_id=%d)\n", dev_id);
        return -EINVAL;
    }

    if ((va == NULL) || (pa == NULL)) {
        devdrv_err("va or pa is null. (dev_id=%d)\n", dev_id);
        return -EINVAL;
    }
    if (va_cnt > DEVDRV_AGENT_SMMU_SUPPORT_MAX_NUM) {
        devdrv_err("va_cnt is invalid. (dev_id=%d, va_cnt=%u)\n", dev_id, va_cnt);
        return -EINVAL;
    }

    ret = devdrv_peh_smmu_iova_to_phys(pci_ctrl, va, va_cnt, pa);
    if (ret != 0) {
        devdrv_err("devdrv_smmu_iova_to_phys_proc failed. (dev_id=%d, ret=%d)\n", dev_id, ret);
        return ret;
    }

    return 0;
}
KA_EXPORT_SYMBOL(devdrv_smmu_iova_to_phys);

void devdrv_pdev_sid_init(struct devdrv_pci_ctrl *pci_ctrl)
{
    struct devdrv_pci_ctrl *pci_ctrl_pf = NULL;
    int pf_dev_id, ret;

    ret = ka_pci_get_dev_iommu_fwspec(ka_pci_get_dev(pci_ctrl->pdev));
    if (ret == -EOPNOTSUPP) {
        devdrv_warn("kernel version is low, can not get pdev sid (devid=%u)\n", pci_ctrl->dev_id);
        return;
    } else if (ret == -EINVAL) {
        pci_ctrl->shr_para->sid = 0;
    } else {
        ret = ka_pci_get_dev_iommu(ka_pci_get_dev(pci_ctrl->pdev));
        if (ret == -EOPNOTSUPP) {
            ka_iommu_domain_t *domain = ka_pci_iommu_get_domain_for_dev(ka_pci_get_dev(pci_ctrl->pdev));
            /* if domain->type == KA_IOMMU_DOMAIN_IDENTITY, mean os enable iommu pass through */
            if ((domain != NULL) && (domain->type != KA_IOMMU_DOMAIN_IDENTITY)) {
                pci_ctrl->shr_para->sid = ka_pci_get_dev_iommu_fwspec_ids0(ka_pci_get_dev(pci_ctrl->pdev));
            } else {
                pci_ctrl->shr_para->sid = 0;
            }
        } else if (ret == -EINVAL) {
            pci_ctrl->shr_para->sid = 0;
        } else {
            ka_iommu_domain_t *domain = ka_pci_iommu_get_domain_for_dev(ka_pci_get_dev(pci_ctrl->pdev));
            /* if domain->type == KA_IOMMU_DOMAIN_IDENTITY, mean os enable iommu pass through */
            if ((domain != NULL) && (domain->type != KA_IOMMU_DOMAIN_IDENTITY)) {
                pci_ctrl->shr_para->sid = ka_pci_get_dev_iommu_fwspec_ids0(ka_pci_get_dev(pci_ctrl->pdev));
            } else {
                pci_ctrl->shr_para->sid = 0;
            }
        }
    }

    /* hccs peh, virtual machine passthrough can not get sid */
    if ((pci_ctrl->virtfn_flag == DEVDRV_SRIOV_TYPE_PF) && (pci_ctrl->shr_para->sid == 0) &&
        (ka_pci_get_is_physfn(pci_ctrl->pdev) == 0)) {
        pci_ctrl->shr_para->sid = DEVDRV_NPU_SID_START -
                                  DEVDRV_NPU_CHIP_SID_OFFSET * (pci_ctrl->dev_id / DEVDRV_DIE_NUM_OF_ONE_CHIP) +
                                  DEVDRV_NPU_DIE_SID_OFFSET * (pci_ctrl->dev_id % DEVDRV_DIE_NUM_OF_ONE_CHIP);
    }

    if (pci_ctrl->virtfn_flag == DEVDRV_SRIOV_TYPE_VF) {
        pf_dev_id = devdrv_sriov_get_pf_devid_by_vf_ctrl(pci_ctrl);
        pci_ctrl_pf = devdrv_get_bottom_half_pci_ctrl_by_id((u32)pf_dev_id);
        if (pci_ctrl_pf != NULL && pci_ctrl_pf->shr_para->sid == 0) {
            pci_ctrl->shr_para->sid = 0;
        }
    }

    devdrv_info("Get host pdev sid=0x%x;devid=%u;.\n", pci_ctrl->shr_para->sid, pci_ctrl->dev_id);
}

void devdrv_pdev_sid_no_init(struct devdrv_pci_ctrl *pci_ctrl)
{
    devdrv_info("In vm, get host pdev sid=0x%x;devid=%u.\n", pci_ctrl->shr_para->sid, pci_ctrl->dev_id);
}

void devdrv_pdev_sid_uninit(struct devdrv_pci_ctrl *pci_ctrl)
{
    pci_ctrl->shr_para->sid = 0;
}

void devdrv_pdev_sid_no_uninit(struct devdrv_pci_ctrl *pci_ctrl)
{}
