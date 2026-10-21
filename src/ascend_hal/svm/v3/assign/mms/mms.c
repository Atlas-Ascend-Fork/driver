/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <securec.h>

#include "ascend_hal.h"

#include "svm_log.h"
#include "svm_sys_cmd.h"
#include "svm_atomic.h"
#include "svm_ioctl_ex.h"
#include "svm_user_adapt.h"
#include "mms_def.h"
#include "mms.h"

struct mms_stats *g_mms_stats[SVM_MAX_DEV_NUM] = {NULL};

struct mms_stats *mms_stats_create(u32 devid)
{
    struct mms_stats *mms_stats = NULL;
    int ret;

    mms_stats = svm_user_mmap(NULL, sizeof(struct mms_stats), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1,
                              0);
    if (mms_stats == MAP_FAILED) {
        svm_err("mms_stats map failed.\n");
        return NULL;
    }

    ret = svm_cmd_ioctl(devid, SVM_MMS_STATS_MEM_CFG, (void *)mms_stats);
    if (ret != 0) {
        svm_err("Svm mms_init ioctl failed. (ret=%d; devid=%u)\n", ret, devid);
        svm_user_munmap(mms_stats, sizeof(struct mms_stats));
        return NULL;
    }
    return mms_stats;
}

int mms_init(u32 devid)
{
    if (g_mms_stats[devid] == NULL) {
        g_mms_stats[devid] = mms_stats_create(devid);
        if (g_mms_stats[devid] == NULL) {
            return DRV_ERROR_OUT_OF_MEMORY;
        }
        (void)memset_s(g_mms_stats[devid], sizeof(struct mms_stats), 0, sizeof(struct mms_stats));
    }
    return 0;
}

static struct mms_type_stats *mms_get_stats(u32 devid, u32 module_id, u32 type)
{
    u32 real_module_id = (module_id >= MMS_MODULE_ID_MAX) ? UNKNOWN_MODULE_ID : module_id;

    return ((devid >= SVM_MAX_DEV_NUM) || (g_mms_stats[devid] == NULL) || (type >= MMS_TYPE_MAX)) ?
               NULL :
               &g_mms_stats[devid]->module_stats[real_module_id].type_stats[type];
}

void svm_mms_add(u32 devid, u32 module_id, u32 type, u64 size)
{
    struct mms_type_stats *stats = mms_get_stats(devid, module_id, type);
    u64 add_size;

    if (stats != NULL) {
        add_size = svm_atomic64_add(&stats->alloced_size, size);
        while (1) {
            u64 old_peak_size = stats->alloced_peak_size;

            if (add_size <= old_peak_size) {
                break;
            }

            if (svm_atomic64_compare_and_swap(&stats->alloced_peak_size, old_peak_size, add_size)) {
                break;
            }
        }

        svm_atomic64_inc(&stats->alloc_cnt);
    }
}

void svm_mms_sub(u32 devid, u32 module_id, u32 type, u64 size)
{
    struct mms_type_stats *stats = mms_get_stats(devid, module_id, type);
    if (stats != NULL) {
        svm_atomic64_sub(&stats->alloced_size, size);
        svm_atomic64_inc(&stats->free_cnt);
    }
}

int svm_mms_get(u32 devid, u32 module_id, u32 type, struct mms_type_stats *stats)
{
    struct mms_type_stats *mms_stats = mms_get_stats(devid, module_id, type);
    if (mms_stats == NULL) {
        return DRV_ERROR_INNER_ERR;
    }

    (void)memcpy_s(stats, sizeof(struct mms_type_stats), mms_stats, sizeof(struct mms_type_stats));
    return 0;
}

__attribute__((constructor)) void svm_mms_init(void)
{
    svm_register_ioctl_dev_init_post_handle(mms_init);
}

__attribute__((destructor)) void svm_mms_uninit(void)
{
    uint32_t i;

    for (i = 0; i < SVM_MAX_DEV_NUM; ++i) {
        if (g_mms_stats[i] != NULL) {
            svm_user_munmap(g_mms_stats[i], sizeof(struct mms_stats));
            g_mms_stats[i] = NULL;
        }
    }
}
