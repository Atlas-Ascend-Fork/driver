/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
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

#include "devdrv_manager.h"
#include "devdrv_manager_common.h"
#include "devdrv_manager_msg.h"
#include "devdrv_black_box.h"
#include "devdrv_common.h"
#include "devdrv_pcie.h"
#include "comm_kernel_interface.h"
#include "dms_urd_forward.h"
#include "dms_event_distribute.h"
#include "hvdevmng_init.h"
#include "tsdrv_status.h"
#include "dev_mnt_vdevice.h"
#include "vmng_kernel_interface.h"
#include "pbl_mem_alloc_interface.h"
#include "davinci_interface.h"
#include "devdrv_user_common.h"
#include "pbl/pbl_davinci_api.h"
#include "dms/dms_devdrv_manager_comm.h"
#include "svm_kernel_interface.h"
#include "dms_hotreset.h"
#include "ascend_kernel_hal.h"
#include "pbl/pbl_soc_res_attr.h"
#include "pbl/pbl_chip_config.h"
#include "adapter_api.h"
#include "ka_ioctl_pub.h"
#include "ka_compiler_pub.h"
#include "ka_system_pub.h"
#include "ka_dfx_pub.h"
#include "devdrv_manager_container.h"
#include "devdrv_manager_pid_map.h"
#include "devdrv_device_online.h"
#include "devdrv_black_box_dump.h"
#include "devdrv_core_info.h"
#include "devdrv_resource_info.h"
#include "devdrv_device_status.h"
#include "devdrv_vdev_info.h"
#include "devdrv_pci_info.h"
#include "devdrv_ipc_notify.h"

#ifdef CFG_FEATURE_DEVICE_SHARE
#include "devdrv_manager_dev_share.h"
#endif

#ifdef CFG_FEATURE_CHIP_DIE
#include "devdrv_chip_dev_map.h"
#endif

#ifdef CFG_FEATURE_TIMESYNC
#include "dms_time.h"
#endif

STATIC int devdrv_manager_ioctl_get_console_loglevel(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    int console_loglevle_value;
    int ret;

    console_loglevle_value = log_level_get();
    ret = copy_to_user_safe((void *)((uintptr_t)arg), &console_loglevle_value, sizeof(int));

    return ret;
}

STATIC int devdrv_manager_ioctl_get_plat_info(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    u32 plat_info;

    plat_info = devdrv_get_manager_info()->plat_info;
    return copy_to_user_safe((void *)((uintptr_t)arg), &plat_info, sizeof(u32));
}

int devdrv_manager_trans_and_check_id(u32 logical_dev_id, u32 *physical_dev_id, u32 *vfid)
{
    int ret;

    if (logical_dev_id >= ASCEND_DEV_MAX_NUM) {
        devdrv_drv_err("Wrong device id. (dev_id=%u)\n", logical_dev_id);
        return -EINVAL;
    }

    ret = devdrv_manager_container_logical_id_to_physical_id(logical_dev_id, physical_dev_id, vfid);
    if (ret != 0) {
        devdrv_drv_err("Can not transfer device id. (logical_dev_id=%u; ret = %d)\n", logical_dev_id, ret);
        return ret;
    }
    if (*physical_dev_id >= ASCEND_DEV_MAX_NUM) {
        devdrv_drv_err("Wrong device id. (physical_dev_id=%u)\n", *physical_dev_id);
        return -EINVAL;
    }
    if ((devdrv_get_manager_info() == NULL) || devdrv_get_devdrv_info_array(*physical_dev_id) == NULL) {
        devdrv_drv_err("Device manager is not initialized. (dev_id=%u; device_manager_info=%d)\n", *physical_dev_id,
                       (devdrv_get_manager_info() == NULL));
        return -EINVAL;
    }
    if (devdrv_get_devdrv_info_array(*physical_dev_id)->status == 1) {
        devdrv_drv_warn("The device status is DEVICE_STATUS_REMOVED. (dev_id=%u; status=%d)\n", *physical_dev_id, 1);
        return -EBUSY;
    }
    return 0;
}

STATIC int devdrv_manager_get_container_devids(unsigned long arg)
{
    struct devdrv_manager_devids *hccl_devinfo = NULL;
    int ret;

    if ((ka_task_get_current_nsproxy() == NULL) ||
        (!devdrv_manager_container_is_host_system(ka_task_get_current_mnt_ns()))) {
        devdrv_drv_err("Do not have permission in container or virtual machine.\n");
        return -EPERM;
    }

    hccl_devinfo = (struct devdrv_manager_devids *)dbl_kzalloc(sizeof(struct devdrv_manager_devids),
                                                               KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (hccl_devinfo == NULL) {
        devdrv_drv_err("Alloc memory for hccl device info failed.\n");
        return -ENOMEM;
    }

    ret = devdrv_get_devids(hccl_devinfo->devids, ASCEND_DEV_MAX_NUM);
    if (ret) {
        devdrv_drv_err("get container devlist failed, ret(%d)\n", ret);
        dbl_kfree(hccl_devinfo);
        hccl_devinfo = NULL;
        return -ENODEV;
    }

    if (copy_to_user_safe((void *)(uintptr_t)arg, hccl_devinfo, sizeof(struct devdrv_manager_devids))) {
        devdrv_drv_err("copy from user failed, ret(%d).\n", ret);
        dbl_kfree(hccl_devinfo);
        hccl_devinfo = NULL;
        return -EINVAL;
    }

    dbl_kfree(hccl_devinfo);
    hccl_devinfo = NULL;
    return 0;
}

STATIC int devdrv_manager_get_devinfo(unsigned long arg)
{
    struct devdrv_manager_hccl_devinfo *hccl_devinfo = NULL;
#ifndef CFG_FEATURE_REFACTOR
    struct devdrv_platform_data *pdata = NULL;
#endif
    struct devdrv_info *dev_info = NULL;
    u32 phys_id = ASCEND_DEV_MAX_NUM + 1;
    u32 aicpu_occupy_bitmap = 0;
    u32 vfid = 0;
    u32 dev_id;
    int ret, connect_type;

    hccl_devinfo = (struct devdrv_manager_hccl_devinfo *)dbl_kzalloc(sizeof(struct devdrv_manager_hccl_devinfo),
                                                                     KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (hccl_devinfo == NULL) {
        devdrv_drv_err("Alloc memory for hccl device info failed.\n");
        return -ENOMEM;
    }

    if (copy_from_user_safe(hccl_devinfo, (void *)(uintptr_t)arg, sizeof(struct devdrv_manager_hccl_devinfo))) {
        devdrv_drv_err("Copy from user failed.\n");
        ret = -EINVAL;
        goto FREE_DEV_INFO_EXIT;
    }

    dev_id = hccl_devinfo->dev_id;
    if (dev_id >= ASCEND_DEV_MAX_NUM) {
        devdrv_drv_err("invalid dev_id(%u)\n", hccl_devinfo->dev_id);
        ret = -ENODEV;
        goto FREE_DEV_INFO_EXIT;
    }
    ret = devdrv_manager_trans_and_check_id(dev_id, &phys_id, &vfid);
    if (ret != 0) {
        devdrv_drv_err_extend(ret, -EBUSY, "Can not convert device id. (devid=%u; ret=%d)\n", dev_id, ret);
        goto FREE_DEV_INFO_EXIT;
    }

    if ((phys_id >= ASCEND_DEV_MAX_NUM) || (vfid > VDAVINCI_MAX_VFID_NUM)) {
        devdrv_drv_err("get invalid output phys_id(%u) vfid(%u)\n", phys_id, vfid);
        ret = -EINVAL;
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

    ret = devdrv_try_get_dev_info_occupy(dev_info);
    if (ret != 0) {
        devdrv_drv_warn("dev %d has been reset\n", dev_info->dev_id);
        ret = -EINVAL;
        dms_hotreset_task_cnt_decrease(phys_id);
        goto FREE_DEV_INFO_EXIT;
    }

    (void)hvdevmng_get_aicore_num(dev_info->dev_id, vfid, &hccl_devinfo->ai_core_num);
    (void)hvdevmng_get_aicpu_num(dev_info->dev_id, vfid, &hccl_devinfo->ai_cpu_core_num, &aicpu_occupy_bitmap);

    hccl_devinfo->aicore_freq = dev_info->aicore_freq;
    hccl_devinfo->ctrl_cpu_core_num = dev_info->ctrl_cpu_core_num;
    hccl_devinfo->ctrl_cpu_occupy_bitmap = dev_info->ctrl_cpu_occupy_bitmap;
    hccl_devinfo->ctrl_cpu_id = dev_info->ctrl_cpu_id;
    hccl_devinfo->ctrl_cpu_ip = dev_info->ctrl_cpu_ip;

    /* 1:little endian 0:big endian */
    hccl_devinfo->ctrl_cpu_endian_little = dev_info->ctrl_cpu_endian_little;
    hccl_devinfo->env_type = dev_info->env_type;
    hccl_devinfo->ai_core_id = dev_info->ai_core_id;
    hccl_devinfo->ai_cpu_core_id = dev_info->ai_cpu_core_id;
    hccl_devinfo->ai_cpu_bitmap = (vfid == 0) ? dev_info->aicpu_occupy_bitmap : aicpu_occupy_bitmap;
    hccl_devinfo->aicore_bitmap[0] = dev_info->aicore_bitmap;
    hccl_devinfo->vector_core_bitmap[0] = dev_info->vector_core_bitmap;
    hccl_devinfo->vector_core_bitmap[1] = dev_info->vector_core_bitmap_h;
    hccl_devinfo->hardware_version = dev_info->hardware_version;
#ifndef CFG_FEATURE_REFACTOR
    pdata = dev_info->pdata;
    if (pdata == NULL) {
        devdrv_drv_err("dev_info pdata is NULL\n");
        ret = -EINVAL;
        goto HOT_RESET_CNT_EXIT;
    }
    hccl_devinfo->ts_cpu_core_num = pdata->ts_pdata[0].ts_cpu_core_num;
    hccl_devinfo->ts_num = devdrv_manager_get_ts_num(dev_info);
#else
    hccl_devinfo->ts_num = dev_info->ts_num;
#endif
    hccl_devinfo->ffts_type = dev_info->ffts_type;
    hccl_devinfo->chip_id = dev_info->chip_id;
    hccl_devinfo->die_id = dev_info->die_id;
#ifdef CFG_SOC_PLATFORM_MINIV2
    hccl_devinfo->vector_core_num = (vfid == 0) ? dev_info->vector_core_num : hccl_devinfo->ai_cpu_core_num;
#else
    hccl_devinfo->vector_core_num = dev_info->vector_core_num;
#endif
    hccl_devinfo->vector_core_freq = dev_info->vector_core_freq;
    hccl_devinfo->l2_num = dev_info->l2_num;
    hccl_devinfo->hbm_num = dev_info->hbm_num;
    devdrv_drv_debug("ctrl_cpu_ip(0x%x), ts_cpu_core_num(%d), dev_id(%u)\n", dev_info->ctrl_cpu_ip,
                     hccl_devinfo->ts_cpu_core_num, phys_id);
    hccl_devinfo->addr_mode = dev_info->addr_mode;
    hccl_devinfo->mainboard_id = dev_info->mainboard_id;
    hccl_devinfo->product_type = dev_info->product_type;
#if ((defined CFG_FEATURE_PCIE_HOST_DEVICE_COMM) || (defined CFG_FEATURE_UB_HOST_DEVICE_COMM))
    connect_type = devdrv_get_connect_protocol(phys_id);
    if (connect_type < 0) {
        devdrv_drv_err("Get host device connect type failed. (dev_id=%u; type=%d)\n", phys_id, connect_type);
        ret = -EINVAL;
        goto HOT_RESET_CNT_EXIT;
    }
#else
    connect_type = CONNECT_PROTOCOL_UNKNOWN;
#endif
    hccl_devinfo->host_device_connect_type = connect_type;
#ifdef CFG_FEATURE_PG
#ifndef CFG_FEATURE_REFACTOR
    ret = strncpy_s(hccl_devinfo->soc_version, SOC_VERSION_LENGTH, dev_info->pg_info.spePgInfo.socVersion,
                    SOC_VERSION_LEN - 1);
#else
    ret = strncpy_s(hccl_devinfo->soc_version, SOC_VERSION_LENGTH, dev_info->soc_version, SOC_VERSION_LEN - 1);
#endif
    if (ret != 0) {
        devdrv_drv_err("Strncpy_s soc_version failed. (ret=%d)\n", ret);
        ret = -EINVAL;
        goto HOT_RESET_CNT_EXIT;
    }
#endif

    if (copy_to_user_safe((void *)(uintptr_t)arg, hccl_devinfo, sizeof(struct devdrv_manager_hccl_devinfo))) {
        devdrv_drv_err("copy to user failed. dev_id(%u)\n", phys_id);
        ret = -EFAULT;
    }

HOT_RESET_CNT_EXIT:
    ka_base_atomic_dec(&dev_info->occupy_ref);
    dms_hotreset_task_cnt_decrease(phys_id);
FREE_DEV_INFO_EXIT:
    dbl_kfree(hccl_devinfo);
    hccl_devinfo = NULL;
    return ret;
}

STATIC int devdrv_manager_devinfo_ioctl(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    int ret;

    switch (cmd) {
        case DEVDRV_MANAGER_GET_CORE_UTILIZATION:
            ret = devdrv_manager_get_core_utilization(arg);
            break;
        case DEVDRV_MANAGER_GET_CONTAINER_DEVIDS:
            ret = devdrv_manager_get_container_devids(arg);
            break;
        case DEVDRV_MANAGER_GET_DEVINFO:
            ret = devdrv_manager_get_devinfo(arg);
            break;
#ifdef CFG_FEATURE_DEVMNG_IOCTL
        case DEVDRV_MANAGER_GET_H2D_DEVINFO:
            ret = devdrv_manager_get_h2d_devinfo(arg);
            break;
#endif
        case DEVDRV_MANAGER_GET_PCIE_ID_INFO:
            ret = devdrv_get_pcie_id(arg);
            break;
        default:
            ret = -EINVAL;
            break;
    }

    return ret;
}

static int devdrv_manager_all_is_pf_device(void)
{
    int i, ret;
    unsigned int phy_id = 0;
    unsigned int vfid = 0;
    unsigned int dev_num = 0;

    dev_num = devdrv_manager_get_devnum();
    if ((dev_num > ASCEND_DEV_MAX_NUM) || (dev_num == 0)) {
        devdrv_drv_err("Can't get device number (dev_num=%u)\n", dev_num);
        return -EINVAL;
    }

    for (i = 0; i < dev_num; i++) {
        ret = devdrv_manager_container_logical_id_to_physical_id(i, &phy_id, &vfid);
        if (ret != 0) {
            devdrv_drv_err("Transfer logical id to physical id failed. (ret=%d)\n", ret);
            return ret;
        }

        if (!devdrv_manager_is_pf_device(phy_id) || (vfid > 0)) {
            return -EOPNOTSUPP;
        }
    }

    return 0;
}

static int devdrv_bind_master_para_check(struct devdrv_ioctl_para_bind_host_pid *para_info)
{
    if ((para_info->cp_type == DEVDRV_PROCESS_DEV_ONLY) || (para_info->vfid != 0) ||
        ((para_info->chip_id >= PID_MAP_DEVNUM) && (para_info->chip_id != HAL_BIND_ALL_DEVICE)) ||
        ((para_info->chip_id == HAL_BIND_ALL_DEVICE) && (para_info->cp_type != DEVDRV_PROCESS_USER))) {
        return -EOPNOTSUPP;
    }

    para_info->sign[DEVDRV_SIGN_LEN - 1] = '\0';
    if ((para_info->len != PROCESS_SIGN_LENGTH) || (para_info->mode >= AICPUFW_MAX_PLAT) || (para_info->cp_type < 0) ||
        (para_info->cp_type >= DEVDRV_PROCESS_CPTYPE_MAX)) {
        devdrv_drv_err("Invalid parameter. (len=%u; mode=%d; cp_type=%d; dev_id=%u; vf_id=%u; master_pid=%d).\n",
                       para_info->len, para_info->mode, para_info->cp_type, para_info->chip_id, para_info->vfid,
                       para_info->host_pid);
        return -EINVAL;
    }

    return 0;
}

STATIC int devdrv_fop_bind_host_pid(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    struct devdrv_ioctl_para_bind_host_pid para_info;
    struct bind_cost_statistics cost_stat;
    int node_id = ka_system_numa_node_id();
    unsigned int phy_id = 0;
    unsigned int vfid = 0;
    int ret;

    (void)memset_s(&cost_stat, sizeof(cost_stat), 0, sizeof(cost_stat));
    cost_stat.bind_start = ka_system_ktime_get();
    if (copy_from_user_safe(&para_info, (void *)(uintptr_t)arg, sizeof(struct devdrv_ioctl_para_bind_host_pid))) {
        devdrv_drv_err("ka_base_copy_from_user error. (dev_id=%u)\n", node_id);
        return -EINVAL;
    }

    ret = devdrv_bind_master_para_check(&para_info);
    if (ret != 0) {
        return ret;
    }

    if (para_info.chip_id < PID_MAP_DEVNUM) {
        ret = devdrv_manager_container_logical_id_to_physical_id(para_info.chip_id, &phy_id, &vfid);
        if (ret != 0) {
            devdrv_drv_err("Transfer logical id to physical id failed. (ret=%d)\n", ret);
            return ret;
        }

        if (!devdrv_manager_is_pf_device(phy_id) || (vfid > 0)) {
            return -EOPNOTSUPP;
        }
        para_info.chip_id = phy_id;
    }

    if (para_info.chip_id == HAL_BIND_ALL_DEVICE) {
        ret = devdrv_manager_all_is_pf_device();
        if (ret != 0) {
            return ret;
        }
    }

    ret = devdrv_bind_hostpid(para_info, &cost_stat);
    if (ret) {
        cost_stat.bind_end = ka_system_ktime_get();
        bind_cost_print(&cost_stat);
        devdrv_drv_err("bind_hostpid error. dev_id:%u, ret:%d, host_pid:%d, cp_type:%d, current_pid:%d\n", node_id, ret,
                       para_info.host_pid, para_info.cp_type, ka_task_get_current_tgid());
        return ret;
    }

    return 0;
}

STATIC int devdrv_get_error_code(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    struct devdrv_error_code_para user_arg = {0, {0}, 0};
    struct devdrv_info *dev_info = NULL;
    u32 phys_id, vfid;
    int ret, i;

    ret = copy_from_user_safe(&user_arg, (void *)((uintptr_t)arg), sizeof(struct devdrv_error_code_para));
    if (ret) {
        devdrv_drv_err("copy_from_user_safe failed. (ret=%d)\n", ret);
        return ret;
    }

    if (devdrv_manager_container_logical_id_to_physical_id(user_arg.dev_id, &phys_id, &vfid)) {
        devdrv_drv_err("can't transform virt id %u.\n", user_arg.dev_id);
        return -ENODEV;
    }

    dev_info = devdrv_manager_get_devdrv_info(phys_id);

    ret = dms_hotreset_task_cnt_increase(phys_id);
    if (ret != 0) {
        devdrv_drv_err("Hotreset task cnt increase failed. (dev_id=%u; ret=%d)\n", phys_id, ret);
        return ret;
    }

    ret = devdrv_try_get_dev_info_occupy(dev_info);
    if (ret != 0) {
        devdrv_drv_err("Get dev_info occupy failed. (ret=%d; devid=%u)\n", ret, phys_id);
        dms_hotreset_task_cnt_decrease(phys_id);
        return ret;
    }

    if (devdrv_manager_shm_info_check(dev_info)) {
        devdrv_drv_err("Share memory info check fail. (dev_id=%u)\n", phys_id);
        devdrv_put_dev_info_occupy(dev_info);
        dms_hotreset_task_cnt_decrease(phys_id);
        return -EFAULT;
    }

    user_arg.error_code_count = dev_info->shm_status->error_cnt;
    for (i = 0; i < DEVMNG_SHM_INFO_ERROR_CODE_LEN; i++) {
        user_arg.error_code[i] = dev_info->shm_status->error_code[i];
    }
    devdrv_put_dev_info_occupy(dev_info);

    ret = copy_to_user_safe((void *)((uintptr_t)arg), &user_arg, sizeof(struct devdrv_error_code_para));
    if (ret != 0) {
        devdrv_drv_err("copy_to_user_safe failed.\n");
        dms_hotreset_task_cnt_decrease(phys_id);
        return ret;
    }

    dms_hotreset_task_cnt_decrease(phys_id);
    return 0;
}

int devmng_dms_get_event_code(u32 devid, u32 *health_code, u32 health_len, struct shm_event_code *event_code,
                              u32 event_len)
{
    struct devdrv_info *dev_info = NULL;
    int i, cnt, ret;

    if ((devid >= ASCEND_DEV_MAX_NUM) || (health_code == NULL) || (health_len != VMNG_VDEV_MAX_PER_PDEV) ||
        (event_code == NULL) || (event_len != DEVMNG_SHM_INFO_EVENT_CODE_LEN)) {
        devdrv_drv_err("Invalid parameter. (devid=%u; health_code=\"%s\"; health_len=%u; "
                       "event_code=\"%s\"; event_len=%u)\n",
                       devid, (health_code == NULL) ? "NULL" : "OK", health_len, (event_code == NULL) ? "NULL" : "OK",
                       event_len);
        return -EINVAL;
    }

    dev_info = devdrv_manager_get_devdrv_info(devid);

    ret = dms_hotreset_task_cnt_increase(devid);
    if (ret != 0) {
        devdrv_drv_err("Hotreset task cnt increase failed. (devid=%u; ret=%d)\n", devid, ret);
        return ret;
    }

    ret = devdrv_try_get_dev_info_occupy(dev_info);
    if (ret != 0) {
        devdrv_drv_err("Get dev_info occupy failed. (ret=%d; devid=%u)\n", ret, devid);
        dms_hotreset_task_cnt_decrease(devid);
        return ret;
    }

    if (devdrv_manager_shm_info_check(dev_info)) {
        devdrv_drv_err("The dev_info is invalid. (devid=%u)\n", devid);
        devdrv_put_dev_info_occupy(dev_info);
        dms_hotreset_task_cnt_decrease(devid);
        return -EFAULT;
    }

    for (i = 0; i < VMNG_VDEV_MAX_PER_PDEV; i++) {
        health_code[i] = dev_info->shm_status->dms_health_status[i];
    }
    cnt = (dev_info->shm_status->event_cnt > DEVMNG_SHM_INFO_EVENT_CODE_LEN) ? DEVMNG_SHM_INFO_EVENT_CODE_LEN :
                                                                               dev_info->shm_status->event_cnt;
    for (i = 0; i < cnt; i++) {
        event_code[i].event_code = dev_info->shm_status->event_code[i].event_code;
        event_code[i].fid = dev_info->shm_status->event_code[i].fid;
    }
    devdrv_put_dev_info_occupy(dev_info);

    dms_hotreset_task_cnt_decrease(devid);
    return 0;
}

STATIC int devdrv_creat_random_sign(char *random_sign, u32 len)
{
    char random[RANDOM_SIZE] = {0};
    int offset = 0;
    int ret;
    int i;

    for (i = 0; i < RANDOM_SIZE; i++) {
        ret = snprintf_s(random_sign + offset, len - offset, len - 1 - offset, "%02x", (u8)random[i]);
        if (ret < 0) {
            devdrv_drv_err("snprintf failed, ret(%d).\n", ret);
            return -EINVAL;
        }
        offset += ret;
    }
    random_sign[len - 1] = '\0';

    return 0;
}

STATIC int devdrv_get_process_sign(struct devdrv_manager_info *d_info, char *sign, u32 len, u32 docker_id)
{
    struct devdrv_process_sign *d_sign = NULL;
    ka_list_head_t *pos = NULL;
    ka_list_head_t *n = NULL;
    int ret;

    if (!ka_list_empty_careful(&d_info->hostpid_list_header)) {
        ka_list_for_each_safe(pos, n, &d_info->hostpid_list_header)
        {
            d_sign = ka_list_entry(pos, struct devdrv_process_sign, list);
            if (d_sign->hostpid == ka_task_get_current_tgid()) {
                ret = strcpy_s(sign, len, d_sign->sign);
                if (ret) {
                    devdrv_drv_err("Copy hostpid sign failed. (docker_id=%u; hostpid=%d; ret=%d)\n", docker_id,
                                   d_sign->hostpid, ret);
                    return -EINVAL;
                }

                return 0;
            }
        }
    }

    if (d_info->devdrv_sign_count[docker_id] >= DEVDRV_MAX_SIGN_NUM) {
        devdrv_drv_err("Master process is full. (max_number=%d; docker_id=%u).\n", DEVDRV_MAX_SIGN_NUM, docker_id);
        return -EINVAL;
    }

    d_sign = dbl_vmalloc(sizeof(struct devdrv_process_sign), KA_GFP_KERNEL | __KA_GFP_ZERO | __KA_GFP_ACCOUNT,
                         KA_PAGE_KERNEL);
    if (d_sign == NULL) {
        devdrv_drv_err("vzalloc failed. (docker_id=%u)\n", docker_id);
        return -ENOMEM;
    }
    d_sign->hostpid = ka_task_get_current_tgid();
#if (!defined(DEVMNG_UT)) && (!defined(DEVDRV_MANAGER_HOST_UT_TEST))
    d_sign->hostpid_start_time = ka_task_get_current_group_starttime();
#endif
    d_sign->docker_id = docker_id;

    ret = devdrv_creat_random_sign(d_sign->sign, PROCESS_SIGN_LENGTH);
    if (ret) {
        devdrv_drv_err("Get sign failed. (ret=%d; docker_id=%u)\n", ret, docker_id);
        dbl_vfree(d_sign);
        d_sign = NULL;
        return ret;
    }
    ret = strcpy_s(sign, len, d_sign->sign);
    if (ret) {
        devdrv_drv_err("strcpy_s failed. (ret=%d; docker_id=%u)\n", ret, docker_id);
        dbl_vfree(d_sign);
        d_sign = NULL;
        return -EINVAL;
    }

    ka_list_add(&d_sign->list, &d_info->hostpid_list_header);
    d_info->devdrv_sign_count[d_sign->docker_id]++;
    return 0;
}

STATIC int devdrv_manager_get_process_sign(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    struct devdrv_manager_info *d_info = devdrv_get_manager_info();
    u32 docker_id = MAX_DOCKER_NUM;
    struct process_sign dev_sign = {0};
    int ret;

    if (d_info == NULL) {
        devdrv_drv_err("d_info is null.\n");
        return -EINVAL;
    }

    /*
     * check current environment :
     * non-container : docker_id is set to 64;
     * container : get docker_id , docker_id is 0 ~ 63.
     */
    ret = devdrv_manager_container_is_in_container();
    if (ret) {
        ret = devdrv_manager_container_get_docker_id(&docker_id);
        if (ret) {
            devdrv_drv_err("container get docker_id failed, ret(%d).\n", ret);
            return -EINVAL;
        }
    }

    ka_task_mutex_lock(&d_info->devdrv_sign_list_lock);
    ret = devdrv_get_process_sign(d_info, dev_sign.sign, PROCESS_SIGN_LENGTH, docker_id);
    if (ret) {
        ka_task_mutex_unlock(&d_info->devdrv_sign_list_lock);
        devdrv_drv_err("get process_sign failed, ret(%d).\n", ret);
        return ret;
    }
    ka_task_mutex_unlock(&d_info->devdrv_sign_list_lock);

    dev_sign.tgid = ka_task_get_current_tgid();

    ret = copy_to_user_safe((void *)((uintptr_t)arg), &dev_sign, sizeof(struct process_sign));
    if (ret) {
        devdrv_drv_err("copy to user failed, ret(%d).\n", ret);
        return ret;
    }
    (void)memset_s(dev_sign.sign, PROCESS_SIGN_LENGTH, 0, PROCESS_SIGN_LENGTH);

    return 0;
}

STATIC int devdrv_host_query_process_by_host_pid(struct devdrv_ioctl_para_query_pid *para_info,
                                                 struct devdrv_info *info)
{
    int ret;
    int out_len = 0;
    ka_pid_t host_tgid = -1;
    struct devdrv_manager_msg_info dev_manager_msg_info = {{0}, {0}};
    struct devdrv_ioctl_para_query_pid *para_info_tmp;

    if (para_info == NULL || info == NULL) {
        devdrv_drv_err("para_info or info is NULL!.\n");
        return -EINVAL;
    }

    if (devdrv_get_tgid_by_pid(para_info->host_pid, &host_tgid) != 0) {
        devdrv_drv_err("Failed to get tgid by pid. (pid=%d)\n", para_info->host_pid);
        return -EINVAL;
    }
    para_info->host_pid = host_tgid;

#ifndef DEVDRV_MANAGER_HOST_UT_TEST
    if ((para_info->cp_type == DEVDRV_PROCESS_CP1) && (para_info->vfid == 0)) { /* not support host cp query */
        /* host has cp, store device cp in dev_only */
        ret = devdrv_query_process_by_host_pid(para_info->host_pid, info->dev_id, DEVDRV_PROCESS_DEV_ONLY,
                                               para_info->vfid, &para_info->pid);
        if (ret == 0) {
            return 0;
        }
    }
#endif

    dev_manager_msg_info.header.dev_id = info->dev_id;
    dev_manager_msg_info.header.msg_id = DEVDRV_MANAGER_CHAN_H2D_QUERY_DEVICE_PID;
    dev_manager_msg_info.header.valid = DEVDRV_MANAGER_MSG_VALID;
    /* give a random value for checking result later */
    dev_manager_msg_info.header.result = (u16)DEVDRV_MANAGER_MSG_INVALID_RESULT;
    /* inform corresponding devid to device side */

    para_info_tmp = (struct devdrv_ioctl_para_query_pid *)dev_manager_msg_info.payload;
    para_info_tmp->cp_type = para_info->cp_type;
    para_info_tmp->host_pid = para_info->host_pid;
    para_info_tmp->vfid = para_info->vfid;
    para_info_tmp->chip_id = para_info->chip_id;

    ret = devdrv_manager_send_msg(info, &dev_manager_msg_info, &out_len);
    if (ret != 0) {
        devdrv_drv_warn("send msg to device fail ret = %d.\n", ret);
        return ret;
    }
    if (out_len != (sizeof(struct devdrv_ioctl_para_query_pid) + sizeof(struct devdrv_manager_msg_head))) {
        devdrv_drv_warn("receive response len is not equal to expected. (len=%d; expected=%ld)\n", out_len,
                        (sizeof(struct devdrv_ioctl_para_query_pid) + sizeof(struct devdrv_manager_msg_head)));
        return -EINVAL;
    }
    if (dev_manager_msg_info.header.result != 0) {
        devdrv_drv_warn("Can not get device pid. (ret=%d).\n", dev_manager_msg_info.header.result);
        return dev_manager_msg_info.header.result;
    }

    para_info->pid = para_info_tmp->pid;
    return 0;
}

int devdrv_host_query_devpid(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    int ret;
    struct devdrv_ioctl_para_query_pid para_info;
    struct devdrv_info *info = NULL;
    unsigned int phy_id = 0;
    unsigned int vfid = 0;

    if (copy_from_user_safe(&para_info, (void *)(uintptr_t)arg, sizeof(struct devdrv_ioctl_para_query_pid))) {
        devdrv_drv_err("[devdrv_query_devpid] copy_from_user error\n");
        return -EINVAL;
    }

    ret = devdrv_manager_container_logical_id_to_physical_id(para_info.chip_id, &phy_id, &vfid);
    if (ret != 0) {
        devdrv_drv_err("can't get phys device id. virt id is %u, ret = %d.\n", para_info.chip_id, ret);
        return -EINVAL;
    }

    info = devdrv_manager_get_devdrv_info(phy_id);
    if (info == NULL) {
        devdrv_drv_err("info is NULL!.\n");
        return -EINVAL;
    }

    ret = dms_hotreset_task_cnt_increase(phy_id);
    if (ret != 0) {
        devdrv_drv_err("Hotreset task cnt increase failed. (dev_id=%u; ret=%d)\n", phy_id, ret);
        return ret;
    }

    ret = devdrv_host_query_process_by_host_pid(&para_info, info);
    if (ret) {
#ifndef DEVDRV_MANAGER_HOST_UT_TEST
        devdrv_drv_warn("Can not query device_pid by host_pid. (ret=%d; host_pid=%u; cp_type=%u)\n", ret,
                        para_info.host_pid, para_info.cp_type);
#endif
        goto out;
    }

    if (copy_to_user_safe((void *)((uintptr_t)arg), &para_info, sizeof(struct devdrv_ioctl_para_query_pid))) {
        devdrv_drv_err("[devdrv_query_devpid] copy_to_user error\n");
        ret = -EINVAL;
        goto out;
    }
    dms_hotreset_task_cnt_decrease(phy_id);
    return 0;
out:
    dms_hotreset_task_cnt_decrease(phy_id);
    return ret;
}

STATIC int devdrv_manager_container_cmd(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    return devdrv_manager_container_process(filep, arg);
}

u32 devdrv_manager_get_probe_num_kernel(void)
{
    u32 probe_num;
    int ret;
    unsigned long flags;

    ret = devdrv_manager_container_is_in_container();
    if (ret != 0) { // if in container, num is the number of device in container
        probe_num = devdrv_manager_get_devnum();
    } else {
        ka_task_spin_lock_irqsave(&devdrv_get_manager_info()->spinlock, flags);
        probe_num = devdrv_get_manager_info()->prob_num;
        ka_task_spin_unlock_irqrestore(&devdrv_get_manager_info()->spinlock, flags);
    }

    return probe_num;
}

#ifdef CFG_FEATURE_CHIP_DIE
int devdrv_manager_ioctl_get_chip_count(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    int ret;
    int count = 0;
    unsigned int phy_id = 0;
    unsigned int vfid = 0;

    ret = devdrv_manager_container_logical_id_to_physical_id(0, &phy_id, &vfid);
    if (ret != 0) {
        devdrv_drv_err("Transfer logical id to physical id failed.\n");
        return -EPERM;
    }

    if (!devdrv_manager_is_pf_device(phy_id) || (vfid > 0) || devdrv_manager_container_is_in_container()) {
        count = devdrv_manager_get_devnum();
    } else {
        ret = devdrv_manager_get_chip_count(&count);
        if (ret != 0) {
            devdrv_drv_err("Get chip count failed. (ret=%d).\n", ret);
            return ret;
        }
    }

    ret = copy_to_user_safe((void *)(uintptr_t)arg, &count, sizeof(int));
    if (ret != 0) {
        devdrv_drv_err("copy to user failed, ret=%d.\n", ret);
        return -EFAULT;
    }

    return 0;
}

int devdrv_manager_ioctl_get_chip_list(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    int ret;
    unsigned int i;
    unsigned int phy_id = 0;
    unsigned int vfid = 0;
    struct devdrv_chip_list chip_list = {0};

    ret = copy_from_user_safe(&chip_list, (void *)((uintptr_t)arg), sizeof(struct devdrv_chip_list));
    if (ret != 0) {
        devdrv_drv_err("copy_from_user_safe fail, ret(%d).\n", ret);
        return ret;
    }

    ret = devdrv_manager_container_logical_id_to_physical_id(0, &phy_id, &vfid);
    if (ret != 0) {
        devdrv_drv_err("Transfer logical id to physical id failed. (ret=%d)\n", ret);
        return -EPERM;
    }

    if (!devdrv_manager_is_pf_device(phy_id) || (vfid > 0) || devdrv_manager_container_is_in_container()) {
        chip_list.count = devdrv_manager_get_devnum();
        /*
         * devdrv_manager_get_devnum's max return is UDA_DEV_MAX_NUM here.
         * host: UDA_DEV_MAX_NUM = 100; VDAVINCI_VDEV_OFFSET = 100;
         * the VDAVINCI_VDEV_OFFSET must be equal to UDA_DEV_MAX_NUM
         **/
        if (chip_list.count > VDAVINCI_VDEV_OFFSET) {
            devdrv_drv_err("Invalid device number. (devnum=%u; max=%u)\n", chip_list.count, VDAVINCI_VDEV_OFFSET);
            return -EINVAL;
        }
        for (i = 0; i < chip_list.count; i++) {
            chip_list.chip_list[i] = i;
        }
    } else {
        ret = devdrv_manager_get_chip_list(&chip_list);
        if (ret != 0) {
            devdrv_drv_err("Get chip list failed. (ret=%d)\n", ret);
            return ret;
        }
    }

    ret = copy_to_user_safe((void *)(uintptr_t)arg, &chip_list, sizeof(struct devdrv_chip_list));
    if (ret != 0) {
        devdrv_drv_err("copy to user failed, ret=%d.\n", ret);
        return -EFAULT;
    }

    return 0;
}

int devdrv_manager_ioctl_get_device_from_chip(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    int ret;
    unsigned int phy_id = 0;
    unsigned int vfid = 0;
    struct devdrv_chip_dev_list *chip_dev_list = NULL;

    chip_dev_list = (struct devdrv_chip_dev_list *)dbl_kzalloc(sizeof(struct devdrv_chip_dev_list),
                                                               KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (chip_dev_list == NULL) {
        devdrv_drv_err("Allocate memory for chip device list failed.\n");
        return -ENOMEM;
    }

    ret = copy_from_user_safe(chip_dev_list, (void *)((uintptr_t)arg), sizeof(struct devdrv_chip_dev_list));
    if (ret != 0) {
        devdrv_drv_err("copy_from_user_safe fail, ret(%d).\n", ret);
        goto FREE_CHIP_DEV_LIST;
    }

    ret = devdrv_manager_container_logical_id_to_physical_id(0, &phy_id, &vfid);
    if (ret != 0) {
        devdrv_drv_err("Transfer logical id to physical id failed. (ret=%d)\n", ret);
        goto FREE_CHIP_DEV_LIST;
    }

    if (!devdrv_manager_is_pf_device(phy_id) || (vfid > 0) || devdrv_manager_container_is_in_container()) {
        if ((chip_dev_list->chip_id < devdrv_manager_get_devnum()) && (chip_dev_list->chip_id < DEVDRV_MAX_CHIP_NUM)) {
            chip_dev_list->count = 1;
            chip_dev_list->dev_list[0] = chip_dev_list->chip_id;
        } else {
            devdrv_drv_err("Chip id is invalid. (chip_id=%u)\n", chip_dev_list->chip_id);
            ret = -EINVAL;
            goto FREE_CHIP_DEV_LIST;
        }
    } else {
        ret = devdrv_manager_get_device_from_chip(chip_dev_list);
        if (ret != 0) {
            devdrv_drv_err("Get device list from chip id failed. (ret=%d)\n", ret);
            goto FREE_CHIP_DEV_LIST;
        }
    }

    ret = copy_to_user_safe((void *)(uintptr_t)arg, chip_dev_list, sizeof(struct devdrv_chip_dev_list));
    if (ret != 0) {
        devdrv_drv_err("copy to user failed, ret=%d.\n", ret);
        goto FREE_CHIP_DEV_LIST;
    }

FREE_CHIP_DEV_LIST:
    dbl_kfree(chip_dev_list);
    chip_dev_list = NULL;
    return ret;
}

int devdrv_manager_ioctl_get_chip_from_device(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    int ret;
    unsigned int phy_id = 0;
    unsigned int vfid = 0;
    struct devdrv_get_dev_chip_id chip_from_dev = {0};

    ret = copy_from_user_safe(&chip_from_dev, (void *)((uintptr_t)arg), sizeof(struct devdrv_get_dev_chip_id));
    if (ret != 0) {
        devdrv_drv_err("copy_from_user_safe fail, ret(%d).\n", ret);
        return ret;
    }

    ret = devdrv_manager_container_logical_id_to_physical_id(0, &phy_id, &vfid);
    if (ret != 0) {
        devdrv_drv_err("Transfer logical id to physical id failed.\n");
        return -EPERM;
    }

    if (!devdrv_manager_is_pf_device(phy_id) || (vfid > 0) || devdrv_manager_container_is_in_container()) {
        if ((chip_from_dev.dev_id < devdrv_manager_get_devnum()) && (chip_from_dev.dev_id < DEVDRV_MAX_CHIP_NUM)) {
            chip_from_dev.chip_id = chip_from_dev.dev_id;
        } else {
            devdrv_drv_err("Device id is invalid. (dev_id=%u)\n", chip_from_dev.dev_id);
            return -EINVAL;
        }
    } else {
        ret = devdrv_manager_get_chip_from_device(&chip_from_dev);
        if (ret != 0) {
            devdrv_drv_err("devdrv_manager_get_chip_from_device failed. (ret=%d)\n", ret);
            return ret;
        }
    }

    ret = copy_to_user_safe((void *)(uintptr_t)arg, &chip_from_dev, sizeof(struct devdrv_get_dev_chip_id));
    if (ret != 0) {
        devdrv_drv_err("copy to user failed, ret=%d.\n", ret);
        return -EFAULT;
    }

    return 0;
}
#endif

STATIC int devdrv_manager_get_container_flag(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    unsigned int flag;
    int ret;

    flag = (unsigned int)devdrv_manager_container_is_in_container();

    ret = copy_to_user_safe((void *)((uintptr_t)arg), &flag, sizeof(unsigned int));

    return ret;
}

STATIC int (*const devdrv_manager_ioctl_handlers[DEVDRV_MANAGER_CMD_MAX_NR])(ka_file_t *filep, unsigned int cmd,
                                                                             unsigned long arg) = {
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_PCIINFO)] = devdrv_manager_get_pci_info,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_PLATINFO)] = devdrv_manager_ioctl_get_plat_info,
    [_KA_IOC_NR(DEVDRV_MANAGER_DEVICE_STATUS)] = devdrv_manager_get_device_status,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_CORE_SPEC)] = devdrv_manager_get_core,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_CORE_INUSE)] = devdrv_manager_get_core,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_CONTAINER_DEVIDS)] = devdrv_manager_devinfo_ioctl,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_DEVINFO)] = devdrv_manager_devinfo_ioctl,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_DEVID_BY_LOCALDEVID)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_DEV_INFO_BY_PHYID)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_PCIE_ID_INFO)] = devdrv_manager_devinfo_ioctl,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_CORE_UTILIZATION)] = devdrv_manager_devinfo_ioctl,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_VOLTAGE)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_TEMPERATURE)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_TSENSOR)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_AI_USE_RATE)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_FREQUENCY)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_POWER)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_HEALTH_CODE)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_ERROR_CODE)] = devdrv_get_error_code,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_DDR_CAPACITY)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_LPM3_SMOKE)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_BLACK_BOX_GET_EXCEPTION)] = devdrv_manager_black_box_get_exception,
    [_KA_IOC_NR(DEVDRV_MANAGER_DEVICE_MEMORY_DUMP)] = devdrv_manager_device_memory_dump,
    [_KA_IOC_NR(DEVDRV_MANAGER_DEVICE_VMCORE_DUMP)] = devdrv_manager_device_vmcore_dump,
    [_KA_IOC_NR(DEVDRV_MANAGER_DEVICE_RESET_INFORM)] = devdrv_manager_device_reset_inform,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_MODULE_STATUS)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_REG_DDR_READ)] = devdrv_manager_reg_ddr_read,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_MINI_BOARD_ID)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_PCIE_PRE_RESET)] = devdrv_manager_pcie_pre_reset,
    [_KA_IOC_NR(DEVDRV_MANAGER_PCIE_RESCAN)] = devdrv_manager_pcie_rescan,
    [_KA_IOC_NR(DEVDRV_MANAGER_PCIE_HOT_RESET)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_P2P_ATTR)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_ALLOC_HOST_DMA_ADDR)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_PCIE_READ)] = drv_pcie_read,
    [_KA_IOC_NR(DEVDRV_MANAGER_PCIE_SRAM_WRITE)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_PCIE_WRITE)] = drv_pcie_write,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_EMMC_VOLTAGE)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_DEVICE_BOOT_STATUS)] = drv_get_device_boot_status,
    [_KA_IOC_NR(DEVDRV_MANAGER_ENABLE_EFUSE_LDO)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_DISABLE_EFUSE_LDO)] = NULL,

    [_KA_IOC_NR(DEVDRV_MANAGER_CONTAINER_CMD)] = devdrv_manager_container_cmd,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_HOST_PHY_MACH_FLAG)] = devdrv_manager_get_host_phy_mach_flag,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_LOCAL_DEVICEIDS)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_IMU_SMOKE)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_SET_NEW_TIME)] = NULL,
#ifndef CFG_FEATURE_REFACTOR
    [_KA_IOC_NR(DEVDRV_MANAGER_IPC_NOTIFY_CREATE)] = devdrv_manager_ipc_notify_ioctl,
    [_KA_IOC_NR(DEVDRV_MANAGER_IPC_NOTIFY_OPEN)] = devdrv_manager_ipc_notify_ioctl,
    [_KA_IOC_NR(DEVDRV_MANAGER_IPC_NOTIFY_CLOSE)] = devdrv_manager_ipc_notify_ioctl,
    [_KA_IOC_NR(DEVDRV_MANAGER_IPC_NOTIFY_DESTROY)] = devdrv_manager_ipc_notify_ioctl,
    [_KA_IOC_NR(DEVDRV_MANAGER_IPC_NOTIFY_SET_PID)] = devdrv_manager_ipc_notify_ioctl,
    [_KA_IOC_NR(DEVDRV_MANAGER_IPC_NOTIFY_RECORD)] = devdrv_manager_ipc_notify_ioctl,
    [_KA_IOC_NR(DEVDRV_MANAGER_IPC_NOTIFY_SET_ATTR)] = devdrv_manager_ipc_notify_ioctl,
    [_KA_IOC_NR(DEVDRV_MANAGER_IPC_NOTIFY_GET_INFO)] = devdrv_manager_ipc_notify_ioctl,
    [_KA_IOC_NR(DEVDRV_MANAGER_IPC_NOTIFY_GET_ATTR)] = devdrv_manager_ipc_notify_ioctl,
#else
    [_KA_IOC_NR(DEVDRV_MANAGER_IPC_NOTIFY_CREATE)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_IPC_NOTIFY_OPEN)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_IPC_NOTIFY_CLOSE)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_IPC_NOTIFY_DESTROY)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_IPC_NOTIFY_SET_PID)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_IPC_NOTIFY_RECORD)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_IPC_NOTIFY_GET_INFO)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_IPC_NOTIFY_SET_ATTR)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_IPC_NOTIFY_GET_ATTR)] = NULL,
#endif
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_CPU_INFO)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_SEND_TO_IMU)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_RECV_FROM_IMU)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_IMU_INFO)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_CONFIG_ECC_ENABLE)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_PROBE_NUM)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_PROBE_LIST)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_DEBUG_INFORM)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_COMPUTE_POWER)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_SYNC_MATRIX_DAEMON_READY)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_BBOX_ERRSTR)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_PCIE_IMU_DDR_READ)] = drv_pcie_bbox_imu_ddr_read,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_SLOT_ID)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_APPMON_BBOX_EXCEPTION_CMD)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_CONTAINER_FLAG)] = devdrv_manager_get_container_flag,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_PROCESS_SIGN)] = devdrv_manager_get_process_sign,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_MASTER_DEV_IN_THE_SAME_OS)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_LOCAL_DEV_ID_BY_HOST_DEV_ID)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_BOOT_DEV_ID)] = devdrv_manager_online_get_devids,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_TSDRV_DEV_COM_INFO)] = devdrv_manager_get_tsdrv_dev_com_info,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_CAPABILITY_GROUP_INFO)] = devdrv_manager_get_ts_group_info,
    [_KA_IOC_NR(DEVDRV_MANAGER_PASSTHRU_MCU)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_P2P_CAPABILITY)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_ETH_ID)] = NULL,
    [_KA_IOC_NR(DEVDRV_MANAGER_BIND_PID_ID)] = devdrv_fop_bind_host_pid,
    [_KA_IOC_NR(DEVDRV_MANAGER_QUERY_HOST_PID)] = devdrv_fop_query_host_pid,
    [_KA_IOC_NR(DEVDRV_MANAGER_QUERY_DEV_PID)] = devdrv_host_query_devpid,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_H2D_DEVINFO)] = devdrv_manager_devinfo_ioctl,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_CONSOLE_LOG_LEVEL)] = devdrv_manager_ioctl_get_console_loglevel,
    [_KA_IOC_NR(DEVDRV_MANAGER_CREATE_VDEV)] = devdrv_manager_ioctl_create_vdev,
    [_KA_IOC_NR(DEVDRV_MANAGER_DESTROY_VDEV)] = devdrv_manager_ioctl_destroy_vdev,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_VDEVINFO)] = devdrv_manager_ioctl_get_vdevinfo,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_STARTUP_STATUS)] = devdrv_manager_get_device_startup_status,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_DEVICE_HEALTH_STATUS)] = devdrv_manager_get_device_health_status,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_DEV_RESOURCE_INFO)] = devdrv_manager_ioctl_get_dev_resource_info,
#ifdef CFG_FEATURE_CHIP_DIE
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_CHIP_COUNT)] = devdrv_manager_ioctl_get_chip_count,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_CHIP_LIST)] = devdrv_manager_ioctl_get_chip_list,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_DEVICE_FROM_CHIP)] = devdrv_manager_ioctl_get_device_from_chip,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_CHIP_FROM_DEVICE)] = devdrv_manager_ioctl_get_chip_from_device,
#endif
    [_KA_IOC_NR(DEVDRV_MANAGER_SET_SVM_VDEVINFO)] = devdrv_manager_ioctl_set_vdevinfo,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_SVM_VDEVINFO)] = devdrv_manager_ioctl_get_svm_vdevinfo,
#ifdef CFG_FEATURE_VASCEND
    [_KA_IOC_NR(DEVDRV_MANAGER_SET_VDEVMODE)] = devdrv_manager_ioctl_set_vdevmode,
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_VDEVMODE)] = devdrv_manager_ioctl_get_vdevmode,
#endif
    [_KA_IOC_NR(DEVDRV_MANAGER_GET_VDEVIDS)] = devdrv_manager_devinfo_ioctl,
    [_KA_IOC_NR(DEVDRV_MANAGER_TS_LOG_DUMP)] = devdrv_manager_tslog_dump,
#ifdef CFG_FEATURE_DEVICE_SHARE
    [_KA_IOC_NR(DEVDRV_MANAGER_CONFIG_DEVICE_SHARE)] = devdrv_manager_config_device_share,
#endif
};

long devdrv_manager_ioctl(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    if (devdrv_get_manager_info() == NULL) {
        devdrv_drv_err("invalid parameter, "
                       "dev_manager_info = %pK, "
                       "arg = 0x%lx\n",
                       devdrv_get_manager_info(), arg);
        return -EINVAL;
    }

    if (_KA_IOC_NR(cmd) >= DEVDRV_MANAGER_CMD_MAX_NR) {
        devdrv_drv_err("cmd out of range, cmd = %u, max value = %u\n", _KA_IOC_NR(cmd), DEVDRV_MANAGER_CMD_MAX_NR);
        return -EINVAL;
    }

    if (devdrv_manager_ioctl_handlers[_KA_IOC_NR(cmd)] == NULL) {
        devdrv_drv_err("invalid cmd, cmd = %u\n", _KA_IOC_NR(cmd));
        return -EINVAL;
    }

    return devdrv_manager_ioctl_handlers[_KA_IOC_NR(cmd)](filep, cmd, arg);
}
