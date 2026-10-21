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

#ifndef TRS_SEC_EH_SQCQ_H
#define TRS_SEC_EH_SQCQ_H

#include "ka_type.h"

#include "trs_pub_def.h"

#include "trs_sec_eh_cfg.h"

struct trs_sec_eh_sq_ctx_info {
    u64 addr_offset;
    u32 sqid;
    u16 sqdepth;
    u8 sqesize;
    u32 mem_type;
    u16 sq_create_cmd_type;

    int pid;

    void *sq_addr;
    u64 sq_paddr;
    void *sq_dev_vaddr; // mapped sq device vaddr
};

struct trs_sec_eh_cq_ctx_info {
    u32 cqid;
    u16 cqdepth;
    u8 cqesize;
    u64 cq_paddr;
};

/* for mbox */
void trs_sec_eh_sq_ctx_init(struct trs_sec_eh_ts_inst *sec_eh_cfg, struct trs_sec_eh_sq_ctx_info *ctx);
void trs_sec_eh_sq_ctx_uninit(struct trs_sec_eh_ts_inst *sec_eh_cfg, u32 sqid);
u64 trs_sec_eh_alloc_sq_mem(struct trs_sec_eh_ts_inst *sec_eh_cfg, struct trs_sec_eh_sq_ctx_info *ctx);
void trs_sec_eh_free_sq_mem(struct trs_sec_eh_ts_inst *sec_eh_cfg, u32 sqid);
int trs_sec_eh_check_and_update_cq_ctx_info(struct trs_sec_eh_ts_inst *sec_eh_cfg, struct trs_sec_eh_cq_ctx_info *ctx);

/* for vpc */
void trs_sec_eh_free_sq_mem_all(struct trs_id_inst *inst);
int _trs_sec_eh_sqe_update(struct trs_id_inst *inst, int pid, u32 sqid, u32 sqeid, u8 data[]);

#endif /* TRS_SEC_EH_SQCQ_H */
