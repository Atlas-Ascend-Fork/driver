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
#include "soc_adapt.h"
#include "pbl/pbl_soc_res.h"
#include "trs_mailbox_def.h"
#include "trs_mbox.h"
#include "trs_chan_mbox.h"
#include "ascend_kernel_hal.h"
#include "trs_core_near_ops_mbox.h"

#ifndef EMU_ST
static int trs_core_ops_cbcq_msg_fill(struct trs_id_inst *inst, u8 *msg, u32 len)
{
    struct trs_cb_cq_mbox *tmp_msg = (struct trs_cb_cq_mbox *)msg;

    tmp_msg->plat_type = TRS_MBOX_SEND_FROM_HOST;

    if (tmp_msg->header.cmd_type == TRS_MBOX_RELEASE_CB_CQ) {
        return 0;
    }

    return trs_chan_ops_get_hw_irq(inst, TS_CQ_UPDATE_IRQ, tmp_msg->cq_irq, &tmp_msg->cq_irq);
}

static int trs_core_ops_shm_sqcq_msg_fill(struct trs_id_inst *inst, u8 *msg, u32 len)
{
    struct trs_shm_sqcq_mbox *tmp_msg = (struct trs_shm_sqcq_mbox *)msg;

    tmp_msg->app_type = TRS_MBOX_SEND_FROM_DEVICE;
    tmp_msg->sq_cq_side = (0 << TRS_CHAN_SQ_MEM_OFFSET) | (0 << TRS_CHAN_CQ_MEM_OFFSET);
    tmp_msg->fid = 0;

    if (tmp_msg->header.cmd_type == TRS_MBOX_SHM_SQCQ_FREE) {
        return 0;
    }

    return trs_chan_ops_get_hw_irq(inst, TS_CQ_UPDATE_IRQ, tmp_msg->cq_irq, &tmp_msg->cq_irq);
}

static int trs_core_ops_sqcq_msg_fill(struct trs_id_inst *inst, u8 *msg, u32 len)
{
    struct trs_normal_cqsq_mailbox *tmp_msg = (struct trs_normal_cqsq_mailbox *)msg;
    struct trs_chan_type types = {.type = CHAN_TYPE_HW, .sub_type = CHAN_SUB_TYPE_HW_RTS};

    tmp_msg->app_type = TRS_MBOX_SEND_FROM_HOST;
    tmp_msg->sq_cq_side = (u8)((trs_soc_get_sq_mem_side(inst, &types) << TRS_CHAN_SQ_MEM_OFFSET) |
                               (trs_soc_get_cq_mem_side(inst) << TRS_CHAN_CQ_MEM_OFFSET));
    tmp_msg->fid = 0;

    if ((tmp_msg->header.cmd_type == TRS_MBOX_RELEASE_CQSQ_CALC) || (tmp_msg->cq_irq == 0)) { /* free or reuse cq */
        return 0;
    } else {
        u32 cq_irq;
        int ret = trs_chan_ops_get_hw_irq(inst, TS_CQ_UPDATE_IRQ, (u32)tmp_msg->cq_irq, &cq_irq);
        tmp_msg->cq_irq = (u16)cq_irq;
        return ret;
    }
}

static int trs_core_ops_logic_cq_alloc_msg_fill(struct trs_id_inst *inst, u8 *msg, u32 len)
{
    struct trs_logic_cq_mbox *tmp_msg = (struct trs_logic_cq_mbox *)msg;
    u32 cq_irq;
    int ret = trs_chan_ops_get_hw_irq(inst, TS_CQ_UPDATE_IRQ, (u32)tmp_msg->mb_alloc.cq_irq, &cq_irq);

    tmp_msg->mb_alloc.cq_irq = (u16)cq_irq;
    tmp_msg->mb_alloc.app_flag = TRS_MBOX_SEND_FROM_HOST;
    tmp_msg->mb_alloc.vfid = 0;

    return ret;
}

static int trs_core_ops_logic_cq_free_msg_fill(struct trs_id_inst *inst, u8 *msg, u32 len)
{
    struct trs_logic_cq_mbox *tmp_msg = (struct trs_logic_cq_mbox *)msg;

    tmp_msg->mb_free.vfid = 0;

    return 0;
}

static int trs_core_ops_recycle_check_msg_fill(struct trs_id_inst *inst, u8 *msg, u32 len)
{
    struct recycle_proc_msg *tmp_msg = (struct recycle_proc_msg *)msg;

    tmp_msg->proc_info.plat_type = TRS_MBOX_SEND_FROM_HOST;
    tmp_msg->proc_info.fid = 0;
    return 0;
}

static int trs_core_ops_res_map_msg_fill(struct trs_id_inst *inst, u8 *msg, u32 len)
{
    struct trs_res_map_msg *tmp_msg = (struct trs_res_map_msg *)msg;

    tmp_msg->vf_id = 0;
    tmp_msg->phy_id = tmp_msg->id;

    return 0;
}

static int trs_core_ops_reset_notify(struct trs_id_inst *inst, u8 *msg, u32 len)
{
    struct trs_notify_msg *tmp_msg = (struct trs_notify_msg *)msg;

    tmp_msg->fid = 0;
    tmp_msg->plat_type = TRS_MBOX_SEND_FROM_HOST;

    return 0;
}
#endif

static int (*const trs_core_ops_fill_mbox_msg[TRS_MBOX_CMD_MAX])(struct trs_id_inst *inst, u8 *msg, u32 len) = {
#ifndef EMU_ST
    [TRS_MBOX_CREATE_CB_CQ] = trs_core_ops_cbcq_msg_fill,
    [TRS_MBOX_RELEASE_CB_CQ] = trs_core_ops_cbcq_msg_fill,
    [TRS_MBOX_SHM_SQCQ_ALLOC] = trs_core_ops_shm_sqcq_msg_fill,
    [TRS_MBOX_SHM_SQCQ_FREE] = trs_core_ops_shm_sqcq_msg_fill,
    [TRS_MBOX_CREATE_CQSQ_CALC] = trs_core_ops_sqcq_msg_fill,
    [TRS_MBOX_RELEASE_CQSQ_CALC] = trs_core_ops_sqcq_msg_fill,
    [TRS_MBOX_LOGIC_CQ_ALLOC] = trs_core_ops_logic_cq_alloc_msg_fill,
    [TRS_MBOX_LOGIC_CQ_FREE] = trs_core_ops_logic_cq_free_msg_fill,
    [TRS_MBOX_RECYCLE_PID] = trs_core_ops_recycle_check_msg_fill,
    [TRS_MBOX_RECYCLE_CHECK] = trs_core_ops_recycle_check_msg_fill,
    [TRS_MBOX_RES_MAP] = trs_core_ops_res_map_msg_fill,
    [TRS_MBOX_RESET_NOTIFY] = trs_core_ops_reset_notify
#endif
};

int trs_core_ops_notice_ts(struct trs_id_inst *inst, u8 *msg, u32 len)
{
    struct trs_mb_header *header = (struct trs_mb_header *)msg;
    int ret;

    if ((header->cmd_type < TRS_MBOX_CMD_MAX) && (trs_core_ops_fill_mbox_msg[header->cmd_type] != NULL)) {
        ret = trs_core_ops_fill_mbox_msg[header->cmd_type](inst, msg, len);
        if (ret != 0) {
            return ret;
        }
    }
    return trs_mbox_send(inst, 0, (void *)msg, (size_t)len, 3000); /* mbox timeout: 3000 ms */
}

int trs_core_ops_send_ctrl_msg(struct trs_id_inst *inst, u8 *msg, u32 len)
{
    struct trs_rpc_call_msg *rpc_call_msg = (struct trs_rpc_call_msg *)msg;

    rpc_call_msg->rpc_call_header.app_flag = TRS_MBOX_SEND_FROM_HOST;
    rpc_call_msg->rpc_call_header.vfid = 0;

    return trs_mbox_send_rpc_call_msg(inst, 0, (void *)msg, (size_t)len, 3000); /* mbox timeout: 3000 ms */
}
