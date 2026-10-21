/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
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

#include "trs_pub_def.h"
#include "trs_sia_adapt_auto_init.h"
#include "trsbase_feature.h"

static int trs_host_trsbase_feature_init(u32 ts_inst_id)
{
    struct trs_id_inst inst = {0};

    trs_ts_inst_to_id_inst(ts_inst_id, &inst);
    if (inst.tsid != 0) {
        return 0;
    }

    return trsbase_feature_init(inst.devid);
}
DECLAER_FEATURE_AUTO_INIT_DEV(trs_host_trsbase_feature_init, FEATURE_LOADER_STAGE_0);

static void trs_host_trsbase_feature_uninit(u32 ts_inst_id)
{
    struct trs_id_inst inst = {0};

    trs_ts_inst_to_id_inst(ts_inst_id, &inst);
    if (inst.tsid != 0) {
        return;
    }

    trsbase_feature_uninit(inst.devid);
}
DECLAER_FEATURE_AUTO_UNINIT_DEV(trs_host_trsbase_feature_uninit, FEATURE_LOADER_STAGE_0);
