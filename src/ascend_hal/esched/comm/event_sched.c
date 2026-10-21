/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "event_sched.h"
#include <sys/ioctl.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/sysinfo.h>
#include <pthread.h>
#include <stdbool.h>
#include "securec.h"
#ifdef CFG_FEATURE_EXTERNAL_CDEV
#include "davinci_interface.h"
#endif
#include "dms_user_interface.h"
#include "pbl_uda_user.h"

#include "esched_ioctl.h"
#include "esched_user_interface.h"
#include "esched_feature.h"
#include "esched_platform.h"

#define ESCHED_CLOSE_MAX_SLEEP 10000 /* 10ms */

static THREAD__ int32_t sched_dev_fd[ESCHED_DEV_NUM];
static esched_proc_grp_info sched_grp[ESCHED_DEV_NUM];

/* Create the key once when proccess attach device, keep the key until proccess exits. */
static pthread_key_t esched_thread_key;
static pthread_once_t esched_thread_key_init = PTHREAD_ONCE_INIT;
static bool esched_thread_key_flag = false;

pthread_mutex_t g_esched_init_mutex = PTHREAD_MUTEX_INITIALIZER;

THREAD__ void (*esched_finish_func[SCHED_MAX_GRP_NUM][EVENT_MAX_NUM])(unsigned int dev_id, unsigned int grp_id,
                                                                      unsigned int event_id,
                                                                      unsigned int subevent_id) = {{NULL}};
THREAD__ void (*esched_finish_func_ex[SCHED_MAX_GRP_NUM][EVENT_MAX_NUM])(unsigned int dev_id, unsigned int grp_id,
                                                                         esched_event_info event_info) = {{NULL}};

THREAD__ void (*esched_ack_func[SCHED_MAX_GRP_NUM][EVENT_MAX_NUM])(unsigned int dev_id, unsigned int subevent_id,
                                                                   char *msg, unsigned int msgLen) = {{NULL}};
THREAD__ void (*esched_trace_record_func[SCHED_MAX_GRP_NUM][EVENT_MAX_NUM])(
    unsigned int grp_id, unsigned int event_id, unsigned int subevent_id,
    sched_trace_time_info *sched_trace_time_info) = {{NULL}};

static bool g_esched_snapshot_flag = false;
unsigned long long esched_get_cur_cpu_timestamp(void)
{
    unsigned long long cur_tick = esched_get_cur_cpu_tick();
    if (g_esched_snapshot_flag) {
        return sched_adapt_curr_time(cur_tick);
    }
    return cur_tick;
}

int esched_device_check(unsigned int dev_id)
{
    if ((dev_id >= ESCHED_LOGIC_DEV_NUM) && (!esched_is_host_virtual_dev(dev_id))) {
        return DRV_ERROR_INVALID_DEVICE;
    }
    return DRV_ERROR_NONE;
}

void esched_clear_grp_info(unsigned int dev_id)
{
    (void)memset_s((void *)sched_grp[dev_id].info, sizeof(sched_grp[dev_id].info), 0, sizeof(sched_grp[dev_id].info));
}

STATIC void esched_init_global_fork(unsigned int dev_id, int fd)
{
    (void)pthread_mutex_lock(&g_esched_init_mutex);
    if (sched_dev_fd[dev_id] == fd) { /* fd may re-init by other threads */
        (void)memset_s((void *)&sched_dev_fd[dev_id], sizeof(int32_t), 0, sizeof(int32_t));
    }
    (void)pthread_mutex_unlock(&g_esched_init_mutex);
}

#ifndef EMU_ST
int esched_open(const char *char_dev_name)
{
    return open(char_dev_name, O_RDWR | O_NONBLOCK | O_CLOEXEC);
}

int esched_close(int fd)
{
    return close(fd);
}

int esched_ioctl(int fd, uint32_t cmd, void *para)
{
    return ioctl(fd, cmd, para);
}
#endif

void esched_query_sync_msg_trace(uint32_t dev_id, struct event_summary *event, uint32_t grp_id, uint32_t thread_id)
{
    drvError_t ret;
    struct sched_ioctl_para_trace para;

    if ((dev_id >= ESCHED_DEV_NUM) || (grp_id < SCHED_MAX_DEFAULT_GRP_NUM)) {
        sched_err("input param is invalid. (dev_id=%u; grp_id=%u; thread_id=%u)\n", dev_id, grp_id, thread_id);
        return;
    }

    para.input.dev_id = (unsigned int)dev_id;
    para.input.dst_pid = (unsigned int)event->pid;
    para.input.dst_gid = event->grp_id;
    para.input.dst_engine = event->dst_engine;
    para.input.event_id = event->event_id;
    para.input.subevent_id = event->subevent_id;
    para.input.msg = *((struct sync_msg_info *)event->msg);
    ret = esched_dev_ioctl(dev_id, SCHED_QUERY_SYNC_MSG_TRACE, &para);
    if (ret != DRV_ERROR_NONE) {
        sched_err("query trace fail. (ret=%d; dev_id=%u; grp_id=%u; thread_id=%u)\n", ret, dev_id, grp_id, thread_id);
        return;
    }

    sched_run_info(
        "trace event cfg. (dev_id=%u; grp_id=%u; thread_id=%u; event_id=%u; subevent_id=%u; pid=%d; dst_engine=%d).\n",
        dev_id, grp_id, thread_id, event->event_id, event->subevent_id, event->pid, event->dst_engine);
    sched_run_info("show trace info. (src_submit_user=%llu; src_submit_kernel=%llu; dst_publish=%llu; "
                   "dst_wait_start=%llu; dst_wait_end=%llu;"
                   " dst_submit_usr=%llu; dst_rsp_submit_kernel=%llu; src_publish=%llu; src_wait_start=%llu; "
                   "src_wait_end=%llu).\n",
                   para.trace.src_submit_user_timestamp, para.trace.src_submit_kernel_timestamp,
                   para.trace.dst_publish_timestamp, para.trace.dst_wait_start_timestamp,
                   para.trace.dst_wait_end_timestamp, para.trace.dst_submit_user_timestamp,
                   para.trace.dst_submit_kernel_timestamp, para.trace.src_publish_timestamp,
                   para.trace.src_wait_start_timestamp, para.trace.src_wait_end_timestamp);

    return;
}

drvError_t halEschedRegisterFinishFunc(unsigned int grpId, unsigned int event_id,
                                       void (*finishFunc)(unsigned int devId, unsigned int grpId, unsigned int event_id,
                                                          unsigned int subevent_id))
{
    if ((grpId >= SCHED_MAX_GRP_NUM) || (event_id >= EVENT_MAX_NUM)) {
        sched_err("The value of grpId or event_id is out of range. (grpId=%u; max=%u; event_id=%u; max=%u)\n", grpId,
                  (SCHED_MAX_GRP_NUM - 1), event_id, (EVENT_MAX_NUM - 1));
        return DRV_ERROR_SCHED_PARA_ERR;
    }

    esched_finish_func[grpId][event_id] = finishFunc;

    return DRV_ERROR_NONE;
}

drvError_t esched_register_finish_func_ex(unsigned int grp_id, unsigned int event_id,
                                          void (*finish_func)(unsigned int dev_id, unsigned int grp_id,
                                                              esched_event_info event_info))
{
    if ((grp_id >= SCHED_MAX_GRP_NUM) || (event_id >= EVENT_MAX_NUM)) {
        sched_err("The value of grp_id or event_id is out of range. (grp_id=%u; max=%u; event_id=%u; max=%u)\n", grp_id,
                  (SCHED_MAX_GRP_NUM - 1), event_id, (EVENT_MAX_NUM - 1));
        return DRV_ERROR_SCHED_PARA_ERR;
    }

    esched_finish_func_ex[grp_id][event_id] = finish_func;

    return DRV_ERROR_NONE;
}

static inline bool esched_thread_info_is_match(esched_thread_info *thread_info, unsigned int dev_id,
                                               unsigned int grp_id, unsigned int thread_id)
{
    return ((thread_info->dev_id == dev_id) && (thread_info->gid == grp_id) && (thread_info->tid == thread_id));
}

static inline void esched_save_thread_info(esched_thread_wait_info *thread_wait_info, uint32_t dev_id, uint32_t grp_id,
                                           uint32_t thread_id)
{
    thread_wait_info->thread_info.dev_id = dev_id;
    thread_wait_info->thread_info.gid = grp_id;
    thread_wait_info->thread_info.tid = thread_id;
}

static inline esched_thread_wait_info_head *esched_get_thread_wait_info_list_head(void)
{
    if (esched_thread_key_flag == false) {
        sched_err("No esched_thread_key was created!\n");
        return NULL;
    }

    return (esched_thread_wait_info_head *)pthread_getspecific(esched_thread_key);
}

static inline void esched_update_cur_thread_info(uint32_t grp_id, uint32_t thread_id)
{
    esched_thread_wait_info_head *wait_info_head = esched_get_thread_wait_info_list_head();
    if (wait_info_head == NULL) {
        return;
    }

    wait_info_head->cur_thread_info.gid = grp_id;
    wait_info_head->cur_thread_info.tid = thread_id;
}

static inline unsigned int esched_get_cur_group_id(void)
{
    esched_thread_wait_info_head *wait_info_head = esched_get_thread_wait_info_list_head();
    if (wait_info_head == NULL) {
        return SCHED_INVALID_GID;
    }

    return wait_info_head->cur_thread_info.gid;
}

static inline unsigned int esched_get_cur_thread_id(void)
{
    esched_thread_wait_info_head *wait_info_head = esched_get_thread_wait_info_list_head();
    if (wait_info_head == NULL) {
        return SCHED_INVALID_TID;
    }

    return wait_info_head->cur_thread_info.tid;
}

static void esched_create_thread_wait_info_list_head(void)
{
    esched_thread_wait_info_head *wait_info_head = NULL;
    int ret;

    if (esched_thread_key_flag == false) {
        sched_err("No esched_thread_key was created!\n");
        return;
    }

    wait_info_head = (esched_thread_wait_info_head *)malloc(sizeof(esched_thread_wait_info_head));
    if (wait_info_head == NULL) {
        sched_err("malloc list_head failed.\n");
        return;
    }

    INIT_LIST_HEAD(&wait_info_head->list_head);
    wait_info_head->cur_thread_info.gid = SCHED_INVALID_GID;
    wait_info_head->cur_thread_info.tid = SCHED_INVALID_TID;
    ret = pthread_setspecific(esched_thread_key, (void *)wait_info_head);
    if (ret != 0) {
        sched_err("set esched_thread_key specific failed.\n");
        free(wait_info_head);
        return;
    }
}

static void esched_thread_destructor(void *key)
{
    esched_thread_wait_info_head *wait_info_head = NULL;
    struct list_head *pos = NULL, *n = NULL;
    esched_thread_wait_info *thread_wait_info = NULL;

    wait_info_head = (esched_thread_wait_info_head *)key;
    if (wait_info_head == NULL) {
        return;
    }

    list_for_each_safe(pos, n, &wait_info_head->list_head)
    {
        thread_wait_info = list_entry(pos, esched_thread_wait_info, list);
        drv_user_list_del(&thread_wait_info->list);
        sched_debug("Free thread wait info node(dev_id=%u; gid=%u; tid=%u).\n", thread_wait_info->thread_info.dev_id,
                    thread_wait_info->thread_info.gid, thread_wait_info->thread_info.tid);
        free(thread_wait_info);
    }

    free(wait_info_head);
}

static void esched_create_thread_key_once(void)
{
    int ret;

    ret = pthread_key_create(&esched_thread_key, esched_thread_destructor);
    if (ret != 0) {
        sched_err("pthread_key_create failed. (ret=%d)\n", ret);
        return;
    }

    esched_thread_key_flag = true;
}

/* Findout the same "devid + grpid + thread_id" list entry. */
STATIC esched_thread_wait_info *esched_find_wait_info(unsigned int dev_id, unsigned int grp_id, unsigned int thread_id)
{
    esched_thread_wait_info_head *wait_info_head = NULL;
    struct list_head *pos = NULL, *n = NULL;
    esched_thread_wait_info *thread_wait_info = NULL;

    wait_info_head = esched_get_thread_wait_info_list_head();
    if (wait_info_head == NULL) {
        return NULL;
    }

    list_for_each_safe(pos, n, &wait_info_head->list_head)
    {
        thread_wait_info = list_entry(pos, esched_thread_wait_info, list);
        if (esched_thread_info_is_match(&thread_wait_info->thread_info, dev_id, grp_id, thread_id) == true) {
            return thread_wait_info;
        }
    }

    return NULL;
}

STATIC esched_thread_wait_info *esched_create_wait_info(unsigned int dev_id, unsigned int grp_id,
                                                        unsigned int thread_id)
{
    esched_thread_wait_info_head *wait_info_head = NULL;
    esched_thread_wait_info *thread_wait_info = NULL;

    wait_info_head = esched_get_thread_wait_info_list_head();
    if (wait_info_head == NULL) {
        sched_err("No wait info list.\n");
        return NULL;
    }

    thread_wait_info = (esched_thread_wait_info *)malloc(sizeof(esched_thread_wait_info));
    if (thread_wait_info == NULL) {
        sched_err("malloc esched_thread_wait_info failed.\n");
        return NULL;
    }

    esched_save_thread_info(thread_wait_info, dev_id, grp_id, thread_id);
    drv_user_list_add_head(&thread_wait_info->list, &wait_info_head->list_head);
    sched_debug("Add thread wait info node(dev_id=%u; gid=%u; tid=%u).\n", dev_id, grp_id, thread_id);
    return thread_wait_info;
}

/* Findout and call the same "devid + grpid + thread_id" finish func. */
STATIC void esched_finish_call_back(unsigned int dev_id, unsigned int grp_id, unsigned int thread_id)
{
    esched_thread_wait_info *thread_wait_info;

    thread_wait_info = esched_find_wait_info(dev_id, grp_id, thread_id);
    if (thread_wait_info == NULL) {
        return; /* It's probably no wait before. */
    }

    if (thread_wait_info->event_valid == 0) {
        return; /* No need to call finish func. */
    }

    /* Only valid parameters are saved in the wait info, so the gid and event_id found in the list must be valid. */
    if (esched_finish_func[grp_id][thread_wait_info->event_info.event_id] != NULL) {
        esched_finish_func[grp_id][thread_wait_info->event_info.event_id](
            dev_id, grp_id, thread_wait_info->event_info.event_id, thread_wait_info->event_info.subevent_id);
    }
    if (esched_finish_func_ex[grp_id][thread_wait_info->event_info.event_id] != NULL) {
        esched_finish_func_ex[grp_id][thread_wait_info->event_info.event_id](dev_id, grp_id,
                                                                             thread_wait_info->event_info);
    }

    /* Update the last event info invalid. */
    thread_wait_info->event_valid = 0;

    sched_debug("Called the finish func.(dev_id=%u; gid=%u; tid=%u; event_id=%u; subevent_id=%u).\n", dev_id, grp_id,
                thread_id, thread_wait_info->event_info.event_id, thread_wait_info->event_info.subevent_id);
}

static void esched_save_wait_info(uint32_t dev_id, uint32_t thread_id, struct sched_ioctl_para_wait para)
{
    esched_thread_wait_info *thread_wait_info = NULL;

    esched_update_cur_thread_info(para.event.gid, thread_id);

    thread_wait_info = esched_find_wait_info(dev_id, para.event.gid, thread_id);
    if (thread_wait_info == NULL) {
        /* Create new wait info when first time wait by this thread info. */
        thread_wait_info = esched_create_wait_info(dev_id, para.event.gid, thread_id);
        if (thread_wait_info == NULL) {
            sched_err("Create new wait info failed. (dev_id=%u; grp_id=%u; tid=%u)\n", dev_id, para.event.gid,
                      thread_id);
            return;
        }
    }

    thread_wait_info->event_info.event_id = para.event.event_id;
    thread_wait_info->event_info.subevent_id = para.event.subevent_id;
    thread_wait_info->event_info.msg_len = para.event.msg_len;
    (void)memcpy_s(thread_wait_info->event_info.msg, EVENT_MAX_MSG_LEN, para.input.msg, para.event.msg_len);
    thread_wait_info->event_valid = 1;
}

#if (!defined(USER_EVENT_SCHED_UT)) && (!defined(EMU_ST))
drvError_t register_esched_trace_record_func(unsigned int grp_id, unsigned int event_id,
                                             void (*finish_func)(unsigned int grp_id, unsigned int event_id,
                                                                 unsigned int subevent_id,
                                                                 sched_trace_time_info *sched_trace_time_info))
{
    if ((grp_id >= SCHED_MAX_GRP_NUM) || (event_id >= EVENT_MAX_NUM)) {
        sched_err("The value of grp_id or event_id is out of range. (grp_id=%u; max=%u; event_id=%u; max=%u)\n", grp_id,
                  (SCHED_MAX_GRP_NUM - 1), event_id, (EVENT_MAX_NUM - 1));
        return DRV_ERROR_SCHED_PARA_ERR;
    }

    esched_trace_record_func[grp_id][event_id] = finish_func;
    return DRV_ERROR_NONE;
}
#endif
STATIC void esched_trace_record_call_back(unsigned int grp_id, struct sched_subscribed_event *event,
                                          uint64_t wait_start_timestamp)
{
#if (!defined(USER_EVENT_SCHED_UT)) && (!defined(EMU_ST))
    sched_trace_time_info sched_trace_time_info_ins;
    unsigned int event_id = event->event_id;
    unsigned int subevent_id = event->subevent_id;

    if (event_id != EVENT_QUEUE_ENQUEUE) {
        return;
    }

    if (grp_id >= SCHED_MAX_GRP_NUM) {
        sched_err("Invalid param. (grp_id=%u)\n", grp_id);
        return;
    }

    if (esched_trace_record_func[grp_id][event_id] == NULL) {
        return;
    }

    sched_trace_time_info_ins.time_stamp[SCHED_TRACE_TIME_THREAD_WAIT] = wait_start_timestamp;
    sched_trace_time_info_ins.time_stamp[SCHED_TRACE_TIME_EVENT_PUBLISH_USER] = event->publish_user_timestamp;
    sched_trace_time_info_ins.time_stamp[SCHED_TRACE_TIME_EVENT_PUBLISH_KERNEL] = event->publish_kernel_timestamp;
    sched_trace_time_info_ins.time_stamp[SCHED_TRACE_TIME_EVENT_PUBLISH_FINISH] = event->publish_finish_timestamp;
    sched_trace_time_info_ins.time_stamp[SCHED_TRACE_TIME_PUBLISH_WAKEUP] = event->publish_wakeup_timestamp;
    sched_trace_time_info_ins.time_stamp[SCHED_TRACE_TIME_THREAD_WAKED] = event->subscribe_timestamp;

    esched_trace_record_func[grp_id][event_id](grp_id, event_id, subevent_id, &sched_trace_time_info_ins);
#endif
}

#ifdef CFG_FEATURE_EXTERNAL_CDEV

static int32_t esched_ex_dev_open(unsigned int dev_id, int fd)
{
    struct davinci_intf_open_arg arg;
    int32_t ret;

    ret = uda_get_udevid_by_devid_ex(dev_id, (unsigned int *)&arg.device_id);
    if (ret != DRV_ERROR_NONE) {
        sched_err("Get phys failed. (devid=%u)\n", dev_id);
        return DRV_ERROR_INVALID_DEVICE;
    }

    ret = strcpy_s(arg.module_name, DAVINIC_MODULE_NAME_MAX, DAVINCI_ESCHED_SUB_MODULE_NAME);
    if (ret < 0) {
        sched_err("Failed to invoke the strcpy_s. (ret=%d).\n", ret);
        return ret;
    }

    ret = esched_ioctl(fd, DAVINCI_INTF_IOCTL_OPEN, &arg);
    if (ret != 0) {
        sched_err("Failed to invoke the INTF_IOCTL_OPEN. (ret=%d)\n", ret);
        return ret;
    }

    return 0;
}

static void esched_ex_dev_close(unsigned int dev_id, int fd)
{
    struct davinci_intf_open_arg arg;
    int32_t ret;

    ret = uda_get_udevid_by_devid_ex(dev_id, (unsigned int *)&arg.device_id);
    if (ret != DRV_ERROR_NONE) {
        sched_err("Get phys failed. (devid=%u)\n", dev_id);
        return;
    }

    ret = strcpy_s(arg.module_name, DAVINIC_MODULE_NAME_MAX, DAVINCI_ESCHED_SUB_MODULE_NAME);
    if (ret < 0) {
        sched_err("Failed to invoke the strcpy_s. (ret=%d).\n", ret);
        return;
    }

    ret = esched_ioctl(fd, DAVINCI_INTF_IOCTL_CLOSE, &arg);
    if (ret != 0) {
        sched_err("Failed to invoke the INTF_IOCTL_CLOSE. (ret=%d)\n", ret);
    }
}
#else
void esched_init_all_fd(int fd)
{
    int i;

    for (i = 0; i < ESCHED_DEV_NUM; i++) {
        sched_dev_fd[i] = fd;
    }
}
void esched_uninit_all_fd(void)
{
    int i;

    for (i = 0; i < ESCHED_DEV_NUM; i++) {
        sched_dev_fd[i] = SCHED_DEV_FD_CLOSED;
    }
}

#endif

STATIC int32_t esched_dev_init_locked(unsigned int dev_id, int fd)
{
    int32_t ret;

#ifdef CFG_FEATURE_EXTERNAL_CDEV
    ret = esched_ex_dev_open(dev_id, fd);
    if (ret != 0) {
        esched_close(fd);
        return ret;
    }
#endif

    ret = esched_dev_feature_init_all(fd);
    if (ret != DRV_ERROR_NONE) {
        sched_err("Failed to init device features.(dev_id=%u)\n", dev_id);
#ifdef CFG_FEATURE_EXTERNAL_CDEV
        esched_ex_dev_close(dev_id, fd);
#endif
        (void)esched_close(fd);
        return ret;
    }

    ret = esched_init_sched_cpu_num(dev_id, fd);
    if (ret != 0) {
        sched_err("Failed to init sched cpu num.(dev_id=%u)\n", dev_id);
#ifdef CFG_FEATURE_EXTERNAL_CDEV
        esched_ex_dev_close(dev_id, fd);
#endif
        (void)esched_close(fd);
        return ret;
    }

#ifdef CFG_FEATURE_EXTERNAL_CDEV
    sched_dev_fd[dev_id] = fd;
#else
    esched_init_all_fd(fd);
#endif
    return DRV_ERROR_NONE;
}

int32_t esched_dev_init(unsigned int dev_id)
{
    int32_t ret;
    int fd;

    if (esched_device_check(dev_id) != 0) {
        sched_err("The dev_id is invalid. (dev_id=%u; max=%u)\n", dev_id, ESCHED_LOGIC_DEV_NUM);
        return DRV_ERROR_INVALID_DEVICE;
    }

    (void)pthread_mutex_lock(&g_esched_init_mutex);
    if (sched_dev_fd[dev_id] == SCHED_DEV_FD_CLOSED) {
        (void)pthread_mutex_unlock(&g_esched_init_mutex);
        return DRV_ERROR_SCHED_PROCESS_EXIT;
    }

    if (sched_dev_fd[dev_id] > 0) {
        (void)pthread_mutex_unlock(&g_esched_init_mutex);
        return 0;
    }

    fd = esched_open(SCHED_CHAR_DEV_FULL_NAME);
    if (fd < 0) {
        sched_err("Failed to open device. (ret=%d)\n", errno);
        (void)pthread_mutex_unlock(&g_esched_init_mutex);
        return DRV_ERROR_INVALID_DEVICE;
    }

    ret = esched_dev_init_locked(dev_id, fd);
    (void)pthread_mutex_unlock(&g_esched_init_mutex);
    return ret;
}

STATIC void esched_dev_close(unsigned int dev_id)
{
    if (sched_dev_fd[dev_id] <= 0) {
        return;
    }

#ifdef CFG_FEATURE_EXTERNAL_CDEV
    esched_ex_dev_close(dev_id, sched_dev_fd[dev_id]);
#endif
    (void)esched_close(sched_dev_fd[dev_id]);

#ifdef CFG_FEATURE_EXTERNAL_CDEV
    sched_dev_fd[dev_id] = SCHED_DEV_FD_CLOSED;
#else
    esched_uninit_all_fd();
#endif
}

int esched_dev_ioctl(unsigned int dev_id, unsigned int cmd, void *para)
{
    int ret;
    int fd;

    while (1) {
        ret = esched_dev_init(dev_id);
        if (ret != 0) {
            return ret;
        }

        fd = sched_dev_fd[dev_id];
        if (fd == 0) {
            continue;
        }

        ret = esched_ioctl(fd, cmd, para);
        if (ret == DRV_ERROR_FILE_OPS) {
            esched_init_global_fork(dev_id, fd);
            continue;
        }
        if (ret != 0) {
            esched_share_log_read();
        }

        break;
    }

    return ret;
}

drvError_t halEschedAttachDevice(uint32_t devId)
{
    struct sched_ioctl_para_attach para_attach;
    int ret;
    ret = esched_attach_device_inner(devId, &para_attach);
    if (ret == DRV_ERROR_NONE) {
        (void)pthread_once(&esched_thread_key_init, esched_create_thread_key_once);
    }

    return ret;
}

drvError_t halEschedDettachDevice(uint32_t devId)
{
    struct sched_ioctl_para_detach para_detach;
    int ret;

    ret = esched_dettach_device_inner(devId, &para_detach);
    return ret;
}

static drvError_t esched_wait_event_comm(esched_thread_info *wait_info, int32_t timeout, struct event_info *event,
                                         esched_event_buffer *event_buffer)
{
    drvError_t ret;
    struct sched_ioctl_para_wait para;
    uint64_t wait_start_timestamp = esched_get_cur_cpu_timestamp();
    esched_thread_wait_info_head *wait_info_head = NULL;

    wait_info_head = esched_get_thread_wait_info_list_head();
    if (wait_info_head == NULL) {
        esched_create_thread_wait_info_list_head();
    }

    esched_finish_call_back(wait_info->dev_id, wait_info->gid, wait_info->tid);

    para.input.dev_id = wait_info->dev_id;
    para.input.grp_id = wait_info->gid;
    para.input.thread_id = wait_info->tid;
    para.input.timeout = (timeout < 0) ? SCHED_WAIT_TIMEOUT_ALWAYS_WAIT : timeout;
    para.input.msg_len = event_buffer->msg_len;
    para.input.msg = event_buffer->msg;

    do {
        ret = esched_dev_ioctl(wait_info->dev_id, SCHED_WAIT_EVENT_ID, &para);
    } while (ret == DRV_ERROR_WAIT_INTERRUPT);

    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    event->comm.pid = para.event.pid;
    event->comm.host_pid = para.event.host_pid;
    event->comm.grp_id = para.event.gid;
    event->comm.event_id = (EVENT_ID)para.event.event_id;
    event->comm.subevent_id = para.event.subevent_id;
    event->priv.msg_len = para.event.msg_len;
    event->comm.submit_timestamp = para.event.publish_user_timestamp;
    event->comm.sched_timestamp = para.event.subscribe_timestamp;

    esched_save_wait_info(wait_info->dev_id, wait_info->tid, para);

    esched_trace_record_call_back(wait_info->gid, &para.event, wait_start_timestamp);

    return DRV_ERROR_NONE;
}

drvError_t esched_wait_event_ex(uint32_t dev_id, uint32_t grp_id, uint32_t thread_id, int32_t timeout,
                                struct event_info *event)
{
    esched_thread_info wait_info = {dev_id, grp_id, thread_id};
    esched_event_buffer *event_buffer = NULL;

    if (event == NULL) {
        sched_err("Event is NULL. (dev_id=%u; grp_id=%u; thread_id=%u)\n", dev_id, grp_id, thread_id);
        return DRV_ERROR_PARA_ERROR;
    }

    event_buffer = (esched_event_buffer *)event->priv.msg;
    return esched_wait_event_comm(&wait_info, timeout, event, event_buffer);
}

drvError_t halEschedWaitEvent(uint32_t devId, uint32_t grpId, uint32_t threadId, int32_t timeout,
                              struct event_info *event)
{
    esched_thread_info wait_info = {devId, grpId, threadId};
    esched_event_buffer event_buffer;

    if (event == NULL) {
        sched_err("Event is NULL. (devId=%u; grpId=%u; threadId=%u)\n", devId, grpId, threadId);
        return DRV_ERROR_PARA_ERROR;
    }

    event_buffer.msg_len = EVENT_MAX_MSG_LEN;
    event_buffer.msg = event->priv.msg;
    return esched_wait_event_comm(&wait_info, timeout, event, &event_buffer);
}

static drvError_t esched_thread_swapout_common(unsigned int dev_id, unsigned int grp_id, unsigned int thread_id,
                                               int timeout)
{
    struct sched_ioctl_para_wait para = {0};
    drvError_t ret;
    unsigned int grp_id_ = grp_id;
    unsigned int thread_id_ = thread_id;
    GROUP_TYPE type;

    if (esched_need_judge_thread_id()) {
        if (esched_get_cur_thread_id() == SCHED_INVALID_TID) {
            return DRV_ERROR_NONE;
        }
    }

    if ((grp_id_ == SCHED_INVALID_GID) && (thread_id_ == SCHED_INVALID_TID)) {
        grp_id_ = esched_get_cur_group_id();
        thread_id_ = esched_get_cur_thread_id();
    }

    para.input.dev_id = dev_id;
    para.input.grp_id = grp_id_;
    para.input.thread_id = thread_id_;
    para.input.timeout = timeout;

    esched_finish_call_back(dev_id, grp_id_, thread_id_);

    ret = esched_query_grp_type(dev_id, grp_id_, &type);
    if (ret != 0) {
        sched_err("Query grp type failed. (dev_id=%u; grp_id=%u; thread_id=%u)\n", dev_id, grp_id_, thread_id_);
        return DRV_ERROR_PARA_ERROR;
    }

    /* non sched cpu not need swap out in kernel. */
    if (type == GRP_TYPE_BIND_CP_CPU) {
        esched_update_cur_thread_info(SCHED_INVALID_GID, SCHED_INVALID_TID);
        return DRV_ERROR_NONE;
    }

    ret = esched_dev_ioctl(dev_id, SCHED_WAIT_EVENT_ID, &para);
    ret = (ret == DRV_ERROR_NO_EVENT) ? 0 : ret;
    if (ret == 0) {
        esched_update_cur_thread_info(SCHED_INVALID_GID, SCHED_INVALID_TID);
    }

    return ret;
}

drvError_t halEschedThreadSwapout(unsigned int devId, unsigned int grpId, unsigned int threadId)
{
    drvError_t ret;
    if (esched_support_thread_swap_out()) {
        ret = esched_thread_swapout_common(devId, grpId, threadId, SCHED_WAIT_TIMEOUT_SWAPOUT);
        return ret;
    } else {
        return DRV_ERROR_NOT_SUPPORT;
    }
}

drvError_t halEschedThreadGiveup(unsigned int devId, unsigned int grpId, unsigned int threadId)
{
    drvError_t ret;
    if (esched_support_thread_giveup()) {
        ret = esched_thread_swapout_common(devId, grpId, threadId, SCHED_WAIT_TIMEOUT_GIVEUP);
        return ret;
    } else {
        return DRV_ERROR_NOT_SUPPORT;
    }
}

drvError_t halEschedGetEvent(uint32_t devId, uint32_t grpId, uint32_t threadId, EVENT_ID eventId,
                             struct event_info *event)
{
    drvError_t ret;
    struct sched_ioctl_para_get_event para;

    if (event == NULL) {
        sched_err("The variable event is NULL. (devId=%u; grpId=%u; threadId=%u)\n", devId, grpId, threadId);
        return DRV_ERROR_PARA_ERROR;
    }

    para.input.dev_id = devId;
    para.input.grp_id = grpId;
    para.input.thread_id = threadId;
    para.input.event_id = (uint32_t)eventId;
    para.input.msg_len = EVENT_MAX_MSG_LEN;
    para.input.msg = event->priv.msg;

    ret = esched_dev_ioctl(devId, SCHED_GET_EXACT_EVENT_ID, &para);
    if (ret != 0) {
        return ret;
    }

    event->comm.pid = para.event.pid;
    event->comm.host_pid = -1;
    event->comm.grp_id = para.event.gid;
    event->comm.event_id = (EVENT_ID)para.event.event_id;
    event->comm.subevent_id = para.event.subevent_id;
    event->priv.msg_len = para.event.msg_len;
    event->comm.submit_timestamp = para.event.publish_user_timestamp;
    event->comm.sched_timestamp = esched_get_cur_cpu_timestamp();

    return DRV_ERROR_NONE;
}

STATIC drvError_t esched_submit_event_comm(uint32_t dev_id, struct event_summary *event, unsigned int tid,
                                           unsigned int dst_dev_id, unsigned int *event_num)
{
    struct sched_ioctl_para_submit para;
    struct timespec publish_timestamp;
    int ret;

    para.dev_id = dev_id;
    para.event_info.dst_engine = event->dst_engine;
    para.event_info.policy = (uint32_t)event->policy;
    para.event_info.pid = event->pid;
    para.event_info.gid = event->grp_id;
    para.event_info.tid = tid;
    para.event_info.event_id = (uint32_t)event->event_id;
    para.event_info.subevent_id = event->subevent_id;
    para.event_info.dst_devid = dst_dev_id;
    para.event_info.msg_len = event->msg_len;
    para.event_info.msg = event->msg;
    para.event_info.event_num = *event_num;
    para.event_info.publish_timestamp = esched_get_cur_cpu_timestamp();
    (void)clock_gettime(CLOCK_MONOTONIC, &publish_timestamp);
    para.event_info.publish_timestamp_of_day = (uint64_t)((publish_timestamp.tv_sec * USEC_PER_SEC) +
                                                          (publish_timestamp.tv_nsec / NSEC_PER_USEC));

    ret = esched_dev_ioctl(dev_id, SCHED_SUBMIT_EVENT_ID, &para);
    *event_num = para.event_info.event_num;
    return ret;
}

drvError_t halEschedSubmitEvent(uint32_t devId, struct event_summary *event)
{
    unsigned int event_num = 1;

    if (event == NULL) {
        sched_err("The variable event is NULL. (devId=%u)\n", devId);
        return DRV_ERROR_PARA_ERROR;
    }

    return esched_submit_event_comm(devId, event, SCHED_INVALID_TID, SCHED_INVALID_DEVID, &event_num);
}

static int esched_check_rsv_data(int *rsv_data, unsigned int len)
{
    unsigned int i;

    for (i = 0; i < len; i++) {
        if (rsv_data[i] != 0) {
            sched_err("The rsv data is invalid. (rsvIdx=%u)\n", i);
            return DRV_ERROR_PARA_ERROR;
        }
    }

    return DRV_ERROR_NONE;
}

static int esched_check_event_rsv_data(struct event_summary *events, unsigned int event_num)
{
    unsigned int i;
    int ret;

    for (i = 0; i < event_num; i++) {
        ret = esched_check_rsv_data(events[i].rsv, EVENT_SUMMARY_RSV);
        if (ret != DRV_ERROR_NONE) {
            sched_err("The rsv data is invalid. (event_idx=%u)\n", i);
            return ret;
        }
    }

    return DRV_ERROR_NONE;
}

DLLEXPORT drvError_t halEschedSubmitEventBatch(unsigned int devId, SUBMIT_FLAG flag, struct event_summary *events,
                                               unsigned int event_num, unsigned int *succ_event_num)
{
    int ret;

    if (!esched_support_extern_interface()) {
        sched_info("Not support yet.\n");
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (events == NULL) {
        sched_err("The variable events is NULL. (devId=%u)\n", devId);
        return DRV_ERROR_PARA_ERROR;
    }

    if (succ_event_num == NULL) {
        sched_err("The variable success_event_num is NULL. (devId=%u)\n", devId);
        return DRV_ERROR_PARA_ERROR;
    }

    if (flag != SHARED_EVENT_ENTRY) {
#ifndef EMU_ST
        sched_warn("The flag is not supported. (flag=%d)\n", flag);
#endif
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (flag == SHARED_EVENT_ENTRY) {
        ret = esched_check_event_rsv_data(events, 1);
    } else {
        ret = esched_check_event_rsv_data(events, event_num);
    }
    if (ret != 0) {
        sched_err("Failed to invoke the esched_check_event_rsv_data. (ret=%d)\n", ret);
        return ret;
    }

    *succ_event_num = event_num;
    return esched_submit_event_comm(devId, events, SCHED_INVALID_TID, SCHED_INVALID_DEVID, succ_event_num);
}

drvError_t halEschedSubmitEventToThread(uint32_t devId, struct event_summary *event)
{
    unsigned int event_num = 1;

    if (!esched_support_extern_interface()) {
        return DRV_ERROR_NOT_SUPPORT;
    }
    if (event == NULL) {
        sched_err("The variable event is NULL. (devId=%u)\n", devId);
        return DRV_ERROR_PARA_ERROR;
    }

    if (event->tid == SCHED_INVALID_TID) {
        sched_err("User specific tid not support. (tid=%u)\n", SCHED_INVALID_TID);
        return DRV_ERROR_PARA_ERROR;
    }

    return esched_submit_event_comm(devId, event, event->tid, SCHED_INVALID_DEVID, &event_num);
}

drvError_t halEschedSubmitEventEx(uint32_t devId, uint32_t dstDevId, struct event_summary *event)
{
    unsigned int event_num = 1;

    if (event == NULL) {
        sched_err("The variable event is NULL. (devId=%u)\n", devId);
        return DRV_ERROR_PARA_ERROR;
    }

    return esched_submit_event_comm(devId, event, event->tid, dstDevId, &event_num);
}

drvError_t halEschedRegisterAckFunc(unsigned int grpId, EVENT_ID eventId,
                                    void (*ackFunc)(unsigned int devId, unsigned int subevent_id, char *msg,
                                                    unsigned int msgLen))
{
    if (!esched_support_extern_interface()) {
        sched_info("Not support yet.\n");
        return DRV_ERROR_NOT_SUPPORT;
    }

    if ((grpId >= SCHED_MAX_GRP_NUM) || (eventId >= EVENT_MAX_NUM)) {
        sched_err("The value of grpId or event_id is out of range. (grpId=%u; max=%u; event_id=%u; max=%u)\n", grpId,
                  (SCHED_MAX_GRP_NUM - 1), (unsigned int)eventId, (EVENT_MAX_NUM - 1));
        return DRV_ERROR_SCHED_PARA_ERR;
    }

    esched_ack_func[grpId][eventId] = ackFunc;

    return DRV_ERROR_NONE;
}

drvError_t halEschedAckEvent(uint32_t devId, EVENT_ID eventId, uint32_t subeventId, char *msg, uint32_t msgLen)
{
    struct sched_ioctl_para_ack para;
    unsigned int cur_grp_id = esched_get_cur_group_id();

    if (eventId >= EVENT_MAX_NUM) {
        return DRV_ERROR_PARA_ERROR;
    }

    if ((cur_grp_id != SCHED_INVALID_GID) && (esched_ack_func[cur_grp_id][eventId] != NULL)) {
        esched_ack_func[cur_grp_id][eventId](devId, subeventId, msg, msgLen);
        return DRV_ERROR_NONE;
    }

    para.dev_id = devId;
    para.event_id = (uint32_t)eventId;
    para.subevent_id = subeventId;
    para.msg_len = msgLen;
    para.msg = msg;

    return esched_dev_ioctl(devId, SCHED_ACK_EVENT_ID, &para);
}

drvError_t halEschedSubscribeEvent(uint32_t devId, uint32_t grpId, uint32_t threadId, u64 eventBitmap)
{
    struct sched_ioctl_para_subscribe para;

    para.dev_id = devId;
    para.gid = grpId;
    para.tid = threadId;
    para.event_bitmap = eventBitmap;

    return esched_dev_ioctl(devId, SCHED_THREAD_SUBSCRIBE_EVENT_ID, &para);
}

drvError_t halEschedSetGrpEventQos(unsigned int devId, unsigned int grpId, EVENT_ID eventId,
                                   struct event_sched_grp_qos *qos)
{
    struct sched_ioctl_para_set_event_max_num para;

    if (qos == NULL) {
        sched_err("The variable qos is NULL. (devId=%u; grpId=%u; eventId=%u)\n", devId, grpId, (unsigned int)eventId);
        return DRV_ERROR_PARA_ERROR;
    }

    para.dev_id = devId;
    para.gid = grpId;
    para.event_id = (uint32_t)eventId;
    para.max_num = qos->maxNum;

    return esched_dev_ioctl(devId, SCHED_GRP_SET_EVENT_MAX_NUM, &para);
}

static int esched_create_grp(uint32_t dev_id, unsigned int grp_id, struct esched_grp_para *grp_para)
{
    struct sched_ioctl_para_add_grp para;
    int ret;

    para.dev_id = dev_id;
    para.gid = grp_id;
    para.sched_mode = (uint32_t)grp_para->type;
    para.thread_num = grp_para->threadNum;
    ret = strcpy_s(para.grp_name, EVENT_MAX_GRP_NAME_LEN, grp_para->grp_name);
    if (ret != 0) {
        sched_err("Failed to invoke strcpy_s. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_INNER_ERR;
    }

    ret = esched_dev_ioctl(dev_id, SCHED_PROC_ADD_GRP_ID, &para);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    sched_grp[dev_id].info[grp_id].gid = grp_id;
    sched_grp[dev_id].info[grp_id].type = grp_para->type;
    sched_info("Esched create grp success. (dev_id=%u; gid=%u; sched_mode=%u; grpName=%s)\n", dev_id, grp_id,
               para.sched_mode, para.grp_name);

    return ret;
}

drvError_t esched_create_extend_grp(uint32_t dev_id, unsigned int grp_id, struct esched_grp_para *grp_para)
{
    if (grp_id >= SCHED_MAX_DEFAULT_GRP_NUM) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    (void)memset_s(grp_para->grp_name, EVENT_MAX_GRP_NAME_LEN, 0, EVENT_MAX_GRP_NAME_LEN);

    return esched_create_grp(dev_id, grp_id, grp_para);
}

static void esched_grp_name_print(uint32_t dev_id)
{
    unsigned int i;

    for (i = SCHED_MAX_DEFAULT_GRP_NUM; i < SCHED_MAX_GRP_NUM; i++) {
        if (sched_grp[dev_id].info[i].type == GRP_TYPE_UNINIT) {
            break;
        }

        sched_info("Show detail. (gid=%u; grpName=%s)\n", sched_grp[dev_id].info[i].gid,
                   sched_grp[dev_id].info[i].grp_name);
    }
}

static int esched_check_grp_name(uint32_t dev_id, const char *name)
{
    unsigned int i;

    for (i = SCHED_MAX_DEFAULT_GRP_NUM; i < SCHED_MAX_GRP_NUM; i++) {
        if (sched_grp[dev_id].info[i].type == GRP_TYPE_UNINIT) {
            break;
        }

        if (strcmp(name, sched_grp[dev_id].info[i].grp_name) == 0) {
            sched_err("Grp name reused. (grp_name=%s; i=%u)\n", name, i);
            esched_grp_name_print(dev_id);
            return DRV_ERROR_REPEATED_USERD;
        }
    }

    return 0;
}

static int esched_alloc_grp_id(uint32_t dev_id, uint32_t *gid)
{
    unsigned int i;

    for (i = SCHED_MAX_DEFAULT_GRP_NUM; i < SCHED_MAX_GRP_NUM; i++) {
        if (sched_grp[dev_id].info[i].type == GRP_TYPE_UNINIT) {
            *gid = i;
            return 0;
        }
    }

    return DRV_ERROR_NO_GROUP;
}

drvError_t halEschedCreateGrp(uint32_t devId, uint32_t grpId, GROUP_TYPE type)
{
    struct esched_grp_para grp_para;

    if (esched_device_check(devId) != 0) {
        sched_err("devId is invalid.(devId=%u;max=%u;)\n", devId, ESCHED_LOGIC_DEV_NUM);
        return DRV_ERROR_PARA_ERROR;
    }
    if (grpId >= SCHED_MAX_DEFAULT_GRP_NUM) {
        sched_err("grpId is invalid.(grpId=%u;max=%u)\n", grpId, SCHED_MAX_DEFAULT_GRP_NUM);
        report_arg_out_of_range(__FUNCTION__, "grpId", grpId, 0, SCHED_MAX_DEFAULT_GRP_NUM);
        return DRV_ERROR_PARA_ERROR;
    }

    grp_para.type = type;
    grp_para.threadNum = SCHED_DEFAULT_THREAD_NUM_IN_GRP;
    (void)memset_s(grp_para.grp_name, EVENT_MAX_GRP_NAME_LEN, 0, EVENT_MAX_GRP_NAME_LEN);

    return esched_create_grp(devId, grpId, &grp_para);
}

drvError_t halEschedCreateGrpEx(uint32_t devId, struct esched_grp_para *grpPara, unsigned int *grpId)
{
    int ret;
    unsigned int alloc_grp_id;

    if ((esched_device_check(devId) != 0) || (grpPara == NULL) || (grpId == NULL)) {
        sched_err("Invalid para. (devId=%u; grpPara=%u; grpId=%u)\n", devId, grpPara == NULL ? 1 : 0,
                  grpId == NULL ? 1 : 0);
        return DRV_ERROR_PARA_ERROR;
    }

    if ((strnlen(grpPara->grp_name, EVENT_MAX_GRP_NAME_LEN) == 0) ||
        (strnlen(grpPara->grp_name, EVENT_MAX_GRP_NAME_LEN) >= EVENT_MAX_GRP_NAME_LEN)) {
        sched_err("Invalid grp name. (grp_name_len=%u)\n",
                  (unsigned int)strnlen(grpPara->grp_name, EVENT_MAX_GRP_NAME_LEN));
        return DRV_ERROR_PARA_ERROR;
    }

    if (esched_check_rsv_data(grpPara->rsv, ESCHED_GRP_PARA_RSV) != 0) {
        sched_err("The rsv data is invalid. (devId=%u)\n", devId);
        return DRV_ERROR_PARA_ERROR;
    }

    (void)pthread_mutex_lock(&(sched_grp[devId].mutex));
    ret = esched_check_grp_name(devId, grpPara->grp_name);
    if (ret) {
        (void)pthread_mutex_unlock(&(sched_grp[devId].mutex));
        return ret;
    }

    ret = esched_alloc_grp_id(devId, &alloc_grp_id);
    if (ret) {
        (void)pthread_mutex_unlock(&(sched_grp[devId].mutex));
        return ret;
    }

    ret = strcpy_s(sched_grp[devId].info[alloc_grp_id].grp_name, EVENT_MAX_GRP_NAME_LEN, grpPara->grp_name);
    if (ret != 0) {
        sched_err("Failed to invoke strcpy_s. (devId=%u)\n", devId);
        (void)pthread_mutex_unlock(&(sched_grp[devId].mutex));
        return DRV_ERROR_INNER_ERR;
    }

    ret = esched_create_grp(devId, alloc_grp_id, grpPara);
    if (ret == 0) {
        *grpId = alloc_grp_id;
    }
    (void)pthread_mutex_unlock(&(sched_grp[devId].mutex));

    return ret;
}

drvError_t halEschedQueryInfo(unsigned int devId, ESCHED_QUERY_TYPE type, struct esched_input_info *inPut,
                              struct esched_output_info *outPut)
{
    struct sched_ioctl_para_query_info para;
    if (esched_device_check(devId) != 0) {
        sched_err("Invalid para.(devId=%u,maxId=%u)\n", devId, ESCHED_LOGIC_DEV_NUM);
        return DRV_ERROR_PARA_ERROR;
    }
    if (type >= QUERY_TYPE_MAX) {
        sched_err("Invalid para.(type=%u)\n", type);
        report_enum_arg_out_of_range(__FUNCTION__, "type", type, ESCHED_QUERY_TYPE_NAME);
        return DRV_ERROR_PARA_ERROR;
    }
    if (inPut == NULL) {
        sched_err("inPut is null.\n");
        report_arg_null_pointer(__FUNCTION__, "inPut");
        return DRV_ERROR_PARA_ERROR;
    }
    if (outPut == NULL) {
        sched_err("outPut is null.\n");
        report_arg_null_pointer(__FUNCTION__, "outPut");
        return DRV_ERROR_PARA_ERROR;
    }

    para.dev_id = devId;
    para.type = type;
    para.dst_devid = SCHED_INVALID_DEVID;
    para.input.inBuff = inPut->inBuff;
    para.input.inLen = inPut->inLen;
    para.output.outBuff = outPut->outBuff;
    para.output.outLen = outPut->outLen;

    return esched_dev_ioctl(devId, SCHED_QUERY_INFO, &para);
}

drvError_t halEschedQueryInfoEx(unsigned int devId, unsigned int dstDevId, ESCHED_QUERY_TYPE type,
                                struct esched_input_info *inPut, struct esched_output_info *outPut)
{
    struct sched_ioctl_para_query_info para;
    unsigned int phy_dest_devid = 0;
    drvError_t ret;

    if ((esched_device_check(devId) != 0) || (type >= QUERY_TYPE_MAX) || (inPut == NULL) || (outPut == NULL)) {
        sched_err("Invalid para. (devId=%u; type=%u; inPut=%u; outPut=%u)\n", devId, type, inPut == NULL ? 1 : 0,
                  outPut == NULL ? 1 : 0);
        return DRV_ERROR_PARA_ERROR;
    }
    ret = uda_get_udevid_by_devid_ex(dstDevId, &phy_dest_devid);
    if (ret != DRV_ERROR_NONE) {
        sched_err("get udevid failed. (dstDevId=%u)\n", dstDevId);
        return ret;
    }

    para.dev_id = devId;
    para.dst_devid = phy_dest_devid;
    para.type = type;
    para.input.inBuff = inPut->inBuff;
    para.input.inLen = inPut->inLen;
    para.output.outBuff = outPut->outBuff;
    para.output.outLen = outPut->outLen;

    return esched_dev_ioctl(devId, SCHED_QUERY_INFO, &para);
}

drvError_t esched_query_grp_type(uint32_t dev_id, uint32_t grp_id, GROUP_TYPE *type)
{
    if ((esched_device_check(dev_id) != 0) || (grp_id >= SCHED_MAX_GRP_NUM)) {
        sched_err("The dev_id or grp_id is invalid. (dev_id=%u; max=%u; grp_id=%u; max=%u)\n", dev_id,
                  ESCHED_LOGIC_DEV_NUM, grp_id, SCHED_MAX_GRP_NUM);
        return DRV_ERROR_PARA_ERROR;
    }

    if (sched_grp[dev_id].info[grp_id].type == GRP_TYPE_UNINIT) {
        sched_err("The group not created yet. (dev_id=%u; grp_id=%u)\n", dev_id, grp_id);
        return DRV_ERROR_UNINIT;
    }

    *type = sched_grp[dev_id].info[grp_id].type;
    return DRV_ERROR_NONE;
}

drvError_t halEschedSetPidPriority(uint32_t devId, SCHEDULE_PRIORITY priority)
{
    struct sched_ioctl_para_set_proc_pri para;

    para.dev_id = devId;
    para.pri = (uint32_t)priority;

    return esched_dev_ioctl(devId, SCHED_SET_PROCESS_PRIORITY_ID, &para);
}

drvError_t halEschedSetEventPriority(uint32_t devId, EVENT_ID eventId, SCHEDULE_PRIORITY priority)
{
    struct sched_ioctl_para_set_event_pri para;

    para.dev_id = devId;
    para.event_id = (uint32_t)eventId;
    para.pri = (uint32_t)priority;

    return esched_dev_ioctl(devId, SCHED_SET_EVENT_PRIORITY_ID, &para);
}

drvError_t halEschedDumpEventTrace(uint32_t devId, char *buff, uint32_t buffLen, uint32_t *dataLen)
{
    struct sched_ioctl_para_get_event_trace para;
    drvError_t ret;

    if (buff == NULL || dataLen == NULL) {
        sched_err("The variable buff or dataLen is NULL. (devId=%u)\n", devId);
        return DRV_ERROR_PARA_ERROR;
    }

    para.dev_id = devId;
    para.buff = buff;
    para.buff_len = buffLen;
    para.data_len = 0;

    ret = esched_dev_ioctl(devId, SCHED_GET_NODE_EVENT_TRACE, &para);
    if (ret != DRV_ERROR_NONE) {
        sched_err("Ioctl return error. (ret=%d)\n", (int32_t)ret);
        return ret;
    }

    *dataLen = para.data_len;
    return DRV_ERROR_NONE;
}

/* cpux_reason_key_timestamp_date */
drvError_t halEschedTraceRecord(uint32_t devId, const char *recordReason, const char *key)
{
    struct sched_ioctl_para_trigger_sched_trace_record para;
    size_t len;
    int32_t ret;

    if (recordReason == NULL || key == NULL) {
        sched_err("The variable recordReason or key is NULL. (devId=%u)\n", devId);
        return DRV_ERROR_PARA_ERROR;
    }

    para.dev_id = devId;
    len = ((strnlen(recordReason, SCHED_STR_MAX_LEN) >= SCHED_STR_MAX_LEN) ? SCHED_STR_MAX_LEN - 1 :
                                                                             strnlen(recordReason, SCHED_STR_MAX_LEN));
    ret = strncpy_s(para.record_reason, SCHED_STR_MAX_LEN, recordReason, len);
    if (ret != 0) {
#ifndef EMU_ST
        sched_warn("Unable to invoke the strncpy_s to copy recordReason. (ret=%d)\n", ret);
#endif
    }

    len = ((strnlen(key, SCHED_STR_MAX_LEN) >= SCHED_STR_MAX_LEN) ? SCHED_STR_MAX_LEN - 1 :
                                                                    strnlen(key, SCHED_STR_MAX_LEN));
    ret = strncpy_s(para.key, SCHED_STR_MAX_LEN, key, len);
    if (ret != 0) {
#ifndef EMU_ST
        sched_warn("Unable to invoke the strncpy_s to copy key. (ret=%d)\n", ret);
#endif
    }

    para.record_reason[SCHED_STR_MAX_LEN - 1] = '\0';
    para.key[SCHED_STR_MAX_LEN - 1] = '\0';

    return esched_dev_ioctl(devId, SCHED_TRIGGER_SCHED_TRACE_RECORD_VALUE, &para);
}

drvError_t esched_query_curr_sched_mode(unsigned int dev_id, unsigned int *sched_mode)
{
    drvError_t ret;
    struct sched_ioctl_para_query_sched_mode para;

    if ((esched_device_check(dev_id) != 0) || (sched_mode == NULL)) {
        sched_err("Invalid para. (dev_id=%u; sched_mode_ptr=%u)\n", dev_id, sched_mode == NULL ? 1 : 0);
        return DRV_ERROR_PARA_ERROR;
    }

    para.dev_id = dev_id;

    ret = esched_dev_ioctl(dev_id, SCHED_QUERY_SCHED_MODE, &para);
    if (ret == 0) {
        *sched_mode = para.sched_mode;
    }
    return DRV_ERROR_NONE;
}
#ifndef DRV_HOST
static __thread unsigned int g_sched_mode[ESCHED_DEV_NUM] = {[0 ...(ESCHED_DEV_NUM - 1)] = 0xff};
#endif
unsigned int esched_get_cpu_mode(uint32_t devid)
{
#ifdef DRV_HOST
    (void)devid;
    return 0;
#else
    if (g_sched_mode[devid] != 0xff) {
        return g_sched_mode[devid];
    }

    drvError_t ret;
    unsigned int cpu_mode = 0;

    ret = esched_query_curr_sched_mode(devid, &cpu_mode);
    if (ret != DRV_ERROR_NONE) {
        return 0;
    }
    g_sched_mode[devid] = cpu_mode;
    return g_sched_mode[devid];
#endif
}

drvError_t esched_device_close_user(uint32_t devid, halDevCloseIn *in)
{
    (void)in;
    if (!esched_support_extern_interface()) {
        return 0;
    }

    if (esched_device_check(devid) != 0) {
        sched_err("The dev_id is invalid. (dev_id=%u; max=%u)\n", devid, ESCHED_LOGIC_DEV_NUM);
        return DRV_ERROR_PARA_ERROR;
    }

    esched_clear_attach_refcnt(devid);

    (void)pthread_mutex_lock(&(sched_grp[devid].mutex));
    (void)memset_s((void *)sched_grp[devid].info, sizeof(sched_grp[devid].info), 0, sizeof(sched_grp[devid].info));
    (void)pthread_mutex_unlock(&(sched_grp[devid].mutex));

    /* esched sync event resource */
    esched_sync_res_reset(devid);

    sched_dev_fd[devid] = 0;
#ifndef DRV_HOST
    g_sched_mode[devid] = 0xff;
#endif
    g_esched_snapshot_flag = true;
    sched_run_info("Close user success. (dev_id=%u)\n", devid);
    return DRV_ERROR_NONE;
}

drvError_t esched_device_close(uint32_t devid, halDevCloseIn *in)
{
    (void)in;
    struct sched_ioctl_para_detach para_detach;

    if (!esched_support_extern_interface()) {
        return 0;
    }

    if (esched_device_check(devid) != 0) {
        sched_err("The dev_id is invalid. (dev_id=%u; max=%u)\n", devid, ESCHED_LOGIC_DEV_NUM);
        return DRV_ERROR_PARA_ERROR;
    }

    esched_detach_device(devid, &para_detach);
    (void)usleep(ESCHED_CLOSE_MAX_SLEEP); /* wait 10ms for other thread exit */
    esched_dev_close(devid);
    esched_clear_attach_refcnt(devid);

    (void)pthread_mutex_lock(&(sched_grp[devid].mutex));
    (void)memset_s((void *)sched_grp[devid].info, sizeof(sched_grp[devid].info), 0, sizeof(sched_grp[devid].info));
    (void)pthread_mutex_unlock(&(sched_grp[devid].mutex));

    /* esched sync event resource */
    esched_sync_res_reset(devid);

    sched_dev_fd[devid] = 0;
#ifndef DRV_HOST
    g_sched_mode[devid] = 0xff;
#endif

    sched_run_info("Close success. (dev_id=%u)\n", devid);
    return DRV_ERROR_NONE;
}

drvError_t esched_device_open(uint32_t devid, halDevOpenIn *in, halDevOpenOut *out)
{
    (void)in;
    (void)out;
    if (!esched_support_extern_interface()) {
        return 0;
    }
    if (esched_device_check(devid) != 0) {
        sched_err("The dev_id is invalid. (dev_id=%u; max=%u)\n", devid, ESCHED_LOGIC_DEV_NUM);
        return DRV_ERROR_PARA_ERROR;
    }

    if (sched_dev_fd[devid] >= 0) {
        halEschedAttachDevice(devid);
        sched_run_info("The dev_id has opened. (dev_id=%u)\n", devid);
        return DRV_ERROR_NONE;
    }

    sched_dev_fd[devid] = 0;
#ifndef DRV_HOST
    g_sched_mode[devid] = 0xff;
#endif
    halEschedAttachDevice(devid);
    sched_run_info("The dev_id open success. (dev_id=%u)\n", devid);
    return DRV_ERROR_NONE;
}

STATIC int32_t __attribute__((constructor)) esched_init(void)
{
    unsigned int i;

    esched_share_log_create();
    for (i = 0; i < ESCHED_DEV_NUM; i++) {
        sched_dev_fd[i] = 0;
        pthread_mutex_init(&(sched_grp[i].mutex), NULL);
    }
    return 0;
}

STATIC void __attribute__((destructor)) esched_exit(void)
{
    unsigned int i;

    for (i = 0; i < ESCHED_DEV_NUM; i++) {
        if (sched_dev_fd[i] > 0) {
            esched_dev_close(i);
        }
        (void)pthread_mutex_destroy(&(sched_grp[i].mutex));
    }
    esched_share_log_destroy();
}
