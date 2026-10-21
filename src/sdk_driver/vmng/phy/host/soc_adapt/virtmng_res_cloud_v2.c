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
#include "virtmnghost_pci.h"
#include "virtmng_res_comm.h"
#include "virtmng_res_cloud_v2.h"

STATIC int vmngh_check_pci_reset_finish(u32 dev_id)
{
    int count = 0;

    while ((devdrv_check_half_probe_finish(dev_id) == false) && (count < VMNG_WAIT_REMOTE_ID_S)) {
        ka_system_ssleep(1);
        count++;
    }

    if (count >= VMNG_WAIT_REMOTE_ID_S) {
        vmng_err("flr_finish >= %ds (dev_id=%d)\n", VMNG_WAIT_REMOTE_ID_S, dev_id);
        return VMNG_ERR;
    }

    return VMNG_OK;
}

STATIC int vmngh_sriov_reset_vdev_cloud_v2(u32 dev_id)
{
    struct uda_mia_dev_para mia_para;
    u32 pf_id, vf_id;
    int ret = 0;

    vmng_info("vmngh_sriov_reset_vdev.(dev_id=%u)\n", dev_id);

    if (uda_udevid_to_mia_devid(dev_id, &mia_para) < 0) {
        vmng_err("Get pfvf id by devid error. (dev_id=%u)\n", dev_id);
        return VMNG_ERR;
    }
    pf_id = mia_para.phy_devid;
    vf_id = mia_para.sub_devid + 1;

    if ((pf_id >= ASCEND_PDEV_MAX_NUM) || (vf_id >= VMNG_VDEV_MAX_PER_PDEV)) {
        vmng_err("Get pf_id or vf_id invalid. (dev_id=%u; pf_id=%u; vf_id=%u)\n", dev_id, pf_id, vf_id);
        return -EINVAL;
    }

    if (!is_sriov_enable(pf_id)) {
        vmng_err("Sriov not enable. (dev_id=%u)\n", dev_id);
        return VMNG_ERR;
    }
    ka_task_mutex_lock(&vmngh_get_ctrl(pf_id, vf_id)->reset_mutex);

    ret = vmngh_container_client_offline_comm(pf_id, vf_id);
    if (ret != 0) {
        ka_task_mutex_unlock(&vmngh_get_ctrl(pf_id, vf_id)->reset_mutex);
        vmng_err("Vf device offline failed.(dev_id=%u; pf_id=%u; vfid=%u; ret=%d)\n", dev_id, pf_id, vf_id, ret);
        return ret;
    }

    if (devdrv_hot_reset_device(dev_id) < 0) {
        vmng_err("devdrv_hot_reset_device error.(dev_id=%u)\n", dev_id);
        ret = VMNG_ERR;
        ka_system_ssleep(VMNG_WAIT_RESET_S); // wait for device RM process finish
        goto vf_recover;
    }

    if (vmngh_check_pci_reset_finish(dev_id) != VMNG_OK) {
        vmng_err("Check pci reset finish timeout. (dev_id=%u)\n", dev_id);
    }
vf_recover:
    if (vmngh_init_instance_client_device(pf_id, vf_id) != VMNG_OK) {
        vmng_err("Init instance all client device failed. (dev_id=%u; pf_id=%u; vfid=%u)\n", dev_id, pf_id, vf_id);
        ret = VMNG_ERR;
    }

    if (vmngh_ctrl_sriov_init_instance(pf_id, vf_id) != VMNG_OK) {
        vmng_err("pci init instance error. (dev_id=%u; pf_id=%u; vfid=%u; ret=%d)\n", dev_id, pf_id, vf_id, ret);
        ret = VMNG_ERR;
    }

    if (vmngh_init_instance_all_client(pf_id, vf_id, VMNGH_CONTAINER) != VMNG_OK) {
        vmng_err("Init instance all client failed. (dev_id=%u; pf_id=%u; vfid=%u)\n", dev_id, pf_id, vf_id);
        ret = VMNG_ERR;
    }

    if (vmngh_add_mia_dev(pf_id, vf_id, 0) != 0) {
        ret = VMNG_ERR;
        vmng_err("Call vmngh_add_mia_dev failed. (dev_id=%u; pf_id=%u; vfid=%u)\n", dev_id, pf_id, vf_id);
    }

    ka_task_mutex_unlock(&vmngh_get_ctrl(pf_id, vf_id)->reset_mutex);
    ka_system_ssleep(VMNG_WAIT_RESET_S);

    return ret;
}

int vmngh_res_init_cloud_v2(u32 devid, struct vmngh_ctrl_ops *ops)
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
    ops->reset_vf = vmngh_sriov_reset_vdev_cloud_v2;
    ops->sriov_init_instance = devdrv_sriov_init_instance;
    ops->sriov_uninit_instance = devdrv_sriov_uninit_instance;
    ops->container_client_online = vmngh_container_client_online_comm;
    ops->container_client_offline = vmngh_container_client_offline_comm;
    ops->valid = VMNG_VALID;

    return VMNG_OK;
}

vmng_feature_bitmap_t vmngh_feature_init_cloud_v2(void)
{
    return (1ULL << VMNG_FEATURE_MIA_RES) | (1ULL << VMNG_FEATURE_VM_FULL_VF_EN);
}
