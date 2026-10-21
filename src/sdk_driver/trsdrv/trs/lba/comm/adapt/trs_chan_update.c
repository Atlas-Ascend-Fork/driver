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
#include "trs_core.h"
#include "trs_cdqm.h"

#include "trs_adapt.h"
#include "pbl/pbl_soc_res.h"
#include "trs_chan_update.h"

static struct trs_sqcq_agent_ops sqcq_agent_ops = {.device_init = NULL,
                                                   .device_uninit = NULL,
                                                   .sq_send_mode_update = NULL,
                                                   .sqe_update = NULL,
                                                   .cqe_update = NULL,
                                                   .sqe_update_src_check = NULL};
static struct trs_res_ops res_ops[TRS_DEV_MAX_NUM] = {{{NULL}}};

void trs_res_ops_register(u32 devid, struct trs_res_ops *ops)
{
    if (ops != NULL) {
        res_ops[devid] = *ops;
        trs_debug("Reg ops. (devid=%u)\n", devid);
    }
}
KA_EXPORT_SYMBOL_GPL(trs_res_ops_register);

void trs_res_ops_unregister(u32 devid)
{
    int type;

    for (type = 0; type < TRS_MAX_ID_TYPE; type++) {
        res_ops[devid].res_belong_proc[type] = NULL;
        res_ops[devid].res_get_info[type] = NULL;
    }
}
KA_EXPORT_SYMBOL_GPL(trs_res_ops_unregister);

void trs_sqcq_agent_ops_register(struct trs_sqcq_agent_ops *ops)
{
    if (ops != NULL) {
        sqcq_agent_ops = *ops;
        trs_info("Reg ops for ts_agent\n");
    }
}
KA_EXPORT_SYMBOL_GPL(trs_sqcq_agent_ops_register);

void trs_sqcq_agent_ops_unregister(void)
{
    sqcq_agent_ops.device_init = NULL;
    sqcq_agent_ops.device_uninit = NULL;
    sqcq_agent_ops.sq_send_mode_update = NULL;
    sqcq_agent_ops.sqe_update = NULL;
    sqcq_agent_ops.cqe_update = NULL;
    sqcq_agent_ops.mb_update = NULL;
    sqcq_agent_ops.sqe_update_src_check = NULL;
}
KA_EXPORT_SYMBOL_GPL(trs_sqcq_agent_ops_unregister);

int trs_chan_ops_sq_send_mode_update(u32 devid, int mode)
{
    if (sqcq_agent_ops.sq_send_mode_update == NULL) {
        return -EOPNOTSUPP;
    }

    return sqcq_agent_ops.sq_send_mode_update(devid, mode);
}
KA_EXPORT_SYMBOL_GPL(trs_chan_ops_sq_send_mode_update);

int trs_chan_get_ts_sram_addr(struct trs_id_inst *inst, phys_addr_t *paddr, size_t *size)
{
    int ret;
    struct res_inst_info res_inst;
    struct soc_rsv_mem_info rsv_mem;

    soc_resmng_inst_pack(&res_inst, inst->devid, TS_SUBSYS, inst->tsid);

    ret = soc_resmng_get_rsv_mem(&res_inst, "TS_SRAM_MEM", &rsv_mem);
    if (ret != 0) {
        return ret;
    }

    *paddr = rsv_mem.rsv_mem;
    *size = rsv_mem.rsv_mem_size;

    return 0;
}

int trs_chan_ops_agent_init(struct trs_id_inst *inst)
{
    int ret = 0;
    struct trs_sqcq_agent_para para;

    if (sqcq_agent_ops.device_init != NULL) {
        ret = trs_chan_get_ts_sram_addr(inst, &para.rsv_phy_addr, &para.rsv_size);
        if (ret != 0) {
            trs_err("Get ts sram failed. (ret=%d; devid=%u, tsid=%u)", ret, inst->devid, inst->tsid);
            return ret;
        }
        ret = sqcq_agent_ops.device_init(inst->devid, inst->tsid, &para);
        if (ret != 0) {
            trs_err("ts agent init failed. (devid=%u; tsid=%u; ret=%d)\n", inst->devid, inst->tsid, ret);
        }
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(trs_chan_ops_agent_init);

void trs_chan_ops_agent_uninit(struct trs_id_inst *inst)
{
    int ret;

    if (sqcq_agent_ops.device_uninit != NULL) {
        ret = sqcq_agent_ops.device_uninit(inst->devid, inst->tsid);
        if (ret != 0) {
            trs_err("Agent uninit failed. (ret=%d; devid=%u, tsid=%u)", ret, inst->devid, inst->tsid);
        }
    }

    return;
}
KA_EXPORT_SYMBOL_GPL(trs_chan_ops_agent_uninit);

int trs_mb_update(struct trs_id_inst *inst, int pid, void *data, u32 size)
{
    int ret = 0;

    if (sqcq_agent_ops.mb_update != NULL) {
        ret = sqcq_agent_ops.mb_update(inst->devid, inst->tsid, pid, data, size);
    }

    return ret;
}

int trs_chan_ops_sqe_update(struct trs_id_inst *inst, struct trs_sqe_update_info *update_info)
{
    int ret = 0;

    if (sqcq_agent_ops.sqe_update != NULL) {
        ret = sqcq_agent_ops.sqe_update(inst->devid, inst->tsid, update_info);
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(trs_chan_ops_sqe_update);

int trs_chan_ops_sqe_update_src_check(struct trs_id_inst *inst, struct trs_sqe_update_info *update_info)
{
    int ret = 0;

    if (sqcq_agent_ops.sqe_update_src_check != NULL) {
        ret = sqcq_agent_ops.sqe_update_src_check(inst->devid, inst->tsid, update_info);
    }

    return ret;
}

int trs_chan_ops_cqe_update(struct trs_id_inst *inst, int pid, u32 cqid, void *cqe)
{
    int ret = 0;

    if (sqcq_agent_ops.cqe_update != NULL) {
        ret = sqcq_agent_ops.cqe_update(inst->devid, inst->tsid, pid, cqid, cqe);
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(trs_chan_ops_cqe_update);

bool trs_is_proc_has_res(u32 devid, u32 tsid, int pid, int res_type, int res_id)
{
    struct trs_id_inst inst = {.devid = devid, .tsid = tsid};

    if ((res_type < (int)TRS_STREAM) || (res_type >= (int)TRS_MAX_ID_TYPE) || (devid >= TRS_DEV_MAX_NUM)) {
        trs_err("Invalid para. (devid=%u; tsid=%u; pid=%u; res_type=%d; res_id=%d)\n", devid, tsid, pid, res_type,
                res_id);
        return false;
    }

    if (res_ops[devid].res_belong_proc[res_type] != NULL) {
        return res_ops[devid].res_belong_proc[res_type](&inst, pid, res_type, res_id);
    }
    return false;
}
KA_EXPORT_SYMBOL_GPL(trs_is_proc_has_res);

/* in stream extend scene, sq que info get from stream  */
int trs_get_res_info(struct trs_id_inst *inst, int res_type, u32 res_id, void *info)
{
    if ((res_type < (int)TRS_STREAM) || (res_type >= (int)TRS_MAX_ID_TYPE) || (inst->devid >= TRS_DEV_MAX_NUM)) {
        trs_err("Invalid para. (res_type=%d; devid=%u)", res_type, inst->devid);
        return -EINVAL;
    }

    if (res_ops[inst->devid].res_get_info[res_type] != NULL) {
        return res_ops[inst->devid].res_get_info[res_type](inst, res_type, res_id, info);
    }
    return -ENODEV;
}

int trs_res_trans_v2p(u32 devid, u32 tsid, int res_type, int res_id, int *trans_res_id)
{
    if ((res_type < (int)TRS_STREAM) || (res_type >= (int)TRS_MAX_ID_TYPE) || (devid >= TRS_DEV_MAX_NUM) ||
        (trans_res_id == NULL)) {
        trs_err("Invalid para. (res_type=%d; devid=%u)", res_type, devid);
        return -EINVAL;
    }

    *trans_res_id = res_id;
    return 0;
}
KA_EXPORT_SYMBOL_GPL(trs_res_trans_v2p);

int trs_res_trans_p2v(u32 devid, u32 tsid, int res_type, int res_id, int *trans_res_id)
{
    if ((res_type < (int)TRS_STREAM) || (res_type >= (int)TRS_MAX_ID_TYPE) || (devid >= TRS_DEV_MAX_NUM) ||
        (trans_res_id == NULL)) {
        trs_err("Invalid para. (res_type=%d; devid=%u)", res_type, devid);
        return -EINVAL;
    }

    *trans_res_id = res_id;
    return 0;
}
KA_EXPORT_SYMBOL_GPL(trs_res_trans_p2v);

int hal_kernel_trs_get_ssid(u32 devid, u32 tsid, int pid, u32 *passid)
{
    struct trs_id_inst inst = {.devid = devid, .tsid = tsid};

    return trs_core_get_ssid(&inst, pid, passid);
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_trs_get_ssid);
