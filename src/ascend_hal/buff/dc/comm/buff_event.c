/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <sys/types.h>
#include <unistd.h>

#include "ascend_hal.h"

#include "drv_buff_common.h"
#include "drv_buff_adp.h"
#include "buff_user_interface.h"
#include "buff_manage_base.h"
#include "drv_user_common.h"
#include "buff_event.h"
#include "buff_platform.h"

#define EVENT_MAX_SUBSCRIBER 12
#define MAX_SUBSCRIBER_NAME_LEN 128

#define EVENT_TYPE_ADD 1
#define EVENT_TYPE_DEL 0

#define EVENT_GRP_MAX_NUM 32

#define PROP_PID_OFFSET 32
#define PROP_DEVID_OFFSET 24
#define PROP_DEVID_MASK 0xff
#define PROP_GRP_OFFSET 16
#define PROP_GRP_MASK 0xff
#define PROP_EVENT_OFFSET 0
#define PROP_EVENT_MASK 0xffff

/*lint -e454 -e429 -e144 -e629 */
struct buff_scale_node {
    struct list_head node;
    void *addr;
    unsigned long long size;
};

LIST_HEAD(scale_buff_list);
static pthread_mutex_t scale_mutex = PTHREAD_MUTEX_INITIALIZER;

static void buff_scale_submit_event(int pool_id, int type, void *addr, unsigned long long size,
                                    unsigned long subscriber)
{
    struct event_summary event;
    struct buf_scale_event scale_event;
    unsigned int devid;
    drvError_t ret;

    event.pid = (int)(subscriber >> PROP_PID_OFFSET);
    event.grp_id = (unsigned int)((subscriber >> PROP_GRP_OFFSET) & PROP_GRP_MASK);
    event.event_id = (int)((subscriber >> PROP_EVENT_OFFSET) & PROP_EVENT_MASK);
    event.subevent_id = 0;
    event.dst_engine = buff_event_get_dst_engine();
    event.policy = ONLY;
    event.msg = (char *)&scale_event;
    event.msg_len = (unsigned int)sizeof(scale_event);

    scale_event.type = type;
    scale_event.grpId = pool_id;
    scale_event.addr = (unsigned long long)(uintptr_t)addr;
    scale_event.size = size;

    devid = (unsigned int)((subscriber >> PROP_DEVID_OFFSET) & PROP_DEVID_MASK);
    ret = halEschedSubmitEvent(devid, &event);
    if (ret != DRV_ERROR_NONE) {
        buff_warn("Notify not successful. (devid=%u; pool_id=%d; pid=%d; grp_id=%d; addr=%p; size=%llx)\n", devid,
                  pool_id, event.pid, event.grp_id, addr, size);
    }
}

static void buff_scale_add_to_list(void *addr, unsigned long long size)
{
    struct buff_scale_node *scale_node = NULL;

    scale_node = (struct buff_scale_node *)malloc(sizeof(struct buff_scale_node));
    if (scale_node == NULL) {
        buff_warn("addr %p size %llx add to list alloc mem not success\n", addr, size);
        return;
    }

    scale_node->addr = addr;
    scale_node->size = size;

    (void)pthread_mutex_lock(&scale_mutex);
    drv_user_list_add_tail(&scale_node->node, &scale_buff_list);
    (void)pthread_mutex_unlock(&scale_mutex);
}

static void buff_scale_del_from_list(void *addr, unsigned long long size)
{
    struct buff_scale_node *scale_node = NULL;
    struct list_head *pos = NULL, *n = NULL;

    (void)pthread_mutex_lock(&scale_mutex);
    list_for_each_safe(pos, n, &scale_buff_list)
    {
        scale_node = list_entry(pos, struct buff_scale_node, node);
        if (scale_node != NULL) {
            if ((scale_node->addr == addr) && (scale_node->size == size)) {
                drv_user_list_del(&scale_node->node);
                free(scale_node);
                break;
            }
        }
    }
    (void)pthread_mutex_unlock(&scale_mutex);
}

static void buff_scale_list_event_submit(int pool_id, unsigned long subscriber)
{
    struct buff_scale_node *scale_node = NULL;
    struct list_head *pos = NULL, *n = NULL;

    (void)pthread_mutex_lock(&scale_mutex);
    list_for_each_safe(pos, n, &scale_buff_list)
    {
        scale_node = list_entry(pos, struct buff_scale_node, node);
        if (scale_node != NULL) {
            buff_scale_submit_event(pool_id, EVENT_TYPE_ADD, scale_node->addr, scale_node->size, subscriber);
        }
    }
    (void)pthread_mutex_unlock(&scale_mutex);
}

static void buff_format_subscriber_name(char *name, unsigned int len, unsigned int index)
{
    if (sprintf_s(name, len, "buf_scale_subscriber%u", index) <= 0) {
        buff_warn("Sprintf_s not success. (index=%u)\n", index);
    }
}

static void buff_scale_notify(int type, void *addr, unsigned long long size)
{
    unsigned long subscriber;
    unsigned int i;
    int pool_id;

    pool_id = buff_get_default_pool_id();

    for (i = 0; i < EVENT_MAX_SUBSCRIBER; i++) {
        char name[MAX_SUBSCRIBER_NAME_LEN];
        drvError_t ret;

        buff_format_subscriber_name(name, MAX_SUBSCRIBER_NAME_LEN, i);
        ret = buff_pool_get_prop(pool_id, name, &subscriber);
        if (ret == DRV_ERROR_NONE) {
            buff_scale_submit_event(pool_id, type, addr, size, subscriber);
        }
    }
}

void buff_scale_out(void *addr, unsigned long long size)
{
    buff_scale_add_to_list(addr, size);
    buff_scale_notify(EVENT_TYPE_ADD, addr, size);
}

void buff_scale_in(void *addr, unsigned long long size)
{
    buff_scale_notify(EVENT_TYPE_DEL, addr, size);
    buff_scale_del_from_list(addr, size);
}

static drvError_t event_report_para_check(const char *grp_name)
{
    unsigned long len;

    if (grp_name == NULL) {
        buff_err("Grp_name is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    len = strnlen(grp_name, BUFF_GRP_NAME_LEN);
    if ((len >= BUFF_GRP_NAME_LEN) || (len == 0)) {
        buff_err("Grp_name len is err. (len=%lu; max_len=%d)\n", len, BUFF_GRP_NAME_LEN);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

drvError_t halBufEventReport(const char *grpName)
{
    unsigned long subscriber;
    drvError_t ret;
    unsigned int i;
    int pool_id = -1;

    ret = event_report_para_check(grpName);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = buff_pool_id_query(grpName, &pool_id);
    if ((ret != DRV_ERROR_NONE) || (pool_id != buff_get_default_pool_id())) {
        buff_err("Buff_pool_id_query failed. (grp_name=%s; pool_id=%d; ret=%d; get_default_pool_id=%d)\n", grpName,
                 pool_id, ret, buff_get_default_pool_id());
        return DRV_ERROR_IOCRL_FAIL;
    }

    for (i = 0; i < EVENT_MAX_SUBSCRIBER; i++) {
        char name[MAX_SUBSCRIBER_NAME_LEN];
        buff_format_subscriber_name(name, MAX_SUBSCRIBER_NAME_LEN, i);
        ret = buff_pool_get_prop(pool_id, name, &subscriber);
        if (ret == DRV_ERROR_NONE) {
            buff_scale_list_event_submit(pool_id, subscriber);
        }
    }

    return DRV_ERROR_NONE;
}

static drvError_t event_subscribe_para_check(const char *grp_name, unsigned int thread_grp_id, unsigned int event_id,
                                             unsigned int devid)
{
    unsigned long len;

    if ((grp_name == NULL) || (thread_grp_id >= EVENT_GRP_MAX_NUM) || (event_id >= EVENT_MAX_NUM) ||
        (devid > PROP_DEVID_MASK)) {
        buff_err("Parameter invalid. (thread_grp_id=%d; event_id=%d; grp_name=%p; devid=%u)\n", thread_grp_id, event_id,
                 grp_name, devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    len = strnlen(grp_name, BUFF_GRP_NAME_LEN);
    if ((len >= BUFF_GRP_NAME_LEN) || (len == 0)) {
        buff_err("Grp_name len is err. (len=%lu; max_len=%d)\n", len, BUFF_GRP_NAME_LEN);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

drvError_t halBufEventSubscribe(const char *grpName, unsigned int threadGrpId, unsigned int event_id,
                                unsigned int devid)
{
    unsigned long subscriber;
    drvError_t ret;
    unsigned int i;
    int pool_id;

    ret = event_subscribe_para_check(grpName, threadGrpId, event_id, devid);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = buff_pool_id_query(grpName, &pool_id);
    if (ret != DRV_ERROR_NONE) {
        buff_err("Can not find grp_id by grp_name. (grp_name=%s)\n", grpName);
        return ret;
    }

    subscriber = (unsigned long)buff_get_current_pid() << PROP_PID_OFFSET; /*lint !e571*/
    subscriber |= (unsigned long)devid << PROP_DEVID_OFFSET;               /*lint !e571*/
    subscriber |= (unsigned long)threadGrpId << PROP_GRP_OFFSET;
    subscriber |= (unsigned long)event_id << PROP_EVENT_OFFSET;

    for (i = 0; i < EVENT_MAX_SUBSCRIBER; i++) {
        char name[MAX_SUBSCRIBER_NAME_LEN];
        buff_format_subscriber_name(name, MAX_SUBSCRIBER_NAME_LEN, i);
        ret = buff_pool_set_prop(pool_id, name, subscriber);
        if (ret == DRV_ERROR_NONE) {
            buff_event("Name register subscriber. (grp_name=%s; subscriber=%lx; i=%u)\n", grpName, subscriber, i);
            return DRV_ERROR_NONE;
        }
    }

    buff_err("Set prop failed. (pool_id=%d; grp_name=%s; ret=%d; i=%u)\n", pool_id, grpName, (int)ret, i);

    return (ret == DRV_ERROR_REPEATED_USERD) ? DRV_ERROR_NO_RESOURCES : ret;
}
/*lint +e429 +e144 +e629 */
