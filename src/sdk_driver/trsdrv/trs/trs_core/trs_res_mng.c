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
#include "ka_memory_pub.h"
#include "ka_task_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_driver_pub.h"

#include "kernel_version_adapt.h"
#include "dms/dms_devdrv_manager_comm.h"
#include "comm_kernel_interface.h"
#include "dpa_kernel_interface.h"
#include "pbl/pbl_uda.h"
#include "pbl/pbl_ka_mem_query.h"

#include "trs_mailbox_def.h"
#include "trs_id.h"
#include "trs_ts_inst.h"
#include "trs_core.h"
#include "trs_chan.h"
#include "trs_res_mng.h"

int id_type_trans[TRS_CORE_MAX_ID_TYPE] = {
    [TRS_STREAM] = TRS_STREAM_ID,
    [TRS_EVENT] = TRS_EVENT_ID,
    [TRS_MODEL] = TRS_MODEL_ID,
    [TRS_NOTIFY] = TRS_NOTIFY_ID,
    [TRS_CMO] = TRS_CMO_ID,
    [TRS_HW_SQ] = TRS_HW_SQ_ID,
    [TRS_HW_CQ] = TRS_HW_CQ_ID,
    [TRS_SW_SQ] = TRS_SW_SQ_ID,
    [TRS_SW_CQ] = TRS_SW_CQ_ID,
    [TRS_CB_SQ] = TRS_CB_SQ_ID,
    [TRS_CB_CQ] = TRS_CB_CQ_ID,
    [TRS_LOGIC_CQ] = TRS_LOGIC_CQ_ID,
    [TRS_CNT_NOTIFY] = TRS_CNT_NOTIFY_ID,
    [TRS_MAINT_SQ] = TRS_MAINT_SQ_ID,
    [TRS_MAINT_CQ] = TRS_MAINT_CQ_ID,
    [TRS_CDQ] = TRS_CDQM_ID,
    [TRS_STARS_H2D_JETTY] = TRS_STARS_H2D_JETTY_ID,
    [TRS_STARS_D2D_IN_BOARD_JETTY] = TRS_STARS_D2D_IN_BOARD_JETTY_ID,
    [TRS_STARS_D2D_CROSS_BOARD_JETTY] = TRS_STARS_D2D_CROSS_BOARD_JETTY_ID,
};

int trs_res_get_id_type(struct trs_id_inst *id_inst, int res_type)
{
    int id_type = id_type_trans[res_type];

    if ((uda_get_chip_type(id_inst->devid) == HISI_CLOUD_V5) && (id_type == TRS_NOTIFY_ID)) {
        id_type = TRS_CNT_NOTIFY_ID;
    }

    return id_type;
}

int trs_res_replace_res_type(struct trs_core_ts_inst *ts_inst, int res_type)
{
    if (!trs_is_stars_inst(ts_inst)) {
        res_type = (res_type == TRS_SW_SQ) ? TRS_HW_SQ : res_type;
        res_type = (res_type == TRS_SW_CQ) ? TRS_HW_CQ : res_type;
    }

    return res_type;
}

int trs_core_get_host_pid(int cur_pid, int *host_pid)
{
    enum devdrv_process_type cp_type;
    u32 chip_id, vfid;
    int ret;

    ret = hal_kernel_devdrv_query_process_host_pid(cur_pid, &chip_id, &vfid, (u32 *)host_pid, &cp_type);
    if (ret != 0) {
        trs_debug("Query host pid unsuccessful. (cur_pid=%d; ret=%d)\n", cur_pid, ret);
    }
    return ret;
}

static bool trs_is_host_pid_match(int cur_pid, int host_pid)
{
    int pid;

    if (trs_core_get_host_pid(cur_pid, &pid) != 0) {
        trs_err("Query host pid fail.\n");
        return false;
    }

    return (pid == host_pid);
}

u32 trs_res_get_id_num(struct trs_core_ts_inst *ts_inst, int res_type)
{
    return ts_inst->res_mng[res_type].id_num;
}

u32 trs_res_get_max_id(struct trs_core_ts_inst *ts_inst, int res_type)
{
    return ts_inst->res_mng[res_type].max_id;
}

u32 trs_res_get_id_used_num(struct trs_core_ts_inst *ts_inst, int res_type)
{
    return ts_inst->res_mng[res_type].use_num;
}

static void trs_stream_ctx_init(struct trs_stream_ctx *stream_ctx)
{
    stream_ctx->sq = -1;
    stream_ctx->cq = -1;
    stream_ctx->logic_cq = -1;
}

static void trs_res_id_init_para(struct trs_core_ts_inst *ts_inst, int res_type, u32 res_id)
{
    if (res_type == TRS_STREAM) {
        trs_stream_ctx_init(&ts_inst->stream_ctx[res_id]);
    }
}

static int trs_event_res_map_notice_ts(struct trs_core_ts_inst *ts_inst, int op, u32 event_id)
{
    struct trs_res_map_msg msg;

    trs_mbox_init_header(&msg.header, TRS_MBOX_RES_MAP);
    msg.resource_type = 1;
    msg.operation_type = op;
    msg.id = event_id;
    msg.host_pid = ts_inst->res_mng[TRS_EVENT].ids[event_id].pid;

    /* adapt fill: vf_id, phy_id */
    return trs_core_notice_ts(ts_inst, (u8 *)&msg, sizeof(msg));
}

static int trs_event_notice_ts(struct trs_core_ts_inst *ts_inst, int op, u32 event_id)
{
    return trs_event_res_map_notice_ts(ts_inst, op, event_id);
}

static int trs_notify_notice_ts(struct trs_core_ts_inst *ts_inst, int op, u32 notify_id, u32 notify_type,
                                u32 config_type)
{
    struct trs_notify_msg msg;
    u32 cmd_type = (config_type == TRS_RES_OP_RESET) ? TRS_MBOX_RESET_NOTIFY : TRS_MBOX_RECORD_NOTIFY;
    u32 id_type = (notify_type == TRS_MBOX_NOTIFY_TYPE) ? TRS_NOTIFY : TRS_CNT_NOTIFY;

    trs_mbox_init_header(&msg.header, cmd_type);
    msg.notifyId = notify_id;
    msg.phy_notifyId = notify_id;
    msg.tgid = ts_inst->res_mng[id_type].ids[notify_id].pid;
    msg.notify_type = notify_type; /* adapt may change it */

    /* adapt fill: plat_type, phy_notifyId, fid */
    return trs_core_notice_ts(ts_inst, (u8 *)&msg, sizeof(msg));
}

int trs_notify_config_with_ts(struct trs_id_inst *inst, u32 notify_id, u32 notify_type, u32 config_type)
{
    struct trs_core_ts_inst *ts_inst = NULL;
    int ret;

    ts_inst = trs_core_inst_get(inst->devid, inst->tsid);
    if (ts_inst == NULL) {
        trs_err("Invalid para. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return -EINVAL;
    }
    ret = trs_notify_notice_ts(ts_inst, 0, notify_id, notify_type, config_type);
    if (ret != 0) {
        trs_err("Notice ts failed. (devid=%u; notify_id=%u; notify_type=%u; ret=%d)\n", inst->devid, notify_id,
                notify_type, ret);
    }
    trs_core_inst_put(ts_inst);
    return ret;
}
KA_EXPORT_SYMBOL_GPL(trs_notify_config_with_ts);

static int trs_stream_notice_ts(struct trs_core_ts_inst *ts_inst, int op, u32 stream_id)
{
    struct trs_stream_msg msg;

    if (op == 0) {
        return 0;
    }

    trs_mbox_init_header(&msg.header, (op == 0) ? TRS_MBOX_ALLOC_STREAM : TRS_MBOX_FREE_STREAM);
    msg.stream_id = stream_id;

    trs_debug("Stream msg. (devid=%u; stream_id=%u)\n", ts_inst->inst.devid, stream_id);
    return trs_core_notice_ts(ts_inst, (u8 *)&msg, sizeof(msg));
}

static int trs_res_notice_ts(struct trs_core_ts_inst *ts_inst, int op, int res_type, u32 res_id)
{
    if (res_type == TRS_EVENT) {
        return trs_event_notice_ts(ts_inst, op, res_id);
    } else if (res_type == TRS_NOTIFY) {
        return trs_notify_notice_ts(ts_inst, op, res_id, TRS_MBOX_NOTIFY_TYPE, TRS_RES_OP_RESET);
    } else if (res_type == TRS_CNT_NOTIFY) {
        return trs_notify_notice_ts(ts_inst, op, res_id, TRS_MBOX_CNT_NOTIFY_TYPE, TRS_RES_OP_RESET);
    } else if (res_type == TRS_STREAM) {
        return trs_stream_notice_ts(ts_inst, op, res_id);
    } else {
        /* do nothing */
    }

    return 0;
}

static int trs_res_mng_id_alloc(struct trs_core_ts_inst *ts_inst, int res_type, u32 flag, u32 para, u32 *res_id)
{
    if (ts_inst->ops.id_alloc != NULL) {
        return ts_inst->ops.id_alloc(&ts_inst->inst, trs_res_get_id_type(&ts_inst->inst, res_type), flag, res_id, para);
    } else {
#if defined(EMU_ST)
        flag = 0;
#endif
        return trs_id_alloc_ex(&ts_inst->inst, trs_res_get_id_type(&ts_inst->inst, res_type), flag, res_id, 1);
    }

    return 0;
}

static void trs_res_mng_id_free(struct trs_core_ts_inst *ts_inst, int res_type, u32 flag, u32 res_id)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    int ret;

    if (ts_inst->ops.id_free != NULL) {
        ret = ts_inst->ops.id_free(inst, trs_res_get_id_type(inst, res_type), flag, res_id);
    } else {
        ret = trs_id_free_ex(inst, trs_res_get_id_type(inst, res_type), flag, res_id);
    }

    if (ret != 0) {
#ifndef EMU_ST
        trs_warn("Free warn. (devid=%u; tsid=%u; res_type=%d; res_id=%u; flag=%u; ret=%d)\n", inst->devid, inst->tsid,
                 res_type, res_id, flag, ret);
#endif
    }
}

static bool trs_res_id_need_reset(int res_type)
{
    return (res_type == TRS_EVENT) || (res_type == TRS_NOTIFY) || (res_type == TRS_CNT_NOTIFY);
}

static int _trs_res_id_ctrl(struct trs_core_ts_inst *ts_inst, int res_type, u32 res_id, u32 cmd)
{
    if (ts_inst->ops.res_id_ctrl != NULL) {
        return ts_inst->ops.res_id_ctrl(&ts_inst->inst, (u32)res_type, res_id, cmd);
    }
    return 0;
}

static int trs_res_id_query_addr(struct trs_core_ts_inst *ts_inst, int res_type, u32 res_id, u64 *value)
{
    if (ts_inst->ops.res_id_query != NULL) {
        return ts_inst->ops.res_id_query(&ts_inst->inst, (u32)res_type, res_id, TRS_RES_QUERY_ADDR, value);
    }
    return 0;
}

int trs_res_id_alloc(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct trs_res_id_para *para)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    u64 value;
    int ret;

    ret = trs_res_mng_id_alloc(ts_inst, para->res_type, para->flag, para->para, &para->id);
    if (ret != 0) {
        if ((ret != -EOPNOTSUPP) &&
            (trs_get_proc_id_dfx_times(proc_ctx, inst->tsid, para->res_type) <= TRS_PROC_DFX_TIMES_MAX)) {
            if (para->res_type != TRS_NOTIFY) {
                trs_err("Alloc res id failed. (devid=%u; tsid=%u; id_type=%u; cur_id_num=%u; ret=%d)\n", inst->devid,
                        inst->tsid, para->res_type, trs_get_proc_res_num(proc_ctx, inst->tsid, para->res_type), ret);
            }
        }
        return ret;
    }

    trs_res_id_init_para(ts_inst, para->res_type, para->id);

    ret = trs_res_notice_ts(ts_inst, 0, para->res_type, para->id);
    if (ret != 0) {
        trs_res_mng_id_free(ts_inst, para->res_type, para->flag, para->id);
        trs_err("Notice ts failed. (devid=%u; tsid=%u; res_type=%d; ret=%d)\n", inst->devid, inst->tsid, para->res_type,
                ret);
        return ret;
    }

    ret = trs_proc_add_res(proc_ctx, ts_inst, para->res_type, para->id);
    if (ret != 0) {
        (void)trs_res_notice_ts(ts_inst, 1, para->res_type, para->id);
        trs_res_mng_id_free(ts_inst, para->res_type, para->flag, para->id);
        return ret;
    }

    para->value[0] = 0;
    para->value[1] = 0;
    ret = trs_res_id_query_addr(ts_inst, para->res_type, para->id, &value);
    if (ret == 0) {
        para->value[0] = (u32)(value & 0xffffffffU); /* 0xffffffffU for low 32bit */
        para->value[1] = (u32)(value >> 32U);        /* 32 for high bit */
    }

    if (trs_res_id_need_reset(para->res_type)) {
        (void)_trs_res_id_ctrl(ts_inst, para->res_type, para->id, TRS_RES_OP_CHECK_AND_RESET);
    }

    return 0;
}

#ifdef CFG_FEATURE_SUPPORT_STREAM_TASK
static inline void trs_pack_stream_mem_attr(struct trs_core_ts_inst *ts_inst, u64 addr, u64 size,
                                            struct ka_mem_attr *stream_mem_attr)
{
    stream_mem_attr->addr = addr;
    stream_mem_attr->size = size;

    if ((ts_inst->ops.get_sq_send_mode != NULL) &&
        (ts_inst->ops.get_sq_send_mode(ts_inst->inst.devid) == TRS_MODE_TYPE_SQ_SEND_HIGH_SECURITY)) {
        stream_mem_attr->cp_only_flag = false; /* change to true when svm support huge page cp only pin */
    } else {
        stream_mem_attr->cp_only_flag = false;
    }

    if ((ts_inst->ops.get_connect_protocol != NULL) &&
        (ts_inst->ops.get_connect_protocol(&ts_inst->inst) == TRS_CONNECT_PROTOCOL_UB)) {
        stream_mem_attr->raw_pa_flag = true;
    } else {
        stream_mem_attr->raw_pa_flag = false;
    }
}

void trs_stream_put_sq_mem_handle(struct trs_core_ts_inst *ts_inst, struct trs_proc_ctx *proc_ctx,
                                  struct trs_stream_ctx *stream_ctx)
{
    if (ts_inst->ops.put_sq_mem_handle != NULL) {
        ts_inst->ops.put_sq_mem_handle(&ts_inst->inst, proc_ctx->pid, &stream_ctx->mem_attr);
    }

    stream_ctx->stream_base_addr = 0;
    stream_ctx->depth = 0;
    stream_ctx->mem_attr.pa_num = 0;
}

static int trs_stream_unbind_sq(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, u32 res_id)
{
    ka_task_mutex_lock(&proc_ctx->ts_ctx[0].mutex);
    if (ts_inst->stream_ctx[res_id].stream_base_addr != 0) {
        if (ts_inst->stream_ctx[res_id].sq != -1) {
            int sqid = ts_inst->stream_ctx[res_id].sq;
            if ((ts_inst->sq_ctx[sqid].stream_id == res_id) && (proc_ctx->status != TRS_PROC_STATUS_EXIT)) {
                ka_task_mutex_unlock(&proc_ctx->ts_ctx[0].mutex);
                trs_err("Sq still binds this stream. (sqid=%u; stream_id=%u)\n", sqid, res_id);
                return -EINVAL;
            }
        }

        trs_stream_put_sq_mem_handle(ts_inst, proc_ctx, &ts_inst->stream_ctx[res_id]);
    }
    ts_inst->stream_ctx[res_id].sq = -1;
    ts_inst->stream_ctx[res_id].tail = 0;
    ka_task_mutex_unlock(&proc_ctx->ts_ctx[0].mutex);
    return 0;
}
#endif

int trs_res_id_pre_del(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, int res_type, u32 res_id)
{
#ifdef CFG_FEATURE_SUPPORT_STREAM_TASK
    if (res_type == TRS_STREAM) {
        return trs_stream_unbind_sq(proc_ctx, ts_inst, res_id);
    }
#endif
    return 0;
}

static int _trs_res_id_free(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, int res_type, u64 flag,
                            u32 res_id)
{
    int ret;

    ret = trs_proc_del_res(proc_ctx, ts_inst, res_type, res_id);
    if (ret < 0) {
        return ret;
    }

    if (ret == 0) {
        if (trs_res_id_need_reset(res_type)) {
            (void)_trs_res_id_ctrl(ts_inst, res_type, res_id, TRS_RES_OP_RESET);
        }
        (void)trs_res_notice_ts(ts_inst, 1, res_type, res_id);
        trs_res_mng_id_free(ts_inst, res_type, flag, res_id);
    }

    return 0;
}

int trs_res_id_free(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct trs_res_id_para *para)
{
    return _trs_res_id_free(proc_ctx, ts_inst, para->res_type, para->flag, para->id);
}

#ifdef CFG_FEATURE_SUPPORT_STREAM_TASK
int trs_stream_get_sq_mem_handle(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst,
                                 struct trs_stream_ctx *stream_ctx, void *stream_mem, u32 depth)
{
    u32 size = depth * TRS_HW_SQE_SIZE;
    int ret;

    if (stream_mem == NULL) {
        trs_err("Invalid NULL stream mem.\n");
        return -EINVAL;
    }

    if (ts_inst->ops.get_sq_mem_handle == NULL) {
        trs_err("get_sq_mem_handle ops not registered. (devid=%u)\n", proc_ctx->devid);
        return -EINVAL;
    }

    trs_pack_stream_mem_attr(ts_inst, (u64)(uintptr_t)stream_mem, size, &stream_ctx->mem_attr.mem_attr);

    ret = ts_inst->ops.get_sq_mem_handle(&ts_inst->inst, proc_ctx->pid, &stream_ctx->mem_attr);
    if (ret != 0) {
        trs_err("Get mem pa list failed. (devid=%u; pid=%d; ret=%d)\n", proc_ctx->devid, proc_ctx->pid, ret);
        return ret;
    }

    stream_ctx->stream_base_addr = (u64)(uintptr_t)stream_mem;
    stream_ctx->depth = depth;
    trs_debug("Get and pin page success. (devid=%u; size=%u; pa_num=%u)\n", proc_ctx->devid, size,
              stream_ctx->mem_attr.pa_num);
    return 0;
}

static int trs_stream_task_fill_proc_mem_check(struct trs_core_ts_inst *ts_inst, struct trs_stream_task_para *para,
                                               struct trs_stream_ctx *stream_ctx)
{
    if ((uda_get_chip_type(ts_inst->inst.devid) == HISI_CLOUD_V2)) {
        if (((para->stream_mem != NULL) && (stream_ctx->stream_base_addr != (u64)(uintptr_t)para->stream_mem)) ||
            (stream_ctx->mem_attr.vmap_pfn_handle == NULL)) {
            trs_err("Stream mem invalid or vmap_pfn not inited. (base=0x%llx; mem=0x%llx)\n",
                    stream_ctx->stream_base_addr, (u64)(uintptr_t)para->stream_mem);
            return -EINVAL;
        }
    } else {
        if ((stream_ctx->stream_base_addr > (u64)(uintptr_t)para->stream_mem) ||
            ((u64)(uintptr_t)para->stream_mem > (stream_ctx->stream_base_addr + stream_ctx->tail * TRS_HW_SQE_SIZE)) ||
            (stream_ctx->mem_attr.vmap_pfn_handle == NULL)) {
            trs_err("Stream mem invalid or vmap_pfn not inited. (base=0x%llx; mem=0x%llx; tail=%u)\n",
                    stream_ctx->stream_base_addr, (u64)(uintptr_t)para->stream_mem, stream_ctx->tail);
            return -EINVAL;
        }
    }

    return 0;
}

static int _trs_stream_task_fill(struct trs_core_ts_inst *ts_inst, struct trs_proc_ctx *proc_ctx,
                                 struct trs_stream_ctx *stream_ctx, struct trs_stream_task_para *para)
{
    u64 stream_uva = stream_ctx->mem_attr.mem_attr.addr;
    u64 stream_size = stream_ctx->mem_attr.mem_attr.size;
    u64 task_offset;
    u32 tail_tmp, i;
    unsigned long cur_jiffies;
    int ret;

    if ((uda_get_chip_type(ts_inst->inst.devid) == HISI_CLOUD_V2)) {
        task_offset = (stream_ctx->stream_base_addr - stream_uva) + stream_ctx->tail * TRS_HW_SQE_SIZE;
        tail_tmp = stream_ctx->tail + para->task_cnt;
    } else {
        task_offset = (u64)(uintptr_t)para->stream_mem - stream_uva;
        tail_tmp = ka_base_max((task_offset / TRS_HW_SQE_SIZE + para->task_cnt), (u64)stream_ctx->tail);
    }

    if ((task_offset + para->task_cnt * TRS_HW_SQE_SIZE) > stream_size) {
        trs_err("Stream space insufficient. (devid=%u; stream=%u; tail=%u; task_cnt=%u)\n", proc_ctx->devid,
                para->stream_id, stream_ctx->tail, para->task_cnt);
        return -ENOSPC;
    }

    cur_jiffies = ka_jiffies;
    for (i = 0; i < para->task_cnt; i++) {
        ret = trs_chan_stream_task_update(&ts_inst->inst, proc_ctx->pid, (para->task_info + i * TRS_HW_SQE_SIZE),
                                          &stream_ctx->long_task_cnt);
        if (ret != 0) {
            trs_err("Update stream task fail. (devid=%u; i=%u; task_cnt=%u; ret=%d)\n", proc_ctx->devid, i,
                    para->task_cnt, ret);
            return ret;
        }
        trs_try_resched(&cur_jiffies, 50); /* timeout is 50 ms */
    }

    ka_mm_vmap_pfn_memcpy_toio(stream_ctx->mem_attr.vmap_pfn_handle, task_offset, para->task_info,
                               para->task_cnt * TRS_HW_SQE_SIZE);
    stream_ctx->tail = tail_tmp;
    return 0;
}

int trs_stream_task_fill_proc(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst,
                              struct trs_stream_task_para *para)
{
    struct trs_stream_ctx *stream_ctx = NULL;
    int ret, get_mem_flag = 0;

    if (!trs_proc_has_res(proc_ctx, ts_inst, TRS_STREAM, para->stream_id)) {
        trs_err("Stream not alloced. (stream_id=%u; pid=%d)\n", para->stream_id, proc_ctx->pid);
        return -EINVAL;
    }

    stream_ctx = &ts_inst->stream_ctx[para->stream_id];
    if (stream_ctx->mem_attr.pa_num == 0) {
        ret = trs_stream_get_sq_mem_handle(proc_ctx, ts_inst, stream_ctx, para->stream_mem, para->task_cnt);
        if (ret != 0) {
            trs_err("Failed to get pa list. (ret=%d; devid=%u; stream=%u)\n", ret, proc_ctx->devid, para->stream_id);
            return ret;
        }
        get_mem_flag = 1;
    } else {
        ret = trs_stream_task_fill_proc_mem_check(ts_inst, para, stream_ctx);
        if (ret != 0) {
            return ret;
        }
    }

    ret = _trs_stream_task_fill(ts_inst, proc_ctx, stream_ctx, para);
    if ((ret != 0) && (get_mem_flag == 1)) {
        trs_stream_put_sq_mem_handle(ts_inst, proc_ctx, stream_ctx);
    }

    return ret;
}
#endif

static trs_res_share_by_proc_ops_t shr_proc_ops[TRS_MAX_ID_TYPE];

void trs_res_share_proc_ops_register(int res_type, trs_res_share_by_proc_ops_t func)
{
    shr_proc_ops[res_type] = func;
}
KA_EXPORT_SYMBOL_GPL(trs_res_share_proc_ops_register);

bool trs_res_is_belong_to_proc(struct trs_id_inst *inst, int pid, int res_type, u32 res_id)
{
    struct trs_core_ts_inst *ts_inst = NULL;
    bool ret;

    ts_inst = trs_core_ts_inst_get(inst);
    if (ts_inst == NULL) {
        trs_err("Invalid para. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return false;
    }

    ret = trs_proc_has_res_with_pid(ts_inst, pid, res_type, res_id);
    trs_core_inst_put(ts_inst);

    if ((ret != true) && (shr_proc_ops[res_type] != NULL)) {
        /* res_id does not belong to this proc, further check whether it is shared by this process */
        ret = shr_proc_ops[res_type](inst, pid, res_type, res_id);
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(trs_res_is_belong_to_proc);

int trs_res_id_check(struct trs_id_inst *inst, int res_type, u32 res_id)
{
    struct trs_core_ts_inst *ts_inst = NULL;
    int ret = 0;

    ts_inst = trs_core_ts_inst_get(inst);
    if (ts_inst == NULL) {
        trs_err("Invalid para. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return -EINVAL;
    }

    if (ts_inst->ops.res_id_check != NULL) {
        ret = ts_inst->ops.res_id_check(inst, res_type, res_id);
    }
    trs_core_inst_put(ts_inst);

    return ret;
}
KA_EXPORT_SYMBOL_GPL(trs_res_id_check);

static int trs_res_id_para_check(struct trs_id_inst *inst, int res_type, u32 res_id)
{
    if (trs_id_inst_check(inst) != 0) {
        return -EINVAL;
    }

    if ((res_type < 0) || (res_type >= TRS_HW_SQ)) {
        trs_err("Invalid para. (devid=%u; tsid=%u; res_type=%d)\n", inst->devid, inst->tsid, res_type);
        return -EINVAL;
    }

    return 0;
}

int trs_res_id_get(struct trs_id_inst *inst, int res_type, u32 res_id)
{
    struct trs_core_ts_inst *ts_inst = NULL;
    int ret = trs_res_id_para_check(inst, res_type, res_id);
    if (ret != 0) {
        return ret;
    }

    ts_inst = trs_core_ts_inst_get(inst);
    if (ts_inst == NULL) {
        trs_err("Invalid para. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return -EINVAL;
    }

    ret = trs_res_get(ts_inst, ka_task_get_current_tgid(), res_type, res_id, 0, 0);

    trs_core_inst_put(ts_inst);
    return ret;
}
KA_EXPORT_SYMBOL_GPL(trs_res_id_get);

int trs_res_id_put(struct trs_id_inst *inst, int res_type, u32 res_id)
{
    struct trs_core_ts_inst *ts_inst = NULL;
    int ret = trs_res_id_para_check(inst, res_type, res_id);
    if (ret != 0) {
        return ret;
    }

    ts_inst = trs_core_ts_inst_get(inst);
    if (ts_inst == NULL) {
        trs_err("Invalid para. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return -EINVAL;
    }

    ret = trs_res_put(ts_inst, res_type, res_id, 0, 0);
    if (ret == 0) {
        (void)trs_res_notice_ts(ts_inst, 1, res_type, res_id);
        trs_res_mng_id_free(ts_inst, res_type, 0, res_id);
    }

    trs_core_inst_put(ts_inst);
    return (ret < 0) ? ret : 0;
}
KA_EXPORT_SYMBOL_GPL(trs_res_id_put);

int _trs_res_id_remote_get(struct trs_id_inst *inst, int pid, int res_type, u32 res_id, u32 hpid, u32 seq)
{
    struct trs_core_ts_inst *ts_inst = NULL;
    int ret = 0;

    ret = trs_res_id_para_check(inst, res_type, res_id);
    if (ret != 0) {
        return ret;
    }

    ts_inst = trs_core_ts_inst_get(inst);
    if (ts_inst == NULL) {
        trs_err("Invalid inst. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return -EINVAL;
    }

    ret = trs_res_get(ts_inst, pid, res_type, res_id, hpid, seq);
    if (ret != 0) {
        trs_err("Remote res get fail. (devid=%u; type=%d; id=%u; ret=%d)\n", inst->devid, res_type, res_id, ret);
    }

    trs_core_inst_put(ts_inst);
    return ret;
}
KA_EXPORT_SYMBOL_GPL(_trs_res_id_remote_get);

int _trs_res_id_remote_put(struct trs_id_inst *inst, int res_type, u32 res_id, u32 hpid, u32 seq)
{
    struct trs_core_ts_inst *ts_inst = NULL;
    int ret = 0;

    ret = trs_res_id_para_check(inst, res_type, res_id);
    if (ret != 0) {
        return ret;
    }

    ts_inst = trs_core_ts_inst_get(inst);
    if (ts_inst == NULL) {
        trs_err("Invalid id inst. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return -EINVAL;
    }

    ret = trs_res_put(ts_inst, res_type, res_id, hpid, seq);
    if (ret == 0) {
        (void)trs_res_notice_ts(ts_inst, 1, res_type, res_id);
        trs_res_mng_id_free(ts_inst, res_type, 0, res_id);
    }

    trs_core_inst_put(ts_inst);
    return (ret < 0) ? ret : 0;
}
KA_EXPORT_SYMBOL_GPL(_trs_res_id_remote_put);

int trs_res_id_remote_get(struct trs_id_inst *inst, int res_type, u32 res_id, u32 hpid, u32 seq)
{
    struct trs_core_ts_inst *ts_inst = NULL;
    int ret = 0;

    ts_inst = trs_core_ts_inst_get(inst);
    if (ts_inst == NULL) {
        trs_err("Invalid para. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return -EINVAL;
    }

    if (ts_inst->ops.res_id_remote_get != NULL) {
        ret = ts_inst->ops.res_id_remote_get(inst, res_type, res_id, hpid, seq);
    }
    trs_core_inst_put(ts_inst);

    return ret;
}
KA_EXPORT_SYMBOL_GPL(trs_res_id_remote_get);

int trs_res_id_remote_put(struct trs_id_inst *inst, int res_type, u32 res_id, u32 hpid, u32 seq)
{
    struct trs_core_ts_inst *ts_inst = NULL;
    int ret = 0;

    ts_inst = trs_core_ts_inst_get(inst);
    if (ts_inst == NULL) {
        trs_err("Invalid para. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return -EINVAL;
    }

    if (ts_inst->ops.res_id_remote_put != NULL) {
        ret = ts_inst->ops.res_id_remote_put(inst, res_type, res_id, hpid, seq);
    }
    trs_core_inst_put(ts_inst);

    return ret;
}
KA_EXPORT_SYMBOL_GPL(trs_res_id_remote_put);

int trs_res_id_ctrl(struct trs_id_inst *inst, int res_type, u32 res_id, u32 cmd)
{
    struct trs_core_ts_inst *ts_inst = NULL;
    int ret = -ENODEV;

    ts_inst = trs_core_ts_inst_get(inst);
    if (ts_inst != NULL) {
        ret = _trs_res_id_ctrl(ts_inst, res_type, res_id, cmd);
        trs_core_inst_put(ts_inst);
    }
    return ret;
}
KA_EXPORT_SYMBOL_GPL(trs_res_id_ctrl);

int trs_res_id_num_query(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct trs_res_id_para *para)
{
    (void)proc_ctx;
    para->para = trs_res_get_id_num(ts_inst, para->res_type);
    return 0;
}

int trs_res_id_max_query(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct trs_res_id_para *para)
{
    (void)proc_ctx;
    para->para = trs_res_get_max_id(ts_inst, para->res_type);
    return 0;
}

int trs_res_id_used_query(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct trs_res_id_para *para)
{
    u32 used_num;
    int ret;

    (void)proc_ctx;
    ret = trs_id_get_used_num(&ts_inst->inst, trs_res_get_id_type(&ts_inst->inst, para->res_type), &used_num);
    if (ret != 0) {
        return ret;
    }
    para->para = used_num;

    if (ts_inst->ops.res_num_query != NULL) {
        struct trs_res_query_para query_para;
        query_para.type = trs_res_get_id_type(&ts_inst->inst, para->res_type);
        ret = ts_inst->ops.res_num_query(&ts_inst->inst, &query_para);
        if (ret == 0) {
            para->para += query_para.alloc_num;
        } else if (ret != -EOPNOTSUPP) {
            trs_err("Failed to query resource number. (ret=%d; devid=%u; type=%d)\n", ret, ts_inst->inst.devid,
                    para->res_type);
            return ret;
        }
        trs_debug("Query from remote. (devid=%u; type=%d; device_alloc_num=%u; alloc_num=%u)\n", ts_inst->inst.devid,
                  para->res_type, query_para.alloc_num, para->para);
    }

    if (!trs_is_stars_inst(ts_inst)) {
        /* non stars hw res store in TRS_HW_* and TRS_SW_* type */
        if (para->res_type == TRS_HW_SQ) {
            ret = trs_id_get_used_num(&ts_inst->inst, trs_res_get_id_type(&ts_inst->inst, TRS_SW_SQ), &used_num);
            if (ret != 0) {
                return ret;
            }
            para->para += used_num;
        }
        if (para->res_type == TRS_HW_CQ) {
            ret = trs_id_get_used_num(&ts_inst->inst, trs_res_get_id_type(&ts_inst->inst, TRS_SW_CQ), &used_num);
            if (ret != 0) {
                return ret;
            }
            para->para += used_num;
        }
    }
    return 0;
}

int trs_res_id_avail_query(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst,
                           struct trs_res_id_para *para)
{
    int type = trs_res_get_id_type(&ts_inst->inst, para->res_type);
    int ret;

    (void)proc_ctx;
    ret = trs_id_get_avail_num_in_pool(&ts_inst->inst, type, &para->para);
    if (ret != 0) {
        trs_warn("Not support. (devid=%u; tsid=%u; type=%d)\n", ts_inst->inst.devid, ts_inst->inst.tsid, type);
    }

    return 0;
}

int trs_res_id_reg_offset_query(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst,
                                struct trs_res_id_para *para)
{
    (void)proc_ctx;

    if (!((para->res_type == TRS_NOTIFY) || (para->res_type == TRS_EVENT)) ||
        (para->id >= trs_res_get_max_id(ts_inst, para->res_type))) {
        trs_err("Invalid para. (res_type=%d; res_id=%u)\n", para->res_type, para->id);
        return -EINVAL;
    }

    if (ts_inst->ops.get_res_reg_offset == NULL) {
        return -EOPNOTSUPP;
    }
    return ts_inst->ops.get_res_reg_offset(&ts_inst->inst, trs_res_get_id_type(&ts_inst->inst, para->res_type),
                                           para->id, &para->para);
}

int trs_res_id_reg_size_query(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst,
                              struct trs_res_id_para *para)
{
    (void)proc_ctx;

    if (!((para->res_type == TRS_NOTIFY) || (para->res_type == TRS_EVENT))) {
        trs_err("Invalid para. (res_type=%d)\n", para->res_type);
        return -EINVAL;
    }

    if (ts_inst->ops.get_res_reg_total_size == NULL) {
        return -EOPNOTSUPP;
    }
    return ts_inst->ops.get_res_reg_total_size(&ts_inst->inst, trs_res_get_id_type(&ts_inst->inst, para->res_type),
                                               &para->para);
}

int trs_get_stream_logic_cq(struct trs_core_ts_inst *ts_inst, u32 stream_id)
{
    if (stream_id >= trs_res_get_max_id(ts_inst, TRS_STREAM)) {
        struct trs_id_inst *inst = &ts_inst->inst;
        trs_err("Invalid stream id. (devid=%u; tsid=%u; stream_id=%u)\n", inst->devid, inst->tsid, stream_id);
        return -1;
    }

    return ts_inst->stream_ctx[stream_id].logic_cq;
}

static int trs_stream_bind_logic_cq(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, u32 stream_id,
                                    u32 logic_cq)
{
    struct trs_stream_ctx *stream_ctx = &ts_inst->stream_ctx[stream_id];
    struct trs_id_inst *inst = &ts_inst->inst;

    if (!trs_proc_has_res(proc_ctx, ts_inst, TRS_LOGIC_CQ, logic_cq)) {
        trs_err("Logic cq not belong to proc. (devid=%u; tsid=%u; stream_id=%u; logic_cq=%u)\n", inst->devid,
                inst->tsid, stream_id, logic_cq);
        return -EINVAL;
    }

    if ((stream_ctx->logic_cq >= 0) && (stream_ctx->logic_cq != logic_cq)) {
        trs_info("Stream Bind new logic cq. (devid=%u; tsid=%u; stream_id=%u; logic_cq=%u; new_logic_cq=%u)\n",
                 inst->devid, inst->tsid, stream_id, stream_ctx->logic_cq, logic_cq);
    }

    stream_ctx->logic_cq = (int)logic_cq;
    return 0;
}

static int trs_stream_unbind_logic_cq(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, u32 stream_id)
{
    ts_inst->stream_ctx[stream_id].logic_cq = -1;
    return 0;
}

static int trs_stream_cfg(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct trs_res_id_para *para)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    u32 stream_id = para->id;
    int ret;

    ret = trs_res_id_get(inst, TRS_STREAM, stream_id);
    if (ret != 0) {
        trs_err("Stream not belong to proc. (devid=%u; tsid=%u; stream_id=%u)\n", inst->devid, inst->tsid, stream_id);
        return -EINVAL;
    }

    if (para->prop == DRV_STREAM_BIND_LOGIC_CQ) {
        ret = trs_stream_bind_logic_cq(proc_ctx, ts_inst, stream_id, para->para);
    } else if (para->prop == DRV_STREAM_UNBIND_LOGIC_CQ) {
        ret = trs_stream_unbind_logic_cq(proc_ctx, ts_inst, stream_id);
    } else {
        trs_warn("Not support. (devid=%u; tsid=%u; prop=%d)\n", inst->devid, inst->tsid, para->prop);
        ret = -EOPNOTSUPP;
    }
    trs_res_id_put(inst, TRS_STREAM, stream_id);

    return ret;
}

static int trs_set_res_id_ctrl(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, u32 type, u32 id,
                               u32 cmd)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    int ret;

    if ((id >= trs_res_get_max_id(ts_inst, (int)type)) || (ts_inst->ops.res_id_ctrl == NULL)) {
        trs_err("Invalid para. (devid=%u; tsid=%u; type=%u; id=%u)\n", inst->devid, inst->tsid, type, id);
        return -EINVAL;
    }

    ret = ts_inst->ops.res_id_ctrl(inst, type, id, cmd);
    if ((ret != 0) && (ret != -EOPNOTSUPP)) {
        trs_err("Set id ctrl failed. (devid=%u; tsid=%u; type=%u; id=%u; cmd=%u; ret=%d)\n", inst->devid, inst->tsid,
                type, id, cmd, ret);
    }

    return ret;
}

static int trs_set_res_id_ctrl_of_proc(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, u32 type)
{
    u32 i;
    int ret = 0;

    ka_task_mutex_lock(&ts_inst->res_mng[type].mutex);
    for (i = 0; i < ts_inst->res_mng[type].max_id; i++) {
        struct trs_res_ids *id = &ts_inst->res_mng[type].ids[i];
        if ((id->ref != 0) && (proc_ctx->pid == id->pid) && (proc_ctx->task_id == id->task_id) &&
            (id->status == RES_STATUS_NORMAL)) {
            ret |= trs_set_res_id_ctrl(proc_ctx, ts_inst, type, i, TRS_RES_OP_RESET);
        }
    }
    ka_task_mutex_unlock(&ts_inst->res_mng[type].mutex);
    return ret;
}

static int trs_res_id_ctrl_comm(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst,
                                struct trs_res_id_para *para)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    enum trs_id_type res_type = (enum trs_id_type)para->res_type;
    u32 res_id = para->id;
    int ret;

    if ((para->prop == DRV_ID_RESET) && (res_id == KA_U32_MAX)) {
        return trs_set_res_id_ctrl_of_proc(proc_ctx, ts_inst, (u32)res_type);
    }

    ret = trs_res_id_get(inst, res_type, res_id);
    if (ret != 0) {
        trs_err("Id not belong to proc. (devid=%u; tsid=%u; res_type=%d; res_id=%u)\n", inst->devid, inst->tsid,
                res_type, res_id);
        return -EINVAL;
    }

    if (para->prop == DRV_ID_RECORD) {
        ret = trs_set_res_id_ctrl(proc_ctx, ts_inst, (u32)res_type, res_id, TRS_RES_OP_RECORD);
    } else if (para->prop == DRV_ID_RESET) {
        ret = trs_set_res_id_ctrl(proc_ctx, ts_inst, (u32)res_type, res_id, TRS_RES_OP_RESET);
    } else {
        trs_warn("Not support. (devid=%u; tsid=%u; prop=%d)\n", inst->devid, inst->tsid, para->prop);
        ret = -EOPNOTSUPP;
    }
    trs_res_id_put(inst, res_type, res_id);

    return ret;
}

static int trs_notify_cfg(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct trs_res_id_para *para)
{
    return trs_res_id_ctrl_comm(proc_ctx, ts_inst, para);
}

static int trs_cnt_notify_cfg(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst,
                              struct trs_res_id_para *para)
{
    return trs_res_id_ctrl_comm(proc_ctx, ts_inst, para);
}

static int trs_event_cfg(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct trs_res_id_para *para)
{
    return trs_res_id_ctrl_comm(proc_ctx, ts_inst, para);
}

static int (*const trs_res_id_cfg_handles[TRS_MAX_ID_TYPE])(struct trs_proc_ctx *proc_ctx,
                                                            struct trs_core_ts_inst *ts_inst,
                                                            struct trs_res_id_para *para) = {
    [TRS_STREAM] = trs_stream_cfg,
    [TRS_EVENT] = trs_event_cfg,
    [TRS_NOTIFY] = trs_notify_cfg,
    [TRS_CNT_NOTIFY] = trs_cnt_notify_cfg};

int trs_res_id_cfg(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct trs_res_id_para *para)
{
    struct trs_id_inst *inst = &ts_inst->inst;

    if (trs_res_id_cfg_handles[para->res_type] == NULL) {
        trs_warn("Not support. (devid=%u; tsid=%u; res_type=%d)\n", inst->devid, inst->tsid, para->res_type);
        return -EOPNOTSUPP;
    }

    return trs_res_id_cfg_handles[para->res_type](proc_ctx, ts_inst, para);
}

static int trs_stream_id_ctrl(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, u32 stream_id, u32 cmd)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    struct trs_stream_ctx *stream_ctx = NULL;
    int sqid;

    if ((stream_id >= trs_res_get_id_num(ts_inst, TRS_STREAM)) || (ts_inst->ops.res_id_ctrl == NULL)) {
        trs_err("Invalid para. (devid=%u; tsid=%u; stream_id=%u)\n", inst->devid, inst->tsid, stream_id);
        return -EINVAL;
    }

    stream_ctx = &ts_inst->stream_ctx[stream_id];
    sqid = stream_ctx->sq;
    if ((sqid < 0) || ((u32)sqid >= trs_res_get_max_id(ts_inst, TRS_HW_SQ))) {
        trs_err("Stream no sq. (devid=%u; tsid=%u; stream_id=%u; sqId=%d)\n", inst->devid, inst->tsid, stream_id, sqid);
        return -EINVAL;
    }

    if (!trs_proc_has_res(proc_ctx, ts_inst, TRS_STREAM, stream_id)) {
        if (!trs_is_host_pid_match(proc_ctx->pid, stream_ctx->host_pid)) {
            trs_err("Stream not belong to proc. (devid=%u; tsid=%u; stream_id=%u; pid=%d)\n", inst->devid, inst->tsid,
                    stream_id, stream_ctx->host_pid);
            return -EINVAL;
        }
    }

    return ts_inst->ops.res_id_ctrl(inst, TRS_HW_SQ, (u32)sqid, cmd);
}

int trs_res_id_enable(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct trs_res_id_para *para)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    int ret;

    if (para->res_type == TRS_STREAM) {
        ret = trs_stream_id_ctrl(proc_ctx, ts_inst, para->id, TRS_RES_OP_ENABLE);
    } else {
        trs_debug("Not support. (devid=%u; tsid=%u; res_type=%d)\n", inst->devid, inst->tsid, para->res_type);
        ret = -EOPNOTSUPP;
    }

    return ret;
}

int trs_res_id_disable(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct trs_res_id_para *para)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    int ret;

    if (para->res_type == TRS_STREAM) {
        ret = trs_stream_id_ctrl(proc_ctx, ts_inst, para->id, TRS_RES_OP_DISABLE);
    } else {
        trs_warn("Not support. (devid=%u; tsid=%u; res_type=%d)\n", inst->devid, inst->tsid, para->res_type);
        ret = -EOPNOTSUPP;
    }

    return ret;
}

void trs_res_id_recycle(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, int res_type, u32 res_id)
{
    (void)_trs_res_id_free(proc_ctx, ts_inst, res_type, 0, res_id);
}

void trs_stream_set_bind_sqcq(struct trs_core_ts_inst *ts_inst, u32 stream_id, u32 sqid, u32 cqid, int host_pid)
{
    struct trs_stream_ctx *stream_ctx = &ts_inst->stream_ctx[stream_id];

    stream_ctx->sq = (int)sqid;
    stream_ctx->cq = (int)cqid;
    stream_ctx->host_pid = host_pid;
}

void trs_sqcq_reg_map(struct trs_core_ts_inst *ts_inst, struct trs_sqcq_reg_map_para *para)
{
    struct trs_res_mng *res_mng = &ts_inst->res_mng[TRS_STREAM];
    if (ts_inst->ops.sqcq_reg_map != NULL) {
        ts_inst->sq_ctx[para->sqid].reg_mem.cp_proc_state = (ts_inst->ops.sqcq_reg_map(&ts_inst->inst, para) == 0) ? 0 :
                                                                                                                     1;
    }
    if (para->stream_id < res_mng->max_id) {
        trs_stream_set_bind_sqcq(ts_inst, para->stream_id, para->sqid, para->cqid, para->host_pid);
    }
}

void trs_sqcq_reg_unmap(struct trs_core_ts_inst *ts_inst, struct trs_sqcq_reg_map_para *para)
{
    struct trs_res_mng *res_mng = &ts_inst->res_mng[TRS_STREAM];
    if (para->stream_id < res_mng->max_id) {
        trs_stream_set_bind_sqcq(ts_inst, para->stream_id, (u32)-1, (u32)-1, -1);
    }
    if (ts_inst->ops.sqcq_reg_unmap != NULL) {
        (void)ts_inst->ops.sqcq_reg_unmap(&ts_inst->inst, para);
    }
}

int trs_stream_bind_remote_sqcq(struct trs_id_inst *inst, u32 stream_id, u32 sqid, u32 cqid, int host_pid)
{
    struct trs_core_ts_inst *ts_inst = NULL;

    if (stream_id == KA_U32_MAX) {
        return 0;
    }

    if (inst == NULL) {
        trs_err("Null ptr. (stream_id=%u)\n", stream_id);
        return -EINVAL;
    }

    ts_inst = trs_core_ts_inst_get(inst);
    if (ts_inst == NULL) {
        trs_err("Invalid para. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return -EINVAL;
    }

    if (stream_id >= trs_res_get_max_id(ts_inst, TRS_STREAM)) {
        trs_core_ts_inst_put(ts_inst);
        trs_err("Invalid para. (devid=%u; tsid=%u; stream_id=%u; sqid=%u; cqid=%u; host_pid=%d)\n", inst->devid,
                inst->tsid, stream_id, sqid, cqid, host_pid);
        return -EINVAL;
    }

    trs_stream_set_bind_sqcq(ts_inst, stream_id, sqid, cqid, host_pid);
    trs_core_ts_inst_put(ts_inst);
    return 0;
}
KA_EXPORT_SYMBOL_GPL(trs_stream_bind_remote_sqcq);

int trs_get_stream_ctx(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, u32 stream_id,
                       struct trs_stream_ctx *stm_ctx)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    struct trs_stream_ctx *stream_ctx = NULL;

    if (trs_proc_has_res(proc_ctx, ts_inst, TRS_STREAM, stream_id) == false) {
        trs_err("Stream is not belong to proc. (devid=%u; tsid=%u; stream_id=%u)\n", inst->devid, inst->tsid,
                stream_id);
        return -EINVAL;
    }

    stream_ctx = &ts_inst->stream_ctx[stream_id];

    stm_ctx->sq = stream_ctx->sq;
    stm_ctx->cq = stream_ctx->cq;
    stm_ctx->logic_cq = stream_ctx->logic_cq;

    return 0;
}

static int trs_res_stream_init(struct trs_core_ts_inst *ts_inst)
{
    u32 stream_num = trs_res_get_max_id(ts_inst, TRS_STREAM);
    struct trs_id_inst *inst = &ts_inst->inst;
    struct trs_stream_ctx *stream_ctx = NULL;
    u32 i;

    if (stream_num == 0) {
        return 0;
    }

    stream_ctx = (struct trs_stream_ctx *)trs_vzalloc(sizeof(struct trs_stream_ctx) * stream_num);
    if (stream_ctx == NULL) {
        trs_err("Mem alloc failed. (devid=%u; tsid=%u; size=%lx)\n", inst->devid, inst->tsid,
                sizeof(struct trs_stream_ctx) * stream_num);
        return -ENOMEM;
    }

    for (i = 0; i < stream_num; i++) {
        trs_stream_ctx_init(&stream_ctx[i]);
    }

    ts_inst->stream_ctx = stream_ctx;

    return 0;
}

static void trs_res_stream_uninit(struct trs_core_ts_inst *ts_inst)
{
    if (ts_inst->stream_ctx != NULL) {
        trs_vfree(ts_inst->stream_ctx);
        ts_inst->stream_ctx = NULL;
    }
}

static int trs_res_id_init(struct trs_core_ts_inst *ts_inst, struct trs_res_mng *res_mng, int res_type)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    int id_type = trs_res_get_id_type(inst, trs_res_replace_res_type(ts_inst, res_type));
    int ret;

    ret = trs_id_get_total_num(inst, id_type, &res_mng->id_num);
    if (ret != 0) {
        trs_debug("Res not support. (devid=%u; tsid=%u; res_type=%d)\n", inst->devid, inst->tsid, res_type);
        res_mng->id_num = 0;
    }

    if (res_mng->id_num > 0) {
        ret = trs_id_get_max_id(inst, id_type, &res_mng->max_id);
        if (ret != 0) {
            trs_err("Get max id failed. (devid=%u; tsid=%u; res_type=%d; ret=%d)\n", inst->devid, inst->tsid, res_type,
                    ret);
            return ret;
        }

        res_mng->ids = (struct trs_res_ids *)trs_vzalloc(sizeof(struct trs_res_ids) * res_mng->max_id);
        if (res_mng->ids == NULL) {
            trs_err("Mem alloc failed. (devid=%u; tsid=%u; res_type=%d; size=%lx)\n", inst->devid, inst->tsid, res_type,
                    sizeof(struct trs_res_ids) * res_mng->max_id);
            return -ENOMEM;
        }
    }

    ka_task_mutex_init(&res_mng->mutex);
    res_mng->res_type = res_type;
    res_mng->ts_inst = ts_inst;
    return 0;
}

static void trs_res_id_uninit(struct trs_res_mng *res_mng)
{
    if (res_mng->ids != NULL) {
        ka_task_mutex_destroy(&res_mng->mutex);
        trs_vfree(res_mng->ids);
        res_mng->ids = NULL;
    }
}

void trs_res_mng_uninit(struct trs_core_ts_inst *ts_inst)
{
    int i;

    trs_res_stream_uninit(ts_inst);

    for (i = 0; i < TRS_CORE_MAX_ID_TYPE; i++) {
        trs_res_id_uninit(&ts_inst->res_mng[i]);
    }
}

int trs_res_mng_init(struct trs_core_ts_inst *ts_inst)
{
    int i, ret;

    for (i = 0; i < TRS_CORE_MAX_ID_TYPE; i++) {
        ret = trs_res_id_init(ts_inst, &ts_inst->res_mng[i], i);
        if (ret != 0) {
            trs_res_mng_uninit(ts_inst);
            return ret;
        }
    }

    ret = trs_res_stream_init(ts_inst);
    if (ret != 0) {
        trs_res_mng_uninit(ts_inst);
        return ret;
    }

    return 0;
}
