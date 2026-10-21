/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef EMU_ST
#include <sys/prctl.h>
#include <sys/types.h>
#include <unistd.h>

#include "securec.h"
#include "queue.h"
#include "que_compiler.h"
#include "que_ub_msg.h"
#include "que_comm_event.h"
#include "que_comm_chan.h"
#include "que_inter_dev_ini.h"
#include "que_inter_dev_tgt.h"

static int que_inter_dev_param_check(unsigned int devid, const void *msg, int msg_len, struct drv_event_proc_rsp *rsp,
                                     size_t size)
{
    if (que_unlikely(queue_inter_devid_invalid(devid))) {
        QUEUE_LOG_ERR("invalid devid. (devid=%u)\n", devid);
        return DRV_ERROR_PARA_ERROR;
    }

    if (que_unlikely((msg == NULL) || (rsp == NULL))) {
        QUEUE_LOG_ERR("que param check fail. (msg_is_null=%d; rsp_is_null=%u)\n", (msg == NULL), (rsp == NULL));
        return DRV_ERROR_PARA_ERROR;
    }

    if (que_unlikely((size_t)msg_len < size)) {
        QUEUE_LOG_ERR("invalid size. (devid=%u; msg_len=%ld; size=%ld)\n", devid, (size_t)msg_len, size);
        return DRV_ERROR_PARA_ERROR;
    }

    return DRV_ERROR_NONE;
}

static drvError_t que_inter_dev_export_tgt(unsigned int dev_id, const void *msg, int msg_len,
                                           struct drv_event_proc_rsp *rsp)
{
    int ret;

    (void)msg_len;

    rsp->real_rsp_data_len = 0;
    const struct que_inter_dev_export_import_in_msg *in = (const struct que_inter_dev_export_import_in_msg *)msg;
    struct shareQueInfo que_info = {.peerDevId = in->peer_dev_id};

    if (in->qid >= MAX_SURPORT_QUEUE_NUM) {
        QUEUE_LOG_ERR("Invalid qid: %d.\n", in->qid);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = que_inter_dev_param_check(dev_id, msg, msg_len, rsp, sizeof(struct que_inter_dev_export_import_in_msg));
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que inter dev param check fail. (ret=%d; devid=%u)\n", ret, dev_id);
        return ret;
    }

    ret = memcpy_s(que_info.shareQueName, SHARE_QUEUE_NAME_LEN, in->share_queue_name, SHARE_QUEUE_NAME_LEN);
    if (ret != 0) {
        QUEUE_LOG_ERR("memcpy_s fail. (ret=%d)\n", ret);
        return DRV_ERROR_MEMORY_OPT_FAIL;
    }
    return que_inter_dev_export_ini_local(dev_id, in->qid, in->phy_peer_devid, &que_info);
}

static drvError_t que_inter_dev_unexport_tgt(unsigned int dev_id, const void *msg, int msg_len,
                                             struct drv_event_proc_rsp *rsp)
{
    int ret;

    (void)msg_len;

    rsp->real_rsp_data_len = 0;
    const struct que_inter_dev_export_import_in_msg *in = (const struct que_inter_dev_export_import_in_msg *)msg;
    struct shareQueInfo que_info = {.peerDevId = in->peer_dev_id};

    if (in->qid >= MAX_SURPORT_QUEUE_NUM) {
        QUEUE_LOG_ERR("Invalid qid: %d.\n", in->qid);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = que_inter_dev_param_check(dev_id, msg, msg_len, rsp, sizeof(struct que_inter_dev_export_import_in_msg));
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que inter dev param check fail. (ret=%d; devid=%u)\n", ret, dev_id);
        return ret;
    }

    ret = memcpy_s(que_info.shareQueName, SHARE_QUEUE_NAME_LEN, in->share_queue_name, SHARE_QUEUE_NAME_LEN);
    if (ret != 0) {
        QUEUE_LOG_ERR("memcpy_s fail. (ret=%d)\n", ret);
        return DRV_ERROR_MEMORY_OPT_FAIL;
    }
    return que_inter_dev_unexport_ini_local(dev_id, in->qid, &que_info);
}

static drvError_t que_inter_dev_import_tgt(unsigned int dev_id, const void *msg, int msg_len,
                                           struct drv_event_proc_rsp *rsp)
{
    struct que_inter_dev_import_out_msg out = {0};
    const struct que_inter_dev_export_import_in_msg *in = (const struct que_inter_dev_export_import_in_msg *)msg;
    struct queue_manages *que_mng = NULL;
    unsigned int virtual_qid;
    unsigned int qid;
    drvError_t ret;

    rsp->real_rsp_data_len = sizeof(struct que_inter_dev_import_out_msg);
    ret = que_inter_dev_param_check(dev_id, msg, msg_len, rsp, sizeof(struct que_inter_dev_export_import_in_msg));
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que inter dev param check fail. (ret=%d; devid=%u)\n", ret, dev_id);
        return ret;
    }

    ret = que_inter_dev_match(in->dev_id, in->share_queue_name, &qid);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_WARN("que import match not success.\n");
        return ret;
    }

    if (!queue_get(qid)) {
        QUEUE_LOG_ERR("queue get failed. (qid=%u)\n", qid);
        return DRV_ERROR_NOT_EXIST;
    }

    que_mng = queue_get_local_mng(qid);
    if (que_mng == NULL) {
        queue_put(qid);
        QUEUE_LOG_ERR("Queue local manage is null. (dev_id=%u, qid=%u)\n", dev_id, qid);
        return DRV_ERROR_NOT_EXIST;
    }

    if (CAS(&que_mng->remote_qid, QUEUE_INVALID_VALUE, in->qid) == false) {
        QUEUE_LOG_WARN("queue remote_qid cas not success. (qid=%u, remote_qid=%d)\n", qid, que_mng->remote_qid);
        queue_put(qid);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = que_set_inter_dev_info(in, que_mng);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        (void)ATOMIC_SET(&que_mng->remote_qid, QUEUE_INVALID_VALUE);
        queue_put(qid);
        return ret;
    }

    virtual_qid = queue_get_virtual_qid(qid, INTER_DEV_QUEUE);
    out.qid = virtual_qid;
    out.depth = queue_get_local_depth(qid);
    out.work_mode = que_mng->work_mode;
    out.flow_ctrl_drop_time = que_mng->drop_time;
    out.flow_ctrl_flag = que_mng->fctl_flag;
    ret = memcpy_s(rsp->rsp_data_buf, (size_t)rsp->rsp_data_buf_len, (void *)&out,
                   sizeof(struct que_inter_dev_import_out_msg));
    if (que_unlikely(ret != 0)) {
        (void)ATOMIC_SET(&que_mng->remote_qid, QUEUE_INVALID_VALUE);
        queue_put(qid);
        QUEUE_LOG_ERR("event response copy fail. (ret=%d; devid=%u; qid=%u; res_buf_len=%d; msg_size=%ld)\n", ret,
                      dev_id, in->qid, rsp->rsp_data_buf_len, sizeof(struct que_inter_dev_import_out_msg));
        return DRV_ERROR_MEMORY_OPT_FAIL;
    }

    queue_put(qid);
    rsp->need_rsp = true;
    return DRV_ERROR_NONE;
}

drvError_t que_inter_dev_import_h2d_tgt(unsigned int dev_id, const void *msg, int msg_len,
                                        struct drv_event_proc_rsp *rsp)
{
    QueueAttr que_attr = {.depth = MAX_QUEUE_DEPTH, .deploy_type = LOCAL_QUEUE_DEPLOY};
    unsigned int qid = 0;
    const struct que_inter_dev_export_import_in_msg *in = (const struct que_inter_dev_export_import_in_msg *)msg;
    struct que_inter_dev_import_out_msg out = {0};
    struct queue_manages *que_mng = NULL;
    unsigned int virtual_qid;
    drvError_t ret, ret_post;

    rsp->real_rsp_data_len = sizeof(struct que_inter_dev_import_out_msg);
    ret = que_inter_dev_param_check(dev_id, msg, msg_len, rsp, sizeof(struct que_inter_dev_export_import_in_msg));
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que inter dev param check fail. (ret=%d; devid=%u)\n", ret, dev_id);
        return ret;
    }

    if (!queue_is_init()) {
        QUEUE_LOG_ERR("queue not init.\n");
        return DRV_ERROR_UNINIT;
    }

    que_attr.name[0] = '\0';
    ret = halQueueCreate(dev_id, &que_attr, &qid);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("local queue create failed. (ret=%d; dev_id=%u)\n", (int)ret, dev_id);
        return ret;
    }

    if (!queue_get(qid)) {
        QUEUE_LOG_ERR("queue get failed. (qid=%u)\n", qid);
        goto queue_destroy;
    }
    que_mng = queue_get_local_mng(qid);
    if (que_mng == NULL) {
        queue_put(qid);
        QUEUE_LOG_ERR("Queue local manage is null. (dev_id=%u, qid=%u)\n", dev_id, qid);
        goto queue_destroy;
    }

    if (CAS(&que_mng->remote_qid, QUEUE_INVALID_VALUE, in->qid) == false) {
        QUEUE_LOG_WARN("queue remote_qid cas not success. (qid=%u, remote_qid=%d)\n", qid, que_mng->remote_qid);
        queue_put(qid);
        goto queue_destroy;
    }

    ret = que_set_inter_dev_info(in, que_mng);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        (void)ATOMIC_SET(&que_mng->remote_qid, QUEUE_INVALID_VALUE);
        queue_put(qid);
        goto queue_destroy;
    }

    ret = que_get_local_devid_grpid(dev_id, &out.devpid, &out.grpid);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que get inter dev info failed. (ret=%d; devid=%u)\n", ret, dev_id);
        queue_put(qid);
        goto queue_destroy;
    }

    virtual_qid = queue_get_virtual_qid(qid, INTER_DEV_QUEUE);
    out.qid = virtual_qid;

    ret = memcpy_s(rsp->rsp_data_buf, (size_t)rsp->rsp_data_buf_len, (void *)&out,
                   sizeof(struct que_inter_dev_import_out_msg));
    if (que_unlikely(ret != 0)) {
        (void)ATOMIC_SET(&que_mng->remote_qid, QUEUE_INVALID_VALUE);
        queue_put(qid);
        QUEUE_LOG_ERR("event response copy fail. (ret=%d; devid=%u; qid=%u; res_buf_len=%d; msg_size=%ld)\n", ret,
                      dev_id, in->qid, rsp->rsp_data_buf_len, sizeof(struct que_inter_dev_import_out_msg));
        goto queue_destroy;
    }
    (void)ATOMIC_SET(&que_mng->inter_dev_state, QUEUE_STATE_IMPORTED);
    queue_put(qid);
    rsp->need_rsp = true;

    return DRV_ERROR_NONE;
queue_destroy:
    ret_post = halQueueDestroy(dev_id, qid);
    if (que_unlikely(ret_post != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que import post proc destroy queue failed. (ret=%d, devid=%u, qid=%u)\n", ret_post, dev_id, qid);
    }
    return ret;
}

drvError_t que_inter_dev_unimport_h2d_tgt(unsigned int dev_id, const void *msg, int msg_len,
                                          struct drv_event_proc_rsp *rsp)
{
    const struct que_inter_dev_export_import_in_msg *in = (const struct que_inter_dev_export_import_in_msg *)msg;
    struct shareQueInfo que_info = {.peerDevId = in->peer_dev_id};
    rsp->need_rsp = true;
    int ret;

    ret = que_inter_dev_param_check(dev_id, msg, msg_len, rsp, sizeof(struct que_inter_dev_export_import_in_msg));
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que inter dev param check fail. (ret=%d; devid=%u)\n", ret, dev_id);
        return ret;
    }

    ret = memcpy_s(que_info.shareQueName, SHARE_QUEUE_NAME_LEN, in->share_queue_name, SHARE_QUEUE_NAME_LEN);
    if (ret != 0) {
        QUEUE_LOG_ERR("memcpy_s fail. (ret=%d)\n", ret);
        return DRV_ERROR_MEMORY_OPT_FAIL;
    }

    return que_inter_dev_unimport_ini_local(dev_id, in->qid, &que_info);
}

static drvError_t que_inter_dev_attach_tgt(unsigned int dev_id, const void *msg, int msg_len,
                                           struct drv_event_proc_rsp *rsp)
{
    int ret;
    unsigned int actualqid;
    const struct que_inter_dev_attach_in_msg *in = (const struct que_inter_dev_attach_in_msg *)msg;

    ret = que_inter_dev_param_check(dev_id, msg, msg_len, rsp, sizeof(struct que_inter_dev_attach_in_msg));
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que inter dev param check fail. (ret=%d; devid=%u)\n", ret, dev_id);
        return ret;
    }

    actualqid = queue_get_actual_qid(in->qid);
    rsp->real_rsp_data_len = 0;
    if (que_unlikely((dev_id >= MAX_DEVICE) || (actualqid >= MAX_SURPORT_QUEUE_NUM))) {
        QUEUE_LOG_ERR("invalid para. (devid=%u; max_devid=%u; qid=%u; actualqid=%u; max_qid=%u)\n", dev_id,
                      (unsigned int)MAX_DEVICE, in->qid, actualqid, (unsigned int)MAX_SURPORT_QUEUE_NUM);
        return DRV_ERROR_INVALID_VALUE;
    }
    ret = que_save_inter_dev_attach_cfg(dev_id, actualqid, in);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que set que info fail. (ret=%d; devid=%u; qid=%u)\n", ret, dev_id, actualqid);
        return ret;
    }
    return ret;
}

static drvError_t que_inter_dev_sub_f2nf_tgt(unsigned int dev_id, const void *msg, int msg_len,
                                             struct drv_event_proc_rsp *rsp)
{
    const struct que_sub_event_in_msg *in = (const struct que_sub_event_in_msg *)msg;
    unsigned int actualqid;
    struct sub_info info;
    int ret;

    ret = que_inter_dev_param_check(dev_id, msg, msg_len, rsp, sizeof(struct que_sub_event_in_msg));
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que param check fail. (ret=%d; devid=%u)\n", ret, dev_id);
        return ret;
    }

    actualqid = queue_get_actual_qid(in->qid);
    info.spec_thread = (in->tid != QUEUE_INVALID_VALUE);
    info.dst_engine = in->dst_engine;
    info.groupid = in->grp_id;
    info.eventid = in->event_id;
    info.pid = in->pid;
    info.tid = in->tid;
    info.dst_devid = in->dst_phy_devid;
    info.inner_sub_flag = in->inner_sub_flag;
    info.sub_send = 0;

    ret = sub_f_to_nf_event(dev_id, actualqid, info);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que sub f2nf event fail. (ret=%d; devid=%u; qid=%u)\n", ret, dev_id, actualqid);
        return ret;
    }
    return DRV_ERROR_NONE;
}

static drvError_t que_inter_dev_status_check(unsigned int dev_id, unsigned int qid)
{
    int ret;
    int inter_dev_state = QUEUE_STATE_DISABLED;

    ret = que_get_inter_dev_status(dev_id, qid, &inter_dev_state, NULL);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    if ((inter_dev_state == QUEUE_STATE_UNEXPORTED) || (inter_dev_state == QUEUE_STATE_UNIMPORTED)) {
        QUEUE_LOG_ERR("queue has been unexported or unimported. (dev_id=%u, qid=%u)\n", dev_id, qid);
        return DRV_ERROR_PARA_ERROR;
    }
    return DRV_ERROR_NONE;
}

static drvError_t que_inter_dev_query_info_tgt(unsigned int dev_id, const void *msg, int msg_len,
                                               struct drv_event_proc_rsp *rsp)
{
    const struct que_query_info_in_msg *in = (const struct que_query_info_in_msg *)msg;
    unsigned int actualqid;
    struct que_query_info_out_msg out;
    QueueInfo que_info;
    int ret;

    ret = que_inter_dev_param_check(dev_id, msg, msg_len, rsp, sizeof(struct que_query_info_in_msg));
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que param check fail. (ret=%d; devid=%u)\n", ret, dev_id);
        return ret;
    }

    rsp->real_rsp_data_len = sizeof(struct que_query_info_out_msg);
    actualqid = queue_get_actual_qid(in->qid);

    ret = que_inter_dev_status_check(dev_id, actualqid);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que inter dev status check fail. (ret=%d; devid=%u)\n", ret, dev_id);
        return ret;
    }

    ret = queue_query_info_local(dev_id, actualqid, &que_info);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que query info fail. (ret=%d; devid=%u; qid=%u)\n", ret, dev_id, in->qid);
        return ret;
    }

    out.size = que_info.size;
    out.status = que_info.status;
    out.work_mode = que_info.workMode;
    out.enque_cnt = que_info.stat.enqueCnt;
    out.deque_cnt = que_info.stat.dequeCnt;
    ret = memcpy_s(rsp->rsp_data_buf, (size_t)rsp->rsp_data_buf_len, (void *)&out,
                   sizeof(struct que_query_info_out_msg));
    if (que_unlikely(ret != EOK)) {
        QUEUE_LOG_ERR("event response copy fail. (ret=%d; devid=%u; qid=%u; res_buf_len=%d; msg_size=%ld)\n", ret,
                      dev_id, in->qid, rsp->rsp_data_buf_len, sizeof(struct que_query_info_out_msg));
        return DRV_ERROR_MEMORY_OPT_FAIL;
    }
    rsp->need_rsp = true;
    return DRV_ERROR_NONE;
}

static drvError_t que_inter_dev_get_status_tgt(unsigned int dev_id, const void *msg, int msg_len,
                                               struct drv_event_proc_rsp *rsp)
{
    const struct que_get_status_in_msg *in = (const struct que_get_status_in_msg *)msg;
    unsigned int actualqid;
    struct que_get_status_out_msg out;
    unsigned int len;
    int ret;

    ret = que_inter_dev_param_check(dev_id, msg, msg_len, rsp, sizeof(struct que_get_status_in_msg));
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que param check fail. (ret=%d; devid=%u)\n", ret, dev_id);
        return ret;
    }

    actualqid = queue_get_actual_qid(in->qid);
    rsp->real_rsp_data_len = sizeof(struct que_get_status_out_msg);
    len = (in->out_len < EVENT_PROC_RSP_LEN) ? in->out_len : EVENT_PROC_RSP_LEN;

    ret = que_inter_dev_status_check(dev_id, actualqid);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que inter dev status check fail. (ret=%d; devid=%u)\n", ret, dev_id);
        return ret;
    }

    ret = queue_get_status_local(dev_id, actualqid, in->query_item, len, &out.data);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que get status fail. (ret=%d; devid=%u; qid=%u; query_item=%d; len=%u)\n", ret, dev_id, in->qid,
                      in->query_item, len);
        return ret;
    }

    ret = memcpy_s(rsp->rsp_data_buf, (size_t)rsp->rsp_data_buf_len, (void *)&out,
                   sizeof(struct que_get_status_out_msg));
    if (que_unlikely(ret != EOK)) {
        QUEUE_LOG_ERR("event response copy fail. (ret=%d; devid=%u; qid=%u; res_buf_len=%d; msg_size=%ld)\n", ret,
                      dev_id, in->qid, rsp->rsp_data_buf_len, sizeof(struct que_get_status_out_msg));
        return DRV_ERROR_MEMORY_OPT_FAIL;
    }
    rsp->need_rsp = true;

    return DRV_ERROR_NONE;
}

struct drv_event_proc que_comm_subevent_proc[DRV_SUBEVENT_QUEUE_MAX_NUM] = {
    [DRV_SUBEVENT_QUEUE_EXPORT_INTER_DEV_MSG] = {.proc_func = que_inter_dev_export_tgt,
                                                 .proc_size = sizeof(struct que_inter_dev_export_import_in_msg),
                                                 .proc_name = "que_export"},
    [DRV_SUBEVENT_QUEUE_UNEXPORT_INTER_DEV_MSG] = {.proc_func = que_inter_dev_unexport_tgt,
                                                   .proc_size = sizeof(struct que_inter_dev_export_import_in_msg),
                                                   .proc_name = "que_unexport"},
    [DRV_SUBEVENT_QUEUE_IMPORT_MSG] = {.proc_func = que_inter_dev_import_tgt,
                                       .proc_size = sizeof(struct que_inter_dev_export_import_in_msg),
                                       .proc_name = "que_import"},
    [DRV_SUBEVENT_QUEUE_IMPORT_INTER_DEV_MSG] = {.proc_func = que_inter_dev_import_h2d_tgt,
                                                 .proc_size = sizeof(struct que_inter_dev_export_import_in_msg),
                                                 .proc_name = "que_import_h2d"},
    [DRV_SUBEVENT_QUEUE_UNIMPORT_INTER_DEV_MSG] = {.proc_func = que_inter_dev_unimport_h2d_tgt,
                                                   .proc_size = sizeof(struct que_inter_dev_export_import_in_msg),
                                                   .proc_name = "que_unimport_h2d"},
    [DRV_SUBEVENT_SUBF2NF_INTER_DEV_MSG] = {.proc_func = que_inter_dev_sub_f2nf_tgt,
                                            .proc_size = sizeof(struct que_sub_event_in_msg),
                                            .proc_name = "que_sub_f2nf"},
    [DRV_SUBEVENT_QUERY_MSG] = {.proc_func = que_inter_dev_query_info_tgt,
                                .proc_size = sizeof(struct que_query_info_in_msg),
                                .proc_name = "que_query_info"},
    [DRV_SUBEVENT_GET_QUEUE_STATUS_MSG] = {.proc_func = que_inter_dev_get_status_tgt,
                                           .proc_size = sizeof(struct que_get_status_in_msg),
                                           .proc_name = "que_status_get"},
    [DRV_SUBEVENT_ATTACH_INTER_DEV_MSG] = {.proc_func = que_inter_dev_attach_tgt,
                                           .proc_size = sizeof(struct que_inter_dev_attach_in_msg),
                                           .proc_name = "que_attach"},
};

struct drv_event_proc *que_get_comm_subevent_proc(enum drv_subevent_id subevent)
{
    return &que_comm_subevent_proc[subevent];
}

#else

void que_inter_dev_tgt_test(void)
{}

#endif