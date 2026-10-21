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
#include "pbl/pbl_soc_res.h"
#include "trs_pub_def.h"
#include "trs_host_id.h"
#include "trs_host_db.h"
#include "trs_host_hard_mbox.h"
#include "trs_sec_eh_core.h"
#include "trs_near_adapt_init.h"
#include "trs_ts_status.h"

#include "trs_sec_eh_chan.h"
#include "trs_sec_eh_init.h"

typedef int (*trs_config_list)(struct trs_id_inst *);
typedef void (*trs_decofnig_list)(struct trs_id_inst *);

static const trs_config_list g_trs_hw_init[] = {trs_ts_db_config};

static const trs_decofnig_list g_trs_hw_uninit[] = {trs_ts_db_deconfig};

static const trs_config_list g_trs_func_init[] = {trs_id_config, trs_chan_config, trs_core_config};

static const trs_decofnig_list g_trs_func_uninit[] = {trs_id_deconfig, trs_chan_deconfig, trs_core_deconfig};

int trs_ts_hw_init(struct trs_id_inst *inst)
{
    int type, i;

    for (type = 0; type < KA_BASE_ARRAY_SIZE(g_trs_hw_init); type++) {
        int ret = g_trs_hw_init[type](inst);
        if (ret != 0) {
            for (i = type - 1; i >= 0; i--) {
                g_trs_hw_uninit[i](inst);
            }
            trs_err("Trs ts hw init fail. (devid=%u; tsid=%u; type=%d)\n", inst->devid, inst->tsid, type);
            return ret;
        }
    }
    return 0;
}

void trs_ts_hw_uninit(struct trs_id_inst *inst)
{
    int type;

    for (type = KA_BASE_ARRAY_SIZE(g_trs_hw_uninit) - 1; type >= 0; type--) {
        g_trs_hw_uninit[type](inst);
    }
}

static int trs_ts_func_init(struct trs_id_inst *inst)
{
    int type, i;

    for (type = 0; type < KA_BASE_ARRAY_SIZE(g_trs_func_init); type++) {
        int ret = g_trs_func_init[type](inst);
        if (ret != 0) {
            for (i = type - 1; i >= 0; i--) {
                g_trs_func_uninit[i](inst);
            }
            trs_err("Failed to init func. (type=%d; ret=%d)\n", type, ret);
            return ret;
        }
    }

    return 0;
}

static void trs_ts_func_uninit(struct trs_id_inst *inst)
{
    int type;

    for (type = KA_BASE_ARRAY_SIZE(g_trs_func_uninit) - 1; type >= 0; type--) {
        g_trs_func_uninit[type](inst);
    }
}

static int trs_ts_init(struct trs_id_inst *inst)
{
    int ret;

    ret = trs_ts_hw_init(inst);
    if (ret != 0) {
        return ret;
    }
    ret = trs_ts_func_init(inst);
    if (ret != 0) {
        trs_err("Failed to exec init func. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        trs_ts_hw_uninit(inst);
        return ret;
    }
    trs_res_ops_init(trs_id_inst_to_ts_inst(inst));
    trs_ts_status_mng_init(inst);

    return ret;
}

static void trs_ts_uninit(struct trs_id_inst *inst)
{
    trs_ts_status_mng_exit(inst);
    trs_res_ops_uninit(trs_id_inst_to_ts_inst(inst));
    trs_ts_func_uninit(inst);
    trs_ts_hw_uninit(inst);
}

int trs_sec_eh_init(u32 devid)
{
    u32 ts_num = 0;
    u32 tsid;
    int ret;

    ret = soc_resmng_subsys_get_num(devid, TS_SUBSYS, &ts_num);
    if ((ret != 0) || (ts_num == 0) || (ts_num > TRS_TS_MAX_NUM)) {
        trs_err("Get ts num fail. (ret=%d; devid=%u; ts_num=%u)\n", ret, devid, ts_num);
        return -EFAULT;
    }

    for (tsid = 0; tsid < ts_num; tsid++) {
        struct trs_id_inst inst;

        trs_id_inst_pack(&inst, devid, tsid);
        ret = trs_ts_init(&inst);
        if (ret != 0) {
            u32 i;
            for (i = 0; i < tsid; i++) {
                trs_id_inst_pack(&inst, devid, i);
                trs_ts_uninit(&inst);
            }
            return -ENODEV;
        }
    }

    return 0;
}

void trs_sec_eh_unint(u32 devid)
{
    u32 ts_num = 0;
    u32 tsid;
    int ret;

    ret = soc_resmng_subsys_get_num(devid, TS_SUBSYS, &ts_num);
    if ((ret != 0) || (ts_num == 0) || (ts_num > TRS_TS_MAX_NUM)) {
        trs_err("Get ts num fail. (ret=%d; ts_num=%u)\n", ret, ts_num);
        return;
    }

    for (tsid = 0; tsid < ts_num; tsid++) {
        struct trs_id_inst inst;

        trs_id_inst_pack(&inst, devid, tsid);
        trs_ts_uninit(&inst);
    }
}
