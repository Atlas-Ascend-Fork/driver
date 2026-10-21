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

#include "ka_base_pub.h"
#include "ka_fs_pub.h"

#include "pbl_feature_loader.h"
#include "svm_kern_log.h"
#include "svm_kapi_stats.h"
#include "svm_proc_fs.h"

static struct svm_kapi_stats g_kapi_stats;

void svm_kapi_stats_inc(enum svm_kapi_stats_type type)
{
    if ((u32)type >= SVM_KAPI_STATS_TYPE_MAX) {
        return;
    }

    ka_base_atomic64_inc(&g_kapi_stats.cur_cnt[type]);
    ka_base_atomic64_inc(&g_kapi_stats.get_cnt[type]);
}

void svm_kapi_stats_dec(enum svm_kapi_stats_type type)
{
    if ((u32)type >= SVM_KAPI_STATS_TYPE_MAX) {
        return;
    }

    ka_base_atomic64_dec(&g_kapi_stats.cur_cnt[type]);
    ka_base_atomic64_inc(&g_kapi_stats.put_cnt[type]);
}

static const char *g_svm_kapi_names[SVM_KAPI_STATS_TYPE_MAX] = {
    [SVM_KAPI_STATS_TYPE_0] = "hal_kernel_svm_get_user_pages/hal_kernel_svm_put_user_pages",
};

int svm_kapi_stats_procfs_show(ka_seq_file_t *seq, void *offset)
{
    enum svm_kapi_stats_type type;
    s64 cur_cnt;
    u64 get_cnt;
    u64 put_cnt;

    (void)offset;
    for (type = SVM_KAPI_STATS_TYPE_0; type < SVM_KAPI_STATS_TYPE_MAX; type++) {
        cur_cnt = (s64)ka_base_atomic64_read(&g_kapi_stats.cur_cnt[type]);
        get_cnt = (u64)ka_base_atomic64_read(&g_kapi_stats.get_cnt[type]);
        put_cnt = (u64)ka_base_atomic64_read(&g_kapi_stats.put_cnt[type]);
        if ((cur_cnt == 0) && (get_cnt == 0) && (put_cnt == 0)) {
            continue;
        }
        ka_fs_seq_printf(seq, "Svm kernel api call stats: api=%s; cur_cnt=%lld; get_cnt=%llu; put_cnt=%llu\n",
                         g_svm_kapi_names[type], cur_cnt, get_cnt, put_cnt);
    }
    return 0;
}

static int svm_kapi_stats_open(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_single_open(file, svm_kapi_stats_procfs_show, ka_base_pde_data(inode));
}

static const ka_procfs_ops_t svm_kapi_stats_ops = {
    ka_fs_init_pf_owner(KA_THIS_MODULE) ka_fs_init_pf_open(svm_kapi_stats_open) ka_fs_init_pf_read(ka_fs_seq_read)
        ka_fs_init_pf_lseek(ka_fs_seq_lseek) ka_fs_init_pf_release(ka_fs_single_release)};

int svm_kapi_stats_procfs_init(void)
{
    svm_proc_fs_add_feature("kernel_api_stats", &svm_kapi_stats_ops, NULL);
    return 0;
}
DECLAER_FEATURE_AUTO_INIT(svm_kapi_stats_procfs_init, FEATURE_LOADER_STAGE_0);

void svm_kapi_stats_procfs_uninit(void)
{}
DECLAER_FEATURE_AUTO_UNINIT(svm_kapi_stats_procfs_uninit, FEATURE_LOADER_STAGE_0);
