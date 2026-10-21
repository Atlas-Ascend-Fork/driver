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

#include "pbl_uda.h"
#include "uda_proc_fs.h"
#include "uda_proc_fs_adapt.h"

const char *uda_get_chip_name(unsigned int type)
{
    static const char *chip_name[HISI_CHIP_NUM] = {
        [HISI_MINI_V1] = "mini_v1",   [HISI_CLOUD_V1] = "cloud_v1",     [HISI_MINI_V2] = "mini_v2",
        [HISI_CLOUD_V2] = "cloud_v2", [HISI_MINI_V3] = "mini_v3",       [HISI_CLOUD_V4] = "cloud_v4",
        [HISI_CLOUD_V5] = "cloud_v5", [HISI_MINI_V4] = "mini_v4",       [HISI_CLOUD_V6] = "cloud_v6",
        [HISI_CLOUD_V7] = "cloud_v7", [HISI_MINI_V5_RC] = "mini_v5_rc", [HISI_MINI_V5] = "mini_v5",
    };

    if (type < HISI_CHIP_NUM && chip_name[type] == NULL) {
        return "unknown";
    }

    return (type < HISI_CHIP_NUM) ? chip_name[type] : "unknown";
}