/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "securec.h"
#include "ascend_hal.h"
#include "davinci_interface.h"
#include "esched_user_interface.h"
#include "dms_user_interface.h"
#include "pbl_uda_user.h"
#include "drv_buff_common.h"
#include "queue.h"
#include "que_clt_pci.h"
#include "que_pci_msg.h"
#include "queue_client_comm.h"
#include "queue_interface.h"
#include "queue_user_manage.h"

#include "queue_client_base.h"
#include "que_platform.h"
#include "queue_feature.h"

enum queue_info_type {
    QUEUE_INFO_ENQUE = 0,
    QUEUE_INFO_F2NF,
    QUEUE_INFO_RETRY,
    QUEUE_INFO_TYPE_MAX
};

struct remote_queue_info {
    bool info[QUEUE_INFO_TYPE_MAX];
};

THREAD pid_t g_hostpid = -1;
THREAD int g_queue_tgid = -1;
THREAD int g_queue_devpid[MAX_DEVICE] = {0};
THREAD int g_queue_gid[MAX_DEVICE] = {-1};
THREAD int g_queue_init_status[MAX_DEVICE] = {0};
THREAD struct remote_queue_info *g_queue_info[MAX_DEVICE] = {NULL};
THREAD pthread_rwlock_t g_queue_inifo_rwlock[MAX_DEVICE];

static char g_mcast_flag[MAX_DEVICE] = {
    0,
};
static unsigned int g_mcast_gid[MAX_DEVICE] = {
    0,
};
static volatile unsigned int g_event_sn[MAX_DEVICE] = {0};
static volatile unsigned int g_threads_num[MAX_DEVICE] = {0};
static unsigned int g_event_sn_offset[MAX_DEVICE] = {0};
static pthread_mutex_t g_queue_mutex = PTHREAD_MUTEX_INITIALIZER;

#define QUEUE_SYNC_WAIT_TIMEOUT 10000 /* 10s */
#define QUEUE_WAIT_TIMEOUT_ALWAYS_WAIT (-1)
#define QUEUE_MSG_IRQ_BASE_TIMEOUT 50000 /* 50s - hdc dma max timeout*/
#define QUEUE_MAX_TIMEOUT_VALUE_INT (0x7FFFFFFF)
#define US_PER_SECOND 1000000

#define QUEUE_SEND_NORMAL 0
#define QUEUE_SEND_WITH_RETRY 1

STATIC drvError_t queue_send_event_sync(unsigned int dev_id, unsigned int retry_flg, int32_t timeout,
                                        struct queue_msg_info *msg_info);
STATIC drvError_t queue_submit_event_sync_send_msg(unsigned int dev_id, struct event_summary *event, void *arg,
                                                   struct event_proc_result *result);
STATIC drvError_t queue_submit_event_sync_queue_enqueue(unsigned int dev_id, struct event_summary *event, void *arg,
                                                        struct event_proc_result *result);
static drvError_t que_clt_api_unsubscribe(unsigned int dev_id, unsigned int qid, unsigned int subevent_id);

static void queue_remote_info_rwlock_init(void)
{
    unsigned int i;
    for (i = 0; i < MAX_DEVICE; i++) {
        pthread_rwlock_init(&g_queue_inifo_rwlock[i], NULL);
    }
}
static drvError_t queue_remote_info_alloc(unsigned int dev_id)
{
    struct remote_queue_info *remote_queue = NULL;
    int i, j;

    remote_queue = (struct remote_queue_info *)malloc(sizeof(struct remote_queue_info) * MAX_SURPORT_QUEUE_NUM);
    if (remote_queue == NULL) {
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    for (i = 0; i < MAX_SURPORT_QUEUE_NUM; i++) {
        for (j = 0; j < QUEUE_INFO_TYPE_MAX; j++) {
            remote_queue[i].info[j] = false;
        }
    }

    pthread_rwlock_wrlock(&g_queue_inifo_rwlock[dev_id]);
    g_queue_info[dev_id] = remote_queue;
    pthread_rwlock_unlock(&g_queue_inifo_rwlock[dev_id]);
    return 0;
}

static void queue_remote_info_free(unsigned int dev_id)
{
    pthread_rwlock_wrlock(&g_queue_inifo_rwlock[dev_id]);
    if (g_queue_info[dev_id] != NULL) {
        free(g_queue_info[dev_id]);
        g_queue_info[dev_id] = NULL;
    }
    pthread_rwlock_unlock(&g_queue_inifo_rwlock[dev_id]);
}

static inline void queue_remote_info_init(unsigned int dev_id, unsigned int qid)
{
    int i;

    if (pthread_rwlock_tryrdlock(&g_queue_inifo_rwlock[dev_id]) != 0) {
        return;
    }

    if (g_queue_info[dev_id] == NULL) {
        pthread_rwlock_unlock(&g_queue_inifo_rwlock[dev_id]);
        return;
    }

    for (i = 0; i < QUEUE_INFO_TYPE_MAX; i++) {
        g_queue_info[dev_id][qid].info[i] = false;
    }
    pthread_rwlock_unlock(&g_queue_inifo_rwlock[dev_id]);
}

static inline bool queue_get_remote_info(unsigned int dev_id, unsigned int qid, unsigned int info_type)
{
    bool value;

    if (pthread_rwlock_tryrdlock(&g_queue_inifo_rwlock[dev_id]) != 0) {
        return false;
    }

    if (g_queue_info[dev_id] == NULL) {
        pthread_rwlock_unlock(&g_queue_inifo_rwlock[dev_id]);
        return false;
    }

    value = g_queue_info[dev_id][qid].info[info_type];

    pthread_rwlock_unlock(&g_queue_inifo_rwlock[dev_id]);
    return value;
}

static inline void queue_set_remote_info(unsigned int dev_id, unsigned int qid, unsigned int info_type, bool value)
{
    if (pthread_rwlock_tryrdlock(&g_queue_inifo_rwlock[dev_id]) != 0) {
        return;
    }

    if (g_queue_info[dev_id] == NULL) {
        pthread_rwlock_unlock(&g_queue_inifo_rwlock[dev_id]);
        return;
    }

    g_queue_info[dev_id][qid].info[info_type] = value;

    pthread_rwlock_unlock(&g_queue_inifo_rwlock[dev_id]);
}

static void queue_inc_thread_num(unsigned int dev_id)
{
    (void)__sync_fetch_and_add(&g_threads_num[dev_id], (unsigned int)1);
}

static void queue_dec_thread_num(unsigned int dev_id)
{
    (void)__sync_fetch_and_sub(&g_threads_num[dev_id], (unsigned int)1);
}

static unsigned int queue_get_thread_num(unsigned int dev_id)
{
    return g_threads_num[dev_id];
}

static unsigned int queue_get_proc_event_sn(unsigned int dev_id, unsigned int event_sn)
{
    return g_event_sn_offset[dev_id] + (event_sn % QUEUE_PROC_EVENT_SN_NUM);
}

static unsigned int queue_atomic_get_event_sn(unsigned int dev_id)
{
    unsigned int sn = __sync_fetch_and_add(&g_event_sn[dev_id], (unsigned int)1);
    return queue_get_proc_event_sn(dev_id, sn);
}

static unsigned int get_cur_cpu_timestamp(void)
{
    struct timeval curr_time;
    unsigned long long tmp;

    que_get_time(&curr_time);
    tmp = (long long unsigned int)(curr_time.tv_sec * MS_PER_SECOND + curr_time.tv_usec / US_PER_MSECOND);

    return (tmp & QUEUE_32BIT_MASK);
}

STATIC int queue_init_check(unsigned int devid, unsigned int qid)
{
    if ((g_queue_init_status[devid] != QUEUE_INITED) || (g_queue_info[devid] == NULL)) {
        QUEUE_LOG_ERR("invalid dev. (devid=%u; qid=%u)\n", devid, qid);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

STATIC pid_t queue_get_hostpid(void)
{
    if (g_hostpid == -1) {
        g_hostpid = drvDeviceGetBareTgid();
        QUEUE_RUN_LOG_INFO("query tgid. (tgid=%d)\n", g_hostpid);
    }
    return g_hostpid;
}

#ifndef EMU_ST
STATIC void queue_free_inherit_resource(void)
{
    unsigned int devid;

    for (devid = 0; devid < MAX_DEVICE; devid++) {
        g_queue_init_status[devid] = -1;
        queue_remote_info_free(devid);
    }
    (void)close(get_g_queue_fd());
    set_g_queue_fd(-1);
    g_queue_tgid = -1;
}
#endif

STATIC drvError_t queue_is_support(unsigned int dev_id)
{
#ifdef __linux
    unsigned int split_mode;
    drvError_t ret;

    ret = halGetDeviceSplitMode(dev_id, &split_mode);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("get split mode failed. (dev_id=%u, ret=%d).\n", dev_id, ret);
        return DRV_ERROR_PARA_ERROR;
    }

    if (split_mode != VMNG_NORMAL_NONE_SPLIT_MODE) {
        QUEUE_LOG_INFO("capacity splitting is not supported. (dev_id=%u; split_mode=%u).\n", dev_id, split_mode);
        return DRV_ERROR_NOT_SUPPORT;
    }
    QUEUE_LOG_INFO("support. (dev_id=%u, split_mode=%u).\n", dev_id, split_mode);
#endif

    return DRV_ERROR_NONE;
}

STATIC drvError_t queue_get_dev_pid(unsigned int dev_id, unsigned int vfid, int hostpid, int *devpid)
{
    struct halQueryDevpidInfo hostpidinfo = {0};
    unsigned int chip_id, vfid_tmp, cp_type;
    int master_pid;
    drvError_t ret;

    ret = drvQueryProcessHostPid(hostpid, &chip_id, &vfid_tmp, (unsigned int *)&master_pid, &cp_type);
    if (ret != 0) {
        master_pid = hostpid;
    }

    hostpidinfo.proc_type = DEVDRV_PROCESS_CP1;
    hostpidinfo.hostpid = master_pid; /* docker pid */
    hostpidinfo.devid = dev_id;
    hostpidinfo.vfid = vfid;
    ret = halQueryDevpid(hostpidinfo, devpid);
    if (ret) {
        QUEUE_LOG_ERR("Query devpid failed. (dev_id=%u; vfid=%u; hostpid=%d; master_pid=%d; proc_type=%u; ret=%d).\n",
                      dev_id, vfid, hostpid, master_pid, hostpidinfo.proc_type, ret);
    } else {
        if (g_queue_devpid[dev_id] <= 0) {
            QUEUE_RUN_LOG_INFO(
                "query devpid succ. (dev_id=%u; vfid=%u; hostpid=%d; master_pid=%d; proc_type=%u; devpid=%d).\n",
                dev_id, vfid, hostpid, master_pid, hostpidinfo.proc_type, *devpid);
        }
    }
    return ret;
}

static int queue_drv_event_query_gid(unsigned int dev_id, unsigned int *grp_id)
{
    struct esched_query_gid_output gid_out = {0};
    struct esched_query_gid_input gid_in = {0};
    struct esched_output_info out_put = {0};
    struct esched_input_info in_put = {0};
    drvError_t ret;

    gid_in.pid = (int)g_queue_devpid[dev_id];
    (void)strcpy_s(gid_in.grp_name, EVENT_MAX_GRP_NAME_LEN, EVENT_DRV_MSG_GRP_NAME);
    in_put.inBuff = &gid_in;
    in_put.inLen = sizeof(struct esched_query_gid_input);
    out_put.outBuff = &gid_out;
    out_put.outLen = sizeof(struct esched_query_gid_output);
    ret = halEschedQueryInfo(dev_id, QUERY_TYPE_REMOTE_GRP_ID, &in_put, &out_put);
    if (ret == DRV_ERROR_NONE) {
        *grp_id = gid_out.grp_id;
    }

    return ret;
}

STATIC drvError_t queue_dc_agent_init(unsigned int dev_id)
{
    struct queue_init_msg_para init_para = {0};
    struct queue_msg_info msg_info;
    drvError_t ret;
    char proc_sn;

    init_para.comm.devid = (short unsigned int)dev_id;
    init_para.hostpid = queue_get_hostpid();
    msg_info.subevent_id = DRV_SUBEVENT_QUEUE_INIT_MSG;
    msg_info.msg = (char *)&init_para;
    msg_info.msg_len = sizeof(struct queue_init_msg_para);

    ret = queue_send_event_sync(dev_id, QUEUE_SEND_WITH_RETRY, QUEUE_SYNC_WAIT_TIMEOUT, &msg_info);
    if (ret != DRV_ERROR_NONE && ret != DRV_ERROR_REPEATED_INIT) {
        return ret;
    }

    if (g_mcast_flag[dev_id] == 1) {
        ret = queue_drv_event_query_gid(dev_id, &g_mcast_gid[dev_id]);
        if (ret != DRV_ERROR_NONE) {
            QUEUE_LOG_ERR("get grpid failed. (ret=%d; devid=%u)\n", ret, dev_id);
            return ret;
        }
    }

    proc_sn = msg_info.result.data[0];
    g_event_sn_offset[dev_id] = (uint32_t)proc_sn * QUEUE_PROC_EVENT_SN_NUM;
    QUEUE_RUN_LOG_INFO(
        "queue_dc_agent_init device. (devid=%u; mcast_flag=%u; mcast_gid=%u; proc_sn=%d; event_sn_offset=%u)\n", dev_id,
        g_mcast_flag[dev_id], g_mcast_gid[dev_id], proc_sn, g_event_sn_offset[dev_id]);

    return DRV_ERROR_NONE;
}

STATIC drvError_t queue_host_init(unsigned int dev_id, int devpid)
{
    drvError_t ret;

    g_queue_gid[dev_id] = -1;
    g_queue_devpid[dev_id] = devpid;
    ret = queue_dc_agent_init(dev_id);
    if (ret != DRV_ERROR_NONE) {
        g_queue_devpid[dev_id] = -1;
        QUEUE_LOG_ERR("queue_dc_agent_init failed. (ret=%d; devid=%u)\n", ret, dev_id);
        return ret;
    }

    ret = queue_remote_info_alloc(dev_id);
    if (ret != 0) {
        g_queue_devpid[dev_id] = -1;
        QUEUE_LOG_ERR("malloc failed. (ret=%d; devid=%u)\n", ret, dev_id);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    ret = queue_host_common_queue_init(dev_id);
    if (ret != DRV_ERROR_NONE) {
        g_queue_devpid[dev_id] = -1;
        queue_remote_info_free(dev_id);
        QUEUE_LOG_ERR("init ioctl failed, ret(%d).\n", ret);
        return DRV_ERROR_IOCRL_FAIL;
    }

    g_queue_init_status[dev_id] = QUEUE_INITED;

    return DRV_ERROR_NONE;
}

/* When the CP process is restarted, the device automatically reclaims
 * the queue and does not need to send a message to deinitialize queue.
 */
STATIC void queue_host_uninit(unsigned int dev_id, unsigned int scene)
{
#ifndef EMU_ST
    drvError_t ret;
    if ((get_g_queue_fd() < 0) || (g_queue_init_status[dev_id] != QUEUE_INITED)) {
        return;
    }
    if (scene == CLOSE_ALL_RES) {
        ret = queue_host_common_queue_uninit(dev_id);
        if (ret != DRV_ERROR_NONE) {
            QUEUE_RUN_LOG_INFO("uninit ioctl. ret(%d).\n", ret);
        }
    }
#endif

    g_queue_init_status[dev_id] = QUEUE_UNINITED;
    queue_remote_info_free(dev_id);
    g_queue_devpid[dev_id] = -1;
    g_hostpid = -1;
    g_mcast_flag[dev_id] = 0;
    g_mcast_gid[dev_id] = 0;
    g_event_sn[dev_id] = 0;
    g_threads_num[dev_id] = 0;
    g_event_sn_offset[dev_id] = 0;
}

STATIC drvError_t queue_init_client(unsigned int dev_id)
{
    int hostpid, devpid;
    drvError_t ret;
    int fd = -1;

    if (!queue_has_dev_feature(dev_id, QUEUE_FEATURE_SUPPORT_SRIOV)) {
        ret = queue_is_support(dev_id);
        if (ret != DRV_ERROR_NONE) {
            return ret;
        }
    }

    (void)pthread_mutex_lock(&g_queue_mutex);
    hostpid = queue_get_hostpid();
    /* child process needs free inherited resource from parent process */
    if (get_g_queue_fd() > 0 && g_queue_tgid != hostpid) {
        queue_free_inherit_resource();
    }
    g_queue_tgid = hostpid;

    fd = queue_open_dev_base();
    if (fd < 0) {
#ifndef EMU_ST
        (void)pthread_mutex_unlock(&g_queue_mutex);
        QUEUE_LOG_ERR("queue_open_dev_base failed.\n");
        return DRV_ERROR_INVALID_DEVICE;
#endif
    }

    ret = queue_get_dev_pid(dev_id, 0, getpid(), &devpid);
    if (ret != DRV_ERROR_NONE) {
        (void)pthread_mutex_unlock(&g_queue_mutex);
        return DRV_ERROR_INNER_ERR;
    }

    if (g_queue_init_status[dev_id] == QUEUE_INITED) {
        if (g_queue_devpid[dev_id] == devpid) {
            (void)pthread_mutex_unlock(&g_queue_mutex);
            return DRV_ERROR_REPEATED_INIT;
        }
        queue_host_uninit(dev_id, CLOSE_ALL_RES);
    }

    ret = queue_host_init(dev_id, devpid);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("init ioctl failed, ret(%d).\n", ret);
    }

    (void)pthread_mutex_unlock(&g_queue_mutex);

    return ret;
}

STATIC void queue_uninit_client(unsigned int dev_id, unsigned int scene)
{
    if (!queue_has_dev_feature(dev_id, QUEUE_FEATURE_SUPPORT_SRIOV)) {
        if (queue_is_support(dev_id) != DRV_ERROR_NONE) {
            return;
        }
    }
    (void)pthread_mutex_lock(&g_queue_mutex);
    queue_host_uninit(dev_id, scene);
    (void)pthread_mutex_unlock(&g_queue_mutex);

    return;
}

drvError_t halQueueGetMaxNum(unsigned int *maxQueNum)
{
    (void)maxQueNum;
    return DRV_ERROR_NOT_SUPPORT;
}

STATIC drvError_t queue_get_event_grp_id(unsigned int devid, unsigned int *grp_id)
{
    struct esched_query_gid_output gid_out = {0};
    struct esched_query_gid_input gid_in = {0};
    struct esched_output_info out_put = {0};
    struct esched_input_info in_put = {0};
    size_t grp_name_len;
    drvError_t ret;

    if (g_queue_gid[devid] != -1) {
        *grp_id = (uint32_t)g_queue_gid[devid];
        return DRV_ERROR_NONE;
    }
    gid_in.pid = g_queue_devpid[devid];
    grp_name_len = strlen(PROXY_HOST_QUEUE_GRP_NAME);
    (void)memcpy_s(gid_in.grp_name, EVENT_MAX_GRP_NAME_LEN, PROXY_HOST_QUEUE_GRP_NAME, grp_name_len);
    in_put.inBuff = &gid_in;
    in_put.inLen = sizeof(struct esched_query_gid_input);
    out_put.outBuff = &gid_out;
    out_put.outLen = sizeof(struct esched_query_gid_output);
    ret = halEschedQueryInfo(devid, QUERY_TYPE_REMOTE_GRP_ID, &in_put, &out_put);
    if (ret == DRV_ERROR_NONE) {
        *grp_id = gid_out.grp_id;
        g_queue_gid[devid] = (int)gid_out.grp_id;
        return DRV_ERROR_NONE;
    } else if (ret == DRV_ERROR_UNINIT) {
        *grp_id = 0; // PROXY_HOST_GRP_NAME grp not exist, use default grpid 0.
        g_queue_gid[devid] = 0;
        return DRV_ERROR_NONE;
    }

    QUEUE_LOG_ERR("query grpid failed. (ret=%d; devid=%u; devpid=%d).\n", ret, devid, gid_in.pid);
    return ret;
}

STATIC drvError_t queue_fill_event(struct event_summary *event_info, unsigned int devid, unsigned int subevent_id,
                                   const char *msg, unsigned int msg_len)
{
    struct event_sync_msg *msg_head = NULL;
    drvError_t ret;

    ret = queue_get_event_grp_id(devid, &event_info->grp_id);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }
    event_info->dst_engine = ACPU_DEVICE;
    event_info->policy = ONLY;
    event_info->event_id = EVENT_DRV_MSG;
    event_info->subevent_id = subevent_id;
    event_info->pid = g_queue_devpid[devid];
    event_info->msg_len = (uint32_t)sizeof(struct event_sync_msg) + msg_len;
    event_info->msg = malloc(event_info->msg_len);
    if (event_info->msg == NULL) {
        QUEUE_LOG_ERR("malloc %u failed.\n", event_info->msg_len);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    msg_head = (struct event_sync_msg *)event_info->msg;
    msg_head->dst_engine = CCPU_HOST;

    if (msg_len == 0) {
        return DRV_ERROR_NONE;
    }

    ret = (drvError_t)memcpy_s(event_info->msg + sizeof(struct event_sync_msg), msg_len, msg, msg_len);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("memcpy_s failed, ret=%d.\n", ret);
        free(event_info->msg);
        event_info->msg = NULL;
        return DRV_ERROR_PARA_ERROR;
    }

    return DRV_ERROR_NONE;
}

STATIC void queue_clear_event(struct event_summary *event_info)
{
    free(event_info->msg);
    event_info->msg = NULL;
}

static drvError_t queue_enbale_event_mcast(unsigned int dev_id)
{
    if (dev_id >= MAX_DEVICE) {
        QUEUE_LOG_ERR("invalid para. (dev_id=%d)\n", dev_id);
        return DRV_ERROR_INVALID_DEVICE;
    }

    if (g_queue_devpid[dev_id] > 0) {
        int ret = queue_drv_event_query_gid(dev_id, &g_mcast_gid[dev_id]);
        if (ret != DRV_ERROR_NONE) {
#ifndef EMU_ST
            QUEUE_LOG_ERR("get grpid failed. (ret=%d; devid=%u)\n", ret, dev_id);
            return ret;
#endif
        }
    }

    g_mcast_flag[dev_id] = 1;
    QUEUE_RUN_LOG_INFO("queue enable event mcast. (dev_id=%u; mcast_gid=%d)\n", dev_id, g_mcast_gid[dev_id]);

    return DRV_ERROR_NONE;
}

static inline void queue_try_disbale_event_mcast(unsigned int dev_id, char remote_flag)
{
    if ((g_mcast_flag[dev_id] == 1) && (remote_flag == 0)) {
        g_mcast_flag[dev_id] = 0;
        QUEUE_RUN_LOG_INFO("queue disable event mcast. (dev_id=%u)\n", dev_id);
    }
}

static inline void queue_event_flow_ctrl(unsigned int dev_id)
{
    int wait_cnt = 0;

    while (queue_get_thread_num(dev_id) > QUEUE_EVENT_OUTSTANDING) {
        if (wait_cnt++ > 10000) { /* 10000 * 100 us = 1s */
            break;
        }
        usleep(100); /* 100 us */
    }

    if (wait_cnt > 0) {
#ifndef EMU_ST
        QUEUE_LOG_WARN("event flow ctrl. (dev_id=%u; event_sn=%u; threads_num=%u; wait_cnt=%d)\n", dev_id,
                       queue_get_proc_event_sn(dev_id, g_event_sn[dev_id]), g_threads_num[dev_id], wait_cnt);
#endif
    }
}

static void queue_fill_mcast_para(unsigned int dev_id, struct queue_mcast_para *mcast_para)
{
    mcast_para->mcast_flag = (uint32_t)g_mcast_flag[dev_id] & 0x1;
    if (mcast_para->mcast_flag == 1) {
        queue_event_flow_ctrl(dev_id);
        mcast_para->gid = (unsigned char)g_mcast_gid[dev_id];
        mcast_para->event_sn = (uint32_t)queue_atomic_get_event_sn(dev_id) & 0x3FFFFF;
        queue_inc_thread_num(dev_id);
    }
}

static void queue_update_finish_event_sn(unsigned int dev_id, struct queue_mcast_para *mcast_para)
{
    if (mcast_para->mcast_flag == 1) {
        queue_dec_thread_num(dev_id);
    }
}

STATIC drvError_t queue_send_event_sync(unsigned int dev_id, unsigned int retry_flg, int32_t timeout,
                                        struct queue_msg_info *msg_info)
{
    struct event_summary event_info;
    struct event_reply reply;
    drvError_t ret;
#ifndef EMU_ST
    if (dev_id >= MAX_DEVICE) {
        QUEUE_LOG_ERR("input dev_id is invalid. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_INVALID_DEVICE;
    }
#endif
    ret = queue_fill_event(&event_info, dev_id, msg_info->subevent_id, msg_info->msg, msg_info->msg_len);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("queue_fill_event failed, ret=%d.\n", ret);
        return ret;
    }
    reply.buf_len = sizeof(struct event_proc_result);
    reply.buf = (char *)&msg_info->result;

    ret = halEschedSubmitEventSync(dev_id, &event_info, QUEUE_SYNC_TIMEOUT, &reply);
    if ((retry_flg == QUEUE_SEND_WITH_RETRY) && (ret == DRV_ERROR_WAIT_TIMEOUT)) {
        ret = halEschedSubmitEventSync(dev_id, &event_info, timeout, &reply);
    }
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("halEschedSubmitEventSync failed, ret(%d).\n", ret);
        queue_clear_event(&event_info);
        return DRV_ERROR_INNER_ERR;
    }
    queue_clear_event(&event_info);

    if (reply.reply_len != reply.buf_len) {
        QUEUE_LOG_ERR("reply_len(%u) not equal to buf_len(%u).\n", reply.reply_len, reply.buf_len);
        return DRV_ERROR_PARA_ERROR;
    }

    return (drvError_t)msg_info->result.ret;
}

STATIC void queue_sub_init(unsigned int dev_id, unsigned int qid)
{
    if (qid >= MAX_SURPORT_QUEUE_NUM) {
        return;
    }
    queue_remote_info_init(dev_id, qid);
    return;
}

STATIC drvError_t queue_create_client(unsigned int dev_id, const QueueAttr *que_attr, unsigned int *qid)
{
    struct queue_ctrl_msg_send_stru ctrl_msg_send;
    struct event_proc_result result;
    struct queue_msg_info msg_info;
    struct event_summary event;
    drvError_t ret;

    msg_info.result_ret = (int)DRV_ERROR_NONE;
    msg_info.subevent_id = DRV_SUBEVENT_CREATE_MSG;
    msg_info.msg = NULL;
    msg_info.msg_len = 0;
    ret = queue_fill_event(&event, dev_id, msg_info.subevent_id, msg_info.msg, msg_info.msg_len);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("queue_fill_event failed. (ret=%d; dev_id=%u)\n", ret, dev_id);
        return ret;
    }

    ctrl_msg_send.devid = dev_id;
    ctrl_msg_send.ctrl_data_addr = (const void *)que_attr;
    ctrl_msg_send.ctrl_data_len = sizeof(QueueAttr);
    ctrl_msg_send.host_timestamp = get_cur_cpu_timestamp();
    ret = queue_submit_event_sync_send_msg(dev_id, &event, &ctrl_msg_send, &result);
    if (ret != DRV_ERROR_NONE) {
        queue_clear_event(&event);
        QUEUE_LOG_ERR("submit event failed. (ret=%d; dev_id=%u; host_timestamp=%ums)\n", ret, dev_id,
                      ctrl_msg_send.host_timestamp);
        return ret;
    }

    if (result.ret == msg_info.result_ret) {
        *qid = *((unsigned int *)result.data);
        queue_sub_init(dev_id, *qid);
    }

    queue_clear_event(&event);
    return (drvError_t)result.ret;
}

STATIC drvError_t queue_grant_client(unsigned int dev_id, unsigned int qid, int pid, QueueShareAttr attr)
{
    QueueGrantPara grant_para;
    struct queue_msg_info msg_info = {0};
    drvError_t ret;

    grant_para.devid = dev_id;
    grant_para.qid = qid;
    grant_para.pid = pid;
    grant_para.attr = attr;
    msg_info.subevent_id = DRV_SUBEVENT_GRANT_MSG;
    msg_info.msg = (char *)&grant_para;
    msg_info.msg_len = sizeof(grant_para);

    ret = queue_send_event_sync(dev_id, QUEUE_SEND_WITH_RETRY, QUEUE_SYNC_WAIT_TIMEOUT, &msg_info);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("queue_send_event_sync failed. (subevent_id=%u; devid=%u; qid=%u; pid=%d; ret=%d).\n",
                      DRV_SUBEVENT_GRANT_MSG, dev_id, qid, pid, ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

STATIC drvError_t queue_attach_client(unsigned int dev_id, unsigned int qid, int time_out)
{
    QueueAttachPara attach_para;
    struct queue_msg_info msg_info = {0};
    drvError_t ret;

    attach_para.devid = dev_id;
    attach_para.qid = qid;
    attach_para.timeout = time_out;
    msg_info.subevent_id = DRV_SUBEVENT_ATTACH_MSG;
    msg_info.msg = (char *)&attach_para;
    msg_info.msg_len = sizeof(attach_para);

    ret = queue_send_event_sync(dev_id, QUEUE_SEND_NORMAL, QUEUE_SYNC_WAIT_TIMEOUT, &msg_info);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("queue_send_event_sync failed. (subevent_id=%u; devid=%u; qid=%u; time_out=%dms; ret=%d).\n",
                      DRV_SUBEVENT_ATTACH_MSG, dev_id, qid, time_out, ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

STATIC drvError_t queue_destroy_client(unsigned int dev_id, unsigned int qid)
{
    struct queue_common_para destroy_para;
    struct queue_msg_info msg_info;
    drvError_t ret;

    destroy_para.qid = (short unsigned int)qid;
    destroy_para.host_timestamp = get_cur_cpu_timestamp();
    msg_info.subevent_id = DRV_SUBEVENT_DESTROY_MSG;
    msg_info.msg = (char *)&destroy_para;
    msg_info.msg_len = sizeof(struct queue_common_para);

    ret = queue_send_event_sync(dev_id, QUEUE_SEND_NORMAL, QUEUE_SYNC_WAIT_TIMEOUT, &msg_info);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("queue_send_event_sync failed. (ret=%d, devid=%u, qid=%u, host_timestamp=%ums).\n", ret, dev_id,
                      qid, destroy_para.host_timestamp);
        return ret;
    }

    return DRV_ERROR_NONE;
}

STATIC drvError_t queue_reset_client(unsigned int dev_id, unsigned int qid)
{
    struct queue_common_para queue_clean_para;
    struct queue_msg_info msg_info;
    drvError_t ret;

    queue_clean_para.qid = (short unsigned int)qid;
    queue_clean_para.host_timestamp = get_cur_cpu_timestamp();
    msg_info.subevent_id = DRV_SUBEVENT_QUEUE_RESET_MSG;
    msg_info.msg = (char *)&queue_clean_para;
    msg_info.msg_len = sizeof(struct queue_common_para);

    ret = queue_send_event_sync(dev_id, QUEUE_SEND_NORMAL, QUEUE_SYNC_WAIT_TIMEOUT, &msg_info);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("queue_send_event_sync failed. (ret=%d; devid=%u; qid=%u; host_timestamp=%ums)\n", ret, dev_id,
                      qid, queue_clean_para.host_timestamp);
        return ret;
    }

    return DRV_ERROR_NONE;
}

STATIC drvError_t queue_en_queue_client(unsigned int dev_id, unsigned int qid, void *mbuf)
{
    (void)dev_id;
    (void)qid;
    (void)mbuf;
    return DRV_ERROR_NOT_SUPPORT;
}

STATIC drvError_t queue_de_queue_client(unsigned int dev_id, unsigned int qid, void **mbuf)
{
    (void)dev_id;
    (void)qid;
    (void)mbuf;
    return DRV_ERROR_NOT_SUPPORT;
}

STATIC drvError_t queue_subscribe_client(unsigned int dev_id, unsigned int qid, unsigned int group_id, int type)
{
    (void)dev_id;
    (void)qid;
    (void)group_id;
    (void)type;
    return DRV_ERROR_NOT_SUPPORT;
}

STATIC drvError_t queue_unsubscribe_client(unsigned int dev_id, unsigned int qid)
{
    (void)dev_id;
    (void)qid;
    return DRV_ERROR_NOT_SUPPORT;
}

STATIC drvError_t queue_sub_f_to_nf_event_client(unsigned int dev_id, unsigned int qid, unsigned int group_id)
{
    (void)dev_id;
    (void)qid;
    (void)group_id;
    return DRV_ERROR_NOT_SUPPORT;
}

STATIC drvError_t queue_unsub_f_to_nf_event_client(unsigned int dev_id, unsigned int qid)
{
    (void)dev_id;
    (void)qid;
    return DRV_ERROR_NOT_SUPPORT;
}

STATIC drvError_t queue_sub_event_client(struct QueueSubPara *sub_para_)
{
    struct queue_sub_para sub_para = {{0}, 0, 0, 0, 0, 0, 0};
    struct queue_msg_info msg_info;
    unsigned int dst_phy_dev_id = QUEUE_INVALID_VALUE;
    drvError_t ret;

    ret = queue_init_check(sub_para_->devId, sub_para_->qid);
    if (ret != 0) {
        return ret;
    }

    if ((sub_para_->flag & QUEUE_SUB_FLAG_SPEC_DST_DEVID) != 0) {
        ret = uda_get_udevid_by_devid(sub_para_->dstDevId, &dst_phy_dev_id);
        if (ret != DRV_ERROR_NONE) {
            QUEUE_LOG_ERR("get udevid failed. (devid=%u, qid=%u)\n", sub_para_->devId, sub_para_->qid);
            return ret;
        }
    }

    sub_para.comm.qid = (short unsigned int)sub_para_->qid;
    sub_para.comm.host_timestamp = get_cur_cpu_timestamp();
    sub_para.pid = g_queue_tgid;
    sub_para.gid = sub_para_->groupId;
    sub_para.tid = ((sub_para_->flag & QUEUE_SUB_FLAG_SPEC_THREAD) != 0) ? sub_para_->threadId : QUEUE_INVALID_VALUE;
    sub_para.event_id = (sub_para_->eventType == QUEUE_F2NF_EVENT) ? EVENT_QUEUE_FULL_TO_NOT_FULL : EVENT_QUEUE_ENQUEUE;
    sub_para.dst_phy_devid = dst_phy_dev_id;
    queue_fill_mcast_para(sub_para_->devId, &sub_para.comm.mcast_para);

    msg_info.msg = (char *)&sub_para;
    msg_info.msg_len = sizeof(struct queue_sub_para);
    msg_info.subevent_id = (sub_para_->eventType == QUEUE_F2NF_EVENT) ? DRV_SUBEVENT_SUBF2NF_MSG :
                                                                        DRV_SUBEVENT_SUBE2NE_MSG;

    ret = queue_send_event_sync(sub_para_->devId, QUEUE_SEND_NORMAL, QUEUE_SYNC_WAIT_TIMEOUT, &msg_info);
    queue_update_finish_event_sn(sub_para_->devId, &sub_para.comm.mcast_para);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("queue_send_event_sync failed. (ret=%d, devid=%u, qid=%u, host_timestamp=%ums)\n", ret,
                      sub_para_->devId, sub_para_->qid, sub_para.comm.host_timestamp);
        return ret;
    }

    queue_try_disbale_event_mcast(sub_para_->devId, msg_info.result.data[0]);

    if (sub_para_->eventType == QUEUE_F2NF_EVENT) {
        queue_set_remote_info(sub_para_->devId, sub_para_->qid, QUEUE_INFO_F2NF, true);
    } else {
        queue_set_remote_info(sub_para_->devId, sub_para_->qid, QUEUE_INFO_ENQUE, true);
    }
    (void)queue_register_callback(sub_para_->groupId);

    QUEUE_LOG_INFO("sub event. (dev_id=%u; qid=%u; event_type=%d)\n", sub_para_->devId, sub_para_->qid,
                   sub_para_->eventType);

    return DRV_ERROR_NONE;
}

STATIC drvError_t queue_unsub_event_client(struct QueueUnsubPara *unsub_para)
{
    int devpid;
    unsigned int subevent_id;
    drvError_t ret;

    ret = queue_init_check(unsub_para->devId, unsub_para->qid);
    if (ret != DRV_ERROR_NONE) {
        return DRV_ERROR_INNER_ERR;
    }

    ret = queue_get_dev_pid(unsub_para->devId, 0, getpid(), &devpid);
    if (ret != DRV_ERROR_NONE) {
        return DRV_ERROR_INNER_ERR;
    }

    subevent_id = (unsub_para->eventType == QUEUE_F2NF_EVENT) ? DRV_SUBEVENT_UNSUBF2NF_MSG : DRV_SUBEVENT_UNSUBE2NE_MSG;
    ret = que_clt_api_unsubscribe(unsub_para->devId, unsub_para->qid, subevent_id);
    if (ret != 0) {
        return ret;
    }

    if (unsub_para->eventType == QUEUE_F2NF_EVENT) {
        queue_set_remote_info(unsub_para->devId, unsub_para->qid, QUEUE_INFO_F2NF, false);
    } else {
        queue_set_remote_info(unsub_para->devId, unsub_para->qid, QUEUE_INFO_ENQUE, false);
    }

    QUEUE_LOG_INFO("unsub event. (dev_id=%u; qid=%u; event_type=%d)\n", unsub_para->devId, unsub_para->qid,
                   unsub_para->eventType);

    return DRV_ERROR_NONE;
}

STATIC drvError_t queue_get_status_client(unsigned int dev_id, unsigned int qid, QUEUE_QUERY_ITEM query_item,
                                          unsigned int len, void *data)
{
    struct queue_get_status_para para = {{0}, 0, 0};
    struct queue_msg_info msg_info = {0};
    drvError_t ret;

    para.comm.devid = (short unsigned int)dev_id;
    para.comm.qid = (short unsigned int)qid;
    para.comm.host_timestamp = get_cur_cpu_timestamp();
    para.query_item = query_item;
    para.out_len = len;

    msg_info.msg = (char *)&para;
    msg_info.msg_len = sizeof(struct queue_get_status_para);
    msg_info.subevent_id = DRV_SUBEVENT_GET_QUEUE_STATUS_MSG;

    ret = queue_send_event_sync(dev_id, QUEUE_SEND_NORMAL, QUEUE_SYNC_TIMEOUT, &msg_info);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("queue_send_event_sync failed. (ret=%d; devid=%u; qid=%u; host_timestamp=%ums)\n", ret, dev_id,
                      qid, para.comm.host_timestamp);
        return ret;
    }

    ret = (drvError_t)memcpy_s(data, len, msg_info.result.data, len);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("memcpy failed. (ret=%d; devid=%u; qid=%u; len=%u)\n", ret, dev_id, qid, len);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

static drvError_t query_queue_max_iovec_num(unsigned int dev_id, QueueQueryInputPara *in_put,
                                            QueueQueryOutputPara *out_put)
{
    (void)dev_id;
    (void)in_put;
    QueueQueryOutput *out_buff = (QueueQueryOutput *)(out_put->outBuff);

    if (out_put->outLen < sizeof(QueQueryMaxIovecNum)) {
        QUEUE_LOG_ERR("Input para error. (out_len=%u)\n", out_put->outLen);
        return DRV_ERROR_INVALID_VALUE;
    }

    out_buff->queQueryMaxIovecNum.count = QUEUE_MAX_IOVEC_NUM;

    return DRV_ERROR_NONE;
}

static drvError_t (*g_queue_query[QUEUE_QUERY_CMD_MAX])(unsigned int dev_id, QueueQueryInputPara *in_put,
                                                        QueueQueryOutputPara *out_put) = {
    [QUEUE_QUERY_MAX_IOVEC_NUM] = query_queue_max_iovec_num,
};

STATIC drvError_t queue_query_client(unsigned int dev_id, QueueQueryCmdType cmd, QueueQueryInputPara *in_put,
                                     QueueQueryOutputPara *out_put)
{
    if (g_queue_query[cmd] == NULL) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return g_queue_query[cmd](dev_id, in_put, out_put);
}

STATIC drvError_t queue_set_client(unsigned int dev_id, QueueSetCmdType cmd, QueueSetInputPara *input)
{
    (void)input;
    if (cmd == QUEUE_ENABLE_CLIENT_EVENT_MCAST) {
        return queue_enbale_event_mcast(dev_id);
    }
    return DRV_ERROR_NOT_SUPPORT;
}

STATIC drvError_t queue_query_info_client(unsigned int dev_id, unsigned int qid, QueueInfo *que_info)
{
    struct queue_common_para query_para;
    struct queue_msg_info msg_info;
    drvError_t ret;

    query_para.qid = (short unsigned int)qid;
    query_para.host_timestamp = get_cur_cpu_timestamp();

    msg_info.msg = (char *)&query_para;
    msg_info.msg_len = sizeof(struct queue_common_para);
    msg_info.subevent_id = DRV_SUBEVENT_QUERY_MSG;

    ret = queue_send_event_sync(dev_id, QUEUE_SEND_WITH_RETRY, QUEUE_SYNC_WAIT_TIMEOUT, &msg_info);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("queue_send_event_sync failed. (ret=%d, devid=%u, qid=%u, host_timestamp=%ums).\n", ret, dev_id,
                      qid, query_para.host_timestamp);
        return ret;
    }

    que_info->size = *((int *)msg_info.result.data);
    QUEUE_LOG_DEBUG("size=%d.\n", que_info->size);
    return DRV_ERROR_NONE;
}

STATIC void queue_finish_callback_client(unsigned int dev_id, unsigned int qid, unsigned int grp_id,
                                         unsigned int event_id)
{
    struct queue_finish_callback_para para;
    struct queue_msg_info msg_info;
    drvError_t ret;

    para.comm.devid = (short unsigned int)dev_id;
    para.comm.qid = (short unsigned int)qid;
    para.comm.host_timestamp = get_cur_cpu_timestamp();
    para.grp_id = grp_id;
    para.event_id = event_id;

    msg_info.msg = (char *)&para;
    msg_info.msg_len = sizeof(struct queue_finish_callback_para);
    msg_info.subevent_id = DRV_SUBEVENT_FINISH_CALLBACK_MSG;

    ret = queue_send_event_sync(dev_id, QUEUE_SEND_NORMAL, QUEUE_SYNC_TIMEOUT, &msg_info);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("queue_send_event_sync failed. (ret=%d, devid=%u, qid=%u, host_timestamp=%ums).\n", ret, dev_id,
                      qid, para.comm.host_timestamp);
        return;
    }

    return;
}

STATIC drvError_t queue_dfx_client(unsigned int dev_id, unsigned int qid, struct event_queue_query_result *queue_result)
{
    struct queue_common_para query_para;
    struct queue_msg_info msg_info;
    drvError_t ret;
    struct event_queue_query_result *result;

    if ((queue_result == NULL) || (dev_id >= MAX_DEVICE) || (qid >= MAX_SURPORT_QUEUE_NUM)) {
        QUEUE_LOG_ERR("input param is invalid, que_result is NULL=%d, dev_id=%u, qid=%u.\n", (queue_result == NULL),
                      dev_id, qid);
        return DRV_ERROR_INVALID_VALUE;
    }

    query_para.qid = (short unsigned int)qid;
    query_para.host_timestamp = get_cur_cpu_timestamp();

    msg_info.msg = (char *)&query_para;
    msg_info.msg_len = sizeof(struct queue_common_para);
    msg_info.subevent_id = DRV_SUBEVENT_QUEUE_DFX_MSG;

    ret = queue_send_event_sync(dev_id, QUEUE_SEND_NORMAL, QUEUE_SYNC_WAIT_TIMEOUT, &msg_info);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("queue_send_event_sync failed, ret=%d, devid=%u, qid=%u, host_timestamp=%ums.\n", ret, dev_id,
                      qid, query_para.host_timestamp);
        return ret;
    }

    result = (struct event_queue_query_result *)msg_info.result.data;
    queue_result->last_host_enqueue_time = result->last_host_enqueue_time;
    queue_result->last_host_dequeue_time = result->last_host_dequeue_time;
    return DRV_ERROR_NONE;
}

STATIC drvError_t queue_query_result(unsigned int dev_id, struct event_summary *event, struct event_proc_result *result)
{
    drvError_t ret;
    unsigned int qid;
    unsigned int host_timestamp;
    struct queue_enqueue_para *enqueue_para;
    struct event_queue_query_result queue_result;

    enqueue_para = (struct queue_enqueue_para *)(event->msg + sizeof(struct event_sync_msg));
    qid = enqueue_para->comm.qid;
    host_timestamp = enqueue_para->comm.host_timestamp;

    if ((result == NULL) || (dev_id >= MAX_DEVICE) || (qid >= MAX_SURPORT_QUEUE_NUM)) {
        QUEUE_LOG_ERR("input param is invalid. (dev_id=%u, qid=%u)\n", dev_id, qid);
        return DRV_ERROR_INNER_ERR;
    }

    if (queue_get_remote_info(dev_id, qid, QUEUE_INFO_RETRY) != true) {
        return DRV_ERROR_INNER_ERR;
    }
    queue_set_remote_info(dev_id, qid, QUEUE_INFO_RETRY, false);

    ret = queue_dfx_client(dev_id, qid, &queue_result);
    if (ret != 0) {
        result->ret = ret;
        QUEUE_LOG_ERR("queue_dfx_client failed, ret=%d.\n", ret);
        return ret;
    }

    if (event->subevent_id == DRV_SUBEVENT_ENQUEUE_MSG) {
        if (queue_result.last_host_enqueue_time == host_timestamp) { /* enqueue success */
            result->ret = DRV_ERROR_NONE;
        } else { /* enqueue fail, retry once */
            result->ret = DRV_ERROR_QUEUE_FULL;
        }
    }
    if (event->subevent_id == DRV_SUBEVENT_DEQUEUE_MSG) {
        if (queue_result.last_host_dequeue_time == host_timestamp) { /* dequeue success */
            result->ret = DRV_ERROR_NONE;
        } else { /* dequeue fail, retry once */
            result->ret = DRV_ERROR_QUEUE_EMPTY;
        }
    }
    QUEUE_RUN_LOG_INFO(
        "query queue success. (devid=%u; qid=%u; "
        "subevent_id=%u; host_time_tick=%u; last_host_enqueue_time_tick=%u; last_host_dequeue_time_tick=%u; ret=%d)\n",
        dev_id, qid, event->subevent_id, host_timestamp, queue_result.last_host_enqueue_time,
        queue_result.last_host_dequeue_time, result->ret);

    return DRV_ERROR_NONE;
}

STATIC drvError_t queue_get_qid_by_name_client(unsigned int dev_id, const char *name, unsigned int *qid)
{
    (void)dev_id;
    (void)name;
    (void)qid;
    return DRV_ERROR_NOT_SUPPORT;
}

STATIC drvError_t queue_get_qids_by_pid_client(unsigned int dev_id, unsigned int pid, unsigned int max_que_size,
                                               QidsOfPid *info)
{
    (void)dev_id;
    (void)pid;
    (void)max_que_size;
    (void)info;
    return DRV_ERROR_NOT_SUPPORT;
}

STATIC drvError_t queue_ctrl_event_client(struct QueueSubscriber *subscriber, QUE_EVENT_CMD cmd_type)
{
    (void)subscriber;
    (void)cmd_type;
    return DRV_ERROR_NOT_SUPPORT;
}

STATIC drvError_t queue_peek_data_client(unsigned int dev_id, unsigned int qid, unsigned int flag,
                                         QueuePeekDataType type, void **mbuf)
{
    (void)dev_id;
    (void)qid;
    (void)flag;
    (void)type;
    (void)mbuf;
    return DRV_ERROR_NOT_SUPPORT;
}

static drvError_t que_clt_api_subscribe(unsigned int dev_id, unsigned int qid, unsigned int subevent_id,
                                        struct event_res *res)
{
    struct queue_sub_para sub_para;
    struct queue_msg_info msg_info;
    drvError_t ret;

    sub_para.comm.qid = (short unsigned int)qid;
    sub_para.comm.host_timestamp = get_cur_cpu_timestamp();
    sub_para.pid = g_queue_tgid;
    sub_para.gid = (short unsigned int)res->gid;
    sub_para.tid = QUEUE_INVALID_VALUE;
    sub_para.event_id = (short unsigned int)res->event_id;
    sub_para.dst_phy_devid = QUEUE_INVALID_VALUE;
    sub_para.inner_sub_flag = QUEUE_INNER_SUB_FLAG;
    queue_fill_mcast_para(dev_id, &sub_para.comm.mcast_para);

    msg_info.msg = (char *)&sub_para;
    msg_info.msg_len = sizeof(struct queue_sub_para);
    msg_info.subevent_id = subevent_id;

    ret = queue_send_event_sync(dev_id, QUEUE_SEND_NORMAL, QUEUE_SYNC_WAIT_TIMEOUT, &msg_info);
    queue_update_finish_event_sn(dev_id, &sub_para.comm.mcast_para);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("queue_send_event_sync failed. (ret=%d, devid=%u, qid=%u, host_timestamp=%ums).\n", ret, dev_id,
                      qid, sub_para.comm.host_timestamp);
        return ret;
    }

    queue_try_disbale_event_mcast(dev_id, msg_info.result.data[0]);

    return DRV_ERROR_NONE;
}

static drvError_t que_clt_api_unsubscribe(unsigned int dev_id, unsigned int qid, unsigned int subevent_id)
{
    struct queue_common_para unsub_para;
    struct queue_msg_info msg_info;
    drvError_t ret;

    unsub_para.qid = (short unsigned int)qid;
    unsub_para.host_timestamp = get_cur_cpu_timestamp();
    queue_fill_mcast_para(dev_id, &unsub_para.mcast_para);

    msg_info.msg = (char *)&unsub_para;
    msg_info.msg_len = sizeof(struct queue_common_para);
    msg_info.subevent_id = subevent_id;

    ret = queue_send_event_sync(dev_id, QUEUE_SEND_NORMAL, QUEUE_SYNC_WAIT_TIMEOUT, &msg_info);
    queue_update_finish_event_sn(dev_id, &unsub_para.mcast_para);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("queue_send_event_sync failed. (ret=%d; devid=%u; qid=%u; host_timestamp=%ums)\n", ret, dev_id,
                      qid, unsub_para.host_timestamp);
        return ret;
    }

    queue_try_disbale_event_mcast(dev_id, msg_info.result.data[0]);

    return DRV_ERROR_NONE;
}

STATIC void queue_set_arg_event_info(struct event_summary *event, struct event_res *res, void *arg)
{
    struct sched_published_event_info *event_info = (struct sched_published_event_info *)arg;

    ((struct event_sync_msg *)event->msg)->dst_engine = CCPU_HOST;
    ((struct event_sync_msg *)event->msg)->pid = (uint32_t)queue_get_hostpid() & 0x3FFFFF;
    ((struct event_sync_msg *)event->msg)->gid = (uint32_t)res->gid & 0x3F;
    ((struct event_sync_msg *)event->msg)->event_id = (uint32_t)res->event_id & 0x3F;
    ((struct event_sync_msg *)event->msg)->subevent_id = res->subevent_id;

    event_info->dst_engine = event->dst_engine;
    event_info->pid = event->pid;
    event_info->gid = event->grp_id;
    event_info->event_id = (unsigned int)event->event_id;
    event_info->subevent_id = event->subevent_id;
    event_info->msg_len = event->msg_len;
    event_info->msg = event->msg;
    event_info->policy = (unsigned int)event->policy;
}

STATIC bool queue_is_need_sync_wait(unsigned int dev_id, unsigned int qid, unsigned int subevent_id)
{
    /* The user has subscribed events, so we does not need to wait. */
    if (subevent_id == DRV_SUBEVENT_ENQUEUE_MSG) {
        if (queue_get_remote_info(dev_id, qid, QUEUE_INFO_F2NF) == true) {
            QUEUE_LOG_INFO("sub event does not need to wait when enque. (dev_id=%u; qid=%u)\n", dev_id, qid);
            return false;
        }
    } else {
        if (queue_get_remote_info(dev_id, qid, QUEUE_INFO_ENQUE) == true) {
            QUEUE_LOG_INFO("sub event does not need to wait when deque. (dev_id=%u; qid=%u)\n", dev_id, qid);
            return false;
        }
    }

    return true;
}

STATIC drvError_t queue_submit_event_sync_send_msg(unsigned int dev_id, struct event_summary *event, void *arg,
                                                   struct event_proc_result *result)
{
    unsigned long long timestamp = esched_get_cur_cpu_timestamp();
    unsigned int qid = MAX_SURPORT_QUEUE_NUM;
    struct timeval start, ioctl_end, wait_end;
    struct event_info back_event = {{0}, {0}};
    int wait_timeout = QUEUE_SYNC_WAIT_TIMEOUT;
    unsigned long long buf_len = 0;
    long ioctl_cost, wait_cost;
    int wait_succ_cnt = 0;
    struct event_res res;
    drvError_t ret;

    que_get_time(&start);

    ret = (drvError_t)esched_alloc_event_res(dev_id, (int)QUEUE_EVENT, &res);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("alloc event res failed. (ret=%d; dev_id=%u)\n", ret, dev_id);
        return DRV_ERROR_INNER_ERR;
    }

    queue_set_arg_event_info(event, &res, arg);
    ret = queue_ctrl_msg_send(arg);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("send msg ioctl failed. (ret=%d; event_id=%u; gid=%u; tid=%u; timeout=%dms; subevent_id=%u).\n",
                      ret, res.event_id, res.gid, res.tid, wait_timeout, res.subevent_id);
        esched_free_event_res(dev_id, (int)QUEUE_EVENT, &res);
        return ret;
    }
    que_get_time(&ioctl_end);

    do {
        ret = (drvError_t)halEschedWaitEvent(dev_id, res.gid, res.tid, wait_timeout, &back_event);
        if (ret != DRV_ERROR_NONE) {
            QUEUE_LOG_ERR("wait event failed. (ret=%d; event_id=%u; gid=%u; tid=%u; wait_succ_cnt=%d; subevent_id=%u, "
                          "cur_tick=%llu)\n",
                          ret, res.event_id, res.gid, res.tid, wait_succ_cnt, res.subevent_id, timestamp);
            esched_free_event_res(dev_id, (int)QUEUE_EVENT, &res);
            return DRV_ERROR_INNER_ERR;
        }

        wait_succ_cnt++;
        if ((back_event.comm.submit_timestamp >= timestamp) && (back_event.comm.subevent_id == res.subevent_id)) {
            if (back_event.priv.msg_len != sizeof(struct event_proc_result)) {
                esched_free_event_res(dev_id, (int)QUEUE_EVENT, &res);
                QUEUE_LOG_ERR("reply len not equal to buf len. (reply_len=%u; buf_len=%ld).\n", back_event.priv.msg_len,
                              sizeof(struct event_proc_result));
                return DRV_ERROR_PARA_ERROR;
            }
            break;
        }

        QUEUE_RUN_LOG_INFO("successfully waited for an event but the condition was not met. (devid=%u; gid=%u; "
                           "tid=%u; cnt=%d; check_time=%lu; back_time=%llu; check_subevent=%u; back_subevent=%u)\n",
                           dev_id, res.gid, res.tid, wait_succ_cnt, timestamp, back_event.comm.submit_timestamp,
                           res.subevent_id, back_event.comm.subevent_id);
    } while (1);

    que_get_time(&wait_end);

    ioctl_cost = (ioctl_end.tv_sec - start.tv_sec) * US_PER_SECOND + ioctl_end.tv_usec - start.tv_usec;
    wait_cost = (wait_end.tv_sec - ioctl_end.tv_sec) * US_PER_SECOND + wait_end.tv_usec - ioctl_end.tv_usec;

    if ((wait_cost / MS_PER_SECOND) > QUEUE_SYNC_TIMEOUT) {
        QUEUE_RUN_LOG_INFO(
            "send msg wait queue event too long. (dev_id=%u; qid=%u; buf_len=%llu; event_id=%u; gid=%u; tid=%u; "
            "timeout=%dms;"
            " ioctl_cost=%ldus; wait_cost=%ldus).\n",
            dev_id, qid, buf_len, res.event_id, res.gid, res.tid, wait_timeout, ioctl_cost, wait_cost);
    }

    esched_free_event_res(dev_id, (int)QUEUE_EVENT, &res);
    (void)memcpy_s(result, sizeof(struct event_proc_result), back_event.priv.msg, sizeof(struct event_proc_result));

    return DRV_ERROR_NONE;
}

STATIC drvError_t queue_submit_event_sync_queue_enqueue(unsigned int dev_id, struct event_summary *event, void *arg,
                                                        struct event_proc_result *result)
{
    unsigned long long timestamp = esched_get_cur_cpu_timestamp();
    unsigned int qid = MAX_SURPORT_QUEUE_NUM;
    struct timeval start, ioctl_end, wait_end;
    struct event_info back_event = {{0}, {0}};
    int timeout = QUEUE_SYNC_WAIT_TIMEOUT;
    unsigned long long buf_len = 0;
    long ioctl_cost, wait_cost;
    int wait_succ_cnt = 0;
    struct event_res res;
    drvError_t ret;
    struct queue_enqueue_para *queue_para;

    que_get_time(&start);
    queue_para = (struct queue_enqueue_para *)(event->msg + sizeof(struct event_sync_msg));
    qid = queue_para->comm.qid;
    buf_len = queue_para->buf_len;

    ret = (drvError_t)esched_alloc_event_res(dev_id, (int)QUEUE_EVENT, &res);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("alloc event res failed. (ret=%d; dev_id=%u)\n", ret, dev_id);
        return DRV_ERROR_INNER_ERR;
    }

    queue_set_arg_event_info(event, &res, arg);
    ret = queue_enqueue_cmd(arg);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR(
            "queue enqueue ioctl failed. (ret=%d; event_id=%u; gid=%u; tid=%u; timeout=%dms; subevent_id=%u).\n", ret,
            res.event_id, res.gid, res.tid, timeout, res.subevent_id);
        esched_free_event_res(dev_id, (int)QUEUE_EVENT, &res);
        return ret;
    }
    que_get_time(&ioctl_end);

    do {
        ret = (drvError_t)halEschedWaitEvent(dev_id, res.gid, res.tid, timeout, &back_event);
        if (ret != DRV_ERROR_NONE) {
            QUEUE_LOG_ERR("queue enqueue wait event failed. (ret=%d; event_id=%u; gid=%u; tid=%u; wait_succ_cnt=%d; "
                          "subevent_id=%u, cur_tick=%llu)\n",
                          ret, res.event_id, res.gid, res.tid, wait_succ_cnt, res.subevent_id, timestamp);
            esched_free_event_res(dev_id, (int)QUEUE_EVENT, &res);
            ret = queue_query_result(dev_id, event, result);
            return ret;
        }

        wait_succ_cnt++;
        if ((back_event.comm.submit_timestamp >= timestamp) && (back_event.comm.subevent_id == res.subevent_id)) {
            if (back_event.priv.msg_len != sizeof(struct event_proc_result)) {
                esched_free_event_res(dev_id, (int)QUEUE_EVENT, &res);
                QUEUE_LOG_ERR("reply len not equal to buf len. (reply_len=%u; buf_len=%ld).\n", back_event.priv.msg_len,
                              sizeof(struct event_proc_result));
                return DRV_ERROR_PARA_ERROR;
            }
            break;
        }

        QUEUE_RUN_LOG_INFO("successfully waited for an event but the condition was not met. (devid=%u; gid=%u; "
                           "tid=%u; cnt=%d; check_time=%lu; back_time=%llu; check_subevent=%u; back_subevent=%u)\n",
                           dev_id, res.gid, res.tid, wait_succ_cnt, timestamp, back_event.comm.submit_timestamp,
                           res.subevent_id, back_event.comm.subevent_id);
    } while (1);

    que_get_time(&wait_end);

    ioctl_cost = (ioctl_end.tv_sec - start.tv_sec) * US_PER_SECOND + ioctl_end.tv_usec - start.tv_usec;
    wait_cost = (wait_end.tv_sec - ioctl_end.tv_sec) * US_PER_SECOND + wait_end.tv_usec - ioctl_end.tv_usec;

    if ((wait_cost / MS_PER_SECOND) > QUEUE_SYNC_TIMEOUT) {
        QUEUE_RUN_LOG_INFO(
            "queue enqueue wait queue event too long. (dev_id=%u; qid=%u; buf_len=%llu; event_id=%u; gid=%u; tid=%u; "
            "timeout=%dms;"
            " ioctl_cost=%ldus; wait_cost=%ldus).\n",
            dev_id, qid, buf_len, res.event_id, res.gid, res.tid, timeout, ioctl_cost, wait_cost);
    }

    esched_free_event_res(dev_id, (int)QUEUE_EVENT, &res);
    (void)memcpy_s(result, sizeof(struct event_proc_result), back_event.priv.msg, sizeof(struct event_proc_result));

    return DRV_ERROR_NONE;
}

STATIC drvError_t queue_send_event_sync_timeout(unsigned int dev_id, unsigned int qid, struct queue_msg_info *msg_info,
                                                int timeout)
{
    struct event_summary event_info;
    struct event_reply reply;
    drvError_t ret;

    ret = queue_fill_event(&event_info, dev_id, msg_info->subevent_id, msg_info->msg, msg_info->msg_len);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("queue_fill_event failed, ret=%d.\n", ret);
        return ret;
    }

    reply.buf_len = sizeof(struct event_proc_result);
    reply.buf = (char *)&msg_info->result;

    while (1) {
        struct timeval start, end;

        ret = halEschedSubmitEventSync(dev_id, &event_info, QUEUE_SYNC_TIMEOUT, &reply);
        if (ret != DRV_ERROR_NONE) {
            QUEUE_LOG_ERR("halEschedSubmitEventSync failed .(ret=%d; dev_id=%u; qid=%u).\n", ret, dev_id, qid);
            queue_clear_event(&event_info);
            return DRV_ERROR_INNER_ERR;
        }

        if (reply.reply_len != reply.buf_len) {
            QUEUE_LOG_ERR("reply_len(%u) not equal to buf_len(%u).\n", reply.reply_len, reply.buf_len);
            queue_clear_event(&event_info);
            return DRV_ERROR_PARA_ERROR;
        }

        if (msg_info->result.ret != msg_info->result_ret) {
            break;
        }

        if (!queue_is_need_sync_wait(dev_id, qid, msg_info->subevent_id)) {
            break;
        }

        que_get_time(&start);
        ret = queue_wait_event(dev_id, qid, msg_info->result.ret, timeout);
        que_get_time(&end);
        if (ret != DRV_ERROR_NONE) {
            break;
        }
        queue_updata_timeout(start, end, &timeout);

        if (msg_info->subevent_id == DRV_SUBEVENT_PEEK_MSG) {
            /* next peek not mcast */
            struct queue_common_para *peek_para = (struct queue_common_para *)(void *)((struct event_sync_msg *)
                                                                                           event_info.msg +
                                                                                       1);
            peek_para->mcast_para.mcast_flag = 0;
        }
    };
    queue_clear_event(&event_info);

    return (drvError_t)msg_info->result.ret;
}

static drvError_t que_clt_api_peek(unsigned int dev_id, unsigned int qid, uint64_t *buf_len, int timeout)
{
    struct queue_common_para peek_para;
    struct queue_msg_info msg_info;
    int timeout_val = timeout;
    drvError_t ret;

    peek_para.qid = (short unsigned int)qid;
    peek_para.host_timestamp = get_cur_cpu_timestamp();
    queue_fill_mcast_para(dev_id, &peek_para.mcast_para);

    msg_info.msg = (char *)&peek_para;
    msg_info.msg_len = sizeof(struct queue_common_para);
    msg_info.result_ret = (int)DRV_ERROR_QUEUE_EMPTY;
    msg_info.subevent_id = DRV_SUBEVENT_PEEK_MSG;

    ret = queue_send_event_sync_timeout(dev_id, qid, &msg_info, timeout_val);
    if (ret == DRV_ERROR_NONE) {
        /* uint64_t require 8 byte alignment, struct shareMbuf will cause UT fail.
         * solution: make 4 byte alignment.
         */
        *(uint32_t *)buf_len = *((uint32_t *)msg_info.result.data);
        *((uint32_t *)buf_len + 1) = *((uint32_t *)msg_info.result.data + 1);
        QUEUE_LOG_DEBUG("buf_len=%llu.\n", (unsigned long long)*buf_len);
    }

    queue_update_finish_event_sn(dev_id, &peek_para.mcast_para);

    return ret;
}

STATIC int queue_calc_send_timeout(int timeout)
{
    if (timeout < 0) {
        return QUEUE_WAIT_TIMEOUT_ALWAYS_WAIT;
    } else if (timeout < (QUEUE_MAX_TIMEOUT_VALUE_INT - QUEUE_MSG_IRQ_BASE_TIMEOUT)) {
        return (timeout + QUEUE_MSG_IRQ_BASE_TIMEOUT);
    } else {
        return timeout;
    }
}

STATIC drvError_t queue_send_queue_event_sync_timeout(unsigned int dev_id, unsigned int qid,
                                                      struct queue_msg_info *msg_info, struct buff_iovec *vector,
                                                      int timeout)
{
    struct queue_enqueue_para *para = (struct queue_enqueue_para *)msg_info->msg;
    struct event_proc_result result;
    struct queue_enqueue_stru arg;
    struct event_summary event;
    drvError_t ret = queue_fill_event(&event, dev_id, msg_info->subevent_id, msg_info->msg, msg_info->msg_len);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("queue_fill_event failed. (ret=%d; dev_id=%u; qid=%u)\n", ret, dev_id, qid);
        return ret;
    }

    arg.devid = para->comm.devid;
    arg.qid = para->comm.qid;
    arg.type = para->type;
    arg.vector = vector;
    arg.time_out = queue_calc_send_timeout(timeout);
    arg.iovec_count = vector->count;
    queue_set_remote_info(dev_id, qid, QUEUE_INFO_RETRY, true);

    while (1) {
        struct timeval start, end;

        ret = queue_submit_event_sync_queue_enqueue(dev_id, &event, &arg, &result);
        if (ret != DRV_ERROR_NONE) {
            QUEUE_LOG_ERR("submit event failed. (ret=%d; dev_id=%u; qid=%u; timeout=%dms; host_timestamp=%ums)\n", ret,
                          dev_id, qid, timeout, para->comm.host_timestamp);
            queue_clear_event(&event);
            return ret;
        }

        queue_try_disbale_event_mcast(dev_id, result.data[0]);

        if (result.ret != msg_info->result_ret) {
            break;
        }

        if (!queue_is_need_sync_wait(dev_id, qid, msg_info->subevent_id)) {
            break;
        }

        para = (struct queue_enqueue_para *)(void *)((struct event_sync_msg *)event.msg + 1); /* next event not mcast */
        para->comm.mcast_para.mcast_flag = 0;

        que_get_time(&start);
        ret = queue_wait_event(dev_id, qid, result.ret, timeout);
        que_get_time(&end);
        if (ret != DRV_ERROR_NONE) {
            break;
        }
        queue_updata_timeout(start, end, &timeout);
    };
    queue_clear_event(&event);
    queue_report_insufficient_device_memory(vector, result.ret);
    return (drvError_t)result.ret;
}

STATIC inline bool queue_is_bare_buff(const void *buff)
{
    struct DVattribute attr;
    drvError_t ret;

    ret = drvMemGetAttribute((uint64_t)(uintptr_t)buff, &attr);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_DEBUG("call drvMemGetAttribute failed. (ret=%d; ptr=0x%llx)\n", ret,
                        (unsigned long long)(uintptr_t)buff);
        return false;
    }

    if (attr.memType == DV_MEM_LOCK_DEV || attr.memType == DV_MEM_LOCK_DEV_DVPP) {
        return true;
    }

    return false;
}

static drvError_t que_clt_api_enque(unsigned int dev_id, unsigned int qid, struct buff_iovec *vector, int timeout)
{
    struct queue_enqueue_para enqueue_para = {{0}, 0, 0, 0, 0, 0};
    struct queue_msg_info msg_info;
    bool is_bare_buff = false;
    drvError_t ret;
    unsigned int i;

    ret = queue_init_check(dev_id, qid);
    if (ret != 0) {
        return ret;
    }

    is_bare_buff = queue_is_bare_buff(vector->ptr[0].iovec_base);
    if (is_bare_buff) {
        if (vector->count > 1) {
            enqueue_para.copy_flag = 1;
        }
        enqueue_para.type = QUEUE_BARE_BUFF;
        enqueue_para.bare_buff = (uintptr_t)vector->ptr[0].iovec_base;
    } else {
        enqueue_para.type = QUEUE_BUFF;
    }
    enqueue_para.comm.host_timestamp = get_cur_cpu_timestamp();
    enqueue_para.comm.devid = (short unsigned int)dev_id;
    enqueue_para.comm.qid = (short unsigned int)qid;
    enqueue_para.hostpid = (uint32_t)queue_get_hostpid() & 0x1FFFFFFF;
    queue_fill_mcast_para(dev_id, &enqueue_para.comm.mcast_para);

    for (i = 0; i < vector->count; i++) {
        enqueue_para.buf_len += vector->ptr[i].len;
    }

    msg_info.msg = (char *)&enqueue_para;
    msg_info.msg_len = sizeof(struct queue_enqueue_para);
    msg_info.result_ret = (int)DRV_ERROR_QUEUE_FULL;
    msg_info.subevent_id = DRV_SUBEVENT_ENQUEUE_MSG;

    ret = queue_send_queue_event_sync_timeout(dev_id, qid, &msg_info, vector, timeout);
    if (ret != 0) {
        QUEUE_LOG_DEBUG("queue_send_queue_event_sync_timeout failed, ret=%d.\n", ret);
    }

    queue_update_finish_event_sn(dev_id, &enqueue_para.comm.mcast_para);

    return (ret < 0) ? DRV_ERROR_INNER_ERR : (drvError_t)ret;
}

static drvError_t que_clt_api_deque(unsigned int dev_id, unsigned int qid, struct buff_iovec *vector, int timeout)
{
    struct queue_enqueue_para dequeue_para = {{0}, 0, 0, 0, 0, 0};
    struct queue_msg_info msg_info;
    bool is_bare_buff = false;
    unsigned int i;
    drvError_t ret;

    ret = queue_init_check(dev_id, qid);
    if (ret != 0) {
        return ret;
    }

    is_bare_buff = queue_is_bare_buff(vector->ptr[0].iovec_base);
    if (is_bare_buff == true) {
        if (vector->count != 1) {
            QUEUE_LOG_ERR("input param is invalid. (devid=%u; count=%u)\n", dev_id, vector->count);
            return DRV_ERROR_INVALID_VALUE;
        }
        dequeue_para.type = QUEUE_BARE_BUFF;
        dequeue_para.bare_buff = (uintptr_t)vector->ptr[0].iovec_base;
    } else {
        dequeue_para.type = QUEUE_BUFF;
    }
    dequeue_para.comm.host_timestamp = get_cur_cpu_timestamp();
    dequeue_para.comm.devid = (short unsigned int)dev_id;
    dequeue_para.comm.qid = (short unsigned int)qid;
    dequeue_para.hostpid = (uint32_t)queue_get_hostpid() & 0x1FFFFFFF;
    queue_fill_mcast_para(dev_id, &dequeue_para.comm.mcast_para);

    for (i = 0; i < vector->count; i++) {
        dequeue_para.buf_len += vector->ptr[i].len;
    }

    msg_info.msg = (char *)&dequeue_para;
    msg_info.msg_len = sizeof(struct queue_enqueue_para);
    msg_info.result_ret = (int)DRV_ERROR_QUEUE_EMPTY;
    msg_info.subevent_id = DRV_SUBEVENT_DEQUEUE_MSG;

    ret = queue_send_queue_event_sync_timeout(dev_id, qid, &msg_info, vector, timeout);
    if (ret != 0) {
        QUEUE_LOG_DEBUG("queue_send_queue_event_sync_timeout failed, ret=%d.\n", ret);
    }
    queue_update_finish_event_sn(dev_id, &dequeue_para.comm.mcast_para);
    return (ret < 0) ? DRV_ERROR_INNER_ERR : (drvError_t)ret;
}

STATIC struct queue_comm_interface_list g_client_remote_interface = {
    .queue_dc_init = queue_init_client,
    .queue_uninit = queue_uninit_client,
    .queue_create = queue_create_client,
    .queue_grant = queue_grant_client,
    .queue_attach = queue_attach_client,
    .queue_destroy = queue_destroy_client,
    .queue_reset = queue_reset_client,
    .queue_en_queue = queue_en_queue_client,
    .queue_de_queue = queue_de_queue_client,
    .queue_subscribe = queue_subscribe_client,
    .queue_unsubscribe = queue_unsubscribe_client,
    .queue_sub_f_to_nf_event = queue_sub_f_to_nf_event_client,
    .queue_unsub_f_to_nf_event = queue_unsub_f_to_nf_event_client,
    .queue_sub_event = queue_sub_event_client,
    .queue_unsub_event = queue_unsub_event_client,
    .queue_ctrl_event = queue_ctrl_event_client,
    .queue_query_info = queue_query_info_client,
    .queue_get_status = queue_get_status_client,
    .queue_get_qid_by_name = queue_get_qid_by_name_client,
    .queue_get_qids_by_pid = queue_get_qids_by_pid_client,
    .queue_query = queue_query_client,
    .queue_peek_data = queue_peek_data_client,
    .queue_set = queue_set_client,
    .queue_finish_cb = queue_finish_callback_client,
    .queue_export = NULL,
    .queue_unexport = NULL,
    .queue_import = NULL,
    .queue_unimport = NULL,
};

static struct que_clt_api g_clt_pci_api = {.api_peek = que_clt_api_peek,
                                           .api_enque_buf = que_clt_api_enque,
                                           .api_deque_buf = que_clt_api_deque,
                                           .api_subscribe = que_clt_api_subscribe,
                                           .api_unsubscribe = que_clt_api_unsubscribe};

struct que_clt_api *que_clt_pci_get_api(void)
{
    queue_set_comm_interface(CLIENT_QUEUE, &g_client_remote_interface);
    queue_remote_info_rwlock_init();
    return &g_clt_pci_api;
}
