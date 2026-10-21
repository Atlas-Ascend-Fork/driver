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
#include "devmm_common.h"
#include "devmm_proc_mem_copy.h"
#include "svm_proc_mng.h"
#include "svm_pa_trans.h"
#ifndef EMU_ST
static int svm_pa_seg_v2p_batch(u32 dst_udevid, u64 pa_array[], u32 pa_num, struct svm_pa_seg pm_pa_seg[],
                                u32 *pm_seg_num)
{
    u32 i;
    int ret;

    ret = devmm_vm_pa_to_pm_pa(dst_udevid, pa_array, pa_num, pa_array, pa_num);
    if (ret != 0) {
        return ret;
    }

    for (i = 0; i < pa_num; i++) {
        pm_pa_seg[*pm_seg_num].pa = pa_array[i];
        pm_pa_seg[*pm_seg_num].size = KA_MM_PAGE_SIZE;
        (*pm_seg_num)++;
    }
    return 0;
}

int svm_pa_seg_v2p(u32 dst_udevid, const struct svm_pa_seg vm_pa_seg[], u32 vm_seg_num, struct svm_pa_seg pm_pa_seg[],
                   u32 *pm_seg_num)
{
    u64 pa_array[DEVDRV_AGENT_SMMU_SUPPORT_MAX_NUM];
    u32 pm_seg_capacity = *pm_seg_num;
    u32 pa_num = 0;
    u32 output_num = 0;
    u32 stamp = (u32)ka_jiffies;
    u32 i;

    for (i = 0; i < vm_seg_num; i++) {
        u64 offset;

        for (offset = 0; offset < vm_pa_seg[i].size; offset += KA_MM_PAGE_SIZE) {
            int ret;

            if (output_num + pa_num >= pm_seg_capacity) {
                devmm_drv_err("Pm pa segment buffer is not enough. (capacity=%u)\n", pm_seg_capacity);
                return -ENOSPC;
            }
            pa_array[pa_num++] = vm_pa_seg[i].pa + offset;
            if (pa_num != DEVDRV_AGENT_SMMU_SUPPORT_MAX_NUM) {
                continue;
            }

            ret = svm_pa_seg_v2p_batch(dst_udevid, pa_array, pa_num, pm_pa_seg, &output_num);
            if (ret != 0) {
                return ret;
            }
            pa_num = 0;
            devmm_try_cond_resched(&stamp);
        }
    }

    if (pa_num != 0) {
        int ret = svm_pa_seg_v2p_batch(dst_udevid, pa_array, pa_num, pm_pa_seg, &output_num);
        if (ret != 0) {
            return ret;
        }
    }
    *pm_seg_num = output_num;
    return 0;
}
#else
void svm_pa_trans_master(void)
{}
#endif