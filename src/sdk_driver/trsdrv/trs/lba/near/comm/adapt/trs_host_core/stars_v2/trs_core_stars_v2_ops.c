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

#include "ascend_hal_define.h"
#include "pbl/pbl_soc_res.h"
#include "soc_adapt.h"
#include "dpa_kernel_interface.h"
#include "comm_kernel_interface.h"
#include "trs_mailbox_def.h"
#include "trs_host_comm.h"
#include "trs_stars_v2_cq.h"
#include "trs_stars_v2_sq.h"
#include "trs_stars.h"
#include "trs_ts_db.h"
#include "trs_id.h"
#include "trs_pm_adapt.h"
#include "trs_core_near_ops_mbox.h"
#include "trs_core_near_ops.h"
#include "trs_host_msg.h"
#include "trs_id_range.h"
#include "trs_core_stars_v2_ops.h"
#include "trs_ub_info.h"
#include "trs_host_mode_config.h"

int trs_core_stars_v2_ops_get_sq_id_head_from_hw_cqe(struct trs_id_inst *inst, void *hw_cqe, u32 *sqid, u32 *sq_head)
{
#ifndef EMU_ST
    trs_stars_v2_cqe_get_sqid(inst, hw_cqe, sqid);
    trs_stars_v2_cqe_get_sq_head(inst, hw_cqe, sq_head);
    return 0;
#endif
}

int trs_core_stars_v2_ops_get_stream_from_cqe(struct trs_id_inst *inst, void *hw_cqe, u32 *stream_id)
{
    return trs_stars_v2_cqe_get_streamid(inst, hw_cqe, stream_id);
}

int trs_core_stars_v2_ops_cqe_to_logic_cqe(struct trs_id_inst *inst, void *hw_cqe, struct trs_logic_cqe *logic_cqe)
{
#ifndef EMU_ST
    trs_stars_v2_cqe_to_logic_cqe(hw_cqe, logic_cqe);
    return 0;
#endif
}

static int trs_core_stars_v2_ops_notice_ts(struct trs_id_inst *inst, u8 *msg, u32 len)
{
    struct trs_mb_header *header = (struct trs_mb_header *)msg;

    if ((header->cmd_type == TRS_MBOX_RES_MAP) || (header->cmd_type == TRS_MBOX_QUERY_SQ_STATUS) ||
        (header->cmd_type == TRS_MBOX_FREE_STREAM) || (header->cmd_type == TRS_MBOX_RESET_NOTIFY)) {
        return 0;
    }

    return trs_core_ops_notice_ts(inst, msg, len);
}

static int trs_core_stars_v2_id_alloc(struct trs_id_inst *inst, int type, u32 flag, u32 *id, u32 para)
{
    if ((flag & TSDRV_RES_RANGE_ID) != 0) {
        return trs_id_alloc_in_range(inst, type, id, para);
    }
    return trs_id_alloc_ex(inst, type, flag, id, 1);
}

static bool trs_core_stars_v2_ops_is_drop_cqe(struct trs_id_inst *inst, struct trs_logic_cqe *logic_cqe)
{
    return false;
}

static int trs_core_stars_v2_ops_notice_proc_release(struct trs_id_inst *inst, int pid)
{
    struct trs_msg_proc_recycle *msg_proc_recycle = NULL;
    struct trs_msg_data msg;
    int ret;

    if (devdrv_get_connect_protocol(inst->devid) != CONNECT_PROTOCOL_UB) {
        return 0;
    }

    msg.header.cmdtype = TRS_MSG_NOTICE_PROC_RECYCLE;
    msg.header.valid = TRS_MSG_SEND_MAGIC;
    msg.header.tsid = inst->tsid;
    msg_proc_recycle = (struct trs_msg_proc_recycle *)msg.payload;
    msg_proc_recycle->pid = pid;
    ret = trs_host_msg_send(inst->devid, &msg, sizeof(struct trs_msg_data));
    if (ret != 0) {
        trs_err("Msg send fail. (devid=%u; ret=%d)\n", inst->devid, ret);
        return ret;
    }

    return ret;
}

static struct trs_core_adapt_ops trs_core_stars_v2_ops = {
    .owner = KA_THIS_MODULE,
    .cq_mem_alloc = trs_core_ops_cq_mem_alloc,
    .cq_mem_free = trs_core_ops_cq_mem_free,
    .ssid_query = trs_host_get_ssid,
    .res_id_check = trs_host_res_id_check,
    .res_id_remote_get = trs_host_res_id_get,
    .res_id_remote_put = trs_host_res_id_put,
    .get_res_support_proc_num = trs_core_ops_get_proc_num,
    .get_sq_id_head_from_hw_cqe = trs_core_stars_v2_ops_get_sq_id_head_from_hw_cqe,
    .get_stream_id_from_hw_cqe = trs_core_stars_v2_ops_get_stream_from_cqe,
    .hw_cqe_to_logic_cqe = trs_core_stars_v2_ops_cqe_to_logic_cqe,
    .is_drop_cqe = trs_core_stars_v2_ops_is_drop_cqe,
    .notice_ts = trs_core_stars_v2_ops_notice_ts,
    .sqcq_reg_map = NULL,
    .sqcq_reg_unmap = NULL,
    .get_res_reg_offset = NULL,
    .get_res_reg_total_size = NULL,
    .get_sq_trigger_irq = NULL,
    .get_trigger_sqid = NULL,
    .get_ts_inst_status = NULL,
    .get_connect_protocol = trs_host_get_connect_protocol,
    .get_host_mach_flag = trs_get_host_mach_flag,
    .id_alloc = trs_core_stars_v2_id_alloc,
    .trace_cqe_fill = trs_stars_v2_trace_cqe_fill,
    .trace_sqe_fill = trs_stars_v2_trace_sqe_fill,
    .ts_rpc_call = trs_core_ops_send_ctrl_msg,
    .ras_report = trs_host_ras_report,
    .ub_info_query = trs_ub_info_query,
    .get_sq_send_mode = trs_get_sq_send_mode,
    .notice_proc_release = trs_core_stars_v2_ops_notice_proc_release,
    .mem_update = trs_core_ops_mem_update,
    .res_num_query = trs_host_res_num_query,
    .get_sq_mem_handle = trs_host_get_sq_mem_handle,
    .put_sq_mem_handle = trs_host_put_sq_mem_handle,
};

struct trs_core_adapt_ops *trs_core_get_stars_v2_adapt_ops(void)
{
    return &trs_core_stars_v2_ops;
}
