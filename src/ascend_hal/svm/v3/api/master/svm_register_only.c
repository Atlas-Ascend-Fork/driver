/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <stdlib.h>
#include <stddef.h>
#include <pthread.h>

#include "ascend_hal.h"

#include "pbl_uda_user.h"
#include "rbtree.h"

#include "svm_log.h"
#include "svm_register.h"
#include "svm_register_pcie_th.h"
#include "svm_register_dma.h"
#include "svm_register_to_master.h"
#include "svm_addr_desc.h"
#include "svm_pipeline.h"
#include "svm_dbi.h"
#include "va_allocator.h"
#include "malloc_mng.h"
#include "svm_criu.h"
#include "svm_sys_cmd.h"
#include "svm_master_init.h"
#include "svm_register_only.h"

struct register_rbtree {
    struct rbtree_root root;
    pthread_rwlock_t rwlock;
};

static struct register_rbtree g_register_ctx[SVM_MAX_DEV_NUM][SVM_MAX_DEV_NUM];

static inline u32 svm_get_register_map_type(uint32_t flag)
{
    u32 mask = ((1U << MEM_PROC_TYPE_BIT) - 1);
    return (flag & mask);
}

/* DEVDRV_PROCESS_CPTYPE_MAX represent the max valid bit number for proc_type*/
#define SVM_MAP_PROC_MASK ((1U << DEVDRV_PROCESS_CPTYPE_MAX) - 1)
static inline u32 svm_get_register_proc_type(uint32_t flag)
{
    return ((flag >> MEM_PROC_TYPE_BIT) & SVM_MAP_PROC_MASK);
}

#define SVM_MAP_ADVISE_MASK MEM_REGISTER_READ_ONLY
static inline uint32_t svm_register_get_access(uint32_t flag)
{
    return (flag & SVM_MAP_ADVISE_MASK);
}

static inline bool svm_is_register_host_pinned(uint32_t flag)
{
    return ((flag & MEM_REGISTER_HOST_PINNED) != 0);
}

struct register_node {
    struct rbtree_node node;
    u64 src;
    u64 size;
    u64 dst;
    u32 flag;
};

static void svm_register_node_get_range(struct rbtree_node *node, struct rb_range_handle *range)
{
    struct register_node *reg_node = rb_entry(node, struct register_node, node);
    range->start = reg_node->src;
    range->end = reg_node->src + reg_node->size - 1;
}

static int svm_register_node_insert(struct register_rbtree *rbtree, struct register_node *reg_node)
{
    int ret;

    (void)pthread_rwlock_wrlock(&rbtree->rwlock);
    ret = rbtree_insert_by_range(&rbtree->root, &reg_node->node, svm_register_node_get_range);
    (void)pthread_rwlock_unlock(&rbtree->rwlock);
    return ret;
}

/* Determine rbtree index by querying VA property.
 * If va is an svm-managed address, check whether it belongs to host or device:
 *   - src is host svm addr  → src_devid=host,  dst_devid=peer(devid)
 *   - src is device svm addr → src_devid=dev,   dst_devid=host
 * If va is a non-svm (plain host) address:
 *   - src_devid=host, dst_devid=peer(devid) */
static struct register_rbtree *svm_register_get_rbtree_index(u64 va, u32 devid)
{
    u32 host_devid = svm_get_host_devid();
    struct svm_prop prop = {0};
    int ret;

    if (svm_va_is_in_range(va, 1ULL)) {
        ret = svm_get_prop(va, &prop);
        if ((ret != DRV_ERROR_NONE) || (prop.devid >= SVM_MAX_DEV_NUM)) {
            svm_err("Invalid addr. (ret=%d; src_va=0x%llx)\n", ret, va);
            return NULL;
        }

        return &g_register_ctx[prop.devid][(prop.devid == host_devid) ? devid : host_devid];
    }
    return &g_register_ctx[host_devid][devid];
}

static struct register_node *svm_register_node_alloc(u64 va, u64 size, u64 dst, u32 flag)
{
    struct register_node *reg_node = NULL;

    reg_node = (struct register_node *)calloc(1, sizeof(struct register_node));
    if (reg_node == NULL) {
        return NULL;
    }
    reg_node->src = va;
    reg_node->size = size;
    reg_node->dst = dst;
    reg_node->flag = flag;
    return reg_node;
}

static void svm_register_node_free(struct register_node *reg_node)
{
    free(reg_node);
}

static int svm_register_node_alloc_and_insert(struct register_rbtree *rbtree, u64 va, u64 size, u64 dst, u32 flag)
{
    struct register_node *reg_node = NULL;
    int ret;

    reg_node = svm_register_node_alloc(va, size, dst, flag);
    if (reg_node == NULL) {
        svm_err("Alloc register node failed. (va=0x%llx; size=%llu; flag=0x%x)\n", va, size, flag);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    ret = svm_register_node_insert(rbtree, reg_node);
    if (ret != 0) {
        svm_register_node_free(reg_node);
        svm_err("Alloc register node failed. (va=0x%llx; size=%llu; flag=0x%x)\n", va, size, flag);
        return DRV_ERROR_BUSY;
    }

    return DRV_ERROR_NONE;
}

static struct register_node *svm_register_node_remove(struct register_rbtree *rbtree, u64 src, u64 size)
{
    struct rb_range_handle range = {.start = src, .end = src + size - 1};
    struct rbtree_node *rb_node = NULL;
    struct register_node *reg_node = NULL;

    (void)pthread_rwlock_wrlock(&rbtree->rwlock);
    rb_node = rbtree_search_by_range(&rbtree->root, &range, svm_register_node_get_range);
    if (rb_node != NULL) {
        reg_node = rb_entry(rb_node, struct register_node, node);
        if (reg_node->src != src) {
            (void)pthread_rwlock_unlock(&rbtree->rwlock);
            return NULL;
        }
        _rbtree_erase(&rbtree->root, rb_node);
        RB_CLEAR_NODE(rb_node);
    }
    (void)pthread_rwlock_unlock(&rbtree->rwlock);

    return (rb_node != NULL) ? rb_entry(rb_node, struct register_node, node) : NULL;
}

static int svm_register_node_query(struct register_rbtree *rbtree, struct svm_global_va *src_info, u64 *peer_va)
{
    struct rb_range_handle range = {.start = src_info->va, .end = src_info->va + src_info->size - 1};
    struct rbtree_node *rb_node = NULL;
    struct register_node *reg_node = NULL;

    (void)pthread_rwlock_rdlock(&rbtree->rwlock);
    rb_node = rbtree_search_by_range(&rbtree->root, &range, svm_register_node_get_range);
    if (rb_node == NULL) {
        (void)pthread_rwlock_unlock(&rbtree->rwlock);
        return DRV_ERROR_NOT_EXIST;
    }
    reg_node = rb_entry(rb_node, struct register_node, node);
    src_info->va = reg_node->src;
    src_info->size = reg_node->size;
    *peer_va = reg_node->dst;
    (void)pthread_rwlock_unlock(&rbtree->rwlock);

    return DRV_ERROR_NONE;
}

static bool svm_va_can_register(struct register_rbtree *rbtree, u64 va, u64 size)
{
    struct rb_range_handle range = {.start = va, .end = va + size - 1};
    bool can_insert;

    (void)pthread_rwlock_rdlock(&rbtree->rwlock);
    can_insert = rbtree_can_insert_range(&rbtree->root, &range, svm_register_node_get_range);
    (void)pthread_rwlock_unlock(&rbtree->rwlock);

    return can_insert;
}

static void __attribute__((constructor)) svm_register_only_dev_init(void)
{
    u32 host_devid = svm_get_host_devid();
    u32 devid;

    for (devid = 0; devid < SVM_MAX_DEV_NUM; devid++) {
        rbtree_init(&g_register_ctx[host_devid][devid].root);
        rbtree_init(&g_register_ctx[devid][host_devid].root);
        (void)pthread_rwlock_init(&g_register_ctx[host_devid][devid].rwlock, NULL);
        (void)pthread_rwlock_init(&g_register_ctx[devid][host_devid].rwlock, NULL);
    }
}

static void svm_register_rbtree_destroy(struct register_rbtree *rbtree)
{
    struct rbtree_node *node = NULL, *tmp = NULL;

    (void)pthread_rwlock_wrlock(&rbtree->rwlock);
    rbtree_node_for_each_prev_safe(node, tmp, &rbtree->root)
    {
        struct register_node *reg_node = rb_entry(node, struct register_node, node);
        _rbtree_erase(&rbtree->root, node);
        RB_CLEAR_NODE(node);
        svm_register_node_free(reg_node);
    }
    (void)pthread_rwlock_unlock(&rbtree->rwlock);
}

static int svm_register_only_dev_uninit(u32 devid)
{
    u32 host_devid = svm_get_host_devid();

    if (devid >= SVM_MAX_DEV_NUM) {
        return DRV_ERROR_NONE;
    }

    svm_register_rbtree_destroy(&g_register_ctx[host_devid][devid]);
    svm_register_rbtree_destroy(&g_register_ctx[devid][host_devid]);
    return DRV_ERROR_NONE;
}

static int svm_register_rbtree_criu_reset_one(struct rbtree_node *rb_node, struct register_rbtree *rbtree)
{
    struct register_node *reg_node = rb_entry(rb_node, struct register_node, node);

    if (svm_get_register_map_type(reg_node->flag) != HOST_IO_MAP_DEV) {
        return DRV_ERROR_NONE;
    }

    _rbtree_erase(&rbtree->root, rb_node);
    RB_CLEAR_NODE(rb_node);
    svm_register_node_free(reg_node);
    return DRV_ERROR_NONE;
}

static int svm_register_rbtree_criu_reset_tree(struct register_rbtree *rbtree)
{
    struct rbtree_node *node = NULL, *tmp = NULL;

    (void)pthread_rwlock_wrlock(&rbtree->rwlock);
    rbtree_node_for_each_prev_safe(node, tmp, &rbtree->root)
    {
        (void)svm_register_rbtree_criu_reset_one(node, rbtree);
    }
    (void)pthread_rwlock_unlock(&rbtree->rwlock);
    return DRV_ERROR_NONE;
}

static int svm_register_rbtree_criu_reset(u32 devid, void *data)
{
    u32 host_devid = svm_get_host_devid();

    SVM_UNUSED(data);
    svm_register_rbtree_criu_reset_tree(&g_register_ctx[host_devid][devid]);
    svm_register_rbtree_criu_reset_tree(&g_register_ctx[devid][host_devid]);
    return DRV_ERROR_NONE;
}

static const struct svm_criu_ops register_criu_ops = {
    .name = "register_rbtree",
    .reset = svm_register_rbtree_criu_reset,
};

static int __attribute__((constructor)) svm_register_only_init(void)
{
    int ret;

    ret = svm_register_ioctl_dev_uninit_pre_handle(svm_register_only_dev_uninit);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Register dev uninit pre handle failed.\n");
    }

    ret = svm_criu_register_ops(&register_criu_ops);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Register CRIU ops failed.\n");
    }

    return 0;
}

static int svm_host_pinned_flag_check(u32 flag)
{
    u32 map_type = svm_get_register_map_type(flag);
    u32 access = svm_register_get_access(flag);

    if (access == MEM_REGISTER_READ_ONLY) {
        svm_run_info("MEM_REGISTER_HOST_PINNED cannot combine with MEM_REGISTER_READ_ONLY. (flag=0x%x)\n", flag);
        return DRV_ERROR_NOT_SUPPORT;
    }

    /* HOST_PINNED can only be used alone or with HOST_MEM_MAP_DEV_V2 */
    if (map_type == 0 || map_type == HOST_MEM_MAP_DEV_V2) {
        return 0;
    }

    svm_run_info("MEM_REGISTER_HOST_PINNED can only combine with HOST_MEM_MAP_DEV_V2. (flag=0x%x)\n", flag);
    return DRV_ERROR_NOT_SUPPORT;
}

static inline u32 svm_get_register_to_peer_flag(u32 flag)
{
    return ((flag & MEM_REGISTER_READ_ONLY) != 0) ? SVM_REGISTER_TO_PEER_FLAG_READ_ONLY : 0;
}

static inline u32 svm_get_register_pcie_th_flag(u32 flag)
{
    return ((flag & MEM_REGISTER_READ_ONLY) != 0) ? SVM_REGISTER_PCIE_TH_FLAG_READ_ONLY : 0;
}

static int svm_register_dev_flag_check(u32 devid, u32 flag)
{
    u32 map_type = svm_get_register_map_type(flag);
    int task_type = (int)svm_get_register_proc_type(flag);

    if (map_type >= HOST_REGISTER_MAX_TPYE) {
        svm_run_info("Not support map_type. (flag=0x%x; map_type=%u)\n", flag, map_type);
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (task_type != DEVDRV_PROCESS_CP1) {
        svm_run_info("Only support CP1. (flag=0x%x; task_type=%d)\n", flag, task_type);
        return DRV_ERROR_NOT_SUPPORT;
    }

    if ((map_type != DEV_SVM_MAP_HOST) && (devid >= SVM_MAX_DEV_NUM)) {
        svm_err("Invalid devid. (map_type=%u; devid=%u)\n", map_type, devid);
        return DRV_ERROR_INVALID_DEVICE;
    }

    return 0;
}

int svm_register_lookup(struct svm_global_va *src_info, u32 peer_devid, u64 *peer_va)
{
    u32 host_devid = svm_get_host_devid();
    return svm_register_node_query(&g_register_ctx[host_devid][peer_devid], src_info, peer_va);
}

static int svm_register_host_pinned_para_check(void *src_ptr, u64 size, u32 flag, u32 devid)
{
    struct svm_prop prop = {0};
    u64 va = (u64)(uintptr_t)(src_ptr);
    int ret;

    if (svm_is_register_host_pinned(flag)) {
        ret = svm_host_pinned_flag_check(flag);
        if (ret != 0) {
            return ret;
        }
        if (svm_va_is_in_range(va, size)) {
            if (!svm_dbi_is_cc_vm()) {
                svm_run_info("HOST_PINNED only supports svm address in confidential VM. (devid=%u)\n", devid);
                /* The error code cannot be modified: callers rely on DRV_ERROR_INVALID_VALUE */
                return DRV_ERROR_INVALID_VALUE;
            }

            ret = svm_get_prop(va, &prop);
            if (ret != DRV_ERROR_NONE) {
                svm_run_info("Get svm prop not success. (ret=%d; va=0x%llx; size=0x%llx)\n", ret, va, size);
                return DRV_ERROR_NOT_SUPPORT;
            }

            if (!svm_flag_cap_is_support_normal_free(prop.flag)) {
                svm_run_info("HOST_PINNED only supports allocated host svm memory. (devid=%u; flag=0x%llx)\n", devid,
                             prop.flag);
                return DRV_ERROR_NOT_SUPPORT;
            }

            if ((prop.devid != svm_get_host_devid()) || (va < prop.start) || (size > prop.aligned_size) ||
                ((va - prop.start) > (prop.aligned_size - size))) {
                svm_run_info("HOST_PINNED only supports allocated host svm memory. "
                             "(devid=%u; va=0x%llx; size=0x%llx; start=0x%llx; aligned_size=0x%llx)\n",
                             prop.devid, va, size, prop.start, prop.aligned_size);
                return DRV_ERROR_NOT_SUPPORT;
            }
        }
    }
    return 0;
}

static int svm_register_svm_addr_check(u64 va, u64 size, u32 flag, u32 devid)
{
    struct svm_prop prop = {0};
    u64 npage_size;
    int ret;

    if (svm_va_is_in_range(va, size) == false) {
        return 0;
    }

    if (svm_get_register_map_type(flag) == DEV_SVM_MAP_HOST) {
        ret = svm_get_prop(va, &prop);
        if ((ret != DRV_ERROR_NONE) || (prop.devid != devid)) {
            svm_err("Invalid addr. (ret=%d; src_va=0x%llx; devid=%u)\n", ret, va, devid);
            return DRV_ERROR_PARA_ERROR;
        }
        return 0;
    }

    ret = svm_dbi_query_npage_size(devid, &npage_size);
    if (ret != 0) {
        svm_err("Get npage size failed. (devid=%u)\n", devid);
        return ret;
    }

    if (SVM_IS_ALIGNED(va, npage_size) == false) {
        svm_err("Svm addr not page aligned. (va=0x%llx; npage_size=0x%llx)\n", va, npage_size);
        return DRV_ERROR_PARA_ERROR;
    }

    return 0;
}

static int svm_register_para_check(void *src_ptr, u64 size, u32 flag, u32 devid, void **dst_ptr)
{
    u64 va = (u64)(uintptr_t)(src_ptr);
    int ret;

    if ((src_ptr == NULL) || (size == 0)) {
        svm_err("srcPtr is null or size is zero. (src_ptr=%u; size=%llu)\n", (src_ptr == NULL), size);
        return DRV_ERROR_PARA_ERROR;
    }

    if ((devid >= SVM_MAX_DEV_NUM) || !svm_is_dev_open(devid)) {
        svm_err("Invalid devid. (devid=%u; flag=0x%x)\n", devid, flag);
        return DRV_ERROR_INVALID_DEVICE;
    }

    if ((svm_is_register_host_pinned(flag) == false) && (dst_ptr == NULL)) {
        svm_err("dst_ptr is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (svm_is_valid_range(va, size) == false) {
        svm_err("Invalid para. (va=0x%llx; size=0x%llx)\n", va, size);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = svm_register_host_pinned_para_check(src_ptr, size, flag, devid);
    if (ret != 0) {
        return ret;
    }

    ret = svm_register_svm_addr_check(va, size, flag, devid);
    if (ret != 0) {
        return ret;
    }

    return svm_register_dev_flag_check(devid, flag);
}

/* HOST_MEM_MAP_DEV_V2 register: UB scenario registers seg for copy performance (same as PCIe);
 * non-svm addr also registers DMA for copy acceleration */
static int svm_host_mem_map_dev_v2(u64 va, u64 size, u32 devid, u32 register_to_peer_flag, u32 register_pcie_th_flag,
                                   u64 *dst_va)
{
    int ret;

    if (svm_get_device_connect_type(devid) == HOST_DEVICE_CONNECT_TYPE_UB) {
        ret = svm_register_to_peer(va, size, devid, register_to_peer_flag, dst_va);
        if ((ret == DRV_ERROR_NONE) && (svm_va_is_in_range(va, size) == false)) {
            ret = svm_dma_register(va, size, devid);
            if (ret != DRV_ERROR_NONE) {
                svm_unregister_to_peer(va, devid);
            }
        }
    } else {
        ret = svm_register_pcie_th(va, size, register_pcie_th_flag, devid, dst_va);
    }

    return ret;
}

static int svm_host_mem_unmap_dev_v2(u64 va, u32 devid)
{
    int ret = DRV_ERROR_NONE;

    if (svm_get_device_connect_type(devid) == HOST_DEVICE_CONNECT_TYPE_UB) {
        if (svm_va_is_in_range(va, 1) == false) {
            ret = svm_dma_unregister(va, devid);
        }
        if (ret == DRV_ERROR_NONE) {
            ret = svm_unregister_to_peer(va, devid);
        }
    } else {
        ret = svm_unregister_pcie_th(va, devid);
    }

    return ret;
}

/* Perform the actual register operation based on map_type. Returns 0 on success. */
static int svm_register(u64 va, u64 size, u32 flag, u32 devid, u64 *dst_va)
{
    u32 map_type = svm_get_register_map_type(flag);
    u32 register_to_peer_flag = svm_get_register_to_peer_flag(flag);
    u32 register_pcie_th_flag = svm_get_register_pcie_th_flag(flag);
    int ret;

    if (svm_is_register_host_pinned(flag) && (map_type != HOST_MEM_MAP_DEV_V2)) {
        return svm_dma_register(va, size, devid);
    }

    switch (map_type) {
        case HOST_MEM_MAP_DEV:
            /* todo add dev method check */
            ret = DRV_ERROR_NOT_SUPPORT;
            break;
        case HOST_SVM_MAP_DEV:
            /* todo add dev method check */
            ret = DRV_ERROR_NOT_SUPPORT;
            break;
        case DEV_SVM_MAP_HOST:
            ret = svm_register_to_peer(va, size, svm_get_host_devid(), register_to_peer_flag, dst_va);
            break;
        case HOST_MEM_MAP_DEV_V2:
            ret = svm_host_mem_map_dev_v2(va, size, devid, register_to_peer_flag, register_pcie_th_flag, dst_va);
            break;
        case HOST_IO_MAP_DEV:
            register_pcie_th_flag |= SVM_REGISTER_PCIE_TH_FLAG_VA_IO_MAP;
            ret = svm_register_pcie_th(va, size, register_pcie_th_flag, devid, dst_va);
            break;
        default:
            ret = DRV_ERROR_NOT_SUPPORT;
            break;
    }

    return ret;
}

static int svm_unregister(u64 va, u32 flag, u32 devid);
drvError_t halHostRegister(void *src_ptr, UINT64 size, UINT32 flag, UINT32 devid, void **dst_ptr)
{
    u64 dst_va = 0;
    u64 va = (u64)(uintptr_t)(src_ptr);
    struct register_rbtree *rbtree = NULL;
    int ret;

    DRV_CURRENT_API_SCOPE();

    svm_debug("HostRegister enter. (src_ptr=0x%llx; size=%llu; flag=0x%x; devid=%u)\n", va, size, flag, devid);

    ret = svm_register_para_check(src_ptr, size, flag, devid, dst_ptr);
    if (ret != 0) {
        return (drvError_t)ret;
    }

    /* Check rbtree for duplicate registration before doing actual register */
    rbtree = svm_register_get_rbtree_index(va, devid);
    if (rbtree == NULL) {
        return DRV_ERROR_PARA_ERROR;
    }
    if (svm_va_can_register(rbtree, va, size) == false) {
        svm_err("Addr is already registered. (src_va=0x%llx; size=%llu; devid=%u)\n", va, size, devid);
        return DRV_ERROR_BUSY;
    }

    ret = svm_register(va, size, flag, devid, &dst_va);
    if (ret != 0) {
        return (drvError_t)ret;
    }

    ret = svm_register_node_alloc_and_insert(rbtree, va, size, dst_va, flag);
    if (ret != DRV_ERROR_NONE) {
        (void)svm_unregister(va, flag, devid);
        return (drvError_t)ret;
    }

    if (dst_ptr != NULL) {
        *dst_ptr = (void *)(uintptr_t)dst_va;
    }

    svm_debug("HostRegister exit. (ret=%d; src_ptr=0x%llx; dst_ptr=0x%llx; size=%llu; flag=0x%x; devid=%u)\n", ret, va,
              dst_va, size, flag, devid);

    return (drvError_t)ret;
}

/* Perform the actual unregister operation based on flag. Returns 0 on success. */
static int svm_unregister(u64 va, u32 flag, u32 devid)
{
    u32 map_type = svm_get_register_map_type(flag);
    int ret;

    if (svm_is_register_host_pinned(flag) && (map_type != HOST_MEM_MAP_DEV_V2)) {
        return svm_dma_unregister(va, devid);
    }

    switch (map_type) {
        case HOST_MEM_MAP_DEV:
            /* todo add dev method check */
            ret = DRV_ERROR_NOT_SUPPORT;
            break;
        case HOST_SVM_MAP_DEV:
            /* todo add dev method check */
            ret = DRV_ERROR_NOT_SUPPORT;
            break;
        case DEV_SVM_MAP_HOST:
            ret = svm_unregister_to_peer(va, svm_get_host_devid());
            break;
        case HOST_MEM_MAP_DEV_V2:
            ret = svm_host_mem_unmap_dev_v2(va, devid);
            break;
        case HOST_IO_MAP_DEV:
            ret = svm_unregister_pcie_th(va, devid);
            break;
        default:
            ret = DRV_ERROR_NOT_SUPPORT;
            break;
    }

    if (ret != DRV_ERROR_NONE) {
        ret = (ret == DRV_ERROR_NO_PROCESS) ? ret : DRV_ERROR_NOT_EXIST;
    }

    return ret;
}

static int svm_unregister_para_check(void *src_ptr, u32 devid)
{
    if (src_ptr == NULL) {
        svm_err("srcPtr is null.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    if (devid >= SVM_MAX_DEV_NUM) {
        return DRV_ERROR_NOT_EXIST;
    }

    return DRV_ERROR_NONE;
}

drvError_t halHostUnregisterEx(void *src_ptr, UINT32 devid, UINT32 flag)
{
    SVM_UNUSED(flag);
    return halHostUnregister(src_ptr, devid);
}

drvError_t halHostUnregister(void *src_ptr, UINT32 devid)
{
    u64 va = (u64)(uintptr_t)src_ptr;
    struct register_rbtree *rbtree = NULL;
    struct register_node *reg_node = NULL;
    int ret;

    DRV_CURRENT_API_SCOPE();
    svm_debug("HostUnregister enter. (src_ptr=0x%llx; devid=%u)\n", va, devid);

    ret = svm_unregister_para_check(src_ptr, devid);
    if (ret != DRV_ERROR_NONE) {
        return (drvError_t)ret;
    }

    rbtree = svm_register_get_rbtree_index(va, devid);
    if (rbtree == NULL) {
        svm_report_addr_not_allocated("src_ptr", va);
        return DRV_ERROR_NOT_EXIST;
    }
    reg_node = svm_register_node_remove(rbtree, va, 1);
    if (reg_node == NULL) {
        svm_err("Unregister fail. (src_ptr=0x%llx; devid=%u)\n", va, devid);
        return DRV_ERROR_NOT_EXIST;
    }

    ret = svm_unregister(va, reg_node->flag, devid);
    svm_register_node_free(reg_node);
    svm_debug("HostUnregister exit. (src_ptr=0x%llx; devid=%u; ret=%d)\n", va, devid, ret);
    return ret;
}

/* Query device capabilities without depending on SVM device open */
static int svm_get_register_d2h_acc_mask(u32 devid, u64 *acc_mask)
{
    int64_t hd_connect_type;
    int ret;

    if (devid == svm_get_host_devid()) {
        *acc_mask = 0;
        return 0;
    }

    ret = halGetDeviceInfo(devid, MODULE_TYPE_SYSTEM, INFO_TYPE_HD_CONNECT_TYPE, &hd_connect_type);
    if (ret != DRV_ERROR_NONE) {
        svm_err("halGetDeviceInfo failed. (devid=%u; ret=%d)\n", devid, ret);
        return DRV_ERROR_INVALID_DEVICE;
    }

    switch (hd_connect_type) {
        case HOST_DEVICE_CONNECT_TYPE_PCIE:
            *acc_mask = (1ULL << DRV_ACC_MODULE_TYPE_STARS) | (1ULL << DRV_ACC_MODULE_TYPE_AIV);
            break;
        case HOST_DEVICE_CONNECT_TYPE_HCCS:
            *acc_mask = (1ULL << DRV_ACC_MODULE_TYPE_STARS) | (1ULL << DRV_ACC_MODULE_TYPE_AIC) |
                        (1ULL << DRV_ACC_MODULE_TYPE_AIV) | (1ULL << DRV_ACC_MODULE_TYPE_PCIEDMA) |
                        (1ULL << DRV_ACC_MODULE_TYPE_RDMA) | (1ULL << DRV_ACC_MODULE_TYPE_SDMA) |
                        (1ULL << DRV_ACC_MODULE_TYPE_DVPP);
            break;
        case HOST_DEVICE_CONNECT_TYPE_UB:
            *acc_mask = (1ULL << DRV_ACC_MODULE_TYPE_STARS) | (1ULL << DRV_ACC_MODULE_TYPE_AICPU) |
                        (1ULL << DRV_ACC_MODULE_TYPE_AIC) | (1ULL << DRV_ACC_MODULE_TYPE_AIV);
            break;
        default:
            *acc_mask = 0;
            break;
    }

    return DRV_ERROR_NONE;
}

drvError_t halHostRegisterCapabilities(uint32_t devid, uint32_t acc_module_type, uint32_t *mem_map_cap)
{
    u64 d2h_acc_mask = 0;
    int ret;

    if (devid >= SVM_MAX_DEV_NUM) {
        svm_err("Devid is invalid. (devid=%u)\n", devid);
        return DRV_ERROR_INVALID_DEVICE;
    }

    if (mem_map_cap == NULL) {
        svm_err("Mem_map_cap is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (acc_module_type >= DRV_ACC_MODULE_TYPE_MAX) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    ret = svm_get_register_d2h_acc_mask(devid, &d2h_acc_mask);
    if (ret != 0) {
        return (drvError_t)ret;
    }

    *mem_map_cap = ((d2h_acc_mask & (1ULL << acc_module_type)) != 0) ? HOST_MEM_MAP_SUPPORTED :
                                                                       HOST_MEM_MAP_NOT_SUPPORTED;

    svm_debug("Host register cap. (Devid=%u; acc_module_type=%u; d2h_acc_mask=0x%llx; mem_map_cap=%u)\n", devid,
              acc_module_type, d2h_acc_mask, *mem_map_cap);
    return DRV_ERROR_NONE;
}

drvError_t halMemHostGetDevPointer(void *src_ptr, uint32_t devid, void **dst_ptr)
{
    struct svm_global_va src_info;
    u64 va = (u64)(uintptr_t)src_ptr;
    u64 peer_va;
    int ret;

    if ((src_ptr == NULL) || (dst_ptr == NULL)) {
        return DRV_ERROR_PARA_ERROR;
    }

    if ((devid >= SVM_MAX_DEV_AGENT_NUM) || !svm_is_dev_open(devid)) {
        return DRV_ERROR_INVALID_DEVICE;
    }

    svm_global_va_pack(svm_get_host_devid(), (int)drvDeviceGetBareTgid(), va, 1, &src_info);
    ret = svm_register_lookup(&src_info, devid, &peer_va);
    if (ret != DRV_ERROR_NONE) {
        return DRV_ERROR_NOT_EXIST;
    }

    /* DMA (HOST_PINNED) has no device address, return NOT_EXIST */
    if (peer_va == 0) {
        return DRV_ERROR_NOT_EXIST;
    }

    /* peer_va is the device address for the registered start va.
     * Add offset from start va to the queried va. */
    *dst_ptr = (void *)(uintptr_t)(peer_va + (va - src_info.va));
    return DRV_ERROR_NONE;
}
