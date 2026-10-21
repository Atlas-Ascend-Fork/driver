/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "devdrv_manager_common.h"
#include "devdrv_pm.h"
#include "devmng_dms_adapt.h"
#include "devdrv_manager_msg.h"
#include "devdrv_platform_resource.h"
#include "devdrv_pcie.h"
#include "devdrv_common.h"
#include "hvdevmng_init.h"
#include "dev_mnt_vdevice.h"
#include "vmng_kernel_interface.h"
#include "pbl_mem_alloc_interface.h"
#include "devdrv_user_common.h"
#include "dms/dms_devdrv_manager_comm.h"
#include "ka_base_pub.h"
#include "ka_kernel_def_pub.h"
#include "devdrv_manager.h"
#include "devdrv_manager_ioctl.h"

STATIC int devdrv_manager_get_dev_resource_info(struct devdrv_resource_info *dinfo)
{
    struct devdrv_manager_msg_resource_info info;
    u32 phy_id = 0;
    u32 vfid = 0;
    int ret;

    if ((dinfo->resource_type == DEVDRV_DEV_PROCESS_PID) || (dinfo->resource_type == DEVDRV_DEV_PROCESS_MEM) ||
        (dinfo->resource_type == DEVDRV_DEV_PROCESS_CONTAINER_PID)) {
        return -EOPNOTSUPP;
    }

#ifndef CFG_FEATURE_DDR
    if ((dinfo->resource_type == DEVDRV_DEV_DDR_TOTAL) || (dinfo->resource_type == DEVDRV_DEV_DDR_FREE)) {
        return -EOPNOTSUPP;
    }
#endif

    ret = devdrv_manager_container_logical_id_to_physical_id(dinfo->devid, &phy_id, &vfid);
    if (ret) {
        devdrv_drv_err("logical_id_to_physical_id fail, devid(%u) ret(%d).\n", dinfo->devid, ret);
        return ret;
    }

    info.vfid = vfid;
    info.info_type = dinfo->resource_type;
    info.owner_id = dinfo->owner_id;
    ret = hvdevmng_get_dev_resource(phy_id, dinfo->tsid, &info);
    if (ret) {
        devdrv_drv_err("get resource info failed, devid(%u), resource type (%u) ret(%d).\n", dinfo->devid,
                       dinfo->resource_type, ret);
        return ret;
    }
    *((u64 *)dinfo->buf) = info.value;

    return 0;
}

STATIC int devdrv_manager_get_hostpid(ka_pid_t container_pid, ka_pid_t *hostpid)
{
    ka_struct_pid_t *pgrp = NULL;

    if (devdrv_manager_container_is_in_container()) {
        pgrp = ka_task_find_get_pid(container_pid);
        if (pgrp == NULL) {
            devdrv_drv_err("The pgrp parameter is NULL.\n");
            return -EINVAL;
        }
        *hostpid = pgrp->numbers[0].nr; /* 0:hostpid */
        ka_task_put_pid(pgrp);
    } else {
        *hostpid = container_pid;
    }

    return 0;
}

STATIC int devdrv_manager_get_process_memory(u32 fid, u32 phyid, struct devdrv_resource_info *dinfo)
{
    int ret;
    ka_pid_t hostpid = -1;
    struct devdrv_manager_msg_resource_info resource_info;

    if (fid >= VMNG_VDEV_MAX_PER_PDEV || dinfo->buf_len > DEVDRV_MAX_PAYLOAD_LEN) {
        devdrv_drv_err("Invalid parameter. (fid=%d; buf_len=%d)\n", fid, dinfo->buf_len);
        return -EINVAL;
    }

    ret = devdrv_manager_get_hostpid(dinfo->owner_id, &hostpid);
    if (ret != 0) {
        devdrv_drv_err("devdrv_manager_get_hostpid failed. (owner_id=%d; hostpid=%d; ret=%d;)\n", dinfo->owner_id,
                       hostpid, ret);
        return -EINVAL;
    }

    resource_info.vfid = fid;
    resource_info.info_type = dinfo->resource_type;
    resource_info.owner_id = hostpid;
    ret = devdrv_manager_h2d_query_resource_info(phyid, &resource_info);
    if (ret) {
        devdrv_drv_ex_notsupport_err(ret, "h2d_query_resource_info failed. (devid=%u; info_type=%d; ret=%d)\n",
                                     dinfo->devid, dinfo->resource_type, ret);
        return ret;
    }
    *((u64 *)dinfo->buf) = resource_info.value;
    if (dinfo->buf_len > sizeof(u64)) {
        dinfo->buf_len = sizeof(u64);
    }

    return 0;
}

STATIC int devdrv_manager_get_process_resource_info(struct devdrv_resource_info *dinfo)
{
    int ret;
    u32 phy_id = 0;
    u32 vfid = 0;

    if (dinfo->devid >= ASCEND_DEV_MAX_NUM) {
        devdrv_drv_err("Invalid parameter. (devid=%u)\n", dinfo->devid);
        return -EINVAL;
    }
    ret = devdrv_manager_container_logical_id_to_physical_id(dinfo->devid, &phy_id, &vfid);
    if (ret) {
        devdrv_drv_err("Logical id to physical id failed. (devid=%u; phy_id=%d; ret=%d)\n", dinfo->devid, phy_id, ret);
        return ret;
    }

    switch (dinfo->resource_type) {
        case DEVDRV_DEV_PROCESS_PID:
            ret = devdrv_manager_get_accounting_pid(phy_id, vfid, dinfo);
            break;
        case DEVDRV_DEV_PROCESS_MEM:
            ret = devdrv_manager_get_process_memory(vfid, phy_id, dinfo);
            break;
#ifdef CFG_FEATURE_ADMIN_CONTAINER_PID
        case DEVDRV_DEV_PROCESS_CONTAINER_PID:
            ret = devdrv_manager_get_accounting_pid(phy_id, vfid, dinfo);
            break;
#endif
        default:
            devdrv_drv_err("Invalid device process resource type. (type=%d)", dinfo->resource_type);
            return -EINVAL;
    }

    if (ret) {
        devdrv_drv_ex_notsupport_err(ret, "devdrv_manager_get_process_resource_info failed. (ret=%d; type=%d)\n", ret,
                                     dinfo->resource_type);
    }
    return ret;
}

STATIC int (*const dmanage_get_resource_handler[DEVDRV_MAX_OWNER_TYPE])(struct devdrv_resource_info *dinfo) = {
    [DEVDRV_DEV_RESOURCE] = devdrv_manager_get_dev_resource_info,
    [DEVDRV_VDEV_RESOURCE] = devdrv_manager_get_vdev_resource_info,
    [DEVDRV_PROCESS_RESOURCE] = devdrv_manager_get_process_resource_info,
};

int devdrv_manager_ioctl_get_dev_resource_info(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    struct devdrv_resource_info dinfo = {0};
    int ret;

    if (copy_from_user_safe(&dinfo, (void *)(uintptr_t)arg, sizeof(dinfo))) {
        return -EFAULT;
    }

    if (dinfo.owner_type >= DEVDRV_MAX_OWNER_TYPE) {
        devdrv_drv_err("devid(%u) invalid owner_type(%u).\n", dinfo.devid, dinfo.owner_type);
        return -EINVAL;
    }

    if (dmanage_get_resource_handler[dinfo.owner_type] == NULL) {
        return -EOPNOTSUPP;
    }

    ret = dmanage_get_resource_handler[dinfo.owner_type](&dinfo);
    if (ret) {
        devdrv_drv_ex_notsupport_err(ret, "get dev resource info failed devid(%u) owner_type(%u), ret = %d.\n",
                                     dinfo.devid, dinfo.owner_type, ret);
        return ret;
    }

    if (copy_to_user_safe((void *)(uintptr_t)arg, &dinfo, sizeof(dinfo))) {
        devdrv_drv_err("copy to user failed.\n");
        return -EFAULT;
    }

    return 0;
}

devmm_get_device_accounting_pids_ops get_device_pids_from_devmm = NULL;
static KA_TASK_DEFINE_MUTEX(devmm_get_pids_mutex);
int devdrv_manager_get_process_pids_register(devmm_get_device_accounting_pids_ops func)
{
    if (func == NULL) {
        devdrv_drv_err("Register pids operation function null.\n");
        return -EINVAL;
    }

    ka_task_mutex_lock(&devmm_get_pids_mutex);
    get_device_pids_from_devmm = func;
    ka_task_mutex_unlock(&devmm_get_pids_mutex);
    return 0;
}
KA_EXPORT_SYMBOL(devdrv_manager_get_process_pids_register);

void devdrv_manager_get_process_pids_unregister(void)
{
    ka_task_mutex_lock(&devmm_get_pids_mutex);
    get_device_pids_from_devmm = NULL;
    ka_task_mutex_unlock(&devmm_get_pids_mutex);
}
KA_EXPORT_SYMBOL(devdrv_manager_get_process_pids_unregister);

STATIC int devdrv_manager_get_container_pid(ka_pid_t hostpid, ka_pid_t *container_pid)
{
    ka_struct_pid_t *pgrp = NULL;
    ka_task_struct_t *tsk = NULL;

    ka_task_rcu_read_lock();
    ka_for_each_process(tsk)
    {
        if ((tsk != NULL) && (tsk->pid == hostpid)) {
            pgrp = ka_task_task_pid(tsk);
            if (pgrp == NULL) {
                ka_task_rcu_read_unlock();
                devdrv_drv_err("The process group parameter is NULL.\n");
                return -EINVAL;
            }
            *container_pid = pgrp->numbers[pgrp->level].nr;
            break;
        }
    }
    ka_task_rcu_read_unlock();

    return 0;
}

STATIC int devdrv_trans_ordinary_container_pid(u32 docker_id, int out_cnt, struct devdrv_resource_info *dinfo)
{
    ka_pid_t pid = -1;
    int ret;
    int i;

    if (docker_id < MAX_DOCKER_NUM) {
        for (i = 0; i < out_cnt; i++) {
            ret = devdrv_manager_get_container_pid(((ka_pid_t *)dinfo->buf)[i], &pid);
            if (ret != 0) {
                devdrv_drv_err("devdrv_manager_get_container_pid failed. (devid=%u; hostpid=%d; ret=%d)\n",
                               dinfo->devid, ((u32 *)dinfo->buf)[i], ret);
                return ret;
            }
            ((ka_pid_t *)dinfo->buf)[i] = pid;
        }
    }

    return 0;
}

STATIC int devdrv_trans_admin_container_pid(u32 docker_id, int out_cnt, struct devdrv_resource_info *dinfo)
{
    ka_pid_t pid = -1;
    int ret;
    int i;

    if (docker_id < MAX_DOCKER_NUM || devdrv_manager_container_is_in_admin_container()) {
        for (i = 0; i < out_cnt; i++) {
            ret = devdrv_manager_get_container_pid(((ka_pid_t *)dinfo->buf)[i], &pid);
            if (ret != 0) {
                devdrv_drv_err("devdrv_manager_get_container_pid failed. (devid=%u; hostpid=%d; ret=%d)\n",
                               dinfo->devid, ((u32 *)dinfo->buf)[i], ret);
                return ret;
            }
            ((ka_pid_t *)dinfo->buf)[i] = pid;
        }
    }

    return 0;
}

int devdrv_manager_get_accounting_pid(u32 phyid, u32 vfid, struct devdrv_resource_info *dinfo)
{
    int ret;
    u32 docker_id = 0;
    int out_cnt = 0;

    if (dinfo->buf_len > DEVDRV_MAX_PAYLOAD_LEN) {
        dinfo->buf_len = DEVDRV_MAX_PAYLOAD_LEN;
    }

    ret = devdrv_manager_get_docker_id(&docker_id);
    if (ret) {
        devdrv_drv_err("The devdrv_manager_container_get_docker_id failed. (devid=%u; docker_id=%d; ret=%d)\n",
                       dinfo->devid, docker_id, ret);
        return ret;
    }

    ka_task_mutex_lock(&devmm_get_pids_mutex);
    if (get_device_pids_from_devmm == NULL) {
        ka_task_mutex_unlock(&devmm_get_pids_mutex);
        devdrv_drv_err("The devmm_get_device_accounting_pids is NULL.\n");
        return -EINVAL;
    }

    out_cnt = get_device_pids_from_devmm(docker_id, phyid, vfid, (ka_pid_t *)dinfo->buf,
                                         (dinfo->buf_len / sizeof(ka_pid_t)));
    if (out_cnt < 0) {
        ka_task_mutex_unlock(&devmm_get_pids_mutex);
        devdrv_drv_err("Failed to obtain the PID list of the process from SVM. (devid=%u; docker_id=%d; count=%d)\n",
                       dinfo->devid, docker_id, out_cnt);
        return -EINVAL;
    }
    ka_task_mutex_unlock(&devmm_get_pids_mutex);

    if (out_cnt > DEVDRV_MAX_PAYLOAD_LEN / sizeof(ka_pid_t)) {
        out_cnt = DEVDRV_MAX_PAYLOAD_LEN / sizeof(ka_pid_t);
    }

    if (dinfo->resource_type == DEVDRV_DEV_PROCESS_PID) {
        ret = devdrv_trans_ordinary_container_pid(docker_id, out_cnt, dinfo);
    } else if (dinfo->resource_type == DEVDRV_DEV_PROCESS_CONTAINER_PID) {
        ret = devdrv_trans_admin_container_pid(docker_id, out_cnt, dinfo);
    } else {
        return -EOPNOTSUPP;
    }

    if (ret != 0) {
        devdrv_drv_err("Failed to convert container pid. (devid=%u; docker_id=%d; resource_type=%d; ret=%d)\n",
                       dinfo->devid, docker_id, dinfo->resource_type, ret);
        return ret;
    }

    dinfo->buf_len = out_cnt * sizeof(ka_pid_t);

    return 0;
}
