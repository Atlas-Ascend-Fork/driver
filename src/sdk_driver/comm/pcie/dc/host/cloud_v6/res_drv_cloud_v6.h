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

#ifndef _RES_DRV_CLOUD_V6_H_
#define _RES_DRV_CLOUD_V6_H_

#include "devdrv_pci.h"

#define DEVDRV_S2S_MAX_CHIP_NUM 8
#define DEVDRV_CLOUD_V6_MODULE_ID_MAX 16U
#define UB_REMOTE_PORT_NUM 3U
#define UB_REMOTE_DEVICE_NUM 5U
#define UB_HW_INFO_RSV2_NUM 10U

int devdrv_cloud_v6_res_init(struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_cloud_v6_init_pf_bar_addr_info(struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_cloud_v6_init_vf_bar_addr_info(struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_cloud_v6_init_pf_intr_info(struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_cloud_v6_init_vf_intr_info(struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_cloud_v6_init_pf_dma_info(struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_cloud_v6_init_vf_dma_info(struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_cloud_v6_init_load_file_info(struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_cloud_v6_init_depend_module_info(struct devdrv_pci_ctrl *pci_ctrl);
int devdrv_cloud_v6_get_pf_msg_chan_cnt(void);
int devdrv_cloud_v6_get_vf_msg_chan_cnt(void);
int devdrv_cloud_v6_alloc_devid(struct devdrv_ctrl *ctrl_this);
u32 devdrv_cloud_v6_get_p2p_support_max_devnum(void);
int devdrv_cloud_v6_is_p2p_access_cap(struct devdrv_pci_ctrl *pci_ctrl, struct devdrv_pci_ctrl *peer_pci_ctrl);
bool devdrv_cloud_v6_is_mdev_vm_full_spec(struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_cloud_v6_set_dev_shr_info(struct devdrv_pci_ctrl *pci_ctrl);
bool devdrv_cloud_v6_is_need_reorder(struct devdrv_pci_ctrl *pci_ctrl, void __ka_mm_iomem *para_addr);
int devdrv_cloud_v6_set_udevid_reorder_para(struct devdrv_pci_ctrl *pci_ctrl);
u32 devdrv_cloud_v6_get_nvme_low_level_db_irq_num(void);
u32 devdrv_cloud_v6_get_nvme_db_irq_strde(void);
void devdrv_cloud_v6_init_pf_msg_cnt(struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_cloud_v6_init_vf_msg_cnt(struct devdrv_pci_ctrl *pci_ctrl);
int devdrv_cloud_v6_init_pf_bar_info(struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_cloud_v6_vf_boot_mode_rebuild(struct devdrv_pci_ctrl *pci_ctrl);
int devdrv_cloud_v6_init_vf_bar_info(struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_cloud_v6_init_pf_setup_runtime_info(struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_cloud_v6_init_vf_setup_runtime_info(struct devdrv_pci_ctrl *pci_ctrl);
enum devdrv_load_wait_mode devdrv_cloud_v6_get_load_wait_mode(struct devdrv_pci_ctrl *pci_ctrl);

#endif
