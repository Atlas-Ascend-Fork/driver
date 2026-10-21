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
#include "securec.h"
#include "trs_id.h"
#include "trs_host_id.h"
#ifdef CFG_FEATURE_TRS_SIA_ADAPT
#include "trs_sia_adapt_auto_init.h"
#elif defined(CFG_FEATURE_TRS_SEC_EH_ADAPT)
#include "trs_sec_eh_auto_init.h"
#endif

static int trs_host_alloc_id_batch(struct trs_id_inst *inst, int type, u32 flag, u32 id[], u32 *id_num)
{
    struct trs_msg_id_sync *id_sync = NULL;
    struct trs_msg_data msg = {0};
    int ret;
    u32 i;

    msg.header.cmdtype = TRS_MSG_GET_RES_ID;
    msg.header.valid = TRS_MSG_SEND_MAGIC;
    msg.header.tsid = inst->tsid;

    id_sync = (struct trs_msg_id_sync *)msg.payload;
    id_sync->head.flag = flag;
    id_sync->head.type = type;
    id_sync->head.req_num = (u16)((*id_num > TRS_MSG_ID_SYNC_MAX_NUM) ? TRS_MSG_ID_SYNC_MAX_NUM : *id_num);
    id_sync->id[0] = id[0]; /* specified id store in id[0] */
    id_sync->num = *id_num;

    ret = trs_host_msg_send(inst->devid, &msg, sizeof(struct trs_msg_data));
    if (ret != 0) {
        return ret;
    }
    if (id_sync->head.ret_num > *id_num) {
        return -EFAULT;
    }

    for (i = 0; i < id_sync->head.ret_num; i++) {
        id[i] = id_sync->id[i];
    }
    *id_num = id_sync->head.ret_num;
    return 0;
}

static int trs_host_free_id_batch(struct trs_id_inst *inst, int type, u32 id[], u32 id_num)
{
    struct trs_msg_id_sync *id_sync = NULL;
    struct trs_msg_data msg;
    u32 i;

    msg.header.cmdtype = TRS_MSG_PUT_RES_ID;
    msg.header.valid = TRS_MSG_SEND_MAGIC;
    msg.header.tsid = inst->tsid;

    id_sync = (struct trs_msg_id_sync *)msg.payload;
    id_sync->head.type = type;
    id_sync->head.req_num = (u16)id_num;

    for (i = 0; i < id_num; i++) {
        id_sync->id[i] = id[i];
    }

    return trs_host_msg_send(inst->devid, &msg, sizeof(struct trs_msg_data));
}

int trs_host_get_id_cap(struct trs_id_inst *inst, int type, struct trs_msg_id_cap *id_cap)
{
    struct trs_msg_id_cap *_id_cap = NULL;
    struct trs_msg_data msg;
    int ret;

    msg.header.valid = TRS_MSG_SEND_MAGIC;
    msg.header.cmdtype = TRS_MSG_GET_RES_CAP;
    msg.header.tsid = inst->tsid;
    _id_cap = (struct trs_msg_id_cap *)msg.payload;
    _id_cap->type = type;

    ret = trs_host_msg_send(inst->devid, &msg, sizeof(struct trs_msg_data));
    if (ret == 0) {
        *id_cap = *_id_cap;
    } else {
        trs_debug("Get id. (devid=%u; tsid=%u; type=%s; ret=%d)\n", inst->devid, inst->tsid, trs_id_type_to_name(type),
                  ret);
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(trs_host_get_id_cap);

static int trs_host_res_avail_query(struct trs_id_inst *inst, int type, u32 *num)
{
    struct trs_msg_res_num *res_num_msg = NULL;
    struct trs_msg_data msg;
    int ret;

    msg.header.tsid = inst->tsid;
    msg.header.valid = TRS_MSG_SEND_MAGIC;
    msg.header.cmdtype = TRS_MSG_GET_RES_AVAIL_NUM;
    msg.header.result = 0;

    res_num_msg = (struct trs_msg_res_num *)msg.payload;
    res_num_msg->type = type;
    ret = trs_host_msg_send(inst->devid, &msg, sizeof(struct trs_msg_data));
    if ((ret != 0) || (msg.header.result != 0)) {
        trs_warn("Not support. (devid=%u; tsid=%u; ret=%d; result=%d)\n", inst->devid, inst->tsid, ret,
                 msg.header.result);
        return -ENODEV;
    }
    *num = res_num_msg->avail_num;
    return 0;
}

static bool trs_host_id_is_non_cache_type(int type)
{
    return ((type == TRS_RSV_HW_SQ_ID) || (type == TRS_RSV_HW_CQ_ID) || (type == TRS_TASK_SCHED_CQ_ID) ||
            (type == TRS_CDQM_ID));
}

static int trs_host_id_init(struct trs_id_inst *inst, int type)
{
    struct trs_id_ops ops = {.owner = KA_THIS_MODULE,
                             .alloc_batch = trs_host_alloc_id_batch,
                             .free_batch = trs_host_free_id_batch,
                             .avail_query = trs_host_res_avail_query,
                             .is_non_cache_type = trs_host_id_is_non_cache_type};
    struct trs_msg_id_cap id_cap;
    struct trs_id_attr attr;
    int ret;

    ret = trs_host_get_id_cap(inst, type, &id_cap);
    if (ret != 0) {
        return ret;
    }

    if ((id_cap.isolate_num <= 0) || ((id_cap.total_num > 0) && (id_cap.isolate_num > id_cap.total_num))) {
        trs_err("Invalid isolate_num. (devid=%u; tsid=%u; type=%s; isolate_num=%u; total_num=%u)\n", inst->devid,
                inst->tsid, trs_id_type_to_name(type), id_cap.isolate_num, id_cap.total_num);
        return -EINVAL;
    }

    attr.batch_num = trs_host_id_is_non_cache_type(type) ? 1 : TRS_MSG_ID_SYNC_MAX_NUM;
    attr.id_start = id_cap.id_start;
    attr.id_end = id_cap.id_end;
    attr.id_num = id_cap.total_num;
    attr.split = id_cap.split;
    attr.isolate_num = id_cap.isolate_num;

    if (trs_id_is_local_type(type)) {
        ret = trs_id_register(inst, type, &attr, NULL);
    } else {
        ret = trs_id_register(inst, type, &attr, &ops);
    }

    if (ret == 0) {
        trs_debug("Id init. (devid=%u; tsid=%u; type=%s; start=%u; end=%u; total_num=%u; split=%u; batch_num=%u)\n",
                  inst->devid, inst->tsid, trs_id_type_to_name(type), attr.id_start, attr.id_end, attr.id_num,
                  attr.split, attr.batch_num);
    }
    return ret;
}

static void trs_host_id_uninit(struct trs_id_inst *inst, int type)
{
    trs_id_unregister(inst, type);
}

int trs_id_config(struct trs_id_inst *inst)
{
    int type, ret;

    for (type = TRS_STREAM_ID; type < TRS_ID_TYPE_MAX; type++) {
        ret = trs_host_id_init(inst, type);
        if (ret == 0) {
            trs_debug("Init id succeed. (devid=%u; tsid=%u; type=%s)\n", inst->devid, inst->tsid,
                      trs_id_type_to_name(type));
        } else if ((ret == -ENODEV) || (ret == -ENOSYS)) {
            trs_id_deconfig(inst);
            return ret;
        }
    }

    return 0;
}

void trs_id_deconfig(struct trs_id_inst *inst)
{
    int type;

    for (type = TRS_STREAM_ID; type < TRS_ID_TYPE_MAX; type++) {
        trs_host_id_uninit(inst, type);
    }
}

int trs_id_init(u32 ts_inst_id)
{
    struct trs_id_inst inst;
    int ret;

    trs_ts_inst_to_id_inst(ts_inst_id, &inst);
    ret = trs_id_config(&inst);
    if (ret != 0) {
        trs_err("Failed to config resource id. (devid=%u; tsid=%u; ret=%d)\n", inst.devid, inst.tsid, ret);
    }
    return ret;
}
DECLAER_FEATURE_AUTO_INIT_DEV(trs_id_init, FEATURE_LOADER_STAGE_3);

void trs_id_uninit(u32 ts_inst_id)
{
    struct trs_id_inst inst;

    trs_ts_inst_to_id_inst(ts_inst_id, &inst);
    trs_id_deconfig(&inst);
}
DECLAER_FEATURE_AUTO_UNINIT_DEV(trs_id_uninit, FEATURE_LOADER_STAGE_3);
