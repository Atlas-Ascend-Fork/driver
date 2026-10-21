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
#include "ka_task_pub.h"
#include "ka_memory_pub.h"

#include "securec.h"

#include "trs_chan.h"
#include "trs_ts_inst.h"
#include "trs_proc.h"
#include "trs_shm_sqcq.h"

#define TRS_SHM_SQE_SIZE 64
#define TRS_SHM_CQE_SIZE 16
#define TRS_SHM_SQ_DEPTH 1024
#define TRS_SHM_CQ_DEPTH 1024

struct trs_shm_cqe {
    u16 phase : 1;
    u16 report_type : 15;
    u16 sqid;
    u16 sq_head;
    u16 reserved0;
    u64 reserved;
};

static bool trs_shm_cqe_is_valid(void *cqe, u32 loop)
{
    struct trs_shm_cqe *shm_cqe = (struct trs_shm_cqe *)cqe;
    return (shm_cqe->phase == ((loop + 1) & 0x1));
}

static void trs_shm_cq_recv_proc(struct trs_core_ts_inst *ts_inst, u32 cqid, void *cqe)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    struct trs_shm_cqe *shm_cqe = (struct trs_shm_cqe *)cqe;
    u32 cqe_sqid = shm_cqe->sqid;

    if (cqe_sqid >= trs_res_get_max_id(ts_inst, TRS_HW_SQ)) {
        trs_err("Invalid sqid. (devid=%u; tsid=%u; cqid=%u; sqid=%u)\n", inst->devid, inst->tsid, cqid, cqe_sqid);
        return;
    }

    (void)trs_chan_ctrl(inst, ts_inst->sq_ctx[cqe_sqid].chan_id, CHAN_CTRL_CMD_SQ_HEAD_UPDATE, shm_cqe->sq_head);
}

static int trs_shm_cq_recv(struct trs_id_inst *inst, u32 cqid, void *cqe)
{
    struct trs_core_ts_inst *ts_inst = NULL;

    ts_inst = trs_core_ts_inst_get(inst);
    if (ts_inst == NULL) {
        trs_err("Invalid para. (devid=%u; tsid=%u; cqid=%u)\n", inst->devid, inst->tsid, cqid);
        return CQ_RECV_FINISH;
    }

    trs_shm_cq_recv_proc(ts_inst, cqid, cqe);
    trs_core_ts_inst_put(ts_inst);

    return CQ_RECV_FINISH;
}

static int trs_shm_alloc_phy_sq(struct trs_core_ts_inst *ts_inst)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    struct trs_shm_ctx *shm_ctx = &ts_inst->shm_ctx;
    struct trs_chan_sq_info sq_info;
    struct trs_chan_para para = {0};
    int ret;

    ka_task_mutex_lock(&shm_ctx->mutex);
    if (shm_ctx->ref > 0) {
        shm_ctx->ref++;
        ka_task_mutex_unlock(&shm_ctx->mutex);
        return 0;
    }

    para.types.type = CHAN_TYPE_SW;
    para.types.sub_type = CHAN_SUB_TYPE_SW_SHM;

    para.sq_para.sqe_size = TRS_SHM_SQE_SIZE;
    para.sq_para.sq_depth = TRS_SHM_SQ_DEPTH;

    para.flag = (0x1 << CHAN_FLAG_ALLOC_SQ_BIT);

    ret = hal_kernel_trs_chan_create(inst, &para, &shm_ctx->chan_id);
    if (ret != 0) {
        ka_task_mutex_unlock(&shm_ctx->mutex);
        trs_err("Alloc phy sq chan failed. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return ret;
    }

    (void)trs_chan_get_sq_info(inst, shm_ctx->chan_id, &sq_info);

    shm_ctx->sqid = sq_info.sqid;
    shm_ctx->sq_pa = sq_info.sq_phy_addr;
    shm_ctx->ref = 1;
    ka_task_mutex_unlock(&shm_ctx->mutex);

    trs_info("Success. (devid=%u; tsid=%u; sqid=%u)\n", inst->devid, inst->tsid, shm_ctx->sqid);

    return 0;
}

static int trs_shm_alloc_phy_cq(struct trs_core_ts_inst *ts_inst, struct trs_proc_shm_ctx *proc_shm_ctx)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    struct trs_chan_cq_info cq_info;
    struct trs_chan_para para = {0};
    int ret;

    ka_task_mutex_lock(&proc_shm_ctx->mutex);
    if (proc_shm_ctx->chan_id >= 0) {
        ka_task_mutex_unlock(&proc_shm_ctx->mutex);
        return -EEXIST;
    }

    para.types.type = CHAN_TYPE_SW;
    para.types.sub_type = CHAN_SUB_TYPE_SW_SHM;

    para.cq_para.cqe_size = TRS_SHM_CQE_SIZE;
    para.cq_para.cq_depth = TRS_SHM_CQ_DEPTH;
    para.ops.cqe_is_valid = trs_shm_cqe_is_valid;
    para.ops.get_sq_head_in_cqe = NULL;
    para.ops.cq_recv = trs_shm_cq_recv;
    para.ops.abnormal_proc = NULL;

    para.flag = (0x1 << CHAN_FLAG_ALLOC_CQ_BIT);

    ret = hal_kernel_trs_chan_create(inst, &para, &proc_shm_ctx->chan_id);
    if (ret != 0) {
        ka_task_mutex_unlock(&proc_shm_ctx->mutex);
        trs_err("Alloc phy cq chan failed. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return ret;
    }

    (void)trs_chan_get_cq_info(inst, proc_shm_ctx->chan_id, &cq_info);

    proc_shm_ctx->cq_irq = cq_info.irq;
    proc_shm_ctx->cq_pa = cq_info.cq_phy_addr;
    proc_shm_ctx->cqid = cq_info.cqid;
    ka_task_mutex_unlock(&proc_shm_ctx->mutex);

    trs_info("Success. (devid=%u; tsid=%u; cqid=%u)\n", inst->devid, inst->tsid, proc_shm_ctx->cqid);

    return 0;
}

static void trs_shm_free_phy_sq(struct trs_core_ts_inst *ts_inst)
{
    struct trs_shm_ctx *shm_ctx = &ts_inst->shm_ctx;
    ka_task_mutex_lock(&shm_ctx->mutex);
    shm_ctx->ref--;
    if (shm_ctx->ref == 0) {
        hal_kernel_trs_chan_destroy(&ts_inst->inst, shm_ctx->chan_id);
        shm_ctx->chan_id = -1;
    }
    ka_task_mutex_unlock(&shm_ctx->mutex);
}

static void trs_shm_free_phy_cq(struct trs_core_ts_inst *ts_inst, struct trs_proc_shm_ctx *proc_shm_ctx)
{
    ka_task_mutex_lock(&proc_shm_ctx->mutex);
    if (proc_shm_ctx->chan_id >= 0) {
        hal_kernel_trs_chan_destroy(&ts_inst->inst, proc_shm_ctx->chan_id);
        proc_shm_ctx->chan_id = -1;
    }
    ka_task_mutex_unlock(&proc_shm_ctx->mutex);
}

static int trs_shm_sqcq_alloc_para_check(struct trs_id_inst *inst, struct halSqCqInputInfo *para)
{
    struct trs_alloc_para *alloc_para = get_alloc_para_addr(para);

    if ((para->sqeSize != TRS_SHM_SQE_SIZE) || (para->sqeDepth != TRS_SHM_SQ_DEPTH) ||
        (para->cqeSize != TRS_SHM_CQE_SIZE) || (para->cqeDepth != TRS_SHM_CQ_DEPTH) ||
        (alloc_para->uio_info->sq_que_addr == 0)) {
        trs_err("Invalid para. (devid=%u; tsid=%u; sqeDepth=%u; sqeSize=%u; cqeDepth=%u; cqeSize=%u; sq_addr=0x%pK)\n",
                inst->devid, inst->tsid, para->sqeDepth, para->sqeSize, para->cqeDepth, para->cqeSize,
                (void *)(uintptr_t)alloc_para->uio_info->sq_que_addr);
        return -EINVAL;
    }

    return 0;
}

static int trs_shm_sq_remap(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst,
                            struct halSqCqInputInfo *para)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    struct trs_proc_ts_ctx *ts_ctx = &proc_ctx->ts_ctx[inst->tsid];
    struct trs_shm_ctx *shm_ctx = &ts_inst->shm_ctx;
    struct trs_alloc_para *alloc_para = get_alloc_para_addr(para);
    struct trs_mem_map_para map_para;
    int ret;

    trs_remap_fill_para(&map_para, TRS_MAP_TYPE_RO_DEV_MEM, true, alloc_para->uio_info->sq_que_addr, shm_ctx->sq_pa,
                        KA_MM_PAGE_ALIGN(para->sqeDepth * para->sqeSize));
    ret = trs_remap_sq(proc_ctx, ts_inst, &map_para);
    if (ret != 0) {
        trs_err("Remap sq failed. (devid=%u; tsid=%u; sqid=%u; va=%lx)\n", inst->devid, inst->tsid, para->sqId,
                alloc_para->uio_info->sq_que_addr);
        return ret;
    }

    ts_ctx->shm_ctx.shm_sq_va = alloc_para->uio_info->sq_que_addr;

    return 0;
}

static void trs_shm_sq_unmap(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst)
{
    struct trs_proc_ts_ctx *ts_ctx = &proc_ctx->ts_ctx[ts_inst->inst.tsid];
    struct trs_mem_unmap_para unmap_para;

    unmap_para.va = ts_ctx->shm_ctx.shm_sq_va;
    unmap_para.len = KA_MM_PAGE_ALIGN(TRS_SHM_SQE_SIZE * TRS_SHM_SQ_DEPTH);
    (void)trs_unmap_sq(proc_ctx, ts_inst, &unmap_para);
}

static int trs_shm_sqcq_alloc_notice_ts(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst,
                                        struct halSqCqInputInfo *para)
{
    struct trs_shm_ctx *shm_ctx = &ts_inst->shm_ctx;
    struct trs_proc_shm_ctx *proc_shm_ctx = &proc_ctx->ts_ctx[ts_inst->inst.tsid].shm_ctx;
    struct trs_shm_sqcq_mbox msg;
    int ret;

    trs_mbox_init_header(&msg.header, TRS_MBOX_SHM_SQCQ_ALLOC);

    msg.sq_addr = shm_ctx->sq_pa;
    msg.cq_addr = proc_shm_ctx->cq_pa;

    msg.sq_id = shm_ctx->sqid;
    msg.cq_id = proc_shm_ctx->cqid;

    msg.sqesize = para->sqeSize;
    msg.cqesize = para->cqeSize;

    msg.sqdepth = para->sqeDepth;
    msg.cqdepth = para->cqeDepth;

    msg.pid = proc_ctx->pid;
    msg.cq_irq = proc_shm_ctx->cq_irq;

    ret = memcpy_s(msg.info, sizeof(msg.info), para->info, sizeof(para->info));
    if (ret != 0) {
        trs_err("Memcopy failed. (dest_len=%lx; src_len=%lx)\n", sizeof(msg.info), sizeof(para->info));
        return ret;
    }

    /* adapt fill: app_type, fid, sq_cq_side */

    return trs_core_notice_ts(ts_inst, (u8 *)&msg, sizeof(msg));
}

static int trs_shm_sqcq_free_notice_ts(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst,
                                       struct halSqCqFreeInfo *para)
{
    struct trs_shm_sqcq_mbox msg;

    trs_mbox_init_header(&msg.header, TRS_MBOX_SHM_SQCQ_FREE);

    msg.sq_id = para->sqId;
    msg.cq_id = para->cqId;
    msg.pid = proc_ctx->pid;

    /* adapt fill: app_type, fid */

    return trs_core_notice_ts(ts_inst, (u8 *)&msg, sizeof(msg));
}

int trs_shm_sqcq_alloc(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct halSqCqInputInfo *para)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    struct trs_proc_ts_ctx *ts_ctx = &proc_ctx->ts_ctx[inst->tsid];
    struct trs_shm_ctx *shm_ctx = &ts_inst->shm_ctx;
    struct trs_proc_shm_ctx *proc_shm_ctx = &ts_ctx->shm_ctx;
    int ret;

    if (trs_is_stars_inst(ts_inst)) {
        trs_err("Stars not support shm. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return -EINVAL;
    }

    ret = trs_shm_sqcq_alloc_para_check(inst, para);
    if (ret != 0) {
        return ret;
    }

    ret = trs_shm_alloc_phy_sq(ts_inst);
    if (ret != 0) {
        trs_err("Phy sq alloc failed. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return ret;
    }

    ret = trs_shm_alloc_phy_cq(ts_inst, proc_shm_ctx);
    if (ret != 0) {
        trs_err("Phy cq alloc failed. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        trs_shm_free_phy_sq(ts_inst);
        return ret;
    }

    para->sqId = shm_ctx->sqid;
    para->cqId = proc_shm_ctx->cqid;
    para->flag = TSRRV_FLAG_SQ_RDONLY;

    ret = trs_shm_sq_remap(proc_ctx, ts_inst, para);
    if (ret != 0) {
        trs_shm_free_phy_sq(ts_inst);
        trs_shm_free_phy_cq(ts_inst, proc_shm_ctx);
        return ret;
    }

    ret = trs_shm_sqcq_alloc_notice_ts(proc_ctx, ts_inst, para);
    if (ret != 0) {
        trs_shm_sq_unmap(proc_ctx, ts_inst);
        trs_shm_free_phy_sq(ts_inst);
        trs_shm_free_phy_cq(ts_inst, proc_shm_ctx);
        return ret;
    }

    trs_info("Alloc success. (devid=%u; tsid=%u; sqid=%u, cqid=%u)\n", inst->devid, inst->tsid, para->sqId, para->cqId);

    return ret;
}

int trs_shm_sqcq_free(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct halSqCqFreeInfo *para)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    struct trs_shm_ctx *shm_ctx = &ts_inst->shm_ctx;
    struct trs_proc_shm_ctx *proc_shm_ctx = &proc_ctx->ts_ctx[inst->tsid].shm_ctx;
    int ret;

    if ((shm_ctx->chan_id < 0) || (shm_ctx->sqid != para->sqId) || (proc_shm_ctx->cqid != para->cqId)) {
        trs_err("Invalid para. (devid=%u; tsid=%u; sqid=%u; cqid=%u; chan_id=%u; shm_sqid=%u; shm_cqid=%u)\n",
                inst->devid, inst->tsid, para->sqId, para->cqId, shm_ctx->chan_id, shm_ctx->sqid, proc_shm_ctx->cqid);
        return -EINVAL;
    }

    ret = trs_shm_sqcq_free_notice_ts(proc_ctx, ts_inst, para);
    if (ret != 0) {
        return ret;
    }

    trs_shm_sq_unmap(proc_ctx, ts_inst);
    trs_shm_free_phy_sq(ts_inst);
    trs_shm_free_phy_cq(ts_inst, proc_shm_ctx);
    trs_info("Free success. (devid=%u; tsid=%u; sqid=%u, cqid=%u)\n", inst->devid, inst->tsid, para->sqId, para->cqId);

    return 0;
}

void trs_shm_sqcq_recycle(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst)
{
    trs_shm_free_phy_sq(ts_inst);
    trs_shm_free_phy_cq(ts_inst, &proc_ctx->ts_ctx[ts_inst->inst.tsid].shm_ctx);
}

int trs_shm_sqcq_init(struct trs_core_ts_inst *ts_inst)
{
    struct trs_shm_ctx *shm_ctx = &ts_inst->shm_ctx;
    ka_task_mutex_init(&shm_ctx->mutex);
    shm_ctx->chan_id = -1;
    return 0;
}

void trs_shm_sqcq_uninit(struct trs_core_ts_inst *ts_inst)
{
    struct trs_shm_ctx *shm_ctx = &ts_inst->shm_ctx;
    ka_task_mutex_destroy(&shm_ctx->mutex);
}
