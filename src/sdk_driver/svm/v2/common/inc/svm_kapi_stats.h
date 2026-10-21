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

#ifndef SVM_KAPI_STATS_H
#define SVM_KAPI_STATS_H

#include "ka_type.h"

#include "ka_common_pub.h"
#include "ka_base_pub.h"

enum devmm_kapi_stats_type {
    DEVMM_KAPI_STATS_TYPE_0 = 0,
    DEVMM_KAPI_STATS_TYPE_MAX
};

struct devmm_kapi_stats {
    ka_atomic64_t cur_cnt[DEVMM_KAPI_STATS_TYPE_MAX];
    ka_atomic64_t get_cnt[DEVMM_KAPI_STATS_TYPE_MAX];
    ka_atomic64_t put_cnt[DEVMM_KAPI_STATS_TYPE_MAX];
};

void devmm_kapi_stats_inc(enum devmm_kapi_stats_type type);
void devmm_kapi_stats_dec(enum devmm_kapi_stats_type type);
int devmm_kapi_stats_procfs_show(ka_seq_file_t *seq, void *offset);

#endif