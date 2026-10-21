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

#include "kernel_version_adapt.h"
#include "esched_kernel_interface.h"

#include "trs_chan.h"
#include "trs_id.h"
#include "trs_ts_inst.h"
#include "trs_uk_msg.h"
#include "trs_cb_sqcq.h"

#define TRS_CB_MIN_CQE_SIZE 32
#define TRS_CB_MAX_CQE_SIZE 64
#define TRS_CB_MIN_CQ_DEPTH 32
#define TRS_CB_MAX_CQ_DEPTH 1024

#define TRS_CB_PHY_SQ_DEPTH 1024
#define TRS_CB_PHY_SQE_SIZE 64
#define TRS_CB_PHY_CQ_DEPTH 1024
#define TRS_CB_PHY_CQE_SIZE 64

static bool trs_cb_cqe_is_valid(void *cqe, u32 loop)
{
    struct trs_cb_cqe *cb_cqe = (struct trs_cb_cqe *)cqe;
    return (cb_cqe->phase == ((loop + 1) & 0x1));
}

static void trs_cb_get_sq_head_in_cqe(void *cqe, u32 *sq_head)
{
    struct trs_cb_cqe *cb_cqe = (struct trs_cb_cqe *)cqe;
    *sq_head = cb_cqe->sq_head;
}

extern int sched_submit_event_to_thread(u32 chip_id, struct sched_published_event *event);
static void trs_cb_send_event(u32 devid, struct sched_published_event *event)
{
    (void)sched_submit_event_to_thread(devid, event);
}

static void trs_cb_send_event_on_free(struct trs_core_ts_inst *ts_inst, u32 cqid)
{
    struct trs_cb_ctx *cb_ctx = &ts_inst->cb_ctx;
    struct trs_id_inst *inst = &ts_inst->inst;
    struct sched_published_event event;

    event.event_info.event_id = EVENT_TS_CALLBACK_MSG;
    event.event_info.pid = cb_ctx->cq[cqid].pid;
    event.event_info.gid = TRS_CB_EVENT_GRP_ID;
    event.event_info.tid = cb_ctx->cq[cqid].grpid;
    event.event_info.subevent_id = TRS_CB_HW_TIMEOUT_SUBEVENTID;
    event.event_info.dst_engine = CCPU_LOCAL;

    event.event_func.event_ack_func = NULL;
    event.event_func.event_finish_func = NULL;

    event.event_info.msg = NULL;
    event.event_info.msg_len = 0;

    trs_cb_send_event(inst->devid, &event);
}

static void trs_cb_cq_recv_proc(struct trs_core_ts_inst *ts_inst, u32 cqid, void *cqe)
{
    struct trs_cb_ctx *cb_ctx = &ts_inst->cb_ctx;
    struct trs_id_inst *inst = &ts_inst->inst;
    struct trs_cb_cqe *cb_cqe = (struct trs_cb_cqe *)cqe;
    u32 cqe_cqid = cb_cqe->cq_id;
    struct sched_published_event event;

    if (cb_ctx->phy_sqcq.cqid != cqid) {
        trs_err("Not cb phy cqid. (devid=%u; tsid=%u; cqid=%u; cb_cqid=%u)\n", inst->devid, inst->tsid, cqid,
                cb_ctx->phy_sqcq.cqid);
        return;
    }

    if ((cqe_cqid >= cb_ctx->cq_num) || (cb_ctx->cq[cqe_cqid].valid == 0)) {
        trs_err("Invalid cb cqid. (devid=%u; tsid=%u; cq_id=%u)\n", inst->devid, inst->tsid, cqe_cqid);
        return;
    }

    event.event_info.event_id = EVENT_TS_CALLBACK_MSG;
    event.event_info.pid = cb_ctx->cq[cqe_cqid].pid;
    event.event_info.gid = TRS_CB_EVENT_GRP_ID;
    event.event_info.tid = cb_ctx->cq[cqe_cqid].grpid;
    event.event_info.subevent_id = TRS_CB_SW_SUBEVENTID;
    event.event_info.dst_engine = CCPU_LOCAL;

    event.event_func.event_ack_func = NULL;
    event.event_func.event_finish_func = NULL;

    event.event_info.msg = (char *)cqe;
    event.event_info.msg_len = sizeof(struct trs_cb_cqe);

    trs_cb_send_event(inst->devid, &event);
}

static int trs_cb_cq_recv(struct trs_id_inst *inst, u32 cqid, void *cqe)
{
    struct trs_core_ts_inst *ts_inst = NULL;

    ts_inst = trs_core_ts_inst_get(inst);
    if (ts_inst == NULL) {
        trs_err("Invalid para. (devid=%u; tsid=%u; cqid=%u)\n", inst->devid, inst->tsid, cqid);
        return CQ_RECV_FINISH;
    }

    trs_cb_cq_recv_proc(ts_inst, cqid, cqe);
    trs_core_ts_inst_put(ts_inst);

    return CQ_RECV_FINISH;
}

static int trs_cb_alloc_phy_sqcq(struct trs_core_ts_inst *ts_inst)
{
    struct trs_cb_phy_sqcq *phy_sqcq = &ts_inst->cb_ctx.phy_sqcq;
    struct trs_id_inst *inst = &ts_inst->inst;
    struct trs_chan_sq_info sq_info;
    struct trs_chan_cq_info cq_info;
    struct trs_chan_para para = {0};
    int ret;

    para.types.type = CHAN_TYPE_TASK_SCHED;
    para.types.sub_type = CHAN_SUB_TYPE_TASK_SCHED_ASYNC_CB;

    para.sq_para.sqe_size = TRS_CB_PHY_SQE_SIZE;
    para.sq_para.sq_depth = TRS_CB_PHY_SQ_DEPTH;
    para.cq_para.cqe_size = TRS_CB_PHY_CQE_SIZE;
    para.cq_para.cq_depth = TRS_CB_PHY_CQ_DEPTH;
    para.ops.cqe_is_valid = trs_cb_cqe_is_valid;
    para.ops.abnormal_proc = NULL;
    para.ops.get_sq_head_in_cqe = trs_cb_get_sq_head_in_cqe;
    para.ops.cq_recv = trs_cb_cq_recv;

    para.flag = (0x1 << CHAN_FLAG_ALLOC_SQ_BIT) | (0x1 << CHAN_FLAG_ALLOC_CQ_BIT) | (0x1 << CHAN_FLAG_NOTICE_TS_BIT) |
                (0x1 << CHAN_FLAG_AUTO_UPDATE_SQ_HEAD_BIT);

    ret = hal_kernel_trs_chan_create(inst, &para, &phy_sqcq->chan_id);
    if (ret != 0) {
        trs_err("Alloc phy sqcq chan failed. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return ret;
    }

    (void)trs_chan_get_sq_info(inst, phy_sqcq->chan_id, &sq_info);
    (void)trs_chan_get_cq_info(inst, phy_sqcq->chan_id, &cq_info);

    phy_sqcq->sqid = sq_info.sqid;
    phy_sqcq->cqid = cq_info.cqid;
    phy_sqcq->cq_irq = cq_info.irq;

    return 0;
}

static void trs_cb_free_phy_sqcq(struct trs_core_ts_inst *ts_inst)
{
    hal_kernel_trs_chan_destroy(&ts_inst->inst, ts_inst->cb_ctx.phy_sqcq.chan_id);
}

static int _trs_cb_sqcq_init(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst,
                             struct halSqCqInputInfo *para)
{
    struct trs_cb_cq *cq = NULL;
    int ret;

    ret = trs_proc_add_res(proc_ctx, ts_inst, TRS_CB_SQ, para->sqId);
    if (ret != 0) {
        return -EINVAL;
    }

    ret = trs_proc_add_res(proc_ctx, ts_inst, TRS_CB_CQ, para->cqId);
    if (ret != 0) {
        (void)trs_proc_del_res(proc_ctx, ts_inst, TRS_CB_SQ, para->sqId);
        return -EINVAL;
    }

    cq = &ts_inst->cb_ctx.cq[para->cqId];
    cq->sqid = para->sqId;
    cq->grpid = para->grpId;
    cq->cq_depth = para->cqeDepth;
    cq->cqe_size = para->cqeSize;
    cq->cqid = para->cqId;
    cq->pid = proc_ctx->pid;
    cq->valid = 1;

    return 0;
}

static void _trs_cb_sqcq_uninit(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, u32 cqid)
{
    struct trs_cb_cq *cq = &ts_inst->cb_ctx.cq[cqid];

    (void)trs_proc_del_res(proc_ctx, ts_inst, TRS_CB_CQ, cqid);
    (void)trs_proc_del_res(proc_ctx, ts_inst, TRS_CB_SQ, cq->sqid);
    cq->valid = 0;
}

static int trs_cb_sqcq_notice_ts(struct trs_core_ts_inst *ts_inst, u32 cmd_type, u32 cqid)
{
    struct trs_cb_phy_sqcq *phy_sqcq = &ts_inst->cb_ctx.phy_sqcq;
    struct trs_cb_cq *cq = &ts_inst->cb_ctx.cq[cqid];
    struct trs_cb_cq_mbox msg;

    trs_mbox_init_header(&msg.header, cmd_type);
    msg.vpid = cq->pid;
    msg.grpid = cq->grpid;
    msg.logic_cqid = cqid;
    msg.phy_cqid = phy_sqcq->cqid;
    msg.phy_sqid = phy_sqcq->sqid;
    msg.cq_irq = phy_sqcq->cq_irq;

    /* adapt fill: plat_type */

    return trs_core_notice_ts(ts_inst, (u8 *)&msg, sizeof(msg));
}

static int trs_cb_sqcq_alloc_para_check(struct trs_core_ts_inst *ts_inst, struct halSqCqInputInfo *para)
{
    struct trs_id_inst *inst = &ts_inst->inst;

    if ((para->cqeDepth > TRS_CB_MAX_CQ_DEPTH) || (para->cqeDepth < TRS_CB_MIN_CQ_DEPTH) ||
        (para->cqeSize > TRS_CB_MAX_CQE_SIZE) || (para->cqeSize < TRS_CB_MIN_CQE_SIZE) ||
        (para->sqeSize != TRS_CB_PHY_SQE_SIZE) || (para->grpId >= TRS_CB_GROUP_NUM)) {
        trs_err("Invalid para. (devid=%u; tsid=%u; cqeDepth=%u; cqeSize=%u; sqeSize=%u; grpId=%u)\n", inst->devid,
                inst->tsid, para->cqeDepth, para->cqeSize, para->sqeSize, para->grpId);
        return -EINVAL;
    }

    return 0;
}

static int trs_cb_sqcq_id_alloc(struct trs_id_inst *inst, u32 *sq_id, u32 *cq_id)
{
    int ret;

    ret = trs_id_alloc(inst, TRS_CB_SQ_ID, sq_id);
    if (ret != 0) {
        trs_err("Alloc cb sq failed. (devid=%u; tsid=%u; ret=%d)\n", inst->devid, inst->tsid, ret);
        return ret;
    }

    ret = trs_id_alloc(inst, TRS_CB_CQ_ID, cq_id);
    if (ret != 0) {
        (void)trs_id_free(inst, TRS_CB_SQ_ID, *sq_id);
        trs_err("Alloc cb cq failed. (devid=%u; tsid=%u; ret=%d)\n", inst->devid, inst->tsid, ret);
        return ret;
    }

    return 0;
}

static void trs_cb_sqcq_id_free(struct trs_id_inst *inst, u32 sq_id, u32 cq_id)
{
    (void)trs_id_free(inst, TRS_CB_CQ_ID, cq_id);
    (void)trs_id_free(inst, TRS_CB_SQ_ID, sq_id);
}

int trs_cb_sqcq_alloc(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct halSqCqInputInfo *para)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    int ret;

    ret = trs_cb_sqcq_alloc_para_check(ts_inst, para);
    if (ret != 0) {
        return ret;
    }

    ret = trs_cb_sqcq_id_alloc(inst, &para->sqId, &para->cqId);
    if (ret != 0) {
        return ret;
    }

    ret = _trs_cb_sqcq_init(proc_ctx, ts_inst, para);
    if (ret != 0) {
        trs_cb_sqcq_id_free(inst, para->sqId, para->cqId);
        return ret;
    }

    if (!trs_is_stars_inst(ts_inst)) {
        if (ts_inst->cb_ctx.phy_sqcq.chan_id == -1) {
            ret = trs_cb_alloc_phy_sqcq(ts_inst);
            if (ret != 0) {
                _trs_cb_sqcq_uninit(proc_ctx, ts_inst, para->cqId);
                trs_cb_sqcq_id_free(inst, para->sqId, para->cqId);
                return ret;
            }
        }

        ret = trs_cb_sqcq_notice_ts(ts_inst, TRS_MBOX_CREATE_CB_CQ, para->cqId);
        if (ret != 0) {
            _trs_cb_sqcq_uninit(proc_ctx, ts_inst, para->cqId);
            trs_cb_sqcq_id_free(inst, para->sqId, para->cqId);
            trs_err("Notice ts failed. (devid=%u; tsid=%u; cqId=%u)\n", inst->devid, inst->tsid, para->cqId);
            return ret;
        }
    }

    trs_debug("Alloc success. (devid=%u; tsid=%u; sqId=%u; cqId=%u;)\n", inst->devid, inst->tsid, para->sqId,
              para->cqId);

    return 0;
}

static int _trs_cb_sqcq_free(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, u32 sq_id, u32 cq_id)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    struct trs_cb_cq *cq = NULL;
    int ret;

    if (!trs_proc_has_res(proc_ctx, ts_inst, TRS_CB_CQ, cq_id)) {
        trs_err("Invalid para. (devid=%u; tsid=%u; cq_id=%u)\n", inst->devid, inst->tsid, cq_id);
        return -EINVAL;
    }

    cq = &ts_inst->cb_ctx.cq[cq_id];
    if (cq->sqid != sq_id) {
        trs_err("Invalid para. (devid=%u; tsid=%u; sq_id=%u)\n", inst->devid, inst->tsid, sq_id);
        return -EINVAL;
    }

    if (proc_ctx->status != TRS_PROC_STATUS_EXIT) {
        trs_cb_send_event_on_free(ts_inst, cq_id);
    }

    if (!trs_is_stars_inst(ts_inst)) {
        ret = trs_cb_sqcq_notice_ts(ts_inst, TRS_MBOX_RELEASE_CB_CQ, cq_id);
        if (ret != 0) {
            trs_warn("Notice ts warn. (devid=%u; tsid=%u; cq_id=%u)\n", inst->devid, inst->tsid, cq_id);
        }
    }

    _trs_cb_sqcq_uninit(proc_ctx, ts_inst, cq_id);
    trs_cb_sqcq_id_free(inst, sq_id, cq_id);

    trs_debug("Free success. (devid=%u; tsid=%u; sq_id=%u; cq_id=%u)\n", inst->devid, inst->tsid, sq_id, cq_id);

    return 0;
}

int trs_cb_sqcq_free(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct halSqCqFreeInfo *para)
{
    return _trs_cb_sqcq_free(proc_ctx, ts_inst, para->sqId, para->cqId);
}

void trs_cb_sqcq_recycle(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, int res_type, u32 res_id)
{
    struct trs_id_inst *inst = &ts_inst->inst;

    if (res_type == TRS_CB_CQ) {
        struct trs_cb_cq *cq = &ts_inst->cb_ctx.cq[res_id];
        (void)_trs_cb_sqcq_free(proc_ctx, ts_inst, cq->sqid, res_id);
    } else {
        trs_err("Unexpected. (devid=%u; tsid=%u; sqId=%u)\n", inst->devid, inst->tsid, res_id);
    }
}

int trs_cb_sqcq_send(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct halTaskSendInfo *para)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    struct trs_chan_send_para send_para;
    int chan_id = ts_inst->cb_ctx.phy_sqcq.chan_id;

    if (chan_id < 0) {
        trs_err("Not alloc chan. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return -EINVAL;
    }

    send_para.sqe = para->sqe_addr;
    send_para.sqe_num = para->sqe_num;
    send_para.timeout = para->timeout;

    return trs_chan_send(inst, chan_id, &send_para);
}

int trs_cb_sqcq_init(struct trs_core_ts_inst *ts_inst)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    struct trs_cb_ctx *cb_ctx = &ts_inst->cb_ctx;
    u32 id_num = trs_res_get_id_num(ts_inst, TRS_CB_CQ);
    u32 max_id = trs_res_get_max_id(ts_inst, TRS_CB_CQ);

    trs_debug("Init cb cq. (devid=%u; tsid=%u; id_num=%u; max_id=%u)\n", inst->devid, inst->tsid, id_num, max_id);

    if (max_id > 0) {
        cb_ctx->cq = (struct trs_cb_cq *)trs_vzalloc(sizeof(struct trs_cb_cq) * max_id);
        if (cb_ctx->cq == NULL) {
            trs_err("Mem alloc failed. (devid=%u; tsid=%u; size=%lx)\n", inst->devid, inst->tsid,
                    sizeof(struct trs_cb_cq) * max_id);
            return -ENOMEM;
        }
    }

    cb_ctx->cq_num = max_id;
    cb_ctx->phy_sqcq.chan_id = -1;

    return 0;
}

void trs_cb_sqcq_uninit(struct trs_core_ts_inst *ts_inst)
{
    struct trs_cb_ctx *cb_ctx = &ts_inst->cb_ctx;

    if (cb_ctx->phy_sqcq.chan_id >= 0) {
        trs_cb_free_phy_sqcq(ts_inst);
    }

    if (cb_ctx->cq != NULL) {
        trs_vfree(cb_ctx->cq);
        cb_ctx->cq = NULL;
    }
}
