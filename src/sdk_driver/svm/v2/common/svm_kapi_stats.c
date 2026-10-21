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

#include "ka_type.h"
#include "ka_fs_pub.h"

#include "svm_log.h"
#include "svm_define.h"
#include "devmm_adapt.h"
#include "devmm_proc_info.h"
#include "svm_kapi_stats.h"

static struct devmm_kapi_stats g_kapi_stats;

void devmm_kapi_stats_inc(enum devmm_kapi_stats_type type)
{
    if ((u32)type >= DEVMM_KAPI_STATS_TYPE_MAX) {
        return;
    }

    ka_base_atomic64_inc(&g_kapi_stats.cur_cnt[type]);
    ka_base_atomic64_inc(&g_kapi_stats.get_cnt[type]);
}

void devmm_kapi_stats_dec(enum devmm_kapi_stats_type type)
{
    if ((u32)type >= DEVMM_KAPI_STATS_TYPE_MAX) {
        return;
    }

    ka_base_atomic64_dec(&g_kapi_stats.cur_cnt[type]);
    ka_base_atomic64_inc(&g_kapi_stats.put_cnt[type]);
}

#ifdef CONFIG_PROC_FS
static const char *g_kapi_names[DEVMM_KAPI_STATS_TYPE_MAX] = {
    [DEVMM_KAPI_STATS_TYPE_0] = "devmm_get_mem_pa_list/devmm_put_mem_pa_list",
};

int devmm_kapi_stats_procfs_show(ka_seq_file_t *seq, void *offset)
{
    enum devmm_kapi_stats_type type;
    s64 cur_cnt;
    u64 get_cnt;
    u64 put_cnt;

    (void)offset;
    for (type = DEVMM_KAPI_STATS_TYPE_0; type < DEVMM_KAPI_STATS_TYPE_MAX; type++) {
        cur_cnt = (s64)ka_base_atomic64_read(&g_kapi_stats.cur_cnt[type]);
        get_cnt = (u64)ka_base_atomic64_read(&g_kapi_stats.get_cnt[type]);
        put_cnt = (u64)ka_base_atomic64_read(&g_kapi_stats.put_cnt[type]);
        if ((cur_cnt == 0) && (get_cnt == 0) && (put_cnt == 0)) {
            continue;
        }
        ka_fs_seq_printf(seq,
                         "Svm kernel api call stats: api=%s; cur_cnt=%lld; "
                         "get_cnt=%llu; put_cnt=%llu\n",
                         g_kapi_names[type], cur_cnt, get_cnt, put_cnt);
    }
    return 0;
}
#endif