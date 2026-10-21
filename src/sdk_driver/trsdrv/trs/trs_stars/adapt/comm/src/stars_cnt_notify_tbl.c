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
#include "ka_task_pub.h"
#include "ka_memory_pub.h"
#include "securec.h"

#include "pbl/pbl_soc_res.h"
#include "trs_pub_def.h"
#include "trs_stars_comm.h"
#include "stars_cnt_notify_tbl.h"

static KA_TASK_DEFINE_RWLOCK(cnt_notify_table_all_reg_lock);
static volatile void *stars_cnt_notify_table_ns_all_reg[TRS_DEV_MAX_NUM][TRS_TS_MAX_NUM] = {NULL};
static struct stars_cnt_notify_tbl_ops *cnt_notify_tbl_ops[TRS_DEV_MAX_NUM] = {NULL};

void trs_cnt_notify_tbl_set_ops(u32 devid, struct stars_cnt_notify_tbl_ops *ops)
{
    cnt_notify_tbl_ops[devid] = ops;
}

void trs_cnt_notify_tbl_clear_ops(u32 devid)
{
    cnt_notify_tbl_ops[devid] = NULL;
}

int trs_init_cnt_notify_tbl_ns_base_addr(struct trs_id_inst *inst)
{
    struct soc_reg_base_info io_base;
    struct res_inst_info res_inst;
    void *base_va;
    int ret;

    soc_resmng_inst_pack(&res_inst, inst->devid, TS_SUBSYS, inst->tsid);
    ret = soc_resmng_get_reg_base(&res_inst, "TS_STARS_CNT_NOTIFY_TBL_REG", &io_base);
    if (ret != 0) {
        trs_warn("No cnt notify reg base. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return ret;
    }

    base_va = ka_mm_ioremap(io_base.io_base, io_base.io_base_size);
    if (base_va == NULL) {
        trs_err("Failed to ioremap. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return -ENOMEM;
    }

    ka_task_write_lock(&cnt_notify_table_all_reg_lock);
    stars_cnt_notify_table_ns_all_reg[inst->devid][inst->tsid] = base_va;
    ka_task_write_unlock(&cnt_notify_table_all_reg_lock);
    return 0;
}

void trs_uninit_cnt_notify_tbl_ns_base_addr(struct trs_id_inst *inst)
{
    void *reg_va = NULL;

    ka_task_write_lock(&cnt_notify_table_all_reg_lock);
    reg_va = (void *)stars_cnt_notify_table_ns_all_reg[inst->devid][inst->tsid];
    stars_cnt_notify_table_ns_all_reg[inst->devid][inst->tsid] = NULL;
    ka_task_write_unlock(&cnt_notify_table_all_reg_lock);

    if (reg_va != NULL) {
        ka_mm_iounmap(reg_va);
    }
}

void trs_stars_set_cnt_notify_tbl_flag(struct trs_id_inst *inst, u32 id, u32 status)
{
    struct stars_cnt_notify_tbl_ops *ops = cnt_notify_tbl_ops[inst->devid];
    void *tbl_info = NULL;

    ka_task_read_lock(&cnt_notify_table_all_reg_lock);
    tbl_info = (void *)stars_cnt_notify_table_ns_all_reg[inst->devid][inst->tsid];
    if ((ops != NULL) && (ops->set_flag != NULL) && (tbl_info != NULL)) {
        ops->set_flag(tbl_info, id, status);
    }
    ka_task_read_unlock(&cnt_notify_table_all_reg_lock);
}
