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
#include <stdlib.h>

#include "ascend_hal.h"
#include "rbtree.h"

#include "svm_log.h"
#include "svm_register_dma.h"
#include "svm_register_to_master.h"
#include "svm_pipeline.h"
#include "svm_dbi.h"
#include "svm_addr_desc.h"
#include "va_allocator.h"
#include "svm_pub.h"
#include "svm_criu.h"
#include "svm_sys_cmd.h"

struct svm_dma_node {
    struct rbtree_node node;
    u64 src_va;
    u64 size;
    u32 devid;
};

static struct rbtree_root g_svm_dma_tree[SVM_MAX_DEV_NUM];
static pthread_rwlock_t g_svm_dma_lock[SVM_MAX_DEV_NUM];

static void svm_dma_get_range(struct rbtree_node *rb_node, struct rb_range_handle *range)
{
    struct svm_dma_node *node = rb_entry(rb_node, struct svm_dma_node, node);
    range->start = node->src_va;
    range->end = node->src_va + node->size - 1;
}

static int svm_dma_do_register(u64 va, u64 size, u32 devid)
{
    struct svm_dst_va register_va;
    u32 flag = REGISTER_TO_MASTER_FLAG_PIN | REGISTER_TO_MASTER_FLAG_ACCESS_WRITE;
    u32 host_devid = svm_get_host_devid();

    svm_dst_va_pack(host_devid, 0, va, size, &register_va);
    return svm_register_to_master(devid, &register_va, flag);
}

static int svm_dma_do_unregister(u64 va, u64 size, u32 devid)
{
    struct svm_dst_va register_va;
    u32 flag = REGISTER_TO_MASTER_FLAG_PIN | REGISTER_TO_MASTER_FLAG_ACCESS_WRITE;
    u32 host_devid = svm_get_host_devid();

    svm_dst_va_pack(host_devid, 0, va, size, &register_va);
    return svm_unregister_to_master(devid, &register_va, flag);
}

static bool svm_dma_check_overlap(u64 va, u64 size, u32 devid)
{
    struct rb_range_handle range = {.start = va, .end = va + size - 1};

    return !rbtree_can_insert_range(&g_svm_dma_tree[devid], &range, svm_dma_get_range);
}

static struct svm_dma_node *svm_dma_rbtree_find(u64 va, u64 size, u32 devid)
{
    struct rb_range_handle range = {.start = va, .end = va + size - 1};
    struct rbtree_node *node;

    node = rbtree_search_by_range(&g_svm_dma_tree[devid], &range, svm_dma_get_range);
    if (node == NULL) {
        return NULL;
    }
    return rb_entry(node, struct svm_dma_node, node);
}

/* Alloc node + insert into rbtree */
static int svm_dma_rbtree_add(u64 va, u64 size, u32 devid)
{
    struct svm_dma_node *node;
    int ret;

    node = (struct svm_dma_node *)calloc(1, sizeof(struct svm_dma_node));
    if (node == NULL) {
        svm_err("Calloc failed.\n");
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    node->src_va = va;
    node->size = size;
    node->devid = devid;

    ret = rbtree_insert_by_range(&g_svm_dma_tree[devid], &node->node, svm_dma_get_range);
    if (ret != 0) {
        free(node);
        return DRV_ERROR_BUSY;
    }

    return DRV_ERROR_NONE;
}

static void svm_dma_rbtree_remove(u32 devid, struct svm_dma_node *node)
{
    _rbtree_erase(&g_svm_dma_tree[devid], &node->node);
    free(node);
}

static int _svm_dma_register(u64 va, u64 size, u32 devid)
{
    int ret;

    if (svm_dma_check_overlap(va, size, devid)) {
        svm_err("Address already registered. (va=0x%llx; size=%llu; devid=%u)\n", va, size, devid);
        return DRV_ERROR_BUSY;
    }

    ret = svm_dma_do_register(va, size, devid);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Register failed. (ret=%d; va=0x%llx; size=%llu; devid=%u)\n", ret, va, size, devid);
        return ret;
    }

    ret = svm_dma_rbtree_add(va, size, devid);
    if (ret != DRV_ERROR_NONE) {
        (void)svm_dma_do_unregister(va, size, devid);
    }

    return ret;
}

int svm_dma_register(u64 va, u64 size, u32 devid)
{
    int ret;

    (void)pthread_rwlock_wrlock(&g_svm_dma_lock[devid]);
    ret = _svm_dma_register(va, size, devid);
    (void)pthread_rwlock_unlock(&g_svm_dma_lock[devid]);

    if (ret == DRV_ERROR_NONE) {
        svm_debug("Dma register success. (va=0x%llx; size=%llu; devid=%u)\n", va, size, devid);
    }
    return ret;
}

static int _svm_dma_unregister(u64 va, u32 devid)
{
    struct svm_dma_node *node = NULL;
    u64 size;
    int ret = DRV_ERROR_NOT_EXIST;

    node = svm_dma_rbtree_find(va, 1, devid);
    if ((node == NULL) || (node->src_va != va)) {
        svm_err("Va not registered. (va=0x%llx; devid=%u; src_va=0x%llx)\n", va, devid, (node ? node->src_va : 0));
        return DRV_ERROR_NOT_EXIST;
    }

    size = node->size;
    svm_dma_rbtree_remove(devid, node);

    ret = svm_dma_do_unregister(va, size, devid);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Unregister failed. (ret=%d; va=0x%llx; devid=%u)\n", ret, va, devid);
    }
    return ret;
}

int svm_dma_unregister(u64 va, u32 devid)
{
    int ret;

    (void)pthread_rwlock_wrlock(&g_svm_dma_lock[devid]);
    ret = _svm_dma_unregister(va, devid);
    (void)pthread_rwlock_unlock(&g_svm_dma_lock[devid]);

    return ret;
}

bool svm_va_can_register_dma(u64 va, u64 size, u32 devid)
{
    bool ret;

    (void)pthread_rwlock_rdlock(&g_svm_dma_lock[devid]);
    ret = svm_dma_check_overlap(va, size, devid);
    (void)pthread_rwlock_unlock(&g_svm_dma_lock[devid]);

    return !ret;
}

int svm_register_dma_lookup(u64 *va, u64 *size, u32 devid)
{
    struct svm_dma_node *node = NULL;
    int ret = DRV_ERROR_NOT_EXIST;

    (void)pthread_rwlock_rdlock(&g_svm_dma_lock[devid]);
    node = svm_dma_rbtree_find(*va, *size, devid);
    if (node != NULL) {
        *va = node->src_va;
        *size = node->size;
        ret = DRV_ERROR_NONE;
    }
    (void)pthread_rwlock_unlock(&g_svm_dma_lock[devid]);

    return ret;
}

static void __attribute__((constructor)) svm_dma_init(void)
{
    u32 i;
    for (i = 0; i < SVM_MAX_DEV_NUM; i++) {
        rbtree_init(&g_svm_dma_tree[i]);
        pthread_rwlock_init(&g_svm_dma_lock[i], NULL);
    }
}

static int svm_register_dma_criu_restore_node(struct rbtree_node *rb_node, void *priv)
{
    u32 devid = *(u32 *)priv;
    struct svm_dma_node *node = rb_entry(rb_node, struct svm_dma_node, node);
    int ret;

    ret = svm_dma_do_register(node->src_va, node->size, devid);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Restore dma register failed. (ret=%d; va=0x%llx; size=%llu; devid=%u)\n", ret, node->src_va,
                node->size, devid);
    }

    return ret;
}

static int svm_register_dma_criu_restore(u32 devid, void *data)
{
    struct rbtree_node *cur = NULL;
    int ret = DRV_ERROR_NONE;

    SVM_UNUSED(data);

    if (devid >= SVM_MAX_DEV_NUM) {
        return DRV_ERROR_NONE;
    }

    (void)pthread_rwlock_rdlock(&g_svm_dma_lock[devid]);
    rbtree_node_for_each(cur, &g_svm_dma_tree[devid])
    {
        ret = svm_register_dma_criu_restore_node(cur, &devid);
        if (ret != DRV_ERROR_NONE) {
            break;
        }
    }
    (void)pthread_rwlock_unlock(&g_svm_dma_lock[devid]);

    return ret;
}

static const struct svm_criu_ops svm_register_dma_criu_ops = {
    .name = "register_dma",
    .restore = svm_register_dma_criu_restore,
};

static int svm_register_dma_dev_uninit(u32 devid)
{
    struct rbtree_node *cur = NULL;
    struct rbtree_node *tmp = NULL;
    u32 recycle_num = 0;

    (void)pthread_rwlock_wrlock(&g_svm_dma_lock[devid]);
    rbtree_node_for_each_prev_safe(cur, tmp, &g_svm_dma_tree[devid])
    {
        struct svm_dma_node *node = rb_entry(cur, struct svm_dma_node, node);
        (void)svm_dma_do_unregister(node->src_va, node->size, devid);
        svm_dma_rbtree_remove(devid, node);
        recycle_num++;
    }
    (void)pthread_rwlock_unlock(&g_svm_dma_lock[devid]);

    if (recycle_num > 0) {
        svm_info("Dma recycle success. (devid=%u; recycle_num=%u)\n", devid, recycle_num);
    }

    return DRV_ERROR_NONE;
}

static void __attribute__((constructor)) svm_register_dma_init(void)
{
    int ret = svm_criu_register_ops(&svm_register_dma_criu_ops);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Register dma CRIU ops failed.\n");
    }

    ret = svm_register_ioctl_dev_uninit_pre_handle(svm_register_dma_dev_uninit);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Register dma dev uninit handle failed.\n");
    }
}
