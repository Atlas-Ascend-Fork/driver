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
#include "ka_dfx_pub.h"

#include "pbl_feature_loader.h"
#include "pbl_chip_config.h"

#include "svm_gfp.h"

static int _svm_get_nids(u32 udevid, u32 memtype, u32 sub_memtype, int nids[], int *out_num)
{
    int num;

    num = dbl_nid_get_nid_num(udevid, memtype, sub_memtype);
    if (num <= 0) {
        return (num == 0) ? -ENOMEM : num;
    }

    num = dbl_nid_get_nid(udevid, memtype, sub_memtype, nids, num);
    if (num <= 0) {
        return -EINVAL;
    }

    *out_num = num;
    return 0;
}

int svm_numa_node_init(void)
{
    svm_register_numa_id_handle(_svm_get_nids);
    return 0;
}
DECLAER_FEATURE_AUTO_INIT(svm_numa_node_init, FEATURE_LOADER_STAGE_0);
