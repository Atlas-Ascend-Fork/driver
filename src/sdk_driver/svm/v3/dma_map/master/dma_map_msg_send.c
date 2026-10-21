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
#include "ka_memory_pub.h"

#include "svm_kern_log.h"
#include "svm_slab.h"
#include "kmc_h2d.h"
#include "dma_map_msg.h"
#include "dma_map_msg_send.h"

static int svm_dma_map_addr_query_remote_once(u32 udevid, const struct svm_global_va *src_info,
                                              struct svm_pa_seg *pa_seg, u64 max_seg_num, u64 *seg_num)
{
    u64 extend_num = ka_base_min_t(
        u64, max_seg_num,
        (svm_kmc_h2d_get_max_msg_len() - sizeof(struct svm_dma_map_addr_query_msg)) / sizeof(struct svm_dma_addr_seg));
    u32 msg_len = (u32)(sizeof(struct svm_dma_map_addr_query_msg) + extend_num * sizeof(struct svm_dma_addr_seg));
    struct svm_dma_map_addr_query_msg *msg = NULL;
    u32 reply_len = msg_len;
    int ret;
    u32 i;

    msg = svm_kvzalloc(msg_len, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (msg == NULL) {
        return -ENOMEM;
    }

    msg->head.msg_id = SVM_KMC_MSG_DMA_ADDR_QUERY;
    msg->head.extend_num = extend_num;
    msg->src_info = *src_info;

    ret = svm_kmc_h2d_send(udevid, msg, (u32)sizeof(*msg), msg_len, &reply_len);
    if (ret == 0) {
        u32 actual_len = (u32)(sizeof(*msg) + msg->head.extend_num * sizeof(struct svm_dma_addr_seg));

        if ((msg->head.extend_num == 0) || (msg->head.extend_num > extend_num) || (reply_len < actual_len)) {
            ret = -EINVAL;
        } else {
            for (i = 0; i < msg->head.extend_num; i++) {
                pa_seg[i].pa = msg->seg[i].dma_addr;
                pa_seg[i].size = msg->seg[i].size;
            }
            *seg_num = msg->head.extend_num;
        }
    }
    svm_kvfree(msg);
    return ret;
}

int svm_dma_map_addr_query_remote(u32 udevid, const struct svm_global_va *src_info, struct svm_pa_seg *pa_seg,
                                  u64 max_seg_num, u64 *seg_num)
{
    struct svm_global_va query_info = *src_info;
    u64 remain_size = src_info->size;
    u64 total_seg_num = 0;
    int ret;

    while (remain_size != 0) {
        u64 batch_size = 0;
        u64 batch_num = 0;
        u64 i;
        query_info.size = remain_size;
        ret = svm_dma_map_addr_query_remote_once(udevid, &query_info, &pa_seg[total_seg_num],
                                                 max_seg_num - total_seg_num, &batch_num);
        if (ret != 0) {
            return ret;
        }

        for (i = 0; i < batch_num; i++) {
            batch_size += pa_seg[total_seg_num + i].size;
        }
        if ((batch_size == 0) || (batch_size > remain_size) || ((total_seg_num + batch_num) > max_seg_num)) {
            return -EINVAL;
        }

        total_seg_num += batch_num;
        query_info.va += batch_size;
        remain_size -= batch_size;
    }

    *seg_num = total_seg_num;
    return 0;
}
