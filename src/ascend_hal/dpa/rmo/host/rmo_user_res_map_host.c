/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>

#include "securec.h"
#include "ascend_hal.h"
#include "ascend_hal_external.h"
#include "ascend_hal_define.h"
#include "esched_user_interface.h"
#include "rmo_msg.h"
#include "rmo.h"

#define DPA_EVENT_SUBMIT_SYNC_TMOUT 50000

static void dpa_clear_event(struct event_summary *event_info)
{
    free(event_info->msg);
    event_info->msg = NULL;
}

static pid_t dpa_get_dev_pid(uint32_t dev_id)
{
    struct halQueryDevpidInfo hostpidinfo = {0};
    drvError_t ret;
    int devpid = -1;

    hostpidinfo.proc_type = DEVDRV_PROCESS_CP1;
    hostpidinfo.hostpid = getpid();
    hostpidinfo.devid = dev_id;
    hostpidinfo.vfid = 0;
    ret = halQueryDevpid(hostpidinfo, &devpid);
    if (ret != 0) {
        rmo_err("Query devpid failed. (devId=%u; vfid=%u; hostpid=%d; proc_type=%u; ret=%d).\n", dev_id, 0, getpid(),
                hostpidinfo.proc_type, ret);
        return ret;
    }

    return devpid;
}

static drvError_t dpa_get_event_grpId(uint32_t devid, uint32_t *grp_id, const char *grp_name)
{
    struct esched_query_gid_output gid_out = {0};
    struct esched_query_gid_input gid_in = {0};
    struct esched_output_info out_put = {0};
    struct esched_input_info in_put = {0};
    size_t grp_name_len;
    drvError_t ret;

    gid_in.pid = dpa_get_dev_pid(devid);
    grp_name_len = strlen(grp_name);
    (void)memcpy_s(gid_in.grp_name, EVENT_MAX_GRP_NAME_LEN, grp_name, grp_name_len);
    in_put.inBuff = &gid_in;
    in_put.inLen = sizeof(struct esched_query_gid_input);
    out_put.outBuff = &gid_out;
    out_put.outLen = sizeof(struct esched_query_gid_output);
    ret = halEschedQueryInfo(devid, QUERY_TYPE_REMOTE_GRP_ID, &in_put, &out_put);
    if (ret == DRV_ERROR_NONE) {
        *grp_id = gid_out.grp_id;
        return DRV_ERROR_NONE;
    } else if (ret == DRV_ERROR_UNINIT) {
        *grp_id = 0; // PROXY_HOST_GRP_NAME grp not exist, use default grpid 0.
        return DRV_ERROR_NONE;
    }

    rmo_err("Query grpid failed. (ret=%d; devid=%u; devpid=%d).\n", ret, devid, gid_in.pid);
    return ret;
}

static drvError_t dpa_fill_event(struct event_summary *event_info, uint32_t devid, uint32_t subevent_id,
                                 const char *msg, uint32_t msg_len)
{
    struct event_sync_msg *msg_head = NULL;
    drvError_t ret;

    ret = dpa_get_event_grpId(devid, &event_info->grp_id, PROXY_HOST_QUEUE_GRP_NAME);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }
    event_info->dst_engine = ACPU_DEVICE;
    event_info->policy = ONLY;
    event_info->event_id = EVENT_DRV_MSG;
    event_info->subevent_id = subevent_id;
    event_info->pid = dpa_get_dev_pid(devid);
    event_info->msg_len = (uint32_t)sizeof(struct event_sync_msg) + msg_len;
    event_info->msg = malloc(event_info->msg_len);
    if (event_info->msg == NULL) {
        rmo_err("malloc %u failed.\n", event_info->msg_len);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    msg_head = (struct event_sync_msg *)event_info->msg;
    msg_head->dst_engine = CCPU_HOST;

    if (msg_len == 0) {
        return DRV_ERROR_NONE;
    }

    ret = (drvError_t)memcpy_s(event_info->msg + sizeof(struct event_sync_msg), msg_len, msg, msg_len);
    if (ret != DRV_ERROR_NONE) {
        rmo_err("memcpy_s failed, ret=%d.\n", ret);
        free(event_info->msg);
        event_info->msg = NULL;
        return DRV_ERROR_PARA_ERROR;
    }

    return DRV_ERROR_NONE;
}

drvError_t dpa_res_map(unsigned int dev_id, struct res_map_info *res_info, unsigned long *va, unsigned int *len)
{
    struct event_proc_result result;
    struct event_reply reply;
    struct event_summary event_info;
    struct rmo_map_msg msg = {0};
    struct rmo_map_msg map_msg = {0};
    drvError_t ret;

    reply.buf_len = sizeof(struct rmo_map_msg) + sizeof(int);
    reply.buf = (char *)&result;

    msg.res_info.target_proc_type = res_info->target_proc_type;
    msg.res_info.res_type = res_info->res_type;
    msg.res_info.res_id = res_info->res_id;
    msg.res_info.flag = res_info->flag;
    ret = dpa_fill_event(&event_info, dev_id, DRV_SUBEVENT_DPA_RES_MAP_MSG, (char *)&msg, sizeof(struct rmo_map_msg));
    if (ret != DRV_ERROR_NONE) {
        rmo_err("DpaFillEvent failed, ret=%d.\n", ret);
        return ret;
    }

    ret = halEschedSubmitEventSync(dev_id, &event_info, DPA_EVENT_SUBMIT_SYNC_TMOUT, &reply);
    if (ret != DRV_ERROR_NONE) {
        rmo_err("Sync failed.  (ret=%d; dev_id=%u).\n", ret, dev_id);
        dpa_clear_event(&event_info);
        return ret;
    }

    if ((reply.reply_len != reply.buf_len) || (result.ret != 0)) {
        dpa_clear_event(&event_info);
        rmo_err("Remote map failed. (reply_len=%u; buf_len=%u; result=%d)\n", reply.reply_len, reply.buf_len,
                result.ret);
        ret = (result.ret == 0) ? DRV_ERROR_INNER_ERR : result.ret;
        return ret;
    }

    dpa_clear_event(&event_info);

    (void)memcpy_s(&map_msg, sizeof(struct rmo_map_msg), result.data, sizeof(struct rmo_map_msg));
    *va = map_msg.va;
    *len = map_msg.len;
    return DRV_ERROR_NONE;
}

drvError_t dpa_res_unmap(unsigned int dev_id, struct res_map_info *res_info)
{
    struct event_proc_result result;
    struct event_reply reply;
    struct event_summary event_info;
    struct rmo_map_msg msg = {0};
    drvError_t ret;

    reply.buf_len = sizeof(int);
    reply.buf = (char *)&result;

    msg.res_info.target_proc_type = res_info->target_proc_type;
    msg.res_info.res_type = res_info->res_type;
    msg.res_info.res_id = res_info->res_id;
    msg.res_info.flag = res_info->flag;

    ret = dpa_fill_event(&event_info, dev_id, DRV_SUBEVENT_DPA_RES_UNMAP_MSG, (char *)&msg, sizeof(struct rmo_map_msg));
    if (ret != DRV_ERROR_NONE) {
        rmo_err("DpaFillEvent failed, ret=%d.\n", ret);
        return ret;
    }

    ret = halEschedSubmitEventSync(dev_id, &event_info, 5000, &reply); /* 5000 -> 5s */
    if (ret != DRV_ERROR_NONE) {
        rmo_err("Sync failed.  (ret=%d; dev_id=%u).\n", ret, dev_id);
        dpa_clear_event(&event_info);
        return ret;
    }

    if ((reply.reply_len != reply.buf_len) || (result.ret != 0)) {
        dpa_clear_event(&event_info);
        rmo_err("Remote unmap failed. (reply_len=%u; buf_len=%u; result=%d)\n", reply.reply_len, reply.buf_len,
                result.ret);
        ret = (result.ret == 0) ? DRV_ERROR_INNER_ERR : result.ret;
        return ret;
    }

    dpa_clear_event(&event_info);
    return DRV_ERROR_NONE;
}

drvError_t dpa_res_read(unsigned int dev_id, struct res_map_info *res_info, void *data, unsigned int len)
{
    struct event_proc_result result;
    struct event_reply reply;
    struct event_summary event_info;
    struct rmo_read_msg msg = {0};
    drvError_t ret;

    if (len > RMO_EVENT_PROC_READ_LEN) {
        rmo_err("Input para invalid. (len=%u, max_len=%u)\n", len, RMO_EVENT_PROC_READ_LEN);
        return DRV_ERROR_PARA_ERROR;
    }

    reply.buf_len = len + (unsigned int)sizeof(int);
    reply.buf = (char *)&result;

    msg.res_info.target_proc_type = res_info->target_proc_type;
    msg.res_info.res_type = res_info->res_type;
    msg.res_info.res_id = res_info->res_id;
    msg.res_info.flag = res_info->flag;
    msg.len = len;
    ret = dpa_fill_event(&event_info, dev_id, DRV_SUBEVENT_DPA_RES_READ_MSG, (char *)&msg, sizeof(struct rmo_read_msg));
    if (ret != DRV_ERROR_NONE) {
        rmo_err("DpaFillEvent failed, ret=%d.\n", ret);
        return ret;
    }

    ret = halEschedSubmitEventSync(dev_id, &event_info, DPA_EVENT_SUBMIT_SYNC_TMOUT, &reply);
    if (ret != DRV_ERROR_NONE) {
        rmo_err("Sync failed.  (ret=%d; dev_id=%u).\n", ret, dev_id);
        dpa_clear_event(&event_info);
        return ret;
    }

    if ((reply.reply_len != reply.buf_len) || (result.ret != 0)) {
        dpa_clear_event(&event_info);
        rmo_err("Remote read failed. (reply_len=%u; buf_len=%u; result=%d)\n", reply.reply_len, reply.buf_len,
                result.ret);
        ret = (result.ret == 0) ? DRV_ERROR_INNER_ERR : result.ret;
        return ret;
    }

    dpa_clear_event(&event_info);

    (void)memcpy_s(data, (size_t)len, result.data, (size_t)len);
    return DRV_ERROR_NONE;
}

drvError_t dpa_res_write(unsigned int dev_id, struct res_map_info *res_info, void *data, unsigned int len)
{
    struct event_proc_result result;
    struct event_reply reply;
    struct event_summary event_info;
    struct rmo_write_msg msg = {0};
    drvError_t ret;

    if (len > RMO_EVENT_PROC_WRITE_LEN) {
        rmo_err("Input para invalid. (len=%u, max_len=%u)\n", len, RMO_EVENT_PROC_WRITE_LEN);
        return DRV_ERROR_PARA_ERROR;
    }

    reply.buf_len = sizeof(int);
    reply.buf = (char *)&result;

    msg.res_info.target_proc_type = res_info->target_proc_type;
    msg.res_info.res_type = res_info->res_type;
    msg.res_info.res_id = res_info->res_id;
    msg.res_info.flag = res_info->flag;
    msg.len = len;
    ret = (drvError_t)memcpy_s(msg.data, sizeof(msg.data), data, len);
    if (ret != DRV_ERROR_NONE) {
        rmo_err("Memcpy data failed. (ret=%d; len=%u; max_len=%u)\n", ret, len, RMO_EVENT_PROC_WRITE_LEN);
        return DRV_ERROR_PARA_ERROR;
    }
    ret = dpa_fill_event(&event_info, dev_id, DRV_SUBEVENT_DPA_RES_WRITE_MSG, (char *)&msg,
                         sizeof(struct rmo_write_msg));
    if (ret != DRV_ERROR_NONE) {
        rmo_err("DpaFillEvent failed, ret=%d.\n", ret);
        return ret;
    }

    ret = halEschedSubmitEventSync(dev_id, &event_info, DPA_EVENT_SUBMIT_SYNC_TMOUT, &reply);
    if (ret != DRV_ERROR_NONE) {
        rmo_err("Sync failed.  (ret=%d; dev_id=%u).\n", ret, dev_id);
        dpa_clear_event(&event_info);
        return ret;
    }

    if ((reply.reply_len != reply.buf_len) || (result.ret != 0)) {
        dpa_clear_event(&event_info);
        rmo_err("Remote write failed. (reply_len=%u; buf_len=%u; result=%d)\n", reply.reply_len, reply.buf_len,
                result.ret);
        ret = (result.ret == 0) ? DRV_ERROR_INNER_ERR : result.ret;
        return ret;
    }

    dpa_clear_event(&event_info);
    return DRV_ERROR_NONE;
}
