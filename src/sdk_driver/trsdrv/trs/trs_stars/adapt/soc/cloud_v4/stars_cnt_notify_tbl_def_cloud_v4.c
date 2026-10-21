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
#include "pbl/pbl_uda.h"
#include "trs_stars_auto_init.h"
#include "stars_cnt_notify_tbl.h"

#define STARS_TABLE_CNT_NOTIFY_NUM 128U
#define STARS_TABLE_CNT_NOTIFY_UNIT_NUM 8U
#define STARS_TABLE_CNT_NOTIFY_NUM_PER_UNIT 16U
#define STARS_TABLE_CNT_NOTIFY_GRP_NUM 16U

typedef union {
    struct {
        unsigned int notify_cnt_status_slice;
        unsigned int rsv0;
        unsigned int notify_cnt_add_slice : 1;
        unsigned int rsv1 : 31;
        unsigned int rsv2;
        unsigned int notify_cnt_bit_wr_slice;
        unsigned int rsv3;
        unsigned int notify_cnt_bit_clr_slice;
    } regs;

    unsigned long long data[4]; /* 4 reg for each cnt notify id, each reg use 8 bytes */
} stars_cnt_notify_table_slice;

typedef struct {
    stars_cnt_notify_table_slice stars_cnt_notify_table_slice[STARS_TABLE_CNT_NOTIFY_NUM_PER_UNIT];
    unsigned int reserved[896];
} stars_cnt_notify_table_unit; /* 4KB */

typedef struct {
    stars_cnt_notify_table_unit stars_cnt_notify_table_unit[STARS_TABLE_CNT_NOTIFY_UNIT_NUM]; /* 32K */
    stars_cnt_notify_table_unit reserved[STARS_TABLE_CNT_NOTIFY_UNIT_NUM];                    /* 32K */
} stars_cnt_notify_table_slice_info;                                                          /* 64KB */

typedef struct {
    stars_cnt_notify_table_slice_info stars_cnt_notify_group_table[STARS_TABLE_CNT_NOTIFY_GRP_NUM];
} stars_cnt_notify_tbl_regs_type;

static stars_cnt_notify_table_slice *trs_get_stars_cnt_notify_tab_slice(stars_cnt_notify_tbl_regs_type *tbl_info,
                                                                        u32 id)
{
    u32 group_id = id / STARS_TABLE_CNT_NOTIFY_NUM;
    u32 unit = (id % STARS_TABLE_CNT_NOTIFY_NUM) / STARS_TABLE_CNT_NOTIFY_NUM_PER_UNIT;
    u32 offset = id % STARS_TABLE_CNT_NOTIFY_NUM % STARS_TABLE_CNT_NOTIFY_NUM_PER_UNIT;
    stars_cnt_notify_table_slice_info *group_info = &(tbl_info->stars_cnt_notify_group_table[group_id]);
    stars_cnt_notify_table_unit *unit_info = &(group_info->stars_cnt_notify_table_unit[unit]);
    return &(unit_info->stars_cnt_notify_table_slice[offset]);
}

static u32 trs_cnt_notify_tbl_set_flag(void *tbl_info, u32 id, u32 status)
{
    stars_cnt_notify_tbl_regs_type *tbl = (stars_cnt_notify_tbl_regs_type *)tbl_info;
    stars_cnt_notify_table_slice *slice = trs_get_stars_cnt_notify_tab_slice(tbl, id);

    slice->regs.notify_cnt_status_slice = status;
    trs_debug("Read cnt notify reg after clear. (id=%u; read_status=%u; status=%u)\n", id,
              slice->regs.notify_cnt_status_slice, status);
    return 0;
}

static struct stars_cnt_notify_tbl_ops cnt_ops = {
    .set_flag = trs_cnt_notify_tbl_set_flag,
};

int stars_cnt_notify_tbl_init_dev_cloud_v4(u32 udevid);
int stars_cnt_notify_tbl_init_dev_cloud_v4(u32 udevid)
{
    u32 chip_type = uda_get_chip_type(udevid);
    if (chip_type != HISI_CLOUD_V4) {
        return 0;
    }
    trs_cnt_notify_tbl_set_ops(udevid, &cnt_ops);
    return 0;
}
DECLAER_FEATURE_AUTO_INIT_DEV(stars_cnt_notify_tbl_init_dev_cloud_v4, FEATURE_LOADER_STAGE_0);

void stars_cnt_notify_tbl_uninit_dev_cloud_v4(u32 udevid);
void stars_cnt_notify_tbl_uninit_dev_cloud_v4(u32 udevid)
{
    u32 chip_type = uda_get_chip_type(udevid);
    if (chip_type != HISI_CLOUD_V4) {
        return;
    }
    trs_cnt_notify_tbl_clear_ops(udevid);
}
DECLAER_FEATURE_AUTO_UNINIT_DEV(stars_cnt_notify_tbl_uninit_dev_cloud_v4, FEATURE_LOADER_STAGE_0);
