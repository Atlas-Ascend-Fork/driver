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
#ifndef STARS_NOTIFY_TBL_H
#define STARS_NOTIFY_TBL_H
#include "ka_type.h"

#include "trs_pub_def.h"

struct stars_notify_tbl_ops {
    int (*get_flag)(void *tbl_info, u32 id);
    u32 (*set_flag)(void *tbl_info, u32 id, u32 status);
    void (*set_pending_clr)(void *tbl_info, u32 id, u32 status);
};

int trs_init_notify_tbl_ns_base_addr(struct trs_id_inst *inst);
void trs_uninit_notify_tbl_ns_base_addr(struct trs_id_inst *inst);
int trs_stars_get_notify_tbl_flag(struct trs_id_inst *inst, u32 id);
void trs_stars_set_notify_tbl_flag(struct trs_id_inst *inst, u32 id, u32 status);
void trs_stars_set_notify_tbl_pending_clr(struct trs_id_inst *inst, u32 id, u32 status);
void trs_notify_tbl_set_ops(u32 devid, struct stars_notify_tbl_ops *ops);
void trs_notify_tbl_clear_ops(u32 devid);
#endif