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
#include "securec.h"

#include "trs_ts_inst.h"
#include "trs_sqcq_map.h"
#include "trs_ioctl.h"
#include "trs_sw_sqcq.h"

struct trs_ctrl_cqe {
    u16 phase : 1;
    u16 sq_id : 11;
    u16 match_flag : 1;
    u16 resv0 : 3;
    u16 sq_head;
    u16 stream_id;
    u16 task_id;
    u32 err_code;
    u32 resv1;
};

#ifndef EMU_ST
static bool trs_ctrl_cqe_is_valid(void *cqe, u32 loop)
{
    struct trs_ctrl_cqe *ctrl_cqe = (struct trs_ctrl_cqe *)cqe;
    return (ctrl_cqe->phase == ((loop + 1) & 0x1));
}

static void trs_ctrl_cqe_to_logic_cqe(struct trs_ctrl_cqe *ctrl_cqe, struct trs_logic_cqe *logic_cqe)
{
    logic_cqe->match_flag = ctrl_cqe->match_flag;
    logic_cqe->stream_id = ctrl_cqe->stream_id;
    logic_cqe->task_id = ctrl_cqe->task_id;
    logic_cqe->sq_id = ctrl_cqe->sq_id;
    logic_cqe->sq_head = ctrl_cqe->sq_head;
    logic_cqe->error_code = ctrl_cqe->err_code;
}

static void trs_sw_cq_recv_proc(struct trs_core_ts_inst *ts_inst, u32 cqid, void *cqe)
{
    struct trs_cq_ctx *cq_ctx = &ts_inst->sw_cq_ctx[cqid];
    struct trs_ctrl_cqe *ctrl_cqe = (struct trs_ctrl_cqe *)cqe;
    struct trs_logic_cqe logic_cqe = {0};

    trs_ctrl_cqe_to_logic_cqe(ctrl_cqe, &logic_cqe);

    (void)trs_logic_cq_enque(ts_inst, cq_ctx->logic_cqid, (u32)ctrl_cqe->stream_id, (u32)ctrl_cqe->task_id,
                             (void *)&logic_cqe);
}

static int trs_sw_cq_recv(struct trs_id_inst *inst, u32 cqid, void *cqe)
{
    struct trs_core_ts_inst *ts_inst = NULL;

    ts_inst = trs_core_ts_inst_get(inst);
    if (ts_inst == NULL) {
        trs_err("Invalid para. (devid=%u; tsid=%u; cqid=%u)\n", inst->devid, inst->tsid, cqid);
        return CQ_RECV_FINISH;
    }

    trs_sw_cq_recv_proc(ts_inst, cqid, cqe);
    trs_core_ts_inst_put(ts_inst);

    return CQ_RECV_CONTINUE;
}

static int trs_sw_alloc_chan(struct trs_proc_ctx *proc_ctx, struct trs_id_inst *inst, struct halSqCqInputInfo *para,
                             int *chan_id)
{
    struct trs_chan_para chan_para = {0};
    int ret;

    chan_para.types.type = CHAN_TYPE_SW;
    chan_para.types.sub_type = CHAN_SUB_TYPE_SW_CTRL;

    chan_para.flag = (0x1 << CHAN_FLAG_ALLOC_SQ_BIT) | (0x1 << CHAN_FLAG_ALLOC_CQ_BIT) |
                     (0x1 << CHAN_FLAG_NOTICE_TS_BIT);

    chan_para.ssid = proc_ctx->cp_ssid;
    chan_para.sq_para.sqe_size = para->sqeSize;
    chan_para.sq_para.sq_depth = para->sqeDepth;

    chan_para.cq_para.cqe_size = para->cqeSize;
    chan_para.cq_para.cq_depth = para->cqeDepth;
    chan_para.ops.cqe_is_valid = trs_ctrl_cqe_is_valid;
    chan_para.ops.abnormal_proc = NULL;
    chan_para.ops.get_sq_head_in_cqe = NULL;
    chan_para.ops.cq_recv = trs_sw_cq_recv;

    ret = memcpy_s(chan_para.msg, sizeof(chan_para.msg), para->info, sizeof(para->info));
    if (ret != 0) {
        trs_err("Memcopy failed. (dest_len=%lx; src_len=%lx)\n", sizeof(chan_para.msg), sizeof(para->info));
        return ret;
    }

    return hal_kernel_trs_chan_create(inst, &chan_para, chan_id);
}
#endif

int trs_sw_sqcq_alloc(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct halSqCqInputInfo *para)
{
#ifndef EMU_ST
    struct trs_id_inst *inst = &ts_inst->inst;
    struct trs_chan_sq_info sq_info;
    struct trs_chan_cq_info cq_info;
    u32 logic_cqid = para->info[0];
    int chan_id, ret;

    if (!trs_proc_has_res(proc_ctx, ts_inst, TRS_LOGIC_CQ, logic_cqid)) {
        trs_err("No logic. (devid=%u; tsid=%u; logic_cqid=%u)\n", ts_inst->inst.devid, ts_inst->inst.tsid, logic_cqid);
        return -EINVAL;
    }

    if (trs_is_proc_res_limited(proc_ctx, ts_inst, TRS_SW_SQ)) {
        return -ENOSPC;
    }

    if (para->sqeDepth < TRS_CTRL_USE_SQE_NUM) {
        trs_err("Invalid sqeDepth. (sqeDepth=%u)\n", para->sqeDepth);
        return -EINVAL;
    }

    ret = trs_sw_alloc_chan(proc_ctx, &ts_inst->inst, para, &chan_id);
    if (ret != 0) {
        trs_err("Alloc chan failed. (devid=%u; tsid=%u; type=%d)\n", inst->devid, inst->tsid, para->type);
        return ret;
    }

    (void)trs_chan_get_sq_info(inst, chan_id, &sq_info);
    (void)trs_chan_get_cq_info(inst, chan_id, &cq_info);

    ret = trs_proc_add_res(proc_ctx, ts_inst, TRS_SW_SQ, sq_info.sqid);
    if (ret != 0) {
        goto destroy_chan;
    }

    ret = trs_proc_add_res(proc_ctx, ts_inst, TRS_SW_CQ, cq_info.cqid);
    if (ret != 0) {
        goto del_sq_res;
    }

    para->sqId = sq_info.sqid;
    para->cqId = cq_info.cqid;

    trs_sq_ctx_init(inst, &ts_inst->sw_sq_ctx[sq_info.sqid], para, KA_U32_MAX, chan_id);
    trs_cq_ctx_init(&ts_inst->sw_cq_ctx[cq_info.cqid], logic_cqid, chan_id);

    trs_logic_set_cqe_version(ts_inst, logic_cqid, LOGIC_CQE_VERSION_V2);

    /* ctrl is sw sqcq, not has head and tail reg addr, so we use last 2 cqe as head and tail addr */
    sq_info.head_addr = sq_info.sq_phy_addr + para->sqeSize * (para->sqeDepth - TRS_CTRL_USE_SQE_NUM);
    sq_info.tail_addr = sq_info.head_addr + para->sqeSize;
    ret = trs_sq_remap(proc_ctx, ts_inst, para, &ts_inst->sw_sq_ctx[sq_info.sqid], &sq_info);
    if (ret != 0) {
        trs_err("Sq remap failed. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
#ifndef EMU_ST
        trs_cq_ctx_uninit(&ts_inst->sw_cq_ctx[cq_info.cqid]);
        trs_sq_ctx_uninit(&ts_inst->sw_sq_ctx[sq_info.sqid]);
        goto del_cq_res;
#endif
    }

    trs_set_sq_status(&ts_inst->sw_sq_ctx[sq_info.sqid], 1);

    trs_info("Alloc sqcq. (devid=%u; tsid=%u; sqId=%u; cqId=%u)\n", inst->devid, inst->tsid, para->sqId, para->cqId);
#endif
    return 0;
#ifndef EMU_ST
del_cq_res:
    (void)trs_proc_del_res(proc_ctx, ts_inst, TRS_SW_CQ, cq_info.cqid);

del_sq_res:
    (void)trs_proc_del_res(proc_ctx, ts_inst, TRS_SW_SQ, sq_info.sqid);

destroy_chan:
    hal_kernel_trs_chan_destroy(inst, chan_id);
    trs_err("Sqcq Alloc failed. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
    return ret;
#endif
}

#ifndef EMU_ST
static int trs_sw_sqcq_free_check(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst,
                                  struct halSqCqFreeInfo *para)
{
    struct trs_id_inst *inst = &ts_inst->inst;

    if (!trs_proc_has_res(proc_ctx, ts_inst, TRS_SW_SQ, para->sqId)) {
        trs_err("Not proc sq. (devid=%u; tsid=%u; sq=%u; cq=%u)\n", inst->devid, inst->tsid, para->sqId, para->cqId);
        return -EINVAL;
    }

    if (!trs_proc_has_res(proc_ctx, ts_inst, TRS_SW_CQ, para->cqId)) {
        trs_err("Not proc cq. (devid=%u; tsid=%u; sq=%u; cq=%u)\n", inst->devid, inst->tsid, para->sqId, para->cqId);
        return -EINVAL;
    }

    if (ts_inst->sw_sq_ctx[para->sqId].chan_id != ts_inst->sw_cq_ctx[para->cqId].chan_id) {
        trs_err("Not pair sqcq. (devid=%u; tsid=%u; sqId=%u; cqId=%u)\n", inst->devid, inst->tsid, para->sqId,
                para->cqId);
        return -EINVAL;
    }

    return 0;
}

static void _trs_sw_sqcq_free(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, u32 sqid, u32 cqid)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    struct trs_sq_ctx *sq_ctx = &ts_inst->sw_sq_ctx[sqid];
    struct trs_cq_ctx *cq_ctx = &ts_inst->sw_cq_ctx[cqid];
    int chan_id = sq_ctx->chan_id;

    trs_info("Free sqcq. (devid=%u; tsid=%u; sqId=%u; cqId=%u)\n", inst->devid, inst->tsid, sqid, cqid);

    trs_sq_ctx_uninit(sq_ctx);
    trs_cq_ctx_uninit(cq_ctx);
    (void)trs_proc_del_res(proc_ctx, ts_inst, TRS_SW_SQ, sqid);
    (void)trs_proc_del_res(proc_ctx, ts_inst, TRS_SW_CQ, cqid);
    hal_kernel_trs_chan_destroy(inst, chan_id);
}
#endif

int trs_sw_sqcq_free(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct halSqCqFreeInfo *para)
{
#ifndef EMU_ST
    int ret = trs_sw_sqcq_free_check(proc_ctx, ts_inst, para);
    if (ret != 0) {
        return ret;
    }

    trs_set_sq_status(&ts_inst->sw_sq_ctx[para->sqId], 0);
    trs_sq_unmap(proc_ctx, ts_inst, &ts_inst->sw_sq_ctx[para->sqId]);
    _trs_sw_sqcq_free(proc_ctx, ts_inst, para->sqId, para->cqId);
#endif
    return 0;
}

void trs_sw_sqcq_recycle(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, int res_type, u32 res_id)
{
#ifndef EMU_ST
    struct trs_id_inst *inst = &ts_inst->inst;

    if (res_type == TRS_SW_SQ) {
        (void)trs_chan_ctrl(inst, ts_inst->sw_sq_ctx[res_id].chan_id, CHAN_CTRL_CMD_NOT_NOTICE_TS, 0);
        _trs_sw_sqcq_free(proc_ctx, ts_inst, res_id, ts_inst->sw_sq_ctx[res_id].cqid);
    } else {
        trs_err("Unexpected. (devid=%u; tsid=%u; cqId=%u)\n", inst->devid, inst->tsid, res_id);
    }
#endif
}

int trs_sw_sqcq_init(struct trs_core_ts_inst *ts_inst)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    u32 sq_id_num = trs_res_get_id_num(ts_inst, TRS_SW_SQ);
    u32 sq_max_id = trs_res_get_max_id(ts_inst, TRS_SW_SQ);
    u32 cq_id_num = trs_res_get_id_num(ts_inst, TRS_SW_CQ);
    u32 cq_max_id = trs_res_get_max_id(ts_inst, TRS_SW_CQ);

    trs_debug("Init sw sqcq. (devid=%u; tsid=%u; sq_id_num=%u; sq_max_id=%u; cq_id_num=%u; cq_max_id=%u)\n",
              inst->devid, inst->tsid, sq_id_num, sq_max_id, cq_id_num, cq_max_id);

    if (sq_max_id > 0) {
        ts_inst->sw_sq_ctx = (struct trs_sq_ctx *)trs_vzalloc(sizeof(struct trs_sq_ctx) * sq_max_id);
        if (ts_inst->sw_sq_ctx == NULL) {
            trs_err("Mem alloc failed. (devid=%u; tsid=%u; size=%lx)\n", inst->devid, inst->tsid,
                    sizeof(struct trs_sq_ctx) * sq_max_id);
            return -ENOMEM;
        }
        ts_inst->sw_sq_ctx->sq_num = sq_max_id;
        trs_sq_ctxs_init(ts_inst->sw_sq_ctx, sq_max_id);
    }

    if (cq_max_id > 0) {
        ts_inst->sw_cq_ctx = (struct trs_cq_ctx *)trs_vzalloc(sizeof(struct trs_cq_ctx) * cq_max_id);
        if (ts_inst->sw_cq_ctx == NULL) {
            trs_sq_ctxs_uninit(ts_inst->sw_sq_ctx);
            trs_vfree(ts_inst->sw_sq_ctx);
            ts_inst->sw_sq_ctx = NULL;
            trs_err("Mem alloc failed. (devid=%u; tsid=%u; size=%lx)\n", inst->devid, inst->tsid,
                    sizeof(struct trs_cq_ctx) * cq_max_id);
            return -ENOMEM;
        }
    }

    return 0;
}

void trs_sw_sqcq_uninit(struct trs_core_ts_inst *ts_inst)
{
    if (ts_inst->sw_sq_ctx != NULL) {
        trs_sq_ctxs_uninit(ts_inst->sw_sq_ctx);
        trs_vfree(ts_inst->sw_sq_ctx);
        ts_inst->sw_sq_ctx = NULL;
    }
    if (ts_inst->sw_cq_ctx != NULL) {
        trs_vfree(ts_inst->sw_cq_ctx);
        ts_inst->sw_cq_ctx = NULL;
    }
}
