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

#ifndef TRS_SEC_EH_CFG_H
#define TRS_SEC_EH_CFG_H

#include "ka_task_pub.h"
#include "pbl_kref_safe.h"

#include "trs_res_id_def.h"
#include "trs_pub_def.h"

struct trs_sec_eh_sq_ctx {
    u8 *d_addr;   // alloc by pm host
    u64 sq_paddr; // for free mem
    int pid;      // for free mem
    void *sq_dev_vaddr;

    u32 sqe_size;
    u32 sq_depth;
    u32 mem_type;
    u32 long_sqe_cnt;
    u32 sq_create_cmd_type;
};

struct trs_sec_eh_id_info {
    int type;
    u32 start;
    u32 end;

    int *id_proc_map;

    u32 bitnum;
    u32 bitmap;
    u32 num_per_bit;
};

struct trs_sec_eh_ts_inst {
    struct trs_id_inst inst;
    struct trs_id_inst pm_inst;

    u32 rtsq_bitmap;
    u32 event_bitmap;
    u32 notify_bitmap;

    struct trs_sec_eh_id_info id_info[TRS_CORE_MAX_ID_TYPE];

    struct trs_sec_eh_sq_ctx *sq_ctx; // Apply for the memory by max ID. Query the memory before de-initialization.
    ka_mutex_t mutex;

    struct kref_safe ref;
};

int trs_sec_eh_ts_inst_create(struct trs_id_inst *inst);
void trs_sec_eh_ts_inst_destroy(struct trs_id_inst *inst);

struct trs_sec_eh_ts_inst *trs_sec_eh_ts_inst_get(struct trs_id_inst *inst);
void trs_sec_eh_ts_inst_put(struct trs_sec_eh_ts_inst *sec_eh_cfg);

#endif /* TRS_SEC_EH_CFG_H */
