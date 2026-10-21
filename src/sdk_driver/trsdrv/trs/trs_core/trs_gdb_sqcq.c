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

#include "trs_ts_inst.h"
#include "trs_ioctl.h"
#include "trs_gdb_sqcq.h"

struct trs_gdb_cqe {
    u8 phase;
    u8 res1[3];
    u16 sq_index;
    u16 sq_head;
    u32 cmd_type;
    u32 return_val;
    u32 msg_id;
    u8 res2[44];
};

static bool trs_gdb_cqe_is_valid(void *cqe, u32 loop)
{
#ifndef EMU_ST
    struct trs_gdb_cqe *gdb_cqe = (struct trs_gdb_cqe *)cqe;

    return (gdb_cqe->phase == ((loop + 1) & 0x1));
#endif
}

static int trs_gdb_cq_recv(struct trs_id_inst *inst, u32 cqid, void *cqe)
{
#ifndef EMU_ST
    struct trs_core_ts_inst *ts_inst = NULL;

    ts_inst = trs_core_ts_inst_get(inst);
    if (ts_inst == NULL) {
        trs_err("Invalid para. (devid=%u; tsid=%u; cqid=%u)\n", inst->devid, inst->tsid, cqid);
        return CQ_RECV_FINISH;
    }

    trs_core_ts_inst_put(ts_inst);

    return CQ_RECV_CONTINUE;
#endif
}

static void trs_gdb_get_sq_head_in_cqe(void *cqe, u32 *sq_head)
{
#ifndef EMU_ST
    struct trs_gdb_cqe *gdb_cqe = (struct trs_gdb_cqe *)cqe;

    *sq_head = gdb_cqe->sq_head;
#endif
}

int trs_gdb_sqcq_send(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct halTaskSendInfo *para)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    struct trs_chan_send_para send_para;

    if (!trs_proc_has_res(proc_ctx, ts_inst, TRS_MAINT_SQ, para->sqId)) {
#ifndef EMU_ST
        trs_err("Not proc owner sq. (devid=%u; tsid=%u; sqId=%u)\n", inst->devid, inst->tsid, para->sqId);
        return -EINVAL;
#endif
    }

    send_para.sqe = para->sqe_addr;
    send_para.sqe_num = para->sqe_num;
    send_para.timeout = para->timeout;

    return trs_chan_send(inst, ts_inst->maint_sq_ctx[para->sqId].chan_id, &send_para);
}

int trs_gdb_sqcq_recv(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct halReportRecvInfo *para)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    struct trs_chan_recv_para recv_para;
    int ret;

    if (!trs_proc_has_res(proc_ctx, ts_inst, TRS_MAINT_CQ, para->cqId)) {
#ifndef EMU_ST
        trs_err("Not proc owner cq. (devid=%u; tsid=%u; cqId=%u)\n", inst->devid, inst->tsid, para->cqId);
        return -EINVAL;
#endif
    }

    recv_para.cqe = para->cqe_addr;
    recv_para.cqe_num = para->cqe_num;
    recv_para.timeout = para->timeout;

    ret = trs_chan_recv(inst, ts_inst->maint_cq_ctx[para->cqId].chan_id, &recv_para);
    if (ret != 0) {
        return ret;
    }

    para->report_cqe_num = recv_para.recv_cqe_num;

    return 0;
}

static int trs_gdb_alloc_chan(struct trs_proc_ctx *proc_ctx, struct trs_id_inst *inst, struct halSqCqInputInfo *para,
                              int *chan_id)
{
    struct trs_chan_para chan_para = {0};
    int ret;

    chan_para.types.type = CHAN_TYPE_MAINT;
    chan_para.types.sub_type = CHAN_SUB_TYPE_MAINT_DBG;

    chan_para.flag = (0x1 << CHAN_FLAG_ALLOC_SQ_BIT) | (0x1 << CHAN_FLAG_ALLOC_CQ_BIT) |
                     (0x1 << CHAN_FLAG_NOTICE_TS_BIT) | (0x1 << CHAN_FLAG_AUTO_UPDATE_SQ_HEAD_BIT) |
                     (0x1 << CHAN_FLAG_RECV_BLOCK_BIT);

    chan_para.sq_para.sqe_size = para->sqeSize;
    chan_para.sq_para.sq_depth = para->sqeDepth;

    chan_para.cq_para.cqe_size = para->cqeSize;
    chan_para.cq_para.cq_depth = para->cqeDepth;
    chan_para.ops.cqe_is_valid = trs_gdb_cqe_is_valid;
    chan_para.ops.abnormal_proc = NULL;
    chan_para.ops.get_sq_head_in_cqe = trs_gdb_get_sq_head_in_cqe;
    chan_para.ops.cq_recv = trs_gdb_cq_recv;

    ret = memcpy_s(chan_para.msg, sizeof(chan_para.msg), para->info, sizeof(para->info));
    if (ret != EOK) {
        trs_err("Memcopy failed. (dest_len=%lx; src_len=%lx)\n", sizeof(chan_para.msg), sizeof(para->info));
        return -EFAULT;
    }

    return hal_kernel_trs_chan_create(inst, &chan_para, chan_id);
}

static void trs_gdb_destroy_chan(struct trs_id_inst *inst, int chan_id)
{
    return hal_kernel_trs_chan_destroy(inst, chan_id);
}

int trs_gdb_sqcq_alloc(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct halSqCqInputInfo *para)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    struct trs_chan_sq_info sq_info;
    struct trs_chan_cq_info cq_info;
    int chan_id, ret;

    ret = trs_gdb_alloc_chan(proc_ctx, &ts_inst->inst, para, &chan_id);
    if (ret != 0) {
        trs_err("Alloc chan failed. (ret=%d; devid=%u; tsid=%u; type=%d)\n", ret, inst->devid, inst->tsid, para->type);
        return ret;
    }

    (void)trs_chan_get_sq_info(inst, chan_id, &sq_info);
    (void)trs_chan_get_cq_info(inst, chan_id, &cq_info);

    ret = trs_proc_add_res(proc_ctx, ts_inst, TRS_MAINT_SQ, sq_info.sqid);
    if (ret != 0) {
        goto destroy_chan;
    }

    ret = trs_proc_add_res(proc_ctx, ts_inst, TRS_MAINT_CQ, cq_info.cqid);
    if (ret != 0) {
        goto del_sq_res;
    }

    para->sqId = sq_info.sqid;
    para->cqId = cq_info.cqid;
    para->flag = 0;

    trs_sq_ctx_init(inst, &ts_inst->maint_sq_ctx[sq_info.sqid], para, KA_U32_MAX, chan_id);

    trs_cq_ctx_init(&ts_inst->maint_cq_ctx[cq_info.cqid], KA_U32_MAX, chan_id);

    trs_set_sq_status(&ts_inst->maint_sq_ctx[sq_info.sqid], 1);

    trs_debug("Alloc maint sqcq. (devid=%u; tsid=%u; sqId=%u; cqId=%u)\n", inst->devid, inst->tsid, para->sqId,
              para->cqId);

    return 0;

del_sq_res:
    (void)trs_proc_del_res(proc_ctx, ts_inst, TRS_MAINT_SQ, sq_info.sqid);

destroy_chan:
    trs_gdb_destroy_chan(inst, chan_id);
    trs_err("Maint sqcq Alloc failed. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
    return ret;
}

static int trs_gdb_sqcq_free_check(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst,
                                   struct halSqCqFreeInfo *para)
{
    struct trs_id_inst *inst = &ts_inst->inst;

    if (!trs_proc_has_res(proc_ctx, ts_inst, TRS_MAINT_SQ, para->sqId)) {
        trs_err("Not proc sq. (devid=%u; tsid=%u; sq=%u; cq=%u)\n", inst->devid, inst->tsid, para->sqId, para->cqId);
        return -EINVAL;
    }

    if (!trs_proc_has_res(proc_ctx, ts_inst, TRS_MAINT_CQ, para->cqId)) {
        trs_err("Not proc cq. (devid=%u; tsid=%u; sq=%u; cq=%u)\n", inst->devid, inst->tsid, para->sqId, para->cqId);
        return -EINVAL;
    }

    if (ts_inst->maint_sq_ctx[para->sqId].chan_id != ts_inst->maint_cq_ctx[para->cqId].chan_id) {
        trs_err("Not pair sqcq. (devid=%u; tsid=%u; sqId=%u; cqId=%u)\n", inst->devid, inst->tsid, para->sqId,
                para->cqId);
        return -EINVAL;
    }

    return 0;
}

static void _trs_gdb_sqcq_free(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, u32 sqid, u32 cqid)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    struct trs_sq_ctx *sq_ctx = &ts_inst->maint_sq_ctx[sqid];
    struct trs_cq_ctx *cq_ctx = &ts_inst->maint_cq_ctx[cqid];
    int chan_id = sq_ctx->chan_id;

    trs_sq_ctx_uninit(sq_ctx);
    trs_cq_ctx_uninit(cq_ctx);

    (void)trs_proc_del_res(proc_ctx, ts_inst, TRS_MAINT_SQ, sqid);
    (void)trs_proc_del_res(proc_ctx, ts_inst, TRS_MAINT_CQ, cqid);
    trs_gdb_destroy_chan(inst, chan_id);
}

int trs_gdb_sqcq_free(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct halSqCqFreeInfo *para)
{
    int ret;

    ret = trs_gdb_sqcq_free_check(proc_ctx, ts_inst, para);
    if (ret != 0) {
        return ret;
    }

    trs_set_sq_status(&ts_inst->maint_sq_ctx[para->sqId], 0);
    _trs_gdb_sqcq_free(proc_ctx, ts_inst, para->sqId, para->cqId);

    return 0;
}

void trs_maint_sqcq_recycle(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, int res_type, u32 res_id)
{
#ifndef EMU_ST
    struct trs_id_inst *inst = &ts_inst->inst;

    if (res_type == TRS_MAINT_SQ) {
        _trs_gdb_sqcq_free(proc_ctx, ts_inst, res_id, ts_inst->maint_sq_ctx[res_id].cqid);
    } else {
        trs_err("Unexpected. (devid=%u; tsid=%u; cqId=%u)\n", inst->devid, inst->tsid, res_id);
    }
#endif
}

int trs_maint_sqcq_init(struct trs_core_ts_inst *ts_inst)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    u32 sq_id_num = trs_res_get_id_num(ts_inst, TRS_MAINT_SQ);
    u32 sq_max_id = trs_res_get_max_id(ts_inst, TRS_MAINT_SQ);
    u32 cq_id_num = trs_res_get_id_num(ts_inst, TRS_MAINT_CQ);
    u32 cq_max_id = trs_res_get_max_id(ts_inst, TRS_MAINT_CQ);

    trs_debug("Init maint sqcq. (devid=%u; tsid=%u; sq_id_num=%u; sq_max_id=%u; cq_id_num=%u; cq_max_id=%u)\n",
              inst->devid, inst->tsid, sq_id_num, sq_max_id, cq_id_num, cq_max_id);

    if (sq_max_id > 0) {
        ts_inst->maint_sq_ctx = (struct trs_sq_ctx *)trs_vzalloc(sizeof(struct trs_sq_ctx) * sq_max_id);
        if (ts_inst->maint_sq_ctx == NULL) {
#ifndef EMU_ST
            trs_err("Mem alloc failed. (devid=%u; tsid=%u; size=0x%lx)\n", inst->devid, inst->tsid,
                    sizeof(struct trs_sq_ctx) * sq_max_id);
            return -ENOMEM;
#endif
        }
        ts_inst->maint_sq_ctx->sq_num = sq_max_id;
        trs_sq_ctxs_init(ts_inst->maint_sq_ctx, sq_max_id);
    }

    if (cq_max_id > 0) {
        ts_inst->maint_cq_ctx = (struct trs_cq_ctx *)trs_vzalloc(sizeof(struct trs_cq_ctx) * cq_max_id);
        if (ts_inst->maint_cq_ctx == NULL) {
#ifndef EMU_ST
            if (ts_inst->maint_sq_ctx != NULL) {
                trs_sq_ctxs_uninit(ts_inst->maint_sq_ctx);
                trs_vfree(ts_inst->maint_sq_ctx);
                ts_inst->maint_sq_ctx = NULL;
            }
            trs_err("Mem alloc failed. (devid=%u; tsid=%u; size=%lx)\n", inst->devid, inst->tsid,
                    sizeof(struct trs_cq_ctx) * cq_max_id);
            return -ENOMEM;
#endif
        }
    }

    return 0;
}

void trs_maint_sqcq_uninit(struct trs_core_ts_inst *ts_inst)
{
    if (ts_inst->maint_sq_ctx != NULL) {
        trs_sq_ctxs_uninit(ts_inst->maint_sq_ctx);
        trs_vfree(ts_inst->maint_sq_ctx);
        ts_inst->maint_sq_ctx = NULL;
    }
    if (ts_inst->maint_cq_ctx != NULL) {
        trs_vfree(ts_inst->maint_cq_ctx);
        ts_inst->maint_cq_ctx = NULL;
    }
}
