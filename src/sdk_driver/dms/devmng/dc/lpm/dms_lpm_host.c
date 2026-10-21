/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
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

#include "ka_system_pub.h"
#include "ka_task_pub.h"
#include "ka_base_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_errno_pub.h"

#include "dms_template.h"
#include "pbl/pbl_uda.h"
#include "pbl/pbl_feature_loader.h"
#include "dms/dms_cmd_def.h"
#include "dms_urd_forward.h"
#include "dms_feature.h"
#include "dms_lpm_cal.h"

#define DMS_DEV_ONLINE_WAIT_CNT 300
#define DMS_DEV_ONLINE_INTERVAL_TIME 100
#define DMS_DEV_ONLINE 1
#define DMS_DEV_OFFLINE 0
#define DMS_TASK_INITED 1
#define DMS_TASK_NOT_INITED 0

struct lpm_power_cons_coef_info {
    unsigned int dev_id;    /* Device id */
    unsigned int reportPwr; /* from device */
    unsigned int fittedPwr; /* to device */
};

struct dms_lpm_dev_wait_stru {
    ka_atomic_t task_inited;
    unsigned int reportPwr[ASCEND_PDEV_MAX_NUM];
    unsigned int fittedPwr[ASCEND_PDEV_MAX_NUM];
    unsigned int last_dev_num;
    unsigned int dev_online_list[ASCEND_PDEV_MAX_NUM];
    ka_task_struct_t *task;
};

STATIC struct dms_lpm_dev_wait_stru g_dms_lpm_dev_wait_stru = {0};
STATIC KA_TASK_DEFINE_MUTEX(g_dms_lpm_task_mutex);

int dms_lpm_get_info_from_device(u32 dev_id)
{
    int ret;
    DMS_FEATURE_S feature_cfg = {0};
    struct lpm_power_cons_coef_info power_info = {0};

    feature_cfg.main_cmd = DMS_MAIN_CMD_LPM;
    feature_cfg.sub_cmd = DMS_SUBCMD_POWER_CONS_COEF;
    feature_cfg.filter = "main_cmd=0x48,sub_cmd=0x13,get";
    feature_cfg.owner_name = DMS_URD_FORWARD_CMD_NAME;
    feature_cfg.privilege = DMS_ACC_ALL | DMS_ENV_ALL | DMS_VDEV_NOTSUPPORT;
    feature_cfg.proc_ctrl_str = NULL;
    feature_cfg.handler = dms_send_msg_to_device_by_h2d_kernel;
    power_info.dev_id = dev_id;

    ret = dms_send_msg_to_device_by_h2d_kernel(&feature_cfg, (char *)&power_info,
                                               sizeof(struct lpm_power_cons_coef_info), (char *)&power_info,
                                               sizeof(struct lpm_power_cons_coef_info));
    if (ret != 0) {
        dms_err("Get info from device failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }
    g_dms_lpm_dev_wait_stru.reportPwr[dev_id] = power_info.reportPwr;
    dms_info("Get info from device success. (dev_id=%u; get_val=%u)\n", dev_id, power_info.reportPwr);
    return 0;
}

int dms_lpm_send_info_to_device(u32 dev_id)
{
    int ret;
    DMS_FEATURE_S feature_cfg = {0};
    struct lpm_power_cons_coef_info power_info = {0};

    feature_cfg.main_cmd = DMS_MAIN_CMD_LPM;
    feature_cfg.sub_cmd = DMS_SUBCMD_POWER_CONS_COEF;
    feature_cfg.filter = "main_cmd=0x48,sub_cmd=0x13,set";
    feature_cfg.owner_name = DMS_URD_FORWARD_CMD_NAME;
    feature_cfg.privilege = DMS_ACC_ROOT | DMS_ENV_ALL | DMS_VDEV_NOTSUPPORT;
    feature_cfg.proc_ctrl_str = NULL;
    feature_cfg.handler = dms_send_msg_to_device_by_h2d_kernel;
    power_info.dev_id = dev_id;
    power_info.fittedPwr = g_dms_lpm_dev_wait_stru.fittedPwr[dev_id];

    ret = dms_send_msg_to_device_by_h2d_kernel(&feature_cfg, (char *)&power_info,
                                               sizeof(struct lpm_power_cons_coef_info), (char *)&power_info,
                                               sizeof(struct lpm_power_cons_coef_info));
    if (ret != 0) {
        dms_err("Send info to device failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }
    dms_info("Send info to device success. (dev_id=%u; set_val=%u)\n", dev_id,
             g_dms_lpm_dev_wait_stru.fittedPwr[dev_id]);
    return 0;
}

int dms_dev_online_handle(void)
{
    int i = 0;
    int ret = 0;

    for (i = 0; i < ASCEND_PDEV_MAX_NUM; i++) {
        if (g_dms_lpm_dev_wait_stru.dev_online_list[i] == DMS_DEV_OFFLINE) {
            continue;
        }
        ret = dms_lpm_get_info_from_device(i);
        if (ret != 0) {
            dms_ex_notsupport_err(ret, "Get info from device failed. (dev_id=%u; ret=%d)\n", i, ret);
            g_dms_lpm_dev_wait_stru.reportPwr[i] = 0;
        }
    }

    dms_cal_dev_fitted_power(g_dms_lpm_dev_wait_stru.reportPwr, ASCEND_PDEV_MAX_NUM, g_dms_lpm_dev_wait_stru.fittedPwr);
    for (i = 0; i < ASCEND_PDEV_MAX_NUM; i++) {
        if (g_dms_lpm_dev_wait_stru.dev_online_list[i] == DMS_DEV_OFFLINE) {
            continue;
        }
        ret = dms_lpm_send_info_to_device(i);
        if (ret != 0) {
            dms_ex_notsupport_err(ret, "Send info to device failed. (dev_id=%u; ret=%d)\n", i, ret);
        }
    }

    return ret;
}

STATIC void dms_wait_online_handle(void)
{
    int timeout_cnt = 0;
    int i = 0;
    int ret = 0;
    struct devdrv_info *dev_info = NULL;
    unsigned int dev_num = 0;

    while (timeout_cnt <= DMS_DEV_ONLINE_WAIT_CNT) {
        if (ka_task_kthread_should_stop() == true) {
            return;
        }
        dev_num = 0;
        for (i = 0; i < ASCEND_PDEV_MAX_NUM; i++) {
            dev_info = devdrv_manager_get_devdrv_info(i);
            if ((dev_info != NULL) && (dev_info->dmp_started == true)) {
                dev_num++;
                g_dms_lpm_dev_wait_stru.dev_online_list[i] = DMS_DEV_ONLINE;
            }
        }
        if (dev_num == uda_get_detected_phy_dev_num()) {
            break;
        }
        ka_system_msleep(DMS_DEV_ONLINE_INTERVAL_TIME);
        timeout_cnt++;
    }
    if (timeout_cnt >= DMS_DEV_ONLINE_WAIT_CNT) {
        dms_debug("dms_wait_all_dev_online timeout. (current dev_num=%u)\n", dev_num);
    }
    if (dev_num > g_dms_lpm_dev_wait_stru.last_dev_num) { // new dev online
        ret = dms_dev_online_handle();
        if (ret != 0) {
            dms_ex_notsupport_err(ret, "dms_dev_online_handle failed. (ret=%d)\n", ret);
        }
    }
    g_dms_lpm_dev_wait_stru.last_dev_num = dev_num;
}

STATIC int dms_wait_all_dev_online_task(void *data)
{
    struct dms_lpm_dev_wait_stru *task_stru = (struct dms_lpm_dev_wait_stru *)data;

    if (task_stru == NULL) {
        dms_err("Input task_stru is NULL.\n");
        return -EINVAL;
    }

    while (ka_task_kthread_should_stop() == false) {
        if (ka_base_atomic_read(&task_stru->task_inited) != DMS_TASK_NOT_INITED) {
            dms_wait_online_handle();
        }
        ka_system_msleep(DMS_DEV_ONLINE_INTERVAL_TIME);
    }
    return 0;
}

STATIC void dms_wait_all_dev_online(void)
{
    ka_task_struct_t *task = NULL;

    ka_task_mutex_lock(&g_dms_lpm_task_mutex);
    if (ka_base_atomic_read(&g_dms_lpm_dev_wait_stru.task_inited) != DMS_TASK_NOT_INITED) {
        goto out;
    }
    ka_base_atomic_set(&g_dms_lpm_dev_wait_stru.task_inited, DMS_TASK_INITED);

    task = ka_task_kthread_run(dms_wait_all_dev_online_task, (void *)&g_dms_lpm_dev_wait_stru, "wait_dev_task");
    if (KA_IS_ERR(task) || (task == NULL)) {
        dms_err("dms_wait_all_dev_online_task create failed. (error=%ld)\n", KA_PTR_ERR(task));
        ka_base_atomic_set(&g_dms_lpm_dev_wait_stru.task_inited, DMS_TASK_NOT_INITED);
        goto out;
    }
    g_dms_lpm_dev_wait_stru.task = task;
out:
    ka_task_mutex_unlock(&g_dms_lpm_task_mutex);
}

int dms_lpm_dev_init(u32 udev_id)
{
    if (!dms_has_feature(udev_id, DMS_RECAL_LPM_POWER)) {
        return 0;
    }

    if (!uda_is_pf_dev(udev_id)) {
        return 0;
    }

    dms_wait_all_dev_online();

    dms_info("Dms lpm dev init success. (udev_id=%u)\n", udev_id);
    return 0;
}
DECLAER_FEATURE_AUTO_INIT_DEV(dms_lpm_dev_init, FEATURE_LOADER_STAGE_5);

void dms_lpm_dev_uninit(u32 udev_id)
{
    if (!dms_has_feature(udev_id, DMS_RECAL_LPM_POWER)) {
        return;
    }

    if (!uda_is_pf_dev(udev_id)) {
        return;
    }
    g_dms_lpm_dev_wait_stru.reportPwr[udev_id] = 0;
    g_dms_lpm_dev_wait_stru.fittedPwr[udev_id] = 0;
    g_dms_lpm_dev_wait_stru.dev_online_list[udev_id] = DMS_DEV_OFFLINE;
    dms_info("Dms lpm dev exit. (udev_id=%u)\n", udev_id);
}
DECLAER_FEATURE_AUTO_UNINIT_DEV(dms_lpm_dev_uninit, FEATURE_LOADER_STAGE_5);

int dms_lpm_init(void)
{
    dms_info("Dms lpm init success.\n");
    return 0;
}
DECLAER_FEATURE_AUTO_INIT(dms_lpm_init, FEATURE_LOADER_STAGE_5);

void dms_lpm_uninit(void)
{
    if (g_dms_lpm_dev_wait_stru.task != NULL) {
        ka_task_kthread_stop(g_dms_lpm_dev_wait_stru.task);
        g_dms_lpm_dev_wait_stru.task = NULL;
        ka_base_atomic_set(&g_dms_lpm_dev_wait_stru.task_inited, DMS_TASK_NOT_INITED);
    }

    dms_info("Dms lpm exit.\n");
}
DECLAER_FEATURE_AUTO_UNINIT(dms_lpm_uninit, FEATURE_LOADER_STAGE_5);
