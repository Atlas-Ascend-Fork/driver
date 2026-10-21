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

#include "comm_kernel_interface.h"

#include "virtmng_public_def.h"
#include "virtmnghost_ctrl.h"
#include "virtmng_res_comm.h"
#include "virtmng_res_cloud_v4.h"

int vmngh_res_init_cloud_v4(u32 devid, struct vmngh_ctrl_ops *ops)
{
    if (ops == NULL) {
        vmng_err("Invalid parameter.\n");
        return VMNG_ERR;
    }

    ops->enable_sriov = vmngh_res_enable_sriov_comm;
    ops->disable_sriov = vmngh_res_disable_sriov_comm;
    ops->alloc_vf = vmngh_alloc_vf_comm;
    ops->free_vf = vmngh_free_vf_comm;
    ops->alloc_vfid = vmngh_alloc_vfid_comm;
    ops->free_vfid = vmngh_free_vfid_comm;
    ops->refresh_vf = vmngh_refresh_vdev_resource_comm;
    ops->enquire_vf = vmngh_enquire_soc_resource_comm;
    ops->container_client_online = vmngh_container_client_online_comm;
    ops->container_client_offline = vmngh_container_client_offline_comm;

    if (devdrv_get_connect_protocol(devid) == CONNECT_PROTOCOL_PCIE) {
        ops->sriov_init_instance = devdrv_sriov_init_instance;
        ops->sriov_uninit_instance = devdrv_sriov_uninit_instance;
    }

    ops->valid = VMNG_VALID;

    return VMNG_OK;
}

vmng_feature_bitmap_t vmngh_feature_init_cloud_v4(void)
{
    return 0;
}
