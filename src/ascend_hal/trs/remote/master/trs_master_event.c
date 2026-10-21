/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <semaphore.h>
#include <string.h>
#include <pthread.h>

#include "securec.h"
#include "ascend_hal.h"
#include "ascend_hal_external.h"

#include "trs_uk_msg.h"
#include "esched_user_interface.h"
#include "trs_user_pub_def.h"
#include "trs_res.h"
#include "trs_sqcq.h"
#include "trs_shr_id_user.h"
#include "trs_master_event.h"
#include "trs_dev_drv.h"

#define TRS_MAX_CONCURRENCY_NUM 32
int g_trs_hostpid[TRS_DEV_NUM] = {0};
int g_trs_devpid[TRS_DEV_NUM] = {0};
static sem_t g_trs_event_sem;

enum trs_event_grp_type {
    TRS_SYNC_EVENT_GROUP_TYPE_DRV_MSG = 0,
    TRS_SYNC_EVENT_GROUP_TYPE_PROXY_HOST_QUEUE,
    TRS_SYNC_EVENT_GROUP_TYPE_MAX,
};

const char *g_trs_grp_name[TRS_SYNC_EVENT_GROUP_TYPE_MAX] = {EVENT_DRV_MSG_GRP_NAME, PROXY_HOST_QUEUE_GRP_NAME};
static int g_trs_devgid[TRS_DEV_NUM][TRS_SYNC_EVENT_GROUP_TYPE_MAX] = {{0}};

static pid_t trs_get_hostpid(uint32_t dev_id)
{
    if (g_trs_hostpid[dev_id] == 0) {
        g_trs_hostpid[dev_id] = getpid();
    }

    return g_trs_hostpid[dev_id];
}

static pid_t trs_get_dev_pid(uint32_t dev_id)
{
    if (g_trs_devpid[dev_id] == 0) {
        struct halQueryDevpidInfo hostpidinfo = {0};
        drvError_t ret;
        int devpid;

        hostpidinfo.proc_type = DEVDRV_PROCESS_CP1;
        hostpidinfo.hostpid = trs_get_hostpid(dev_id);
        hostpidinfo.devid = dev_id;
        hostpidinfo.vfid = 0;
        ret = halQueryDevpid(hostpidinfo, &devpid);
        if (ret) {
            trs_err("Query devpid failed. (dev_id=%u; vfid=%u; hostpid=%d; proc_type=%u; ret=%d).\n", dev_id, 0,
                    trs_get_hostpid(dev_id), hostpidinfo.proc_type, ret);
        } else {
            g_trs_devpid[dev_id] = devpid;
            trs_info("Query devpid succ. (dev_id=%u; vfid=%u; hostpid=%d; proc_type=%u; devpid=%d).\n", dev_id, 0,
                     trs_get_hostpid(dev_id), hostpidinfo.proc_type, devpid);
        }
    }

    return g_trs_devpid[dev_id];
}

static drvError_t trs_get_event_grp_id(uint32_t devid, uint32_t *grp_id, enum trs_event_grp_type grp_type)
{
    drvError_t ret = 0;

    if (g_trs_devgid[devid][grp_type] == -1) {
        const char *grp_name = g_trs_grp_name[grp_type];
        struct esched_query_gid_output gid_out = {0};
        struct esched_query_gid_input gid_in = {0};
        struct esched_output_info output = {0};
        struct esched_input_info input = {0};
        size_t grp_name_len;

        gid_in.pid = trs_get_dev_pid(devid);
        grp_name_len = strlen(grp_name);
        (void)memcpy_s(gid_in.grp_name, EVENT_MAX_GRP_NAME_LEN, grp_name, grp_name_len);
        input.inBuff = &gid_in;
        input.inLen = sizeof(struct esched_query_gid_input);
        output.outBuff = &gid_out;
        output.outLen = sizeof(struct esched_query_gid_output);
        ret = halEschedQueryInfo(devid, QUERY_TYPE_REMOTE_GRP_ID, &input, &output);
        if (ret == DRV_ERROR_NONE) {
            g_trs_devgid[devid][grp_type] = (int)gid_out.grp_id;
        } else if (ret == DRV_ERROR_UNINIT) {
            g_trs_devgid[devid][grp_type] = 0; // PROXY_HOST_GRP_NAME grp not exist, use default grpid 0.
        } else {
            trs_err("Query grpid failed. (ret=%d; devid=%u; devpid=%d).\n", ret, devid, gid_in.pid);
            return ret;
        }
    }

    *grp_id = (uint32_t)g_trs_devgid[devid][grp_type];
    return DRV_ERROR_NONE;
}

static inline enum trs_event_grp_type trs_get_group_type(uint32_t subevent_id)
{
    if (((subevent_id >= DRV_SUBEVENT_TRS_ALLOC_SQCQ_WITH_URMA_MSG) &&
         (subevent_id <= DRV_SUBEVENT_TRS_FILL_WQE_MSG)) ||
        (subevent_id == DRV_SUBEVENT_TRS_RES_ID_CONFIG_MSG)) {
        return TRS_SYNC_EVENT_GROUP_TYPE_DRV_MSG;
    }
    return TRS_SYNC_EVENT_GROUP_TYPE_PROXY_HOST_QUEUE;
}

static uint32_t trs_get_dst_engine(uint32_t subevent_id)
{
    if (((subevent_id >= DRV_SUBEVENT_TRS_ALLOC_SQCQ_WITH_URMA_MSG) &&
         (subevent_id <= DRV_SUBEVENT_TRS_FILL_WQE_MSG)) ||
        (subevent_id == DRV_SUBEVENT_TRS_RES_ID_CONFIG_MSG)) {
        return CCPU_DEVICE;
    }
    return ACPU_DEVICE;
}

static drvError_t trs_fill_event(struct event_summary *event_info, uint32_t devid, uint32_t subevent_id,
                                 const char *msg, uint32_t msg_len)
{
    struct event_sync_msg *msg_head = NULL;
    drvError_t ret;

    ret = trs_get_event_grp_id(devid, &event_info->grp_id, trs_get_group_type(subevent_id));
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }
    event_info->dst_engine = trs_get_dst_engine(subevent_id);
    event_info->policy = ONLY;
    event_info->event_id = EVENT_DRV_MSG;
    event_info->subevent_id = subevent_id;
    event_info->pid = trs_get_dev_pid(devid);
    event_info->msg_len = (unsigned int)sizeof(struct event_sync_msg) + msg_len;
    event_info->msg = malloc(event_info->msg_len);
    if (event_info->msg == NULL) {
        trs_err("malloc %u failed.\n", event_info->msg_len);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    msg_head = (struct event_sync_msg *)event_info->msg;
    msg_head->dst_engine = CCPU_HOST;

    if (msg_len == 0) {
        return DRV_ERROR_NONE;
    }

    ret = (drvError_t)memcpy_s(event_info->msg + sizeof(struct event_sync_msg), msg_len, msg, msg_len);
    if (ret != DRV_ERROR_NONE) {
#ifndef EMU_ST
        trs_err("memcpy_s failed, ret=%d.\n", ret);
        free(event_info->msg);
        event_info->msg = NULL;
        return DRV_ERROR_PARA_ERROR;
#endif
    }

    return DRV_ERROR_NONE;
}

static void trs_clear_event(struct event_summary *event_info)
{
    if (event_info == NULL || event_info->msg == NULL) {
        return;
    }

    free(event_info->msg);
    event_info->msg = NULL;
}

#define TRS_SQCQ_EVENT_THREAD_MAX 16U
#define TRS_SQCQ_EVENT_GRP_NAME "trs_sqcq_grp"
unsigned int g_sqcq_gid[TRS_DEV_NUM];
pthread_mutex_t g_trs_event_sync_mutex[TRS_DEV_NUM];

static int trs_event_query_gid(unsigned int dev_id, unsigned int *grp_id)
{
    struct esched_query_gid_output gid_out = {0};
    struct esched_query_gid_input gid_in = {0};
    struct esched_output_info output = {0};
    struct esched_input_info input = {0};
    drvError_t ret;

    gid_in.pid = (int)getpid();
    (void)strcpy_s(gid_in.grp_name, EVENT_MAX_GRP_NAME_LEN, TRS_SQCQ_EVENT_GRP_NAME);
    input.inBuff = &gid_in;
    input.inLen = sizeof(struct esched_query_gid_input);
    output.outBuff = &gid_out;
    output.outLen = sizeof(struct esched_query_gid_output);
    ret = halEschedQueryInfo(dev_id, QUERY_TYPE_LOCAL_GRP_ID, &input, &output);
    if (ret == DRV_ERROR_NONE) {
        *grp_id = gid_out.grp_id;
    }

    return ret;
}

static int trs_sq_cq_event_init(uint32_t dev_id)
{
    struct esched_grp_para grp_para = {0};
    unsigned long event_bitmap = (0x1 << EVENT_DRV_MSG);
    struct event_info event = {0};
    unsigned int tid, sqcq_gid;
    int ret;

    ret = halEschedAttachDevice(dev_id);
    if (ret != DRV_ERROR_NONE) {
        trs_err("Call halEschedAttachDevice failed. (dev_id=%u, ret=%d)\n", dev_id, ret);
        return ret;
    }

    ret = trs_event_query_gid(dev_id, &sqcq_gid);
    if (ret == DRV_ERROR_NONE) {
        return DRV_ERROR_NONE;
    }

    grp_para.type = GRP_TYPE_BIND_CP_CPU;
    grp_para.threadNum = TRS_SQCQ_EVENT_THREAD_MAX;
    (void)strcpy_s(grp_para.grp_name, EVENT_MAX_GRP_NAME_LEN, TRS_SQCQ_EVENT_GRP_NAME);

    ret = halEschedCreateGrpEx(dev_id, &grp_para, &sqcq_gid);
    if (ret != DRV_ERROR_NONE) {
        (void)halEschedDettachDevice(dev_id);
        trs_err("Call halEschedCreateGrp failed. (dev_id=%u, ret=%d)\n", dev_id, ret);
    }
    g_sqcq_gid[dev_id] = sqcq_gid;

    for (tid = 0; tid < TRS_SQCQ_EVENT_THREAD_MAX; tid++) {
        ret = halEschedSubscribeEvent(dev_id, sqcq_gid, tid, event_bitmap);
        if (ret != DRV_ERROR_NONE) {
            (void)halEschedDettachDevice(dev_id);
            trs_err("Call halEschedSubscribeEvent failed. (dev_id=%u, tid=%u; ret=%d)\n", dev_id, tid, ret);
            return ret;
        }
        (void)halEschedWaitEvent(dev_id, sqcq_gid, tid, 0, &event);
    }
    (void)pthread_mutex_init(&g_trs_event_sync_mutex[dev_id], NULL);
    trs_debug("Sqcq event init success. (dev_id=%u)\n", dev_id);
    return DRV_ERROR_NONE;
}

static void trs_sq_cq_event_un_init(uint32_t dev_id)
{
    (void)halEschedDettachDevice(dev_id);
}

#ifndef EMU_ST
static drvError_t trs_submit_event_sync(uint32_t dev_id, struct event_summary *event, int32_t timeout,
                                        struct event_reply *reply)
{
    struct event_info back_event_info = {0};
    esched_event_buffer *event_buffer = (esched_event_buffer *)back_event_info.priv.msg;
    struct event_res res;
    int wait_succ_cnt = 0;
    drvError_t ret;

    if (event == NULL || reply == NULL) {
        trs_err("The variable event or reply is NULL.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    if (event->msg == NULL || event->msg_len < sizeof(struct event_sync_msg)) {
        trs_err("The event->msg is NULL or event->msg_len is invalid. (msg_len=%u)\n", event->msg_len);
        return DRV_ERROR_PARA_ERROR;
    }

    res.event_id = EVENT_DRV_MSG;
    res.gid = g_sqcq_gid[dev_id];
    res.subevent_id = (unsigned int)(event->subevent_id & 0xfff);
    res.tid = 0;
    esched_fill_sync_msg(event, &res);
    (void)pthread_mutex_lock(&g_trs_event_sync_mutex[dev_id]);
    ret = halEschedSubmitEvent(dev_id, event);
    if (ret != DRV_ERROR_NONE) {
        (void)pthread_mutex_unlock(&g_trs_event_sync_mutex[dev_id]);
        trs_warn("Unable to invoke the halEschedSubmitEvent. (dev_id=%u, event_id=%u; ret=%d)\n", dev_id,
                 event->event_id, ret);
        return ret;
    }

    event_buffer->msg = reply->buf;
    event_buffer->msg_len = reply->buf_len;

    do {
        ret = esched_wait_event_ex(dev_id, res.gid, res.tid, timeout, &back_event_info);
        if (ret != DRV_ERROR_NONE) {
            (void)pthread_mutex_unlock(&g_trs_event_sync_mutex[dev_id]);
            trs_err("Failed. (ret=%d; event_id=%u; gid=%u; tid=%u; timeout=%dms; subevent_id=%u)\n", ret, res.event_id,
                    res.gid, res.tid, timeout, res.subevent_id);
            return ret;
        }

        wait_succ_cnt++;
        if (back_event_info.comm.subevent_id == res.subevent_id) {
            break;
        }

        trs_warn("Successfully waited for an event but the condition was not met. (devid=%u; gid=%u; "
                 "tid=%u; cnt=%d; check_subevent=%u; back_subevent=%u)\n",
                 dev_id, res.gid, res.tid, wait_succ_cnt, res.subevent_id, back_event_info.comm.subevent_id);
    } while (1);
    (void)pthread_mutex_unlock(&g_trs_event_sync_mutex[dev_id]);

    reply->reply_len = back_event_info.priv.msg_len;
    trs_debug("Sync event success. (dev_id=%u)\n", dev_id);
    return 0;
}
#endif

#if defined(CFG_SOC_PLATFORM_CLOUD_V4) && defined(CFG_SOC_PLATFORM_ESL_FPGA)
#define TRS_SYNC_EVENT_WAIT_TIME_MS 1200000
#else
#define TRS_SYNC_EVENT_WAIT_TIME_MS 300000
#endif
drvError_t trs_svm_mem_event_sync(uint32_t dev_id, void *in, UINT64 size, uint32_t subevent_id,
                                  struct event_reply *reply)
{
    struct event_summary event_info = {0};
    void *msg = NULL;
    drvError_t ret;

    ret = halMemAlloc(&msg, size, MEM_DEV | MEM_TYPE_DDR | dev_id | ((uint64_t)TSDRV_MODULE_ID << MEM_MODULE_ID_BIT));
    if (ret != DRV_ERROR_NONE) {
        trs_err("Alloc device mem fail. (devid=%u; size=%llu; ret=%d)\n", dev_id, size, ret);
        return DRV_ERROR_INNER_ERR;
    }

    ret = drvMemcpy((DVdeviceptr)(uintptr_t)msg, size, (DVdeviceptr)(uintptr_t)in, size);
    if (ret != DRV_ERROR_NONE) {
        (void)halMemFree(msg);
        trs_err("Memcpy failed. (ret=%d).\n", ret);
        return DRV_ERROR_INNER_ERR;
    }

    ret = trs_fill_event(&event_info, dev_id, subevent_id, (char *)&msg, sizeof(unsigned long long));
    if (ret != DRV_ERROR_NONE) {
        (void)halMemFree(msg);
        trs_clear_event(&event_info);
        trs_err("Trs_fill_event failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = sem_wait(&g_trs_event_sem);
    if (ret != 0) {
        (void)halMemFree(msg);
        trs_clear_event(&event_info);
        trs_err("Sem wait failed. (ret=%d)\n", ret);
        return DRV_ERROR_INNER_ERR;
    }

    if (subevent_id == DRV_SUBEVENT_TRS_ALLOC_SQCQ_WITH_URMA_MSG) {
        ret = trs_submit_event_sync(dev_id, &event_info, TRS_SYNC_EVENT_WAIT_TIME_MS, reply);
    } else {
        ret = halEschedSubmitEventSync(dev_id, &event_info, TRS_SYNC_EVENT_WAIT_TIME_MS, reply);
    }
    (void)sem_post(&g_trs_event_sem);
    if (ret != DRV_ERROR_NONE) {
        (void)halMemFree(msg);
        trs_err("halEschedSubmitEventSync failed, ret(%d).\n", ret);
        trs_clear_event(&event_info);
        return DRV_ERROR_INNER_ERR;
    }

    if (reply->reply_len != reply->buf_len) {
        if ((reply->reply_len < sizeof(int)) || (DRV_EVENT_REPLY_BUFFER_RET(reply->buf) == 0)) {
            trs_err("Sync event failed. (reply_len=%u; buf_len=%u)\n", reply->reply_len, reply->buf_len);
            ret = DRV_ERROR_INNER_ERR;
        } else {
            trs_debug("Sync event proc debug. (result=%d)\n", DRV_EVENT_REPLY_BUFFER_RET(reply->buf));
        }
    }

    (void)halMemFree(msg);
    trs_clear_event(&event_info);

    return ret;
}

static drvError_t trs_res_id_alloc_sync(uint32_t dev_id, struct halResourceIdInputInfo *in,
                                        struct halResourceIdOutputInfo *out)
{
    struct event_proc_result result;
    struct event_reply reply;
    drvError_t ret;

    reply.buf_len = sizeof(struct event_proc_result);
    reply.buf = (char *)&result;
    ret = trs_svm_mem_event_sync(dev_id, in, sizeof(struct halResourceIdInputInfo), DRV_SUBEVENT_TRS_ALLOC_RES_ID_MSG,
                                 &reply);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = memcpy_s(out, sizeof(struct halResourceIdOutputInfo), result.data, EVENT_PROC_RSP_LEN);
    if (ret != 0) {
        trs_err("Memcpy failed.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    trs_debug("Res info. (dev_id=%u; res_type=%d; res_id=%u)\n", dev_id, in->type, out->resourceId);

    return (drvError_t)result.ret;
}

static drvError_t trs_res_id_free_sync(uint32_t dev_id, struct halResourceIdInputInfo *in)
{
    struct event_proc_result result;
    struct event_reply reply;
    drvError_t ret;

    reply.buf_len = sizeof(struct event_proc_result);
    reply.buf = (char *)&result;
    ret = trs_svm_mem_event_sync(dev_id, in, sizeof(struct halResourceIdInputInfo), DRV_SUBEVENT_TRS_FREE_RES_ID_MSG,
                                 &reply);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    trs_debug("Res info. (dev_id=%u; res_type=%d; res_id=%u)\n", dev_id, in->type, in->resourceId);

    return (drvError_t)result.ret;
}

static drvError_t trs_res_id_config_sync(uint32_t dev_id, struct halResourceIdInputInfo *in,
                                         struct halResourceConfigInfo *para)
{
    struct trs_resid_config_msg config_info;
    struct event_proc_result result;
    struct event_reply reply;
    drvError_t ret;

    config_info.in = *in;
    config_info.para = *para;

    reply.buf_len = sizeof(struct event_proc_result);
    reply.buf = (char *)&result;
    ret = trs_svm_mem_event_sync(dev_id, &config_info, sizeof(struct trs_resid_config_msg),
                                 DRV_SUBEVENT_TRS_RES_ID_CONFIG_MSG, &reply);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    trs_debug("Res info. (dev_id=%u; res_type=%d; res_id=%u; prop=%u)\n", dev_id, in->type, in->resourceId, para->prop);
    return (drvError_t)result.ret;
}

drvError_t trs_sq_cq_query_sync(uint32_t dev_id, struct halSqCqQueryInfo *info)
{
    struct event_reply reply;
    drvError_t ret;
    int result;

    reply.buf_len = sizeof(uint32_t) * SQCQ_QUERY_INFO_LENGTH + sizeof(int);
    reply.buf = (char *)malloc(reply.buf_len);
    if (reply.buf == NULL) {
        trs_err("Malloc reply buffer failed.\n");
        return DRV_ERROR_OUT_OF_MEMORY;
    };
    ret = trs_svm_mem_event_sync(dev_id, info, sizeof(struct halSqCqQueryInfo), DRV_SUBEVENT_TRS_SQCQ_QUERY_MSG,
                                 &reply);
    result = DRV_EVENT_REPLY_BUFFER_RET(reply.buf);
    if ((ret != 0) || (result != 0)) {
        trs_err("Failed to sync sqcq query event. (ret=%d; result=%d)\n", ret, result);
        free(reply.buf);
        return (ret != 0) ? ret : (drvError_t)result;
    }

    ret = memcpy_s(info->value, sizeof(uint32_t) * SQCQ_QUERY_INFO_LENGTH, DRV_EVENT_REPLY_BUFFER_DATA_PTR(reply.buf),
                   sizeof(uint32_t) * SQCQ_QUERY_INFO_LENGTH);
    if (ret != DRV_ERROR_NONE) {
        trs_err("Memcpy_s failed. (devid=%u; dst_size=%u; src_size=%u)\n", dev_id,
                sizeof(uint32_t) * SQCQ_QUERY_INFO_LENGTH, sizeof(uint32_t) * SQCQ_QUERY_INFO_LENGTH);
        free(reply.buf);
        return ret;
    }
    trs_debug("Query success. (dev_id=%u; sq_id=%u; prop=%d)\n", dev_id, info->sqId, info->prop);

    free(reply.buf);
    return 0;
}

drvError_t trs_sq_cq_config_sync(uint32_t dev_id, struct halSqCqConfigInfo *info)
{
    struct event_reply reply;
    drvError_t ret;
    int result;

    reply.buf_len = sizeof(int);
    reply.buf = (char *)&result;
    ret = trs_svm_mem_event_sync(dev_id, info, sizeof(struct halSqCqQueryInfo), DRV_SUBEVENT_TRS_SQCQ_CONFIG_MSG,
                                 &reply);
    if ((ret != 0) || (result != 0)) {
        trs_err("Failed to sync sqcq config event. (ret=%d; result=%d)\n", ret, result);
        return (ret != 0) ? ret : (drvError_t)result;
    }

    trs_debug("Config success. (dev_id=%u; sq_id=%u; prop=%d)\n", dev_id, info->sqId, info->prop);

    return 0;
}

static drvError_t trs_sq_cq_alloc_sync(uint32_t dev_id, struct halSqCqInputInfo *in, struct halSqCqOutputInfo *out)
{
    struct event_proc_result result;
    struct event_reply reply;
    void *sync_msg = (void *)in;
    UINT64 sync_msg_len = sizeof(struct halSqCqInputInfo);
    drvError_t ret;
    int memcpy_ret;

    if ((in->ext_info != NULL) && (in->ext_info_len != 0)) {
        sync_msg_len += in->ext_info_len;
        sync_msg = malloc(sync_msg_len);
        if (sync_msg == NULL) {
            trs_err("Malloc sync_msg failed.\n");
            return DRV_ERROR_OUT_OF_MEMORY;
        }
        memcpy_ret = memcpy_s(sync_msg, sync_msg_len, in, sizeof(struct halSqCqInputInfo));
        if (memcpy_ret != 0) {
            trs_err("Failed to memcpy_s. (ret=%d)\n", memcpy_ret);
            free(sync_msg);
            return DRV_ERROR_PARA_ERROR;
        }
        if ((in->ext_info != NULL) && (in->ext_info_len != 0)) {
            memcpy_ret = memcpy_s((char *)sync_msg + sizeof(struct halSqCqInputInfo),
                                  sync_msg_len - sizeof(struct halSqCqInputInfo), in->ext_info, in->ext_info_len);
            if (memcpy_ret != 0) {
                trs_err("Failed to memcpy_s ext_info. (ret=%d)\n", memcpy_ret);
                free(sync_msg);
                return DRV_ERROR_PARA_ERROR;
            }
        }
    }

    reply.buf_len = sizeof(struct event_proc_result);
    reply.buf = (char *)&result;
    ret = trs_svm_mem_event_sync(dev_id, sync_msg, sync_msg_len, DRV_SUBEVENT_TRS_ALLOC_SQCQ_MSG, &reply);
    if (ret != DRV_ERROR_NONE) {
        if (sync_msg != in) {
            free(sync_msg);
        }
        return ret;
    }

    memcpy_ret = memcpy_s(out, sizeof(struct halSqCqOutputInfo), result.data, EVENT_PROC_RSP_LEN);
    if (memcpy_ret != 0) {
#ifndef EMU_ST
        trs_err("Memcpy failed. (ret=%d)\n", memcpy_ret);
        if (sync_msg != in) {
            free(sync_msg);
        }
        return DRV_ERROR_PARA_ERROR;
#endif
    }

    trs_debug("Res info. (dev_id=%u; res_type=%d; sqid=%u; cqid=%u)\n", dev_id, in->type, out->sqId, out->cqId);
    if (sync_msg != in) {
        free(sync_msg);
    }
    return (drvError_t)result.ret;
}

static drvError_t trs_sq_cq_free_sync(uint32_t dev_id, struct halSqCqFreeInfo *in)
{
    struct event_proc_result result;
    struct event_reply reply;
    drvError_t ret;

    reply.buf_len = sizeof(struct event_proc_result);
    reply.buf = (char *)&result;
    ret = trs_svm_mem_event_sync(dev_id, in, sizeof(struct halSqCqFreeInfo), DRV_SUBEVENT_TRS_FREE_SQCQ_MSG, &reply);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    trs_debug("Res info. (dev_id=%u; res_type=%d; sqid=%u; cqid=%u)\n", dev_id, in->type, in->sqId, in->cqId);

    return (drvError_t)result.ret;
}

drvError_t trs_local_mem_event_sync(uint32_t dev_id, void *in, UINT64 size, uint32_t subevent_id,
                                    struct event_reply *reply)
{
    struct event_summary event_info = {0};
    const char *msg = (char *)in;
    drvError_t ret;

    ret = trs_fill_event(&event_info, dev_id, subevent_id, msg, (uint32_t)size);
    if (ret != DRV_ERROR_NONE) {
#ifndef EMU_ST
        trs_clear_event(&event_info);
        trs_err("Trs_fill_event failed, ret=%d.\n", ret);
        return ret;
#endif
    }

    ret = sem_wait(&g_trs_event_sem);
    if (ret != 0) {
        trs_clear_event(&event_info);
        trs_err("Sem wait failed. (ret=%d)\n", ret);
        return DRV_ERROR_INNER_ERR;
    }
    ret = halEschedSubmitEventSync(dev_id, &event_info, TRS_SYNC_EVENT_WAIT_TIME_MS, reply);
    (void)sem_post(&g_trs_event_sem);
    if (ret != DRV_ERROR_NONE) {
        trs_err("Sync failed.  (ret=%d; devid=%u).\n", ret, dev_id);
        trs_clear_event(&event_info);
        return DRV_ERROR_INNER_ERR;
    }

    if (reply->reply_len != reply->buf_len) { /* check if sync event fail */
        if ((reply->reply_len < sizeof(int)) || (DRV_EVENT_REPLY_BUFFER_RET(reply->buf) == 0)) {
            trs_err("Sync event failed. (reply_len=%u; buf_len=%u)\n", reply->reply_len, reply->buf_len);
            ret = DRV_ERROR_INNER_ERR;
        } else {
            trs_warn("Sync event proc warn. (result=%d)\n", DRV_EVENT_REPLY_BUFFER_RET(reply->buf));
        }
    }

    trs_clear_event(&event_info);
    return ret;
}

static int trs_shr_id_config(uint32_t dev_id, struct drvShrIdInfo *info)
{
    struct event_proc_result result;
    struct event_reply reply;
    drvError_t ret;

    reply.buf_len = sizeof(struct event_proc_result);
    reply.buf = (char *)&result;
    ret = trs_local_mem_event_sync(dev_id, info, sizeof(struct drvShrIdInfo), DRV_SUBEVENT_TRS_SHR_ID_CONFIG_MSG,
                                   &reply);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    trs_debug("Res info. (dev_id=%u; rudevid=%u; id_type=%u; shrid=%u)\n", dev_id, info->devid, info->id_type,
              info->shrid);

    return (drvError_t)result.ret;
}

static int trs_shr_id_deonfig(uint32_t dev_id, struct drvShrIdInfo *info)
{
    struct event_proc_result result;
    struct event_reply reply;
    drvError_t ret;

    reply.buf_len = sizeof(struct event_proc_result);
    reply.buf = (char *)&result;
    ret = trs_local_mem_event_sync(dev_id, info, sizeof(struct drvShrIdInfo), DRV_SUBEVENT_TRS_SHR_ID_DECONFIG_MSG,
                                   &reply);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    trs_debug("Res info. (dev_id=%u; rudevid=%u; id_type=%u; shrid=%u)\n", dev_id, info->devid, info->id_type,
              info->shrid);

    return (drvError_t)result.ret;
}

static int trs_master_event_init(uint32_t dev_id)
{
    int ret;
    int i;

    ret = trs_sq_cq_event_init(dev_id);
    if (ret != 0) {
        return ret;
    }

    g_trs_hostpid[dev_id] = 0;
    g_trs_devpid[dev_id] = 0;
    for (i = 0; i < TRS_SYNC_EVENT_GROUP_TYPE_MAX; i++) {
        g_trs_devgid[dev_id][i] = -1;
    }

    return 0;
}

static void trs_master_event_un_init(uint32_t dev_id)
{
    int i;

    g_trs_hostpid[dev_id] = 0;
    g_trs_devpid[dev_id] = 0;
    for (i = 0; i < TRS_SYNC_EVENT_GROUP_TYPE_MAX; i++) {
        g_trs_devgid[dev_id][i] = -1;
    }

    trs_sq_cq_event_un_init(dev_id);
}

static int __attribute__((constructor)) trs_master_event_construct(void)
{
    struct trs_res_remote_ops res_ops;
    struct trs_sqcq_remote_ops sqcq_ops;
    struct trs_shrid_remote_ops shrid_ops;
    static struct trs_dev_init_ops event_dev_ops;
    int ret;

    res_ops.resid_alloc = trs_res_id_alloc_sync;
    res_ops.resid_free = trs_res_id_free_sync;
    res_ops.resid_config = trs_res_id_config_sync;
    trs_register_res_remote_ops(&res_ops);

    sqcq_ops.sqcq_alloc = trs_sq_cq_alloc_sync;
    sqcq_ops.sqcq_free = trs_sq_cq_free_sync;
    trs_register_sqcq_remote_ops(&sqcq_ops);

    shrid_ops.shrid_config = trs_shr_id_config;
    shrid_ops.shrid_deconfig = trs_shr_id_deonfig;
    trs_register_shrid_remote_ops(&shrid_ops);

    (void)sem_init(&g_trs_event_sem, 0, TRS_MAX_CONCURRENCY_NUM);
    event_dev_ops.dev_init = trs_master_event_init;
    event_dev_ops.dev_uninit = trs_master_event_un_init;
    ret = trs_register_dev_init_ops(&event_dev_ops);
    if (ret != DRV_ERROR_NONE) {
        trs_err("Register event dev ops failed. (ret=%d)\n", ret);
        return ret;
    }
    trs_info("Register master event.\n");

    return 0;
}
