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
#include "trs_mbox.h"

#include "trs_sec_eh_msg.h"
#include "trs_sec_eh_id.h"
#include "trs_sec_eh_sq.h"
#include "trs_sec_eh_mbox.h"
#include "trs_sec_eh_cfg.h"
#include "trs_sec_eh_agent_msg.h"

static int trs_sec_eh_ver_nego(u32 dev_id, struct vmng_rx_msg_proc_info *proc_info)
{
    struct trs_sec_eh_ver_nego_info *ver_nego = (struct trs_sec_eh_ver_nego_info *)proc_info->data;

    *(proc_info->real_out_len) = sizeof(struct trs_sec_eh_ver_nego_info);

    if ((proc_info->in_data_len != sizeof(struct trs_sec_eh_ver_nego_info)) ||
        (proc_info->out_data_len != sizeof(struct trs_sec_eh_ver_nego_info))) {
        trs_err("Check failed. (in_len=%u; out_len=%u)\n", proc_info->in_data_len, proc_info->out_data_len);
        return -EINVAL;
    }

    return (ver_nego->ver == TRS_SEC_EH_VER) ? 0 : -EINVAL;
}

static bool trs_sec_eh_mb_not_support(u16 cmd_type)
{
    return (cmd_type == TRS_MBOX_MEM_DISPATCH) || (cmd_type == TRS_MBOX_SQ_SWITCH_STREAM);
}

static int trs_sec_eh_mb_send(u32 devid, struct vmng_rx_msg_proc_info *proc_info)
{
    struct trs_sec_eh_mbox_info *mb = (struct trs_sec_eh_mbox_info *)proc_info->data;
    struct trs_mb_header *mb_head = (struct trs_mb_header *)mb->data;
    struct trs_id_inst inst;
    int ret;

    *(proc_info->real_out_len) = sizeof(struct trs_sec_eh_mbox_info);

    if ((proc_info->in_data_len != sizeof(struct trs_sec_eh_mbox_info)) ||
        (proc_info->out_data_len != sizeof(struct trs_sec_eh_mbox_info))) {
        trs_err("Check failed. (in_len=%u; out_len=%u)\n", proc_info->in_data_len, proc_info->out_data_len);
        return -EINVAL;
    }

    trs_id_inst_pack(&inst, devid, mb->head.tsid);
    if ((mb_head->cmd_type == TRS_MBOX_RPC_CALL) || (mb_head->cmd_type == TRS_MBOX_DSMI_RPC_CALL)) {
        return trs_mbox_send_rpc_call_msg(&inst, 0, (void *)mb->data, sizeof(struct trs_normal_cqsq_mailbox),
                                          TRS_DEVICE_CHAN_MBOX_TIMEOUT_MS);
    }

    if (trs_sec_eh_mb_not_support(mb_head->cmd_type)) {
        return -ENOTSUPP;
    }

    ret = trs_sec_eh_mb_update(&inst, mb_head->cmd_type, (void *)mb->data);
    if (ret != 0) {
        trs_err("Mb update fail. (devid=%u; tsid=%u; cmd=%u; ret=%d)\n", inst.devid, inst.tsid, mb_head->cmd_type, ret);
        return ret;
    }

    ret = trs_mbox_send(&inst, 0, (void *)mb->data, sizeof(struct trs_normal_cqsq_mailbox),
                        TRS_DEVICE_CHAN_MBOX_TIMEOUT_MS);
    if (ret != 0) {
        trs_err("Mbox send fail. (devid=%u; tsid=%u; cmd=%u; ret=%d)\n", inst.devid, inst.tsid, mb_head->cmd_type, ret);
    }
    if ((mb_head->cmd_type == TRS_MBOX_RECYCLE_CHECK) && (mb_head->result != 0)) {
        ret = (int)mb_head->result;
    }

    return ret;
}

static int trs_sec_eh_sqe_update(u32 devid, struct vmng_rx_msg_proc_info *proc_info)
{
    struct trs_sec_eh_sq_update_info *info = (struct trs_sec_eh_sq_update_info *)proc_info->data;
    const size_t data_size = 256; /* 256 bytes is max size */
    struct trs_id_inst inst;
    int ret = -EINVAL;
    u32 data_len;

    if (info->sqe_num > data_size / TRS_HW_SQE_SIZE) {
        return -EINVAL;
    }
    data_len = sizeof(struct trs_sec_eh_sq_update_info) - data_size + info->sqe_num * TRS_HW_SQE_SIZE;
    *(proc_info->real_out_len) = data_len;

    if ((proc_info->in_data_len != data_len) || (proc_info->out_data_len != data_len)) {
        trs_err("Check failed. (in_len=%u; out_len=%u; data_len=%u)\n", proc_info->in_data_len, proc_info->out_data_len,
                data_len);
        return -EINVAL;
    }

    trs_id_inst_pack(&inst, devid, info->head.tsid);
    ret = _trs_sec_eh_sqe_update(&inst, info->pid, info->sqid, info->first_sqeid, info->data);
    if (ret != 0) {
        trs_err("Update failed. (devid=%u; sqid=%u; sqeid=%u)\n", devid, info->sqid, info->first_sqeid);
    }

    return ret;
}

static int trs_sec_eh_res_ctrl(u32 devid, struct vmng_rx_msg_proc_info *proc_info)
{
    struct trs_sec_eh_res_ctrl_info *info = (struct trs_sec_eh_res_ctrl_info *)proc_info->data;
    struct trs_id_inst inst;
    int ret = -EINVAL;

    *(proc_info->real_out_len) = sizeof(struct trs_sec_eh_res_ctrl_info);
    if ((proc_info->in_data_len != sizeof(struct trs_sec_eh_res_ctrl_info)) ||
        (proc_info->out_data_len != sizeof(struct trs_sec_eh_res_ctrl_info))) {
        trs_err("Check failed. (in_len=%u; out_len=%u)\n", proc_info->in_data_len, proc_info->out_data_len);
        return -EINVAL;
    }

    trs_id_inst_pack(&inst, devid, info->head.tsid);
    ret = _trs_sec_eh_res_ctrl(&inst, info->id_type, info->id, info->cmd);
    if (ret != 0) {
        trs_err("Res ctrl failed. (devid=%u; id_type=%u; id=%u; cmd=%u)\n", devid, info->id_type, info->id, info->cmd);
    }

    return ret;
}

static const sec_eh_vpc_func sec_eh_vpc_handlers[TRS_SEC_EH_MAX] = {
    [TRS_SEC_EH_VER_NEGO] = trs_sec_eh_ver_nego,
    [TRS_SEC_EH_MB_SEND] = trs_sec_eh_mb_send,
    [TRS_SEC_EH_SQE_UPDATE] = trs_sec_eh_sqe_update,
    [TRS_SEC_EH_RES_CTRL] = trs_sec_eh_res_ctrl,
};

sec_eh_vpc_func trs_sec_eh_get_vpc_func(u32 cmd)
{
    return sec_eh_vpc_handlers[cmd];
}
