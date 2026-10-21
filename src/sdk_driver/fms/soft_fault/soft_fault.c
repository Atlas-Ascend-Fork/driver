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

#ifndef CFG_EDGE_HOST
#include <linux/profile.h>
#endif

#include "comm_kernel_interface.h"
#include "devdrv_common.h"
#include "dms_template.h"
#include "urd_feature.h"
#include "dms/dms_cmd_def.h"
#include "urd_acc_ctrl.h"
#include "dms/dms_notifier.h"
#include "dms_define.h"
#include "pbl_mem_alloc_interface.h"
#include "soft_fault_define.h"
#include "urd_notifier.h"
#include "urd_init.h"
#include "pbl/pbl_davinci_api.h"
#if (!defined CFG_EDGE_HOST) && (defined CFG_FEATURE_OS_INIT_EVENT)
#include "os_reset.h"
#endif
#include "drv_kernel_soft.h"
#include "pbl_feature_loader.h"
#include "ka_dfx_pub.h"
#include "ka_system_pub.h"
#include "ka_list_pub.h"
#include "ka_memory_pub.h"
#include "ka_task_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_pci_pub.h"
#include "ka_common_pub.h"

#define EVENT_EXIST 1
#ifdef CFG_EDGE_HOST
STATIC const ka_pci_device_id_t soft_fault_driver_tbl[] = {
    {KA_PCI_VDEVICE(HUAWEI, 0xd100), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd105), 0},
    {KA_PCI_VDEVICE(HUAWEI, PCI_DEVICE_CLOUD), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd801), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd500), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd501), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd802), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd803), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd804), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd805), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd806), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd807), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd808), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd809), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd200), 0},
    {DEVDRV_DIVERSITY_PCIE_VENDOR_ID, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {0x20C6, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {0x203F, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {0x20E9, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {0x20C6, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {0x203F, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {0x20E9, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {}};
KA_MODULE_DEVICE_TABLE(pci, soft_fault_driver_tbl);
#endif

STATIC struct dms_node_operations g_soft_ops = {.init = soft_ops_init,
                                                .uninit = soft_ops_exit,
                                                .get_info_list = soft_get_dev_info_list,
                                                .get_state = soft_get_dev_state,
                                                .get_capacity = soft_get_dev_capacity,
                                                .set_power_state = soft_set_dev_power_state,
                                                .scan = NULL,
                                                .fault_diag = NULL,
                                                .event_notify = NULL,
                                                .get_link_state = NULL,
                                                .set_link_state = NULL};

STATIC struct drv_soft_ctrl g_soft_ctrl;

STATIC void soft_kfree(struct soft_error_list **ptr)
{
    if (*ptr != NULL) {
        dbl_kfree(*ptr);
        *ptr = NULL;
    }
}

struct dms_node_operations *soft_get_ops(void)
{
    return &g_soft_ops;
}
KA_EXPORT_SYMBOL(soft_get_ops);

struct drv_soft_ctrl *soft_get_ctrl(void)
{
    return &g_soft_ctrl;
}
KA_EXPORT_SYMBOL(soft_get_ctrl);

int soft_get_dev_capacity(struct dms_node *device, unsigned long long *capacity)
{
    if ((device == NULL) || (capacity == NULL)) {
        soft_drv_err("invalid para. (device=%d; capacity=%d)\n", (device == NULL), (capacity == NULL));
        return -EINVAL;
    }

    *capacity = device->capacity;

    return 0;
}

int soft_get_dev_info_list(struct dms_node *device, struct dms_dev_data_attr *info_list)
{
    return 0;
}

int soft_get_dev_state(struct dms_node *device, unsigned int *state)
{
    if ((device == NULL) || (state == NULL)) {
        soft_drv_err("invalid para, (device=%d; state=%d)\n", (device == NULL), (state == NULL));
        return -EINVAL;
    }

    *state = device->state;

    return 0;
}

int soft_set_dev_power_state(struct dms_node *device, DSMI_POWER_STATE power_state)
{
    return 0;
}

int soft_ops_init(struct dms_node *device)
{
    return 0;
}

void soft_ops_exit(struct dms_node *device)
{
    return;
}

u64 soft_combine_private_data(unsigned int dev_id, unsigned int user_id, unsigned int node_type, unsigned int node_id,
                              unsigned int sensor_id)
{
    u64 private_data;

    private_data = (((u64)(dev_id & SF_MASK_16BIT) << SF_OFFSET_48BIT) |
                    ((u64)(user_id & SF_MASK_16BIT) << SF_OFFSET_32BIT) |
                    ((u64)(node_type & SF_MASK_16BIT) << SF_OFFSET_16BIT) |
                    ((u64)(node_id & SF_MASK_8BIT) << SF_OFFSET_8BIT) | ((u64)(sensor_id & SF_MASK_8BIT)));
    return private_data;
}
KA_EXPORT_SYMBOL(soft_combine_private_data);

STATIC int soft_add_fault_event(struct soft_error_list *error_new, struct soft_event *event_queue)
{
    struct soft_error_list *pos = NULL;
    struct soft_error_list *n = NULL;

    if ((event_queue == NULL) || (error_new == NULL)) {
        soft_drv_err("invalid para. event_queue or error_new is NULL\n");
        return -EINVAL;
    }

    ka_task_mutex_lock(&event_queue->mutex);
    ka_list_for_each_entry_safe(pos, n, &event_queue->error_list.list, list)
    {
        if ((pos->error.sensor_type == error_new->error.sensor_type) &&
            (pos->error.err_type == error_new->error.err_type)) {
            if (pos->report_status == EVENT_REPORT_RECOVERED) {
                pos->report_status = EVENT_REPORT_INIT;
            }
            ka_task_mutex_unlock(&event_queue->mutex);
            return EVENT_EXIST; /* It means found fault event already in event list, don't need add */
        }
    }
    ka_list_add(&error_new->list, &event_queue->error_list.list); /* add new event to queue */
    event_queue->error_num++;
    error_new->report_status = EVENT_REPORT_INIT;
    ka_task_mutex_unlock(&event_queue->mutex);

    return 0;
}

/*
 * true means the queued event should be removed when the corresponding processing stage arrives.
 * For example, EVENT_REPORT_RECOVERED + EVENT_REPORT_SCANNED is true because recover arrives before
 * the first scan, so the event should be reported once in scan and then removed from the queue.
 */
STATIC const bool err_report_status_table[EVENT_REPORT_STATUS_MAX][EVENT_REPORT_STATUS_MAX] = {
    /* row: report_status           col: process_status */
    /* ------------------------------------------------ */
    /*                              EVENT_REPORT_INIT   EVENT_REPORT_SCANNED EVENT_REPORT_RECOVERED */
    /* EVENT_REPORT_INIT */ {false, false, false},
    /* EVENT_REPORT_SCANNED */ {false, false, true},
    /* EVENT_REPORT_RECOVERED */ {false, true, false},
};

STATIC bool check_error_report_status(EVENT_REPORT_STATUS err_status, EVENT_REPORT_STATUS process_status)
{
    if (err_status >= EVENT_REPORT_STATUS_MAX || process_status >= EVENT_REPORT_STATUS_MAX) {
        return false;
    }

    return err_report_status_table[err_status][process_status];
}

STATIC bool is_soft_fault_recover(struct soft_event *event_queue, struct soft_fault *event_new)
{
    bool recover_flag = false;
    struct soft_error_list *pos = NULL;
    struct soft_error_list *n = NULL;
    struct soft_fault *event = NULL;

    ka_task_mutex_lock(&event_queue->mutex);
    ka_list_for_each_entry_safe(pos, n, &event_queue->error_list.list, list)
    {
        event = &pos->error;
        if ((event->err_type == event_new->err_type) && (event->assertion == GENERAL_EVENT_TYPE_OCCUR)) {
            if (check_error_report_status(pos->report_status, EVENT_REPORT_RECOVERED)) {
                ka_list_del(&pos->list);
                dbl_kfree(pos);
                pos = NULL;
                event_queue->error_num--;
            } else {
                pos->report_status = EVENT_REPORT_RECOVERED;
            }
            recover_flag = true;
        }
    }
    ka_task_mutex_unlock(&event_queue->mutex);

    return recover_flag;
}

STATIC struct soft_event *soft_event_queue_get(struct soft_dev_client *client, unsigned int node_type,
                                               unsigned int node_id, unsigned int idx)
{
    struct soft_dev *pos = NULL;
    struct soft_dev *n = NULL;
    struct soft_event *event_queue = NULL;

    ka_list_for_each_entry_safe(pos, n, &client->head, list)
    {
        if (pos->dev_node.node_type == node_type && pos->node_id == node_id) {
            event_queue = &pos->sensor_event_queue[idx];
            break;
        }
    }

    return event_queue;
}

int soft_fault_event_scan(unsigned long long private_data, struct dms_sensor_event_data *data)
{
    int ret;
    unsigned int dev_id, user_id, node_type, node_id, idx;
    struct soft_dev_client *client = NULL;
    struct soft_error_list *pos = NULL;
    struct soft_error_list *n = NULL;
    struct soft_event *event_queue = NULL;
    struct soft_fault *event = NULL;
    struct drv_soft_ctrl *soft_ctrl = soft_get_ctrl();

    dev_id = private_data >> SF_OFFSET_48BIT;
    user_id = (private_data & SF_MASK_48BIT) >> SF_OFFSET_32BIT;
    node_type = (private_data & SF_MASK_32BIT) >> SF_OFFSET_16BIT;
    node_id = (private_data & SF_MASK_16BIT) >> SF_OFFSET_8BIT;
    idx = private_data & SF_MASK_8BIT;

    if ((data == NULL) || (dev_id >= ASCEND_DEV_MAX_NUM) || (user_id >= SF_USER_MAX) || (idx >= SF_SUB_ID_MAX)) {
        soft_drv_err("invalid para. (dev_id=%u; s_id=%u; node_type=0x%x; idx=%u)\n", dev_id, user_id, node_type, idx);
        return -EINVAL;
    }

    client = soft_ctrl->s_dev_t[dev_id][user_id];
    if (client == NULL) {
        soft_drv_err("client is null. (dev_id=%u; user_id=%u)\n", dev_id, user_id);
        return -EINVAL;
    }

    event_queue = soft_event_queue_get(client, node_type, node_id, idx);
    if (event_queue == NULL) {
        soft_drv_err("get event_queue failed. (dev_id=%u; node_id=%u; idx=%u)\n", dev_id, node_id, idx);
        return -EINVAL;
    }

    data->event_count = 0;
    ka_task_mutex_lock(&event_queue->mutex);
    /* get every event of the sensor event queue */
    ka_list_for_each_entry_safe(pos, n, &event_queue->error_list.list, list)
    {
        event = &pos->error;
        data->sensor_data[data->event_count].current_value = event->err_type;
        data->sensor_data[data->event_count].data_size = event->data_len;
        ret = memcpy_s(data->sensor_data[data->event_count].event_data, DMS_MAX_EVENT_DATA_LENGTH, event->data,
                       event->data_len);
        if (ret != 0) {
            soft_drv_warn("memcpy_s failed, continue. (ret=%d)\n", ret);
            continue;
        }

        if ((event->assertion == GENERAL_EVENT_TYPE_ONE_TIME) ||
            check_error_report_status(pos->report_status, EVENT_REPORT_SCANNED)) { /*  notification_event check. */
            ka_list_del(&pos->list);
            dbl_kfree(pos);
            pos = NULL;
            event_queue->error_num--;
        } else {
            pos->report_status = EVENT_REPORT_SCANNED;
        }
        data->event_count++;
        if (data->event_count == DMS_MAX_SENSOR_EVENT_COUNT) {
            break;
        }
    }
    ka_task_mutex_unlock(&event_queue->mutex);

    return 0;
}
KA_EXPORT_SYMBOL(soft_fault_event_scan);

static int soft_add_new_fault_event(struct soft_fault *event, struct soft_event *event_queue)
{
    int ret;
    struct soft_error_list *error_new = NULL;

    error_new = dbl_kzalloc(sizeof(struct soft_error_list), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (error_new == NULL) {
        soft_drv_err("new_event ka_mm_kzalloc failed.\n");
        return -ENOMEM;
    }

    ret = memcpy_s(&error_new->error, sizeof(struct soft_fault), event, sizeof(struct soft_fault));
    if (ret != 0) {
        soft_drv_err("new_event memcpy_s failed. (ret=%d)\n", ret);
        goto out;
    }

    ret = soft_add_fault_event(error_new, event_queue);
    if (ret != 0) {
        soft_kfree(&error_new);
        return ret;
    }

    return ret;
out:
    soft_kfree(&error_new);
    soft_drv_err("fault event handler error. (dev_id=%u; user_id=%u; err_type=0x%x)\n", event->dev_id, event->user_id,
                 event->err_type);
    return ret;
}

int soft_fault_event_handler(struct soft_fault *event)
{
    struct soft_dev_client *client = NULL;
    struct soft_event *event_queue = NULL;
    struct drv_soft_ctrl *soft_ctrl = soft_get_ctrl();

    if (event == NULL) {
        soft_drv_err("invalid para, event is NULL.\n");
        return -EINVAL;
    }

    if ((event->dev_id >= ASCEND_DEV_MAX_NUM) || (event->user_id >= SF_USER_MAX) || (event->sub_id >= SF_SUB_ID_MAX) ||
        (event->data_len > DMS_MAX_EVENT_DATA_LENGTH)) {
        soft_drv_err("invalid para. (dev_id=%u; user_id=%u; node_type=0x%x; idx=%u; data_len=%u)\n", event->dev_id,
                     event->user_id, event->node_type, event->sub_id, event->data_len);
        return -EINVAL;
    }

    client = soft_ctrl->s_dev_t[event->dev_id][event->user_id];
    if (client == NULL) {
        soft_drv_err("client is null. (dev_id=%u; user_id=%u)\n", event->dev_id, event->user_id);
        return -EINVAL;
    }

    event_queue = soft_event_queue_get(client, event->node_type, event->node_id, event->sub_id);
    if (event_queue == NULL) {
        soft_drv_err("get event_queue failed. (dev_id=%u; node_type=0x%x; node_id=%u; idx=%u)\n", event->dev_id,
                     event->node_type, event->node_id, event->sub_id);
        return -EINVAL;
    }

    if (event->assertion == GENERAL_EVENT_TYPE_RESUME) {
        if (is_soft_fault_recover(event_queue, event)) {
            soft_drv_event("fault recover. "
                           "(dev_id=%u; user_id=%u; assertion=%u; node_type=0x%x; sensor_type=0x%x; err_type=0x%x)\n",
                           event->dev_id, event->user_id, event->assertion, event->node_type, event->sensor_type,
                           event->err_type);
            return 0;
        }
        soft_drv_warn("Not found existing err_type or assertion. "
                      "(dev_id=%u; user_id=%u; assertion=%u; node_type=0x%x; sensor_type=0x%x; err_type=0x%x)\n",
                      event->dev_id, event->user_id, event->assertion, event->node_type, event->sensor_type,
                      event->err_type);
        return -EINVAL;
    }

    return soft_add_new_fault_event(event, event_queue);
}
KA_EXPORT_SYMBOL(soft_fault_event_handler);

void soft_fault_event_free(struct soft_event *event_queue)
{
    struct soft_error_list *pos = NULL;
    struct soft_error_list *n = NULL;

    if (event_queue == NULL) {
        soft_drv_warn("Invalid para, event_queue is NULL\n");
        return;
    }

    ka_task_mutex_lock(&event_queue->mutex);
    ka_list_for_each_entry_safe(pos, n, &event_queue->error_list.list, list)
    {
        ka_list_del(&pos->list);
        dbl_kfree(pos);
        pos = NULL;
        event_queue->error_num--;
    }
    ka_task_mutex_unlock(&event_queue->mutex);
}

int soft_register_one_node(struct soft_dev *s_dev)
{
    int ret;
    unsigned int i;

    if (s_dev == NULL) {
        soft_drv_err("Invalid para, h_dev is NULL.\n");
        return -EINVAL;
    }

    if (s_dev->registered == 1) {
        soft_drv_warn("soft_dev is registered. (dev_id=%u; registered=%u)\n", s_dev->dev_id, s_dev->registered);
        return 0;
    }

    ret = dms_register_dev_node(&s_dev->dev_node);
    if (ret != 0) {
        soft_drv_err("soft_fault register dev_node failed. (ret=%d)\n", ret);
        return ret;
    }

    for (i = 0; i < s_dev->sensor_obj_num; i++) {
        ret = dms_sensor_register(&s_dev->dev_node, &s_dev->sensor_obj_table[i]);
        if (ret != 0) {
            soft_drv_err("soft_fault register sensor failed. (s_idx=%u; ret=%d)\n", i, ret);
            goto out;
        }
        s_dev->sensor_obj_registered[i] = 1;
    }

    s_dev->registered = 1;
    return 0;
out:
    if (i > 0) {
        (void)dms_sensor_node_unregister(&s_dev->dev_node);
    }
    (void)dms_unregister_dev_node(&s_dev->dev_node);

    return ret;
}
KA_EXPORT_SYMBOL(soft_register_one_node);

void soft_unregister_one_node(struct soft_dev *s_dev)
{
    int ret;

    if (s_dev == NULL) {
        soft_drv_err("Invalid para, h_dev is NULL.\n");
        return;
    }

    if (s_dev->registered == 0) {
        return;
    }

    ret = dms_sensor_node_unregister(&s_dev->dev_node);
    if (ret != 0) {
        soft_drv_warn("soft_fault unregister sensor node failed. (ret=%d)\n", ret);
    }

    ret = dms_unregister_dev_node(&s_dev->dev_node);
    if (ret != 0) {
        soft_drv_warn("soft_fault unregister dev node failed. (ret=%d)\n", ret);
    }

    s_dev->registered = 0;
    return;
}

void soft_free_one_node(struct soft_dev_client *client, unsigned int node_type)
{
    unsigned int i;
    struct soft_dev *pos = NULL;
    struct soft_dev *n = NULL;

    if (client == NULL) {
        soft_drv_warn("invalid para, client is NULL.\n");
        return;
    }

    ka_task_mutex_lock(&client->mutex);
    ka_list_for_each_entry_safe(pos, n, &client->head, list)
    {
        if ((pos->dev_node.node_type == node_type) && (pos->registered == 1)) {
            soft_unregister_one_node(pos);
            for (i = 0; i < SF_SUB_ID_MAX; i++) {
                if (pos->sensor_obj_registered[i] == 1) {
                    soft_fault_event_free(&pos->sensor_event_queue[i]);
                }
            }
            soft_one_dev_exit(pos);
            ka_list_del(&pos->list);
            dbl_kfree(pos);
            pos = NULL;
            client->node_num--;
            break;
        }
    }

    if (client->node_num == 0) {
        client->pid = -1;
        client->user_id = 0;
        client->registered = 0;
    }

    ka_task_mutex_unlock(&client->mutex);
    return;
}
KA_EXPORT_SYMBOL(soft_free_one_node);

STATIC int soft_h2d_event(void *data)
{
#if (!defined CFG_EDGE_HOST) && (defined CFG_FEATURE_OS_INIT_EVENT) /* device side */
    int ret;
    u32 dev_id;
    dev_id = *(u32 *)data;
    ret = os_device_notifier_func(dev_id);
    if (ret != 0) {
        soft_drv_err("Soc dev notify failed. (dev_id=%u; ret=%d) \n", dev_id, ret);
        return ret;
    }
#endif

    return 0;
}

STATIC int soft_notifier(ka_notifier_block_t *nb, unsigned long mode, void *data)
{
    int ret = 0;

    if (data == NULL) {
        soft_drv_err("Data is null, invalid parameter. \n");
        return -EINVAL;
    }
    switch (mode) {
        case DMS_H2D_EVENT:
            ret = soft_h2d_event(data);
            break;
        default:
            break;
    }

    return 0;
}

STATIC ka_notifier_block_t g_soft_notifier = {
    .notifier_call = soft_notifier,
};

STATIC void soft_fault_release_prepare(ka_pid_t owner_pid)
{
    soft_client_release(owner_pid);
    if (dms_event_is_converge()) {
        dms_event_cb_release(owner_pid);
    }
    return;
}

STATIC int soft_exit_notifier(ka_notifier_block_t *nb, unsigned long mode, void *data)
{
    ka_task_struct_t *task = (ka_task_struct_t *)data;
    (void)nb;
    (void)mode;
    /* This code ensures that soft_fault resource can be released when the process exit abnormally */
    if ((task != NULL) && ka_task_judge_mm(task) && (task->tgid != 0) && (task->tgid == task->pid) &&
        ascend_intf_is_pid_init(task->tgid, DAVINCI_INTF_MODULE_DMS)) {
        /* Only the process open davinci device is check. */
        soft_fault_release_prepare(task->tgid);
    }

    return 0;
}

STATIC ka_notifier_block_t g_soft_exit_notifier = {
    .notifier_call = soft_exit_notifier,
};

STATIC int soft_urd_notifier(ka_notifier_block_t *nb, unsigned long mode, void *data)
{
    (void)nb;
    (void)data;
    switch (mode) {
        case URD_NOTIFIER_RELEASE_PREPARE:
            /* This code ensures that soft_fault resource can be released when the process exit normally */
            soft_fault_release_prepare(ka_task_get_current_tgid());
            break;
        default:
            break;
    }

    return 0;
}

STATIC ka_notifier_block_t g_soft_urd_notifier = {
    .notifier_call = soft_urd_notifier,
};

void soft_one_dev_init(struct soft_dev *s_dev)
{
    int i;

    if (s_dev == NULL) {
        return;
    }

    s_dev->registered = 0;
    s_dev->sensor_obj_num = 0;
    ka_task_mutex_init(&s_dev->mutex);

    for (i = 0; i < SF_SUB_ID_MAX; i++) {
        ka_task_mutex_init(&s_dev->sensor_event_queue[i].mutex);
        KA_INIT_LIST_HEAD(&s_dev->sensor_event_queue[i].error_list.list);
    }

    return;
}
KA_EXPORT_SYMBOL(soft_one_dev_init);

void soft_one_dev_exit(struct soft_dev *s_dev)
{
    int i;

    if (s_dev == NULL) {
        return;
    }

    for (i = 0; i < SF_SUB_ID_MAX; i++) {
        ka_task_mutex_destroy(&s_dev->sensor_event_queue[i].mutex);
    }

    ka_task_mutex_destroy(&s_dev->mutex);
    s_dev->registered = 0;
    s_dev->sensor_obj_num = 0;

    return;
}

STATIC int soft_ctrl_init(void)
{
    int i, j;
    struct drv_soft_ctrl *soft_ctrl = soft_get_ctrl();

    for (i = 0; i < ASCEND_DEV_MAX_NUM; i++) {
        soft_ctrl->user_num[i] = 1;
        ka_task_mutex_init(&soft_ctrl->mutex[i]);
        for (j = 0; j < SF_USER_MAX; j++) {
            soft_ctrl->s_dev_t[i][j] = dbl_kzalloc(sizeof(struct soft_dev_client), KA_GFP_KERNEL);
            if (soft_ctrl->s_dev_t[i][j] == NULL) {
                soft_drv_err("Kzalloc s_dev_t failed. \n");
                return -ENOMEM;
            }

            soft_ctrl->s_dev_t[i][j]->pid = -1;
            soft_ctrl->s_dev_t[i][j]->registered = 0;
            soft_ctrl->s_dev_t[i][j]->node_num = 0;
            ka_task_mutex_init(&soft_ctrl->s_dev_t[i][j]->mutex);
            KA_INIT_LIST_HEAD(&soft_ctrl->s_dev_t[i][j]->head);
        }
    }

    return 0;
}

STATIC void soft_ctrl_exit(void)
{
    int i, j;
    struct drv_soft_ctrl *soft_ctrl = soft_get_ctrl();

    for (i = 0; i < ASCEND_DEV_MAX_NUM; i++) {
        for (j = 0; j < SF_USER_MAX; j++) {
            if (soft_ctrl->s_dev_t[i][j] != NULL) {
                ka_task_mutex_destroy(&soft_ctrl->s_dev_t[i][j]->mutex);
                soft_ctrl->s_dev_t[i][j]->node_num = 0;
                soft_ctrl->s_dev_t[i][j]->registered = 0;
                soft_ctrl->s_dev_t[i][j]->pid = -1;
                dbl_kfree(soft_ctrl->s_dev_t[i][j]);
                soft_ctrl->s_dev_t[i][j] = NULL;
            }
        }
        ka_task_mutex_destroy(&soft_ctrl->mutex[i]);
        soft_ctrl->user_num[i] = 0;
    }

    return;
}

int soft_init(void)
{
    int ret;

    CALL_INIT_MODULE(soft_fault);
    ret = soft_ctrl_init();
    if (ret != 0) {
        soft_drv_err("Soft ctrl init failed. (ret=%d)\n", ret);
        goto SOFT_CTRL_FAIL;
    }

    ret = ka_dfx_profile_event_register(KA_PROFILE_TASK_EXIT, &g_soft_exit_notifier);
    if (ret != 0) {
        soft_drv_err("register profile notifier failed. (ret=%d)\n", ret);
        goto SOFT_CTRL_FAIL;
    }

    /* Register callback to the URD. When a process exits, the DMS node,
     * sensor, and aggregated fault data related to the process need
     * to be released. */
    ret = urd_register_notifier(&g_soft_urd_notifier);
    if (ret != 0) {
        soft_drv_err("register urd notifier failed. (ret=%d)\n", ret);
        goto URD_REGISTER_FAIL;
    }

    ret = dms_register_notifier(&g_soft_notifier);
    if (ret != 0) {
        soft_drv_err("register dms notifier failed. (ret=%d)\n", ret);
        goto DMS_REGISTER_FAIL;
    }

    soft_drv_info("soft event driver init success.\n");
    return 0;

DMS_REGISTER_FAIL:
    (void)urd_unregister_notifier(&g_soft_urd_notifier);
URD_REGISTER_FAIL:
    (void)ka_dfx_profile_event_unregister(KA_PROFILE_TASK_EXIT, &g_soft_exit_notifier);
SOFT_CTRL_FAIL:
    soft_ctrl_exit();
    CALL_EXIT_MODULE(soft_fault);
    return ret;
}
DECLAER_FEATURE_AUTO_INIT(soft_init, FEATURE_LOADER_STAGE_5);

void soft_uninit(void)
{
    (void)dms_unregister_notifier(&g_soft_notifier);
#if (!defined CFG_EDGE_HOST) && (defined CFG_FEATURE_OS_INIT_EVENT) /* device side */
    os_dev_unregister();
#endif
#ifdef CFG_FEATURE_DRV_KERNEL_SOFT_EVENT
    drv_kernel_soft_fault_unregister();
#endif
    (void)urd_unregister_notifier(&g_soft_urd_notifier);
    (void)ka_dfx_profile_event_unregister(KA_PROFILE_TASK_EXIT, &g_soft_exit_notifier);
    soft_dev_exit();
    soft_ctrl_exit();
    CALL_EXIT_MODULE(soft_fault);
    soft_drv_info("soft event driver exit success.\n");
}
DECLAER_FEATURE_AUTO_UNINIT(soft_uninit, FEATURE_LOADER_STAGE_5);
