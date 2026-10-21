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

#include "pbl_uda.h"
#include "pbl_soc_res.h"

#include "ubmm_log.h"
#include "ubmm_map.h"
#include "ubmm_uba.h"

static int ubmm_query_uba_info(u32 udevid, u64 *uba_base, u64 *uba_size)
{
    struct soc_reg_base_info io_base;
    u32 udevid_ex = 0;
    int ret;

    (void)uda_get_cur_ns_udevids(&udevid_ex, 1);
    udevid_ex = (udevid == uda_get_host_id()) ? udevid_ex : udevid; /* Not host_id real dev, change to first udevid. */
    ret = soc_resmng_dev_get_reg_base(udevid_ex, "UBA_BASE", &io_base);
    if (ret != 0) {
        return -EAGAIN;
    }

    *uba_base = (u64)io_base.io_base;
    *uba_size = (u64)io_base.io_base_size;

    return 0;
}

static int ubmm_init_uba(u32 udevid)
{
    u64 uba_base, uba_size;
    int ret;

    ret = ubmm_query_uba_info(udevid, &uba_base, &uba_size);
    if (ret != 0) {
        return ret;
    }

    return ubmm_uba_pool_create(uba_base, uba_size);
}

int ubmm_init_dev_res(u32 udevid)
{
    int ret;

    ret = ubmm_init_map(udevid);
    if (ret != 0) {
        return ret;
    }

    ret = ubmm_init_uba(udevid);
    if (ret != 0) {
        ubmm_uninit_map(udevid);
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(ubmm_init_dev_res);

static void ubmm_uninit_uba(void)
{
    ubmm_uba_pool_destroy();
}

void ubmm_uninit_dev_res(u32 udevid)
{
    ubmm_uninit_uba();
    ubmm_uninit_map(udevid);
}
KA_EXPORT_SYMBOL_GPL(ubmm_uninit_dev_res);
