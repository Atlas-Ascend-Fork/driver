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
#include "ka_common_pub.h"
#include "ka_system_pub.h"

#include "devdrv_manager.h"
#include "devdrv_manager_container.h"
#include "pbl_mem_alloc_interface.h"

#include "dms_define.h"
#include "dms_event.h"
#include "dms_event_distribute.h"
#include "dms_event_converge.h"
#include "devdrv_black_box.h"
#include "hvdevmng_init.h"
#include "dms/dms_interface.h"
#include "ka_task_pub.h"
#include "ka_list_pub.h"
#include "ka_memory_pub.h"
#include "ka_errno_pub.h"
#include "dms_event_host.h"

static ka_mutex_t g_hvdms_subscribe_status_mutex;
static u32 g_hvdms_subscribe_status_bitmap[ASCEND_DEV_MAX_NUM] = {0};
ka_task_struct_t *g_event_save_task;

void devdrv_device_black_box_add_exception(u32 devid, u32 code)
{}
int dms_event_box_add_exception(u32 devid, u32 code, ka_timespec_t stamp)
{
    return devdrv_host_black_box_add_exception(devid, code, stamp, NULL);
}

int dms_event_subscribe_from_host(u32 devid, void *msg, u32 in_len, u32 *ack_len)
{
    return DRV_ERROR_NOT_SUPPORT;
}

int dms_event_distribute_to_bar(u32 phyid)
{
    /* not need to fresh event code to bar in host */
    return DRV_ERROR_NONE;
}

int dms_distribute_all_devices_event_to_bar(void)
{
    /* no need to fresh event code to bar in host */
    return DRV_ERROR_NONE;
}

int dms_event_subscribe_from_device(u32 phyid)
{
    struct devdrv_manager_msg_info dev_manager_msg_info = {{0}};
    void *no_trans_chan = NULL;
    u32 out_len;
    int ret;

    dev_manager_msg_info.header.msg_id = DEVDRV_MANAGER_CHAN_H2D_DMS_EVENT_SUBSCRIBE;
    dev_manager_msg_info.header.valid = DEVDRV_MANAGER_MSG_H2D_MAGIC;
    dev_manager_msg_info.header.result = DEVDRV_MANAGER_MSG_INVALID_RESULT;

    no_trans_chan = devdrv_manager_get_no_trans_chan(phyid);
    if (no_trans_chan == NULL) {
        dms_err("Failed to get no_trans_chan. (phy_id=%u)\n", phyid);
        return DRV_ERROR_NOT_EXIST;
    }

    dev_manager_msg_info.header.dev_id = phyid;
    ret = devdrv_sync_msg_send(no_trans_chan, &dev_manager_msg_info, sizeof(dev_manager_msg_info),
                               sizeof(dev_manager_msg_info), &out_len);
    if (ret || (dev_manager_msg_info.header.result == DEVDRV_MANAGER_MSG_INVALID_RESULT) ||
        (dev_manager_msg_info.header.valid != DEVDRV_MANAGER_MSG_H2D_MAGIC)) {
        dms_err("Send message to device failed. (dev_id=%u; result=%u; valid=0x%x; ret=%d)\n", phyid,
                dev_manager_msg_info.header.result, dev_manager_msg_info.header.valid, ret);
        return DRV_ERROR_INNER_ERR;
    }

    dms_info("Subscribe from device success. (phy_id=%u)\n", phyid);
    return DRV_ERROR_NONE;
}

int dms_event_clean_to_device(u32 phyid)
{
    struct devdrv_manager_msg_info dev_manager_msg_info = {{0}};
    void *no_trans_chan = NULL;
    u32 out_len;
    int ret;

    dev_manager_msg_info.header.msg_id = DEVDRV_MANAGER_CHAN_H2D_DMS_EVENT_CLEAN;
    dev_manager_msg_info.header.valid = DEVDRV_MANAGER_MSG_H2D_MAGIC;
    dev_manager_msg_info.header.result = DEVDRV_MANAGER_MSG_INVALID_RESULT;

    no_trans_chan = devdrv_manager_get_no_trans_chan(phyid);
    if (no_trans_chan == NULL) {
        dms_err("Failed to get trans. (phy_id=%u)\n", phyid);
        return DRV_ERROR_NOT_EXIST;
    }

    dev_manager_msg_info.header.dev_id = phyid;
    ret = devdrv_sync_msg_send(no_trans_chan, &dev_manager_msg_info, sizeof(dev_manager_msg_info),
                               sizeof(dev_manager_msg_info), &out_len);
    if (ret || (dev_manager_msg_info.header.result == DEVDRV_MANAGER_MSG_INVALID_RESULT) ||
        (dev_manager_msg_info.header.valid != DEVDRV_MANAGER_MSG_H2D_MAGIC)) {
        dms_err("Send message to device failed. (phy_id=%u; result=%u; valid=0x%x; ret=%d)\n", phyid,
                dev_manager_msg_info.header.result, dev_manager_msg_info.header.valid, ret);
        return DRV_ERROR_INNER_ERR;
    }
    dms_event("Clean event to device success. (phy_id=%u)\n", phyid);

    return DRV_ERROR_NONE;
}

STATIC int dms_event_mask_to_device(u32 phyid, u32 event_code, u8 mask)
{
    struct devdrv_manager_msg_info dev_manager_msg_info = {{0}};
    void *no_trans_chan = NULL;
    u32 *data_load = NULL;
    u32 out_len;
    int ret;

    dev_manager_msg_info.header.msg_id = DEVDRV_MANAGER_CHAN_H2D_DMS_EVENT_MASK;
    dev_manager_msg_info.header.valid = DEVDRV_MANAGER_MSG_H2D_MAGIC;
    dev_manager_msg_info.header.result = DEVDRV_MANAGER_MSG_INVALID_RESULT;

    no_trans_chan = devdrv_manager_get_no_trans_chan(phyid);
    if (no_trans_chan == NULL) {
        dms_err("Failed to get trans. (phy_id=%u)\n", phyid);
        return DRV_ERROR_NOT_EXIST;
    }

    dev_manager_msg_info.header.dev_id = phyid;
    data_load = (u32 *)dev_manager_msg_info.payload;
    *data_load++ = event_code;
    *data_load = mask;
    ret = devdrv_sync_msg_send(no_trans_chan, &dev_manager_msg_info, sizeof(dev_manager_msg_info),
                               sizeof(dev_manager_msg_info), &out_len);
    if (ret || (dev_manager_msg_info.header.result == DEVDRV_MANAGER_MSG_INVALID_RESULT) ||
        (dev_manager_msg_info.header.valid != DEVDRV_MANAGER_MSG_H2D_MAGIC)) {
        dms_err("Send message to device failed. (phy_id=%u; result=%u; valid=0x%x; ret=%d)\n", phyid,
                dev_manager_msg_info.header.result, dev_manager_msg_info.header.valid, ret);
        return DRV_ERROR_INNER_ERR;
    }

    dms_event("Mask event to device success. (phyid=%u; event_code=0x%x; mask=%u)\n", phyid, event_code, mask);
    return DRV_ERROR_NONE;
}

int dms_event_mask_event_code(u32 phyid, u32 event_code, u8 mask)
{
    int ret;

    ret = dms_event_mask_by_phyid(phyid, event_code, mask);
    if (ret) {
        dms_err("Mask event code in host failed. (phyid=%u; event_code=0x%x; mask=%u; ret=%d)\n", phyid, event_code,
                mask, ret);
        return ret;
    }

    ret = dms_event_mask_to_device(phyid, event_code, mask);
    if (ret) {
        dms_err("Mask event code to device failed. (phyid=%u; event_code=0x%x; mask=%u; ret=%d)\n", phyid, event_code,
                mask, ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

int dms_event_get_exception_from_device(void *msg, u32 *ack_len)
{
    struct devdrv_manager_msg_info *dev_manager_msg_info = NULL;
    DMS_EVENT_NODE_STRU *exception_buf = NULL;
    int ret;

    dev_manager_msg_info = (struct devdrv_manager_msg_info *)msg;
    if ((dev_manager_msg_info->header.dev_id >= ASCEND_DEV_MAX_NUM) ||
        (dev_manager_msg_info->header.valid != DEVDRV_MANAGER_MSG_D2H_MAGIC)) {
        dms_err("Invalid message from device. (dev_id=%u; valid=%u)\n", dev_manager_msg_info->header.dev_id,
                dev_manager_msg_info->header.valid);
        return DRV_ERROR_INVALID_VALUE;
    }
    *ack_len = sizeof(*dev_manager_msg_info);

    exception_buf = (DMS_EVENT_NODE_STRU *)dev_manager_msg_info->payload;

    exception_buf->event.deviceid = (unsigned short)dev_manager_msg_info->header.dev_id;
    exception_buf->event.notify_serial_num = dms_event_get_notify_serial_num();

    ret = dms_event_distribute_handle(exception_buf, DMS_DISTRIBUTE_PRIORITY0);
    if (ret) {
        dms_err("Distribute handle failed. (dev_id=%u; ret=%d)\n", exception_buf->event.deviceid, ret);
        return ret;
    }
    dms_debug("Get event from device success. (phy_id=%u; event_id=0x%x)\n", dev_manager_msg_info->header.dev_id,
              exception_buf->event.event_id);

    dev_manager_msg_info->header.result = (u16)DEVDRV_MANAGER_MSG_VALID;
    return DRV_ERROR_NONE;
}

int dms_get_event_code_from_bar(u32 devid, u32 *health_code, u32 health_len, struct shm_event_code *event_code,
                                u32 event_len)
{
    int ret;

    ret = devmng_dms_get_event_code(devid, health_code, health_len, event_code, event_len);
    if (ret) {
        dms_err("Get event code from bar zone. (ret=%d)\n", ret);
        return DRV_ERROR_INNER_ERR;
    }

    return DRV_ERROR_NONE;
}

int dms_get_event_code_from_local(u32 devid, u32 *health_code, struct shm_event_code *event_code, u32 event_len)
{
    struct dms_device_event *device_fault_event = NULL;
    DMS_EVENT_NODE_STRU *pos = NULL;
    DMS_EVENT_NODE_STRU *n = NULL;
    unsigned int i = 0;

    device_fault_event = dms_get_device_event(devid);
    if (device_fault_event == NULL) {
        dms_err("device_fault_info is NULL. (dev_id=%u)\n", devid);
        return -EINVAL;
    }

    ka_task_mutex_lock(&device_fault_event->lock);
    *health_code = device_fault_event->highest_severity;
    ka_list_for_each_entry_safe(pos, n, &device_fault_event->head, node)
    {
        event_code[i].event_code = pos->event.event_code;
        i++;
        if (i >= event_len) {
            break;
        }
    }
    ka_task_mutex_unlock(&device_fault_event->lock);
    return 0;
}

int dms_get_health_code_from_bar(u32 devid, u32 *health_code, u32 health_len)
{
    int ret = devmng_dms_get_health_code(devid, health_code, health_len);
    if (ret != 0) {
        dms_err("Get health code from bar zone failed. (ret=%d)\n", ret);
        return DRV_ERROR_INNER_ERR;
    }

    return DRV_ERROR_NONE;
}

int dms_get_health_code_from_local(u32 devid, u32 *health_code)
{
    struct dms_device_event *device_fault_event = NULL;

    device_fault_event = dms_get_device_event(devid);
    if (device_fault_event == NULL) {
        dms_err("device_fault_info is NULL. (dev_id=%u)\n", devid);
        return -EINVAL;
    }

    ka_task_mutex_lock(&device_fault_event->lock);
    *health_code = device_fault_event->highest_severity;
    ka_task_mutex_unlock(&device_fault_event->lock);
    return 0;
}

STATIC int dms_get_list_event_para(int dev_id, struct dms_device_event *device_event, struct dms_event_para *dms_event,
                                   unsigned int in_cnt, unsigned int *event_num)
{
    int ret;
    unsigned int num = 0;
    DMS_EVENT_NODE_STRU *pos = NULL;
    DMS_EVENT_NODE_STRU *n = NULL;

    if (device_event == NULL) {
        dms_err("device_event is NULL. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_INNER_ERR;
    }

    ka_task_mutex_lock(&device_event->lock);
    ka_list_for_each_entry_safe(pos, n, &device_event->head, node)
    {
        ret = memcpy_s(&dms_event[num], sizeof(struct dms_event_para), &pos->event, sizeof(struct dms_event_para));
        if (ret != 0) {
            ka_task_mutex_unlock(&device_event->lock);
            dms_err("Call memcpy_s failed. (ret=%d)\n", ret);
            return DRV_ERROR_INNER_ERR;
        }

        num++;
        if (num >= in_cnt) {
            goto OUT;
        }
    }

OUT:
    ka_task_mutex_unlock(&device_event->lock);
    *event_num = num;
    return DRV_ERROR_NONE;
}

int dms_get_event_para(int dev_id, struct dms_event_para *dms_event, unsigned int in_cnt, unsigned int *event_num)
{
    return dms_get_list_event_para(dev_id, dms_get_device_event(dev_id), dms_event, in_cnt, event_num);
}

int dms_get_notify_event_para(int dev_id, struct dms_event_para *dms_event, unsigned int in_cnt,
                              unsigned int *event_num)
{
    return dms_get_list_event_para(dev_id, dms_get_device_notify_event(dev_id), dms_event, in_cnt, event_num);
}

STATIC bool is_fault_event_node_same(struct dms_event_para *event1, struct dms_event_para *event2)
{
    if ((event1->pid != event2->pid) || (event1->deviceid != event2->deviceid) ||
        (event1->event_id != event2->event_id) || (event1->node_type != event2->node_type) ||
        (event1->node_id != event2->node_id) || (event1->sub_node_type != event2->sub_node_type) ||
        (event1->sub_node_id != event2->sub_node_id) || (event1->sensor_num != event2->sensor_num) ||
        (event1->event_serial_num != event2->event_serial_num)) {
        return false;
    }

    return true;
}

STATIC DMS_EVENT_NODE_STRU *get_an_event_node_from_list(struct dms_device_event *event_ctrl,
                                                        struct dms_event_para *fault_event)
{
    DMS_EVENT_NODE_STRU *pos = NULL;
    DMS_EVENT_NODE_STRU *n = NULL;

    ka_list_for_each_entry_safe(pos, n, &event_ctrl->head, node)
    {
        if (is_fault_event_node_same(&pos->event, fault_event)) {
            return pos;
        }
    }

    return NULL;
}

STATIC void update_event_highest_severity(struct dms_device_event *event_ctrl, struct dms_event_para *fault_event)
{
    DMS_EVENT_NODE_STRU *pos = NULL;
    DMS_EVENT_NODE_STRU *n = NULL;

    if (fault_event->severity < event_ctrl->highest_severity) {
        return;
    }

    event_ctrl->highest_severity = 0;
    ka_list_for_each_entry_safe(pos, n, &event_ctrl->head, node)
    {
        event_ctrl->highest_severity = (pos->event.severity > event_ctrl->highest_severity) ?
                                           pos->event.severity :
                                           event_ctrl->highest_severity;
    }
}

STATIC int del_event_in_full_device_event(unsigned int dev_id, struct dms_device_event *device_event)
{
    DMS_EVENT_NODE_STRU *event_node = NULL;

    if (device_event->event_num < DMANAGE_ERROR_ARRAY_NUM) {
        return 0;
    }
    if (ka_list_empty(&device_event->head)) {
        dms_err("The device_event list is empty. (dev_id=%u; event_num=%d)\n", dev_id, device_event->event_num);
        return DRV_ERROR_INNER_ERR;
    }

    event_node = ka_list_last_entry(&device_event->head, DMS_EVENT_NODE_STRU, node);
    if (event_node == NULL) {
        dms_err("The list_last_entry event_node is NULL. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_INNER_ERR;
    }

    ka_list_del(&event_node->node);
    device_event->event_num--;
    update_event_highest_severity(device_event, &event_node->event);
    dbl_kfree(event_node);
    event_node = NULL;

    return 0;
}

STATIC int dms_add_event_in_local(struct dms_event_para *fault_event)
{
    int ret;
    unsigned int dev_id = fault_event->deviceid;
    struct dms_device_event *device_fault_event = NULL;
    DMS_EVENT_NODE_STRU *event_node = NULL;

    device_fault_event = dms_get_device_event(dev_id);
    if (device_fault_event == NULL) {
        dms_err("device_fault_info is NULL. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_PARA_ERROR;
    }

    ka_task_mutex_lock(&device_fault_event->lock);
    event_node = get_an_event_node_from_list(device_fault_event, fault_event);
    if (event_node != NULL) {
        ka_task_mutex_unlock(&device_fault_event->lock);
        /* the same event already exists in the chain; there is no need to add it again */
        return 0;
    }
    ret = del_event_in_full_device_event(dev_id, device_fault_event);
    if (ret != 0) {
        ka_task_mutex_unlock(&device_fault_event->lock);
        return ret;
    }

    event_node = (DMS_EVENT_NODE_STRU *)dbl_kzalloc(sizeof(DMS_EVENT_NODE_STRU), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (event_node == NULL) {
        ka_task_mutex_unlock(&device_fault_event->lock);
        dms_err("Malloc memory failed!\n");
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    ret = memcpy_s(&event_node->event, sizeof(struct dms_event_para), fault_event, sizeof(struct dms_event_para));
    if (ret != 0) {
        dms_err("Call memcpy_s failed. (dev_id=%u, event_id=0x%x, ret=%d)\n", dev_id, fault_event->event_id, ret);
        goto FREE;
    }
    device_fault_event->highest_severity = (fault_event->severity > device_fault_event->highest_severity) ?
                                               fault_event->severity :
                                               device_fault_event->highest_severity;

    ka_list_add(&event_node->node, &device_fault_event->head);
    device_fault_event->event_num++;
    ka_task_mutex_unlock(&device_fault_event->lock);

    dms_info("Save fault event. (dev_id=%u; event_id=0x%x; node_id=%u; alarm_raised_time=%llu ms)\n", dev_id,
             fault_event->event_id, fault_event->node_id, fault_event->alarm_raised_time);
    return 0;

FREE:
    ka_task_mutex_unlock(&device_fault_event->lock);
    dbl_kfree(event_node);
    event_node = NULL;
    return ret;
}

int dms_del_event_in_local(struct dms_event_para *fault_event)
{
    unsigned int dev_id;
    struct dms_device_event *device_fault_event = NULL;
    DMS_EVENT_NODE_STRU *event_node = NULL;

    dev_id = fault_event->deviceid;
    if (dev_id >= ASCEND_PDEV_MAX_NUM) {
        dms_err("Invalid device id. (dev_id=%u; event_id=%u)\n", dev_id, fault_event->event_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    device_fault_event = dms_get_device_event(dev_id);
    if (device_fault_event == NULL) {
        dms_err("device_fault_info is NULL. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_PARA_ERROR;
    }

    ka_task_mutex_lock(&device_fault_event->lock);
    event_node = get_an_event_node_from_list(device_fault_event, fault_event);
    if (event_node == NULL) {
        ka_task_mutex_unlock(&device_fault_event->lock);
        dms_warn("cannot find the event node. (event_id=0x%x, dev_id=%u)\n", fault_event->event_id,
                 fault_event->deviceid);
        /* the event to be deleted does not exist in the linked list */
        return 0;
    }

    ka_list_del(&event_node->node);
    device_fault_event->event_num--;
    update_event_highest_severity(device_fault_event, fault_event);
    ka_task_mutex_unlock(&device_fault_event->lock);
    dbl_kfree(event_node);
    event_node = NULL;
    dms_info("Delete fault event. (dev_id=%u; event_id=0x%x; node_id=%u; alarm_raised_time=%llu ms)\n", dev_id,
             fault_event->event_id, fault_event->node_id, fault_event->alarm_raised_time);
    return 0;
}

STATIC bool is_same_notify_event_node(struct dms_event_para *event1, struct dms_event_para *event2)
{
    return event1->deviceid == event2->deviceid && event1->event_id == event2->event_id;
}

STATIC DMS_EVENT_NODE_STRU *get_notify_event_node_from_list(struct dms_device_event *device_notify_event,
                                                            struct dms_event_para *notify_event)
{
    DMS_EVENT_NODE_STRU *pos = NULL;
    DMS_EVENT_NODE_STRU *n = NULL;

    ka_list_for_each_entry_safe(pos, n, &device_notify_event->head, node)
    {
        if (is_same_notify_event_node(&pos->event, notify_event)) {
            return pos;
        }
    }

    return NULL;
}

STATIC int dms_add_notify_event_in_local(struct dms_event_para *notify_event)
{
    int ret;
    unsigned int dev_id = notify_event->deviceid;
    struct dms_device_event *device_notify_event = NULL;
    DMS_EVENT_NODE_STRU *event_node = NULL;

    device_notify_event = dms_get_device_notify_event(dev_id);
    if (device_notify_event == NULL) {
        dms_err("device_fault_info is NULL. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_PARA_ERROR;
    }

    ka_task_mutex_lock(&device_notify_event->lock);
    event_node = get_notify_event_node_from_list(device_notify_event, notify_event);
    if (event_node != NULL) { /* the same event already exists in the list */
        ka_list_del(&event_node->node);
        device_notify_event->event_num--;
        update_event_highest_severity(device_notify_event, notify_event);
        dbl_kfree(event_node);
        event_node = NULL;
    }
    ret = del_event_in_full_device_event(dev_id, device_notify_event);
    if (ret != 0) {
        ka_task_mutex_unlock(&device_notify_event->lock);
        return ret;
    }

    event_node = (DMS_EVENT_NODE_STRU *)dbl_kzalloc(sizeof(DMS_EVENT_NODE_STRU), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (event_node == NULL) {
        ka_task_mutex_unlock(&device_notify_event->lock);
        dms_err("Malloc memory failed!\n");
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    ret = memcpy_s(&event_node->event, sizeof(struct dms_event_para), notify_event, sizeof(struct dms_event_para));
    if (ret != 0) {
        dms_err("Call memcpy_s failed. (dev_id=%u, event_id=0x%x, ret=%d)\n", dev_id, notify_event->event_id, ret);
        goto FREE;
    }
    device_notify_event->highest_severity = (notify_event->severity > device_notify_event->highest_severity) ?
                                                notify_event->severity :
                                                device_notify_event->highest_severity;

    ka_list_add(&event_node->node, &device_notify_event->head);
    device_notify_event->event_num++;
    ka_task_mutex_unlock(&device_notify_event->lock);

    dms_info("Save notify event. (dev_id=%u; event_id=0x%x; node_id=%u; alarm_raised_time=%llu ms)\n", dev_id,
             notify_event->event_id, notify_event->node_id, notify_event->alarm_raised_time);
    return 0;

FREE:
    ka_task_mutex_unlock(&device_notify_event->lock);
    dbl_kfree(event_node);
    event_node = NULL;
    return ret;
}

int dms_save_remote_event_in_local(struct dms_event_para *fault_event)
{
    unsigned char assertion;

    assertion = fault_event->assertion;
    switch (assertion) {
        case DMS_EVENT_TYPE_ONE_TIME: /* notify event occur */
            return dms_add_notify_event_in_local(fault_event);
        case DMS_EVENT_TYPE_OCCUR: /* fault event occur */
            return dms_add_event_in_local(fault_event);
        case DMS_EVENT_TYPE_RESUME: /* fault event resume */
            return dms_del_event_in_local(fault_event);
        default:
            return 0;
    }
}

int dms_save_exception_in_local(void *arg)
{
    int ret;
    int timeout = 100; /* wait for 100ms each time; return if no event occurs within 100ms */
    struct dms_event_para fault_event = {0};
    struct dms_event_filter filter = {0};

    dms_info("dms save remote exception in local task start\n");
    while (!ka_task_kthread_should_stop()) {
        ret = dms_event_get_exception(DMS_FAULT_DEVICE_ALL, &filter, &fault_event, timeout, FROM_KERNEL);
        if (ret != 0) {
            continue;
        }

        ret = dms_save_remote_event_in_local(&fault_event);
        if (ret != 0) {
            continue;
        }
    }

    dms_event_release_proc(ka_task_get_current_tgid(), ka_task_get_current_pid());

    dms_info("dms save remote exception in local task exit\n");
    return 0;
}

int dms_remote_event_save_in_local_init(void)
{
#ifndef DMS_UT
    g_event_save_task = ka_task_kthread_create(dms_save_exception_in_local, (void *)NULL, "dms_save_exception_task");
    if (KA_IS_ERR(g_event_save_task)) {
        dms_err("ka_task_kthread_create failed.\n");
        return DRV_ERROR_INNER_ERR;
    }

    (void)ka_task_wake_up_process(g_event_save_task);
#endif
    return DRV_ERROR_NONE;
}

void dms_remote_event_save_in_local_exit(void)
{
    if (!KA_IS_ERR(g_event_save_task)) {
        ka_task_kthread_stop(g_event_save_task);
    }

    g_event_save_task = NULL;
    return;
}

void dms_event_host_init(void)
{
    u32 i;

    ka_task_mutex_init(&g_hvdms_subscribe_status_mutex);
    ka_task_mutex_lock(&g_hvdms_subscribe_status_mutex);
    for (i = 0; i < ASCEND_DEV_MAX_NUM; i++) {
        g_hvdms_subscribe_status_bitmap[i] = 0;
    }
    ka_task_mutex_unlock(&g_hvdms_subscribe_status_mutex);
}

void dms_event_host_uninit(void)
{
    u32 i;

    ka_task_mutex_lock(&g_hvdms_subscribe_status_mutex);
    for (i = 0; i < ASCEND_DEV_MAX_NUM; i++) {
        g_hvdms_subscribe_status_bitmap[i] = 0;
    }
    ka_task_mutex_unlock(&g_hvdms_subscribe_status_mutex);
    ka_task_mutex_destroy(&g_hvdms_subscribe_status_mutex);
}
