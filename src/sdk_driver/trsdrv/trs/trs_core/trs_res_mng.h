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

#ifndef TRS_RES_MNG_H
#define TRS_RES_MNG_H

#include "ka_task_pub.h"
#include "trs_ioctl.h"
#include "trs_core.h"
#include "trs_proc.h"

struct trs_core_ts_inst;

struct trs_stream_ctx {
    int host_pid;
    int sq;
    int cq;
    int logic_cq; /* -1: invalid */
    u64 stream_base_addr;
    u32 depth;
    u32 tail;
    u32 long_task_cnt;
    struct trs_sq_mem_handle mem_attr;
};

#define RES_STATUS_NORMAL 0
#define RES_STATUS_DEL 1
struct trs_res_ids {
    int status; /* 0:normal(alloc: pid is set, free: pid is 0), 1:del */
    s64 task_id;
    int pid;
    int ref;
    bool is_agent_res; /* if agent host app res id, used in ub scene */
    int ref_hpid;      /* host pid of remote get, 0 means not remote-get */
    u32 ref_seq;       /* seq of the remote get for idempotency check */
};

struct trs_res_mng {
    struct trs_core_ts_inst *ts_inst;
    ka_mutex_t mutex;
    int res_type;
    u32 id_num;
    u32 max_id;
    u32 use_num;
    struct trs_res_ids *ids;
};

u32 trs_res_get_id_num(struct trs_core_ts_inst *ts_inst, int res_type);
u32 trs_res_get_max_id(struct trs_core_ts_inst *ts_inst, int res_type);
u32 trs_res_get_id_used_num(struct trs_core_ts_inst *ts_inst, int res_type);
void trs_sqcq_reg_map(struct trs_core_ts_inst *ts_inst, struct trs_sqcq_reg_map_para *para);
void trs_sqcq_reg_unmap(struct trs_core_ts_inst *ts_inst, struct trs_sqcq_reg_map_para *para);
void trs_stream_set_bind_sqcq(struct trs_core_ts_inst *ts_inst, u32 stream_id, u32 sqid, u32 cqid, int host_pid);
int trs_stream_task_fill_proc(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst,
                              struct trs_stream_task_para *para);
int trs_stream_get_sq_mem_handle(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst,
                                 struct trs_stream_ctx *stream_ctx, void *stream_mem, u32 depth);
void trs_stream_put_sq_mem_handle(struct trs_core_ts_inst *ts_inst, struct trs_proc_ctx *proc_ctx,
                                  struct trs_stream_ctx *stream_ctx);
int trs_res_id_pre_del(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, int res_type, u32 res_id);
int trs_res_id_num_query(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct trs_res_id_para *para);
int trs_res_id_max_query(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct trs_res_id_para *para);
int trs_res_id_used_query(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst,
                          struct trs_res_id_para *para);
int trs_res_id_avail_query(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst,
                           struct trs_res_id_para *para);
int trs_res_id_reg_offset_query(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst,
                                struct trs_res_id_para *para);
int trs_res_id_reg_size_query(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst,
                              struct trs_res_id_para *para);
int trs_res_id_cfg(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct trs_res_id_para *para);
int trs_res_id_alloc(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct trs_res_id_para *para);
int trs_res_id_free(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct trs_res_id_para *para);
int trs_res_id_enable(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct trs_res_id_para *para);
int trs_res_id_disable(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct trs_res_id_para *para);
void trs_res_id_recycle(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, int res_type, u32 res_id);

int trs_get_stream_logic_cq(struct trs_core_ts_inst *ts_inst, u32 stream_id);

int trs_res_get_id_type(struct trs_id_inst *id_inst, int res_type);
int trs_res_replace_res_type(struct trs_core_ts_inst *ts_inst, int res_type);
int trs_get_stream_ctx(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, u32 stream_id,
                       struct trs_stream_ctx *stm_ctx);

int trs_res_mng_init(struct trs_core_ts_inst *ts_inst);
void trs_res_mng_uninit(struct trs_core_ts_inst *ts_inst);

#endif
