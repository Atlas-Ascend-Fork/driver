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
#include "que_con_type.h"
#include "que_comm_event.h"
#include "que_comm_chan.h"
#include "que_comm_ctx.h"
#include "pbl_uda_user.h"
#include "queue_interface.h"
#include "ascend_hal_define.h"
#include "que_inter_dev_ini.h"

static drvError_t queue_inter_dev_init(unsigned int dev_id)
{
    drvError_t ret;
    int64_t val;
    ret = dms_get_connect_type(&val);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("dms_get_connect_type failed. (ret=%d)\n", ret);
        return ret;
    }
    if (val == HOST_DEVICE_CONNECT_TYPE_UB) {
        ret = que_ub_res_init(dev_id);
        if (que_unlikely((ret != DRV_ERROR_NONE) && (ret != DRV_ERROR_REPEATED_INIT))) {
            QUEUE_LOG_ERR("que clt ub res init fail. (ret=%d; devid=%u)\n", ret, dev_id);
            return ret;
        }
        bool uninit_flag = (ret != DRV_ERROR_REPEATED_INIT);

        ret = queue_init_local(dev_id);
        if (que_unlikely((ret != DRV_ERROR_NONE) && (ret != DRV_ERROR_REPEATED_INIT))) {
            que_ub_res_uninit(dev_id, uninit_flag);
            QUEUE_LOG_ERR("que init local fail. (ret=%d; devid=%u)\n", ret, dev_id);
            return ret;
        }
    } else {
        ret = queue_init_local(dev_id);
        if (que_unlikely((ret != DRV_ERROR_NONE) && (ret != DRV_ERROR_REPEATED_INIT))) {
            QUEUE_LOG_ERR("que init local fail. (ret=%d; devid=%u)\n", ret, dev_id);
            return ret;
        }
    }

    return DRV_ERROR_NONE;
}

static drvError_t que_inter_dev_send_tjetty(unsigned int dev_id, unsigned int qid, unsigned int remote_qid,
                                            unsigned int remote_que_depth, urma_jfr_id_t *local_tjfr_id,
                                            urma_token_t token)
{
    int ret;
    struct que_inter_dev_attach_in_msg in = {
        .qid = remote_qid, .depth = remote_que_depth, .tjfr_id = *local_tjfr_id, .token = token};
    struct que_event_msg event_msg = {
        .in = (char *)&in, .in_len = sizeof(struct que_inter_dev_attach_in_msg), .out = NULL, .out_len = 0};
    ret = que_comm_event_send_ex(dev_id, qid, DRV_SUBEVENT_ATTACH_INTER_DEV_MSG, &event_msg, QUE_EVENT_MAX_WAIT_10S);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        return ret;
    }
    return DRV_ERROR_NONE;
}

static drvError_t que_inter_dev_attach_proc(unsigned int dev_id, unsigned int qid, int inter_dev_state,
                                            unsigned long create_time)
{
    int ret, destroy_ret;
    unsigned int peer_qid, d2d_flag;
    struct que_peer_que_attr peer_que_attr;
    urma_jfr_id_t tjfr_id;
    urma_token_t token;

    ret = que_ctx_chan_check(dev_id, qid, create_time);
    if (que_unlikely((ret != DRV_ERROR_NONE) && (ret != DRV_ERROR_QUEUE_REPEEATED_INIT))) {
        QUEUE_LOG_ERR("que ctx queue check fail. (ret=%d; devid=%u; qid=%u)\n", ret, dev_id, qid);
        return ret;
    }

    ret = que_get_peer_que_info(dev_id, qid, &peer_qid, &peer_que_attr);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        return (ret == DRV_ERROR_NO_RESOURCES) ? DRV_ERROR_RESUME : ret;
    }

    que_get_d2d_flag(dev_id, peer_que_attr.peer_devid, &d2d_flag);
    ret = que_ctx_chan_create(dev_id, qid, CHAN_INTER_DEV_ATTACH, create_time, d2d_flag);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        if (ret == DRV_ERROR_QUEUE_REPEEATED_INIT) {
            return DRV_ERROR_NONE;
        }
        QUEUE_LOG_ERR("que ctx queue create fail. (ret=%d; devid=%u; qid=%u)\n", ret, dev_id, qid);
        return ret;
    }

    ret = que_ctx_chan_update(dev_id, peer_que_attr.peer_devid, qid, &tjfr_id, &token);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        goto out;
    }

    if (inter_dev_state == QUEUE_STATE_EXPORTED) {
        ret = que_inter_dev_send_tjetty(dev_id, qid, peer_qid, peer_que_attr.depth, &tjfr_id, token);
        if (que_unlikely(ret != DRV_ERROR_NONE)) {
            goto out;
        }
    }

    return ret;

out:
    QUEUE_LOG_ERR("que inter dev attach fail. (ret=%d; devid=%u; qid=%u)\n", ret, dev_id, qid);
    destroy_ret = que_ctx_chan_destroy(dev_id, qid);
    if (que_unlikely(destroy_ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que ctx queue destroy fail. (ret=%d; devid=%u; qid=%u)\n", destroy_ret, dev_id, qid);
    }
    return ret;
}

static drvError_t que_inter_dev_attach_ini(unsigned int dev_id, unsigned int qid, int time_out)
{
    int ret;
    unsigned long create_time;
    int inter_dev_state = QUEUE_STATE_DISABLED;

    ret = queue_attach_local(dev_id, qid, time_out);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        return ret;
    }
    ret = queue_get_qid_create_time(dev_id, qid, &create_time);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que get create time fail. (ret=%d; devid=%u)\n", ret, dev_id);
        return ret;
    }

    ret = que_get_inter_dev_status(dev_id, qid, &inter_dev_state, NULL);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("que get inter dev status fail. (ret=%d; devid=%u; inter_dev_state=%d)\n", ret, dev_id,
                      inter_dev_state);
        return ret;
    }

    if (inter_dev_state == QUEUE_STATE_DISABLED) {
        return DRV_ERROR_NONE;
    }

    if ((inter_dev_state == QUEUE_STATE_EXPORTED) || (inter_dev_state == QUEUE_STATE_IMPORTED)) {
        return que_inter_dev_attach_proc(dev_id, qid, inter_dev_state, create_time);
    }
    QUEUE_LOG_INFO("que operation not support. (inter_dev_state=%d)\n", inter_dev_state);
    return DRV_ERROR_NOT_SUPPORT;
}

static drvError_t que_inter_dev_f2nf_sub(unsigned int devid, unsigned int qid, unsigned int peer_qid,
                                         struct que_peer_que_attr *peer_que_attr)
{
    int ret;
    struct que_f2nf_res f2nf_res;
    struct que_sub_event_in_msg in = {.tid = QUEUE_INVALID_VALUE, .qid = peer_qid};
    struct que_event_msg event_msg = {
        .in = (char *)&in, .in_len = sizeof(struct que_sub_event_in_msg), .out = NULL, .out_len = 0};

    (void)peer_que_attr;

    ret = que_ctx_get_f2nf_res(devid, qid, &f2nf_res);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    in.pid = getpid();
    in.grp_id = f2nf_res.f2nf_event_res.gid;
    in.event_id = f2nf_res.f2nf_event_res.event_id;
    in.dst_phy_devid = que_get_unified_devid(devid);
    in.dst_engine = que_get_sched_engine_type(devid);
    in.inner_sub_flag = QUEUE_INTER_SUB_FLAG;

    ret = que_comm_event_send_ex(devid, qid, DRV_SUBEVENT_SUBF2NF_INTER_DEV_MSG, &event_msg, QUE_EVENT_MAX_WAIT_10S);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("sub event send fail. (ret=%d; devid=%u; local_qid=%u; remote_qid=%u)\n", ret, devid, qid,
                      in.qid);
        return ret;
    }

    return DRV_ERROR_NONE;
}

static drvError_t que_inter_dev_sub_event_ini(struct QueueSubPara *sub_para)
{
    int ret, unsub_ret;
    unsigned int peer_qid;
    struct que_peer_que_attr peer_que_attr;
    struct QueueUnsubPara unsub_para = {0};

    ret = queue_sub_event_local(sub_para);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    if (sub_para->eventType != QUEUE_F2NF_EVENT) {
        return DRV_ERROR_NONE;
    }

    ret = que_get_peer_que_info(sub_para->devId, sub_para->qid, &peer_qid, &peer_que_attr);
    if (ret != DRV_ERROR_NONE) {
        goto unsub;
    }

    ret = que_inter_dev_f2nf_sub(sub_para->devId, sub_para->qid, peer_qid, &peer_que_attr);
    if (ret != DRV_ERROR_NONE) {
        ret = DRV_ERROR_QUEUE_INNER_ERROR;
        goto unsub;
    }
    return DRV_ERROR_NONE;

unsub:
    unsub_para.devId = sub_para->devId;
    unsub_para.qid = sub_para->qid;
    unsub_para.eventType = QUEUE_F2NF_EVENT;
    unsub_ret = queue_unsub_event_local(&unsub_para);
    if (unsub_ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("que unsub f2nf failed. (ret=%d; devid=%u; local_qid=%d)\n", unsub_ret, sub_para->devId,
                      sub_para->qid);
    }
    return ret;
}

static drvError_t que_inter_dev_query_info_ini(unsigned int dev_id, unsigned int qid, QueueInfo *que_info)
{
    int ret;
    struct que_peer_que_attr peer_que_attr = {0};
    struct que_query_info_in_msg in = {0};
    struct que_query_info_out_msg out;
    struct que_event_msg event_msg = {.in = (char *)&in,
                                      .in_len = sizeof(struct que_query_info_in_msg),
                                      .out = (char *)&out,
                                      .out_len = sizeof(struct que_query_info_out_msg)};

    ret = que_get_peer_que_info(dev_id, qid, &in.qid, &peer_que_attr);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        return ret;
    }

    if (peer_que_attr.inter_dev_state == QUEUE_STATE_EXPORTED) {
        ret = queue_query_info_local(dev_id, qid, que_info);
        if (que_unlikely(ret != DRV_ERROR_NONE)) {
            QUEUE_LOG_ERR("que query info fail. (ret=%d; devid=%u; qid=%u)\n", ret, dev_id, qid);
        }
        return ret;
    }

    if (peer_que_attr.inter_dev_state != QUEUE_STATE_IMPORTED) {
        QUEUE_LOG_WARN("query info not success. (devid=%u; qid=%u; remote_qid=%u; inter_dev_state=%d)\n", dev_id, qid,
                       in.qid, peer_que_attr.inter_dev_state);
        return DRV_ERROR_NOT_EXIST;
    }

    ret = que_comm_event_send_ex(dev_id, qid, DRV_SUBEVENT_QUERY_MSG, &event_msg, QUE_EVENT_MAX_WAIT_10S);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("query info send fail. (ret=%d; devid=%u; local_qid=%u; remote_qid=%u)\n", ret, dev_id, qid,
                      in.qid);
        return ret;
    }

    que_info->size = out.size;
    que_info->status = out.status;
    que_info->workMode = out.work_mode;
    que_info->stat.enqueCnt = out.enque_cnt;
    que_info->stat.dequeCnt = out.deque_cnt;

    return DRV_ERROR_NONE;
}

static drvError_t que_inter_dev_get_peer_status(unsigned int dev_id, unsigned int qid, QUEUE_QUERY_ITEM query_item,
                                                unsigned int len, void *data)
{
    int ret;
    struct que_peer_que_attr peer_que_attr = {0};
    struct que_get_status_in_msg in = {.query_item = query_item, .out_len = len};
    struct que_get_status_out_msg out;
    struct que_event_msg event_msg = {.in = (char *)&in,
                                      .in_len = sizeof(struct que_get_status_in_msg),
                                      .out = (char *)&out,
                                      .out_len = sizeof(struct que_get_status_out_msg)};

    ret = que_get_peer_que_info(dev_id, qid, &in.qid, &peer_que_attr);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        return ret;
    }

    if (peer_que_attr.inter_dev_state == QUEUE_STATE_EXPORTED) {
        ret = queue_get_status_local(dev_id, qid, query_item, len, data);
        if (que_unlikely(ret != DRV_ERROR_NONE)) {
            QUEUE_LOG_ERR("que get status fail. (ret=%d; devid=%u; qid=%u; query_item=%d; len=%u)\n", ret, dev_id, qid,
                          query_item, len);
        }
        return ret;
    }

    if (peer_que_attr.inter_dev_state != QUEUE_STATE_IMPORTED) {
        QUEUE_LOG_WARN("query status not success. (devid=%u; qid=%u; remote_qid=%u; inter_dev_state=%d)\n", dev_id, qid,
                       in.qid, peer_que_attr.inter_dev_state);
        return DRV_ERROR_NOT_EXIST;
    }

    ret = que_comm_event_send_ex(dev_id, qid, DRV_SUBEVENT_GET_QUEUE_STATUS_MSG, &event_msg, QUE_EVENT_MAX_WAIT_10S);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("get que status event send fail. (ret=%d; devid=%u; local_qid=%u; remote_qid=%u)\n", ret, dev_id,
                      qid, in.qid);
        return ret;
    }

    ret = memcpy_s(data, len, out.data, len);
    if (que_unlikely(ret != EOK)) {
        QUEUE_LOG_ERR("get que status copy fail. (ret=%d; dev_id=%u; qid=%u; len=%u)\n", ret, dev_id, qid, len);
        return DRV_ERROR_INVALID_VALUE;
    }
    return DRV_ERROR_NONE;
}

static drvError_t que_inter_dev_get_status_ini(unsigned int dev_id, unsigned int qid, QUEUE_QUERY_ITEM query_item,
                                               unsigned int len, void *data)
{
    int ret;

    switch (query_item) {
        case QUERY_INTER_QUEUE_IMPORT_STATUS:
            if (len != sizeof(int)) {
                QUEUE_LOG_ERR("len invalid. (len=%u; expect=%zu)\n", len, sizeof(int));
                return DRV_ERROR_INVALID_VALUE;
            }
            que_get_single_export_que_import_stat(dev_id, qid, (unsigned int *)data);
            break;
        case QUERY_INTER_QUEUE_ALL_IMPORT_STATUS:
            if (len != sizeof(int)) {
                QUEUE_LOG_ERR("len invalid. (len=%u; expect=%zu)\n", len, sizeof(int));
                return DRV_ERROR_INVALID_VALUE;
            }
            que_get_all_export_que_import_stat(dev_id, (unsigned int *)data);
            break;
        default:
            ret = que_inter_dev_get_peer_status(dev_id, qid, query_item, len, data);
            if (que_unlikely(ret != DRV_ERROR_NONE)) {
                return ret;
            }
            break;
    }

    return DRV_ERROR_NONE;
}

static drvError_t que_inter_dev_enque_ini(unsigned int devid, unsigned int qid, void *mbuf)
{
    drvError_t ret;
    unsigned int num = 0;
    if (que_unlikely(queue_inter_devid_invalid(devid)) || (qid >= MAX_SURPORT_QUEUE_NUM)) {
        QUEUE_LOG_ERR("para is error. (dev_id=%u, qid=%u)\n", devid, qid);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = halMbufChainGetMbufNum(mbuf, &num);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("get mbuf num failed. (ret=%d; qid=%u; devid=%u)\n", ret, qid, devid);
        return ret;
    }

    if (num > 1) { // 1:for inter_dev queue, mbuf chains are not supported
        QUEUE_LOG_ERR("mbuf num is invalid. (num=%u; qid=%u; devid=%u)\n", num, qid, devid);
        return DRV_ERROR_PARA_ERROR;
    }

    return que_ctx_async_ini(devid, qid, mbuf);
}

drvError_t que_set_inter_dev_info(const struct que_inter_dev_export_import_in_msg *in, struct queue_manages *que_manage)
{
    drvError_t ret;

    ret = strncpy_s(que_manage->share_queue_name, SHARE_QUEUE_NAME_MAX_LEN, in->share_queue_name,
                    strnlen(in->share_queue_name, SHARE_QUEUE_NAME_LEN));
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("strncpy share que name failed. (qid=%u; ret=%d)\n", que_manage->id, ret);
        return DRV_ERROR_INVALID_VALUE;
    }

    que_manage->share_queue_name[SHARE_QUEUE_NAME_LEN] = '\0';
    (void)ATOMIC_SET(&que_manage->remote_devpid, in->devpid);
    (void)ATOMIC_SET(&que_manage->remote_devid, in->peer_dev_id);
    (void)ATOMIC_SET(&que_manage->phy_remote_devid, in->phy_peer_devid);
    (void)ATOMIC_SET(&que_manage->remote_grpid, in->grpid);

    return DRV_ERROR_NONE;
}

drvError_t que_set_inter_dev_info_ex(unsigned int remote_devid, const char *share_que_name,
                                     struct queue_manages *que_manage)
{
    unsigned int remote_grpid = 0;
    unsigned int devid = que_manage->dev_id;
    pid_t remote_devpid;
    drvError_t ret;
    unsigned int phy_remote_devid = 0;

    ret = que_inter_dev_get_info(devid, remote_devid, &remote_devpid, &remote_grpid);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que get inter dev info failed. (ret=%d; devid=%u; remote_devid=%d)\n", ret, devid, remote_devid);
        return ret;
    }

    ret = strncpy_s(que_manage->share_queue_name, SHARE_QUEUE_NAME_MAX_LEN, share_que_name,
                    strnlen(share_que_name, SHARE_QUEUE_NAME_LEN));
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("strncpy share que name failed. (qid=%u; ret=%d)\n", que_manage->id, ret);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = uda_get_udevid_by_devid_ex(remote_devid, &phy_remote_devid);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("get udevid failed. (remote_devid=%u)\n", remote_devid);
        return ret;
    }

    que_manage->share_queue_name[SHARE_QUEUE_NAME_LEN] = '\0';
    (void)ATOMIC_SET(&que_manage->remote_devpid, remote_devpid);
    (void)ATOMIC_SET(&que_manage->remote_devid, remote_devid);
    (void)ATOMIC_SET(&que_manage->phy_remote_devid, phy_remote_devid);
    (void)ATOMIC_SET(&que_manage->remote_grpid, remote_grpid);

    return DRV_ERROR_NONE;
}

void que_clear_inter_dev_info(struct queue_manages *que_mng)
{
    (void)ATOMIC_SET(&que_mng->remote_devpid, QUE_INTER_DEV_INVALID_VALUE);
    (void)ATOMIC_SET(&que_mng->remote_devid, QUEUE_INVALID_VALUE);
    (void)ATOMIC_SET(&que_mng->phy_remote_devid, QUEUE_INVALID_VALUE);
    (void)ATOMIC_SET(&que_mng->remote_grpid, QUE_INTER_DEV_INVALID_VALUE);
    (void)ATOMIC_SET(&que_mng->remote_qid, QUEUE_INVALID_VALUE);
}

void que_set_import_que_attr(struct queue_manages *que_mng, struct que_inter_dev_import_out_msg que_out_info)
{
    unsigned int qid = que_mng->id;

    (void)ATOMIC_SET(&que_mng->remote_qid, que_out_info.qid);
    (void)ATOMIC_SET(&que_mng->work_mode, que_out_info.work_mode);
    (void)ATOMIC_SET(&que_mng->fctl_flag, que_out_info.flow_ctrl_flag);
    (void)ATOMIC_SET(&que_mng->drop_time, que_out_info.flow_ctrl_drop_time);
    queue_set_local_depth(qid, que_out_info.depth);
}

drvError_t que_share_que_name_cmp(const char *que_share_name_src, const char *que_share_name_dst,
                                  unsigned int que_inter_dev_flag)
{
    int que_share_name_src_len;
    int que_share_name_dst_len;

    que_share_name_src_len = (int)strnlen(que_share_name_src, SHARE_QUEUE_NAME_LEN);
    que_share_name_dst_len = (int)strnlen(que_share_name_dst, SHARE_QUEUE_NAME_LEN);
    if (que_share_name_src_len != que_share_name_dst_len) {
        if (que_inter_dev_flag != QUE_TGT_FLAG) {
            QUEUE_LOG_ERR("name len compare failed. (que_share_name_src_len=%d, que_share_name_dst_len=%d)\n",
                          que_share_name_src_len, que_share_name_dst_len);
        }
        return DRV_ERROR_INVALID_VALUE;
    }

    if (strncmp(que_share_name_src, que_share_name_dst, (size_t)que_share_name_src_len) != 0) {
        if (que_inter_dev_flag != QUE_TGT_FLAG) {
            QUEUE_LOG_ERR("share que name is invalid.\n");
        }
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

drvError_t que_inter_dev_export_ini_local(unsigned int dev_id, unsigned int qid, unsigned int phy_remote_devid,
                                          struct shareQueInfo *que_info)
{
    struct queue_manages *que_mng = NULL;
    drvError_t ret;

    if (!queue_is_init()) {
        QUEUE_LOG_ERR("queue not init.\n");
        return DRV_ERROR_UNINIT;
    }

    if (!queue_get(qid)) {
        QUEUE_LOG_ERR("queue get failed. (qid=%u)\n", qid);
        return DRV_ERROR_NOT_EXIST;
    }

    que_mng = queue_get_local_mng(qid);
    if (que_mng == NULL) {
        QUEUE_LOG_ERR("Queue local manage is null. (dev_id=%u, qid=%u)\n", dev_id, qid);
        ret = DRV_ERROR_NOT_EXIST;
        goto out;
    }

    if (que_mng->valid != QUEUE_CREATED) {
        QUEUE_LOG_ERR("queue is not created. (qid=%u)\n", qid);
        ret = DRV_ERROR_NOT_EXIST;
        goto out;
    }

    if (CAS(&que_mng->inter_dev_state, QUEUE_STATE_DISABLED, QUEUE_STATE_BUSY) == false) {
        QUEUE_LOG_ERR("queue inter_dev_state cas busy failed. (qid=%u, inter_dev_state=%d)\n", qid,
                      que_mng->inter_dev_state);
        ret = DRV_ERROR_REPEATED_INIT;
        goto out;
    }

    ret = strncpy_s(que_mng->share_queue_name, SHARE_QUEUE_NAME_MAX_LEN, que_info->shareQueName,
                    strnlen(que_info->shareQueName, SHARE_QUEUE_NAME_LEN));
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("strncpy share que name failed. (qid=%u; ret=%d)\n", que_mng->id, ret);
        (void)ATOMIC_SET(&que_mng->inter_dev_state, QUEUE_STATE_DISABLED);
        goto out;
    }
    que_mng->share_queue_name[SHARE_QUEUE_NAME_LEN] = '\0';
    (void)ATOMIC_SET(&que_mng->remote_devid, que_info->peerDevId);
    (void)ATOMIC_SET(&que_mng->phy_remote_devid, phy_remote_devid);
    (void)ATOMIC_SET(&que_mng->inter_dev_state, QUEUE_STATE_EXPORTED);

    queue_put(qid);
    return DRV_ERROR_NONE;
out:
    queue_put(qid);
    return ret;
}

static drvError_t que_inter_dev_export_unexport_send(unsigned int devid, unsigned int qid,
                                                     struct shareQueInfo *que_info, unsigned int sub_event)
{
    struct que_inter_dev_export_import_in_msg in = {.qid = qid, .dev_id = devid, .peer_dev_id = que_info->peerDevId};
    struct que_event_msg event_msg;
    struct que_comm_event_attr attr = {.devid = halGetHostDevid(), .sub_event = sub_event, .local_pid = getpid()};
    int ret;

    ret = uda_get_udevid_by_devid_ex(que_info->peerDevId, &in.phy_peer_devid);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("get udevid failed. (peerDevId=%u)\n", que_info->peerDevId);
        return ret;
    }

    ret = uda_get_udevid_by_devid_ex(devid, &attr.remote_devid);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("get udevid failed. (devid=%u)\n", que_info->peerDevId);
        return ret;
    }

    ret = que_inter_dev_get_info(halGetHostDevid(), devid, &attr.remote_pid, &attr.remote_grpid);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que get inter dev info failed. (ret=%d; devid=%u; local_devid=%d)\n", ret, devid,
                      halGetHostDevid());
        return ret;
    }

    ret = strncpy_s(in.share_queue_name, SHARE_QUEUE_NAME_MAX_LEN, que_info->shareQueName,
                    strnlen(que_info->shareQueName, SHARE_QUEUE_NAME_LEN));
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("strncpy share que name failed. (qid=%u; ret=%d)\n", qid, ret);
        return DRV_ERROR_INVALID_VALUE;
    }
    in.share_queue_name[SHARE_QUEUE_NAME_LEN] = '\0';

    event_msg.in = (char *)&in;
    event_msg.out = NULL;
    event_msg.in_len = sizeof(struct que_inter_dev_export_import_in_msg);
    event_msg.out_len = 0;

    ret = que_comm_event_send(&attr, &event_msg, QUE_EVENT_MAX_WAIT_10S);
    if ((ret != DRV_ERROR_NONE) && (ret != DRV_ERROR_RESUME)) {
        QUEUE_LOG_ERR("que event send fail. (ret=%d; devid=%u; qid=%u; sub_event=%d)\n", ret, devid, qid, sub_event);
    }

    return ret;
}

static drvError_t que_inter_dev_export_ini(unsigned int dev_id, unsigned int qid, struct shareQueInfo *que_info)
{
    int ret;
    unsigned int phy_devid = 0;
    if (dev_id == halGetHostDevid()) {
        ret = uda_get_udevid_by_devid_ex(que_info->peerDevId, &phy_devid);
        if (ret != DRV_ERROR_NONE) {
            QUEUE_LOG_ERR("get udevid failed. (peerDevId=%u)\n", que_info->peerDevId);
            return ret;
        }
        return que_inter_dev_export_ini_local(dev_id, qid, phy_devid, que_info);
    }

    ret = que_inter_dev_export_unexport_send(dev_id, qid, que_info, DRV_SUBEVENT_QUEUE_EXPORT_INTER_DEV_MSG);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("que export send fail. (ret=%d; devid=%u; qid=%u)\n", ret, dev_id, qid);
        return ret;
    }

    return DRV_ERROR_NONE;
}

drvError_t que_inter_dev_unexport_ini_local(unsigned int dev_id, unsigned int qid, struct shareQueInfo *que_info)
{
    struct queue_manages *que_mng = NULL;

    if (!queue_is_init()) {
        QUEUE_LOG_ERR("queue not init.\n");
        return DRV_ERROR_UNINIT;
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

    if (que_mng->valid != QUEUE_CREATED) {
        queue_put(qid);
        QUEUE_LOG_ERR("queue is not created. (qid=%u)\n", qid);
        return DRV_ERROR_NOT_EXIST;
    }

    if ((que_share_que_name_cmp(que_info->shareQueName, que_mng->share_queue_name, QUE_INI_FLAG) != DRV_ERROR_NONE) ||
        (que_mng->remote_devid != que_info->peerDevId)) {
        QUEUE_LOG_ERR("queue inter dev info compare failed. (remote_devid=%u, peer_devid=%u)\n", que_mng->remote_devid,
                      que_info->peerDevId);
        queue_put(qid);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (CAS(&que_mng->inter_dev_state, QUEUE_STATE_EXPORTED, QUEUE_STATE_UNEXPORTED) == false) {
        queue_put(qid);
        QUEUE_LOG_ERR("queue inter_dev_state unexport cas failed.(qid=%u, inter_dev_state=%d)\n", qid,
                      que_mng->inter_dev_state);
        return DRV_ERROR_INVALID_VALUE;
    }

    (void)que_chan_inter_dev_clear_mbuf(dev_id, qid);
    que_clear_inter_dev_info(que_mng);
    queue_put(qid);

    return DRV_ERROR_NONE;
}

static drvError_t que_inter_dev_unexport_ini(unsigned int dev_id, unsigned int qid, struct shareQueInfo *que_info)
{
    int ret;
    if (dev_id == halGetHostDevid()) {
        return que_inter_dev_unexport_ini_local(dev_id, qid, que_info);
    }

    ret = que_inter_dev_export_unexport_send(dev_id, qid, que_info, DRV_SUBEVENT_QUEUE_UNEXPORT_INTER_DEV_MSG);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("que unexport send fail. (ret=%d; devid=%u; qid=%u)\n", ret, dev_id, qid);
        return ret;
    }

    return DRV_ERROR_NONE;
}

drvError_t que_inter_dev_unimport_ini_local(unsigned int dev_id, unsigned int qid, struct shareQueInfo *que_info)
{
    struct queue_manages *que_mng = NULL;
    unsigned int virtual_qid;
    drvError_t destroy_ret;

    if (!queue_is_init()) {
        QUEUE_LOG_ERR("queue not init.\n");
        return DRV_ERROR_UNINIT;
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

    if (que_mng->valid != QUEUE_CREATED) {
        queue_put(qid);
        QUEUE_LOG_ERR("queue is not created. (qid=%u)\n", qid);
        return DRV_ERROR_NOT_EXIST;
    }

    if ((que_share_que_name_cmp(que_info->shareQueName, que_mng->share_queue_name, QUE_INI_FLAG) != DRV_ERROR_NONE) ||
        (que_mng->remote_devid != que_info->peerDevId)) {
        QUEUE_LOG_ERR("queue inter dev info compare failed. (remote_devid=%u, peer_devid=%u)\n", que_mng->remote_devid,
                      que_info->peerDevId);
        queue_put(qid);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (CAS(&que_mng->inter_dev_state, QUEUE_STATE_IMPORTED, QUEUE_STATE_UNIMPORTED) == false) {
        queue_put(qid);
        QUEUE_LOG_ERR("queue inter_dev_state unimport cas failed.(qid=%u, inter_dev_state=%d)\n", qid,
                      que_mng->inter_dev_state);
        return DRV_ERROR_INVALID_VALUE;
    }

    (void)que_chan_inter_dev_clear_mbuf(dev_id, qid);
    que_clear_inter_dev_info(que_mng);
    queue_put(qid);

    virtual_qid = queue_get_virtual_qid(qid, INTER_DEV_QUEUE);
    destroy_ret = halQueueDestroy(dev_id, virtual_qid);
    if (destroy_ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("queue destroy failed. (dev_id=%u, actual_qid=%u, virtual_qid=%u, destroy_ret=%d)\n", dev_id, qid,
                      virtual_qid, destroy_ret);
    }
    return DRV_ERROR_NONE;
}

drvError_t que_inter_dev_unimport_ini(unsigned int dev_id, unsigned int qid, struct shareQueInfo *que_info)
{
    int ret;
    if (dev_id == halGetHostDevid()) {
        return que_inter_dev_unimport_ini_local(dev_id, qid, que_info);
    }
    ret = que_inter_dev_export_unexport_send(dev_id, qid, que_info, DRV_SUBEVENT_QUEUE_UNIMPORT_INTER_DEV_MSG);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("que unimport send fail. (ret=%d; devid=%u; qid=%u)\n", ret, dev_id, qid);
        return ret;
    }

    return DRV_ERROR_NONE;
}

static drvError_t que_inter_dev_import_send(unsigned int devid, struct shareQueInfo *que_info, unsigned int actual_qid,
                                            unsigned int virtual_qid, pid_t devpid, unsigned int grpid,
                                            struct que_inter_dev_import_out_msg *que_out_info)
{
    unsigned int host_dev_id = halGetHostDevid();
    struct que_inter_dev_export_import_in_msg in = {
        .qid = virtual_qid, .dev_id = que_info->peerDevId, .peer_dev_id = devid, .devpid = devpid, .grpid = grpid};
    struct que_event_msg event_msg;
    int ret = uda_get_udevid_by_devid_ex(devid, &in.phy_peer_devid);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("get udevid failed. (devid=%u)\n", devid);
        return ret;
    }

    ret = strncpy_s(in.share_queue_name, SHARE_QUEUE_NAME_MAX_LEN, que_info->shareQueName,
                    strnlen(que_info->shareQueName, SHARE_QUEUE_NAME_LEN));
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("strncpy share que name failed. (qid=%u; ret=%d)\n", virtual_qid, ret);
        return DRV_ERROR_INVALID_VALUE;
    }
    in.share_queue_name[SHARE_QUEUE_NAME_LEN] = '\0';

    event_msg.in = (char *)&in;
    event_msg.out = (char *)que_out_info;
    event_msg.in_len = sizeof(struct que_inter_dev_export_import_in_msg);
    event_msg.out_len = sizeof(struct que_inter_dev_import_out_msg);

    if ((devid != host_dev_id) && (que_info->peerDevId != host_dev_id)) {
        ret = que_comm_event_send_d2d(host_dev_id, que_info->peerDevId, DRV_SUBEVENT_QUEUE_IMPORT_MSG, &event_msg,
                                      QUE_EVENT_MAX_WAIT_10S);
        if ((ret != DRV_ERROR_NONE)) {
            QUEUE_LOG_ERR("que import event send d2d fail. (ret=%d; local devid=%u; peer devid=%u)\n", ret, host_dev_id,
                          que_info->peerDevId);
        }
    } else {
        ret = que_comm_event_send_ex(devid, actual_qid, DRV_SUBEVENT_QUEUE_IMPORT_MSG, &event_msg,
                                     QUE_EVENT_MAX_WAIT_10S);
        if ((ret != DRV_ERROR_NONE) && (ret != DRV_ERROR_RESUME)) {
            QUEUE_LOG_ERR("que import event send fail. (ret=%d; devid=%u; qid=%u)\n", ret, devid, virtual_qid);
        }
    }

    return ret;
}

static drvError_t que_inter_dev_import_proc(unsigned int devid, unsigned int actual_qid, unsigned int virtual_qid,
                                            struct shareQueInfo *que_info)
{
    struct que_inter_dev_import_out_msg que_out_info;
    struct queue_manages *que_mng = NULL;
    pid_t devpid = 0;
    unsigned int grpid = 0;
    drvError_t ret;

    que_mng = queue_get_local_mng(actual_qid);
    if (que_mng == NULL) {
        QUEUE_LOG_ERR("queue local mng is null. (devid=%u, actual_qid=%u, virtual_qid=%u)\n", devid, actual_qid,
                      virtual_qid);
        return DRV_ERROR_NOT_EXIST;
    }

    ret = que_set_inter_dev_info_ex(que_info->peerDevId, que_info->shareQueName, que_mng);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        return ret;
    }

    ret = que_get_local_devid_grpid(devid, &devpid, &grpid);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que get inter dev info failed. (ret=%d; devid=%u)\n", ret, devid);
        que_clear_inter_dev_info(que_mng);
        return ret;
    }

    ret = que_inter_dev_import_send(devid, que_info, actual_qid, virtual_qid, devpid, grpid, &que_out_info);
    if (ret != DRV_ERROR_NONE) {
        if (ret != DRV_ERROR_RESUME) {
            QUEUE_LOG_ERR("que import event send fail. (ret=%d; devid=%u)\n", ret, devid);
        }
        que_clear_inter_dev_info(que_mng);
        return ret;
    }
    que_set_import_que_attr(que_mng, que_out_info);
    (void)ATOMIC_SET(&que_mng->inter_dev_state, QUEUE_STATE_IMPORTED);

    return DRV_ERROR_NONE;
}

drvError_t que_inter_dev_import_ini_local(unsigned int dev_id, struct shareQueInfo *que_info, unsigned int *qid)
{
    QueueAttr que_attr = {.depth = MAX_QUEUE_DEPTH, .deploy_type = LOCAL_QUEUE_DEPLOY};
    unsigned int actual_qid;
    drvError_t ret, ret_post;

    if (!queue_is_init()) {
        QUEUE_LOG_ERR("queue not init.\n");
        return DRV_ERROR_UNINIT;
    }

    que_attr.name[0] = '\0';
    ret = halQueueCreate(dev_id, &que_attr, qid);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("local queue create failed. (ret=%d; dev_id=%u)\n", (int)ret, dev_id);
        return ret;
    }

    actual_qid = queue_get_actual_qid(*qid);
    ret = que_inter_dev_import_proc(dev_id, actual_qid, *qid, que_info);
    if (ret != DRV_ERROR_NONE) {
        goto out;
    }

    return DRV_ERROR_NONE;
out:
    ret_post = halQueueDestroy(dev_id, *qid);
    if (que_unlikely(ret_post != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que import post proc destroy queue failed. (ret=%d, devid=%u, actual_qid=%u, virtual_qid=%u)\n",
                      ret_post, dev_id, actual_qid, *qid);
    }
    return ret;
}

drvError_t que_inter_dev_match(unsigned int dev_id, const char *share_queue_name, unsigned int *qid)
{
    struct queue_manages *que_mng = NULL;
    unsigned int id;

    for (id = 0; id < MAX_SURPORT_QUEUE_NUM; id++) {
        if ((!queue_is_init()) || (queue_is_valid(id) == false)) {
            continue;
        }

        if (!queue_get(id)) {
            continue;
        }

        que_mng = queue_get_local_mng(id);
        if (que_mng == NULL) {
            queue_put(id);
            continue;
        }

        if ((que_mng->valid != QUEUE_CREATED) || (que_mng->inter_dev_state != QUEUE_STATE_EXPORTED)) {
            queue_put(id);
            continue;
        }

        if ((que_share_que_name_cmp(share_queue_name, que_mng->share_queue_name, QUE_TGT_FLAG) == DRV_ERROR_NONE)) {
            QUEUE_LOG_INFO("que inter dev info compare success. (name_src_len=%d, name_dst_len=%d, "
                           "devid_src=%u, devid_dst=%u)\n",
                           (int)strnlen(share_queue_name, SHARE_QUEUE_NAME_LEN),
                           (int)strnlen(que_mng->share_queue_name, SHARE_QUEUE_NAME_LEN), dev_id,
                           que_mng->remote_devid);
            queue_put(id);
            break;
        }

        queue_put(id);
    }

    if (id >= MAX_SURPORT_QUEUE_NUM) {
        return DRV_ERROR_RESUME;
    }

    *qid = id;
    return DRV_ERROR_NONE;
}

static drvError_t que_inter_dev_import_send_h2d(unsigned int devid, struct shareQueInfo *que_info,
                                                struct que_inter_dev_import_out_msg *que_out)
{
    int ret;
    struct que_inter_dev_export_import_in_msg in = {.dev_id = devid, .peer_dev_id = que_info->peerDevId};
    struct que_inter_dev_import_out_msg que_out_info = {0};
    struct que_comm_event_attr attr = {
        .devid = halGetHostDevid(), .sub_event = DRV_SUBEVENT_QUEUE_IMPORT_INTER_DEV_MSG, .local_pid = getpid()};
    struct que_event_msg event_msg;

    if (uda_get_udevid_by_devid_ex(que_info->peerDevId, &in.phy_peer_devid) != DRV_ERROR_NONE) {
        return DRV_ERROR_INNER_ERR;
    }

    if (uda_get_udevid_by_devid_ex(devid, &attr.remote_devid) != DRV_ERROR_NONE) {
        return DRV_ERROR_INNER_ERR;
    }

    if (que_info->peerDevId == halGetHostDevid()) {
        ret = que_get_local_devid_grpid(halGetHostDevid(), &in.devpid, &in.grpid);
        if (que_unlikely(ret != DRV_ERROR_NONE)) {
            QUEUE_LOG_ERR("que get inter dev info failed. (ret=%d; devid=%u)\n", ret, devid);
            return ret;
        }
    } else {
        ret = que_inter_dev_get_info(halGetHostDevid(), que_info->peerDevId, &in.devpid, &in.grpid);
        if (que_unlikely(ret != DRV_ERROR_NONE)) {
            QUEUE_LOG_ERR("que get inter dev info failed. (ret=%d; devid=%u; peer_devid=%d)\n", ret, devid,
                          que_info->peerDevId);
            return ret;
        }
    }

    ret = que_inter_dev_get_info(halGetHostDevid(), devid, &attr.remote_pid, &attr.remote_grpid);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_ERR("que get inter dev info failed. (ret=%d; devid=%u; peer_devid=%d)\n", ret, devid,
                      que_info->peerDevId);
        return ret;
    }

    ret = strncpy_s(in.share_queue_name, SHARE_QUEUE_NAME_MAX_LEN, que_info->shareQueName,
                    strnlen(que_info->shareQueName, SHARE_QUEUE_NAME_LEN));
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("strncpy share que name failed. (ret=%d)\n", ret);
        return DRV_ERROR_INVALID_VALUE;
    }
    in.share_queue_name[SHARE_QUEUE_NAME_LEN] = '\0';

    event_msg.in = (char *)&in;
    event_msg.out = (char *)&que_out_info;
    event_msg.in_len = sizeof(struct que_inter_dev_export_import_in_msg);
    event_msg.out_len = sizeof(struct que_inter_dev_import_out_msg);

    ret = que_comm_event_send(&attr, &event_msg, QUE_EVENT_MAX_WAIT_10S);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        return ret;
    }

    que_out->qid = que_out_info.qid;
    que_out->devpid = que_out_info.devpid;
    que_out->grpid = que_out_info.grpid;
    return DRV_ERROR_NONE;
}

static drvError_t que_inter_dev_match_fill_remote_qid(unsigned int dev_id, const char *share_queue_name,
                                                      unsigned int qid)
{
    int ret;
    unsigned int local_qid;
    struct queue_manages *que_mng;

    ret = que_inter_dev_match(dev_id, share_queue_name, &local_qid);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        QUEUE_LOG_WARN("que import match not success.\n");
        return ret;
    }

    if (!queue_get(local_qid)) {
        QUEUE_LOG_ERR("queue get failed. (qid=%u)\n", local_qid);
        return DRV_ERROR_NOT_EXIST;
    }

    que_mng = queue_get_local_mng(local_qid);
    if (que_mng == NULL) {
        queue_put(local_qid);
        QUEUE_LOG_ERR("Queue local manage is null. (dev_id=%u, qid=%u)\n", dev_id, local_qid);
        return DRV_ERROR_NOT_EXIST;
    }

    ret = que_set_inter_dev_info_ex(dev_id, share_queue_name, que_mng);
    if (que_unlikely(ret != DRV_ERROR_NONE)) {
        queue_put(local_qid);
        return ret;
    }

    if (CAS(&que_mng->remote_qid, QUEUE_INVALID_VALUE, qid) == false) {
        QUEUE_LOG_WARN("queue remote qid cas not success. (qid=%u, remote qid=%u)\n", qid, que_mng->remote_qid);
        queue_put(local_qid);
        return DRV_ERROR_INVALID_VALUE;
    }
    queue_put(local_qid);

    return DRV_ERROR_NONE;
}

drvError_t que_inter_dev_import_ini(unsigned int dev_id, struct shareQueInfo *que_info, unsigned int *qid)
{
    int ret, original_ret;
    unsigned int actual_qid;
    struct que_inter_dev_import_out_msg que_out = {0};
    struct que_inter_dev_import_out_msg que_out_tmp = {0};
    if (dev_id == halGetHostDevid()) {
        return que_inter_dev_import_ini_local(dev_id, que_info, qid);
    }

    ret = que_inter_dev_import_send_h2d(dev_id, que_info, &que_out);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("que export send fail. (ret=%d; devid=%u)\n", ret, dev_id);
        return ret;
    }

    if (que_info->peerDevId != halGetHostDevid()) {
        actual_qid = queue_get_actual_qid(que_out.qid);
        ret = que_inter_dev_import_send(dev_id, que_info, actual_qid, que_out.qid, que_out.devpid, que_out.grpid,
                                        &que_out_tmp);
        if (ret != DRV_ERROR_NONE) {
            QUEUE_LOG_ERR("que inter dev import send failed. (ret=%d)\n", ret);
            goto out;
        }
    } else {
        ret = que_inter_dev_match_fill_remote_qid(dev_id, que_info->shareQueName, que_out.qid);
        if (ret != DRV_ERROR_NONE) {
            QUEUE_LOG_ERR("que inter dev match fill remote qid failed. (ret=%d)\n", ret);
            goto out;
        }
    }

    *qid = que_out.qid;
    return DRV_ERROR_NONE;

out:
    original_ret = ret;
    ret = que_inter_dev_export_unexport_send(dev_id, que_out.qid, que_info, DRV_SUBEVENT_QUEUE_UNIMPORT_INTER_DEV_MSG);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("que unexport send fail. (ret=%d; devid=%u; qid=%u)\n", ret, dev_id, que_out.qid);
        return ret;
    }

    return original_ret;
}

static struct queue_comm_interface_list g_que_inter_dev_intf = {
    .queue_dc_init = queue_inter_dev_init,
    .queue_uninit = NULL,
    .queue_create = NULL,
    .queue_grant = NULL,
    .queue_attach = que_inter_dev_attach_ini,
    .queue_destroy = NULL,
    .queue_reset = NULL,
    .queue_en_queue = que_inter_dev_enque_ini,
    .queue_de_queue = queue_dequeue_local,
    .queue_subscribe = NULL,
    .queue_unsubscribe = NULL,
    .queue_sub_f_to_nf_event = NULL,
    .queue_unsub_f_to_nf_event = NULL,
    .queue_sub_event = que_inter_dev_sub_event_ini,
    .queue_unsub_event = queue_unsub_event_local,
    .queue_ctrl_event = NULL,
    .queue_query_info = que_inter_dev_query_info_ini,
    .queue_get_status = que_inter_dev_get_status_ini,
    .queue_get_qid_by_name = NULL,
    .queue_get_qids_by_pid = NULL,
    .queue_query = NULL,
    .queue_peek_data = queue_peek_data_local,
    .queue_set = queue_set_local,
    .queue_finish_cb = NULL,
    .queue_export = que_inter_dev_export_ini,
    .queue_unexport = que_inter_dev_unexport_ini,
    .queue_import = que_inter_dev_import_ini,
    .queue_unimport = que_inter_dev_unimport_ini,
};

static int __attribute__((constructor)) que_inter_dev_get_api(void)
{
    queue_set_comm_interface(INTER_DEV_QUEUE, &g_que_inter_dev_intf);
    return DRV_ERROR_NONE;
}
#else

void que_inter_dev_ini_test(void)
{}

#endif
