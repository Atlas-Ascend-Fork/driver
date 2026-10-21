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

#include "ka_task_pub.h"
#include "ka_kernel_def_pub.h"
#include "devdrv_device_status.h"
#include "devdrv_common.h"
#include "devdrv_manager_msg.h"
#include "devdrv_user_common.h"
#include "devdrv_manager_container.h"
#include "devdrv_manager.h"
#include "tsdrv_status.h"
#include "tsdrv_kernel_common.h"
#include "devdrv_manager_common.h"

static u32 g_device_process_status[ASCEND_DEV_MAX_NUM] = {0};
static KA_TASK_DEFINE_MUTEX(g_device_process_status_mutex);

void devdrv_manager_device_status_init(u32 dev_id)
{
    g_device_process_status[dev_id] = 0;
}

STATIC int dms_get_device_startup_status_form_bar(struct devdrv_info *dev_info, unsigned int *dmp_started,
                                                  unsigned int *device_process_status)
{
    if ((dev_info->shm_head == NULL) || (dev_info->shm_status == NULL) ||
        (dev_info->shm_head->head_info.magic != DEVMNG_SHM_INFO_HEAD_MAGIC)) {
        *dmp_started = false;
        *device_process_status = DSMI_BOOT_STATUS_UNINIT;
    } else if (dev_info->shm_head->head_info.version != DEVMNG_SHM_INFO_HEAD_VERSION) {
        devdrv_drv_err("dev(%u) version of share memory in host is 0x%llx, "
                       "the version in device is 0x%llx, magic is 0x%x.\n",
                       dev_info->dev_id, DEVMNG_SHM_INFO_HEAD_VERSION, dev_info->shm_head->head_info.version,
                       dev_info->shm_head->head_info.magic);
        return -EINVAL;
    } else {
        if (dev_info->dmp_started == false) {
            dev_info->dmp_started = devdrv_manager_h2d_query_dmp_started(dev_info->dev_id);
        }

        *dmp_started = dev_info->dmp_started;
        *device_process_status = (unsigned int)dev_info->shm_status->os_status;
    }

    return 0;
}

#define DEVDRV_SYSTEM_START_FINISH 16
int dms_get_device_startup_status_form_device(struct devdrv_info *dev_info, unsigned int *dmp_started,
                                              unsigned int *device_process_status)
{
    int ret = 0;

    if (dev_info->dmp_started == false) {
        dev_info->dmp_started = devdrv_manager_h2d_query_dmp_started(dev_info->dev_id);
    }
    *dmp_started = dev_info->dmp_started;

    ka_task_mutex_lock(&g_device_process_status_mutex);
    if (g_device_process_status[dev_info->dev_id] == DEVDRV_SYSTEM_START_FINISH) {
        *device_process_status = DEVDRV_SYSTEM_START_FINISH;
        ka_task_mutex_unlock(&g_device_process_status_mutex);
        return 0;
    }
    ret = devdrv_manager_h2d_get_device_process_status(dev_info->dev_id, &g_device_process_status[dev_info->dev_id]);
    if (ret != 0) {
        devdrv_drv_err("Failed to obtain the device process status through H2D. (dev_id=%u; ret=%d)", dev_info->dev_id,
                       ret);
        ka_task_mutex_unlock(&g_device_process_status_mutex);
        return ret;
    }
    *device_process_status = g_device_process_status[dev_info->dev_id];
    ka_task_mutex_unlock(&g_device_process_status_mutex);
    return 0;
}

int devdrv_manager_get_device_startup_status(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    int ret;
    unsigned int phys_id;
    unsigned int vfid = 0;
    struct devdrv_info *dev_info = NULL;
    struct devdrv_device_work_status para = {0};
    int connect_type = CONNECT_PROTOCOL_UNKNOWN;

    ret = copy_from_user_safe(&para, (void *)(uintptr_t)arg, sizeof(struct devdrv_device_work_status));
    if (ret != 0) {
        devdrv_drv_err("copy from user failed, ret(%d).\n", ret);
        return -EINVAL;
    }

    ret = devdrv_manager_container_logical_id_to_physical_id(para.device_id, &phys_id, &vfid);
    if (ret != 0) {
        devdrv_drv_err("can't transform virt id %u \n", para.device_id);
        return -EFAULT;
    }

    dev_info = devdrv_manager_get_devdrv_info(phys_id);
    if (dev_info == NULL) {
        para.dmp_started = false;
        para.device_process_status = DSMI_BOOT_STATUS_UNINIT;
        goto startup_status_out;
    }

    ka_base_atomic_inc(&dev_info->occupy_ref);
    if (dev_info->status == DEVINFO_STATUS_REMOVED) {
        ka_base_atomic_dec(&dev_info->occupy_ref);
        para.dmp_started = false;
        para.device_process_status = DSMI_BOOT_STATUS_UNINIT;
        goto startup_status_out;
    }

    connect_type = devdrv_get_connect_protocol(phys_id);
    if (connect_type < 0) {
        devdrv_drv_err("Get host device connect type failed. (dev_id=%u; ret=%d)\n", phys_id, connect_type);
        ka_base_atomic_dec(&dev_info->occupy_ref);
        return -EINVAL;
    } else if (connect_type == CONNECT_PROTOCOL_UB) {
        ret = dms_get_device_startup_status_form_device(dev_info, &para.dmp_started, &para.device_process_status);
    } else {
        ret = dms_get_device_startup_status_form_bar(dev_info, &para.dmp_started, &para.device_process_status);
    }
    if (ret != 0) {
        devdrv_drv_err("Failed to obtain the device process status. (dev_id=%u; ret=%d)\n", phys_id, ret);
        ka_base_atomic_dec(&dev_info->occupy_ref);
        return ret;
    }
    ka_base_atomic_dec(&dev_info->occupy_ref);

startup_status_out:
    ret = copy_to_user_safe((void *)(uintptr_t)arg, &para, sizeof(struct devdrv_device_work_status));
    if (ret != 0) {
        devdrv_drv_err("copy to user failed, ret(%d)\n", ret);
        return -EINVAL;
    }

    return 0;
}

int devdrv_manager_get_device_health_status(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    int ret;
    unsigned int phys_id;
    unsigned int vfid = 0;
    struct devdrv_info *dev_info = NULL;
    struct devdrv_device_health_status para;

    ret = copy_from_user_safe(&para, (void *)(uintptr_t)arg, sizeof(struct devdrv_device_health_status));
    if (ret != 0) {
        devdrv_drv_err("copy from user failed, ret(%d).\n", ret);
        return -EINVAL;
    }

    ret = devdrv_manager_container_logical_id_to_physical_id(para.device_id, &phys_id, &vfid);
    if (ret != 0) {
        devdrv_drv_err("can't transform virt id %u \n", para.device_id);
        return -EFAULT;
    }

    dev_info = devdrv_manager_get_devdrv_info(phys_id);
    ret = devdrv_try_get_dev_info_occupy(dev_info);
    if (ret != 0) {
        devdrv_drv_err("Get dev_info occupy failed. (ret=%d; devid=%u)\n", ret, phys_id);
        return ret;
    }

    if (devdrv_manager_shm_info_check(dev_info)) {
        devdrv_drv_err("devid(%u) shm info check fail.\n", phys_id);
        devdrv_put_dev_info_occupy(dev_info);
        return -EFAULT;
    }

    para.device_health_status = (unsigned int)dev_info->shm_status->health_status;
    devdrv_put_dev_info_occupy(dev_info);

    ret = copy_to_user_safe((void *)(uintptr_t)arg, &para, sizeof(struct devdrv_device_health_status));
    if (ret != 0) {
        devdrv_drv_err("copy to user failed, ret(%d)\n", ret);
        return -EINVAL;
    }

    return 0;
}

int devdrv_manager_get_device_status(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    enum devdrv_ts_status ts_status;
    u32 loc_id;
    u32 vfid = 0;
    u32 status;
    int ret;

    u32 phys_id = ASCEND_DEV_MAX_NUM + 1;
    ret = copy_from_user_safe(&loc_id, (void *)(uintptr_t)arg, sizeof(u32));
    if (ret) {
        devdrv_drv_err("copy from user failed, ret(%d).\n", ret);
        return -EINVAL;
    }
    if (loc_id >= ASCEND_DEV_MAX_NUM) {
        devdrv_drv_err("invalid dev_id(%u).\n", loc_id);
        return -EINVAL;
    }

    if (devdrv_manager_container_logical_id_to_physical_id(loc_id, &phys_id, &vfid) != 0) {
        devdrv_drv_err("can't transform virt id %u.\n", loc_id);
        return -EFAULT;
    }

    ts_status = tsdrv_get_ts_status(phys_id, 0);

    if ((devdrv_get_manager_info() == NULL) || (devdrv_get_manager_info()->dev_info[phys_id] == NULL)) {
        status = DRV_STATUS_INITING;
    } else if (devdrv_get_manager_info()->device_status[phys_id] == DRV_STATUS_COMMUNICATION_LOST) {
        status = DRV_STATUS_COMMUNICATION_LOST;
    } else if (ts_status == TS_DOWN) {
        status = DRV_STATUS_EXCEPTION;
    } else if (ts_status == TS_WORK) {
        status = DRV_STATUS_WORK;
    } else {
        status = DRV_STATUS_INITING;
    }

    ret = copy_to_user_safe((void *)(uintptr_t)arg, &status, sizeof(u32));
    if (ret) {
        devdrv_drv_err("copy to user failed, ret(%d). dev_id(%u)\n", ret, phys_id);
        return -EINVAL;
    }

    return 0;
}