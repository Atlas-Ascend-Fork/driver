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
#include "ka_list_pub.h"

#include "dpa_kernel_interface.h"
#include "pbl_uda.h"

#include "framework_cmd.h"
#include "framework_task.h"
#include "k2umc.h"
#include "ksvmm.h"
#include "smm_flag.h"
#include "smm_uk_msg.h"
#include "svm_dev_topology.h"
#include "svm_kern_log.h"
#include "svm_slab.h"
#include "svm_sub_event_type_uk_msg.h"
#include "share_repair.h"

struct share_repair_dst_info {
    ka_list_head_t list;
    u32 udevid;
    int tgid; /* Master tgid of the destination share task. */
    u64 dst_va;
    u64 dst_size;
    struct svm_global_va src_info;
};

struct share_repair_collect_priv {
    struct svm_global_va src_va;
    u32 dst_udevid;
    int dst_tgid; /* Master tgid of the destination share task. */
    ka_list_head_t *dst_head;
    int ret;
};

static bool share_repair_src_match(struct svm_global_va *target, struct svm_global_va *cur)
{
    if ((target->server_id != cur->server_id) || (target->udevid != cur->udevid) || (target->tgid != cur->tgid)) {
        return false;
    }

    if ((target->va < cur->va) || ((target->va - cur->va) >= cur->size)) {
        return false;
    }

    return true;
}

static void share_repair_free_dst_list(ka_list_head_t *head)
{
    struct share_repair_dst_info *dst_info = NULL;
    struct share_repair_dst_info *next = NULL;

    ka_list_for_each_entry_safe(dst_info, next, head, list)
    {
        ka_list_del(&dst_info->list);
        svm_kfree(dst_info);
    }
}

static int share_repair_try_append_dst_info(u64 start, struct svm_global_va *src_info,
                                            struct share_repair_collect_priv *priv)
{
    struct share_repair_dst_info *dst_info = NULL;

    if (!share_repair_src_match(&priv->src_va, src_info)) {
        return 0;
    }

    if (svm_dev_is_ub_connect(priv->dst_udevid, src_info->server_id, src_info->udevid)) {
        return 0;
    }

    dst_info = (struct share_repair_dst_info *)svm_kzalloc(sizeof(*dst_info), KA_GFP_ATOMIC | __KA_GFP_ACCOUNT);
    if (dst_info == NULL) {
        return -ENOMEM;
    }

    dst_info->udevid = priv->dst_udevid;
    dst_info->tgid = priv->dst_tgid;
    dst_info->dst_va = start;
    dst_info->dst_size = src_info->size;
    dst_info->src_info = *src_info;
    ka_list_add_tail(&dst_info->list, priv->dst_head);
    svm_debug("Collect share repair dst. (src_udevid=%u; src_tgid=%d; src_va=0x%llx; dst_udevid=%u; "
              "dst_tgid=%d; dst_va=0x%llx; size=0x%llx)\n",
              src_info->udevid, src_info->tgid, src_info->va, priv->dst_udevid, priv->dst_tgid, start, src_info->size);
    return 0;
}

static int share_repair_collect_seg(u64 start, struct svm_global_va *src_info, void *data)
{
    struct share_repair_collect_priv *priv = (struct share_repair_collect_priv *)data;

    return share_repair_try_append_dst_info(start, src_info, priv);
}

static void share_repair_collect_seg_by_task(void *task_ctx, void *data)
{
    struct share_repair_collect_priv *priv = (struct share_repair_collect_priv *)data;

    if (priv->ret != 0) {
        return;
    }

    priv->dst_tgid = svm_get_task_tgid(task_ctx);
    priv->ret = ksvmm_for_each_seg(priv->dst_udevid, priv->dst_tgid, share_repair_collect_seg, priv);
}

static int share_repair_collect_dst_list(struct svm_global_va *src_va, ka_list_head_t *dst_head)
{
    struct share_repair_collect_priv priv = {
        .src_va = *src_va,
        .dst_head = dst_head,
        .ret = 0,
    };
    u32 dst_udevid;
    u32 host_udevid = uda_get_host_id();
    u32 udev_max_num = uda_get_udev_max_num();

    for (dst_udevid = 0; dst_udevid < udev_max_num; dst_udevid++) {
        if (dst_udevid == host_udevid) {
            continue;
        }

        priv.dst_udevid = dst_udevid;
        svm_task_ctx_for_each(dst_udevid, &priv, share_repair_collect_seg_by_task);
        if (priv.ret != 0) {
            break;
        }
    }

    return priv.ret;
}

static int share_repair_pack_dst_desc(struct share_repair_dst_info *dst_info, struct svm_k2umc_desc *dst_desc)
{
    int ret;

    ret = hal_kernel_apm_query_slave_tgid_by_master(dst_info->tgid, dst_info->udevid, PROCESS_CP1,
                                                    &dst_desc->request_pid);
    if (ret != 0) {
        svm_err("Query target slave tgid failed in share repair. (ret=%d; udevid=%u; master_tgid=%d)\n", ret,
                dst_info->udevid, dst_info->tgid);
        return ret;
    }

    dst_desc->response_pid = dst_info->tgid;
    dst_desc->request_udevid = dst_info->udevid;
    return 0;
}

static int share_repair_unmap(struct share_repair_dst_info *dst_info, struct svm_k2umc_desc *dst_desc)
{
    struct svm_smm_unmap_msg unmap_msg = {{0}};

    unmap_msg.dst_va = dst_info->dst_va;
    unmap_msg.dst_size = dst_info->dst_size;
    unmap_msg.dst_task_type = PROCESS_CP1;
    unmap_msg.flag = SVM_SMM_FLAG_PG_NC;
    unmap_msg.src_info = dst_info->src_info;

    dst_desc->subevent_id = SVM_SMM_MUNMAP_EVENT;
    return svm_k2umc_send(dst_desc, K2UMC_FLAG_TO_DEVICE | K2UMC_FLAG_SYNC, (char *)&unmap_msg, sizeof(unmap_msg));
}

static int share_repair_map(struct share_repair_dst_info *dst_info, struct svm_k2umc_desc *dst_desc)
{
    struct svm_smm_map_msg map_msg = {{0}};

    map_msg.dst_va = dst_info->dst_va;
    map_msg.dst_size = dst_info->dst_size;
    map_msg.dst_task_type = PROCESS_CP1;
    map_msg.flag = SVM_SMM_FLAG_PG_NC;
    map_msg.src_info = dst_info->src_info;
    map_msg.src_va = dst_info->src_info.va;

    dst_desc->subevent_id = SVM_SMM_MMAP_EVENT;
    return svm_k2umc_send(dst_desc, K2UMC_FLAG_TO_DEVICE | K2UMC_FLAG_SYNC, (char *)&map_msg, sizeof(map_msg));
}

static int _share_repair_dst(struct share_repair_dst_info *dst_info)
{
    struct svm_k2umc_desc dst_desc = {0};
    int ret = 0;

    ret = share_repair_pack_dst_desc(dst_info, &dst_desc);
    if (ret != 0) {
        return ret;
    }

    ret = share_repair_unmap(dst_info, &dst_desc);
    if (ret != 0) {
        svm_err("Share repair unmap dst failed. (ret=%d; udevid=%u; tgid=%d; dst_va=0x%llx; size=0x%llx)\n", ret,
                dst_info->udevid, dst_info->tgid, dst_info->dst_va, dst_info->dst_size);
        return ret;
    }

    ret = share_repair_map(dst_info, &dst_desc);
    if (ret != 0) {
        svm_err("Share repair map dst failed. (ret=%d; udevid=%u; tgid=%d; dst_va=0x%llx; size=0x%llx)\n", ret,
                dst_info->udevid, dst_info->tgid, dst_info->dst_va, dst_info->dst_size);
        return ret;
    }

    svm_debug("Share repair dst success. (udevid=%u; tgid=%d; dst_va=0x%llx; size=0x%llx)\n", dst_info->udevid,
              dst_info->tgid, dst_info->dst_va, dst_info->dst_size);
    return 0;
}

static int share_repair_dst(ka_list_head_t *dst_head)
{
    struct share_repair_dst_info *dst_info = NULL;
    int ret;

    ka_list_for_each_entry(dst_info, dst_head, list)
    {
        ret = _share_repair_dst(dst_info);
        if (ret != 0) {
            return ret;
        }
    }

    return 0;
}

int svm_repair_share_dst(struct svm_global_va *src_va)
{
    ka_list_head_t dst_head = KA_LIST_HEAD_INIT(dst_head);
    int ret;

    ret = share_repair_collect_dst_list(src_va, &dst_head);
    if (ret == 0) {
        ret = share_repair_dst(&dst_head);
    }
    share_repair_free_dst_list(&dst_head);
    return ret;
}
