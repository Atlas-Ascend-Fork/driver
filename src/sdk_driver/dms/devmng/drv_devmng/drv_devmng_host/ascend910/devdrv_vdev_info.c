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
#include "ka_task_pub.h"
#include "devdrv_manager.h"
#include "devdrv_manager_common.h"
#include "devdrv_pcie.h"
#include "dms/dms_devdrv_manager_comm.h"
#include "vmng_kernel_interface.h"
#include "svm_kernel_interface.h"

#define DEVMNG_SET_ALL_DEV 0xFFFFFFFFU

STATIC int (*dms_set_dev_info_handlers[DMS_DEV_INFO_TYPE_MAX])(u32 devid, const void *buf, u32 buf_size);
static KA_TASK_DEFINE_MUTEX(dev_info_handler_mutex);

#ifndef DEVDRV_MANAGER_HOST_UT_TEST
int dms_register_set_dev_info_handler(DMS_DEV_INFO_TYPE type, dms_set_dev_info_ops func)
{
    if (func == NULL) {
        devdrv_drv_err("Register function of setting device information is null.\n");
        return -EINVAL;
    }

    if (type >= DMS_DEV_INFO_TYPE_MAX) {
        devdrv_drv_err("Register type is error. (type=%u)\n", type);
        return -EINVAL;
    }
    ka_task_mutex_lock(&dev_info_handler_mutex);
    dms_set_dev_info_handlers[type] = func;
    ka_task_mutex_unlock(&dev_info_handler_mutex);
    return 0;
}
KA_EXPORT_SYMBOL(dms_register_set_dev_info_handler);
#endif

int dms_unregister_set_dev_info_handler(DMS_DEV_INFO_TYPE type)
{
    if (type >= DMS_DEV_INFO_TYPE_MAX) {
        devdrv_drv_err("Unregister type is error. (type=%u)\n", type);
        return -EINVAL;
    }
    ka_task_mutex_lock(&dev_info_handler_mutex);
    dms_set_dev_info_handlers[type] = NULL;
    ka_task_mutex_unlock(&dev_info_handler_mutex);
    return 0;
}
KA_EXPORT_SYMBOL(dms_unregister_set_dev_info_handler);

STATIC int (*dms_get_svm_dev_info_handlers[DMS_DEV_INFO_TYPE_MAX])(u32 devid, void *buf, u32 *buf_size);
static KA_TASK_DEFINE_MUTEX(svm_get_dev_info_mutex);

int dms_register_get_svm_dev_info_handler(DMS_DEV_INFO_TYPE type, dms_get_dev_info_ops func)
{
    if (func == NULL) {
        devdrv_drv_err("Register function of getting device information is null.\n");
        return -EINVAL;
    }

    if (type >= DMS_DEV_INFO_TYPE_MAX) {
        devdrv_drv_err("Register type is error. (type=%u)\n", type);
        return -EINVAL;
    }
    ka_task_mutex_lock(&svm_get_dev_info_mutex);
    dms_get_svm_dev_info_handlers[type] = func;
    ka_task_mutex_unlock(&svm_get_dev_info_mutex);
    return 0;
}
KA_EXPORT_SYMBOL(dms_register_get_svm_dev_info_handler);

int dms_unregister_get_svm_dev_info_handler(DMS_DEV_INFO_TYPE type)
{
    if (type >= DMS_DEV_INFO_TYPE_MAX) {
        devdrv_drv_err("Unregister type is error. (type=%u)\n", type);
        return -EINVAL;
    }
    ka_task_mutex_lock(&svm_get_dev_info_mutex);
    dms_get_svm_dev_info_handlers[type] = NULL;
    ka_task_mutex_unlock(&svm_get_dev_info_mutex);
    return 0;
}
KA_EXPORT_SYMBOL(dms_unregister_get_svm_dev_info_handler);

int devdrv_manager_ioctl_set_vdevinfo(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
#ifdef CFG_FEATURE_DMS_SVM_DEV
    int ret;
    u32 vfid = 0;
    u32 phy_id = 0;
    struct devdrv_svm_vdev_info vinfo = {0};
    struct devmm_set_convert_len_para len_para = {0};

    if (devdrv_manager_check_permission()) {
        return -EOPNOTSUPP;
    }

    ret = copy_from_user_safe(&vinfo, (void *)((uintptr_t)arg), sizeof(struct devdrv_svm_vdev_info));
    if (ret != 0) {
        devdrv_drv_err("Copy from user failed. (ret=%d)\n", ret);
        return -EFAULT;
    }

    if (vinfo.type >= DMS_DEV_INFO_TYPE_MAX) {
        devdrv_drv_err("Invalid type. (dev_id=%u; type=%u)\n", vinfo.devid, vinfo.type);
        return -EINVAL;
    }

    if (vinfo.devid != DEVMNG_SET_ALL_DEV) {
        ret = devdrv_manager_container_logical_id_to_physical_id(vinfo.devid, &phy_id, &vfid);
        if (ret != 0) {
            devdrv_drv_err("logical_id_to_physical_id fail, devid(%u) ret(%d).\n", vinfo.devid, ret);
            return ret;
        }
        vinfo.devid = phy_id;
    }

    if (vinfo.buf_size != sizeof(len_para)) {
        devdrv_drv_err("Buffer size is error. (dev_id=%u; buf_size=%u;)\n", vinfo.devid, vinfo.buf_size);
        return -EINVAL;
    }

    ret = memcpy_s(&len_para, sizeof(len_para), vinfo.buf, vinfo.buf_size);
    if (ret != 0) {
        devdrv_drv_err("Memcpy failed. (ret=%d)\n", ret);
        return -EFAULT;
    }

    ka_task_mutex_lock(&dev_info_handler_mutex);
    if (dms_set_dev_info_handlers[vinfo.type] == NULL) {
        ka_task_mutex_unlock(&dev_info_handler_mutex);
        return -EOPNOTSUPP;
    }

    ret = dms_set_dev_info_handlers[vinfo.type](vinfo.devid, (void *)&len_para, sizeof(len_para));
    if (ret != 0) {
        ka_task_mutex_unlock(&dev_info_handler_mutex);
        devdrv_drv_err("Set vdevice information failed. (dev_id=%u; type=%u)\n", vinfo.devid, vinfo.type);
        return ret;
    }
    ka_task_mutex_unlock(&dev_info_handler_mutex);

    return 0;
#else
    return -EOPNOTSUPP;
#endif
}

int devdrv_manager_ioctl_get_svm_vdevinfo(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
#ifdef CFG_FEATURE_DMS_SVM_DEV
    int ret;
    u32 vfid = 0;
    u32 phy_id = 0;
    struct devdrv_svm_vdev_info vinfo = {0};

    if (devdrv_manager_check_permission()) {
        return -EOPNOTSUPP;
    }

    ret = copy_from_user_safe(&vinfo, (void *)((uintptr_t)arg), sizeof(struct devdrv_svm_vdev_info));
    if (ret != 0) {
        devdrv_drv_err("Copy from user failed. (ret=%d)\n", ret);
        return -EFAULT;
    }

    if (vinfo.type >= DMS_DEV_INFO_TYPE_MAX) {
        devdrv_drv_err("Invalid type. (dev_id=%u; type=%u)\n", vinfo.devid, vinfo.type);
        return -EINVAL;
    }

    ret = devdrv_manager_container_logical_id_to_physical_id(vinfo.devid, &phy_id, &vfid);
    if (ret != 0) {
        devdrv_drv_err("logical_id_to_physical_id fail, devid(%u) ret(%d).\n", vinfo.devid, ret);
        return ret;
    }
    vinfo.devid = phy_id;

    if (vinfo.buf_size != DEVDRV_SVM_VDEV_LEN) {
        devdrv_drv_err("Buffer size is error. (dev_id=%u; buf_size=%u;)\n", vinfo.devid, vinfo.buf_size);
        return -EINVAL;
    }

    ka_task_mutex_lock(&svm_get_dev_info_mutex);
    if (dms_get_svm_dev_info_handlers[vinfo.type] == NULL) {
        ka_task_mutex_unlock(&svm_get_dev_info_mutex);
        return -EOPNOTSUPP;
    }

    ret = dms_get_svm_dev_info_handlers[vinfo.type](vinfo.devid, (void *)vinfo.buf, &vinfo.buf_size);
    if (ret != 0) {
        ka_task_mutex_unlock(&svm_get_dev_info_mutex);
        devdrv_drv_err("Get vdevice information failed. (dev_id=%u; type=%u)\n", vinfo.devid, vinfo.type);
        return ret;
    }
    ka_task_mutex_unlock(&svm_get_dev_info_mutex);

    ret = copy_to_user_safe((void *)((uintptr_t)arg), &vinfo, sizeof(struct devdrv_svm_vdev_info));
    if (ret != 0) {
        devdrv_drv_err("copy to user failed. (devid=%d; ret=%d)\n", vinfo.devid, ret);
        return ret;
    }

    return 0;
#else
    return -EOPNOTSUPP;
#endif
}

#ifdef CFG_FEATURE_VASCEND
int devdrv_manager_ioctl_set_vdevmode(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    int ret;
    int mode;

    if (devdrv_manager_check_permission()) {
        return -EOPNOTSUPP;
    }

    ret = copy_from_user_safe(&mode, (void *)((uintptr_t)arg), sizeof(int));
    if (ret != 0) {
        devdrv_drv_err("Copy from user failed. (ret=%d)\n", ret);
        return -EFAULT;
    }

    ret = hw_dvt_set_mode(mode);
    if (ret != 0) {
        devdrv_drv_err("Set vdevice mode failed. (mode=%d)\n", mode);
        return ret;
    }

    return 0;
}

int devdrv_manager_ioctl_get_vdevmode(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    int ret;
    int mode;

    if (devdrv_manager_check_permission()) {
        return -EOPNOTSUPP;
    }

    ret = hw_dvt_get_mode(&mode);
    if (ret != 0) {
        devdrv_drv_err("hw_dvt_get_mode fail, ret=%d.\n", ret);
        return ret;
    }

    ret = copy_to_user_safe((void *)(uintptr_t)arg, &mode, sizeof(int));
    if (ret != 0) {
        devdrv_drv_err("copy to user failed, ret=%d.\n", ret);
        return -EFAULT;
    }

    return 0;
}
#endif
