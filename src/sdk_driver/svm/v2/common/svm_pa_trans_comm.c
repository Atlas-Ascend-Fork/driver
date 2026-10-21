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
#include "comm/comm_pcie.h"

#include "devmm_common.h"
#include "svm_pa_trans.h"

int svm_pa_seg_d2h(u32 src_udevid, struct svm_pa_seg pa_seg[], u32 seg_num)
{
    u32 stamp = (u32)ka_jiffies;
    u32 i;
    int ret;

    for (i = 0; i < seg_num; i++) {
        ret = devdrv_devmem_addr_d2h(src_udevid, pa_seg[i].pa, &pa_seg[i].pa);
        if (ret != 0) {
#ifndef EMU_ST
            devmm_drv_err("Convert device to host pa failed. (devid=%u; index=%u; ret=%d)\n", src_udevid, i, ret);
            return ret;
#endif
        }
        devmm_try_cond_resched(&stamp);
    }
    return 0;
}
