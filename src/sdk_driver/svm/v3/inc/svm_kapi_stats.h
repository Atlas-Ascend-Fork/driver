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

#ifndef SVM_KAPI_STATS_H
#define SVM_KAPI_STATS_H

#include "ka_common_pub.h"

enum svm_kapi_stats_type {
    SVM_KAPI_STATS_TYPE_0 = 0,
    SVM_KAPI_STATS_TYPE_MAX
};

struct svm_kapi_stats {
    ka_atomic64_t cur_cnt[SVM_KAPI_STATS_TYPE_MAX];
    ka_atomic64_t get_cnt[SVM_KAPI_STATS_TYPE_MAX];
    ka_atomic64_t put_cnt[SVM_KAPI_STATS_TYPE_MAX];
};

void svm_kapi_stats_inc(enum svm_kapi_stats_type type);
void svm_kapi_stats_dec(enum svm_kapi_stats_type type);
int svm_kapi_stats_procfs_show(ka_seq_file_t *seq, void *offset);
int svm_kapi_stats_procfs_init(void);
void svm_kapi_stats_procfs_uninit(void);

#endif
