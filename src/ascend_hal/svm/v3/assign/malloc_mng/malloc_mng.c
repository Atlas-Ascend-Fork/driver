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
#include <semaphore.h>

#include "ascend_hal.h"
#include "uref.h"
#include "rbtree.h"
#include "securec.h"

#include "svm_log.h"
#include "svm_atomic.h"
#include "svm_user_adapt.h"
#include "svm_pagesize.h"
#include "normal_malloc.h"
#include "cache_malloc.h"
#include "svm_dbi.h"
#include "svm_apbi.h"
#include "va_mng.h"
#include "svm_init_pri.h"
#include "svm_criu.h"
#include "svm_sys_cmd.h"
#include "malloc_mng.h"

#define MNG_OPS_NUM 2

enum handle_state {
    HANDLE_STATE_UNINITED = 0U,
    HANDLE_STATE_INITED,
    HANDLE_STATE_MAX
};

struct handle_mng {
    struct rbtree_root root;
    pthread_rwlock_t rwlock;
    struct svm_mng_ops *ops[MNG_OPS_NUM];
};

typedef struct handle {
    struct rbtree_node node;
    u32 ref;
    u32 task_bitmap;

    pthread_rwlock_t rwlock;
    enum handle_state state;

    struct svm_prop prop;
    bool is_from_cache;
    u64 align;
    memSource mem_source;

    void *priv;
    struct svm_priv_ops *priv_ops;
} handle_t;

static struct handle_mng mng = {.root = RB_ROOT, .rwlock = PTHREAD_RWLOCK_INITIALIZER};

void svm_mng_set_ops(struct svm_mng_ops *ops)
{
    int i;

    pthread_rwlock_wrlock(&mng.rwlock);
    for (i = 0; i < MNG_OPS_NUM; i++) {
        if (mng.ops[i] == NULL) {
            mng.ops[i] = ops;
            break;
        }
    }
    pthread_rwlock_unlock(&mng.rwlock);
}

static int svm_mng_ops_post_malloc(void *handle, u64 start, struct svm_prop *prop)
{
    int i;

    for (i = 0; i < MNG_OPS_NUM; i++) {
        if ((mng.ops[i] != NULL) && (mng.ops[i]->post_malloc != NULL)) {
            int ret = mng.ops[i]->post_malloc(handle, start, prop);
            if (ret != 0) {
                return ret;
            }
        }
    }

    return 0;
}

static void svm_mng_ops_pre_free(void *handle, u64 start, struct svm_prop *prop)
{
    int i;

    for (i = 0; i < MNG_OPS_NUM; i++) {
        if ((mng.ops[i] != NULL) && (mng.ops[i]->pre_free != NULL)) {
            mng.ops[i]->pre_free(handle, start, prop);
        }
    }
}

static void rb_range_of_handle(struct rbtree_node *node, struct rb_range_handle *range)
{
    handle_t *handle = rb_entry(node, handle_t, node);

    range->start = handle->prop.start;
    range->end = handle->prop.start + handle->prop.size - 1;
}

static void handle_set_state(handle_t *handle, enum handle_state state)
{
    handle->state = state;
}

static bool handle_is_match_state(handle_t *handle, enum handle_state state)
{
    return (handle->state == state);
}

static void handle_wrlock(handle_t *handle)
{
    (void)pthread_rwlock_wrlock(&handle->rwlock);
}

static void handle_unlock(handle_t *handle)
{
    (void)pthread_rwlock_unlock(&handle->rwlock);
}

static int handle_set_priv(handle_t *handle, void *priv, struct svm_priv_ops *priv_ops)
{
    if ((handle->priv != NULL) || (handle->priv_ops != NULL)) {
        return DRV_ERROR_REPEATED_INIT;
    }

    handle->priv = priv;
    handle->priv_ops = priv_ops;
    return DRV_ERROR_NONE;
}

static void *handle_get_priv(handle_t *handle)
{
    return handle->priv;
}

static int handle_release_priv(handle_t *handle, bool force)
{
    int ret;

    if ((handle->priv_ops != NULL) && (handle->priv_ops->release != NULL)) {
        ret = handle->priv_ops->release(handle->priv, force);
        if (ret != DRV_ERROR_NONE) {
            svm_err("Handle priv_ops.pre_release failed. (ret=%d; start=0x%llx; size=%llu)\n", ret, handle->prop.start,
                    handle->prop.size);
            return ret;
        }

        handle->priv = NULL;
        handle->priv_ops = NULL;
        return DRV_ERROR_NONE;
    }

    if (handle->state == HANDLE_STATE_INITED) {
        return svm_flag_is_must_with_priv(handle->prop.flag) ? DRV_ERROR_INVALID_VALUE : DRV_ERROR_NONE;
    }

    return DRV_ERROR_NONE;
}

static bool handle_should_criu_reset(handle_t *handle)
{
    return (handle->priv_ops != NULL) && (handle->priv_ops->should_criu_reset != NULL) &&
           handle->priv_ops->should_criu_reset(handle->priv);
}

static int handle_get_prop(handle_t *handle, u64 va, struct svm_prop *prop)
{
    if ((handle->priv_ops != NULL) && (handle->priv_ops->get_prop != NULL)) {
        return handle->priv_ops->get_prop(handle->priv, va, prop);
    }

    *prop = handle->prop;
    return DRV_ERROR_NONE;
}

static handle_t *handle_get(u64 va)
{
    handle_t *handle = NULL;
    struct rbtree_node *node = NULL;
    struct rb_range_handle rb_range = {.start = va, .end = va};

    pthread_rwlock_rdlock(&mng.rwlock);
    node = rbtree_search_by_range(&mng.root, &rb_range, rb_range_of_handle);
    if (node != NULL) {
        handle = rb_entry(node, handle_t, node);
        if (handle_is_match_state(handle, HANDLE_STATE_INITED) == false) {
            pthread_rwlock_unlock(&mng.rwlock);
            return NULL;
        }
        svm_atomic_inc(&handle->ref);
    }
    pthread_rwlock_unlock(&mng.rwlock);

    return handle;
}

static void handle_put(handle_t *handle)
{
    svm_atomic_dec(&handle->ref);
}

static handle_t *handle_alloc(void)
{
    handle_t *handle = NULL;

    handle = svm_ua_calloc(1, sizeof(handle_t));
    if (handle == NULL) {
        svm_err("Calloc handle failed. (size=%llu)\n", sizeof(handle_t));
    }

    return handle;
}

static void handle_free(handle_t *handle)
{
    svm_ua_free(handle);
}

static void handle_init(handle_t *handle, struct svm_prop *prop, bool is_from_cache, u64 align)
{
    RB_CLEAR_NODE(&handle->node);

    handle_set_state(handle, HANDLE_STATE_INITED);

    (void)pthread_rwlock_init(&handle->rwlock, NULL);

    handle->ref = 0;
    handle->prop = *prop;
    handle->is_from_cache = is_from_cache;
    handle->align = align;

    if ((prop->devid <= SVM_MAX_DEV_AGENT_NUM) && (!svm_flag_attr_is_va_only(prop->flag))) {
        handle->task_bitmap = 1; /* cp */
    }

    handle->priv = NULL;
    handle->priv_ops = NULL;
}

static int handle_uninit(handle_t *handle, bool force)
{
    int ret;

    ret = handle_release_priv(handle, force);
    if (ret != DRV_ERROR_NONE) {
        return ((ret == DRV_ERROR_BUSY) ? DRV_ERROR_CLIENT_BUSY : ret);
    }

    handle_set_state(handle, HANDLE_STATE_UNINITED);
    RB_CLEAR_NODE(&handle->node);
    return DRV_ERROR_NONE;
}

static int _handle_insert(handle_t *handle)
{
    int ret;

    ret = rbtree_insert_by_range(&mng.root, &handle->node, rb_range_of_handle);
    if (ret != 0) {
        return DRV_ERROR_BUSY;
    }

    return 0;
}

static int _handle_erase(handle_t *handle)
{
    if (svm_atomic_read(&handle->ref) != 0) {
        svm_err("Handle is still occupied. (ref=%d)\n", handle->ref);
        return DRV_ERROR_CLIENT_BUSY;
    }

    _rbtree_erase(&mng.root, &handle->node);
    return DRV_ERROR_NONE;
}

static int handle_insert(handle_t *handle)
{
    int ret;

    pthread_rwlock_wrlock(&mng.rwlock);
    ret = _handle_insert(handle);
    pthread_rwlock_unlock(&mng.rwlock);

    return ret;
}

static int handle_erase(u64 start, handle_t **handle)
{
    handle_t *tmp = NULL;
    struct rbtree_node *node = NULL;
    struct rb_range_handle rb_range = {.start = start, .end = start};
    int ret;

    pthread_rwlock_wrlock(&mng.rwlock);
    node = rbtree_search_by_range(&mng.root, &rb_range, rb_range_of_handle);
    if (node == NULL) {
        pthread_rwlock_unlock(&mng.rwlock);
        return DRV_ERROR_NOT_EXIST;
    }

    tmp = rb_entry(node, handle_t, node);
    if (tmp->prop.start != start) {
        svm_err("Isn't start va. (start=0x%llx; handle.start=0x%llx)\n", start, tmp->prop.start);
        pthread_rwlock_unlock(&mng.rwlock);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = _handle_erase(tmp);
    pthread_rwlock_unlock(&mng.rwlock);

    if (ret == DRV_ERROR_NONE) {
        *handle = tmp;
    }

    return ret;
}

static inline u32 svm_flag_to_cache_flag(u64 flag)
{
    u32 cache_flag;

    cache_flag = svm_flag_attr_is_hpage(flag) ? SVM_CACHE_MALLOC_FLAG_PA_HPAGE : 0;
    cache_flag |= svm_flag_attr_is_p2p(flag) ? SVM_CACHE_MALLOC_FLAG_PA_P2P : 0;
    cache_flag |= svm_flag_attr_is_master_uva(flag) ? SVM_CACHE_MALLOC_FLAG_MASTER_UVA : 0;

    return cache_flag;
}

static inline u32 svm_flag_to_normal_flag(u64 flag, u32 numa_id)
{
    u32 normal_flag;

    normal_flag = svm_flag_attr_is_va_only(flag) ? SVM_NORMAL_MALLOC_FLAG_VA_ONLY : 0;
    normal_flag |= svm_flag_attr_is_hpage(flag) ? SVM_NORMAL_MALLOC_FLAG_PA_HPAGE : 0;
    normal_flag |= svm_flag_attr_is_gpage(flag) ? SVM_NORMAL_MALLOC_FLAG_PA_GPAGE : 0;
    normal_flag |= svm_flag_attr_is_p2p(flag) ? SVM_NORMAL_MALLOC_FLAG_PA_P2P : 0;
    normal_flag |= svm_flag_is_dev_cp_only(flag) ? SVM_NORMAL_MALLOC_FLAG_DEV_CP_ONLY : 0;
    normal_flag |= svm_flag_is_dev_private(flag) ? SVM_NORMAL_MALLOC_FLAG_DEV_PRIVATE : 0;
    normal_flag |= svm_flag_attr_is_specified_va(flag) ? SVM_NORMAL_MALLOC_FLAG_SPACIFIED_VA : 0;
    normal_flag |= svm_flag_attr_is_va_without_master(flag) ? 0 : SVM_NORMAL_MALLOC_FLAG_VA_WITH_MASTER;
    normal_flag |= svm_flag_attr_is_master_uva(flag) ? SVM_NORMAL_MALLOC_FLAG_MASTER_UVA : 0;
    normal_flag |= svm_flag_attr_is_pg_rdonly(flag) ? SVM_NORMAL_MALLOC_FLAG_PG_RDONLY : 0;
    normal_flag |= (svm_flag_cap_is_support_sync_copy(flag) | svm_flag_cap_is_support_async_copy_submit(flag) |
                    svm_flag_cap_is_support_dma_desc_convert(flag) | svm_flag_cap_is_support_sync_copy_ex(flag) |
                    svm_flag_cap_is_support_sync_copy_2d(flag)) ?
                       SVM_NORMAL_MALLOC_FLAG_CAP_COPY :
                       0;
    normal_flag |= svm_flag_is_va_only_without_populate(flag) ? SVM_NORMAL_MALLOC_FLAG_VA_WITHOUT_POPULATE : 0;

    if (numa_id != SVM_MALLOC_NUMA_NO_NODE) {
        normal_flag |= SVM_NORMAL_MALLOC_FLAG_FIXED_NUMA;
        normal_malloc_flag_set_numa_id(&normal_flag, numa_id);
    }

    return normal_flag;
}

static bool svm_flag_is_support_cache(u64 flag)
{
    if (svm_flag_is_by_pass_cache(flag)) {
        return false;
    }

    if (svm_flag_attr_is_contiguous(flag)) {
        return false;
    }

    if (svm_flag_attr_is_gpage(flag)) {
        return false;
    }

    if (svm_flag_attr_is_pg_rdonly(flag)) {
        return false;
    }

    return true;
}

static bool go_malloc_cache(u32 devid, u32 numa_id, u64 flag, u64 align, u64 size)
{
    u32 cache_flag = svm_flag_to_cache_flag(flag);
    return ((numa_id == SVM_MALLOC_NUMA_NO_NODE) && svm_flag_is_support_cache(flag) &&
            svm_cache_is_support(devid, cache_flag, align, (u32)size));
}

static int malloc_cache(u32 devid, u64 flag, u64 align, u64 size, u64 *start)
{
    u32 cache_flag = svm_flag_to_cache_flag(flag);
    int ret;

    ret = svm_cache_malloc(devid, cache_flag, align, start, size);
    if (ret != DRV_ERROR_NONE) {
        svm_info("Can't cache malloc. (devid=%u; cache_flag=0x%x; size=%llu)\n", devid, cache_flag, size);
    }

    return ret;
}

static int free_cache(u32 devid, u64 flag, u64 align, u64 start, u64 size)
{
    u32 cache_flag = svm_flag_to_cache_flag(flag);
    int ret;

    ret = svm_cache_free(devid, cache_flag, align, start, size);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Cache mem free failed. (devid=%u; cache_flag=0x%x; start=0x%llx; size=%llu)\n", devid, cache_flag,
                start, size);
    }

    return ret;
}

static void shrink_cache(u32 devid, u64 flag, u64 *shrinked_size)
{
    svm_cache_shrink(devid, svm_flag_to_cache_flag(flag), shrinked_size);
}

static int malloc_normal(u32 devid, u32 numa_id, u64 flag, u64 align, u64 size, u64 *start)
{
    u32 devid_ex = svm_flag_cap_is_support_ipc_close(flag) ? SVM_INVALID_DEVID : devid;
    u32 normal_flag = svm_flag_to_normal_flag(flag, numa_id);
    u64 shrinked_size = 0;
    int ret;

    ret = svm_normal_malloc(devid_ex, normal_flag, align, start, size);
    if ((ret == DRV_ERROR_OUT_OF_MEMORY) && (svm_flag_attr_is_va_only(flag) == false)) {
        shrink_cache(devid_ex, flag, &shrinked_size);
        if (shrinked_size >= size) {
            ret = svm_normal_malloc(devid_ex, normal_flag, align, start, size);
        }
    }
    if (ret != DRV_ERROR_NONE) {
        svm_info("Can't normal malloc. (devid=%u; normal_flag=%x; start=0x%llx; size=%llu)\n", devid_ex, normal_flag,
                 *start, size);
    }

    return ret;
}

static int free_normal(u32 devid, u64 flag, u64 align, u64 start, u64 size)
{
    u32 devid_ex = svm_flag_cap_is_support_ipc_close(flag) ? SVM_INVALID_DEVID : devid;
    u32 normal_flag = svm_flag_to_normal_flag(flag, SVM_MALLOC_NUMA_NO_NODE);
    int ret;

    ret = svm_normal_free(devid_ex, normal_flag, align, start, size);
    if ((ret != DRV_ERROR_NONE) && (ret != DRV_ERROR_BUSY)) {
        svm_err("Normal mem can not be freed. (devid=%u; normal_flag=%u; start=0x%llx; size=%llu; ret=%u)\n", devid_ex,
                normal_flag, start, size, ret);
    }

    return ret;
}

static int get_aligned_size(u32 devid, u64 flag, u64 align, u64 size, u64 *aligned_size)
{
    u64 pa_size;
    int ret;

    /* Contiguous page is compound page, pg_num should be power of 2. */
    if (svm_flag_attr_is_contiguous(flag)) {
        ret = svm_get_order_aligned_size(devid, flag, size, &pa_size);
        if (ret != DRV_ERROR_NONE) {
            svm_err("Get order aligned size failed. (ret=%d; devid=%u; flag=0x%llx; size=0x%llx)\n", ret, devid, flag,
                    size);
            return ret;
        }
        *aligned_size = svm_align_up(pa_size, align);
    } else {
        *aligned_size = svm_align_up(size, align);
    }

    return DRV_ERROR_NONE;
}

static u64 svm_get_master_uva_aligned_size(u32 devid, u64 size, u64 align, u64 flag)
{
    u64 align_size = 0;

    if (svm_flag_cap_is_support_vmm_va_free(flag)) { /* vmm devid is invalid, use vmm align */
        align_size = align;
    } else {
        (void)svm_dbi_query_hpage_size(devid, &align_size);
    }

    return svm_align_up(size, align_size);
}

/* size: in alloc size, out actual alloced size by page align */
static int _svm_malloc(u64 *start, u64 *size, u64 align, u64 flag, struct svm_malloc_location *location,
                       bool *is_from_cache)
{
    u32 devid = location->devid;
    u32 numa_id = location->numa_id;
    u64 aligned_size;
    bool go_cache;
    int ret;

    ret = get_aligned_size(devid, flag, align, *size, &aligned_size);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    go_cache = go_malloc_cache(devid, numa_id, flag, align, aligned_size);
    if (go_cache) {
        ret = malloc_cache(devid, flag, align, aligned_size, start);
    } else {
        /* Host uva need 2M aligned size to mpl. */
        aligned_size = svm_flag_attr_is_master_uva(flag) ?
                           svm_get_master_uva_aligned_size(devid, aligned_size, align, flag) :
                           aligned_size;
        ret = malloc_normal(devid, numa_id, flag, align, aligned_size, start);
    }
    if (ret == DRV_ERROR_NONE) {
        *is_from_cache = go_cache;
        *size = aligned_size;
    }

    return ret;
}

static int _svm_free(u32 devid, u64 flag, u64 align, u64 start, u64 size, bool is_from_cache)
{
    if (is_from_cache) {
        return free_cache(devid, flag, align, start, size);
    } else {
        return free_normal(devid, flag, align, start, size);
    }
}

int svm_get_dev_tgid(u32 devid, int *tgid)
{
    if (devid == svm_get_host_devid()) {
        *tgid = (int)drvDeviceGetBareTgid();
    } else {
        int ret = svm_apbi_query_tgid(devid, DEVDRV_PROCESS_CP1, tgid);
        if (ret != DRV_ERROR_NONE) {
            svm_err("Get mem tgid failed. (ret=%d; devid=%u)\n", ret, devid);
            return ret;
        }
    }

    return DRV_ERROR_NONE;
}

static int svm_prop_pack(u32 devid, u64 flag, u64 start, u64 size, bool is_from_cache, struct svm_prop *prop)
{
    prop->devid = svm_flag_attr_is_va_only(flag) ? SVM_INVALID_DEVID : devid;
    prop->flag = flag;
    prop->start = start;
    prop->size = size;
    prop->is_from_cache = is_from_cache;

    if (prop->devid == SVM_INVALID_DEVID) {
        return DRV_ERROR_NONE;
    }

    return svm_get_dev_tgid(prop->devid, &prop->tgid);
}

static int malloc_align_matched_with_page_size_check(u32 devid, u64 flag, u64 align)
{
    u64 pg_size;
    int ret;

    ret = svm_query_page_size_by_svm_flag(devid, flag, &pg_size);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Query page size failed. (ret=%d; devid=%u; flag=0x%llx)\n", ret, devid, flag);
        return ret;
    }

    if ((align % pg_size) != 0) {
        svm_err("Invalid align. (align=0x%llx; pg_size=0x%llx)\n", align, pg_size);
        return DRV_ERROR_INVALID_VALUE;
    }

    /* Contiguous page is compound page, pg_num should be power of 2. */
    if (svm_flag_attr_is_contiguous(flag) && (svm_check_power_of_2(align / pg_size) != 0)) {
        svm_err("For contiguous, align/pg_size should be power of 2. (align=0x%llx; pg_size=0x%llx)\n", align, pg_size);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

static int malloc_para_check(u64 size, u64 align, u64 flag, struct svm_malloc_location *location)
{
    u64 range_start, range_size;

    svm_get_va_range(&range_start, &range_size);

    /* U64_MAX - xx will align to 0  */
    if (size > range_size) {
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    if (location->numa_id > SVM_MALLOC_NUMA_NO_NODE) {
        svm_err("Invalid numa id. (numa_id=%u)\n", location->numa_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (svm_flag_attr_is_va_only(flag)) {
        return DRV_ERROR_NONE;
    }

    return malloc_align_matched_with_page_size_check(location->devid, flag, align);
}

int svm_malloc(u64 *start, u64 size, u64 align, u64 flag, struct svm_malloc_location *location)
{
    u32 devid = location->devid;
    u64 aligned_size;
    handle_t *handle = NULL;
    struct svm_prop prop;
    bool is_from_cache;
    int ret;

    ret = malloc_para_check(size, align, flag, location);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    aligned_size = size;
    ret = _svm_malloc(start, &aligned_size, align, flag, location, &is_from_cache);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    handle = handle_alloc();
    if (handle == NULL) {
        ret = DRV_ERROR_OUT_OF_MEMORY;
        goto free_mem;
    }

    ret = svm_prop_pack(devid, flag, *start, size, is_from_cache, &prop);
    if (ret != DRV_ERROR_NONE) {
        goto free_handle;
    }
    prop.aligned_size = aligned_size;

    handle_init(handle, &prop, is_from_cache, align);

    ret = svm_mng_ops_post_malloc(handle, *start, &handle->prop);
    if (ret != DRV_ERROR_NONE) {
        goto uninit_handle;
    }

    ret = handle_insert(handle);
    if (ret != DRV_ERROR_NONE) {
        goto ops_pre_free;
    }

    return 0;
ops_pre_free:
    svm_mng_ops_pre_free(handle, *start, &handle->prop);
uninit_handle:
    (void)handle_uninit(handle, true);
free_handle:
    handle_free(handle);
free_mem:
    (void)_svm_free(devid, flag, align, *start, aligned_size, is_from_cache);
    return ret;
}

int svm_free(u64 start)
{
    handle_t *handle = NULL;
    int ret;

    ret = handle_erase(start, &handle);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = handle_uninit(handle, false);
    if (ret != DRV_ERROR_NONE) {
        handle_insert(handle);
        return ret;
    }

    svm_mng_ops_pre_free(handle, start, &handle->prop);

    ret = _svm_free(handle->prop.devid, handle->prop.flag, handle->align, handle->prop.start, handle->prop.aligned_size,
                    handle->is_from_cache);
    if (ret == DRV_ERROR_BUSY) {
        ret = handle_insert(handle);
        if (ret != DRV_ERROR_NONE) {
            handle_free(handle);
            svm_err("Unlikely insert failed.\n");
            return DRV_ERROR_INNER_ERR;
        }
        return DRV_ERROR_BUSY;
    } else {
        handle_free(handle);
    }

    return ret;
}

static int _svm_recycle_handle(handle_t *handle)
{
    int ret;

    /* No need check handle->ref, force recycle. */
    _rbtree_erase(&mng.root, &handle->node);

    ret = handle_uninit(handle, true);
    if (ret != DRV_ERROR_NONE) {
        _handle_insert(handle);
        return DRV_ERROR_INNER_ERR;
    }

    ret = _svm_free(handle->prop.devid, handle->prop.flag, handle->align, handle->prop.start, handle->prop.aligned_size,
                    handle->is_from_cache);
    if (ret == DRV_ERROR_BUSY) {
        ret = _handle_insert(handle);
    } else {
        handle_free(handle);
    }

    return DRV_ERROR_NONE;
}

int svm_recycle_mem_by_dev(u32 devid)
{
    handle_t *handle = NULL;
    struct rbtree_node *cur = NULL;
    struct rbtree_node *n = NULL;
    int ret = DRV_ERROR_NONE;
    u32 recyle_num = 0;

    pthread_rwlock_wrlock(&mng.rwlock);
    rbtree_node_for_each_prev_safe(cur, n, &mng.root)
    {
        handle = rb_entry(cur, handle_t, node);
        if ((devid == handle->prop.devid) || (devid == SVM_INVALID_DEVID)) {
            ret = _svm_recycle_handle(handle);
            if (ret != DRV_ERROR_NONE) {
                break;
            }
            recyle_num++;
        }
    }
    pthread_rwlock_unlock(&mng.rwlock);

    if (recyle_num > 0) {
        svm_info("Recycle success. (devid=%u; recyle_num=%u)\n", devid, recyle_num);
    }

    return ret;
}

static u32 svm_show_mem_priv(handle_t *handle, char *buf, u32 buf_len)
{
    if ((handle->priv_ops != NULL) && (handle->priv_ops->show != NULL)) {
        return handle->priv_ops->show(handle->priv, buf, buf_len);
    }

    return 0;
}

static u32 svm_show_handle(handle_t *handle, int id, char *buf, u32 buf_len)
{
    struct svm_prop *prop = &handle->prop;

    if (buf == NULL) {
        svm_info("id %d: ref %u task_bitmap 0x%x devid %u start 0x%llx size 0x%llx "
                 "aligned_size 0x%llx, flag 0x%llx tgid %d state %d is_from_cache %d\n",
                 id, handle->ref, handle->task_bitmap, prop->devid, prop->start, prop->size, prop->aligned_size,
                 prop->flag, prop->tgid, handle->state, handle->is_from_cache);
        (void)svm_show_mem_priv(handle, buf, buf_len);
        return 0;
    } else {
        int len;
        u32 priv_format_len;

        if (buf_len == 0) {
            return 0;
        }

        len = snprintf_s(buf, buf_len, buf_len - 1,
                         "id %d: ref %u task_bitmap 0x%x devid %u start 0x%llx size 0x%llx "
                         "aligned_size 0x%llx, flag 0x%llx tgid %d state %d is_from_cache %d\n",
                         id, handle->ref, handle->task_bitmap, prop->devid, prop->start, prop->size, prop->aligned_size,
                         prop->flag, prop->tgid, handle->state, handle->is_from_cache);
        if (len < 0) {
            return 0;
        }
        priv_format_len = svm_show_mem_priv(handle, buf + len, buf_len - (u32)len);
        return (u32)len + priv_format_len;
    }
}

void svm_show_dev_mem(u32 devid, char *buf, u32 buf_len)
{
    struct rbtree_node *cur = NULL;
    int id = 0;
    u32 len = 0;

    (void)pthread_rwlock_rdlock(&mng.rwlock);
    rbtree_node_for_each(cur, &mng.root)
    {
        handle_t *handle = rb_entry(cur, handle_t, node);
        if ((devid == handle->prop.devid) || (devid == SVM_INVALID_DEVID)) {
            len += svm_show_handle(handle, id++, buf + len, buf_len - len);
        }
    }
    pthread_rwlock_unlock(&mng.rwlock);
}

void svm_show_mem(void)
{
    svm_show_dev_mem(SVM_INVALID_DEVID, NULL, 0);
}

static int _svm_for_each_handle(int (*func)(void *handle, u64 start, struct svm_prop *prop, void *priv), void *priv,
                                bool check_valid)
{
    struct rbtree_node *cur = NULL;
    int ret = 0;

    (void)pthread_rwlock_rdlock(&mng.rwlock);
    rbtree_node_for_each(cur, &mng.root)
    {
        handle_t *handle = rb_entry(cur, handle_t, node);
        if (check_valid && !handle_is_match_state((handle_t *)handle, HANDLE_STATE_INITED)) {
            continue;
        }
        ret = func((void *)handle, handle->prop.start, &handle->prop, priv);
        if (ret != 0) {
            break;
        }
    }
    pthread_rwlock_unlock(&mng.rwlock);
    return ret;
}

int svm_for_each_handle(int (*func)(void *handle, u64 start, struct svm_prop *prop, void *priv), void *priv)
{
    return _svm_for_each_handle(func, priv, false);
}

int svm_for_each_valid_handle(int (*func)(void *handle, u64 start, struct svm_prop *prop, void *priv), void *priv)
{
    return _svm_for_each_handle(func, priv, true);
}

int svm_get_prop(u64 va, struct svm_prop *prop)
{
    handle_t *handle = NULL;
    int ret;

    handle = handle_get(va);
    if (handle == NULL) {
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = handle_get_prop(handle, va, prop);
    handle_put(handle);
    return ret;
}

int svm_get_nearby_prop(u64 va, struct svm_prop *left_prop, bool *has_left, struct svm_prop *right_prop,
                        bool *has_right)
{
    struct rbtree_node *left_node = NULL;
    struct rbtree_node *right_node = NULL;
    struct rbtree_node *upper = NULL;
    int ret;

    if ((left_prop == NULL) || (has_left == NULL) || (right_prop == NULL) || (has_right == NULL)) {
        return DRV_ERROR_INVALID_VALUE;
    }

    *has_left = false;
    *has_right = false;

    pthread_rwlock_rdlock(&mng.rwlock);
    upper = rbtree_search_upper_bound_range(&mng.root, va, rb_range_of_handle);
    left_node = (upper != NULL) ? rbtree_prev(upper) : rbtree_last(&mng.root);
    right_node = upper;

    while (left_node != NULL) {
        handle_t *left_handle = rb_entry(left_node, handle_t, node);

        if (handle_is_match_state(left_handle, HANDLE_STATE_INITED)) {
            ret = handle_get_prop(left_handle, left_handle->prop.start, left_prop);
            if (ret == DRV_ERROR_NONE && left_prop->devid != SVM_INVALID_DEVID) {
                *has_left = true;
                break;
            }
        }
        left_node = rbtree_prev(left_node);
    }

    while (right_node != NULL) {
        handle_t *right_handle = rb_entry(right_node, handle_t, node);

        if (handle_is_match_state(right_handle, HANDLE_STATE_INITED)) {
            ret = handle_get_prop(right_handle, right_handle->prop.start, right_prop);
            if (ret == DRV_ERROR_NONE && right_prop->devid != SVM_INVALID_DEVID) {
                *has_right = true;
                break;
            }
        }
        right_node = rbtree_next(right_node);
    }
    pthread_rwlock_unlock(&mng.rwlock);

    return DRV_ERROR_NONE;
}

void svm_add_prop_flag(void *handle, u64 flag)
{
    ((handle_t *)handle)->prop.flag |= flag;
}

void svm_mod_prop_flag(void *handle, u64 flag)
{
    ((handle_t *)handle)->prop.flag = flag;
}

void svm_mod_prop_devid(void *handle, u32 devid)
{
    ((handle_t *)handle)->prop.devid = devid;
}

static void svm_mod_prop_tgid(void *handle, int tgid)
{
    ((handle_t *)handle)->prop.tgid = tgid;
}

void svm_set_task_bitmap(void *handle, u32 task_bitmap)
{
    ((handle_t *)handle)->task_bitmap = task_bitmap;
}

u32 svm_get_task_bitmap(void *handle)
{
    return ((handle_t *)handle)->task_bitmap;
}

void svm_set_mem_source(void *handle, u32 source_type)
{
    ((handle_t *)handle)->mem_source = source_type;
}

u32 svm_get_mem_source(void *handle)
{
    return ((handle_t *)handle)->mem_source;
}

bool svm_handle_mem_is_cache(void *handle)
{
    return ((handle_t *)handle)->is_from_cache;
}

void *svm_handle_get(u64 va)
{
    return (void *)handle_get(va);
}

void svm_handle_put(void *handle)
{
    handle_put((handle_t *)handle);
}

int svm_set_priv(void *handle, void *priv, struct svm_priv_ops *priv_ops)
{
    handle_t *tmp = handle;
    int ret;

    handle_wrlock(tmp);
    ret = handle_set_priv(tmp, priv, priv_ops);
    handle_unlock(tmp);
    return ret;
}

void svm_set_priv_ops(void *handle, struct svm_priv_ops *priv_ops)
{
    handle_t *tmp = handle;
    handle_wrlock(tmp);
    tmp->priv_ops = priv_ops;
    handle_unlock(tmp);
}

void *svm_get_priv(void *handle)
{
    return handle_get_priv((handle_t *)handle);
}

int svm_malloc_mng_criu_reset(u32 devid, void *data)
{
    struct rbtree_node *cur = NULL;
    struct rbtree_node *n = NULL;
    int ret = DRV_ERROR_NONE;

    SVM_UNUSED(data);

    pthread_rwlock_wrlock(&mng.rwlock);
    rbtree_node_for_each_prev_safe(cur, n, &mng.root)
    {
        handle_t *handle = rb_entry(cur, handle_t, node);
        if ((handle->prop.devid != devid) ||
            (!handle_is_match_state(handle, HANDLE_STATE_UNINITED) && !handle_should_criu_reset(handle))) {
            continue;
        }

        ret = _svm_recycle_handle(handle);
        if (ret != DRV_ERROR_NONE) {
            svm_err("Recycle handle failed during CRIU reset. (ret=%d; devid=%u; start=0x%llx; size=%llu)\n", ret,
                    devid, handle->prop.start, handle->prop.size);
            break;
        }
    }
    pthread_rwlock_unlock(&mng.rwlock);

    return ret;
}

static int svm_handle_recover_pa(void *va_handle, u64 start, struct svm_prop *prop, void *priv)
{
    handle_t *handle = va_handle;
    u32 normal_flag = svm_flag_to_normal_flag(prop->flag, SVM_MALLOC_NUMA_NO_NODE) |
                      SVM_NORMAL_MALLOC_FLAG_POPULATE_ONLY;
    u32 devid = *(u32 *)priv;
    int tgid, ret;

    if (devid != prop->devid) {
        return DRV_ERROR_NONE;
    }

    ret = svm_get_dev_tgid(devid, &tgid);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }
    svm_mod_prop_tgid(handle, tgid);

    if (!prop->is_from_cache) {
        ret = svm_normal_malloc(devid, normal_flag, handle->align, &start, prop->aligned_size);
        if (ret != DRV_ERROR_NONE) {
            svm_err("Recover handle pa failed. (ret=%d; devid=%u; flag=0x%llx; start=0x%llx; size=%llu)\n", ret,
                    prop->devid, prop->flag, start, prop->aligned_size);
            return ret;
        }

        ret = svm_mng_ops_post_malloc(va_handle, start, &handle->prop);
        if (ret != DRV_ERROR_NONE) {
            svm_err("Post malloc operation failed. (ret=%d; devid=%u; start=0x%llx; size=%llu)\n", ret, devid, start,
                    prop->aligned_size);
            (void)svm_normal_free(devid, normal_flag, handle->align, start, prop->aligned_size);
            return ret;
        }
    }

    return 0;
}

int svm_malloc_mng_criu_restore(u32 devid, void *data)
{
    SVM_UNUSED(data);
    return svm_for_each_valid_handle(svm_handle_recover_pa, &devid);
}

static const struct svm_criu_ops g_malloc_mng_criu_ops = {
    .name = "malloc_mng",
    .reset = svm_malloc_mng_criu_reset,
    .restore = svm_malloc_mng_criu_restore,
};

void __attribute__((constructor(SVM_INIT_PRI_FINAL - 2))) svm_mng_init(void)
{
    int ret;

    ret = svm_criu_register_ops(&g_malloc_mng_criu_ops);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Register CRIU ops failed.\n");
    }
}

#ifdef EMU_ST /* Because free mem tasks time, we only destruct in UT for memory leak detect. */
void __attribute__((destructor)) svm_mng_uninit(void)
{
    (void)svm_recycle_mem_by_dev(SVM_INVALID_DEVID);
}
#endif
