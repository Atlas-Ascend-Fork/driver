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
#include "ka_system_pub.h"

#include "securec.h"
#include "vmng_kernel_interface.h"
#include "trs_sec_eh_msg.h"
#include "trs_chan_update.h"
#include "trs_sec_eh_vpc.h"
#include "trs_mailbox_def.h"
#include "trs_sec_eh_auto_init.h"

static int trs_vpc_msg_recv(u32 devid, u32 fid, struct vmng_rx_msg_proc_info *proc_info)
{
    return -EPERM;
}

static struct vmng_vpc_client trs_vpc_client = {
    .vpc_type = VMNG_VPC_TYPE_TSDRV,
    .init = NULL,
    .msg_recv = trs_vpc_msg_recv,
};

int trs_sec_eh_vpc_init(u32 ts_inst_id)
{
    struct trs_id_inst inst;

    trs_ts_inst_to_id_inst(ts_inst_id, &inst);
    if (inst.tsid > 0) {
        return 0;
    }
    return vmnga_vpc_register_client(inst.devid, &trs_vpc_client);
}
DECLAER_FEATURE_AUTO_INIT_DEV(trs_sec_eh_vpc_init, FEATURE_LOADER_STAGE_1);

void trs_sec_eh_vpc_uninit(u32 ts_inst_id)
{
    struct trs_id_inst inst;

    trs_ts_inst_to_id_inst(ts_inst_id, &inst);
    if (inst.tsid == 0) {
        (void)vmnga_vpc_unregister_client(inst.devid, &trs_vpc_client);
    }
}
DECLAER_FEATURE_AUTO_UNINIT_DEV(trs_sec_eh_vpc_uninit, FEATURE_LOADER_STAGE_1);

int trs_sec_eh_vpc_msg_send(u32 devid, void *msg, size_t size)
{
    struct vmng_tx_msg_proc_info tx_msg = {.data = msg, .in_data_len = size, .out_data_len = size, .real_out_len = 0};
    int retry = 100; /* retry 100 times */
    int ret;

    do {
        ret = vpc_msg_send(devid, VPC_VM_FID, VMNG_VPC_TYPE_TSDRV, &tx_msg, VPC_DEFAULT_TIMEOUT);
        if (ret != -ENOSPC) {
            break;
        }
        ka_system_usleep_range(100, 200); /* 100 us ~ 200 us */
    } while (--retry >= 0);

    return ret;
}

static void trs_sec_eh_vpc_mbox_msg_pos_proc(int cmd_type, void *vpc_data, void *msg)
{
    if ((cmd_type == TRS_MBOX_RPC_CALL) || (cmd_type == TRS_MBOX_DSMI_RPC_CALL)) {
        *((struct trs_rpc_call_msg *)msg) = *((struct trs_rpc_call_msg *)vpc_data);
    }
}

static int trs_sec_eh_vpc_mbox_msg_send(struct trs_id_inst *inst, void *msg, size_t size)
{
    struct trs_sec_eh_mbox_info vpc_mbox = {0};
    struct trs_mb_header *mb_head = (struct trs_mb_header *)vpc_mbox.data;
    int ret = 0;

    vpc_mbox.head.cmd_type = TRS_SEC_EH_MB_SEND;
    vpc_mbox.head.result = 0;
    vpc_mbox.head.tsid = inst->tsid;
    vpc_mbox.head.rsv = 0;
    ret = memcpy_s((void *)vpc_mbox.data, 64, (const void *)msg, size); /* 64 bytes for mbox length */
    if (ret != EOK) {
        trs_err("Memcpy fail. (ret=%d; size=%lu)\n", ret, size);
        return ret;
    }
    ret = trs_sec_eh_vpc_msg_send(inst->devid, (void *)&vpc_mbox, sizeof(struct trs_sec_eh_mbox_info));
    if ((ret != 0) || (vpc_mbox.head.result != 0)) {
#ifndef EMU_ST
        /* don't chang loge level. app_exit_check may enter */
        trs_debug("Vpc send fail. (devid=%u; ret=%d; result=%d)\n", inst->devid, ret, vpc_mbox.head.result);
        ret = -EFAULT;
#endif
    }

    if ((mb_head->cmd_type == TRS_MBOX_RECYCLE_PID) && (mb_head->result != 0)) {
        ((struct trs_mb_header *)msg)->result = mb_head->result;
    }

    trs_sec_eh_vpc_mbox_msg_pos_proc(mb_head->cmd_type, (void *)vpc_mbox.data, msg);

    return ret;
}

static bool trs_is_mb_update_only(void *data)
{
    struct trs_mb_header *header = (struct trs_mb_header *)data;

    if (header->cmd_type == TRS_MBOX_FREE_STREAM) {
        return true;
    }
    return false;
}

int trs_mbox_send(struct trs_id_inst *inst, u32 chan_id, void *data, size_t size, int timeout)
{
    int ret;

    ret = trs_mb_update(inst, (int)ka_task_get_current_tgid(), data, (u32)size);
    if (ret != 0) {
        return ret;
    }

    if (trs_is_mb_update_only(data)) {
        return 0;
    }

    return trs_sec_eh_vpc_mbox_msg_send(inst, data, size);
}

int trs_mbox_send_ex(struct trs_id_inst *inst, u32 chan_id, void *data, size_t size, int timeout)
{
    return trs_mbox_send(inst, chan_id, data, size, timeout);
}

int trs_mbox_send_rpc_call_msg(struct trs_id_inst *inst, u32 chan_id, void *data, size_t size, int timeout)
{
    return trs_sec_eh_vpc_mbox_msg_send(inst, data, size);
}
