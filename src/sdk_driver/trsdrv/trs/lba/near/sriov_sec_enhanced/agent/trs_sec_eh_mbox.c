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

#include "trs_chan_near_ops_rsv_mem.h"
#include "trs_chan_near_ops_mem.h"
#include "trs_pm_adapt.h"
#include "trs_chan.h"

#include "trs_sec_eh_mbox.h"
#include "trs_sec_eh_sq.h"
#include "trs_sec_eh_id.h"

typedef int (*mb_update_func)(struct trs_sec_eh_ts_inst *sec_eh_cfg, void *mb_data);

static int trs_sec_eh_update_rsv_mem_info(struct trs_id_inst *inst, u32 sq_mem_type,
                                          struct trs_normal_cqsq_mailbox *mb_data)
{
    mb_data->sq_cq_side &= 0xfe; // 0xfe: clear sq side bit
    mb_data->sq_cq_side |= ((trs_chan_mem_is_dev_mem(sq_mem_type) ? 0 : 1) << TRS_CHAN_SQ_MEM_OFFSET);
    if (trs_chan_get_sqcq_mem_side(sq_mem_type) == TRS_CHAN_DEV_RSV_MEM) {
        int ret = trs_chan_near_sqcq_rsv_mem_h2d(inst, mb_data->sq_addr, &mb_data->sq_addr);
        if (ret != 0) {
            trs_err("Rsv mem h2d failed. (devid=%u; tsid=%u; ret=%d)\n", inst->devid, inst->tsid, ret);
            return ret;
        }
    }

    return 0;
}

static void trs_sec_eh_sq_ctx_info_pack(struct trs_normal_cqsq_mailbox *mb_data, struct trs_sec_eh_sq_ctx_info *sq_ctx)
{
    sq_ctx->sqid = mb_data->sq_index;
    sq_ctx->sqesize = mb_data->sqesize;
    sq_ctx->sqdepth = mb_data->sqdepth;
    sq_ctx->addr_offset = mb_data->sq_addr;
    sq_ctx->pid = mb_data->pid;
    sq_ctx->sq_create_cmd_type = mb_data->header.cmd_type;
}

static void trs_sec_eh_cq_ctx_info_pack(struct trs_normal_cqsq_mailbox *mb_data, struct trs_sec_eh_cq_ctx_info *cq_ctx)
{
    cq_ctx->cqid = mb_data->cq0_index;
    cq_ctx->cqesize = mb_data->cqesize;
    cq_ctx->cqdepth = mb_data->cqdepth;
    cq_ctx->cq_paddr = mb_data->cq0_addr;
}

static int trs_sec_eh_create_sqcq_mb_update(struct trs_sec_eh_ts_inst *sec_eh_cfg, void *data)
{
    struct trs_normal_cqsq_mailbox *mb_data = (struct trs_normal_cqsq_mailbox *)data;
    struct trs_sec_eh_sq_ctx_info sq_ctx = {0};
    struct trs_sec_eh_cq_ctx_info cq_ctx = {0};
    int ret = -EINVAL;

    if ((!trs_sec_eh_id_is_belong_to_vf(&sec_eh_cfg->id_info[TRS_HW_SQ], mb_data->sq_index)) ||
        (!trs_sec_eh_id_is_belong_to_vf(&sec_eh_cfg->id_info[TRS_HW_CQ], mb_data->cq0_index))) {
        trs_debug("Invalid. (devid=%u; sq=%u; cq=%u)\n", sec_eh_cfg->inst.devid, mb_data->sq_index, mb_data->cq0_index);
        return -EACCES;
    }

    trs_sec_eh_cq_ctx_info_pack(mb_data, &cq_ctx);
    ret = trs_sec_eh_check_and_update_cq_ctx_info(sec_eh_cfg, &cq_ctx);
    if (ret != 0) {
        return ret;
    }
    mb_data->cq0_addr = cq_ctx.cq_paddr;

    trs_sec_eh_sq_ctx_info_pack(mb_data, &sq_ctx);
    ret = trs_sec_eh_alloc_sq_mem(sec_eh_cfg, &sq_ctx);
    if (ret != 0) {
        return ret;
    }

    trs_sec_eh_sq_ctx_init(sec_eh_cfg, &sq_ctx);

    mb_data->sq_addr = sq_ctx.sq_paddr;
    ret = trs_sec_eh_update_rsv_mem_info(&sec_eh_cfg->pm_inst, sq_ctx.mem_type, mb_data);
    if (ret != 0) {
#ifndef EMU_ST
        trs_sec_eh_free_sq_mem(sec_eh_cfg, mb_data->sq_index);
        trs_sec_eh_sq_ctx_uninit(sec_eh_cfg, mb_data->sq_index);
        return ret;
#endif
    }
    trs_debug("Id info. (devid=%u; tsid=%u; sq=%u; cq=%u; pid=%d; sqcq_side=%u; sq_mem_type=0x%x)\n",
              sec_eh_cfg->inst.devid, sec_eh_cfg->inst.tsid, mb_data->sq_index, mb_data->cq0_index, mb_data->pid,
              mb_data->sq_cq_side, sq_ctx.mem_type);

    return ret;
}

static int trs_sec_eh_free_sqcq_mb_update(struct trs_sec_eh_ts_inst *sec_eh_cfg, void *data)
{
    struct trs_normal_cqsq_mailbox *mb_data = (struct trs_normal_cqsq_mailbox *)data;

    if ((!trs_sec_eh_id_is_belong_to_vf(&sec_eh_cfg->id_info[TRS_HW_SQ], mb_data->sq_index)) ||
        (!trs_sec_eh_id_is_belong_to_vf(&sec_eh_cfg->id_info[TRS_HW_CQ], mb_data->cq0_index))) {
        trs_debug("Invalid. (devid=%u; sq=%u; cq=%u)\n", sec_eh_cfg->inst.devid, mb_data->sq_index, mb_data->cq0_index);
        return -EACCES;
    }

    trs_sec_eh_free_sq_mem(sec_eh_cfg, mb_data->sq_index);
    trs_sec_eh_sq_ctx_uninit(sec_eh_cfg, mb_data->sq_index);
    trs_debug("Id info. (devid=%u; tsid=%u; sq=%u; cq=%u; pid=%d)\n", sec_eh_cfg->inst.devid, sec_eh_cfg->inst.tsid,
              mb_data->sq_index, mb_data->cq0_index, mb_data->pid);

    return 0;
}

static int trs_sec_eh_free_all_sqcq_mb_update(struct trs_sec_eh_ts_inst *sec_eh_cfg, void *data)
{
    struct recycle_proc_msg *mb_data = (struct recycle_proc_msg *)data;
    struct trs_sec_eh_sq_ctx *sq_ctx = sec_eh_cfg->sq_ctx;
    u32 sqid;

    for (sqid = 0; sqid < sec_eh_cfg->id_info[TRS_HW_SQ].end; sqid++) {
        if (sq_ctx[sqid].pid == mb_data->proc_info.pid[0]) {
            trs_sec_eh_free_sq_mem(sec_eh_cfg, sqid);
            trs_sec_eh_sq_ctx_uninit(sec_eh_cfg, sqid);
            trs_debug("Id info. (devid=%u; tsid=%u; pid=%d; sqid=%u)\n", sec_eh_cfg->inst.devid, sec_eh_cfg->inst.tsid,
                      mb_data->proc_info.pid[0], sqid);
        }
    }

    return 0;
}

static const mb_update_func sec_eh_mb_update_handle[TRS_MBOX_CMD_MAX] = {
    [TRS_MBOX_CREATE_CQSQ_CALC] = trs_sec_eh_create_sqcq_mb_update,
    [TRS_MBOX_RELEASE_CQSQ_CALC] = trs_sec_eh_free_sqcq_mb_update,
    [TRS_MBOX_RECYCLE_PID] = trs_sec_eh_free_all_sqcq_mb_update,
    [TRS_MBOX_CREATE_TOPIC_SQCQ] = trs_sec_eh_create_sqcq_mb_update,
    [TRS_MBOX_RELEASE_TOPIC_SQCQ] = trs_sec_eh_free_sqcq_mb_update,
};

int trs_sec_eh_mb_update(struct trs_id_inst *inst, u16 cmd, void *mb_data)
{
    struct trs_sec_eh_ts_inst *sec_eh_cfg = NULL;
    int ret = -EINVAL;

    if (cmd >= TRS_MBOX_CMD_MAX) {
        return ret;
    }

    if (sec_eh_mb_update_handle[cmd] == NULL) {
        return 0;
    }

    sec_eh_cfg = trs_sec_eh_ts_inst_get(inst);
    if (sec_eh_cfg != NULL) {
        ret = sec_eh_mb_update_handle[cmd](sec_eh_cfg, mb_data);
        trs_sec_eh_ts_inst_put(sec_eh_cfg);
    }

    return ret;
}
