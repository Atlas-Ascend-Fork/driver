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
#ifndef SOC_ADAPT_H__
#define SOC_ADAPT_H__

#include "ka_type.h"

#include "trs_pub_def.h"
#include "ascend_kernel_hal.h"
#include "trs_core.h"

int trs_soc_get_db_stride(struct trs_id_inst *inst, size_t *size);
int trs_soc_get_db_cfg(struct trs_id_inst *inst, int db_type, u32 *start, u32 *end);
void trs_soc_set_chip_type(u32 phy_devid, int chip_type);
int trs_soc_get_chip_type(u32 phy_devid);
int trs_soc_get_notify_offset(struct trs_id_inst *inst, u32 notify_id, u32 *offset);
int trs_soc_get_notify_size(struct trs_id_inst *inst, size_t *notify_size);
int trs_soc_get_cnt_notify_offset(struct trs_id_inst *inst, u32 cnt_notify_id, u32 *offset);
int trs_soc_get_cnt_notify_size(struct trs_id_inst *inst, size_t *cnt_notify_size);
int trs_soc_get_event_offset(struct trs_id_inst *inst, u32 event_id, u32 *offset);
u32 trs_soc_get_sq_mem_side(struct trs_id_inst *inst, struct trs_chan_type *types);
u32 trs_soc_get_cq_mem_side(struct trs_id_inst *inst);
int trs_soc_get_stars_sched_stride(struct trs_id_inst *inst, size_t *stride);
int trs_soc_get_hw_type(u32 phy_devid);
int trs_soc_get_hwcq_rsv_mem_type(struct trs_id_inst *inst, int *rsv_mem_type);
int trs_soc_get_sq_head_tail_reg_size(struct trs_id_inst *inst, size_t *reg_size);
int trs_soc_get_sq_head_reg_offset(struct trs_id_inst *inst, u32 *offset);
int trs_soc_get_sq_tail_reg_offset(struct trs_id_inst *inst, u32 *offset);
int trs_soc_get_sq_status_reg_offset(struct trs_id_inst *inst, u32 *offset);
int trs_soc_get_cq_head_reg_offset(struct trs_id_inst *inst, u32 *offset);
int trs_soc_get_cq_tail_reg_offset(struct trs_id_inst *inst, u32 *offset);
int trs_soc_chan_stars_init(struct trs_id_inst *inst);
void trs_soc_chan_stars_uninit(struct trs_id_inst *inst);
int trs_soc_chan_stars_ops_init(struct trs_id_inst *inst);
void trs_soc_chan_stars_ops_uninit(struct trs_id_inst *inst);
struct trs_core_adapt_ops *trs_soc_core_get_stars_adapt_ops(struct trs_id_inst *inst);
struct trs_chan_adapt_ops *trs_soc_chan_get_stars_adapt_ops(struct trs_id_inst *inst);
int trs_soc_sq_send_trigger_db_init(struct trs_id_inst *inst);
void trs_soc_sq_send_trigger_db_uninit(struct trs_id_inst *inst);
bool trs_soc_is_support_soft_mbox(struct trs_id_inst *inst);
bool trs_soc_is_support_virt(struct trs_id_inst *inst);
bool trs_soc_support_sq_rsvmem_map(struct trs_id_inst *inst);
bool trs_soc_support_hw_ts_mbox(struct trs_id_inst *inst);
#endif /* SOC_ADAPT_H__ */