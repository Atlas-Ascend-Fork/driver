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
#include "trs_chan_stars_v2_ops_stars.h"
#include "trs_chan_sqcq.h"
#include "soc_adapt.h"
#include "trs_adapt.h"
#include "trs_pm_adapt.h"
#include "trs_ts_db.h"
#include "trs_chan_maint_sqcq.h"
#include "trs_chan_near_event_update.h"
#include "trs_ub_init.h"
#include "trs_msg.h"
#include "comm_kernel_interface.h"
#include "trs_id_range.h"
#include "trs_host_comm.h"
#include "trs_host_ts_cq.h"
#include "trs_stars_v2_chan_sqcq.h"
#include "trs_chan_stars_v2_maint_sqcq.h"
#include "trs_sqe_update.h"
#include "trs_host_mode_config.h"

#include "dpa_kernel_interface.h"
#include "trs_chan_stars_v2_ops.h"

int trs_stars_v2_chan_ops_request_irq(struct trs_id_inst *inst, u32 irq_type, int irq_index, void *para,
                                      int (*handler)(int irq_type, int irq_index, void *para, u32 cqid[], u32 cq_num))
{
    struct trs_chan_irq_attr attr;
    u32 group_index = irq_index;
    int ret;

    switch (irq_type) {
        case TS_CQ_UPDATE_IRQ:
            attr.get_valid_cq = trs_stars_v2_chan_ops_get_valid_cq_list;
            attr.intr_mask_config = trs_stars_v2_chan_ops_intr_mask_config;
            break;
        case TS_FUNC_CQ_IRQ:
            return trs_register_ts_cq_process_info(inst->devid, handler, para);
        default:
            trs_warn("Unknown irq_type. (irq_type=%d)\n", irq_type);
            return -ENODEV;
    }

    ret = trs_chan_stars_v2_get_cq_group(inst, group_index, &attr.group);
    if (ret != 0) {
        return ret;
    }

    attr.handler = handler;
    attr.para = para;
    attr.request_chan_irq = trs_adapt_ops_request_irq;
    attr.free_chan_irq = trs_adapt_ops_free_irq;
    if (devdrv_get_connect_protocol(inst->devid) == CONNECT_PROTOCOL_UB) {
#ifdef CFG_FEATURE_SUPPORT_UB_CONNECTION
        ret = trs_ub_request_cq_update_irq(inst, irq_type, irq_index, &attr);
#endif
    } else {
        ret = trs_chan_request_irq(inst, irq_type, irq_index, &attr);
    }

    return ret;
}

int trs_stars_v2_chan_ops_free_irq(struct trs_id_inst *inst, int irq_type, int irq_index, void *para)
{
    int ret = 0;

    if (irq_type == TS_FUNC_CQ_IRQ) {
        trs_unregister_ts_cq_process_info(inst->devid);
        return 0;
    }

    if (devdrv_get_connect_protocol(inst->devid) == CONNECT_PROTOCOL_UB) {
#ifdef CFG_FEATURE_SUPPORT_UB_CONNECTION
        ret = trs_ub_free_cq_update_irq(inst, irq_type, irq_index, para);
#endif
    } else {
        ret = trs_chan_free_irq(inst, irq_type, irq_index, para);
    }

    return ret;
}

static int trs_stars_v2_chan_ops_get_irq(struct trs_id_inst *inst, u32 irq_type, u32 irq[], u32 irq_num,
                                         u32 *valid_irq_num)
{
    int ret;

    if (devdrv_get_connect_protocol(inst->devid) == CONNECT_PROTOCOL_UB) {
        *valid_irq_num = 1;
        return 0;
    }

    ret = trs_chan_get_irq(inst, irq_type, irq, irq_num, valid_irq_num);

    if (irq_type == TS_CQ_UPDATE_IRQ) {
        u32 group_num;

        ret |= trs_chan_stars_v2_get_cq_group_num(inst, &group_num);
        if (ret == 0) {
            *valid_irq_num = (*valid_irq_num > group_num) ? group_num : *valid_irq_num;
        }
    }

    return ret;
}

static int trs_stars_v2_chan_get_cq_affinity_irq(struct trs_id_inst *inst, u32 cq_id, u32 *irq_index)
{
    u32 group, group_index;
    int ret = trs_stars_get_cq_affinity_group(inst, cq_id, &group);
    if (ret != 0) {
        return ret;
    }

    ret = trs_chan_stars_v2_get_cq_group_index(inst, group, &group_index);
    if (ret == 0) {
        *irq_index = group_index;
    }

    return ret;
}

static int trs_chan_adapt_sqe_update(struct trs_id_inst *inst, struct trs_sqe_update_info *update_info)
{
    return trs_chan_ops_sqe_update(inst, update_info);
}

int trs_chan_stars_v2_ops_ctrl_sqcq(struct trs_id_inst *inst, struct trs_chan_type *types, u32 id, u32 cmd, u32 para)
{
    int ret = -EINVAL;

    switch (types->type) {
        case CHAN_TYPE_HW:
            ret = trs_stars_v2_chan_ops_ctrl_sqcq(inst, id, cmd, para);
            break;
        case CHAN_TYPE_MAINT:
            ret = trs_stars_v2_chan_ops_ctrl_maint_sqcq(inst, id, types->sub_type, cmd, para);
            break;
        default:
            break;
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(trs_chan_stars_v2_ops_ctrl_sqcq);

int trs_chan_stars_v2_ops_query_sqcq(struct trs_id_inst *inst, struct trs_chan_type *types, u32 id, u32 cmd, u64 *value)
{
#ifndef EMU_ST
    int ret = -EINVAL;

    *value = 0;
    switch (types->type) {
        case CHAN_TYPE_HW:
            if (cmd == QUERY_CMD_SQ_DB_PADDR) {
                if (trs_get_sq_send_mode(inst->devid) == TRS_MODE_TYPE_SQ_SEND_HIGH_PERFORMANCE) {
                    ret = trs_stars_v2_chan_ops_query_sqcq(inst, id, QUERY_CMD_SQ_TAIL_PADDR, value);
                } else {
                    *value = 0;
                    ret = 0;
                }
            } else if ((cmd == QUERY_CMD_SQ_TAIL_PADDR) &&
                       (trs_get_sq_send_mode(inst->devid) == TRS_MODE_TYPE_SQ_SEND_HIGH_PERFORMANCE)) {
                *value = 0;
                ret = 0;
            } else {
                ret = trs_stars_v2_chan_ops_query_sqcq(inst, id, cmd, value);
            }
            break;
        case CHAN_TYPE_MAINT:
            ret = 0;
            break;
        default:
            break;
    }

    return ret;
#endif
}
KA_EXPORT_SYMBOL_GPL(trs_chan_stars_v2_ops_query_sqcq);

void trs_chan_stars_v2_update_ssid(struct trs_id_inst *inst, struct trs_chan_info *chan_info)
{
    if (chan_info->op == 1) {
#ifdef CFG_FEATURE_SUPPORT_APM
        u32 ssid = 0;
        int ret;
        ret = hal_kernel_apm_query_slave_ssid_by_master(inst->devid, ka_task_get_current_tgid(), PROCESS_CP1, &ssid);
        if (ret != 0) {
            trs_warn("Get ssid warn. (devid=%u; ret=%d)\n", inst->devid, ret);
        }

        chan_info->ssid = (u16)ssid;
#endif
    }
}

static int trs_chan_stars_v2_ops_notice_ts(struct trs_id_inst *inst, struct trs_chan_info *chan_info)
{
    int ret = 0;

    if ((chan_info->types.type == CHAN_TYPE_HW) && ((chan_info->ext_msg == NULL) || (chan_info->ext_msg_len == 0))) {
        trs_err("Ext info is NULL. (ext_msg_len=%u)\n", chan_info->ext_msg_len);
        return -EINVAL;
    }

    if ((devdrv_get_connect_protocol(inst->devid) == CONNECT_PROTOCOL_UB) && (chan_info->types.type == CHAN_TYPE_HW)) {
#ifdef CFG_FEATURE_SUPPORT_UB_CONNECTION
        ret = trs_ub_sqcq_info_update(inst, chan_info);
#endif
    } else {
        trs_chan_stars_v2_update_ssid(inst, chan_info);
        ret = trs_chan_near_ops_mbox_send(inst, chan_info);
    }

    return ret;
}

static bool trs_chan_ops_cq_need_resched(struct trs_id_inst *inst, struct trs_chan_type *types)
{
#ifndef EMU_ST
    if ((types->type == CHAN_TYPE_HW) && (devdrv_get_connect_protocol(inst->devid) != CONNECT_PROTOCOL_UB)) {
        return true;
    }
#endif
    return false;
}

static int trs_stars_v2_chan_ops_sqcq_alloc(struct trs_id_inst *inst, int type, u32 flag, u32 *id, void *para)
{
    if (trs_id_is_ranged(flag)) {
        struct trs_ext_info_header *header = (struct trs_ext_info_header *)para;
        if (header == NULL) {
            return -EINVAL;
        }
        return trs_id_alloc_in_range(inst, type, id, header->vfid);
    }

    if (devdrv_get_connect_protocol(inst->devid) == CONNECT_PROTOCOL_UB) {
        return trs_stars_v2_chan_ops_sqcq_speicified_id_alloc(inst, type, id);
    } else {
        return trs_id_alloc_ex(inst, type, flag, id, 1);
    }
}

static int trs_stars_v2_chan_ops_sqcq_free(struct trs_id_inst *inst, int type, u32 flag, u32 id)
{
    if (trs_id_is_ranged(flag)) {
        return trs_id_free_ex(inst, type, flag, id);
    }

    if (devdrv_get_connect_protocol(inst->devid) == CONNECT_PROTOCOL_UB) {
        return trs_stars_v2_chan_ops_sqcq_speicified_id_free(inst, type, id);
    } else {
        return trs_id_free_ex(inst, type, flag, id);
    }
}

extern int trs_sqe_update_desc_create(u32 devid, u32 tsid, struct trs_dma_desc_addr_info *addr_info,
                                      struct trs_dma_desc *dma_desc, bool is_src_secure);
static int trs_chan_stars_v2_ops_sq_dma_desc_create(struct trs_id_inst *inst, struct trs_chan_dma_desc *para)
{
#ifndef CFG_FEATURE_VM_ADAPT
    struct trs_dma_desc_addr_info addr_info = {0};
    struct trs_dma_desc dma_desc = {0};
    int ret;

#ifdef CFG_FEATURE_SUPPORT_APM
    ret = hal_kernel_apm_query_slave_ssid_by_master(inst->devid, ka_task_get_current_tgid(), PROCESS_CP1,
                                                    &addr_info.passid);
    if (ret != 0) {
        trs_err("Get ssid failed. (devid=%u; ret=%d)\n", inst->devid, ret);
        return ret;
    }
#endif

    addr_info.size = para->len;
    addr_info.sqid = para->sq_id;
    addr_info.sqeid = para->sqe_pos;
    addr_info.src_va = (u64)(uintptr_t)para->src;
    ret = trs_sqe_update_desc_create(inst->devid, inst->tsid, &addr_info, &dma_desc, false);
    if (ret != 0) {
        trs_err("Create sqe update dma desc failed. (devid=%u; tsid=%u; sqid=%u; sqeid=%u; ret=%d)\n", inst->devid,
                inst->tsid, para->sq_id, para->sqe_pos, ret);
        return ret;
    }

    para->dma_base = (unsigned long long)(uintptr_t)dma_desc.pciedma_desc.sq_addr;
    para->dma_node_num = 1;
    return 0;
#else
    return -EOPNOTSUPP;
#endif
}

static void trs_chan_stars_v2_ops_sq_dma_desc_destroy(struct trs_id_inst *inst, u32 sqid)
{
#ifndef CFG_FEATURE_VM_ADAPT
    hal_kernel_sqe_update_desc_destroy(inst->devid, inst->tsid, sqid);
#else
    return;
#endif
}

static struct trs_chan_adapt_ops g_trs_chan_stars_v2_ops = {
    .owner = KA_THIS_MODULE,
    .sq_mem_alloc = trs_chan_ops_sq_mem_alloc,
    .sq_mem_free = trs_chan_ops_sq_mem_free,
    .cq_mem_alloc = trs_chan_ops_cq_mem_alloc,
    .cq_mem_free = trs_chan_ops_cq_mem_free,
    .sqcq_speified_id_alloc = trs_stars_v2_chan_ops_sqcq_alloc,
    .sqcq_speified_id_free = trs_stars_v2_chan_ops_sqcq_free,
    .flush_cache = trs_chan_ops_flush_sqe_cache,
    .invalid_cache = NULL,
    .cqe_is_valid = trs_stars_v2_chan_ops_cqe_is_valid,
    .get_sq_head_in_cqe = trs_stars_v2_chan_ops_get_sq_head_in_cqe,
    .sqe_update = trs_chan_adapt_sqe_update,
    .cqe_update = trs_chan_ops_cqe_update,
    .sqcq_ctrl = trs_chan_stars_v2_ops_ctrl_sqcq,
    .sqcq_query = trs_chan_stars_v2_ops_query_sqcq,
    .notice_ts = trs_chan_stars_v2_ops_notice_ts,
    .get_irq = trs_stars_v2_chan_ops_get_irq,
    .get_cq_affinity_irq = trs_stars_v2_chan_get_cq_affinity_irq,
    .request_irq = trs_stars_v2_chan_ops_request_irq,
    .free_irq = trs_stars_v2_chan_ops_free_irq,
    .sq_mem_map = NULL,
    .sq_mem_unmap = NULL,
    .cq_need_resched = trs_chan_ops_cq_need_resched,
    .sq_dma_desc_create = trs_chan_stars_v2_ops_sq_dma_desc_create,
    .sq_dma_desc_destroy = trs_chan_stars_v2_ops_sq_dma_desc_destroy,
};

struct trs_chan_adapt_ops *trs_chan_get_stars_v2_adapt_ops(void)
{
    return &g_trs_chan_stars_v2_ops;
}

int trs_chan_stars_v2_ops_init(struct trs_id_inst *inst)
{
    return trs_chan_stars_v2_ops_stars_init(inst);
}

void trs_chan_stars_v2_ops_uninit(struct trs_id_inst *inst)
{
    trs_chan_stars_v2_ops_stars_uninit(inst);
}
