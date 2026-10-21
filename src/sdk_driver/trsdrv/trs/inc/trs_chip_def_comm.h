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
#ifndef TRS_CHIP_DEF_COMM_H
#define TRS_CHIP_DEF_COMM_H

#include "ka_type.h"

#include "trs_res_id_def.h"

struct trs_ts_db_cfg {
    bool flag;
    u32 start;
    u32 end;
};

enum {
    TRS_DB_OFFLINE_MBOX = 0,
    TRS_DB_ONLINE_MBOX,
    TRS_DB_TASK_SQ,
    TRS_DB_TASK_CQ,
    TRS_DB_MAINT_SQ,
    TRS_DB_MAINT_CQ,
    TRS_DB_TRIGGER_SQ,
    TRS_DB_TYPE_MAX,
};

/* later replace with pub def */
#define TRS_DEVICE_MAX_PHY_DEV 4
#define TRS_DEVICE_VIR_DEV_BASE 32
#define TRS_DEVICE_VIR_DEV_NUM 32
#define TRS_DEVICE_MAX_DEV 64

enum {
    TRS_CHIP_TYPE_UNKNOWN = 0,
    TRS_CHIP_TYPE_MINI_V1,
    TRS_CHIP_TYPE_CLOUD_V1,
    TRS_CHIP_TYPE_MINI_V2,
    TRS_CHIP_TYPE_CLOUD_V2,
    TRS_CHIP_TYPE_MINI_V3,
    TRS_CHIP_TYPE_CLOUD_V4,
    TRS_CHIP_TYPE_CLOUD_V5,
    TRS_CHIP_TYPE_MAX
};

static const char *trs_chip_type_name[TRS_CHIP_TYPE_MAX] = {
    [TRS_CHIP_TYPE_UNKNOWN] = "Unknown",   [TRS_CHIP_TYPE_MINI_V1] = "Mini_V1",   [TRS_CHIP_TYPE_CLOUD_V1] = "Cloud_V1",
    [TRS_CHIP_TYPE_MINI_V2] = "Mini_V2",   [TRS_CHIP_TYPE_CLOUD_V2] = "Cloud_V2", [TRS_CHIP_TYPE_MINI_V3] = "Mini_V3",
    [TRS_CHIP_TYPE_CLOUD_V4] = "Cloud_V4", [TRS_CHIP_TYPE_CLOUD_V5] = "Cloud_V5"};

static inline const char *trs_chip_type_to_name(int chip_type)
{
    if ((chip_type < TRS_CHIP_TYPE_UNKNOWN) || (chip_type >= TRS_CHIP_TYPE_MAX)) {
        return "MaxChipType";
    }
    return trs_chip_type_name[chip_type];
}

static inline int trs_get_hw_type_by_chip_type(int chip_type)
{
    if ((chip_type == TRS_CHIP_TYPE_MINI_V3) || (chip_type == TRS_CHIP_TYPE_CLOUD_V2) ||
        (chip_type == TRS_CHIP_TYPE_CLOUD_V4) || (chip_type == TRS_CHIP_TYPE_CLOUD_V5)) {
        return TRS_HW_TYPE_STARS;
    } else {
        return TRS_HW_TYPE_TSCPU;
    }
}

#endif /* TRS_CHIP_DEF_COMM_H */
