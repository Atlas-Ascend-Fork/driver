/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>

#ifdef CFG_DSMI_DEVICE_ENV
#include "drvfault_fault.h"
#include "drvfault_msg.h"
#endif
#include "ascend_hal.h"
#include "ascend_inpackage_hal.h"
#include "drv_type.h"
#include "dsmi_common.h"
#include "dms_cmd_def.h"
#include "dms_fault.h"
#include "dsmi_common_interface.h"
#include "dsmi_dmp_command.h"
#include "dsmi_adapt.h"

#if defined(CFG_FEATURE_GPIO_STATUS)
#define KEY_CHIP_TYPE_INDEX (8U)
#define FAULT_INJECT_INFO_NUM_MAX (64U)
#endif

int dsmi_register_fault_event_handler(int device_id, fault_event_handler handler)
{
#ifdef CFG_DSMI_DEVICE_ENV
#ifdef CFG_FEATURE_FAULT_EVENT
    int ret;

    if ((device_id >= MAX_CHIP_NUM) || (device_id < 0) || (handler == NULL)) {
        DEV_MON_ERR("devid %d is invalid or handler is NULL.\n", device_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = drvfault_get_event_thread_init(handler);
    if (ret != 0) {
        DEV_MON_ERR("devid %d drvfault_get_event_thread_init failed, ret(%d).\n", device_id, ret);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;

#else
    (void)device_id;
    (void)handler;
    return DRV_ERROR_NOT_SUPPORT;
#endif
#else
    (void)device_id;
    (void)handler;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

#define DSMI_READ_FAULT_DEVICE_ALL (-1)
#define DSMI_READ_FAULT_NO_TIMEOUT_FLAG (-1)
#define DSMI_EVENT_WAIT_TIME_ZERO 0 /* return immediately no block */

#if (!defined DRV_HOST) && (!defined CFG_FEATURE_DEFAULT_STACK_SIZE)
#define DSMI_FAULT_EVT_THREAD_STACK_SIZE (128 * 1024)
#endif
#define DSMI_FAULT_THREAD_SLEEP (20 * 1000) /* sleep 20 ms */
#define DSMI_FAULT_THREAD_INIT_VAL (-1)

struct dsmi_fault_event_thread {
    int device_id;
    bool run_flg;
    int result;
    struct dsmi_event_filter filter;
    fault_event_callback handler;
    pthread_mutex_t thread_lock;
    pthread_t thread_id;
};

STATIC struct dsmi_fault_event_thread g_fault_thread_para = {
    .device_id = -1,
    .run_flg = false,
    .result = DSMI_FAULT_THREAD_INIT_VAL,
    .filter = {0},
    .handler = NULL,
    .thread_lock = PTHREAD_MUTEX_INITIALIZER,
    .thread_id = 0,
};

typedef enum {
    THREAD_RUNING,
    THREAD_STOPING,
    THREAD_STOPPED,
} thread_running_state;

static volatile thread_running_state g_fault_event_running_flag = THREAD_STOPPED;

STATIC void dsmi_set_fault_event_thread_state(thread_running_state flag)
{
    g_fault_event_running_flag = flag;
}

STATIC thread_running_state dsmi_get_fault_event_thread_state(void)
{
    return g_fault_event_running_flag;
}

STATIC int dsmi_thread_read_fault_event(struct dsmi_fault_event_thread *thread_para, int timeout,
                                        struct dsmi_event *event)
{
    int ret;

    ret = dsmi_read_fault_event(thread_para->device_id, timeout, thread_para->filter, event);
    if (ret == (int)DRV_ERROR_NONE) {
        if (thread_para->handler != NULL) {
            thread_para->handler(event);
        } else {
            DEV_MON_INFO("Fault event has unsubscribed\n");
            return DRV_ERROR_UNINIT;
        }
    } else if (ret == (int)DRV_ERROR_NO_EVENT || ret == (int)DRV_ERROR_WAIT_TIMEOUT) {
        return ret;
    } else if (ret == -ERESTARTSYS) {
        DEV_MON_WARNING("Fault event reading thread will exit as system reboot\n");
        dsmi_set_fault_event_thread_state(THREAD_STOPPED);
        return ret;
    } else if (ret == (int)DRV_ERROR_NOT_SUPPORT) {
        dsmi_set_fault_event_thread_state(THREAD_STOPPED);
        return ret;
    } else if (ret == (int)DRV_ERROR_RESOURCE_OCCUPIED) {
        dsmi_set_fault_event_thread_state(THREAD_STOPPED);
        return ret;
    } else {
        DEV_MON_ERR("Get fault event failed. (ret=%d)\n", ret);
        (void)usleep(DSMI_FAULT_THREAD_SLEEP);
    }

    return ret;
}

STATIC void *dsmi_fault_event_thread_func(void *data)
{
    int ret;
    struct dsmi_event event = {0};
    struct dsmi_fault_event_thread *thread_para = (struct dsmi_fault_event_thread *)data;

    (void)mmSetCurrentThreadName("fault_event_subscribe");
    ret = dsmi_thread_read_fault_event(thread_para, DSMI_EVENT_WAIT_TIME_ZERO, &event);
    if (ret == (int)DRV_ERROR_NOT_SUPPORT) {
        __atomic_store_n(&thread_para->result, ret, __ATOMIC_RELEASE);
        return NULL;
    } else if (ret == (int)DRV_ERROR_RESOURCE_OCCUPIED) {
        DEV_MON_ERR("Over max number of process. (device id =%d; ret=%d)\n", thread_para->device_id, ret);
        __atomic_store_n(&thread_para->result, ret, __ATOMIC_RELEASE);
        return NULL;
    } else {
        __atomic_store_n(&thread_para->result, DRV_ERROR_NONE, __ATOMIC_RELEASE);
    }

    while (dsmi_get_fault_event_thread_state() == THREAD_RUNING) {
        ret = dsmi_thread_read_fault_event(thread_para, DSMI_READ_FAULT_NO_TIMEOUT_FLAG, &event);
        if (ret == (int)DRV_ERROR_UNINIT) {
            DEV_MON_INFO("User unsubscribed, thread exit\n");
            break;
        } else if (ret == -ERESTARTSYS) {
            DEV_MON_WARNING("Fault event reading thread exited\n");
            return NULL;
        }

        pthread_testcancel(); /* set thread cancel point */
    }
    dsmi_set_fault_event_thread_state(THREAD_STOPPED);
    thread_para->run_flg = false;
    DEV_MON_WARNING("Something went wrong with dsmi_fault_event_thread_func!\n");

    return NULL;
}

STATIC int dsmi_unsubscribe_fault_event(struct dsmi_fault_event_thread *thread_para)
{
    int ret;

    if (dsmi_get_fault_event_thread_state() == THREAD_RUNING) {
        DEV_MON_INFO("unsubscribe fault event\n");
        ret = pthread_cancel(thread_para->thread_id);
        if (ret != 0) {
            DEV_MON_ERR("stop fault event thread failed.\n");
        } else {
            DEV_MON_INFO("stop fault event thread successfully.\n");
        }
        (void)pthread_mutex_lock(&thread_para->thread_lock);
        thread_para->handler = NULL;
        (void)pthread_mutex_unlock(&thread_para->thread_lock);
        return ret;
    }
    DEV_MON_INFO("fault event thread is not running, unsubscribe finished.\n");
    return 0;
}

#define DSMI_FAULT_EVENT_THREAD_PRIORITY (10)

STATIC int dsmi_init_and_create_fault_thread(struct dsmi_fault_event_thread *thread_para, pthread_attr_t *attr,
                                             struct sched_param *sched_param)
{
    int ret;

    (void)pthread_attr_init(attr);
    (void)pthread_attr_setdetachstate(attr, PTHREAD_CREATE_DETACHED);
    (void)pthread_attr_setscope(attr, PTHREAD_SCOPE_SYSTEM);
#if (!defined DRV_HOST) && (!defined CFG_FEATURE_DEFAULT_STACK_SIZE)
    (void)pthread_attr_setstacksize(attr, DSMI_FAULT_EVT_THREAD_STACK_SIZE);
#endif

    (void)pthread_attr_setschedpolicy(attr, SCHED_RR);
    sched_param->sched_priority = sched_get_priority_min(SCHED_RR) + DSMI_FAULT_EVENT_THREAD_PRIORITY;
    (void)pthread_attr_setschedparam(attr, sched_param);

    dsmi_set_fault_event_thread_state(THREAD_RUNING);
    ret = pthread_create(&thread_para->thread_id, attr, dsmi_fault_event_thread_func, (void *)thread_para);
    if (ret != 0) {
        DEV_MON_ERR("Create get_event thread failed. (ret=%d)\n", ret);
        thread_para->run_flg = false;
        dsmi_set_fault_event_thread_state(THREAD_STOPPED);
        (void)pthread_mutex_unlock(&thread_para->thread_lock);
        (void)pthread_attr_destroy(attr);
        return DRV_ERROR_INNER_ERR;
    }

    return 0;
}

int dsmi_subscribe_fault_event(int device_id, struct dsmi_event_filter filter, fault_event_callback handler)
{
    struct dsmi_fault_event_thread *thread_para = &g_fault_thread_para;
    struct sched_param thread_sched_param;
    pthread_attr_t attr;
    int ret;

    DEV_MON_EVENT("subscribe fault event. (device_id=%d; uid=%u)\n", device_id, getuid());

    if ((device_id < 0) && (device_id != DSMI_READ_FAULT_DEVICE_ALL)) {
        DEV_MON_ERR("Invalid parameter. (device_id=%d, valid range=[0, devnum) or DSMI_READ_FAULT_DEVICE_ALL)\n",
                    device_id);
        return DRV_ERROR_PARA_ERROR;
    }
    if (handler == NULL) {
        return dsmi_unsubscribe_fault_event(thread_para);
    }

    if (device_id != DSMI_READ_FAULT_DEVICE_ALL) {
        ret = dsmi_check_device_id(device_id);
        CHECK_DEVICE_BUSY(device_id, ret);
        if (ret != 0) {
            DEV_MON_ERR("Device does not exist. (dev_id=0x%x; ret=%d)\n", device_id, ret);
            return DRV_ERROR_INVALID_DEVICE;
        }
    }

    if (pthread_mutex_trylock(&thread_para->thread_lock) != 0) {
        DEV_MON_ERR("has started one thread before.\n");
        return DRV_ERROR_RESOURCE_OCCUPIED;
    }

    if (thread_para->run_flg == true) {
        (void)pthread_mutex_unlock(&thread_para->thread_lock);
        DEV_MON_ERR("A thread has already been started.\n");
        return DRV_ERROR_RESOURCE_OCCUPIED;
    }

    thread_para->run_flg = true;
    thread_para->device_id = device_id;
    thread_para->handler = handler;
    thread_para->filter = filter;

    ret = dsmi_init_and_create_fault_thread(thread_para, &attr, &thread_sched_param);
    if (ret != 0) {
        return ret;
    }

    while (__atomic_load_n(&thread_para->result, __ATOMIC_ACQUIRE) == DSMI_FAULT_THREAD_INIT_VAL) {
        (void)usleep(DSMI_FAULT_THREAD_SLEEP);
    }
    ret = __atomic_load_n(&thread_para->result, __ATOMIC_ACQUIRE);

    (void)pthread_mutex_unlock(&thread_para->thread_lock);
    (void)pthread_attr_destroy(&attr);

    DEV_MON_DEBUG("dsmi_subscribe_fault_event end ***!\n");

    return ret;
}

#if (defined CFG_FEATURE_FAULT_EVENT) && (defined DRV_HOST)
STATIC int dsmi_fault_event_data_fill(struct dsmi_event *event_buf, struct halFaultEventInfo *event_info, int event_cnt)
{
    int i;
    int ret;

    for (i = 0; i < event_cnt; i++) {
        event_buf[i].event_t.dms_event.event_id = event_info[i].event_id;
        event_buf[i].event_t.dms_event.deviceid = event_info[i].deviceid;
        event_buf[i].event_t.dms_event.node_type = (unsigned char)event_info[i].node_type;
        event_buf[i].event_t.dms_event.node_id = event_info[i].node_id;
        event_buf[i].event_t.dms_event.sub_node_type = (unsigned char)event_info[i].sub_node_type;
        event_buf[i].event_t.dms_event.sub_node_id = event_info[i].sub_node_id;
        event_buf[i].event_t.dms_event.severity = event_info[i].severity;
        event_buf[i].event_t.dms_event.assertion = event_info[i].assertion;
        event_buf[i].event_t.dms_event.event_serial_num = event_info[i].event_serial_num;
        event_buf[i].event_t.dms_event.notify_serial_num = event_info[i].notify_serial_num;
        event_buf[i].event_t.dms_event.alarm_raised_time = event_info[i].alarm_raised_time;
        event_buf[i].event_t.dms_event.node_type_ex = event_info[i].node_type;
        event_buf[i].event_t.dms_event.sub_node_type_ex = event_info[i].sub_node_type;
        if (sprintf_s(event_buf[i].event_t.dms_event.event_name, DMS_MAX_EVENT_NAME_LENGTH, "deviceid=%u, %s",
                      event_info[i].deviceid, event_info[i].event_name) < 0) {
            DEV_MON_ERR("sprintf_s event_name failed.\n");
            return DRV_ERROR_INNER_ERR;
        }
        ret = memcpy_s(event_buf[i].event_t.dms_event.additional_info, DMS_MAX_EVENT_DATA_LENGTH,
                       event_info[i].additional_info, DMS_MAX_EVENT_DATA_LENGTH);
        if (ret != 0) {
            DEV_MON_ERR("memcpy_s additional_Info failed.\n");
            return DRV_ERROR_INNER_ERR;
        }
        event_buf[i].event_t.dms_event.os_id = 0;
    }

    return DRV_ERROR_NONE;
}

STATIC int dsmi_get_fault_event_on_host(int device_id, struct dsmi_event *event_buf, int max_event_cnt, int *event_cnt)
{
    struct halEventFilter filter = {0};
    struct halFaultEventInfo *event_info = NULL;
    int ret;

    event_info = (struct halFaultEventInfo *)calloc((unsigned long)max_event_cnt, sizeof(struct halFaultEventInfo));
    if (event_info == NULL) {
        DEV_MON_ERR("Failed to alloc memory for event_buff. (devid=%d)\n", device_id);
        return DRV_ERROR_MALLOC_FAIL;
    }

    ret = halGetFaultEvent((unsigned int)device_id, &filter, event_info, (unsigned int)max_event_cnt,
                           (unsigned int *)event_cnt);
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "Failed to obtain fault events. (dev_id=%d; ret=%d)\n", device_id, ret);
        goto FREE_EVENT_INFO;
    }

    if (*event_cnt > max_event_cnt) {
        DEV_MON_WARNING("Event count exceeds max event count. (dev_id=%d; event_cnt=%d; max_event_cnt=%d)\n", device_id,
                        *event_cnt, max_event_cnt);
        *event_cnt = max_event_cnt;
    }

    ret = dsmi_fault_event_data_fill(event_buf, event_info, *event_cnt);
    if (ret != 0) {
        DEV_MON_ERR("Failed to fill DSMI fault event data. (dev_id=%d; event_cnt=%d; ret=%d)\n", device_id, *event_cnt,
                    ret);
        goto FREE_EVENT_INFO;
    }

    ret = DRV_ERROR_NONE;
FREE_EVENT_INFO:
    free(event_info);
    event_info = NULL;
    return ret;
}
#endif

int dsmi_get_fault_event(int device_id, int max_event_cnt, struct dsmi_event *event_buf, int *event_cnt)
{
#if (defined CFG_FEATURE_FAULT_EVENT) || (defined DEV_MON_UT)
    int ret;
    int input_event_cnt = max_event_cnt;
    if ((event_buf == NULL) || (event_cnt == NULL)) {
        DEV_MON_ERR("Invalid parameter. event_buf or event_cnt is null.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dsmi_check_device_id(device_id);
    CHECK_DEVICE_BUSY(device_id, ret);
    if (ret != 0) {
        DEV_MON_ERR("Device didn't exist. (device_id=%d; ret=%d)\n", device_id, ret);
        return DRV_ERROR_PARA_ERROR;
    }

    if (max_event_cnt <= 0) {
        DEV_MON_ERR("Invalid parameter. max_event_cnt needs to be greater than 0. (max_event_cnt=%d)\n", max_event_cnt);
        return DRV_ERROR_PARA_ERROR;
    }

    if (input_event_cnt > MAX_EVENT_COUNT_OF_GET_FAULT_EVENT) {
        DEV_MON_WARNING("Reset max_event_cnt because input exceeds limit. (max_event_cnt=%d; input=%d; limit=%d)\n",
                        MAX_EVENT_COUNT_OF_GET_FAULT_EVENT, input_event_cnt, MAX_EVENT_COUNT_OF_GET_FAULT_EVENT);
        input_event_cnt = MAX_EVENT_COUNT_OF_GET_FAULT_EVENT;
    }

#ifdef DRV_HOST
    ret = dsmi_get_fault_event_on_host(device_id, event_buf, input_event_cnt, event_cnt);
#else
    ret = halDmsGetHistoryFaultEvent(device_id, event_buf, input_event_cnt, event_cnt);
#endif
    if (ret != 0) {
#ifdef CFG_FEATURE_ERR_CODE_NOT_OPTIMIZATION
        return DRV_ERROR_INNER_ERR;
#else
        return ret;
#endif
    }
    return DRV_ERROR_NONE;
#else
    (void)device_id;
    (void)max_event_cnt;
    (void)event_buf;
    (void)event_cnt;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

static int get_remain_time(int timeout, struct timespec *time_last)
{
    long times_gap;
    struct timespec time_current = {0, 0};

    if (timeout == DSMI_READ_FAULT_NO_TIMEOUT_FLAG) {
        return DSMI_READ_FAULT_NO_TIMEOUT_FLAG;
    }

    (void)clock_gettime(CLOCK_MONOTONIC, &time_current);
    /* 1000 ms per second */
    times_gap = (long)((time_current.tv_sec - time_last->tv_sec) * 1000) +
                (time_current.tv_nsec - time_last->tv_nsec) / 1000000; /* 1000000 ns per ms */

    *time_last = time_current;
    if (times_gap < 0) {
        return 0;
    } else if (timeout < times_gap) {
        return 0;
    } else {
        return timeout - (int)times_gap;
    }
}

int dsmi_read_fault_event(int device_id, int timeout, struct dsmi_event_filter filter, struct dsmi_event *event)
{
    struct dms_fault_event event_buf = {0};
    struct timespec time_last = {0, 0};
    struct dms_event_filter dms_filter = {0};
    int remain_time = timeout;
    int ret;

    if (((device_id < 0) && (device_id != DSMI_READ_FAULT_DEVICE_ALL)) || (event == NULL)) {
        DEV_MON_ERR("Invalid parameter. (device_id=%d; timeout=%dms; event=%s)\n", device_id, timeout,
                    event == NULL ? "NULL" : "OK");
        return DRV_ERROR_PARA_ERROR;
    }

    if (device_id != DSMI_READ_FAULT_DEVICE_ALL) {
        ret = dsmi_check_device_id(device_id);
        if (ret != 0) {
            DEV_MON_ERR("device_id didn't exist. (device_id=0x%x; ret=%d)\n", device_id, ret);
            return ret;
        }
    }

    dms_filter.filter_flag = filter.filter_flag;
    dms_filter.event_id = filter.event_id;
    dms_filter.severity = filter.severity;
    dms_filter.node_type = (unsigned short)filter.node_type;

    (void)clock_gettime(CLOCK_MONOTONIC, &time_last);
    if (memset_s(&event_buf, sizeof(event_buf), 0, sizeof(event_buf)) != EOK) {
        DEV_MON_WARNING("memset event buff warn\n");
    }

    do {
        ret = (int)halDmsGetFaultEvent(device_id, &dms_filter, remain_time, &event_buf);
        remain_time = get_remain_time(remain_time, &time_last);
        if (ret == 0) {
            break;
        } else if (ret == DRV_ERROR_NO_EVENT) {
            continue;
        } else {
            return ret;
        }
    } while ((remain_time > 0) || (remain_time == DSMI_READ_FAULT_NO_TIMEOUT_FLAG));

    if (ret == 0) {
        if (memcpy_s(&event->event_t.dms_event, sizeof(struct dms_fault_event), &event_buf,
                     sizeof(struct dms_fault_event)) != 0) {
            DEV_MON_ERR("memcpy_s failed.\n");
            return DRV_ERROR_INNER_ERR;
        }
        event->type = DMS_FAULT_EVENT;
    }

    return ret;
}

void dsmi_stop_fault_event_thread(void)
{
    if (dsmi_get_fault_event_thread_state() == THREAD_RUNING) {
        dsmi_set_fault_event_thread_state(THREAD_STOPING);
        while (dsmi_get_fault_event_thread_state() != THREAD_STOPPED) {
            (void)usleep(DM_COMMON_INIT_EXIT_DELAY);
        }
    }
}

int dsmi_get_fault_inject_info(unsigned int device_id, unsigned int max_info_cnt, DSMI_FAULT_INJECT_INFO *info_buf,
                               unsigned int *real_info_cnt)
{
    return _dsmi_get_fault_inject_info(device_id, max_info_cnt, info_buf, real_info_cnt);
}
