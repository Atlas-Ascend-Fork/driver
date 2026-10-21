/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <pthread.h>

#include "ascend_hal.h"

#include "drv_user_common.h" /* user list */
#include "pbl_uda_user.h"

#include "svm_user_adapt.h"
#include "svm_log.h"
#include "svm_pagesize.h"
#include "svm_dbi.h"
#include "va_allocator.h"
#include "malloc_mng.h"
#include "svm_vmm.h"
#include "svmm.h"
#include "smm_client.h"
#include "svm_share_type.h"
#include "svm_share_align.h"
#include "svm_pipeline.h"
#include "svm_addr_desc.h"
#include "normal_malloc.h"
#include "svm_sys_cmd.h"
#include "svm_init_pri.h"
#include "svm_criu.h"
#include "svm_register.h"

struct svm_register_priv {
    u32 dst_devid;
    u64 src_va;
    u64 size;
    u64 dst_start_va;
    u64 npage_size;
    u64 src_start_va;
    u64 src_total_size;
    bool flag;
};

struct svm_register_node {
    struct svm_share_priv_head head;
    struct list_head node;
    void *svmm_inst[SVM_MAX_DEV_NUM];
    u64 va;
    bool is_non_svm; /* For user malloc va. */
};

static struct list_head register_head = LIST_HEAD_INIT(register_head);
static pthread_mutex_t register_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct svm_register_node *non_svm_register_node[SVM_MAX_DEV_NUM] = {NULL};
#define MAX_VA_SIZE (256 * SVM_BYTES_PER_TB)

struct svm_register_ops *register_ops = NULL;

void svm_register_set_ops(struct svm_register_ops *ops)
{
    register_ops = ops;
}

static int register_ops_post_map(struct svm_global_va *src_info, u32 devid, u64 dst_va, u32 register_to_peer_flag)
{
    if ((register_ops != NULL) && (register_ops->post_map != NULL)) {
        return register_ops->post_map(src_info, devid, dst_va, register_to_peer_flag);
    }
    return 0;
}

static void register_ops_pre_unmap(struct svm_global_va *src_info, u32 devid, u64 dst_va)
{
    if ((register_ops != NULL) && (register_ops->pre_unmap != NULL)) {
        register_ops->pre_unmap(src_info, devid, dst_va);
    }
}

static int _svm_register_for_each_seg(
    u32 dst_devid, int (*handle)(void *va_handle, u64 start, struct svm_global_va *src_info, void *priv), void *priv)
{
    struct list_head *node = NULL, *tmp = NULL;
    int ret;

    list_for_each_safe(node, tmp, &register_head)
    {
        struct svm_register_node *register_node = list_entry(node, struct svm_register_node, node);
        ret = svm_svmm_for_each_seg_handle(register_node->svmm_inst[dst_devid], handle, priv);
        if (ret != 0) {
            if (ret < 0) {
                svm_err("Svm register for each seg failed. (devid=%u)\n", dst_devid);
            }
            return ret;
        }
    }
    return 0;
}

int svm_register_for_each_seg(u32 dst_devid,
                              int (*handle)(void *va_handle, u64 start, struct svm_global_va *src_info, void *priv),
                              void *priv)
{
    int ret;

    if (dst_devid >= SVM_MAX_DEV_NUM) {
        svm_err("Devid is invalid. (devid=%u)\n", dst_devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    (void)pthread_mutex_lock(&register_mutex);
    ret = _svm_register_for_each_seg(dst_devid, handle, priv);
    (void)pthread_mutex_unlock(&register_mutex);
    return ret;
}

static int svm_register_to_peer_alloc_va(u32 devid, u64 src_va, u64 size, u64 *va)
{
    u32 normal_flag = SVM_NORMAL_MALLOC_FLAG_VA_ONLY | SVM_NORMAL_MALLOC_FLAG_VA_WITH_MASTER;
    u64 hpage_size, offset, aligned_size;
    u64 tmp_va = 0;
    int ret;

    ret = svm_dbi_query_hpage_size(devid, &hpage_size);
    if (ret != 0) {
        svm_err("Get hpage size failed. (devid-%u)\n", devid);
        return ret;
    }

    /* We keep the same offset between dst_va and src_va to build dst HPAGE. */
    offset = src_va - svm_align_down(src_va, hpage_size);
    aligned_size = svm_align_up(size + offset, hpage_size);
    ret = svm_normal_malloc(SVM_INVALID_DEVID, normal_flag, hpage_size, &tmp_va,
                            svm_get_non_dev_align_size(aligned_size));
    if (ret != 0) {
        svm_err("Alloc va failed. (devid=%u; size=%llu; offset=%llu)\n", devid, size, offset);
        return ret;
    }

    *va = tmp_va + offset;
    return 0;
}

/* Compute aligned va/size from src_info for SMM communication.
 * aligned_info: aligned src_info for SMM (NULL if not needed)
 * aligned_size: aligned size for va alloc/free (NULL if not needed) */
void svm_register_compute_aligned(const struct svm_global_va *src_info, u64 npage_size,
                                  struct svm_global_va *aligned_info, u64 *aligned_size)
{
    u64 aligned_va = svm_align_down(src_info->va, npage_size);
    u64 size = svm_align_up(src_info->size + (src_info->va - aligned_va), npage_size);

    if (aligned_info != NULL) {
        *aligned_info = *src_info;
        aligned_info->va = aligned_va;
        aligned_info->size = size;
    }
    if (aligned_size != NULL) {
        *aligned_size = size;
    }
}

static void svm_register_to_peer_free_va(u32 devid, u64 va, u64 size)
{
    u32 normal_flag = SVM_NORMAL_MALLOC_FLAG_VA_ONLY | SVM_NORMAL_MALLOC_FLAG_VA_WITH_MASTER;
    u64 hpage_size, offset, aligned_size;
    int ret;

    ret = svm_dbi_query_hpage_size(devid, &hpage_size);
    if (ret != 0) {
        svm_err("Get hpage size failed. (devid-%u)\n", devid);
        return;
    }

    offset = va - svm_align_down(va, hpage_size);
    aligned_size = svm_align_up(size + offset, hpage_size);
    ret = svm_normal_free(SVM_INVALID_DEVID, normal_flag, hpage_size, va - offset,
                          svm_get_non_dev_align_size(aligned_size));
    if (ret != 0) {
        svm_err("Free va failed. (devid=%u; va=0x%llx; size=%llu; offset=%llu)\n", devid, va, size, offset);
        return;
    }
}

static void svm_register_dst_recycle_proc(u32 devid, u32 udevid, void *svmm_inst)
{
    u32 recyle_num = 0;

    /* recycle dst devid match */
    while (1) {
        struct svm_global_va src_info;
        struct svm_global_va aligned_src_info;
        struct svm_dst_va dst_info;
        u64 va, svm_flag, aligned_size, npage_size;
        u32 smm_flag = 0;
        int ret;

        ret = svm_svmm_get_first_seg(svmm_inst, &devid, &va, &svm_flag, &src_info);
        if (ret != 0) {
            break;
        }
        if (svm_dbi_query_npage_size(devid, &npage_size) != 0) {
            break;
        }

        svm_register_compute_aligned(&src_info, npage_size, &aligned_src_info, &aligned_size);

        register_ops_pre_unmap(&aligned_src_info, devid, va);

        svm_dst_va_pack(devid, PROCESS_CP1, va, aligned_size, &dst_info);
        (void)svm_smm_client_unmap(&dst_info, &aligned_src_info, smm_flag);
        if (svm_va_is_in_range(src_info.va, src_info.size) == false) {
            svm_register_to_peer_free_va(devid, va, aligned_size);
        }

        (void)svm_svmm_del_seg(svmm_inst, devid, va, src_info.size, true);
        recyle_num++;
    }

    if (recyle_num > 0) {
        svm_info("Recycle success. (devid=%u; udevid=%u; recycle_num=%u)\n", devid, udevid, recyle_num);
    }
}

static void svm_register_dst_recycle(u32 devid)
{
    struct list_head *node = NULL, *tmp = NULL;
    u32 udevid;

    if (uda_get_udevid_by_devid_ex(devid, &udevid) != 0) {
        svm_err("Get udevid failed. (devid=%u)\n", devid);
        return;
    }

    list_for_each_safe(node, tmp, &register_head)
    {
        struct svm_register_node *register_node = list_entry(node, struct svm_register_node, node);
        void *va_handle = register_node->is_non_svm ? NULL : svm_handle_get(register_node->va);
        if ((register_node->is_non_svm) ||
            (va_handle != NULL)) { /* get va handle to access its priv svmm_inst in node */
            svm_register_dst_recycle_proc(devid, udevid, register_node->svmm_inst[devid]);
            if (va_handle != NULL) {
                svm_handle_put(va_handle);
            }
        }
    }
}

static int svm_is_register_to_peer_src_range_proc(void *va_handle, u64 start, struct svm_global_va *src_info,
                                                  void *priv)
{
    struct svm_register_priv *register_priv = (struct svm_register_priv *)priv;
    SVM_UNUSED(va_handle);
    SVM_UNUSED(start);

    if ((register_priv->src_va == src_info->va) && (register_priv->size <= src_info->size)) {
        register_priv->flag = true;
    }
    return 0;
}

bool svm_is_register_to_peer_src_range(u64 src_va, u64 size, u32 dst_devid)
{
    struct svm_register_priv priv = {.dst_devid = dst_devid, .src_va = src_va, .size = size, .flag = false};
    (void)svm_register_for_each_seg(dst_devid, svm_is_register_to_peer_src_range_proc, (void *)&priv);
    return priv.flag;
}

/* Check if va+size has overlap with any registered seg (for register para check) */
static int svm_va_can_register_peer_proc(void *va_handle, u64 start, struct svm_global_va *src_info, void *priv)
{
    struct svm_register_priv *register_priv = (struct svm_register_priv *)priv;
    SVM_UNUSED(va_handle);
    SVM_UNUSED(start);

    if ((register_priv->src_va < (src_info->va + src_info->size)) &&
        ((register_priv->src_va + register_priv->size) > src_info->va)) {
        return 1; /* found, stop iteration */
    }
    return 0;
}

bool svm_va_can_register_peer(u64 src_va, u64 size, u32 devid)
{
    struct svm_register_priv priv = {.dst_devid = devid, .src_va = src_va, .size = size, .flag = true};
    int ret;
    ret = svm_register_for_each_seg(devid, svm_va_can_register_peer_proc, (void *)&priv);
    if (ret == 1) {
        return false;
    }
    return true;
}

/* Lookup proc: find seg whose range contains query range (containment check), return start dev addr and registered
 * va/size */
static int svm_register_peer_lookup_proc(void *va_handle, u64 start, struct svm_global_va *src_info, void *priv)
{
    struct svm_register_priv *register_priv = (struct svm_register_priv *)priv;
    SVM_UNUSED(va_handle);

    if ((register_priv->src_va >= src_info->va) &&
        ((register_priv->src_va + register_priv->size) <= (src_info->va + src_info->size))) {
        u64 aligned_va = svm_align_down(src_info->va, register_priv->npage_size);
        register_priv->dst_start_va = start + (src_info->va - aligned_va);
        register_priv->src_start_va = src_info->va;
        register_priv->src_total_size = src_info->size;
        return 1; /* found, stop iteration */
    }
    return 0;
}

int svm_register_peer_lookup(struct svm_global_va *src_info, u32 peer_devid, u64 *peer_va)
{
    struct svm_register_priv priv = {.dst_devid = peer_devid, .src_va = src_info->va, .size = src_info->size};
    int ret;

    ret = svm_dbi_query_npage_size(peer_devid, &priv.npage_size);
    if (ret != 0) {
        svm_err("Get npage size failed. (devid=%u)\n", peer_devid);
        return ret;
    }

    if (svm_register_for_each_seg(peer_devid, svm_register_peer_lookup_proc, (void *)&priv) != 1) {
        return DRV_ERROR_NOT_EXIST;
    }
    src_info->va = priv.src_start_va;
    src_info->size = priv.src_total_size;
    *peer_va = priv.dst_start_va;
    return DRV_ERROR_NONE;
}

static void svm_register_svmm_destroy_insts(struct svm_register_node *register_node)
{
    u32 devid;

    for (devid = 0; devid < SVM_MAX_DEV_NUM; devid++) {
        svm_svmm_destroy_inst(register_node->svmm_inst[devid]);
    }
}

static int svm_register_svmm_create_insts(struct svm_register_node *register_node, u64 start, u64 size, u64 svm_flag)
{
    void *svmm_inst = NULL;
    u32 devid, tmp_devid;

    for (devid = 0; devid < SVM_MAX_DEV_NUM; devid++) {
        svmm_inst = svm_svmm_create_inst(start, size, SVMM_NON_OVERLAP, svm_flag);
        if (svmm_inst == NULL) {
            svm_err("Create inst priv failed. (va=0x%llx)\n", start);
            goto destroy_insts;
        }

        register_node->svmm_inst[devid] = svmm_inst;
    }

    return DRV_ERROR_NONE;
destroy_insts:
    for (tmp_devid = 0; tmp_devid < devid; tmp_devid++) {
        svm_svmm_destroy_inst(register_node->svmm_inst[tmp_devid]);
        register_node->svmm_inst[tmp_devid] = NULL;
    }
    return DRV_ERROR_OUT_OF_MEMORY;
}

static void svm_register_svmm_destroy_locked(struct svm_register_node *register_node)
{
    svm_register_svmm_destroy_insts(register_node);
    drv_user_list_del(&register_node->node);
    svm_ua_free(register_node);
}

static void svm_register_svmm_destroy(struct svm_register_node *register_node)
{
    (void)pthread_mutex_lock(&register_mutex);
    svm_register_svmm_destroy_locked(register_node);
    (void)pthread_mutex_unlock(&register_mutex);
}

static int svm_register_svmm_inst_release(void *svmm_inst, u32 devid, bool force)
{
    u32 recyle_num = 0;

    while (1) {
        struct svm_global_va src_info;
        struct svm_global_va aligned_src_info;
        struct svm_dst_va dst_info;
        u64 va, svm_flag, aligned_size, npage_size;
        u32 smm_flag = 0;
        u32 tmp_devid;
        int ret;

        ret = svm_svmm_get_first_seg(svmm_inst, &tmp_devid, &va, &svm_flag, &src_info);
        if (ret != 0) {
            break;
        }
        if (svm_dbi_query_npage_size(tmp_devid, &npage_size) != 0) {
            return DRV_ERROR_INNER_ERR;
        }

        if (force == false) {
            return DRV_ERROR_BUSY;
        }

        svm_register_compute_aligned(&src_info, npage_size, &aligned_src_info, &aligned_size);

        register_ops_pre_unmap(&aligned_src_info, devid, va);

        svm_dst_va_pack(devid, PROCESS_CP1, va, aligned_size, &dst_info);
        (void)svm_smm_client_unmap(&dst_info, &aligned_src_info, smm_flag);
        (void)svm_svmm_del_seg(svmm_inst, devid, va, src_info.size, true);
        recyle_num++;
    }

    if (recyle_num > 0) {
        u64 svmma_start, svmma_size, svm_flag;
        svm_svmm_parse_inst_info(svmm_inst, &svmma_start, &svmma_size, &svm_flag);
        svm_info("Release success. (va=0x%llx; size=0x%llx; devid=%u; recycle_num=%u)\n", svmma_start, svmma_size,
                 devid, recyle_num);
    }

    return DRV_ERROR_NONE;
}

static int svm_register_svmm_release(void *priv, bool force)
{
    struct svm_register_node *register_node = (struct svm_register_node *)priv;
    u32 devid;
    int ret;

    for (devid = 0; devid < SVM_MAX_DEV_NUM; devid++) {
        ret = svm_register_svmm_inst_release(register_node->svmm_inst[devid], devid, force);
        if (ret != DRV_ERROR_NONE) {
            return ret;
        }
    }
    svm_register_svmm_destroy(register_node);
    return 0;
}

static u32 _register_show(struct svm_register_node *register_node, char *buf, u32 buf_len)
{
    u32 devid;
    u32 len = 0;

    for (devid = 0; devid < SVM_MAX_DEV_NUM; devid++) {
        svm_info("register show devid[%u]:\n", devid);
        len += svm_svmm_inst_show_detail(register_node->svmm_inst[devid], buf + len, buf_len - len);
    }

    return len;
}

static u32 register_show(void *priv, char *buf, u32 buf_len)
{
    if (priv != NULL) {
        struct svm_register_node *register_node = (struct svm_register_node *)priv;
        return _register_show(register_node, buf, buf_len);
    }

    return 0;
}

static struct svm_register_node *svm_register_non_svm_svmm_create(u64 start, u64 size, u64 svm_flag)
{
    struct svm_register_node *register_node = NULL;
    int ret;

    register_node = (struct svm_register_node *)svm_ua_calloc(1, sizeof(*register_node));
    if (register_node == NULL) {
        svm_err("Malloc failed.\n");
        return NULL;
    }

    ret = svm_register_svmm_create_insts(register_node, start, size, svm_flag);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Create insts failed.\n");
        svm_ua_free(register_node);
        return NULL;
    }

    register_node->head.type = SVM_SHARE_TYPE_REGISTER;
    register_node->va = 0;
    register_node->is_non_svm = true;
    drv_user_list_add_tail(&register_node->node, &register_head);

    return register_node;
}

static void svm_register_non_svm_register_node_destroy(struct svm_register_node *node)
{
    if (node != NULL) {
        svm_register_svmm_destroy_insts(node);
        drv_user_list_del(&node->node);
        svm_ua_free(node);
    }
}

void svm_register_recycle(u32 devid)
{
    (void)pthread_mutex_lock(&register_mutex);
    svm_register_dst_recycle(devid);
    svm_register_non_svm_register_node_destroy(non_svm_register_node[devid]);
    non_svm_register_node[devid] = NULL;
    (void)pthread_mutex_unlock(&register_mutex);
}

static void __attribute__((destructor)) svm_host_non_svm_register_node_destroy(void)
{
    u32 devid = svm_get_host_devid();
    svm_register_non_svm_register_node_destroy(non_svm_register_node[devid]);
    non_svm_register_node[devid] = NULL;
}

static void *svm_register_get_non_svm_svmm_inst(u32 src_devid, u32 dst_devid)
{
    struct svm_register_node *register_node = NULL;

    if ((src_devid >= SVM_MAX_DEV_NUM) || (dst_devid >= SVM_MAX_DEV_NUM)) {
        return NULL;
    }

    (void)pthread_mutex_lock(&register_mutex);
    register_node = non_svm_register_node[src_devid];
    if (register_node != NULL) {
        (void)pthread_mutex_unlock(&register_mutex);
        return register_node->svmm_inst[dst_devid];
    }

    register_node = svm_register_non_svm_svmm_create(0, MAX_VA_SIZE, 0);
    if (register_node == NULL) {
        (void)pthread_mutex_unlock(&register_mutex);
        svm_err("Create failed. (devid=%d)\n", src_devid);
        return NULL;
    }

    non_svm_register_node[src_devid] = register_node;
    (void)pthread_mutex_unlock(&register_mutex);
    return register_node->svmm_inst[dst_devid];
}

static bool svm_register_should_criu_reset(void *priv)
{
    SVM_UNUSED(priv);
    return false;
}

static struct svm_priv_ops register_priv_ops = {
    .release = svm_register_svmm_release,
    .get_prop = NULL,
    .show = register_show,
    .should_criu_reset = svm_register_should_criu_reset,
};

static struct svm_svmm_seg_priv_ops register_seg_priv_ops = {
    .release = svm_register_svmm_release,
    .show = register_show,
};

static struct svm_register_node *svm_register_svmm_create(u64 start, u64 size, u64 svm_flag)
{
    struct svm_register_node *register_node = NULL;
    int ret;

    register_node = (struct svm_register_node *)svm_ua_calloc(1, sizeof(*register_node));
    if (register_node == NULL) {
        svm_err("Malloc failed. (va=0x%llx)\n", start);
        return NULL;
    }

    ret = svm_register_svmm_create_insts(register_node, start, size, svm_flag);
    if (ret != DRV_ERROR_NONE) {
        svm_ua_free(register_node);
        return NULL;
    }

    register_node->head.type = SVM_SHARE_TYPE_REGISTER;
    register_node->va = start;
    register_node->is_non_svm = false;
    drv_user_list_add_tail(&register_node->node, &register_head);

    return register_node;
}

static void *svm_normal_get_register_svmm(void *va_handle, u32 devid)
{
    struct svm_register_node *register_node = svm_get_priv(va_handle);
    return ((register_node != NULL) && (register_node->head.type == SVM_SHARE_TYPE_REGISTER)) ?
               register_node->svmm_inst[devid] :
               NULL;
}

static void *svm_normal_create_register_svmm(void *va_handle, u32 devid, u64 start, u64 size, u64 svm_flag)
{
    void *svmm_inst = NULL;

    (void)pthread_mutex_lock(&register_mutex);
    svmm_inst = svm_normal_get_register_svmm(va_handle, devid);
    if (svmm_inst == NULL) {
        struct svm_register_node *register_node = svm_register_svmm_create(start, size, svm_flag);
        if (register_node != NULL) {
            int ret = svm_set_priv(va_handle, (void *)register_node, &register_priv_ops);
            if (ret == 0) {
                svmm_inst = register_node->svmm_inst[devid];
            } else { /* may be set by prefetch */
                svm_err("Set va priv failed. (start=0x%llx)\n", start);
                svm_register_svmm_destroy_locked(register_node);
            }
        }
    }
    (void)pthread_mutex_unlock(&register_mutex);

    return svmm_inst;
}

static void *svm_vmm_get_register_svmm(void *seg_handle, u32 devid)
{
    struct svm_register_node *register_node = svm_svmm_get_seg_priv(seg_handle);
    return ((register_node != NULL) && (register_node->head.type == SVM_SHARE_TYPE_REGISTER)) ?
               register_node->svmm_inst[devid] :
               NULL;
}

static void *svm_vmm_create_register_svmm(void *seg_handle, u32 devid, u64 start, u64 size, u64 svm_flag)
{
    void *svmm_inst = NULL;

    (void)pthread_mutex_lock(&register_mutex);
    svmm_inst = svm_vmm_get_register_svmm(seg_handle, devid);
    if (svmm_inst == NULL) {
        struct svm_register_node *register_node = svm_register_svmm_create(start, size, svm_flag);
        if (register_node != NULL) {
            int ret = svm_svmm_set_seg_priv(seg_handle, (void *)register_node, &register_seg_priv_ops);
            if (ret == 0) {
                svmm_inst = register_node->svmm_inst[devid];
            } else { /* may be set by prefetch */
                svm_err("Set seg priv failed. (start=0x%llx)\n", start);
                svm_register_svmm_destroy_locked(register_node);
            }
        }
    }
    (void)pthread_mutex_unlock(&register_mutex);

    return svmm_inst;
}

static inline u32 svm_register_to_peer_get_smm_flag(u32 register_to_peer_flag)
{
    return ((register_to_peer_flag & SVM_REGISTER_TO_PEER_FLAG_READ_ONLY) != 0) ? SVM_SMM_FLAG_PG_RDONLY : 0;
}

static int svm_register(void *svmm_inst, u64 va, u32 devid, struct svm_global_va *src_info, u32 register_to_peer_flag)
{
    struct svm_dst_va dst_info;
    struct svm_global_va aligned_src_info;
    u64 npage_size, aligned_size;
    u64 svm_flag = ((register_to_peer_flag & SVM_REGISTER_TO_PEER_FLAG_READ_ONLY) != 0) ? SVM_FLAG_ATTR_PG_RDONLY : 0;
    u32 smm_flag = svm_register_to_peer_get_smm_flag(register_to_peer_flag);
    int ret;

    ret = uda_get_udevid_by_devid_ex(src_info->udevid, &src_info->udevid);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Get udevid failed. (devid=%u; ret=%d)\n", devid, ret);
        return ret;
    }

    /* SVMM: track with original src_info (byte-level overlap detection) */
    ret = svm_svmm_add_seg(svmm_inst, devid, va, svm_flag, src_info);
    if (ret != 0) {
        svm_err("Add seg failed. (devid=%u; va=0x%llx)\n", devid, va);
        return ret;
    }

    /* SMM: communicate with aligned va/size (UB framework constraint) */
    ret = svm_dbi_query_npage_size(devid, &npage_size);
    if (ret != 0) {
        svm_err("Get npage size failed. (devid=%u)\n", devid);
        (void)svm_svmm_del_seg(svmm_inst, devid, va, src_info->size, true);
        return ret;
    }
    svm_register_compute_aligned(src_info, npage_size, &aligned_src_info, &aligned_size);

    svm_dst_va_pack(devid, PROCESS_CP1, va, aligned_size, &dst_info);

    ret = svm_smm_client_map(&dst_info, &aligned_src_info, smm_flag);
    if (ret != 0) {
        svm_err("Smm map failed. (devid=%u; va=0x%llx)\n", devid, va);
        (void)svm_svmm_del_seg(svmm_inst, devid, va, src_info->size, true);
        if (ret == DRV_ERROR_NO_RESOURCES) {
            svm_report_ubmem_cfg_abnormal();
            ret = DRV_ERROR_INNER_ERR;
        }
        return ret;
    }

    ret = register_ops_post_map(&aligned_src_info, devid, va, register_to_peer_flag);
    if (ret != 0) {
        svm_err("Smm map failed. (devid=%u; va=0x%llx)\n", devid, va);
        (void)svm_smm_client_unmap(&dst_info, &aligned_src_info, 0);
        (void)svm_svmm_del_seg(svmm_inst, devid, va, src_info->size, true);
        return ret;
    }

    return ret;
}

static int svm_unregister(void *svmm_inst, u64 va, u32 devid)
{
    struct svm_dst_va dst_info;
    struct svm_global_va src_info;
    struct svm_global_va aligned_src_info;
    u64 start = va;
    u64 svm_flag = 0;
    u64 npage_size, aligned_size;
    u32 smm_flag = 0;
    int ret;

    ret = svm_svmm_get_seg_by_va(svmm_inst, &devid, &start, &svm_flag, &src_info);
    if (ret != 0) {
        svm_err("Addr is not registered. (va=0x%llx; devid=%u)\n", start, devid);
        return DRV_ERROR_PARA_ERROR;
    }
    if (va != start) {
        svm_err("Should be the registered start va. (va=0x%llx; start=0x%llx)\n", va, start);
        return DRV_ERROR_PARA_ERROR;
    }

    /* SMM: communicate with aligned va/size */
    ret = svm_dbi_query_npage_size(devid, &npage_size);
    if (ret != 0) {
        svm_err("Get npage size failed. (devid=%u)\n", devid);
        return ret;
    }
    svm_register_compute_aligned(&src_info, npage_size, &aligned_src_info, &aligned_size);

    register_ops_pre_unmap(&aligned_src_info, devid, start);

    svm_dst_va_pack(devid, PROCESS_CP1, start, aligned_size, &dst_info);
    ret = svm_smm_client_unmap(&dst_info, &aligned_src_info, smm_flag);
    if (ret != 0) {
        svm_warn("Smm unmap failed. (devid=%u; va=0x%llx)\n", devid, start);
    }

    /* SVMM: delete with original size */
    (void)svm_svmm_del_seg(svmm_inst, devid, start, src_info.size, true);

    return ret;
}

static int svm_vmm_register(void *va_handle, u64 va, u64 size, u32 devid, u32 register_to_peer_flag)
{
    void *vmm_svmm_inst = NULL, *register_svmm_inst = NULL;
    void *seg_handle = NULL;
    u32 seg_devid;
    u64 svm_flag, seg_va;
    struct svm_global_va src_info;
    int ret;

    vmm_svmm_inst = vmm_get_svmm(va_handle);
    if (vmm_svmm_inst == NULL) {
        svm_err("Vmm alloc not finish. (va=0x%llx)\n", va);
        return DRV_ERROR_PARA_ERROR;
    }

    seg_handle = svm_svmm_seg_handle_get(vmm_svmm_inst, va);
    if (seg_handle == NULL) { /* maybe va has been unmap after get prop or not map */
        svm_err("Get seg handle failed. (va=%llx)\n", va);
        return DRV_ERROR_PARA_ERROR;
    }

    /* seg va may be update to seg start va which is small than register va */
    seg_va = va;
    ret = svm_svmm_get_seg_by_va(vmm_svmm_inst, &seg_devid, &seg_va, &svm_flag, &src_info);
    if (ret != 0) {
        svm_svmm_seg_handle_put(seg_handle);
        /* should not in, because we has been get seg handle */
        svm_err("Get seg info failed. (va=%llx)\n", va);
        return ret;
    }

    vmm_restore_real_src_va(&src_info);

    register_svmm_inst = svm_vmm_get_register_svmm(seg_handle, devid);
    if (register_svmm_inst == NULL) { /* vmm map addr first register */
        register_svmm_inst = svm_vmm_create_register_svmm(seg_handle, devid, seg_va, src_info.size, svm_flag);
        if (register_svmm_inst == NULL) {
            svm_svmm_seg_handle_put(seg_handle);
            return DRV_ERROR_OUT_OF_MEMORY;
        }
    }

    /* update register va real src info */
    src_info.va += va - seg_va;
    src_info.size = size;
    ret = svm_register(register_svmm_inst, va, devid, &src_info, register_to_peer_flag);
    if (ret == 0) {
        u64 flag = svm_flag | SVM_FLAG_CAP_LDST;
        svm_svmm_mod_seg_svm_flag(seg_handle, flag);
    }

    svm_svmm_seg_handle_put(seg_handle);

    return ret;
}

static int svm_vmm_unregister(void *va_handle, u64 va, u32 devid)
{
    void *vmm_svmm_inst = NULL, *register_svmm_inst = NULL;
    void *seg_handle = NULL;
    int ret;

    vmm_svmm_inst = vmm_get_svmm(va_handle);
    if (vmm_svmm_inst == NULL) {
        svm_err("Vmm alloc not finish. (va=0x%llx)\n", va);
        return DRV_ERROR_PARA_ERROR;
    }

    seg_handle = svm_svmm_seg_handle_get(vmm_svmm_inst, va);
    if (seg_handle == NULL) { /* maybe va has been unmap after get prop or not map */
        svm_err("Get seg handle failed. (va=%llx)\n", va);
        return DRV_ERROR_PARA_ERROR;
    }

    register_svmm_inst = svm_vmm_get_register_svmm(seg_handle, devid);
    if (register_svmm_inst == NULL) { /* vmm map addr first register */
        svm_svmm_seg_handle_put(seg_handle);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = svm_unregister(register_svmm_inst, va, devid);
    if (ret == 0) {
        u64 flag = svm_svmm_get_seg_svm_flag(seg_handle) & (~SVM_FLAG_CAP_LDST);
        svm_svmm_mod_seg_svm_flag(seg_handle, flag);
    }

    svm_svmm_seg_handle_put(seg_handle);

    return ret;
}

static int svm_normal_register(void *va_handle, u64 va, u64 size, u32 devid, struct svm_prop *prop,
                               u32 register_to_peer_flag)
{
    void *svmm_inst = NULL;
    struct svm_global_va src_info;
    int ret;

    svmm_inst = svm_normal_get_register_svmm(va_handle, devid);
    if (svmm_inst == NULL) { /* vmm map addr first register */
        svmm_inst = svm_normal_create_register_svmm(va_handle, devid, prop->start, prop->aligned_size, prop->flag);
        if (svmm_inst == NULL) {
            return DRV_ERROR_OUT_OF_MEMORY;
        }
    }

    /* trans devid to udevid in svm_register */
    svm_global_va_pack(prop->devid, prop->tgid, va, size, &src_info);

    ret = svm_register(svmm_inst, va, devid, &src_info, register_to_peer_flag);
    if (ret == 0) {
        if (prop->devid < SVM_MAX_DEV_AGENT_NUM) {
            u64 flag = prop->flag | SVM_FLAG_CAP_LDST;
            svm_mod_prop_flag(va_handle, flag);
        }
    }

    return ret;
}

static int svm_normal_unregister(void *va_handle, u64 va, u32 devid, struct svm_prop *prop)
{
    void *svmm_inst = NULL;
    int ret;

    svmm_inst = svm_normal_get_register_svmm(va_handle, devid);
    if (svmm_inst == NULL) { /* vmm map addr first register */
        return DRV_ERROR_PARA_ERROR;
    }

    ret = svm_unregister(svmm_inst, va, devid);
    if (ret == 0) {
        if (prop->devid < SVM_MAX_DEV_AGENT_NUM) {
            u64 flag = prop->flag & (~SVM_FLAG_CAP_LDST);
            svm_mod_prop_flag(va_handle, flag);
        }
    }

    return ret;
}

static int svm_register_user_malloc_para_check(u64 va, u64 size, u32 devid)
{
    if (devid >= SVM_MAX_DEV_AGENT_NUM) {
        svm_err("Invalid para. (devid=%u)\n", devid);
        return DRV_ERROR_INVALID_DEVICE;
    }

    if ((va == 0) || (size == 0)) {
        svm_err("Src_va is zero or map_size is zero. (src_va=0x%llx; size=%llu)\n", va, size);
        return DRV_ERROR_INVALID_VALUE;
    }

    return 0;
}

static int svm_register_user_malloc_to_peer(u64 va, u64 size, u32 devid, u32 register_to_peer_flag, u64 *dst_va)
{
    u64 alloc_va, npage_size, aligned_va, aligned_size, dst_offset;
    u32 host_devid = svm_get_host_devid();
    struct svm_global_va src_info;
    void *svmm_inst = NULL;
    int ret;

    ret = svm_register_user_malloc_para_check(va, size, devid);
    if (ret != 0) {
        return ret;
    }

    svmm_inst = svm_register_get_non_svm_svmm_inst(host_devid, devid);
    if (svmm_inst == NULL) {
        svm_err("Init failed.\n");
        return DRV_ERROR_INNER_ERR;
    }

    /* Compute aligned va/size for SMM communication and va allocation */
    ret = svm_dbi_query_npage_size(devid, &npage_size);
    if (ret != 0) {
        svm_err("Get npage size failed. (devid=%u)\n", devid);
        return ret;
    }
    aligned_va = svm_align_down(va, npage_size);
    dst_offset = va - aligned_va;
    aligned_size = svm_align_up(size + dst_offset, npage_size);

    /* Duplicate check is handled by register_root rbtree in halHostRegister */

    ret = svm_register_to_peer_alloc_va(devid, aligned_va, aligned_size, &alloc_va);
    if (ret != 0) {
        return ret;
    }

    svm_global_va_pack(host_devid, (int)drvDeviceGetBareTgid(), va, size, &src_info);
    ret = svm_register(svmm_inst, alloc_va, devid, &src_info, register_to_peer_flag);
    if (ret != 0) {
        svm_register_to_peer_free_va(devid, alloc_va, aligned_size);
        svm_err("Svm register failed. (va=0x%llx; size=%llu; devid=%u)\n", va, size, devid);
        return ret;
    }

    *dst_va = alloc_va + dst_offset;
    return 0;
}

static int svm_unregister_user_malloc_to_peer(u64 va, u32 devid)
{
    u32 host_devid = svm_get_host_devid();
    struct svm_global_va src_info;
    void *svmm_inst = NULL;
    u64 dst_va, svm_flag, npage_size, aligned_size;
    int ret;

    if (devid >= SVM_MAX_DEV_AGENT_NUM) {
        svm_err("Invalid para. (devid=%u)\n", devid);
        return DRV_ERROR_INVALID_DEVICE;
    }

    svmm_inst = svm_register_get_non_svm_svmm_inst(host_devid, devid);
    if (svmm_inst == NULL) {
        svm_err("Init failed.\n");
        return DRV_ERROR_INNER_ERR;
    }

    svm_global_va_pack(host_devid, (int)drvDeviceGetBareTgid(), va, 0, &src_info);
    ret = svm_svmm_get_seg_by_src_udevid_and_va(svmm_inst, &devid, &dst_va, &svm_flag, &src_info);
    if (ret != 0) {
        svm_err("Has not been registered. (va=0x%llx; devid=%u)\n", va, devid);
        return ret;
    }

    ret = svm_unregister(svmm_inst, dst_va, devid);
    if (ret != 0) {
        svm_err("Svm unregister failed. (va=0x%llx; devid=%u)\n", va, devid);
        return ret;
    }

    /* free_va: use aligned_size (matching alloc) */
    ret = svm_dbi_query_npage_size(devid, &npage_size);
    if (ret != 0) {
        svm_err("Get npage size failed. (devid=%u)\n", devid);
        return ret;
    }
    svm_register_compute_aligned(&src_info, npage_size, NULL, &aligned_size);
    svm_register_to_peer_free_va(devid, dst_va, aligned_size);
    return 0;
}

static int svm_register_svm_to_peer_para_check(u64 va, u64 size, u32 devid)
{
    u32 host_devid = svm_get_host_devid();
    struct svm_prop prop;
    int ret;

    if (devid >= SVM_MAX_DEV_NUM) {
        svm_err("Invalid para. (devid=%u)\n", devid);
        return DRV_ERROR_INVALID_DEVICE;
    }

    ret = svm_get_prop(va, &prop);
    if (ret != 0) {
        svm_err("Get prop failed. (va=0x%llx)\n", va);
        return DRV_ERROR_PARA_ERROR;
    }

    if (!svm_flag_cap_is_support_register(prop.flag)) {
        svm_err("Addr cap does not support register. (va=0x%llx)\n", va);
        return DRV_ERROR_PARA_ERROR;
    }

    if (!(((prop.devid == host_devid) && (devid < SVM_MAX_DEV_AGENT_NUM)) ||
          ((prop.devid < SVM_MAX_DEV_AGENT_NUM) && (devid == host_devid)))) {
        svm_err("Only support h<->d. (register_va_devid=%u; access_devid=%u)\n", prop.devid, devid);
        return DRV_ERROR_PARA_ERROR;
    }

    if (size == 0) {
        svm_err("Size is zero.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    return 0;
}

static int svm_register_svm_to_peer(u64 va, u64 size, u32 devid, u32 register_to_peer_flag, u64 *dst_va)
{
    void *va_handle = NULL;
    struct svm_prop prop;
    u64 aligned_size;
    int ret;

    ret = svm_register_svm_to_peer_para_check(va, size, devid);
    if (ret != 0) {
        return ret;
    }

    ret = svm_get_prop(va, &prop);
    if (ret != 0) {
        svm_err("Get prop failed. (va=0x%llx)\n", va);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = svm_share_get_src_aligned_size(prop.devid, prop.flag, va, size, &aligned_size);
    if (ret != 0) {
        svm_err("Get aligned size failed. (src_devid=%u; flag=0x%llx; va=0x%llx; size=0x%llx)\n", prop.devid, prop.flag,
                va, size);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = svm_share_check_dst_va_align(va, aligned_size, devid, va);
    if (ret != 0) {
        svm_err("Check dst va align failed. (va=0x%llx; aligned_size=0x%llx; devid=%u)\n", va, aligned_size, devid);
        return DRV_ERROR_PARA_ERROR;
    }

    if ((va + size) > (prop.start + prop.size)) {
        svm_err("Size is out of bounds. (va=0x%llx; size=%d; align_size=0x%llx; prop start=0x%llx; prop size=0x%llx)\n",
                va, size, aligned_size, prop.start, prop.size);
        return (devid == svm_get_host_devid()) ? DRV_ERROR_PARA_ERROR : DRV_ERROR_INVALID_VALUE;
    }

    va_handle = svm_handle_get(va);
    if (va_handle == NULL) { /* maybe va has been free after get prop */
        svm_err("Get va handle failed. (va=0x%llx)\n", va);
        return DRV_ERROR_PARA_ERROR;
    }

    if (svm_flag_cap_is_support_vmm_unmap(prop.flag)) {
        ret = svm_vmm_register(va_handle, va, aligned_size, devid, register_to_peer_flag);
    } else {
        ret = svm_normal_register(va_handle, va, aligned_size, devid, &prop, register_to_peer_flag);
    }

    svm_handle_put(va_handle);
    *dst_va = (ret == 0) ? va : *dst_va;
    return ret;
}

static int _svm_register_to_peer(u64 va, u64 size, u32 devid, u32 register_to_peer_flag, u64 *dst_va)
{
    return svm_va_is_in_range(va, size) ?
               svm_register_svm_to_peer(va, size, devid, register_to_peer_flag, dst_va) :
               svm_register_user_malloc_to_peer(va, size, devid, register_to_peer_flag, dst_va);
}

static int svm_unregister_svm_to_peer(u64 va, u32 devid)
{
    void *va_handle = NULL;
    struct svm_prop prop;
    int ret;

    ret = svm_register_svm_to_peer_para_check(va, 1, devid);
    if (ret != 0) {
        return ret;
    }

    ret = svm_get_prop(va, &prop);
    if (ret != 0) {
        svm_err("Get prop failed. (va=0x%llx)\n", va);
        return DRV_ERROR_PARA_ERROR;
    }

    va_handle = svm_handle_get(va);
    if (va_handle == NULL) { /* maybe va has been free after get prop */
        svm_err("Get va handle failed. (va=0x%llx)\n", va);
        return DRV_ERROR_PARA_ERROR;
    }

    if (svm_flag_cap_is_support_vmm_unmap(prop.flag)) {
        ret = svm_vmm_unregister(va_handle, va, devid);
    } else {
        ret = svm_normal_unregister(va_handle, va, devid, &prop);
    }

    svm_handle_put(va_handle);
    return ret;
}

static int _svm_unregister_to_peer(u64 va, u32 devid)
{
    return svm_va_is_in_range(va, 1) ? svm_unregister_svm_to_peer(va, devid) :
                                       svm_unregister_user_malloc_to_peer(va, devid);
}

int svm_register_to_peer(u64 va, u64 size, u32 devid, u32 register_to_peer_flag, u64 *dst_va)
{
    int ret;

    svm_use_pipeline();
    ret = _svm_register_to_peer(va, size, devid, register_to_peer_flag, dst_va);
    svm_unuse_pipeline();

    return ret;
}

int svm_unregister_to_peer(u64 va, u32 devid)
{
    int ret;

    svm_use_pipeline();
    ret = _svm_unregister_to_peer(va, devid);
    svm_unuse_pipeline();

    return ret;
}

struct svm_register_seg_criu_priv {
    u32 src_devid;
    u32 dst_devid;
};

static int svm_register_seg_criu_restore(void *seg_handle, u64 start, struct svm_global_va *src_info, void *priv)
{
    struct svm_register_seg_criu_priv *criu_priv = (struct svm_register_seg_criu_priv *)priv;
    struct svm_dst_va dst_info;
    struct svm_global_va aligned_src_info;
    struct svm_prop src_prop;
    u64 svm_flag = svm_svmm_get_seg_svm_flag(seg_handle);
    u32 dst_devid = criu_priv->dst_devid;
    u32 src_devid = criu_priv->src_devid;
    u32 src_udevid;
    u64 npage_size, aligned_size;
    u32 smm_flag = svm_flag_attr_is_pg_rdonly(svm_flag) ? SVM_SMM_FLAG_PG_RDONLY : 0;
    int ret;

    ret = uda_get_udevid_by_devid_ex(src_devid, &src_udevid);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Get src udev id failed. (ret=%d; src_devid=%u)\n", ret, src_devid);
        return ret;
    }

    if (src_info->udevid != src_udevid) {
        return 0;
    }

    if (svm_va_is_in_range(src_info->va, src_info->size)) {
        ret = svm_get_prop(src_info->va, &src_prop);
        if (ret != DRV_ERROR_NONE) {
            svm_err("Get seg prop failed. (ret=%d; va=0x%llx)\n", ret, src_info->va);
            return ret;
        }
        src_info->tgid = src_prop.tgid;
    } else {
        src_info->tgid = (int)drvDeviceGetBareTgid();
    }
    svm_svmm_mod_seg_src_tgid(seg_handle, src_info->tgid);

    /* SMM: communicate with aligned va/size */
    ret = svm_dbi_query_npage_size(dst_devid, &npage_size);
    if (ret != 0) {
        svm_err("Get npage size failed. (devid=%u)\n", dst_devid);
        return ret;
    }
    svm_register_compute_aligned(src_info, npage_size, &aligned_src_info, &aligned_size);

    svm_dst_va_pack(dst_devid, PROCESS_CP1, start, aligned_size, &dst_info);
    ret = svm_smm_client_map(&dst_info, &aligned_src_info, smm_flag);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Restore register map failed. (ret=%d; dst_devid=%u; src_devid=%u; va=0x%llx; size=0x%llx)\n", ret,
                dst_devid, src_devid, start, src_info->size);
    }

    return ret;
}

static int svm_register_svmm_insts_criu_restore(void *svmm_inst[], u32 num, u32 devid)
{
    u32 i;

    for (i = 0; i < num; i++) {
        if (svmm_inst[i] != NULL) {
            struct svm_register_seg_criu_priv priv = {.src_devid = devid, .dst_devid = i};
            int ret = svm_svmm_for_each_seg_handle(svmm_inst[i], svm_register_seg_criu_restore, &priv);
            if (ret != DRV_ERROR_NONE) {
                return ret;
            }
        }
    }

    return DRV_ERROR_NONE;
}

static int svm_register_criu_restore(u32 devid, void *data)
{
    struct list_head *node = NULL;
    struct list_head *tmp = NULL;
    int ret = DRV_ERROR_NONE;

    SVM_UNUSED(data);

    (void)pthread_mutex_lock(&register_mutex);
    list_for_each_safe(node, tmp, &register_head)
    {
        struct svm_register_node *register_node = list_entry(node, struct svm_register_node, node);
        ret = svm_register_svmm_insts_criu_restore(register_node->svmm_inst, SVM_MAX_DEV_NUM, devid);
        if (ret != DRV_ERROR_NONE) {
            break;
        }
    }
    (void)pthread_mutex_unlock(&register_mutex);

    return ret;
}

static const struct svm_criu_ops g_register_criu_ops = {
    .name = "register",
    .restore = svm_register_criu_restore,
};

void __attribute__((constructor)) svm_register_init(void)
{
    int ret = svm_criu_register_ops(&g_register_criu_ops);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Register CRIU ops failed. (ret=%d)\n", ret);
    }
}
