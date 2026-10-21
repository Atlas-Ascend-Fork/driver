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
#include "stars_notify_tbl.h"

#define STARS_TABLE_NOTIFY_NUM 512

/*
 * DEFINE REGISTER UNION
 */
/* Define the union StarsNotifyTableSlice */
typedef union {
    /* Define the struct bits */
    struct {
        /*
         * 0: notify_id has not be written to,
         *    or there was a notify_wait successfully passed and auto cleared
         * 1: notify_id has been written
         *    Software should ensure when writing to this bit, the value was 0.
         *    Only the case that during TS FW initialziation,
         *    we would allow using write to to notify table entry with flag == 1
         */
        unsigned int notifyTableFlagSlice : 1; /* [0] */
        /*
         * 1: notify pending  will be clear.
         * 0: notify pending  will not be clear.
         *    when ffts+ notify wait occurs before notify record for this notify ID,
         *    the corresponding pending will be set 1.when SLICE is reset or destroys.
         *    the pending need to be clear.
         */
        unsigned int notifyTablePendingClrSlice : 1; /* [1] */
        unsigned int reserved : 30;                  /* [31:2] */
    } bits;

    /* Define an unsigned member */
    unsigned int u32;
} StarsNotifyTableSlice;

/*
 * DEFINE EVENT TABLE INFO STRUCT
 */
typedef struct {
    StarsNotifyTableSlice starsNotifyTableSlice[STARS_TABLE_NOTIFY_NUM];
    unsigned int reserved[15872];
} StarsNotifyTableSliceInfo;

/*
 * DEFINE GLOBAL STRUCT
 */
typedef struct {
    StarsNotifyTableSliceInfo StarsNotifyGroupTable[16];
} StarsNotifyTblRegsType;

static StarsNotifyTableSlice *trs_get_stars_notify_tab_slice(StarsNotifyTblRegsType *tbl_info, u32 id)
{
    u32 group_id = id / STARS_TABLE_NOTIFY_NUM;
    u32 offset = id % STARS_TABLE_NOTIFY_NUM;
    StarsNotifyTableSliceInfo *group_info = &(tbl_info->StarsNotifyGroupTable[group_id]);
    return &(group_info->starsNotifyTableSlice[offset]);
}

static int trs_notify_tbl_get_flag(void *tbl_info, u32 id)
{
    StarsNotifyTblRegsType *tbl = (StarsNotifyTblRegsType *)tbl_info;
    StarsNotifyTableSlice *slice = trs_get_stars_notify_tab_slice(tbl, id);
    return (int)slice->bits.notifyTableFlagSlice;
}

static u32 trs_notify_tbl_set_flag(void *tbl_info, u32 id, u32 status)
{
    StarsNotifyTblRegsType *tbl = (StarsNotifyTblRegsType *)tbl_info;
    StarsNotifyTableSlice *slice = trs_get_stars_notify_tab_slice(tbl, id);

    slice->bits.notifyTableFlagSlice = status;
    trs_debug("Read after clear. (id=%u; read_status=%u; status=%u)\n", id, slice->bits.notifyTableFlagSlice, status);
    return 0;
}

static void trs_notify_tbl_set_pending_clr(void *tbl_info, u32 id, u32 status)
{
    StarsNotifyTblRegsType *tbl = (StarsNotifyTblRegsType *)tbl_info;
    StarsNotifyTableSlice *slice = trs_get_stars_notify_tab_slice(tbl, id);
    slice->bits.notifyTablePendingClrSlice = status;
}

static struct stars_notify_tbl_ops ops = {
    .get_flag = trs_notify_tbl_get_flag,
    .set_flag = trs_notify_tbl_set_flag,
    .set_pending_clr = trs_notify_tbl_set_pending_clr,
};

int stars_notify_tbl_init_dev_cloud_v2(u32 udevid);
int stars_notify_tbl_init_dev_cloud_v2(u32 udevid)
{
    u32 chip_type = uda_get_chip_type(udevid);
    if (chip_type != HISI_CLOUD_V2) {
        return 0;
    }
    trs_notify_tbl_set_ops(udevid, &ops);
    return 0;
}
DECLAER_FEATURE_AUTO_INIT_DEV(stars_notify_tbl_init_dev_cloud_v2, FEATURE_LOADER_STAGE_0);

void stars_notify_tbl_uninit_dev_cloud_v2(u32 udevid);
void stars_notify_tbl_uninit_dev_cloud_v2(u32 udevid)
{
    u32 chip_type = uda_get_chip_type(udevid);
    if (chip_type != HISI_CLOUD_V2) {
        return;
    }
    trs_notify_tbl_clear_ops(udevid);
}
DECLAER_FEATURE_AUTO_UNINIT_DEV(stars_notify_tbl_uninit_dev_cloud_v2, FEATURE_LOADER_STAGE_0);
