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
#include "ka_kernel_def_pub.h"
#include "pbl/pbl_soc_res.h"
#include "trs_chan_irq.h"

#include "trs_tscpu_chan_sqcq.h"
#include "trs_chan_update.h"
#include "trs_pm_adapt.h"
#include "trs_tscpu_chan_near_ops_db.h"
#include "trs_chan_near_ops_mbox.h"
#include "trs_host_comm.h"
#include "trs_tscpu_chan_near_ops.h"

static int trs_tscpu_chan_ops_request_irq(struct trs_id_inst *inst, u32 irq_type, int irq_index, void *para,
                                          int (*handler)(int irq_type, int irq_index, void *para, u32 cqid[],
                                                         u32 cq_num))
{
    struct trs_chan_irq_attr attr;

    attr.get_valid_cq = NULL;
    attr.intr_mask_config = NULL;

    if (irq_type != TS_CQ_UPDATE_IRQ) {
        trs_err("Unknown irq_type. (irq_type=%d)\n", irq_type);
        return -ENODEV;
    }

    attr.group = 0;
    attr.handler = handler;
    attr.para = para;
    attr.request_chan_irq = trs_adapt_ops_request_irq;
    attr.free_chan_irq = trs_adapt_ops_free_irq;
    return trs_chan_request_irq(inst, irq_type, irq_index, &attr);
}

static int trs_chan_adapt_sqe_update(struct trs_id_inst *inst, struct trs_sqe_update_info *update_info)
{
    return trs_chan_ops_sqe_update(inst, update_info);
}

static int _trs_chan_ops_ctrl_sqcq(struct trs_id_inst *inst, struct trs_chan_type *types, u32 id, u32 cmd, u32 para)
{
    int ret = -EINVAL;

    switch (types->type) {
        case CHAN_TYPE_HW:
        case CHAN_TYPE_SW:
        case CHAN_TYPE_TASK_SCHED:
            ret = trs_tscpu_chan_ops_ctrl_sqcq(inst, id, cmd, para);
            break;
        default:
            break;
    }

    return ret;
}

static int _trs_chan_ops_query_sqcq(struct trs_id_inst *inst, struct trs_chan_type *types, u32 id, u32 cmd, u64 *value)
{
    int ret = -EINVAL;

    switch (types->type) {
        case CHAN_TYPE_HW:
        case CHAN_TYPE_SW:
        case CHAN_TYPE_TASK_SCHED:
            ret = trs_tscpu_chan_ops_query_sqcq(inst, id, cmd, value);
            break;
        default:
            break;
    }

    return ret;
}

void trs_chan_tscpu_update_ssid(struct trs_id_inst *inst, struct trs_chan_info *chan_info)
{
#ifndef EMU_ST
    if ((chan_info->types.type == CHAN_TYPE_HW) && (chan_info->types.sub_type == CHAN_SUB_TYPE_HW_DVPP)) {
        int user_visible_flag;
        int ssid = 0;
        int ret = trs_host_get_ssid(inst, &user_visible_flag, &ssid);
        if (ret != 0) {
            trs_warn("Get ssid warn. (devid=%u; ret=%d)\n", inst->devid, ret);
        }
        chan_info->ssid = (u16)ssid;
    }
#endif
}

static int trs_chan_tscpu_ops_notice_ts(struct trs_id_inst *inst, struct trs_chan_info *chan_info)
{
    trs_chan_tscpu_update_ssid(inst, chan_info);
    return trs_chan_near_ops_mbox_send(inst, chan_info);
}

static struct trs_chan_adapt_ops g_trs_tscpu_chan_ops = {
    .owner = KA_THIS_MODULE,
    .sq_mem_alloc = trs_chan_ops_sq_mem_alloc,
    .sq_mem_free = trs_chan_ops_sq_mem_free,
    .cq_mem_alloc = trs_chan_ops_cq_mem_alloc,
    .cq_mem_free = trs_chan_ops_cq_mem_free,
    .flush_cache = trs_chan_ops_flush_sqe_cache,
    .invalid_cache = trs_chan_ops_invalid_cqe_cache,
    .cqe_is_valid = trs_tscpu_chan_ops_cqe_is_valid,
    .get_sq_head_in_cqe = trs_tscpu_chan_ops_get_sq_head_in_cqe,
    .sqe_update = trs_chan_adapt_sqe_update,
    .cqe_update = trs_chan_ops_cqe_update,
    .sqcq_ctrl = _trs_chan_ops_ctrl_sqcq,
    .sqcq_query = _trs_chan_ops_query_sqcq,
    .notice_ts = trs_chan_tscpu_ops_notice_ts,
    .get_irq = trs_chan_get_irq,
    .request_irq = trs_tscpu_chan_ops_request_irq,
    .free_irq = trs_chan_free_irq,
};

struct trs_chan_adapt_ops *trs_chan_get_tscpu_adapt_ops(void)
{
    return &g_trs_tscpu_chan_ops;
}

int trs_tscpu_chan_ops_init(struct trs_id_inst *inst)
{
    return trs_tscpu_chan_near_ops_db_init(inst);
}

void trs_tscpu_chan_ops_uninit(struct trs_id_inst *inst)
{
    trs_tscpu_chan_near_ops_db_uninit(inst);
}
