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
#include "ka_memory_pub.h"

#include "securec.h"

#include "pbl_feature_loader.h"
#include "pbl_uda.h"

#include "svm_kern_log.h"
#include "svm_slab.h"
#include "svm_gfp.h"
#include "kmc_msg.h"
#include "kmc_h2d.h"
#include "pmq.h"
#include "pmq_msg.h"
#include "pmq_client.h"

static int pmq_client_query(u32 udevid, struct svm_global_va *src_info, struct svm_pa_seg pa_seg[], u64 *seg_num,
                            u32 msg_id)
{
    struct pmq_query_pa_msg *msg = NULL;
    u64 start = src_info->va;
    u64 end = src_info->va + src_info->size;
    u64 max_extend_num = ((udevid != uda_get_host_id()) && (src_info->udevid != uda_get_host_id())) ?
                             SVM_KMC_P2P_MAX_EXTEND_NUM :
                             ((u64)pmq_get_max_msg_len() - sizeof(struct pmq_query_pa_msg)) / sizeof(struct svm_pa_seg);
    u64 extend_num = ka_base_min_t(u64, *seg_num, max_extend_num);
    u32 in_len, real_out_len, msg_len;
    u64 i = 0, total_size = 0;
    u64 va, size;
    int ret;

    in_len = sizeof(struct pmq_query_pa_msg);
    msg_len = sizeof(struct pmq_query_pa_msg) + extend_num * sizeof(struct svm_pa_seg);
    msg = svm_kvzalloc(msg_len, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (msg == NULL) {
        svm_err("Kvzalloc failed. (extend_num=%u)\n", msg_len);
        return -ENOMEM;
    }

    for (va = start; va < end; va += size) {
        msg->head.msg_id = msg_id;
        msg->head.extend_num = extend_num;
        msg->tgid = src_info->tgid;
        msg->va = va;
        msg->size = end - va;

        real_out_len = msg_len;
        ret = pmq_msg_send(udevid, src_info->udevid, (void *)msg, in_len, &real_out_len);
        if ((ret != 0) || (msg->head.extend_num > extend_num) || (msg->head.extend_num == 0ULL) ||
            ((msg->head.extend_num + i) > *seg_num)) {
            svm_err("Kmc send query pa failed. (ret=%d; out_num=%llu; in_num=%llu; seg_num=%llu; i=%llu)\n", ret,
                    msg->head.extend_num, extend_num, *seg_num, i);
            svm_kvfree(msg);
            return (ret != 0) ? ret : -EINVAL;
        }

        (void)memcpy_s((void *)&pa_seg[i], (*seg_num - i) * sizeof(struct svm_pa_seg), msg->seg,
                       msg->head.extend_num * sizeof(struct svm_pa_seg));
        i += msg->head.extend_num;
        size = svm_get_pa_size(msg->seg, msg->head.extend_num);
        if ((size == 0) || ((va + size) < start)) {
            svm_err("Pa size is invalid. (extend_num=%llu; size=%llu; va=0x%llx)\n", msg->head.extend_num, size, va);
            svm_kvfree(msg);
            return -EINVAL;
        }
        total_size += size;
    }
    if (total_size != src_info->size) {
        svm_err("Total size is invalid. (total_size=%llu; requested_size=%llu)\n", total_size, src_info->size);
        svm_kvfree(msg);
        return -EINVAL;
    }

    *seg_num = i;

    svm_kvfree(msg);
    return 0;
}

int svm_pmq_client_pa_query(u32 local_udevid, struct svm_global_va *src_info, struct svm_pa_seg pa_seg[], u64 *seg_num)
{
    return pmq_client_query(local_udevid, src_info, pa_seg, seg_num, SVM_KMC_MSG_QUERY_PA);
}

int svm_pmq_client_host_bar_query(u32 local_udevid, struct svm_global_va *src_info, struct svm_pa_seg pa_seg[],
                                  u64 *seg_num)
{
    return pmq_client_query(local_udevid, src_info, pa_seg, seg_num, SVM_KMC_MSG_QUERY_HOST_BAR);
}
