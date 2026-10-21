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

#include "ka_errno_pub.h"
#include "asdrv_ub_main.h"
#include "asdrv_ub_admin_msg.h"
#include "asdrv_ub_hotreset.h"
#include "asdrv_ub_link_chan.h"
#include "asdrv_ub_pair_dev_info.h"

STATIC KA_TASK_DEFINE_MUTEX(g_ub_hot_mutex);
STATIC int g_ub_hot_reset_status[ASCEND_UB_PF_DEV_MAX_NUM] = {0};
#define UB_HOT_RESET_ALL_DEVICE_MASK (~0x0ULL)

STATIC int ubdrv_device_set_hot_reset_flag(u32 dev_id, enum ubdrv_dev_status expect_flag,
                                           enum ubdrv_dev_status new_flag)
{
    struct ascend_ub_dev_status *status_mng = NULL;
    int status;

    status_mng = ubdrv_get_dev_status_mng(dev_id);
    ka_task_down_write(&status_mng->rw_sem);
    status = status_mng->device_status;
    if (status == expect_flag) {
        status_mng->device_status = new_flag;
        ka_task_up_write(&status_mng->rw_sem);
        return 0;
    } else {
        ubdrv_warn("Device not support reset. (dev_id=%u;status=%d;uid=%u)\n", dev_id, status,
                   ka_task_get_current_cred_uid());
        ka_task_up_write(&status_mng->rw_sem);
    }
    return -ENODEV;
}

STATIC int ubdrv_pod_hot_reset_single_device(u32 dev_id)
{
    struct ascend_ub_user_data user_desc = {0};
    u32 devid = dev_id;
    int ret;

    user_desc.opcode = UBDRV_HOST_HOT_RESET;
    user_desc.size = (u32)sizeof(u32);
    user_desc.reply_size = (u32)sizeof(u32);
    user_desc.cmd = &devid;
    user_desc.reply = NULL;
    ret = ubdrv_device_set_hot_reset_flag(dev_id, UBDRV_DEVICE_ONLINE, UBDRV_DEVICE_BEGIN_OFFLINE);
    if (ret != 0) {
        ubdrv_err("Failed to set hot reset flag. (dev_id=%u;ret=%d;uid=%u)\n", dev_id, ret,
                  ka_task_get_current_cred_uid());
        return ret;
    }
    devdrv_ub_set_device_boot_status(dev_id, DSMI_BOOT_STATUS_UNINIT);
    /* 1. remove davinci dev to uninit */
    ubdrv_remove_davinci_dev(dev_id, UDA_REAL);
    /* 2. send admin msg to dev for hot reset */
    ret = ubdrv_admin_send_msg(dev_id, &user_desc);
    if (ret != 0) {
        ubdrv_err("Send msg to reset device fail. (dev_id=%u;ret=%d;uid=%u)\n", dev_id, ret,
                  ka_task_get_current_cred_uid());
    }
    ubdrv_set_device_status(dev_id, UBDRV_DEVICE_DEAD);
    ubdrv_del_msg_device(dev_id, UBDRV_DEVICE_UNINIT);
    ubdrv_info("Hot reset pod finish. (dev_id=%u;ret=%d;uid=%u)\n", dev_id, ret, ka_task_get_current_cred_uid());
    return ret;
}

STATIC int ubdrv_hot_reset_all_device(void)
{
    int ret = 0, single_ret = 0;
    u32 i;

    for (i = 0; i < ASCEND_UB_PF_DEV_MAX_NUM; i++) {
        if (ubdrv_get_device_status(i) != UBDRV_DEVICE_ONLINE) {
            continue;
        }
        single_ret = ubdrv_pod_hot_reset_single_device(i);
        if (single_ret != 0) {
            ret |= single_ret;
            ubdrv_err("Hot reset failed. (single_ret=%d;ret=%d;dev_id=%u)\n", single_ret, ret, i);
        }
    }
    return ret;
}

STATIC int ubdrv_check_reset_status(u32 dev_id)
{
    int i = 0;

    if (dev_id != 0xffff) {
        if (g_ub_hot_reset_status[dev_id] != 0) {
            ubdrv_warn("The device is being reset. (dev_id=%u)\n", dev_id);
            return -EAGAIN;
        }
        return 0;
    }
    for (i = 0; i < ASCEND_UB_PF_DEV_MAX_NUM; i++) {
        if (g_ub_hot_reset_status[i] != 0) {
            ubdrv_warn("The device is being reset. (dev_id=%u)\n", i);
            return -EAGAIN;
        }
    }
    return 0;
}

STATIC void ubdrv_set_reset_status(u32 dev_id, int status)
{
    int i = 0;

    if (dev_id != 0xffff) {
        g_ub_hot_reset_status[dev_id] = status; // 1:reset 0:not reset
        return;
    }
    for (i = 0; i < ASCEND_UB_PF_DEV_MAX_NUM; i++) {
        g_ub_hot_reset_status[i] = status; // 1:reset 0:not reset
    }
    return;
}

STATIC int ubdrv_set_hot_reset_bitmap(u32 dev_id)
{
    int ret;

    ka_task_mutex_lock(&g_ub_hot_mutex);
    ret = ubdrv_check_reset_status(dev_id);
    if (ret != 0) {
        ka_task_mutex_unlock(&g_ub_hot_mutex);
        return ret;
    }
    ubdrv_set_reset_status(dev_id, 1); // 1:being reset
    ka_task_mutex_unlock(&g_ub_hot_mutex);
    return ret;
}

STATIC void ubdrv_clr_hot_reset_bitmap(u32 dev_id)
{
    ka_task_mutex_lock(&g_ub_hot_mutex);
    ubdrv_set_reset_status(dev_id, 0); // 0:not reset
    ka_task_mutex_unlock(&g_ub_hot_mutex);
}

int ubdrv_hot_reset_device(u32 dev_id)
{
    int ret = 0;

    if ((dev_id >= ASCEND_UB_PF_DEV_MAX_NUM) && (dev_id != 0xffff)) {
        ubdrv_err("Invalid dev_id. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    ret = ubdrv_set_hot_reset_bitmap(dev_id);
    if (ret != 0) {
        return ret;
    }

    if (dev_id == 0xffff) {
        ret = ubdrv_hot_reset_all_device();
    } else {
        ret = ubdrv_pod_hot_reset_single_device(dev_id);
    }
    if (ret == 0) {
        ubdrv_info("Hotreset, hot reset success. (dev_id=%u;uid=%u)\n", dev_id, ka_task_get_current_cred_uid());
    }

    ubdrv_clr_hot_reset_bitmap(dev_id);
    return ret;
}

int ubdrv_normal_preset(u32 dev_id)
{
    int ret;

    ret = ubdrv_device_set_hot_reset_flag(dev_id, UBDRV_DEVICE_ONLINE, UBDRV_DEVICE_DEAD);
    if ((ret != 0) && (ubdrv_get_device_status(dev_id) != UBDRV_DEVICE_DEAD)) {
        // for host reset case: lost device heart
        ubdrv_err("Device pre_reset fail. (dev_id=%u;dev_status=%d)\n", dev_id, ubdrv_get_device_status(dev_id));
        return ret;
    }
    devdrv_ub_set_device_boot_status(dev_id, DSMI_BOOT_STATUS_UNINIT);
    ubdrv_remove_davinci_dev(dev_id, UDA_REAL);
    ubdrv_del_msg_device(dev_id, UBDRV_DEVICE_UNINIT);
    ubdrv_info("Normal pre reset success. (dev_id=%u;uid=%u)\n", dev_id, ka_task_get_current_cred_uid());
    return 0;
}

STATIC int ubdrv_fe_reset_preset(u32 dev_id)
{
    struct ub_idev *idev = NULL;
    int ret;

    if (dev_id < ASCEND_UB_PF_DEV_MAX_NUM) {
        ret = ubdrv_init_h2d_eid_index(dev_id);
        if (ret != 0) {
            return ret;
        }
    }

    idev = ubdrv_find_idev_by_udevid(dev_id);
    if (idev == NULL) {
        ubdrv_err("Find idev fail, when pre_reset. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    ret = ubdrv_create_single_link_chan(idev);
    if (ret != 0) {
        ubdrv_err("Create link jetty fail, when pre_reset. (dev_id=%u;idev_id=%u;ue_idx=%u)\n", dev_id, idev->idev_id,
                  idev->ue_idx);
        return ret;
    }
    ubdrv_set_device_status(dev_id, UBDRV_DEVICE_UNINIT);
    ubdrv_info("Pre reset success, in fe reset case. (dev_id=%u;uid=%u)\n", dev_id, ka_task_get_current_cred_uid());
    return 0;
}

int ubdrv_device_pre_reset(u32 dev_id)
{
    if (dev_id >= ASCEND_UB_PF_DEV_MAX_NUM) {
        ubdrv_err("Invalid dev_id. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    if (ubdrv_get_device_status(dev_id) != UBDRV_DEVICE_FE_RESET) {
        return ubdrv_normal_preset(dev_id);
    }
    return ubdrv_fe_reset_preset(dev_id);
}

int ubdrv_device_rescan(u32 dev_id)
{
    if (dev_id >= ASCEND_UB_PF_DEV_MAX_NUM) {
        ubdrv_err("Invalid dev_id. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    ubdrv_info("Hot rescan success. (dev_id=%u; uid=%u)\n", dev_id, ka_task_get_current_cred_uid());
    return 0;
}