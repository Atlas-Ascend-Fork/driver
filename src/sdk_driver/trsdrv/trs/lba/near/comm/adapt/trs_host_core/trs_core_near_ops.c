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
#include "ascend_hal_define.h"
#include "trs_core.h"
#include "soc_adapt.h"
#include "trs_host_msg.h"
#include "trs_id.h"
#include "trs_ts_status.h"
#include "trs_host_comm.h"
#include "trs_core_near_ops.h"

int trs_core_ops_get_proc_num(struct trs_id_inst *inst, u32 *proc_num)
{
    struct trs_msg_proc_num *proc_num_msg = NULL;
    struct trs_msg_data msg = {0};
    int ret;

    msg.header.tsid = inst->tsid;
    msg.header.valid = TRS_MSG_SEND_MAGIC;
    msg.header.cmdtype = TRS_MSG_GET_PROC_NUM;
    msg.header.result = 0;

    ret = trs_host_msg_send(inst->devid, &msg, sizeof(struct trs_msg_data));
    if ((ret != 0) || (msg.header.result != 0)) {
        trs_err("Msg send fail. (devid=%u; tsid=%u; ret=%d; result=%d)\n", inst->devid, inst->tsid, ret,
                msg.header.result);
        return -ENODEV;
    }
    proc_num_msg = (struct trs_msg_proc_num *)msg.payload;
    *proc_num = proc_num_msg->proc_num;
    return 0;
}

int trs_core_ops_get_res_reg_offset(struct trs_id_inst *inst, int type, u32 id, u32 *offset)
{
    if (type == TRS_NOTIFY_ID) {
        return trs_soc_get_notify_offset(inst, id, offset);
    }
    if (type == TRS_EVENT_ID) {
        return trs_soc_get_event_offset(inst, id, offset);
    }
    return -ENODEV;
}

static int trs_core_ops_get_notify_reg_total_size(struct trs_id_inst *inst, u32 *total_size)
{
    size_t notify_size;
    u32 start, end;
    int ret;

    ret = trs_id_get_range(inst, TRS_NOTIFY_ID, &start, &end);
    if (ret != 0) {
        return ret;
    }

    ret = trs_soc_get_notify_size(inst, &notify_size);
    if (ret != 0) {
        return ret;
    }

    *total_size = (end - start) * notify_size;
    return 0;
}

int trs_core_ops_get_res_reg_total_size(struct trs_id_inst *inst, int type, u32 *total_size)
{
    if (type == TRS_NOTIFY_ID) {
        return trs_core_ops_get_notify_reg_total_size(inst, total_size);
    }

    return -ENODEV;
}

int trs_core_ops_get_ts_inst_status(struct trs_id_inst *inst, u32 *status)
{
    return trs_get_ts_status(inst, status);
}

static int res_addr_type_to_id_type[RES_ADDR_TYPE_MAX] = {[RES_ADDR_TYPE_STARS_NOTIFY_RECORD] = TRS_NOTIFY,
                                                          [RES_ADDR_TYPE_STARS_CNT_NOTIFY_RECORD] = TRS_CNT_NOTIFY,
                                                          [RES_ADDR_TYPE_STARS_RTSQ] = TRS_HW_SQ};

bool trs_host_res_is_belong_to_proc(int master_tgid, int slave_tgid, u32 udevid, struct res_map_info_in *res_info)
{
    struct trs_id_inst inst;
    u32 res_id = res_info->res_id & 0xFFFFF; /* 0xFFFFF: first 20bits for res id */

    trs_id_inst_pack(&inst, udevid, res_info->id);
    if (trs_res_is_belong_to_proc(&inst, master_tgid, res_addr_type_to_id_type[res_info->res_type], res_id)) {
        trs_debug("Res id belong to master process. (master_tgid=%d; slave_tgid=%d; type=%d; id=%d)\n", master_tgid,
                  slave_tgid, res_info->res_type, res_id);
        return true;
    }

    if (res_info->priv != NULL) {
        struct trs_id_inst remote_inst;
        struct trs_res_map_priv *priv = (struct trs_res_map_priv *)res_info->priv;

        if (res_info->priv_len < sizeof(struct trs_res_map_priv)) {
            return false;
        }

        trs_id_inst_pack(&remote_inst, priv->remote_devid, res_info->id);
        if ((priv->flag & TSDRV_FLAG_REMOTE_ID) != 0) {
            if (trs_res_is_belong_to_proc(&remote_inst, master_tgid, res_addr_type_to_id_type[res_info->res_type],
                                          res_id)) {
                trs_debug("Res id belong to remote process. (master_tgid=%d; slave_tgid=%d; type=%d; id=%d)\n",
                          master_tgid, slave_tgid, res_info->res_type, res_id);
                return true;
            }
        }
    }
#ifndef EMU_ST
    /* res id belongs to cp1 in MC2 */
    if (trs_host_res_id_check(&inst, res_addr_type_to_id_type[res_info->res_type], res_id) == 0) {
        trs_debug("Res id belong to cp1 process. (master_tgid=%d; slave_tgid=%d; type=%d; id=%d)\n", master_tgid,
                  slave_tgid, res_info->res_type, res_id);
        return true;
    }
    trs_err("Res id not belong to master process. (master_tgid=%d; slave_tgid=%d; type=%d; id=%d)\n", master_tgid,
            slave_tgid, res_info->res_type, res_id);
    return false;
#endif
}

void *trs_core_ops_cq_mem_alloc(struct trs_id_inst *inst, size_t size)
{
    return trs_vzalloc(size);
}

void trs_core_ops_cq_mem_free(struct trs_id_inst *inst, void *vaddr, size_t size)
{
    trs_vfree(vaddr);
}

int trs_core_ops_mem_update(struct trs_id_inst *inst, u64 in_addr, u64 *out_addr, int flag)
{
    int ret;
    if (flag == 0) {
        ret = devdrv_devmem_addr_d2h(inst->devid, in_addr, (phys_addr_t *)out_addr);
        if (ret != 0) {
            trs_err("Failed to get bar. (ret=%d; devid=%u)\n", ret, inst->devid);
        }
    } else {
        ret = devdrv_devmem_addr_h2d(inst->devid, in_addr, (phys_addr_t *)out_addr);
        if (ret != 0) {
            trs_err("Failed to get pa. (ret=%d; devid=%u)\n", ret, inst->devid);
        }
    }
    return ret;
}
