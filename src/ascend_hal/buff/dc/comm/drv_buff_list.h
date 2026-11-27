/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef DRV_BUFF_LIST_H
#define DRV_BUFF_LIST_H

#include <stdint.h>

#include "drv_user_common.h"
#include "drv_buff_common.h"

#define BUFF_LIST_MUTEX_FLAG_UNLOCK 0
#define BUFF_LIST_MUTEX_FLAG_LOCK 1

/* BUFF_MAX_MZ_NODE_NUM = BUFF_MAX_CFG_NUM + 8, max config num for total config = user config + default config
 * BUFF_MAX_CFG_NUM: 64, max config num for user config in normal memtype
 * 8: max config num for default config in special memtype(dvpp mem). */
#define BUFF_MAX_MZ_NODE_NUM (BUFF_MAX_CFG_NUM + 8)

struct buff_memzone_list_node {
    struct list_head list;
    pthread_rwlock_t mz_list_mutex;
    uint32_t cfg_id;
    uint32_t mz_cnt; /* for create++, delete-- */
    uint64_t mz_total_size;
    uint64_t mz_avaliable_size;
    uint32_t mz_using_cnt; /* for malloc++, free-- */
    uint32_t elastic_enable;
    uint32_t elastic_low_level;
    struct memzone_user_mng_t *latest_mz;
};

struct buff_grp_list_node {
    int dev_id;
    int grp_id;
    struct list_head list;
    struct buff_memzone_list_node mz_list_node[BUFF_MAX_MZ_NODE_NUM];
    uint32_t list_node_cnt[BUFF_MAX_MZ_NODE_NUM];
    uint64_t list_total_size[BUFF_MAX_MZ_NODE_NUM];
    uint32_t total_num;
    uint64_t total_size;
};

void buff_list_lock(uint32_t mutex_flag);
void buff_list_unlock(uint32_t mutex_flag);
struct list_head *buff_get_grp_mng_list(void);
struct buff_grp_list_node *buff_create_grp_list_node(int devid, int grp_id);
struct buff_grp_list_node *buff_get_grp_list_node(int grp);
void buff_del_grp_list_node(struct buff_grp_list_node *node);
void buff_grp_list_clear(void);
struct buff_memzone_list_node *buff_get_buff_mng_list(int devid, int grp, uint32_t list_idx);
int buff_add_buff_mng_list_node(int grp, uint32_t list_idx, struct list_head *new_node);
int buff_del_buff_mng_list_node(struct list_head *node, uint32 mutex_flag);
int buff_get_devid_by_mz_list(struct buff_memzone_list_node *mz_list_node);

#endif /* _DRV_BUFF_LIST_H_ */
