/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <sys/types.h>
#include <unistd.h>

#include "devmm_svm.h"
#include "devmm_rbtree.h"
#include "svm_user_msg.h"
#include "devmm_virt_interface.h"
#include "devmm_dynamic_addr.h"

struct svm_da_node {
    struct rbtree_node rb_node;
    uint64_t va;
    uint64_t size;
    uint32_t flag;
};

struct svm_da_mng {
    pthread_rwlock_t lock;
    int dev_valid_flag[SVM_MAX_AGENT_NUM];
    uint64_t try_negotiate_va;
    struct rbtree_root da_range_root;
};

static struct svm_da_mng g_da_mng;

static void (*da_pre_free)(uint64_t va, uint64_t size) = NULL;

static struct svm_da_mng *svm_get_da_mng(void)
{
    return &g_da_mng;
}

void svm_da_free_set_ops(void (*pre_free)(uint64_t va, uint64_t size))
{
    da_pre_free = pre_free;
}

static uint32_t svm_da_get_first_dev(struct svm_da_mng *da_mng)
{
    uint32_t devid;

    for (devid = 0; devid < SVM_MAX_AGENT_NUM; devid++) {
        if (da_mng->dev_valid_flag[devid] != 0) {
            break;
        }
    }

    return devid;
}

static bool svm_da_has_dev(struct svm_da_mng *da_mng)
{
    return (svm_da_get_first_dev(da_mng) < SVM_MAX_AGENT_NUM);
}

static bool svm_da_has_master(uint32_t flag)
{
    return ((flag & SVM_DA_FLAG_WITH_MASTER) != 0);
}

static bool svm_da_range_is_valid(uint64_t va, uint64_t size)
{
    return ((size != 0) && (va <= va + size - 1));
}

static void svm_da_range_handle(struct rbtree_node *rb_node, struct rb_range_handle *range_handle)
{
    struct svm_da_node *node = rb_entry(rb_node, struct svm_da_node, rb_node);

    range_handle->start = node->va;
    range_handle->end = node->va + node->size - 1;
}

static int svm_da_add_node(struct svm_da_mng *da_mng, uint64_t va, uint64_t size, uint32_t flag)
{
    struct svm_da_node *node = NULL;
    int ret;

    if (svm_da_range_is_valid(va, size) == false) {
        DEVMM_DRV_ERR("Invalid dynamic addr range. (va=0x%llx; size=0x%llx)\n", va, size);
        return DRV_ERROR_INVALID_VALUE;
    }

    node = malloc(sizeof(*node));
    if (node == NULL) {
        DEVMM_DRV_ERR("Malloc node failed. (va=0x%llx)\n", (uint64_t)va);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    RB_CLEAR_NODE(&node->rb_node);
    node->va = va;
    node->size = size;
    node->flag = flag;

    ret = rbtree_insert_by_range(&da_mng->da_range_root, &node->rb_node, svm_da_range_handle);
    if (ret != 0) {
        free(node);
        DEVMM_DRV_ERR("Insert dynamic addr node failed. (va=0x%llx; size=0x%llx; ret=%d)\n", va, size, ret);
        return DRV_ERROR_REPEATED_USERD;
    }

    return DRV_ERROR_NONE;
}

static void svm_da_del_node(struct svm_da_mng *da_mng, struct svm_da_node *node)
{
    _rbtree_erase(&da_mng->da_range_root, &node->rb_node);
    free(node);
}

static struct svm_da_node *svm_da_find_node_in_range(struct svm_da_mng *da_mng, uint64_t va, uint64_t size)
{
    struct rb_range_handle range;
    struct rbtree_node *rb_node = NULL;

    if (svm_da_range_is_valid(va, size) == false) {
        return NULL;
    }

    range.start = va;
    range.end = va + size - 1;
    rb_node = rbtree_search_by_range(&da_mng->da_range_root, &range, svm_da_range_handle);
    if (rb_node == NULL) {
        return NULL;
    }

    return rb_entry(rb_node, struct svm_da_node, rb_node);
}

static struct svm_da_node *svm_da_find_node(struct svm_da_mng *da_mng, uint64_t va)
{
    struct svm_da_node *node = NULL;

    node = svm_da_find_node_in_range(da_mng, va, 1);
    if ((node != NULL) && (node->va == va)) {
        return node;
    }

    return NULL;
};

static int svm_da_mmap_agent(uint32_t devid, uint64_t *va, uint64_t size, int fixed_va_flag)
{
    DVdeviceptr tmp_va = *va;
    int ret;

    ret = devmm_process_task_mmap(devid, DEVDRV_PROCESS_CP1, &tmp_va, size, fixed_va_flag);
    if (ret != 0) {
        return ret;
    }

    /*
        case1: cp2 is luanched up before mmap cp1, mmap success here
        case2: cp2 is luanched up after mmap cp1, before mmap cp2, cp2 mapped when init with map seg getted by cp1
        case3: cp2 is luanched up after mmap cp2, cp2 mapped when init with map seg getted by cp1
    */
    ret = devmm_process_task_mmap(devid, DEVDRV_PROCESS_CP2, &tmp_va, size, fixed_va_flag);
    if (ret != 0) {
        /* may be mmap by halMemBindSibling, not return error */
    }

    *va = tmp_va;

    return 0;
}

static void svm_da_munmap_agent(uint32_t devid, uint64_t va, uint64_t size)
{
    int ret;

    ret = devmm_process_task_munmap(devid, DEVDRV_PROCESS_CP2, va, size);
    if (ret != 0) {
        if (ret != DRV_ERROR_NO_PROCESS) {
            DEVMM_DRV_WARN("Munmap cp2 failed. (devid=%u; va=0x%llx; ret=%d)\n", devid, va, ret);
        }
    }

    ret = devmm_process_task_munmap(devid, DEVDRV_PROCESS_CP1, va, size);
    if (ret != 0) {
        DEVMM_DRV_WARN("Munmap cp1 failed. (devid=%u; va=0x%llx; ret=%d)\n", devid, va, ret);
    }
}

static void svm_da_munmap_agents(struct svm_da_mng *da_mng, uint64_t va, uint64_t size, uint32_t min_devid,
                                 uint32_t max_devid)
{
    uint32_t devid;

    for (devid = min_devid; devid <= max_devid; devid++) {
        if (da_mng->dev_valid_flag[devid] == 0) {
            continue;
        }
        svm_da_munmap_agent(devid, va, size);
    }
}

static int svm_da_mmap_agents(struct svm_da_mng *da_mng, uint64_t *va, uint64_t size, uint32_t min_devid,
                              uint32_t max_devid)
{
    uint32_t devid;
    int ret;

    for (devid = min_devid; devid <= max_devid; devid++) {
        if (da_mng->dev_valid_flag[devid] == 0) {
            continue;
        }

        ret = svm_da_mmap_agent(devid, va, size, 1);
        if (ret != 0) {
            if (devid > 0) {
                svm_da_munmap_agents(da_mng, *va, size, min_devid, devid - 1);
            }
            return ret;
        }
    }

    return DRV_ERROR_NONE;
}

static int svm_da_mmap_master(uint64_t *va, uint64_t size)
{
    void *maped_addr = devmm_svm_map_with_flag((void *)(uintptr_t)*va, size, 0, 1);
    return (maped_addr != NULL) ? DRV_ERROR_NONE : DRV_ERROR_OUT_OF_MEMORY;
}

static void svm_da_munmap_master(uint64_t va, uint64_t size)
{
    devmm_svm_munmap((void *)(uintptr_t)va, size);
}

static int svm_da_fixed_va_mmap(struct svm_da_mng *da_mng, uint64_t *va, uint64_t size, uint32_t flag)
{
    int ret;

    if (!IS_ALIGNED(*va, DEVMM_HEAP_SIZE)) {
        DEVMM_DRV_ERR("Va is not heap align. (va=0x%llx)\n", *va);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = svm_da_mmap_agents(da_mng, va, size, 0, SVM_MAX_AGENT_NUM - 1);
    if (ret != 0) {
        return ret;
    }

    if (svm_da_has_master(flag)) {
        ret = svm_da_mmap_master(va, size);
        if (ret != 0) {
            svm_da_munmap_agents(da_mng, *va, size, 0, SVM_MAX_AGENT_NUM - 1);
            return ret;
        }
    }

    return DRV_ERROR_NONE;
}

/* success return negotiate_va, else return 0 */
static uint64_t svm_da_negotiate_va(uint32_t negotiate_devid, uint64_t negotiate_va_min, uint64_t negotiate_va_max,
                                    uint64_t size, uint32_t flag)
{
    uint64_t negotiate_va = negotiate_va_min;

    while (negotiate_va < negotiate_va_max) {
        int ret = svm_da_mmap_agent(negotiate_devid, &negotiate_va, size, 1);
        if (ret == 0) {
            /* negotiate success, different device not need negotiate */
            if (!svm_da_has_master(flag)) {
                return negotiate_va;
            }

            ret = svm_da_mmap_master(&negotiate_va, size);
            if (ret == 0) {
                return negotiate_va;
            }

            svm_da_munmap_agent(negotiate_devid, negotiate_va, size);
        }

        negotiate_va += DEVMM_HEAP_SIZE;
    }

    return 0ULL;
}

static int svm_da_negotiate_va_mmap(struct svm_da_mng *da_mng, uint64_t *va, uint64_t size, uint32_t flag)
{
    uint64_t negotiate_va, negotiate_va_min, negotiate_va_max;
    uint64_t va_max = DEVMM_PROC_USER_VA_MAX_SIZE - size;
    uint32_t devid;
    int ret;

    if (size > DEVMM_PROC_USER_VA_MAX_SIZE) {
        DEVMM_DRV_ERR("Invalid size. (size=0x%llx)\n", (uint64_t)size);
        return DRV_ERROR_INVALID_VALUE;
    }

    devid = svm_da_get_first_dev(da_mng);

    negotiate_va_min = da_mng->try_negotiate_va;
    negotiate_va_max = va_max;
    negotiate_va = svm_da_negotiate_va(devid, negotiate_va_min, negotiate_va_max, size, flag);
    if (negotiate_va == 0) {
        if (da_mng->try_negotiate_va > DEVMM_MAX_DYN_ALLOC_BASE) {
            negotiate_va_min = DEVMM_MAX_DYN_ALLOC_BASE;
            negotiate_va_max = (da_mng->try_negotiate_va >= va_max) ? va_max : da_mng->try_negotiate_va;
            negotiate_va = svm_da_negotiate_va(devid, negotiate_va_min, negotiate_va_max, size, flag);
        }
    }

    if (negotiate_va == 0) {
        DEVMM_DRV_ERR("Negotiate va failed. (size=0x%llx)\n", (uint64_t)size);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    ret = svm_da_mmap_agents(da_mng, &negotiate_va, size, devid + 1, SVM_MAX_AGENT_NUM - 1);
    if (ret != 0) {
        if (svm_da_has_master(flag)) {
            svm_da_munmap_master(negotiate_va, size);
        }
        svm_da_munmap_agent(devid, negotiate_va, size);
        return ret;
    }

    da_mng->try_negotiate_va = negotiate_va + size;
    *va = negotiate_va;

    return DRV_ERROR_NONE;
}

static int svm_da_mmap(struct svm_da_mng *da_mng, uint64_t *va, uint64_t size, uint32_t flag)
{
    if (*va != 0) {
        return svm_da_fixed_va_mmap(da_mng, va, size, flag);
    } else {
        return svm_da_negotiate_va_mmap(da_mng, va, size, flag);
    }
}

static void svm_da_munmap(struct svm_da_mng *da_mng, uint64_t va, uint64_t size, uint32_t flag)
{
    if (svm_da_has_master(flag)) {
        svm_da_munmap_master(va, size);
    }
    svm_da_munmap_agents(da_mng, va, size, 0, SVM_MAX_AGENT_NUM - 1);
}

static int _svm_da_alloc(struct svm_da_mng *da_mng, uint64_t *va, uint64_t size, uint32_t flag)
{
    int ret;

    if (!svm_da_has_dev(da_mng)) {
        DEVMM_DRV_ERR("No device has been opened.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = svm_da_mmap(da_mng, va, size, flag);
    if (ret != 0) {
        return ret;
    }

    ret = svm_da_add_node(da_mng, *va, size, flag);
    if (ret != 0) {
        svm_da_munmap(da_mng, *va, size, flag);
    }

    return ret;
}

int svm_da_alloc(uint64_t *va, uint64_t size, uint32_t flag)
{
    struct svm_da_mng *da_mng = svm_get_da_mng();
    int ret;

    if (!IS_ALIGNED(size, DEVMM_HEAP_SIZE)) {
        devmm_report_size_not_aligned("size", (uint64_t)size, (size_t)DEVMM_HEAP_SIZE);
        DEVMM_DRV_ERR("Size is not heap align. (size=0x%llx)\n", size);
        return DRV_ERROR_INVALID_VALUE;
    }

    (void)pthread_rwlock_wrlock(&da_mng->lock);
    ret = _svm_da_alloc(da_mng, va, size, flag);
    (void)pthread_rwlock_unlock(&da_mng->lock);

    if (ret == 0) {
        DEVMM_DRV_INFO("Da alloc success. (va=0x%llx; size=0x%llx; flag=0x%x)\n", *va, size, flag);
    }

    return ret;
}

static int _svm_da_free(struct svm_da_mng *da_mng, uint64_t va)
{
    struct svm_da_node *node = NULL;

    node = svm_da_find_node(da_mng, va);
    if (node == 0) {
        DEVMM_DRV_ERR("Invalid para. (va=0x%llx)\n", va);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (da_pre_free != NULL) {
        da_pre_free(node->va, node->size);
    }
    svm_da_munmap(da_mng, node->va, node->size, node->flag);
    svm_da_del_node(da_mng, node);

    return DRV_ERROR_NONE;
}

int svm_da_free(uint64_t va)
{
    struct svm_da_mng *da_mng = svm_get_da_mng();
    int ret;

    (void)pthread_rwlock_wrlock(&da_mng->lock);
    ret = _svm_da_free(da_mng, va);
    (void)pthread_rwlock_unlock(&da_mng->lock);

    if (ret == 0) {
        DEVMM_DRV_INFO("Da free success. (va=0x%llx)\n", va);
    }

    return ret;
}

static int _svm_da_query_size(struct svm_da_mng *da_mng, uint64_t va, uint64_t *size)
{
    struct svm_da_node *node = NULL;

    node = svm_da_find_node(da_mng, va);
    if (node == 0) {
        DEVMM_DRV_ERR("Invalid para. (va=0x%llx)\n", va);
        return DRV_ERROR_INVALID_VALUE;
    }

    *size = node->size;

    return DRV_ERROR_NONE;
}

int svm_da_query_size(uint64_t va, uint64_t *size)
{
    struct svm_da_mng *da_mng = svm_get_da_mng();
    int ret;

    (void)pthread_rwlock_rdlock(&da_mng->lock);
    ret = _svm_da_query_size(da_mng, va, size);
    (void)pthread_rwlock_unlock(&da_mng->lock);

    return ret;
}

static bool _svm_is_dyn_addr(struct svm_da_mng *da_mng, uint64_t va)
{
    return (svm_da_find_node_in_range(da_mng, va, 1) != NULL);
}

bool svm_is_dyn_addr(uint64_t va)
{
    struct svm_da_mng *da_mng = svm_get_da_mng();
    bool ret;

    (void)pthread_rwlock_rdlock(&da_mng->lock);
    ret = _svm_is_dyn_addr(da_mng, va);
    (void)pthread_rwlock_unlock(&da_mng->lock);

    return ret;
}

int svm_da_add_dev(uint32_t devid)
{
    struct svm_da_mng *da_mng = svm_get_da_mng();
    struct rbtree_node *rb_node = NULL;
    uint64_t va = 0;
    int ret = DRV_ERROR_NONE;

    (void)pthread_rwlock_wrlock(&da_mng->lock);
    if (da_mng->dev_valid_flag[devid] == 1) {
        (void)pthread_rwlock_unlock(&da_mng->lock);
        return DRV_ERROR_NONE;
    }

    rbtree_node_for_each(rb_node, &da_mng->da_range_root)
    {
        struct svm_da_node *node = rb_entry(rb_node, struct svm_da_node, rb_node);

        va = node->va;
        ret = svm_da_mmap_agent(devid, &va, node->size, 1);
        if (ret != 0) {
            DEVMM_DRV_ERR("Add dev failed. (devid=%u)\n", devid);
            break;
        }
    }

    if (ret != DRV_ERROR_NONE) {
        rbtree_node_for_each(rb_node, &da_mng->da_range_root)
        {
            struct svm_da_node *node = rb_entry(rb_node, struct svm_da_node, rb_node);
            if (va == node->va) {
                break;
            }

            svm_da_munmap_agent(devid, node->va, node->size);
        }
    } else {
        da_mng->dev_valid_flag[devid] = 1;
    }

    (void)pthread_rwlock_unlock(&da_mng->lock);

    return ret;
}

void svm_da_del_dev(uint32_t devid)
{
    struct svm_da_mng *da_mng = svm_get_da_mng();

    (void)pthread_rwlock_wrlock(&da_mng->lock);
    /* agent process has been exit, not need to unmap */
    da_mng->dev_valid_flag[devid] = 0;
    (void)pthread_rwlock_unlock(&da_mng->lock);
}

static void __attribute__((constructor)) svm_da_mng_init(void)
{
    struct svm_da_mng *da_mng = svm_get_da_mng();
    rbtree_init(&da_mng->da_range_root);
    (void)pthread_rwlock_init(&da_mng->lock, NULL);
    da_mng->try_negotiate_va = DEVMM_MAX_DYN_ALLOC_BASE;
}
