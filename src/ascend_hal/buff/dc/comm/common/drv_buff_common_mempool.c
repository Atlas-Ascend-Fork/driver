/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef BUFF_MANAGE_KERNEL_UT
#include "drv_buff_common_mempool.h"
#include "drv_buff_unibuff.h"
#include "drv_buff_adp.h"
#include "drv_buff_mbuf.h"
#include "drv_usr_buff_mempool.h"
#include "bitmap.h"

drvError_t mp_free_buff(void *mempool, void *buff)
{
    struct mempool_t *mp = (struct mempool_t *)mempool;
    struct uni_buff_head_t *head = NULL;
    unsigned long offset;
    unsigned int buff_index;

    head = buff_mempool_get_head(buff);

    if (mp->stat.buff_time_stat_enable == 1) {
        unsigned int cur_timestamp = (unsigned int)buff_api_timestamp();
        unsigned int cost_timestamp = cur_timestamp - head->timestamp;
        if (cur_timestamp < head->timestamp) {
            cost_timestamp = (unsigned int)((unsigned long)cur_timestamp + BUFF_TIME_OVERTURN_OFFSET - head->timestamp);
        }

        if (mp->stat.over_time_value <= cost_timestamp) {
            buff_api_atomic_inc(&mp->stat.blk_over_time_num);
        }

        if (mp->stat.blk_max_use_time < cost_timestamp) {
            mp->stat.blk_max_use_time = cost_timestamp;
        }
    }

    buff_index = head->index;
    if (buff_index >= mp->blk_num) {
        buff_err("Buff free. (mp=%p; buff=%p; index=%u; bit_num=%u)\n", mp, buff, buff_index, mp->blk_num);
        return DRV_ERROR_INVALID_VALUE;
    }

    head->mbuf_data_flag = UNI_MBUF_DATA_DISABLE;
    head->status = UNI_STATUS_IDLE;
    head->buff_type = TYPE_NONE;

    offset = buff_index & (BITS_PER_LONG - 1);
    bitmap_clear_bit_atomic(&mp->bitmap[buff_index / BITS_PER_LONG], offset);
    mp_mng_put(mp);

    buff_api_atomic_inc(&mp->blk_available);

    return DRV_ERROR_NONE;
}

static void mp_free_release_buff(struct mempool_t *mp, void *buff, int *num_out)
{
    int num = *num_out;
    struct uni_buff_head_t *head = NULL;
    head = buff_mempool_get_head(buff);
    (void)pthread_mutex_lock(&mp->mutex);
    if (head->status == UNI_STATUS_RELEASE) {
        (void)mp_free_buff(mp, buff);
        num++;
    }
    (void)pthread_mutex_unlock(&mp->mutex);
    *num_out = num;
}

/* return: -1 finish, 0 no free, 1 free one */
static int mp_try_free_one_buff(struct mempool_t *mp, unsigned long total_size, unsigned long *offset)
{
    void *buff = NULL;
    int num = 0;

    *offset = bitmap_find_next_bit(mp->bitmap, total_size, *offset);
    if ((*offset >= total_size) || (*offset >= mp->blk_num)) {
        return -1;
    }

    buff = mp_get_buff_uva_by_index(mp, *offset);
    mp_free_release_buff(mp, buff, &num);

    return num;
}

int mp_scan_free_single_buff(struct mempool_t *mp)
{
    unsigned long i;
    int num = 0;

    for (i = mp->free_index; i < mp->bit_num; i++) {
        num = mp_try_free_one_buff(mp, mp->bit_num, &i);
        if (num != 0) {
            break;
        }
    }

    if (num > 0) {
        mp->free_index = (uint32)((i + 1) % mp->bit_num);
        return num;
    }

    for (i = 0; i < mp->free_index; i++) {
        num = mp_try_free_one_buff(mp, mp->free_index, &i);
        if (num != 0) {
            break;
        }
    }

    mp->free_index = (uint32)((i + 1) % mp->bit_num);
    return num;
}

void mp_scan_free_all_buff(struct mempool_t *mp)
{
    unsigned long i;

    for (i = 0; i < mp->bit_num; i++) {
        int ret;

        if (mp->scan_in_alloc == 1) {
            break;
        }

        ret = mp_try_free_one_buff(mp, mp->bit_num, &i);
        if (ret < 0) {
            break;
        }
    }
    return;
}

void *mp_get_valist_start_addr(struct mempool_t *mp, uint32 bit_num)
{
    return (void *)((uintptr_t)&mp->bitmap[bit_num / BITS_PER_LONG]); // lint !e507
}
#else
void drv_buff_common_mempool_ut(void)
{
    return;
}
#endif
