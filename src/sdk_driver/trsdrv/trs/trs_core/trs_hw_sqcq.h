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

#ifndef TRS_HW_SQCQ_H
#define TRS_HW_SQCQ_H

#include "ka_type.h"
#include "ascend_hal_define.h"
#include "trs_ioctl.h"
#include "trs_proc.h"

#define TRS_HW_SQE_SIZE 64
struct trs_core_ts_inst;
struct trs_sq_ctx;

enum trs_sq_send_sched_type {
    SQ_SEND_NON_FAIR_MODE,
    SQ_SEND_FAIR_MODE
};

int trs_hw_sqcq_alloc(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct halSqCqInputInfo *para);
int trs_hw_sqcq_free(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct halSqCqFreeInfo *para);
int trs_hw_sqcq_get(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct halSqCqInputInfo *para);
int trs_hw_sqcq_restore(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct halSqCqInputInfo *para);

int trs_hw_sqcq_config(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct halSqCqConfigInfo *para);
int trs_sqcq_query(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct halSqCqQueryInfo *para);

int trs_hw_sqcq_send(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct halTaskSendInfo *para);
int trs_hw_sqcq_recv(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct halReportRecvInfo *para);
void trs_proc_diable_sq_status(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, int res_type,
                               u32 res_id);
void trs_hw_sqcq_recycle(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, int res_type, u32 res_id);

void trs_hw_sq_trigger_irq_hw_res_uninit(struct trs_core_ts_inst *ts_inst);

int trs_hw_sqcq_init(struct trs_core_ts_inst *ts_inst);
void trs_hw_sqcq_uninit(struct trs_core_ts_inst *ts_inst);
int trs_hw_sq_send_thread_create(struct trs_core_ts_inst *ts_inst);
void trs_hw_sq_send_thread_destroy(struct trs_core_ts_inst *ts_inst);
int trs_hw_sqcq_dma_desc_create(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst,
                                struct trs_cmd_dma_desc *para);
int trs_sq_switch_stream(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst,
                         struct sq_switch_stream_info *info);

int trs_hw_sq_send_task(struct trs_sq_ctx *sq_ctx, enum trs_sq_send_sched_type sched_mode);
#endif
