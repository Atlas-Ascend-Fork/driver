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
#include "securec.h"
#include "pbl/pbl_soc_res.h"
#include "trs_chan.h"
#include "trs_chan_mem.h"
#include "trs_rsv_mem.h"
#include "trs_chan_irq.h"
#include "trs_chan_update.h"
#include "trs_chip_def_comm.h"
#include "trs_tscpu_chan_sqcq.h"
#include "trs_chan_near_ops_mbox.h"
#include "trs_chan_near_ops_rsv_mem.h"
#include "trs_chan_near_ops_id.h"
#include "trs_chan_stars_v1_ops_stars.h"
#include "trs_chan_sqcq.h"
#include "soc_adapt.h"
#include "trs_pm_adapt.h"
#include "trs_ts_db.h"
#include "trs_chan_maint_sqcq.h"
#include "trs_host_comm.h"
#include "trs_chan_stars_v1_ops.h"

int trs_chan_ops_request_irq(struct trs_id_inst *inst, u32 irq_type, int irq_index, void *para,
                             int (*handler)(int irq_type, int irq_index, void *para, u32 cqid[], u32 cq_num))
{
    struct trs_chan_irq_attr attr;
    u32 group_index = irq_index;
    int ret;

    switch (irq_type) {
        case TS_CQ_UPDATE_IRQ:
            attr.get_valid_cq = trs_chan_ops_get_valid_cq_list;
            attr.intr_mask_config = trs_chan_ops_intr_mask_config;
            break;
        case TS_FUNC_CQ_IRQ:
            attr.get_valid_cq = NULL;
            attr.intr_mask_config = NULL;
            break;
        default:
            trs_err("Unknown irq_type. (irq_type=%d)\n", irq_type);
            return -ENODEV;
    }

    ret = trs_chan_get_cq_group(inst, group_index, &attr.group);
    if (ret != 0) {
        return ret;
    }

    attr.handler = handler;
    attr.para = para;
    attr.request_chan_irq = trs_adapt_ops_request_irq;
    attr.free_chan_irq = trs_adapt_ops_free_irq;
    return trs_chan_request_irq(inst, irq_type, irq_index, &attr);
}

static int trs_chan_ops_get_irq(struct trs_id_inst *inst, u32 irq_type, u32 irq[], u32 irq_num, u32 *valid_irq_num)
{
    int ret = trs_chan_get_irq(inst, irq_type, irq, irq_num, valid_irq_num);

    if (irq_type == TS_CQ_UPDATE_IRQ) {
        u32 group_num;

        ret |= trs_chan_get_cq_group_num(inst, &group_num);
        if (ret == 0) {
            *valid_irq_num = (*valid_irq_num > group_num) ? group_num : *valid_irq_num;
        }
    }

    return ret;
}

static int trs_chan_get_cq_affinity_irq(struct trs_id_inst *inst, u32 cq_id, u32 *irq_index)
{
    u32 group, group_index;
    int ret = trs_stars_get_cq_affinity_group(inst, cq_id, &group);
    if (ret != 0) {
        return ret;
    }

    ret = trs_chan_get_cq_group_index(inst, group, &group_index);
    if (ret == 0) {
        *irq_index = group_index;
    }

    return ret;
}

static int trs_chan_adapt_sqe_update(struct trs_id_inst *inst, struct trs_sqe_update_info *update_info)
{
    return trs_chan_ops_sqe_update(inst, update_info);
}

int trs_chan_ops_ctrl_sqcq(struct trs_id_inst *inst, struct trs_chan_type *types, u32 id, u32 cmd, u32 para)
{
    int ret = -EINVAL;

    switch (types->type) {
        case CHAN_TYPE_HW:
            ret = trs_stars_chan_ops_ctrl_sqcq(inst, id, cmd, para);
            break;
        case CHAN_TYPE_SW:
            ret = trs_tscpu_chan_ops_ctrl_sqcq(inst, id, cmd, para);
            break;
        case CHAN_TYPE_MAINT:
            ret = trs_chan_ops_ctrl_maint_sqcq(inst, id, cmd, para);
            break;
        default:
            break;
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(trs_chan_ops_ctrl_sqcq);

int trs_chan_ops_query_sqcq(struct trs_id_inst *inst, struct trs_chan_type *types, u32 id, u32 cmd, u64 *value)
{
    int ret = -EINVAL;

    *value = 0;
    switch (types->type) {
        case CHAN_TYPE_HW:
            ret = trs_stars_chan_ops_query_sqcq(inst, id, cmd, value);
            break;
        case CHAN_TYPE_SW:
            ret = trs_tscpu_chan_ops_query_sqcq(inst, id, cmd, value);
            break;
        case CHAN_TYPE_MAINT:
            ret = 0;
            break;
        default:
            break;
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(trs_chan_ops_query_sqcq);

static bool trs_chan_ops_cq_need_resched(struct trs_id_inst *inst, struct trs_chan_type *types)
{
#ifndef EMU_ST
    if (types->type == CHAN_TYPE_HW) {
        return true;
    }
#endif
    return false;
}

void trs_chan_stars_v1_update_ssid(struct trs_id_inst *inst, struct trs_chan_info *chan_info)
{
    if ((chan_info->types.type == CHAN_TYPE_HW) && (chan_info->types.sub_type == CHAN_SUB_TYPE_HW_DVPP)) {
        int user_visible_flag;
        int ssid = 0;
        int ret = trs_host_get_ssid(inst, &user_visible_flag, &ssid);
        if (ret != 0) {
            trs_warn("Get ssid warn. (devid=%u; ret=%d)\n", inst->devid, ret);
        }
        chan_info->ssid = (u16)ssid;
    }
}

static int trs_chan_stars_v1_ops_notice_ts(struct trs_id_inst *inst, struct trs_chan_info *chan_info)
{
    trs_chan_stars_v1_update_ssid(inst, chan_info);
    return trs_chan_near_ops_mbox_send(inst, chan_info);
}

static struct trs_chan_adapt_ops g_trs_chan_stars_v1_ops = {
    .owner = KA_THIS_MODULE,
    .sq_mem_alloc = trs_chan_ops_sq_mem_alloc,
    .sq_mem_free = trs_chan_ops_sq_mem_free,
    .cq_mem_alloc = trs_chan_ops_cq_mem_alloc,
    .cq_mem_free = trs_chan_ops_cq_mem_free,
    .flush_cache = trs_chan_ops_flush_sqe_cache,
    .invalid_cache = trs_chan_ops_invalid_cqe_cache,
    .cqe_is_valid = trs_chan_ops_cqe_is_valid,
    .get_sq_head_in_cqe = trs_chan_ops_get_sq_head_in_cqe,
    .sqe_update = trs_chan_adapt_sqe_update,
    .cqe_update = trs_chan_ops_cqe_update,
    .sqcq_ctrl = trs_chan_ops_ctrl_sqcq,
    .sqcq_query = trs_chan_ops_query_sqcq,
    .notice_ts = trs_chan_stars_v1_ops_notice_ts,
    .get_irq = trs_chan_ops_get_irq,
    .get_cq_affinity_irq = trs_chan_get_cq_affinity_irq,
    .request_irq = trs_chan_ops_request_irq,
    .free_irq = trs_chan_free_irq,
    .sq_mem_map = trs_chan_ops_sq_rsvmem_map,
    .sq_mem_unmap = trs_chan_ops_sq_rsvmem_unmap,
    .cq_need_resched = trs_chan_ops_cq_need_resched,
    .sqcq_speified_id_alloc = trs_chan_ops_sqcq_speified_id_alloc,
    .sqcq_speified_id_free = trs_chan_ops_sqcq_speified_id_free};

struct trs_chan_adapt_ops *trs_chan_get_stars_v1_adapt_ops(void)
{
    return &g_trs_chan_stars_v1_ops;
}

int trs_chan_stars_v1_ops_init(struct trs_id_inst *inst)
{
    return trs_chan_stars_v1_ops_stars_init(inst);
}

void trs_chan_stars_v1_ops_uninit(struct trs_id_inst *inst)
{
    trs_chan_stars_v1_ops_stars_uninit(inst);
}
