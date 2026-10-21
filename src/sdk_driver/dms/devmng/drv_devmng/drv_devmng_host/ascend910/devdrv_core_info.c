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
#include "ka_ioctl_pub.h"
#include "ka_compiler_pub.h"
#include "ka_system_pub.h"
#include "ka_dfx_pub.h"
#include "devdrv_manager.h"
#include "pbl_mem_alloc_interface.h"
#include "devdrv_manager_common.h"
#include "devdrv_manager_msg.h"
#include "devdrv_common.h"
#include "comm_kernel_interface.h"
#include "dms_hotreset.h"
#include "devdrv_manager_container.h"
#include "hvdevmng_init.h"

#ifdef CFG_FEATURE_INUSE_NUM_DYNAMIC
int devdrv_get_core_inuse(u32 devid, u32 vfid, struct devdrv_hardware_inuse *inuse)
{
    struct devdrv_info *dev_info = NULL;
    u32 vf_aicore_num_inused = 0;
    u32 vf_aicpu_num_inused = 0;
    u32 aicpu_bitmap = 0;

    if (devid >= ASCEND_DEV_MAX_NUM || vfid > VDAVINCI_MAX_VFID_NUM) {
        devdrv_drv_err("Invalid para,(devid=%u,vfid=%u).\n", devid, vfid);
        return -EINVAL;
    }

    if ((inuse == NULL) || (dev_manager_info == NULL) || (dev_manager_info->dev_info[devid] == NULL)) {
        devdrv_drv_err("inuse(%pK) or dev_manager_info(%pK) or dev_manager_info->dev_info[devid(%u)] is NULL.\n", inuse,
                       dev_manager_info, devid);
        return -EINVAL;
    }

    dev_info = dev_manager_info->dev_info[devid];
#ifndef DEVDRV_MANAGER_HOST_UT_TEST
    if (dev_info == NULL) {
        devdrv_drv_err("dev_info is NULL. (dev_id=%u).\n", devid);
        return -EINVAL;
    }
#endif

    if (tsdrv_is_ts_work(devid, 0) == false) {
        devdrv_drv_err("device(%u) is not working.\n", devid);
        return -ENXIO;
    }

    (void)hvdevmng_get_aicore_num(devid, vfid, &vf_aicore_num_inused);
    (void)hvdevmng_get_aicpu_num(devid, vfid, &vf_aicpu_num_inused, &aicpu_bitmap);

    inuse->ai_core_num = ((vfid == 0) ? (dev_info->inuse.ai_core_num) : (vf_aicore_num_inused));
    inuse->ai_core_error_bitmap = ((vfid == 0) ? (dev_info->inuse.ai_core_error_bitmap) : (0x0));
    inuse->ai_cpu_num = ((vfid == 0) ? (dev_info->inuse.ai_cpu_num) : (vf_aicpu_num_inused));
    inuse->ai_cpu_error_bitmap = ((vfid == 0) ? (dev_info->inuse.ai_cpu_error_bitmap) : (0x0));

    return 0;
}
#else
int devdrv_get_core_inuse(u32 devid, u32 vfid, struct devdrv_hardware_inuse *inuse)
{
    struct devdrv_info *dev_info = NULL;
    (void)vfid;

    if ((inuse == NULL) || (devid >= ASCEND_DEV_MAX_NUM)) {
        devdrv_drv_err("Invalid parameter, (inuse_is_null=%d, devid=%u)\n", (inuse == NULL), devid);
        return -EINVAL;
    }

    dev_info = devdrv_manager_get_devdrv_info(devid);
    if (dev_info == NULL) {
        devdrv_drv_err("Device manager is not initialized. (devid=%u)\n", devid);
        return -EINVAL;
    }

    inuse->ai_core_num = dev_info->ai_core_num;
    inuse->ai_core_error_bitmap = 0;
    inuse->ai_cpu_num = dev_info->ai_cpu_core_num;
    inuse->ai_cpu_error_bitmap = 0;

    return 0;
}
#endif
KA_EXPORT_SYMBOL(devdrv_get_core_inuse);

int devdrv_manager_get_core_utilization(unsigned long arg)
{
    u32 phys_id = ASCEND_DEV_MAX_NUM + 1;
    u32 vfid = 0;
    u32 dev_id;
    int ret;
    struct devdrv_core_utilization util_info = {0};
    struct devdrv_info *dev_info = NULL;

    ret = copy_from_user_safe(&util_info, (void *)(uintptr_t)arg, sizeof(struct devdrv_core_utilization));
    if (ret != 0) {
        devdrv_drv_err("Copy from user failed. (ret=%d)\n", ret);
        return ret;
    }

    dev_id = util_info.dev_id;
    if (dev_id >= ASCEND_DEV_MAX_NUM) {
        devdrv_drv_err("Invalid device id. (dev_id=%u)\n", util_info.dev_id);
        return -ENODEV;
    }

    if (util_info.core_type >= DEV_DRV_TYPE_MAX) {
        devdrv_drv_err("Core_type is wrong. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    ret = devdrv_manager_container_logical_id_to_physical_id(dev_id, &phys_id, &vfid);
    if (ret != 0) {
        devdrv_drv_err("Transform virtual id failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return -EFAULT;
    }
    if (!devdrv_manager_is_pf_device(phys_id) || (vfid > 0)) {
        return -EOPNOTSUPP;
    }

    dev_info = devdrv_manager_get_devdrv_info(phys_id);
    if (dev_info == NULL) {
        devdrv_drv_err("The device is not initialized. (phys_id=%u)\n", phys_id);
        return -ENODEV;
    }

    ret = dms_hotreset_task_cnt_increase(phys_id);
    if (ret != 0) {
        devdrv_drv_err("Hotreset task count increase failed. (phys_id=%u; ret=%d)\n", phys_id, ret);
        return ret;
    }

    ka_base_atomic_inc(&dev_info->occupy_ref);
    if (dev_info->status == DEVINFO_STATUS_REMOVED) {
        ka_base_atomic_dec(&dev_info->occupy_ref);
        devdrv_drv_warn("The device has been reset. (dev_id=%u)\n", dev_info->dev_id);
        dms_hotreset_task_cnt_decrease(phys_id);
        return -EINVAL;
    }

    util_info.dev_id = phys_id;
    ret = devdrv_manager_h2d_sync_get_core_utilization(&util_info);
    if (ret != 0) {
        ka_base_atomic_dec(&dev_info->occupy_ref);
        devdrv_drv_err("The core utilization get failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        dms_hotreset_task_cnt_decrease(phys_id);
        return ret;
    }

    ka_base_atomic_dec(&dev_info->occupy_ref);

    ret = copy_to_user_safe((void *)(uintptr_t)arg, &util_info, sizeof(struct devdrv_core_utilization));
    if (ret != 0) {
        devdrv_drv_err("Copy to user failed. (phys_id=%u; ret=%d)\n", phys_id, ret);
        dms_hotreset_task_cnt_decrease(phys_id);
        return -EFAULT;
    }

    dms_hotreset_task_cnt_decrease(phys_id);

    return ret;
}

int devdrv_manager_get_core(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    struct devdrv_hardware_inuse inuse;
    struct devdrv_hardware_spec spec;
    u32 phys_id = ASCEND_DEV_MAX_NUM + 1;
    u32 vfid = 0;
    int ret;

    switch (cmd) {
        case DEVDRV_MANAGER_GET_CORE_SPEC:
            ret = copy_from_user_safe(&spec, (void *)((uintptr_t)arg), sizeof(struct devdrv_hardware_spec));
            if (ret) {
                devdrv_drv_err("copy_from_user_safe failed, ret(%d).\n", ret);
                return ret;
            }
            if (devdrv_manager_container_logical_id_to_physical_id(spec.devid, &phys_id, &vfid) != 0) {
                devdrv_drv_err("can't transform virt id %u \n", spec.devid);
                return -EFAULT;
            }

            ret = devdrv_get_core_spec(phys_id, vfid, &spec);
            if (ret) {
                devdrv_drv_err("devdrv_get_core_spec failed, ret(%d), dev_id(%u).\n", ret, phys_id);
                return ret;
            }
            ret = copy_to_user_safe((void *)((uintptr_t)arg), &spec, sizeof(struct devdrv_hardware_spec));
            if (ret) {
                devdrv_drv_err("copy_to_user_safe failed, ret(%d). dev_id(%u)\n", ret, phys_id);
                return ret;
            }
            break;
        case DEVDRV_MANAGER_GET_CORE_INUSE:
            ret = copy_from_user_safe(&inuse, (void *)((uintptr_t)arg), sizeof(struct devdrv_hardware_inuse));
            if (ret) {
                devdrv_drv_err("copy_from_user_safe failed, ret(%d).\n", ret);
                return ret;
            }

            if (devdrv_manager_container_logical_id_to_physical_id(inuse.devid, &phys_id, &vfid) != 0) {
                devdrv_drv_err("can't transform virt id %u \n", inuse.devid);
                return -EFAULT;
            }

            ret = devdrv_get_core_inuse(phys_id, vfid, &inuse);
            if (ret) {
                devdrv_drv_err("devdrv_get_core_inuse failed, ret(%d), dev_id(%u).\n", ret, phys_id);
                return ret;
            }
            ret = copy_to_user_safe((void *)(uintptr_t)arg, &inuse, sizeof(struct devdrv_hardware_inuse));
            if (ret) {
                devdrv_drv_err("copy_to_user_safe failed, ret(%d). dev_id(%u)\n", ret, phys_id);
                return ret;
            }
            break;
        default:
            devdrv_drv_err("invalid cmd. (cmd=%u)\n", cmd);
            return -EINVAL;
    }
    return 0;
}

int devdrv_get_core_spec(u32 devid, u32 vfid, struct devdrv_hardware_spec *spec)
{
    struct devdrv_info *dev_info = NULL;
    u32 aicpu_bitmap = 0;

    if ((devid >= ASCEND_DEV_MAX_NUM) || (vfid > VDAVINCI_MAX_VFID_NUM)) {
        devdrv_drv_err("invalid dev_id(%u) vfid(%u).\n", devid, vfid);
        return -EINVAL;
    }

    if ((spec == NULL) || (dev_manager_info == NULL) || (dev_manager_info->dev_info[devid] == NULL)) {
        devdrv_drv_err("spec(%pK) or dev_manager_info(%pK) or dev_manager_info->dev_info[devid(%u)] is NULL.\n", spec,
                       dev_manager_info, devid);
        return -EINVAL;
    }

    dev_info = dev_manager_info->dev_info[devid];
#ifndef DEVDRV_MANAGER_HOST_UT_TEST
    if (dev_info == NULL) {
        devdrv_drv_err("dev_info is NULL. (dev_id=%u).\n", devid);
        return -EINVAL;
    }
#endif

    (void)hvdevmng_get_aicore_num(dev_info->dev_id, vfid, &spec->ai_core_num);
    (void)hvdevmng_get_aicpu_num(dev_info->dev_id, vfid, &spec->ai_cpu_num, &aicpu_bitmap);
    spec->first_ai_core_id = dev_info->ai_core_id;
    spec->first_ai_cpu_id = dev_info->ai_cpu_core_id;
#ifndef CFG_FEATURE_REFACTOR
    spec->ai_core_num_level = dev_info->pdata->ai_core_num_level;
    spec->ai_core_freq_level = dev_info->pdata->ai_core_freq_level;
#else
    spec->ai_core_num_level = 0;
    spec->ai_core_freq_level = 0;
#endif
    return 0;
}
KA_EXPORT_SYMBOL(devdrv_get_core_spec);

#ifdef CFG_FEATURE_DEVMNG_IOCTL
STATIC void devdrv_manager_set_computing_value(struct devdrv_manager_hccl_devinfo *hccl_devinfo,
                                               struct devdrv_info *dev_info, bool valid)
{
    int i;

    if (valid) {
        for (i = 0; i < DEVDRV_MAX_COMPUTING_POWER_TYPE; i++) {
            hccl_devinfo->computing_power[i] = dev_info->computing_power[i];
        }
    } else {
        for (i = 0; i < DEVDRV_MAX_COMPUTING_POWER_TYPE; i++) {
            hccl_devinfo->computing_power[i] = DEVDRV_COMPUTING_VALUE_ERROR;
        }
    }
}

int devdrv_manager_get_h2d_devinfo(unsigned long arg)
{
    struct devdrv_manager_hccl_devinfo *hccl_devinfo = NULL;
    struct devdrv_platform_data *pdata = NULL;
    struct devdrv_info *dev_info = NULL;
    u32 phys_id = ASCEND_DEV_MAX_NUM + 1;
    u32 vfid = 0;
    u32 dev_id;
    int ret;

    hccl_devinfo = (struct devdrv_manager_hccl_devinfo *)dbl_kzalloc(sizeof(struct devdrv_manager_hccl_devinfo),
                                                                     KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (hccl_devinfo == NULL) {
        devdrv_drv_err("Alloc memory for hccl device info failed.\n");
        return -ENOMEM;
    }

    if (copy_from_user_safe(hccl_devinfo, (void *)(uintptr_t)arg, sizeof(struct devdrv_manager_hccl_devinfo))) {
        devdrv_drv_err("copy from user failed.\n");
        ret = -EINVAL;
        goto FREE_DEV_INFO_EXIT;
    }

    dev_id = hccl_devinfo->dev_id;
    if (dev_id >= ASCEND_DEV_MAX_NUM) {
        devdrv_drv_err("invalid dev_id(%u)\n", hccl_devinfo->dev_id);
        ret = -ENODEV;
        goto FREE_DEV_INFO_EXIT;
    }
    if (devdrv_manager_container_logical_id_to_physical_id(dev_id, &phys_id, &vfid) != 0) {
        devdrv_drv_err("can't transform virt id %u \n", dev_id);
        ret = -EFAULT;
        goto FREE_DEV_INFO_EXIT;
    }

    dev_info = devdrv_manager_get_devdrv_info(phys_id);
    if (dev_info == NULL) {
        devdrv_drv_err("device(%u) is not initialized\n", phys_id);
        ret = -ENODEV;
        goto FREE_DEV_INFO_EXIT;
    }

    ret = dms_hotreset_task_cnt_increase(phys_id);
    if (ret != 0) {
        devdrv_drv_err("Hotreset task cnt increase failed. (dev_id=%u; ret=%d)\n", phys_id, ret);
        goto FREE_DEV_INFO_EXIT;
    }

    ka_base_atomic_inc(&dev_info->occupy_ref);
    if (dev_info->status == DEVINFO_STATUS_REMOVED) {
        ka_base_atomic_dec(&dev_info->occupy_ref);
        devdrv_drv_warn("dev %d has been reset\n", dev_info->dev_id);
        dms_hotreset_task_cnt_decrease(phys_id);
        ret = -EINVAL;
        goto FREE_DEV_INFO_EXIT;
    }

    pdata = dev_info->pdata;
    hccl_devinfo->ai_core_num = dev_info->ai_core_num;
    hccl_devinfo->aicore_freq = dev_info->aicore_freq;
    hccl_devinfo->ai_cpu_core_num = dev_info->ai_cpu_core_num;
    hccl_devinfo->ctrl_cpu_core_num = dev_info->ctrl_cpu_core_num;
    hccl_devinfo->ctrl_cpu_occupy_bitmap = dev_info->ctrl_cpu_occupy_bitmap;

    /* 1:little endian 0:big endian */
    hccl_devinfo->ctrl_cpu_endian_little = dev_info->ctrl_cpu_endian_little;
    hccl_devinfo->ctrl_cpu_id = dev_info->ctrl_cpu_id;
    hccl_devinfo->ctrl_cpu_ip = dev_info->ctrl_cpu_ip;
    hccl_devinfo->ts_cpu_core_num = pdata->ts_pdata[0].ts_cpu_core_num;
    hccl_devinfo->env_type = dev_info->env_type;
    hccl_devinfo->ai_core_id = dev_info->ai_core_id;
    hccl_devinfo->ai_cpu_core_id = dev_info->ai_cpu_core_id;
    hccl_devinfo->ai_cpu_bitmap = dev_info->aicpu_occupy_bitmap;
    hccl_devinfo->hardware_version = dev_info->hardware_version;
    hccl_devinfo->ts_num = devdrv_manager_get_ts_num(dev_info);

    if (devdrv_manager_h2d_sync_get_devinfo(dev_info)) {
        devdrv_drv_err("device info get failed. dev_id(%u)\n", phys_id);
        devdrv_manager_set_computing_value(hccl_devinfo, dev_info, false);
    } else {
        devdrv_manager_set_computing_value(hccl_devinfo, dev_info, true);
    }
    hccl_devinfo->cpu_system_count = dev_info->cpu_system_count;
    hccl_devinfo->monotonic_raw_time_ns = dev_info->monotonic_raw_time_ns;
    hccl_devinfo->ffts_type = dev_info->ffts_type;
    hccl_devinfo->vector_core_num = dev_info->vector_core_num;
    hccl_devinfo->vector_core_freq = dev_info->vector_core_freq;
    devdrv_drv_debug("ctrl_cpu_ip(0x%x), ts_cpu_core_num(%d), dev_id(%u)\n", dev_info->ctrl_cpu_ip,
                     hccl_devinfo->ts_cpu_core_num, phys_id);

    ka_base_atomic_dec(&dev_info->occupy_ref);

    if (copy_to_user_safe((void *)(uintptr_t)arg, hccl_devinfo, sizeof(struct devdrv_manager_hccl_devinfo))) {
        devdrv_drv_err("copy to user failed. dev_id(%u)\n", phys_id);
        dms_hotreset_task_cnt_decrease(phys_id);
        ret = -EFAULT;
        goto FREE_DEV_INFO_EXIT;
    }

    dms_hotreset_task_cnt_decrease(phys_id);

FREE_DEV_INFO_EXIT:
    dbl_kfree(hccl_devinfo);
    hccl_devinfo = NULL;
    return ret;
}
#endif

int devdrv_manager_get_tsdrv_dev_com_info(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    struct tsdrv_dev_com_info dev_com_info;

    dev_com_info.mach_type = PHY_MACHINE_TYPE;
    dev_com_info.ts_num = 1;

    if (copy_to_user_safe((void *)(uintptr_t)arg, &dev_com_info, sizeof(struct tsdrv_dev_com_info))) {
        devdrv_drv_err("copy to user failed.\n");
        return -EFAULT;
    }

    return 0;
}

int devdrv_manager_host_get_group_info(struct devdrv_manager_msg_info *dev_manager_msg_info,
                                       struct get_ts_group_para *group_para, struct devdrv_info *info)
{
    int ret;
    int out_len = 0;

    dev_manager_msg_info->header.msg_id = DEVDRV_MANAGER_CHAN_H2D_GET_TS_GROUP_INFO;
    dev_manager_msg_info->header.valid = DEVDRV_MANAGER_MSG_VALID;
    /* give a random value for checking result later */
    dev_manager_msg_info->header.result = (u16)DEVDRV_MANAGER_MSG_INVALID_RESULT;
    /* inform corresponding devid to device side */
    dev_manager_msg_info->header.dev_id = info->dev_id;

    ret = memcpy_s(dev_manager_msg_info->payload, DEVDRV_MANAGER_INFO_PAYLOAD_LEN, group_para,
                   sizeof(struct get_ts_group_para));
    if (ret != 0) {
        devdrv_drv_err("memcpy failed ret = %d\n", ret);
        return -EFAULT;
    }

    ret = devdrv_manager_send_msg(info, dev_manager_msg_info, &out_len);
    if (ret != 0) {
        devdrv_drv_err("send msg to device fail ret = %d\n", ret);
        return ret;
    }
    if (out_len != (DEVDRV_TS_GROUP_NUM * sizeof(struct ts_group_info) + sizeof(struct devdrv_manager_msg_head))) {
        devdrv_drv_err("received response len %d is not equal to %ld\n", out_len,
                       DEVDRV_TS_GROUP_NUM * sizeof(struct ts_group_info));
        return -EINVAL;
    }
    if (dev_manager_msg_info->header.result != 0) {
        devdrv_drv_err("get response from host error ret = %d\n", dev_manager_msg_info->header.result);
        return dev_manager_msg_info->header.result;
    }
    return 0;
}

int devdrv_manager_get_group_para(struct devdrv_ioctl_info *ioctl_buf, struct get_ts_group_para *group_para,
                                  unsigned long arg)
{
    int ret;

    ret = copy_from_user_safe((void *)ioctl_buf, (void *)((uintptr_t)arg), sizeof(struct devdrv_ioctl_info));
    if (ret != 0) {
        devdrv_drv_err("copy from user failed %d\n", ret);
        return ret;
    }
    if ((ioctl_buf->input_len != sizeof(struct get_ts_group_para)) ||
        (ioctl_buf->input_len > DEVDRV_MANAGER_INFO_PAYLOAD_LEN)) {
        devdrv_drv_err("input_len %d is invalid: should equal %ld and be less than %ld\n", ioctl_buf->input_len,
                       sizeof(struct get_ts_group_para), DEVDRV_MANAGER_INFO_PAYLOAD_LEN);
        return -EINVAL;
    }
    ret = copy_from_user_safe((void *)group_para, (void *)(ioctl_buf->input_buf), ioctl_buf->input_len);
    if (ret != 0) {
        devdrv_drv_err("copy from user failed %d\n", ret);
        return ret;
    }
    return 0;
}

int devdrv_manager_get_ts_group_info(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    struct devdrv_ioctl_info ioctl_buf = {0, NULL, 0, NULL, 0, {0}};
    struct get_ts_group_para group_para = {0};
    int ret;
    struct devdrv_manager_msg_info dev_manager_msg_info = {{0}, {0}};
    struct devdrv_info *info = NULL;
    struct devdrv_manager_info *d_info = NULL;
    unsigned int phy_id = 0;
    unsigned int vfid = 0;

    ret = devdrv_manager_get_group_para(&ioctl_buf, &group_para, arg);
    if (ret != 0) {
        devdrv_drv_err("get group para fail ret = %d\n", ret);
        return ret;
    }
    ret = devdrv_manager_container_logical_id_to_physical_id(group_para.device_id, &phy_id, &vfid);
    if (ret != 0) {
        devdrv_drv_err("can't get phys device id. virt id is %u, ret = %d\n", group_para.device_id, ret);
        return -EINVAL;
    }

    d_info = devdrv_get_manager_info();
    if (d_info == NULL) {
        devdrv_drv_err("info is NULL: cannot get dev_info.\n");
        return -EINVAL;
    }
    if (phy_id >= ASCEND_DEV_MAX_NUM) {
        devdrv_drv_err("group_para phy device_id %d must be less than %d\n", phy_id, ASCEND_DEV_MAX_NUM);
        return -EINVAL;
    }
    info = d_info->dev_info[phy_id];
    if (info == NULL) {
        devdrv_drv_err("info is NULL: invalid virtual device id = %d, phy dev_id is %u\n", group_para.device_id,
                       phy_id);
        return -EINVAL;
    }
    ret = devdrv_manager_host_get_group_info(&dev_manager_msg_info, &group_para, info);
    if (ret != 0) {
        devdrv_drv_err("host get ts group info fail %u\n", ret);
        return ret;
    }
    if (ioctl_buf.out_len > DEVDRV_MANAGER_INFO_PAYLOAD_LEN) {
        devdrv_drv_err("out len %d is invalid: should be less than %ld\n", ioctl_buf.out_len,
                       DEVDRV_MANAGER_INFO_PAYLOAD_LEN);
        return -EINVAL;
    }
    if (copy_to_user_safe((void *)(ioctl_buf.out_buf), (void *)dev_manager_msg_info.payload, ioctl_buf.out_len)) {
        devdrv_drv_err("copy to user failed.\n");
        return -EFAULT;
    }
    return 0;
}

#ifndef CFG_FEATURE_REFACTOR
u32 devdrv_manager_get_ts_num(struct devdrv_info *dev_info)
{
    if (dev_info == NULL) {
        devdrv_drv_err("invalid input handler.\n");
        return (u32)-1;
    }
    if (dev_info->pdata == NULL) {
        devdrv_drv_err("invalid input handler.\n");
        return (u32)-1;
    }

    if (dev_info->pdata->ts_num > DEVDRV_MAX_TS_NUM) {
        devdrv_drv_err("ts_num(%u).\n", dev_info->pdata->ts_num);
        return (u32)-1;
    }
    dev_info->pdata->ts_num = 1;

    return dev_info->pdata->ts_num;
}
KA_EXPORT_SYMBOL(devdrv_manager_get_ts_num);
#endif

#ifndef CFG_FEATURE_REFACTOR
u32 devdrv_get_ts_num(void)
{
    u32 tsid;

#ifndef CFG_SOC_PLATFORM_MINIV2
    tsid = DEVDRV_MAX_TS_NUM;
#else
    tsid = 1; // tsid should be transferred from dev side later
#endif /* CFG_SOC_PLATFORM_MINIV2 */
    return tsid;
}
#endif
