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
#include "trs_chan_update.h"
#include "trs_host_core.h"
#include "trs_core.h"
#include "trs_chan.h"

#include "trs_sec_eh_cfg.h"
#include "trs_sec_eh_id.h"

static void trs_sec_eh_get_id_range_by_bit(struct trs_sec_eh_id_info *id_info, u32 type, u32 bit, u32 *start, u32 *end)
{
    if (!trs_id_is_local_type(type)) {
        *start = id_info->start + bit * id_info->num_per_bit;
        *end = *start + id_info->num_per_bit;
    }
    trs_debug("Get id range. (bit=%u; type=%d; start=%u; end=%u; num_per_bit=%u)\n", bit, type, *start, *end,
              id_info->num_per_bit);
}

bool trs_sec_eh_id_is_belong_to_vf(struct trs_sec_eh_id_info *id_info, u32 id)
{
    u32 start = 0;
    u32 end = 0;
    u32 bit;

    for (bit = 0; bit < id_info->bitnum; bit++) {
        if (trs_bitmap_bit_is_vaild(id_info->bitmap, bit)) {
            trs_sec_eh_get_id_range_by_bit(id_info, id_info->type, bit, &start, &end);
            if ((id >= start) && (id < end)) {
                return true;
            }
        }
    }

    return false;
}

bool trs_sec_eh_res_is_belong_to_proc(struct trs_id_inst *inst, int pid, int res_type, u32 res_id)
{
    struct trs_sec_eh_ts_inst *sec_eh_cfg = NULL;
    bool ret = false;

    if (trs_id_inst_check(inst) != 0) {
        return ret;
    }

    sec_eh_cfg = trs_sec_eh_ts_inst_get(inst);
    if (sec_eh_cfg != NULL) {
        if (trs_sec_eh_id_is_belong_to_vf(&sec_eh_cfg->id_info[res_type], res_id)) {
            ret = true;
        }
        trs_sec_eh_ts_inst_put(sec_eh_cfg);
    }

    return ret;
}

int trs_sec_eh_get_sq_info(struct trs_id_inst *inst, int res_type, u32 sqid, void *info)
{
    struct trs_sec_eh_ts_inst *sec_eh_cfg = NULL;
    struct trs_chan_sq_info *sq_info = (struct trs_chan_sq_info *)info;
    int ret = -EINVAL;

    if (trs_id_inst_check(inst) != 0) {
        return ret;
    }

    if (info == NULL) {
        return ret;
    }

    sec_eh_cfg = trs_sec_eh_ts_inst_get(inst);
    if (sec_eh_cfg == NULL) {
        return -EINVAL;
    }

    if (sqid >= sec_eh_cfg->id_info[TRS_HW_SQ].end) {
        trs_sec_eh_ts_inst_put(sec_eh_cfg);
        return -EINVAL;
    }

    sq_info->sq_phy_addr = (uintptr_t)sec_eh_cfg->sq_ctx[sqid].sq_paddr;
    sq_info->sq_para.sq_depth = sec_eh_cfg->sq_ctx[sqid].sq_depth;
    sq_info->sq_para.sqe_size = sec_eh_cfg->sq_ctx[sqid].sqe_size;
    sq_info->sq_vaddr = (void *)sec_eh_cfg->sq_ctx[sqid].d_addr;
    sq_info->sq_dev_vaddr = sec_eh_cfg->sq_ctx[sqid].sq_dev_vaddr;
    sq_info->mem_type = sec_eh_cfg->sq_ctx[sqid].mem_type;
    trs_sec_eh_ts_inst_put(sec_eh_cfg);
    trs_debug("Sq info. (devid=%u; tsid=%u; sqid=%u)\n", inst->devid, inst->tsid, sqid);

    return 0;
}

void trs_sec_eh_id_config(struct trs_id_inst *inst)
{
    struct trs_res_ops ops = {0};
    int type;

    for (type = 0; type < TRS_CORE_MAX_ID_TYPE; type++) {
        ops.res_belong_proc[type] = trs_sec_eh_res_is_belong_to_proc;
    }
    ops.res_get_info[TRS_HW_SQ] = trs_sec_eh_get_sq_info;
    trs_res_ops_register(inst->devid, &ops);
}

void trs_sec_eh_id_deconfig(struct trs_id_inst *inst)
{
    trs_res_ops_unregister(inst->devid);
}

int _trs_sec_eh_res_ctrl(struct trs_id_inst *inst, u32 id_type, u32 res_id, u32 cmd)
{
    struct trs_sec_eh_ts_inst *sec_eh_cfg = NULL;
    int ret = -EINVAL;

    if ((id_type >= TRS_CORE_MAX_ID_TYPE) || (cmd >= TRS_RES_OP_MAX)) {
        trs_err("Invalid para. (id_type=%u; cmd=%u)\n", id_type, cmd);
        return -EINVAL;
    }

    sec_eh_cfg = trs_sec_eh_ts_inst_get(inst);
    if (sec_eh_cfg != NULL) {
        if (!trs_sec_eh_id_is_belong_to_vf(&sec_eh_cfg->id_info[id_type], res_id)) {
            trs_sec_eh_ts_inst_put(sec_eh_cfg);
            trs_err("Invalid id. (devid=%u; type=%u; id=%u)\n", inst->devid, id_type, res_id);
            return -EACCES;
        }

        ret = trs_core_ops_stars_soc_res_ctrl(inst, id_type, res_id, cmd);
        if (ret != 0) {
            trs_sec_eh_ts_inst_put(sec_eh_cfg);
            trs_err("Res ctrl failed. (devid=%u; id_type=%u; id=%u; cmd=%u)\n", inst->devid, id_type, res_id, cmd);
            return ret;
        }
        trs_sec_eh_ts_inst_put(sec_eh_cfg);
    }

    return ret;
}
