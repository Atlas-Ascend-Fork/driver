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
#include "soc_adapt.h"
#include "trs_mailbox_def.h"
#include "trs_host_comm.h"
#include "trs_stars_cq.h"
#include "trs_stars_sq.h"
#include "trs_stars.h"
#include "trs_ts_db.h"
#include "trs_id.h"
#include "trs_pm_adapt.h"
#include "trs_core_near_ops_mbox.h"
#include "trs_core_near_ops.h"
#include "trs_host_msg.h"
#include "trs_core_stars_v1_ops.h"

int trs_core_ops_get_sq_id_head_from_hw_cqe(struct trs_id_inst *inst, void *hw_cqe, u32 *sqid, u32 *sq_head)
{
    trs_stars_cqe_get_sqid(inst, hw_cqe, sqid);
    trs_stars_cqe_get_sq_head(inst, hw_cqe, sq_head);
    return 0;
}

int trs_core_ops_get_stream_from_cqe(struct trs_id_inst *inst, void *hw_cqe, u32 *stream_id)
{
    return trs_stars_cqe_get_streamid(inst, hw_cqe, stream_id);
}

int trs_core_ops_cqe_to_logic_cqe(struct trs_id_inst *inst, void *hw_cqe, struct trs_logic_cqe *logic_cqe)
{
    trs_stars_cqe_to_logic_cqe(hw_cqe, logic_cqe);
    return 0;
}

static int trs_stars_core_ops_notice_ts(struct trs_id_inst *inst, u8 *msg, u32 len)
{
    struct trs_mb_header *header = (struct trs_mb_header *)msg;

    /* stars not need res map */
    if (header->cmd_type == TRS_MBOX_RES_MAP) {
        return 0;
    }

    return trs_core_ops_notice_ts(inst, msg, len);
}

static int trs_core_ops_get_sq_trigger_irq(struct trs_id_inst *inst, u32 *irq, u32 *irq_type)
{
    struct res_inst_info res_inst;
    *irq_type = TS_SQ_SEND_TRIGGER_IRQ;
    soc_resmng_inst_pack(&res_inst, inst->devid, TS_SUBSYS, inst->tsid);
    return soc_resmng_get_irq_by_index(&res_inst, TS_SQ_SEND_TRIGGER_IRQ, 0, irq);
}

static int trs_core_ops_get_trigger_sqid(struct trs_id_inst *inst, u32 *sqid)
{
    return trs_get_ts_db_val(inst, TRS_DB_TRIGGER_SQ, 0, sqid);
}

int trs_core_ops_sqcq_reg_map(struct trs_id_inst *inst, struct trs_sqcq_reg_map_para *para)
{
    struct trs_msg_sqcq_sync *msg_sqcq_sync = NULL;
    struct trs_msg_data msg;
    int ret;

    msg.header.cmdtype = TRS_MSG_SQ_REG_MAP;
    msg.header.valid = TRS_MSG_SEND_MAGIC;
    msg.header.tsid = inst->tsid;
    msg_sqcq_sync = (struct trs_msg_sqcq_sync *)msg.payload;
    msg_sqcq_sync->remote_tgid = para->host_pid;
    msg_sqcq_sync->bind_sqcq.stream_id = para->stream_id;
    msg_sqcq_sync->sq_id = para->sqid;
    msg_sqcq_sync->cq_id = para->cqid;
    ret = trs_host_msg_send(inst->devid, &msg, sizeof(struct trs_msg_data));
    if ((ret != 0) && (ret != -ESRCH)) {
        trs_err("Msg send fail. (devid=%u; ret=%d)\n", inst->devid, ret);
        return ret;
    }
    (void)trs_set_sq_reg_vaddr(inst, para->sqid, msg_sqcq_sync->sq_map.va, msg_sqcq_sync->sq_map.size);
    return ret;
}

int trs_core_ops_sqcq_reg_unmap(struct trs_id_inst *inst, struct trs_sqcq_reg_map_para *para)
{
    struct trs_msg_sqcq_sync *msg_sqcq_sync = NULL;
    struct trs_msg_data msg;
    int ret;

    msg.header.cmdtype = TRS_MSG_SQ_REG_UNMAP;
    msg.header.valid = TRS_MSG_SEND_MAGIC;
    msg.header.tsid = inst->tsid;
    msg_sqcq_sync = (struct trs_msg_sqcq_sync *)msg.payload;
    msg_sqcq_sync->remote_tgid = para->host_pid;
    msg_sqcq_sync->bind_sqcq.stream_id = para->stream_id;
    msg_sqcq_sync->sq_id = para->sqid;
    msg_sqcq_sync->cq_id = para->cqid;
    ret = trs_host_msg_send(inst->devid, &msg, sizeof(struct trs_msg_data));
    if (ret != 0) {
        trs_err("Msg send fail. (devid=%u; ret=%d)\n", inst->devid, ret);
        return ret;
    }
    (void)trs_set_sq_reg_vaddr(inst, para->sqid, 0, 0);
    return ret;
}

static bool trs_core_ops_is_drop_cqe(struct trs_id_inst *inst, struct trs_logic_cqe *logic_cqe)
{
    return (logic_cqe->drop_flag == 1);
}

static int trs_core_stars_v1_res_id_remote_get(struct trs_id_inst *inst, int type, u32 id, u32 hpid, u32 seq)
{
    (void)hpid;
    (void)seq;
    return trs_host_res_id_check(inst, type, id);
}

static struct trs_core_adapt_ops trs_core_stars_v1_ops = {
    .owner = KA_THIS_MODULE,
    .cq_mem_alloc = trs_core_ops_cq_mem_alloc,
    .cq_mem_free = trs_core_ops_cq_mem_free,
    .ssid_query = trs_host_get_ssid,
    .res_id_check = trs_host_res_id_check,
    .res_id_remote_get = trs_core_stars_v1_res_id_remote_get,
    .res_id_remote_put = NULL,
    .get_res_support_proc_num = trs_core_ops_get_proc_num,
    .get_sq_id_head_from_hw_cqe = trs_core_ops_get_sq_id_head_from_hw_cqe,
    .get_stream_id_from_hw_cqe = trs_core_ops_get_stream_from_cqe,
    .hw_cqe_to_logic_cqe = trs_core_ops_cqe_to_logic_cqe,
    .is_drop_cqe = trs_core_ops_is_drop_cqe,
    .notice_ts = trs_stars_core_ops_notice_ts,
    .sqcq_reg_map = trs_core_ops_sqcq_reg_map,
    .sqcq_reg_unmap = trs_core_ops_sqcq_reg_unmap,
    .get_res_reg_offset = trs_core_ops_get_res_reg_offset,
    .get_res_reg_total_size = trs_core_ops_get_res_reg_total_size,
    .get_sq_trigger_irq = trs_core_ops_get_sq_trigger_irq,
    .get_trigger_sqid = trs_core_ops_get_trigger_sqid,
    .request_irq = trs_adapt_ops_request_irq,
    .free_irq = trs_adapt_ops_free_irq,
    .get_ts_inst_status = trs_core_ops_get_ts_inst_status,
    .get_connect_protocol = trs_host_get_connect_protocol,
    .get_host_mach_flag = trs_get_host_mach_flag,
    .trace_cqe_fill = trs_stars_trace_cqe_fill,
    .trace_sqe_fill = trs_stars_trace_sqe_fill,
    .ts_rpc_call = trs_core_ops_send_ctrl_msg,
    .ras_report = trs_host_ras_report,
    .mem_update = trs_core_ops_mem_update,
    .res_num_query = trs_host_res_num_query,
    .get_sq_mem_handle = trs_host_get_sq_mem_handle,
    .put_sq_mem_handle = trs_host_put_sq_mem_handle,
};

struct trs_core_adapt_ops *trs_core_get_stars_v1_adapt_ops(void)
{
    return &trs_core_stars_v1_ops;
}

static int trs_sq_trigger_msg_send(struct trs_id_inst *inst, u32 db, u32 hwirq)
{
    struct trs_sq_trigger_msg msg;
    int ret;

    trs_mbox_init_header(&msg.header, TRS_MBOX_NOTICE_SQ_TRIGGER);
    msg.db = db;
    msg.irq = hwirq;
    msg.vfid = 0;

    ret = trs_mbox_send(inst, 0, &msg, sizeof(struct trs_sq_trigger_msg), 3000); /* timeout 3000 ms */
    if ((ret != 0) || (msg.header.result != 0)) {
        trs_err("Mbox send fail. (ret=%d; result=%d)\n", ret, msg.header.result);
        return -EFAULT;
    }

    return 0;
}

int trs_sq_send_trigger_db_init(struct trs_id_inst *inst)
{
    struct res_inst_info res_inst;
    u32 start, end, irq, hwirq;
    int ret;

    ret = trs_soc_get_db_cfg(inst, TRS_DB_TRIGGER_SQ, &start, &end);
    if ((ret != 0) || (start >= end)) {
        trs_err("Trs get db cfg fail. (devid=%u; tsid=%u; ret=%d; start=%u; end=%u)\n", inst->devid, inst->tsid, ret,
                start, end);
        return -ENODEV;
    }

    ret = trs_ts_db_init(inst, TRS_DB_TRIGGER_SQ, start, end);
    if (ret != 0) {
        trs_err("Trs db init fail. (devid=%u; tsid=%u; ret=%d)\n", inst->devid, inst->tsid, ret);
        return ret;
    }

    soc_resmng_inst_pack(&res_inst, inst->devid, TS_SUBSYS, inst->tsid);
    ret = soc_resmng_get_irq_by_index(&res_inst, TS_SQ_SEND_TRIGGER_IRQ, 0, &irq);
    ret |= soc_resmng_get_hwirq(&res_inst, TS_SQ_SEND_TRIGGER_IRQ, irq, &hwirq);
    if (ret != 0) {
        trs_ts_db_uninit(inst, TRS_DB_TRIGGER_SQ);
        trs_err("Get irq fail. (devid=%u; tsid=%u; ret=%d)\n", inst->devid, inst->tsid, ret);
        return ret;
    }

    ret = trs_sq_trigger_msg_send(inst, start, hwirq);
    if (ret != 0) {
        trs_ts_db_uninit(inst, TRS_DB_TRIGGER_SQ);
        return ret;
    }

    return 0;
}

void trs_sq_send_trigger_db_uninit(struct trs_id_inst *inst)
{
    trs_ts_db_uninit(inst, TRS_DB_TRIGGER_SQ);
}
