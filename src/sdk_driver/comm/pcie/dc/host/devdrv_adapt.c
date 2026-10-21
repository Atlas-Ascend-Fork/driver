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
#include "res_drv.h"
#include "comm_kernel_interface.h"
#include "devdrv_util.h"
#include "devdrv_pci.h"
#include "res_drv_cloud_v1.h"
#include "res_drv_cloud_v2.h"
#include "res_drv_cloud_v4.h"
#include "res_drv_cloud_v6.h"
#include "res_drv_mini_v2.h"
#include "res_drv_mini_v3.h"
#include "res_drv_mini_v5.h"
#include "devdrv_adapt.h"

const ka_pci_device_id_t g_devdrv_tbl[] = {
    {KA_PCI_VDEVICE(HUAWEI, CLOUD_V1_DEVICE), HISI_CLOUD_V1},
    {KA_PCI_VDEVICE(HUAWEI, MINI_V2_DEVICE), HISI_MINI_V2},
    {KA_PCI_VDEVICE(HUAWEI, MINI_V2_2P_DEVICE), HISI_MINI_V2},
    {KA_PCI_VDEVICE(HUAWEI, CLOUD_V2_DEVICE), HISI_CLOUD_V2},
    {KA_PCI_VDEVICE(HUAWEI, CLOUD_V2_2P_DEVICE), HISI_CLOUD_V2},
    {KA_PCI_VDEVICE(HUAWEI, CLOUD_V2_HCCS_IEP_DEVICE), HISI_CLOUD_V2},
    {KA_PCI_VDEVICE(HUAWEI, CLOUD_V2_VF_DEVICE), HISI_CLOUD_V2},
    {DEVDRV_DIVERSITY_PCIE_VENDOR_ID, MINI_V2_DEVICE, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, HISI_MINI_V2},
    {KA_PCI_VDEVICE(HUAWEI, MINI_V3_DEVICE), HISI_MINI_V3},
    {KA_PCI_VDEVICE(HUAWEI, CLOUD_V4_DEVICE), HISI_CLOUD_V4},
    {KA_PCI_VDEVICE(HUAWEI, CLOUD_V5_DEVICE), HISI_CLOUD_V5},
    {KA_PCI_VDEVICE(HUAWEI, CLOUD_V6_DEVICE), HISI_CLOUD_V6},
    {KA_PCI_VDEVICE(HUAWEI, MINI_V4_DEVICE), HISI_MINI_V4},
    {KA_PCI_VDEVICE(HUAWEI, MINI_V5_DEVICE), HISI_MINI_V5},
    {DEVDRV_PCI_SUBSYS_PRIVATE_VENDOR_HK, MINI_V2_DEVICE, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, HISI_MINI_V2},
    {DEVDRV_PCI_SUBSYS_PRIVATE_VENDOR_KL, MINI_V2_DEVICE, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, HISI_MINI_V2},
    {DEVDRV_PCI_SUBSYS_PRIVATE_VENDOR_HK, CLOUD_V2_DEVICE, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, HISI_CLOUD_V2},
    {DEVDRV_PCI_SUBSYS_PRIVATE_VENDOR_KL, CLOUD_V2_DEVICE, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, HISI_CLOUD_V2},
    {DEVDRV_PCI_SUBSYS_PRIVATE_VENDOR_CJ, CLOUD_V2_DEVICE, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, HISI_CLOUD_V2},
    {DEVDRV_PCI_SUBSYS_PRIVATE_VENDOR_CJ, MINI_V2_DEVICE, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, HISI_MINI_V2},
    {}};
KA_MODULE_DEVICE_TABLE(pci, g_devdrv_tbl);

int devdrv_get_device_id_tbl_num(void)
{
    return (int)(sizeof(g_devdrv_tbl) / sizeof(ka_pci_device_id_t));
}

// clang-format off
int (*devdrv_res_init_func[HISI_CHIP_NUM])(struct devdrv_pci_ctrl *pci_ctrl) = {
    [HISI_CLOUD_V1] = devdrv_cloud_v1_res_init,
    [HISI_MINI_V2] = devdrv_mini_v2_res_init,
    [HISI_CLOUD_V2] = devdrv_cloud_v2_res_init,
    [HISI_MINI_V3] = devdrv_mini_v3_res_init,
    [HISI_CLOUD_V4] = devdrv_cloud_v4_res_init,
    [HISI_CLOUD_V5] = devdrv_cloud_v4_res_init,
    [HISI_MINI_V4] = devdrv_cloud_v4_res_init,
    [HISI_CLOUD_V6] = devdrv_cloud_v6_res_init,
    [HISI_MINI_V5] = devdrv_mini_v5_res_init,
};
// clang-format on

void devdrv_peer_ctrl_init(void)
{
    return;
}

/* devdrv_pci.c */
int devdrv_get_product(void)
{
    return HOST_PRODUCT_DC;
}

void devdrv_shutdown(ka_pci_dev_t *pdev)
{
    if (devdrv_get_host_type() == HOST_TYPE_NORMAL) {
        devdrv_info("Shutdown, pci remove driver.\n");
        drv_pcie_remove(pdev);
    }
    return;
}

static u64 g_aer_critical_cnt = 0;
static u64 g_aer_noncritical_cnt = 0;

STATIC ka_pci_ers_result_t devdrv_error_detected(ka_pci_dev_t *pdev, ka_pci_channel_state_t state)
{
    struct devdrv_pci_ctrl *pci_ctrl = devdrv_get_pdev_main_davinci_dev(pdev);
    ka_pci_ers_result_t ret = KA_PCI_ERS_RESULT_NEED_RESET;
    if (pci_ctrl == NULL) {
        devdrv_err("pci_ctrl is invalid.\n");
        return KA_PCI_ERS_RESULT_NEED_RESET;
    }
    devdrv_warn("Error detected. (dev_id=%u;  state=%d)\n", pci_ctrl->dev_id, (int)state);
    devdrv_notify_blackbox_err(pci_ctrl->dev_id, DEVDRV_PCIE_AER_ERROR);
    switch (state) {
        /* I/O channel is in normal state */
        case ka_pci_channel_io_normal:
            g_aer_noncritical_cnt++;
            devdrv_err("Channel is in normal state. (dev_id=%d; aer_noncritical_cnt=%llu)\n", pci_ctrl->dev_id,
                       g_aer_noncritical_cnt);
            ret = KA_PCI_ERS_RESULT_CAN_RECOVER;
            break;

        /* I/O to channel is blocked */
        case ka_pci_channel_io_frozen:
            g_aer_critical_cnt++;
            devdrv_err("Channel is blocked. (dev_id=%u; aer_critical_cnt=%llu)\n", pci_ctrl->dev_id,
                       g_aer_critical_cnt);

            if (ka_pci_is_enabled(pdev) != 0) {
                ka_pci_disable_pcie_error_reporting(pdev);
                ka_pci_clear_master(pdev);
                ka_pci_release_regions(pdev);
                ka_pci_disable_device(pdev);
            }
            ret = KA_PCI_ERS_RESULT_NEED_RESET;
            break;

        /* PCI card is dead */
        case ka_pci_channel_io_perm_failure:
            ret = KA_PCI_ERS_RESULT_DISCONNECT;
            break;
        default:
            ret = KA_PCI_ERS_RESULT_NEED_RESET;
            break;
    }

    /* Request a slot reset. */
    return ret;
}

STATIC ka_pci_ers_result_t devdrv_slot_reset(ka_pci_dev_t *pdev)
{
    u8 byte = 0;
    int ret;

    devdrv_info("Enter slot reset.\n");

    if (ka_pci_enable_device(pdev) != 0) {
        devdrv_err("Call ka_pci_enable_device failed.\n");
        return KA_PCI_ERS_RESULT_DISCONNECT;
    }

    ka_pci_read_config_byte(pdev, 0x8, &byte);
    if (byte == 0xff) {
        devdrv_err("Slot_reset failed. got another pci error.\n");
        return KA_PCI_ERS_RESULT_DISCONNECT;
    }

    if (ka_pci_request_regions(pdev, "devdrv") != 0) {
        devdrv_err("Call ka_pci_request_regions failed.\n");
        return KA_PCI_ERS_RESULT_DISCONNECT;
    }

    /*lint -e598 -e648 */
    if (ka_mm_dma_set_mask_and_coherent(ka_pci_get_dev(pdev), KA_DMA_BIT_MASK(DEVDRV_DMA_BIT_MASK_64)) != 0) {
        /*lint +e598 +e648 */
        devdrv_err("dma_set_mask 64 bit failed.\n");
        ret = ka_mm_dma_set_mask_and_coherent(ka_pci_get_dev(pdev), KA_DMA_BIT_MASK(DEVDRV_DMA_BIT_MASK_32));
        if (ret != 0) {
            devdrv_err("dma_set_mask failed. (ret=%d)\n", ret);
            return KA_PCI_ERS_RESULT_DISCONNECT;
        }
    }

    ka_pci_set_master(pdev);

    devdrv_err("The resume of top business (vnic/hdc/devmm) still needs to be done.\n");

    return KA_PCI_ERS_RESULT_RECOVERED;
}

STATIC void devdrv_error_resume(ka_pci_dev_t *pdev)
{
    devdrv_info("Enter pcie reporting resume.\n");
    ka_pci_aer_clear_status(pdev);
    ka_pci_enable_pcie_error_reporting(pdev);
}

// clang-format off
const ka_pci_error_handlers_t g_devdrv_err_handler = {
    ka_pci_error_detected(devdrv_error_detected)
    ka_pci_slot_reset(devdrv_slot_reset)
    ka_pci_resume(devdrv_error_resume)
};
// clang-format on

/* devdrv_dma.c */
void devdrv_traffic_and_manage_dma_chan_config(struct devdrv_dma_dev *dma_dev)
{
    dma_dev->data_chan[DEVDRV_DMA_DATA_TRAFFIC].chan_start_id = dma_dev->data_chan[DEVDRV_DMA_DATA_PCIE_MSG]
                                                                    .chan_start_id +
                                                                dma_dev->data_chan[DEVDRV_DMA_DATA_PCIE_MSG].chan_num;
    if (dma_dev->remote_chan_num <= dma_dev->data_chan[DEVDRV_DMA_DATA_TRAFFIC].chan_start_id) {
        dma_dev->data_chan[DEVDRV_DMA_DATA_TRAFFIC].chan_start_id = 0;
    }
    dma_dev->data_chan[DEVDRV_DMA_DATA_TRAFFIC].chan_num = dma_dev->remote_chan_num -
                                                           dma_dev->data_chan[DEVDRV_DMA_DATA_TRAFFIC].chan_start_id;
    dma_dev->data_chan[DEVDRV_DMA_DATA_TRAFFIC].last_use_chan = dma_dev->data_chan[DEVDRV_DMA_DATA_TRAFFIC]
                                                                    .chan_start_id;
}

#ifndef CFG_FEATURE_PCIE_SENTRY
bool devdrv_is_sentry_work_mode(void)
{
    return false;
}
#endif

void devdrv_load_half_resume(struct devdrv_pci_ctrl *pci_ctrl)
{
    return;
}

__attribute__((unused)) int drv_pcie_suspend(ka_device_t *dev)
{
    return 0;
}

__attribute__((unused)) int drv_pcie_resume_notify(ka_device_t *dev)
{
    return 0;
}

STATIC void devdrv_feature_set_ops_bitmap(u32 *bitmap, u32 val, u32 feat, u32 *offset)
{
    u32 bit = *offset;
    if (val == feat) {
        (*bitmap) |= (u32)KA_BASE_BIT(bit);
    }
    (*offset)++;
}

devdrv_feature_ops_bitmap_t devdrv_feature_get_rescfg_bitmap(struct devdrv_pci_ctrl *pci_ctrl)
{
    devdrv_feature_ops_bitmap_t bitmap = 0;
    u32 offset = 0;

    /* Each bit in the bitmap is fixed and cannot be changed arbitrarily. The bit sequence cannot
       be altered, and new bits can only be added at the end. */
    devdrv_feature_set_ops_bitmap(&bitmap, pci_ctrl->virtfn_flag, DEVDRV_SRIOV_TYPE_PF, &offset);
    devdrv_feature_set_ops_bitmap(&bitmap, pci_ctrl->virtfn_flag, DEVDRV_SRIOV_TYPE_VF, &offset);
    devdrv_feature_set_ops_bitmap(&bitmap, pci_ctrl->connect_protocol, CONNECT_PROTOCOL_PCIE, &offset);
    devdrv_feature_set_ops_bitmap(&bitmap, pci_ctrl->connect_protocol, CONNECT_PROTOCOL_HCCS, &offset);
    devdrv_feature_set_ops_bitmap(&bitmap, pci_ctrl->env_boot_mode, DEVDRV_PHY_BOOT, &offset);
    devdrv_feature_set_ops_bitmap(&bitmap, pci_ctrl->env_boot_mode, DEVDRV_SRIOV_VF_BOOT, &offset);
    devdrv_feature_set_ops_bitmap(&bitmap, pci_ctrl->env_boot_mode, DEVDRV_MDEV_FULL_SPEC_VF_PM_BOOT, &offset);
    devdrv_feature_set_ops_bitmap(&bitmap, pci_ctrl->env_boot_mode, DEVDRV_MDEV_FULL_SPEC_VF_VM_BOOT, &offset);
    devdrv_feature_set_ops_bitmap(&bitmap, pci_ctrl->env_boot_mode, DEVDRV_MDEV_VF_PM_BOOT, &offset);
    devdrv_feature_set_ops_bitmap(&bitmap, pci_ctrl->env_boot_mode, DEVDRV_MDEV_VF_VM_BOOT, &offset);

    return bitmap;
}

struct res_config *devdrv_feature_get_res_cfg(struct res_config *res_cfg, size_t res_cfg_size,
                                              devdrv_feature_ops_bitmap_t mode)
{
    u32 i;

    for (i = 0; i < res_cfg_size; i++) {
        if (res_cfg[i].mode == mode) {
            return &res_cfg[i];
        }
    }

    return NULL;
}
