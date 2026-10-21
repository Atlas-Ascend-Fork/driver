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

#include "ascend_kernel_hal.h"
#include "davinci_interface.h"
#include "comm_kernel_interface.h"
#include "devdrv_manager_common.h"
#include "soft_fault_define.h"
#include "tsdrv_status.h"
#include "pbl/pbl_uda.h"
#include "dms_timer.h"
#include "devdrv_common.h"
#include "devdrv_pm.h"
#include "pbl_mem_alloc_interface.h"
#include "heart_beat.h"
#include "pbl/pbl_davinci_api.h"
#include "ka_base_pub.h"
#include "ka_task_pub.h"
#include "ka_list_pub.h"
#include "ka_system_pub.h"
#include "ka_memory_pub.h"
#include "hb_read.h"

#ifdef CFG_ENV_FPGA
#define HEART_BEAT_TIMER_EXPIRE_MS (6000 * 50)
#define HEART_BEAT_TIMER_URGENT_EXPIRE_MS (500 * 50)
#else
#define HEART_BEAT_TIMER_EXPIRE_MS 6000
#define HEART_BEAT_TIMER_URGENT_EXPIRE_MS (50)
#endif

#ifndef DMS_UT
unsigned int heart_beat_get_max_lost_count(unsigned int dev_id)
{
    static unsigned int max_lost_count = KA_UINT_MAX;
    int soc_type = -1;
    int ret;

    if (max_lost_count != KA_UINT_MAX) {
        return max_lost_count;
    }
    ret = hal_kernel_get_soc_type(dev_id, &soc_type);
    if (ret != 0) {
        return KA_UINT_MAX;
    }
    if (soc_type == SOC_TYPE_CLOUD_V3) {
        max_lost_count = HEART_BEAT_HCCS_DEATH_COUNT;
    } else {
        max_lost_count = HEART_BEAT_DEATH_COUNT;
    }

    return max_lost_count;
}

int hb_set_heart_beat_count(unsigned int dev_id, unsigned long long count)
{
    return devdrv_set_heartbeat_count(dev_id, count);
}

int hb_get_heart_beat_count(unsigned int dev_id, unsigned long long *heart_beat_count, unsigned int *hb_read_fail_count)
{
    int ret = 0;

#ifdef CFG_FEATURE_HEARTBEAT_CNT_PCIE
    // get heartbeat count by pcie api
    ret = devdrv_get_heartbeat_count(dev_id, heart_beat_count);
#else
    struct devdrv_info *dev_info = devdrv_manager_get_devdrv_info(dev_id);

    ret = devdrv_try_get_dev_info_occupy(dev_info);
    if (ret != 0) {
        dms_err("Get dev_info occupy failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    if (dev_info->shm_heartbeat == NULL) {
        devdrv_put_dev_info_occupy(dev_info);
        dms_err("Shm heartbeat status is NULL. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    if (devdrv_get_connect_protocol(dev_id) == CONNECT_PROTOCOL_UB) {
        ret = devdrv_rao_read(dev_id, DEVDRV_RAO_CLIENT_DEVMNG, dev_info->shm_head->head_info.offset_heartbeat,
                              sizeof(U_SHM_INFO_HEARTBEAT));
        if (ret != 0) {
            devdrv_put_dev_info_occupy(dev_info);
            (*hb_read_fail_count)++;
            dms_err("Read rao data fail. (dev_id=%u; ret=%d)\n", dev_id, ret);
            return ret;
        } else {
            (*hb_read_fail_count) = 0;
        }
        *heart_beat_count = dev_info->shm_heartbeat->heartbeat_cnt;
        devdrv_put_dev_info_occupy(dev_info);
    } else {
        *heart_beat_count = dev_info->shm_heartbeat->heartbeat_cnt;
        devdrv_put_dev_info_occupy(dev_info);
    }
#endif
    return ret;
}

int check_and_update_link_abnormal_status(u32 dev_id, u64 count)
{
    int ret;
    u32 status = 0;
    struct ascend_intf_get_status_para status_para = {0};

    if (count == KA_U64_MAX) {
        (void)ascend_intf_report_device_status(dev_id, DAVINCI_INTF_DEVICE_STATUS_LINK_ABNORMAL);
    } else {
        status_para.para.device_id = dev_id;
        status_para.type = DAVINCI_STATUS_TYPE_DEVICE;
        ret = ascend_intf_get_status(status_para, &status);
        if (ret != 0) {
            soft_drv_warn("Can't get link status. (dev_id=%u; ret=%d)\n", dev_id, ret);
            return ret;
        }

        if ((status & DAVINCI_INTF_DEVICE_STATUS_LINK_ABNORMAL) == DAVINCI_INTF_DEVICE_STATUS_LINK_ABNORMAL) {
            (void)ascend_intf_report_device_status(
                dev_id, DAVINCI_INTF_DEVICE_CLEAR_STATUS | DAVINCI_INTF_DEVICE_STATUS_LINK_ABNORMAL);
        }
    }

    return DRV_ERROR_NONE;
}

STATIC void host_manager_device_exception(u32 dev_id)
{
    struct devdrv_manager_info *d_info = NULL;
    ka_list_head_t *pos = NULL;
    ka_list_head_t *n = NULL;
    struct devdrv_pm *pm = NULL;

    d_info = devdrv_get_manager_info();
    if (d_info == NULL) {
        soft_drv_err("d_info is NULL. (dev_id=%u) \n", dev_id);
        return;
    }

    ka_task_mutex_lock(&d_info->pm_list_lock);
    if (!ka_list_empty_careful(&d_info->pm_list_header)) {
        ka_list_for_each_safe(pos, n, &d_info->pm_list_header)
        {
            pm = ka_list_entry(pos, struct devdrv_pm, list);
            if (pm->ts_status_notify != NULL) {
                (void)pm->ts_status_notify(dev_id, TS_DOWN);
            }
        }
    }
    ka_task_mutex_unlock(&d_info->pm_list_lock);
}

int hb_report_heart_beat_lost_event(unsigned int dev_id)
{
    int ret;
    int tsid = 0;

    int data_len;
    struct soft_fault heartbeat_fault;
    char *fault_info = "device heartbeat lost";

    data_len = ka_base_strlen(fault_info) + 1;
    ret = memcpy_s(heartbeat_fault.data, DMS_MAX_EVENT_DATA_LENGTH, fault_info, data_len);
    if (ret != 0) {
        soft_drv_err("Memcpy failed! (device=%u) \n", dev_id);
    }

    heartbeat_fault.data_len = data_len;
    heartbeat_fault.dev_id = dev_id;
    heartbeat_fault.sensor_type = DMS_SEN_TYPE_HEARTBEAT;
    heartbeat_fault.user_id = SF_SENSOR_DAVINCI;
    heartbeat_fault.node_type = DMS_DEV_TYPE_BASE_SERVCIE;
    heartbeat_fault.node_id = SF_USER_NODE_DAVINCI;
    heartbeat_fault.sub_id = SF_SUB_ID0;
    heartbeat_fault.err_type = HEARTBEAT_ERROR_TYPE_HEARTBEAT_LOST;
    heartbeat_fault.assertion = GENERAL_EVENT_TYPE_OCCUR;

    ret = soft_fault_event_handler(&heartbeat_fault);
    if (ret != 0) {
        soft_drv_err("Soft fault report failed! (device=%u) \n", dev_id);
    }

    devdrv_hb_broken_stop_msg_send(dev_id);
    tsdrv_set_ts_status(dev_id, tsid, TS_DOWN);
    host_manager_device_exception(dev_id);

    ret = uda_dev_ctrl(dev_id, UDA_CTRL_COMMUNICATION_LOST);
    return ret;
}

void heartbeat_resume(u32 dev_id)
{
    int ret;
    int data_len;
    struct soft_fault heartbeat_fault;
    const char *fault_info = "device heartbeat resume";

    data_len = ka_base_strlen(fault_info) + 1;
    ret = memcpy_s(heartbeat_fault.data, DMS_MAX_EVENT_DATA_LENGTH, fault_info, data_len);
    if (ret != 0) {
        soft_drv_err("Memcpy failed! (device=%u) \n", dev_id);
        return;
    }

    heartbeat_fault.data_len = data_len;
    heartbeat_fault.dev_id = dev_id;
    heartbeat_fault.sensor_type = DMS_SEN_TYPE_HEARTBEAT;
    heartbeat_fault.user_id = SF_SENSOR_DAVINCI;
    heartbeat_fault.node_type = DMS_DEV_TYPE_BASE_SERVCIE;
    heartbeat_fault.node_id = SF_USER_NODE_DAVINCI;
    heartbeat_fault.sub_id = SF_SUB_ID0;
    heartbeat_fault.err_type = HEARTBEAT_ERROR_TYPE_HEARTBEAT_LOST;
    heartbeat_fault.assertion = GENERAL_EVENT_TYPE_RESUME;
    ret = soft_fault_event_handler(&heartbeat_fault);
    if (ret != 0) {
        soft_drv_warn("Report heartbeat recover failed! (device=%u) \n", dev_id);
        return;
    }
    return;
}

int hb_read_item_work_start(unsigned int dev_id, struct hb_read_block *hb_read_item)
{
    if (hb_read_item->hb_stutas == HEART_BEAT_LOST) {
        soft_drv_err("Heartbeat already initialized, just start it. (dev_id=%u)\n", dev_id);
        heartbeat_resume(dev_id); /* if heartbeat lost before and resume by hot-reset, it should clear here. */
    }
    hb_read_item->dev_id = dev_id;
    hb_read_item->hb_stutas = HEART_BEAT_READY;
    hb_read_item->old_count = 0;
    hb_read_item->lost_count = 0;
    hb_read_item->total_lost_count = 0;
    hb_read_item->miss_read_count = 0;
    hb_read_item->last_read_time = ka_system_ktime_get_raw_ns();
    (void)ascend_intf_report_device_status(dev_id, DAVINCI_INTF_DEVICE_CLEAR_STATUS |
                                                       DAVINCI_INTF_DEVICE_STATUS_HEARTBIT_LOST |
                                                       DAVINCI_INTF_DEVICE_STATUS_LINK_ABNORMAL);
    return 0;
}

STATIC int heartbeat_dev_node_config(unsigned int user_id, struct soft_dev *s_dev)
{
    int ret;
    int pid = ka_task_get_current_tgid();
    u32 len = DMS_SENSOR_DESCRIPT_LENGTH;
    struct dms_node_operations *soft_ops = soft_get_ops();
    struct dms_node s_node = SOFT_NODE_DEF(DMS_DEV_TYPE_BASE_SERVCIE, "davinci", s_dev->dev_id, s_dev->node_id,
                                           soft_ops);
    struct dms_sensor_object_cfg s_cfg = SOFT_SENSOR_DEF(DMS_SEN_TYPE_HEARTBEAT, "dev0_heartbeat", 0UL, user_id,
                                                         SF_SUB_ID0, AST_MASK, DST_MASK, SF_SENSOR_SCAN_TIME,
                                                         soft_fault_event_scan, pid);

    ret = snprintf_s(s_cfg.sensor_name, len, len - 1, "dev%u_heartbeat", s_dev->dev_id);
    if (ret <= 0) {
        soft_drv_err("snprintf_s failed. (dev_id=%u; ret=%d)\n", s_dev->dev_id, ret);
        return ret;
    }

    s_cfg.private_data = soft_combine_private_data(s_dev->dev_id, user_id, DMS_DEV_TYPE_BASE_SERVCIE, s_dev->node_id,
                                                   SF_SUB_ID0);
    /* for kernel space, not used the pid, set default value -1 */
    s_cfg.pid = -1;
    s_node.pid = -1;
    s_dev->sensor_obj_num++;
    s_dev->sensor_obj_table[SF_SUB_ID0] = s_cfg;
    s_dev->dev_node = s_node;

    return 0;
}

int heartbeat_dev_register(u32 dev_id)
{
    int ret;
    unsigned int user_id = SF_SENSOR_DAVINCI;
    struct soft_dev_client *client = NULL;
    struct soft_dev *s_dev = NULL;
    struct drv_soft_ctrl *soft_ctrl = soft_get_ctrl();

    ka_task_mutex_lock(&soft_ctrl->mutex[dev_id]);
    client = soft_ctrl->s_dev_t[dev_id][user_id];
    if (client->registered == 1) {
        soft_drv_warn("heartbeat is registered, return. (dev_id=%u; registered=%u)\n", dev_id, client->registered);
        ka_task_mutex_unlock(&soft_ctrl->mutex[dev_id]);
        return 0;
    }

    s_dev = dbl_kzalloc(sizeof(struct soft_dev), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (s_dev == NULL) {
        soft_drv_err("kzalloc soft_dev failed.\n");
        ka_task_mutex_unlock(&soft_ctrl->mutex[dev_id]);
        return -ENOMEM;
    }

    soft_one_dev_init(s_dev);
    s_dev->dev_id = dev_id;
    s_dev->node_id = SF_USER_NODE_DAVINCI;
    ret = heartbeat_dev_node_config(user_id, s_dev);
    if (ret != 0) {
        soft_drv_err("soft_fault register one node failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        goto ERROR;
    }

    ret = soft_register_one_node(s_dev);
    if (ret != 0) {
        soft_drv_err("soft_fault register one node failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        goto ERROR;
    }

    ka_list_add(&s_dev->list, &client->head);
    client->user_id = user_id;
    client->registered = 1;
    client->node_num++;
    soft_ctrl->user_num[dev_id]++;
    ka_task_mutex_unlock(&soft_ctrl->mutex[dev_id]);

    return 0;
ERROR:
    dbl_kfree(s_dev);
    s_dev = NULL;
    ka_task_mutex_unlock(&soft_ctrl->mutex[dev_id]);
    return ret;
}

void heartbeat_dev_unregister(void)
{
    int i;
    struct soft_dev_client *client = NULL;
    struct drv_soft_ctrl *soft_ctrl = soft_get_ctrl();
    if (soft_ctrl == NULL) {
        soft_drv_err("soft_fault is NULL.\n");
        return;
    }

    for (i = 0; i < DEVICE_NUM_MAX; i++) {
        ka_task_mutex_lock(&soft_ctrl->mutex[i]);
        client = soft_ctrl->s_dev_t[i][SF_SENSOR_DAVINCI];
        soft_free_one_node(client, DMS_DEV_TYPE_BASE_SERVCIE);
        soft_ctrl->user_num[i]--;
        ka_task_mutex_unlock(&soft_ctrl->mutex[i]);
    }

    return;
}

STATIC int heart_beat_urgent_judge(u32 dev_id)
{
    int ret;
    struct hb_read_block *heartbeat_info = get_heart_beat_read_item(dev_id);
    struct devdrv_manager_info *manager_info = devdrv_get_manager_info();
    struct devdrv_info *dev_info = devdrv_manager_get_devdrv_info(dev_id);
    static unsigned int rao_read_fail_count[ASCEND_PDEV_MAX_NUM] = {0};

    if (dev_info == NULL || manager_info == NULL || heartbeat_info == NULL ||
        heartbeat_info->hb_stutas != HEART_BEAT_READY) {
        return DRV_ERROR_NONE;
    }

    ret = devdrv_try_get_dev_info_occupy(dev_info);
    if (ret != 0) {
        dms_err("Get dev_info occupy failed. (ret=%d; dev_id=%u)\n", ret, dev_id);
        return ret;
    }

    if (dev_info->shm_vaddr == NULL) {
        devdrv_put_dev_info_occupy(dev_info);
        dms_err("Shm vaddr is NULL. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    ret = devdrv_rao_read(dev_id, DEVDRV_RAO_CLIENT_DEVMNG, dev_info->shm_head->head_info.offset_heartbeat,
                          sizeof(U_SHM_INFO_HEARTBEAT));
    if (ret != 0) {
        devdrv_put_dev_info_occupy(dev_info);
        rao_read_fail_count[dev_id]++;
        dms_warn("Can not read rao data. (dev_id=%u; ret=%d; fail_cnt=%u)\n", dev_id, ret, rao_read_fail_count[dev_id]);
        return DRV_ERROR_NONE;
    } else {
        rao_read_fail_count[dev_id] = 0;
    }

    if ((dev_info->shm_heartbeat->magic == DEVMNG_HEART_BEAT_MAGIC) &&
        (dev_info->shm_heartbeat->heartbeat_lost_flag == DEVMNG_HEART_BEAT_LOST)) {
        devdrv_put_dev_info_occupy(dev_info);
        ret = DRV_ERROR_NONE;
        goto HEARTBEAT_LOST;
    }

    devdrv_put_dev_info_occupy(dev_info);
    return DRV_ERROR_NONE;

HEARTBEAT_LOST:
    dms_err("The device urgent heartbeat is lost! (device=%u) \n", dev_id);
    manager_info->device_status[dev_id] = DRV_STATUS_COMMUNICATION_LOST;
    hb_read_item_work_stop(dev_id, heartbeat_info);
    ka_task_queue_work(heartbeat_info->hb_lost_wq, &heartbeat_info->hb_lost_work);
    return ret;
}

STATIC int heart_beat_urgent_event(u64 dev_id)
{
    int ret;

    /* check device heartbeat */
    ret = heart_beat_urgent_judge((u32)dev_id);
    if (ret != 0) {
        dms_err("The device does not exist! (device=%llu) \n", dev_id);
        return ret;
    }

    return DRV_ERROR_NONE;
}

int heart_beat_register_urgent_timer(unsigned int dev_id)
{
    int ret;
    u32 timer_urgent_task_id = 0;
    struct dms_timer_task heart_beat_task = {0};
    struct hb_read_block *heartbeat_info = get_heart_beat_read_item(dev_id);

    if (heartbeat_info == NULL) {
        return 0;
    }
    if ((!uda_is_pf_dev(dev_id)) || (devdrv_get_connect_protocol(dev_id) != CONNECT_PROTOCOL_UB)) {
        return 0;
    }

    heart_beat_task.expire_ms = HEART_BEAT_TIMER_URGENT_EXPIRE_MS;
    heart_beat_task.count_ms = 0;
    heart_beat_task.user_data = dev_id;
    heart_beat_task.handler_mode = HIGH_PRI_WORK;
    heart_beat_task.exec_task = heart_beat_urgent_event;
    ret = dms_timer_task_register(&heart_beat_task, &timer_urgent_task_id);
    if (ret != 0) {
        dms_err("Dms timer task register failed. (dev_id=%u; ret=%d) \n", dev_id, ret);
        return ret;
    }

    heartbeat_info->urgent_task_id = timer_urgent_task_id;
    heartbeat_info->timer_urgent_registered = 1;
    return 0;
}
#endif
void heart_beat_unregister_urgent_timer(u32 dev_id)
{
    struct hb_read_block *heartbeat_info = get_heart_beat_read_item(dev_id);

    if (heartbeat_info == NULL) {
        return;
    }
    if (heartbeat_info->timer_urgent_registered != 1) {
        return;
    }

    (void)dms_timer_task_unregister(heartbeat_info->urgent_task_id);
    heartbeat_info->timer_urgent_registered = 0;
    return;
}
