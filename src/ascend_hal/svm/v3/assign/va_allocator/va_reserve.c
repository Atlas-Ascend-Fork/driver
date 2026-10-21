/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "rbtree.h"
#include "bitmap.h"
#ifdef CFG_FEATURE_SUPPORT_UB
#include "comm_user_interface.h"
#endif
#include "svm_init_pri.h"
#include "svm_criu.h"
#include "svm_log.h"
#include "svm_sys_cmd.h"
#include "svm_dbi.h"
#include "svm_user_adapt.h"
#include "svm_umc_client.h"
#include "svm_sub_event_type_uk_msg.h"
#include "svm_apbi.h"
#include "va_reserve_uk_msg.h"
#include "va_mng.h"
#include "va_gap.h"
#include "va_reserve.h"

#define VA_RESERVE_HANDLE_MAX_NUM 4
#define VA_RELEASE_HANDLE_MAX_NUM 4

#define SVM_VA_RESERVE_GRAN (1 * SVM_BYTES_PER_GB)

#define SVM_VA_RESERVE_BIT_NUM (SVM_VA_RESERVE_SIZE / SVM_VA_RESERVE_GRAN)
#define SVM_VA_RESERVE_BITMAP_NUM (SVM_VA_RESERVE_BIT_NUM / (sizeof(bitmap_t) * 8)) /* 8: byte bit num */

struct va_reserve_node {
    struct rbtree_node node;
    u64 va;
    u64 size;
    u32 flag;
};

struct va_reserve_mng {
    pthread_rwlock_t lock;
    int dev_valid_flag[SVM_MAX_DEV_NUM];
    u64 start;
    u64 size;
    int bitnum;
    bitmap_t bitmap[SVM_VA_RESERVE_BITMAP_NUM];
    struct rbtree_root root;
};

static int (*va_reserve_post_handle[VA_RESERVE_HANDLE_MAX_NUM])(u64 va, u64 size) = {
    NULL,
};
static int (*va_release_pre_handle[VA_RELEASE_HANDLE_MAX_NUM])(u64 va, u64 size) = {
    NULL,
};

static struct va_reserve_mng g_va_reserve_mng;

static struct va_reserve_mng *va_reserve_get_mng(void)
{
    return &g_va_reserve_mng;
}

static bool va_reserve_agent_has_master(u32 flag)
{
    return ((flag & SVM_VA_RESERVE_FLAG_WITH_MASTER) != 0);
}

static bool va_reserve_has_custom_cp(u32 flag)
{
    return ((flag & SVM_VA_RESERVE_FLAG_WITH_CUSTOM_CP) != 0);
}

static bool va_reserve_has_hccp(u32 flag)
{
    return ((flag & SVM_VA_RESERVE_FLAG_WITH_HCCP) != 0);
}

static bool va_reserve_is_master_only(u32 flag)
{
    return ((flag & SVM_VA_RESERVE_FLAG_MASTER_ONLY) != 0);
}

static bool va_reserve_is_private(u32 flag)
{
    return ((flag & SVM_VA_RESERVE_FLAG_PRIVATE) != 0);
}

static bool va_reserve_has_master(u32 flag)
{
    return va_reserve_agent_has_master(flag) || va_reserve_is_master_only(flag);
}

static bool va_reserve_has_agent(u32 flag)
{
    return !va_reserve_is_master_only(flag);
}

int svm_register_va_reserve_post_handle(int (*fn)(u64 va, u64 size))
{
    int i;

    for (i = 0; i < VA_RESERVE_HANDLE_MAX_NUM; i++) {
        if (va_reserve_post_handle[i] == NULL) {
            va_reserve_post_handle[i] = fn;
            return DRV_ERROR_NONE;
        }
    }

    return DRV_ERROR_INNER_ERR;
}

int svm_register_va_release_pre_handle(int (*fn)(u64 va, u64 size))
{
    int i;

    for (i = 0; i < VA_RELEASE_HANDLE_MAX_NUM; i++) {
        if (va_release_pre_handle[i] == NULL) {
            va_release_pre_handle[i] = fn;
            return DRV_ERROR_NONE;
        }
    }

    return DRV_ERROR_INNER_ERR;
}

static int svm_call_va_reserve_post_handle(u64 va, u64 size)
{
    int ret, i;

    for (i = 0; i < VA_RESERVE_HANDLE_MAX_NUM; i++) {
        if (va_reserve_post_handle[i] != NULL) {
            ret = va_reserve_post_handle[i](va, size);
            if (ret != DRV_ERROR_NONE) {
                svm_err("Post failed. (i=%d; ret=%d; va=0x%llx; size=0x%llx)\n", i, ret, va, size);
                return ret;
            }
        }
    }

    return DRV_ERROR_NONE;
}

static int svm_call_va_release_pre_handle(u64 va, u64 size)
{
    int ret, i;

    for (i = VA_RELEASE_HANDLE_MAX_NUM - 1; i >= 0; i--) {
        if (va_release_pre_handle[i] != NULL) {
            ret = va_release_pre_handle[i](va, size);
            if (ret != DRV_ERROR_NONE) {
                svm_err("Pre failed. (i=%d; ret=%d; va=0x%llx; size=0x%llx)\n", i, ret, va, size);
                return ret;
            }
        }
    }

    return DRV_ERROR_NONE;
}

static int va_reserve_va_to_bit(struct va_reserve_mng *reserve_mng, u64 va)
{
    return (int)((va - reserve_mng->start) / SVM_VA_RESERVE_GRAN);
}

static u64 va_reserve_bit_to_va(struct va_reserve_mng *reserve_mng, int bit_start)
{
    return reserve_mng->start + ((u64)bit_start * SVM_VA_RESERVE_GRAN);
}

static u32 va_reserve_get_first_dev(struct va_reserve_mng *reserve_mng)
{
    u32 devid;

    for (devid = 0; devid < SVM_MAX_AGENT_NUM; devid++) {
        if (reserve_mng->dev_valid_flag[devid] != 0) {
            break;
        }
    }

    return devid;
}

bool va_reserve_has_dev(void)
{
    struct va_reserve_mng *reserve_mng = va_reserve_get_mng();

    return (va_reserve_get_first_dev(reserve_mng) < SVM_MAX_AGENT_NUM);
}

static void va_reserve_rb_range(struct rbtree_node *node, struct rb_range_handle *range)
{
    struct va_reserve_node *reserve_node = rb_entry(node, struct va_reserve_node, node);

    range->start = reserve_node->va;
    range->end = reserve_node->va + reserve_node->size - 1;
}

static struct va_reserve_node *va_reserve_search_node(struct va_reserve_mng *reserve_mng, u64 va, u64 size)
{
    struct rb_range_handle rb_range = {.start = va, .end = va + size - 1};
    struct rbtree_node *node = NULL;

    node = rbtree_search_by_range(&reserve_mng->root, &rb_range, va_reserve_rb_range);
    if (node == NULL) {
        return NULL;
    }

    return rb_entry(node, struct va_reserve_node, node);
}

static int va_reserve_insert_node(struct va_reserve_mng *reserve_mng, struct va_reserve_node *reserve_node)
{
    int ret = rbtree_insert_by_range(&reserve_mng->root, &reserve_node->node, va_reserve_rb_range);
    return (ret != 0) ? DRV_ERROR_REPEATED_USERD : 0;
}

static void va_release_remove_node(struct va_reserve_mng *reserve_mng, struct va_reserve_node *reserve_node)
{
    _rbtree_erase(&reserve_mng->root, &reserve_node->node);
}

int va_reserve_for_each_node(int (*func)(u64 va, u64 size, void *priv), void *priv)
{
    struct va_reserve_mng *reserve_mng = va_reserve_get_mng();
    struct rbtree_node *node = NULL;

    (void)pthread_rwlock_wrlock(&reserve_mng->lock);
    rbtree_node_for_each(node, &reserve_mng->root)
    {
        struct va_reserve_node *reserve_node = rb_entry(node, struct va_reserve_node, node);
        int ret;

        if ((va_reserve_is_private(reserve_node->flag)) || (va_reserve_is_master_only(reserve_node->flag))) {
            continue;
        }

        ret = func(reserve_node->va, reserve_node->size, priv);
        if (ret != 0) {
            (void)pthread_rwlock_unlock(&reserve_mng->lock);
            return ret;
        }
    }
    (void)pthread_rwlock_unlock(&reserve_mng->lock);

    return 0;
}

static int va_reserve_add_node(struct va_reserve_mng *reserve_mng, u64 va, u64 size, u32 flag)
{
    struct va_reserve_node *node = NULL;
    int ret;

    node = (struct va_reserve_node *)svm_ua_calloc(1, sizeof(*node));
    if (node == NULL) {
        svm_debug("Malloc node not success. (va=0x%llx)\n", va);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    node->va = va;
    node->size = size;
    node->flag = flag;

    ret = va_reserve_insert_node(reserve_mng, node);
    if (ret != 0) {
        svm_ua_free(node);
        svm_err("Add node failed. (va=0x%llx; size=0x%llx)\n", va, size);
        return ret;
    }

    return 0;
}

static void va_release_del_node(struct va_reserve_mng *reserve_mng, struct va_reserve_node *node)
{
    va_release_remove_node(reserve_mng, node);
    svm_ua_free(node);
}

static struct va_reserve_node *va_find_reserve_node(struct va_reserve_mng *reserve_mng, u64 va)
{
    struct va_reserve_node *node = NULL;

    node = va_reserve_search_node(reserve_mng, va, 1);
    if ((node != NULL) && (node->va == va)) {
        return node;
    }

    return NULL;
};

static int va_reserve_op_agent_task(u32 devid, u64 *va, u64 size, u32 flag, int task_type, int op)
{
    struct svm_umc_msg_head head;
    struct svm_va_reserve_msg reserve_msg = {
        .op = op, .flag = flag, .va = *va, .size = size, .gap_size = svm_va_get_gap()};
    struct svm_umc_msg msg = {.msg_in = (char *)(uintptr_t)&reserve_msg,
                              .msg_in_len = sizeof(struct svm_va_reserve_msg),
                              .msg_out = (char *)(uintptr_t)&reserve_msg,
                              .msg_out_len = sizeof(struct svm_va_reserve_msg)};
    struct svm_apbi apbi;
    int ret;

    ret = svm_apbi_query(devid, task_type, &apbi);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    svm_umc_msg_head_pack(devid, apbi.tgid, apbi.grp_id, SVM_VA_RESERVE_EVENT, &head);
    ret = svm_umc_h2d_send(&head, &msg);
    if (ret != DRV_ERROR_NONE) {
        if (ret == DRV_ERROR_NO_PROCESS) {
            svm_apbi_clear(devid, task_type);
        }

        return ret;
    }

    if (reserve_msg.status == VA_RESERVE_STATUS_OK) {
        return 0;
    } else if (reserve_msg.status == VA_RESERVE_STATUS_FAIL_WITH_SUGGEST) {
        *va = reserve_msg.va;
        return DRV_ERROR_REPEATED_USERD;
    } else {
        svm_debug("remote map not success. (devid=%u; status=%d)\n", devid, reserve_msg.status);
        return DRV_ERROR_OUT_OF_MEMORY;
    }
}

static int va_reserve_agent_client(u32 devid, u64 *va, u64 size, int flag)
{
    u32 mmap_flag = va_reserve_is_private((u32)flag) ? SVM_MMAP_FLAG_PRIVATE : 0;
    int ret;

    ret = va_reserve_op_agent_task(devid, va, size, mmap_flag, DEVDRV_PROCESS_CP1, 1);
    if (ret != 0) {
        return ret;
    }

    if (va_reserve_has_custom_cp((u32)flag)) {
        (void)va_reserve_op_agent_task(devid, va, size, mmap_flag, DEVDRV_PROCESS_CP2, 1);
    }

    if (va_reserve_has_hccp((u32)flag)) {
        (void)va_reserve_op_agent_task(devid, va, size, mmap_flag, DEVDRV_PROCESS_HCCP, 1);
    }

    return 0;
}

static void va_release_agent_client(u32 devid, u64 va, u64 size, u32 flag)
{
    u32 mmap_flag = va_reserve_is_private(flag) ? SVM_MMAP_FLAG_PRIVATE : 0;
    u64 release_va = va;

    if (va_reserve_has_hccp(flag)) {
        (void)va_reserve_op_agent_task(devid, &release_va, size, mmap_flag, DEVDRV_PROCESS_HCCP, 0);
    }

    if (va_reserve_has_custom_cp(flag)) {
        (void)va_reserve_op_agent_task(devid, &release_va, size, mmap_flag, DEVDRV_PROCESS_CP2, 0);
    }

    (void)va_reserve_op_agent_task(devid, &release_va, size, mmap_flag, DEVDRV_PROCESS_CP1, 0);
}

static void va_release_agents(struct va_reserve_mng *reserve_mng, u64 va, u64 size, u32 flag, u32 min_devid,
                              u32 max_devid)
{
    u32 devid;

    for (devid = min_devid; devid <= max_devid; devid++) {
        if (reserve_mng->dev_valid_flag[devid] == 0) {
            continue;
        }
        va_release_agent_client(devid, va, size, flag);
    }
}

static int va_reserve_agents(struct va_reserve_mng *reserve_mng, u64 va, u64 size, u32 flag, u32 min_devid,
                             u32 max_devid)
{
    u32 devid;
    int ret;

    for (devid = min_devid; devid <= max_devid; devid++) {
        if (reserve_mng->dev_valid_flag[devid] == 0) {
            continue;
        }

        ret = va_reserve_agent_client(devid, &va, size, (int)flag);
        if (ret == DRV_ERROR_NO_PROCESS) {
            svm_warn("Process exited. (devid=%u)\n", devid);
            reserve_mng->dev_valid_flag[devid] = 0;
            continue;
        }
        if (ret != 0) {
            if (devid > 0) {
                va_release_agents(reserve_mng, va, size, flag, min_devid, devid - 1);
            }
            return ret;
        }
    }

    return DRV_ERROR_NONE;
}

static int va_reserve_master(u64 va, u64 size)
{
    void *mem_mapped_addr = NULL;

    mem_mapped_addr = svm_cmd_mmap((void *)(uintptr_t)va, size, PROT_READ | PROT_WRITE, MAP_SHARED, 0);
    if (mem_mapped_addr == MAP_FAILED) {
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    if ((u64)(uintptr_t)mem_mapped_addr != va) {
        (void)svm_cmd_munmap(mem_mapped_addr, size);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    if (svm_ua_madvise(mem_mapped_addr, size, MADV_DONTDUMP) != 0) {
        svm_err("Madvise failed. (start=0x%llx; size=0x%llx)\n", va, size);
        (void)svm_cmd_munmap(mem_mapped_addr, size);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    svm_debug("Master mmap. (start=0x%llx; size=0x%llx)\n", va, size);

    return DRV_ERROR_NONE;
}

static void va_release_master(u64 va, u64 size)
{
    svm_debug("Master munmap. (start=0x%llx; size=0x%llx)\n", va, size);
    (void)svm_cmd_munmap((void *)(uintptr_t)va, size);
}

static int va_reserve_fixed_va(struct va_reserve_mng *reserve_mng, u64 va, u64 size, u32 flag)
{
    u32 bit_num = (u32)(size / SVM_VA_RESERVE_GRAN);
    int fix_bit_start, index;
    int ret;

    fix_bit_start = va_reserve_va_to_bit(reserve_mng, va);
    index = (int)bitmap_find_next_zero_area(reserve_mng->bitmap, (unsigned long)reserve_mng->bitnum,
                                            (unsigned long)fix_bit_start, bit_num, 0);
    if (index != fix_bit_start) {
        svm_debug("Fix va has been reserved. (va=0x%llx; size=0x%llx)\n", va, size);
        return DRV_ERROR_BUSY;
    }

    ret = va_reserve_agents(reserve_mng, va, size, flag, 0, SVM_MAX_AGENT_NUM - 1);
    if (ret != 0) {
        return ret;
    }

    if (va_reserve_agent_has_master(flag)) {
        ret = va_reserve_master(va, size);
        if (ret != 0) {
            va_release_agents(reserve_mng, va, size, flag, 0, SVM_MAX_AGENT_NUM - 1);
            return ret;
        }
    }

    return 0;
}

static int _va_negotiate_master_and_agent(struct va_reserve_mng *reserve_mng, u32 negotiate_devid, u64 size, u32 flag,
                                          int step, u64 *negotiate_va)
{
    int bit_num = (int)(size / SVM_VA_RESERVE_GRAN);
    int cursor = 0;

    /* reserve from head */
    while ((cursor + bit_num) <= reserve_mng->bitnum) {
        int negotiate_bit_start = (int)bitmap_find_next_zero_area(
            reserve_mng->bitmap, (unsigned long)reserve_mng->bitnum, (unsigned long)cursor, (u32)bit_num, 0);
        u64 tmp_va, suggest_va;
        int ret;

        if (negotiate_bit_start >= reserve_mng->bitnum) {
            break;
        }

        tmp_va = va_reserve_bit_to_va(reserve_mng, negotiate_bit_start);
        suggest_va = tmp_va;
        ret = va_reserve_agent_client(negotiate_devid, &suggest_va, size, (int)flag);
        /* Device link may be abnormal, not need to negotiate other va. */
        if ((ret != 0) && (ret != DRV_ERROR_OUT_OF_MEMORY) && (ret != DRV_ERROR_REPEATED_USERD)) {
            return ret;
        } else if (ret == 0) {
            ret = va_reserve_master(tmp_va, size);
            if (ret == 0) {
                *negotiate_va = tmp_va;
                return 0;
            }

            va_release_agent_client(negotiate_devid, tmp_va, size, flag);
        }

        svm_debug("Negotiate continue. (negotiate_va=0x%llx; size=0x%llx; suggest_va=0x%llx; ret=%d)\n", tmp_va, size,
                  suggest_va, ret);

        cursor = negotiate_bit_start + step;
    }

    return DRV_ERROR_OUT_OF_MEMORY;
}

static int va_negotiate_master_and_agent(struct va_reserve_mng *reserve_mng, u32 negotiate_devid, u64 size, u32 flag,
                                         u64 *negotiate_va)
{
    int step;
    int ret;

    /* first, use TB to fast negotiate */
    step = SVM_BYTES_PER_TB / SVM_VA_RESERVE_GRAN;
    ret = _va_negotiate_master_and_agent(reserve_mng, negotiate_devid, size, flag, step, negotiate_va);
    if (ret == DRV_ERROR_OUT_OF_MEMORY) {
        /* second, use GB to slowly negotiate */
        step = SVM_BYTES_PER_GB / SVM_VA_RESERVE_GRAN;
        ret = _va_negotiate_master_and_agent(reserve_mng, negotiate_devid, size, flag, step, negotiate_va);
    }
    return ret;
}

static int _va_negotiate_agent(struct va_reserve_mng *reserve_mng, u32 negotiate_devid, u64 size, u32 flag, int step,
                               u64 *negotiate_va)
{
    u32 bit_num = (u32)(size / SVM_VA_RESERVE_GRAN);
    int cursor = reserve_mng->bitnum - (int)bit_num;

    /* reserve from tail:
       x86 has only 128T virtual address space, arm has 256T. when we reserved for devices(arm),
       address space outside of 128T is priority reserved */
    while (cursor >= 0) {
        u64 tmp_va, suggest_va;
        int negotiate_bit_start, ret;

        negotiate_bit_start = (int)bitmap_find_next_zero_area(reserve_mng->bitmap, (unsigned long)reserve_mng->bitnum,
                                                              (unsigned long)cursor, bit_num, 0);
        /* must same with start cursor, not try again with same negotiate_bit_start */
        if (negotiate_bit_start != cursor) {
            cursor -= step;
            continue;
        }

        tmp_va = va_reserve_bit_to_va(reserve_mng, negotiate_bit_start);
        suggest_va = tmp_va;
        ret = va_reserve_agent_client(negotiate_devid, &suggest_va, size, (int)flag);
        if (ret == 0) {
            *negotiate_va = tmp_va;
            return 0;
        }

        /* Device link may be abnormal, not need to negotiate other va. */
        if ((ret != DRV_ERROR_OUT_OF_MEMORY) && (ret != DRV_ERROR_REPEATED_USERD)) {
            return ret;
        }

        svm_debug("Negotiate continue. (negotiate_va=0x%llx; size=0x%llx; suggest_va=0x%llx; ret=%d)\n", tmp_va, size,
                  suggest_va, ret);

        if (suggest_va != tmp_va) {
            int suggest_cursor = va_reserve_va_to_bit(reserve_mng, suggest_va);
            if (suggest_cursor < negotiate_bit_start) {
                cursor = suggest_cursor;
                continue;
            }
        }

        cursor -= step;
    }

    return DRV_ERROR_OUT_OF_MEMORY;
}

static int va_negotiate_agent(struct va_reserve_mng *reserve_mng, u32 negotiate_devid, u64 size, u32 flag,
                              u64 *negotiate_va)
{
    int step;
    int ret;

    /* first, use TB to fast negotiate */
    step = SVM_BYTES_PER_TB / SVM_VA_RESERVE_GRAN;
    ret = _va_negotiate_agent(reserve_mng, negotiate_devid, size, flag, step, negotiate_va);
    if (ret == DRV_ERROR_OUT_OF_MEMORY) {
        /* second, use GB to slowly negotiate */
        step = SVM_BYTES_PER_GB / SVM_VA_RESERVE_GRAN;
        ret = _va_negotiate_agent(reserve_mng, negotiate_devid, size, flag, step, negotiate_va);
    }
    return ret;
}

static int va_reserve_negotiate(struct va_reserve_mng *reserve_mng, u32 negotiate_devid, u64 size, u32 flag,
                                u64 *negotiate_va)
{
    if (va_reserve_agent_has_master(flag)) {
        return va_negotiate_master_and_agent(reserve_mng, negotiate_devid, size, flag, negotiate_va);
    } else {
        return va_negotiate_agent(reserve_mng, negotiate_devid, size, flag, negotiate_va);
    }
}

static int va_reserve_get_negotiate_va(struct va_reserve_mng *reserve_mng, u64 size, u32 flag, u32 *negotiate_devid,
                                       u64 *negotiate_va)
{
    u32 devid;
    int ret;

    for (devid = 0; devid < SVM_MAX_AGENT_NUM; devid++) {
        if (reserve_mng->dev_valid_flag[devid] == 0) {
            continue;
        }

        ret = va_reserve_negotiate(reserve_mng, devid, size, flag, negotiate_va);
        if (ret == DRV_ERROR_NO_PROCESS) {
            svm_warn("Process exited. (devid=%u)\n", devid);
            reserve_mng->dev_valid_flag[devid] = 0;
            continue;
        }
        if (ret != 0) {
            return ret;
        }

        *negotiate_devid = devid;
        return 0;
    }

    return DRV_ERROR_OUT_OF_MEMORY;
}

static int va_reserve_non_fixed_va(struct va_reserve_mng *reserve_mng, u64 *va, u64 size, u32 flag)
{
    u64 negotiate_va = 0;
    u32 negotiate_devid;
    int ret;

    ret = va_reserve_get_negotiate_va(reserve_mng, size, flag, &negotiate_devid, &negotiate_va);
    if (ret != 0) {
        return ret;
    }

    ret = va_reserve_agents(reserve_mng, negotiate_va, size, flag, negotiate_devid + 1, SVM_MAX_AGENT_NUM - 1);
    if (ret != 0) {
        if (va_reserve_agent_has_master(flag)) {
            va_release_master(negotiate_va, size);
        }
        va_release_agent_client(negotiate_devid, negotiate_va, size, flag);
        return ret;
    }

    *va = negotiate_va;
    return 0;
}

static int va_reserve(struct va_reserve_mng *reserve_mng, u64 *va, u64 size, u32 flag)
{
    int ret;

    if (*va != 0) {
        ret = va_reserve_fixed_va(reserve_mng, *va, size, flag);
    } else {
        ret = va_reserve_non_fixed_va(reserve_mng, va, size, flag);
    }

    if (ret == 0) {
        bitmap_set(reserve_mng->bitmap, va_reserve_va_to_bit(reserve_mng, *va), (int)(size / SVM_VA_RESERVE_GRAN));
    }

    return ret;
}

static void va_release(struct va_reserve_mng *reserve_mng, u64 va, u64 size, u32 flag)
{
    bitmap_clear(reserve_mng->bitmap, va_reserve_va_to_bit(reserve_mng, va), (int)(size / SVM_VA_RESERVE_GRAN));

    if (va_reserve_agent_has_master(flag)) {
        va_release_master(va, size);
    }
    va_release_agents(reserve_mng, va, size, flag, 0, SVM_MAX_AGENT_NUM - 1);
}

static int _svm_reserve_va(struct va_reserve_mng *reserve_mng, u64 *va, u64 size, u32 flag)
{
    int ret;

    ret = va_reserve(reserve_mng, va, size, flag);
    if (ret != 0) {
        return ret;
    }

    ret = va_reserve_add_node(reserve_mng, *va, size, flag);
    if (ret != 0) {
        va_release(reserve_mng, *va, size, flag);
        return ret;
    }

    if (!va_reserve_is_private(flag)) {
        ret = svm_call_va_reserve_post_handle(*va, size);
        if (ret != 0) {
            va_release_del_node(reserve_mng, va_find_reserve_node(reserve_mng, *va));
            va_release(reserve_mng, *va, size, flag);
            return ret;
        }
    }

    return 0;
}

static int _svm_release_va(struct va_reserve_mng *reserve_mng, u64 start)
{
    struct va_reserve_node *node = NULL;
    u64 va, size;
    u32 flag;

    node = va_find_reserve_node(reserve_mng, start);
    if (node == 0) {
        svm_err("Invalid para. (start=0x%llx)\n", start);
        return DRV_ERROR_INVALID_VALUE;
    }

    va = node->va;
    size = node->size;
    flag = node->flag;

    if (!va_reserve_is_private(flag)) {
        (void)svm_call_va_release_pre_handle(va, size);
    }
    va_release_del_node(reserve_mng, node);
    va_release(reserve_mng, va, size, flag);

    return DRV_ERROR_NONE;
}

int svm_reserve_va(u64 va, u64 size, u32 flag, u64 *start)
{
    struct va_reserve_mng *reserve_mng = va_reserve_get_mng();
    u64 alloc_va = va;
    int ret;

    if ((!SVM_IS_ALIGNED(size, SVM_VA_RESERVE_ALIGN)) || (size > SVM_VA_RESERVE_MAX_SIZE)) {
        svm_err("Invalid size. (size=0x%llx)\n", size);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((va != 0) && (!SVM_IS_ALIGNED(va, SVM_VA_RESERVE_ALIGN))) {
        svm_err("Va is not align. (va=0x%llx)\n", va);
        return DRV_ERROR_INVALID_VALUE;
    }

    (void)pthread_rwlock_wrlock(&reserve_mng->lock);
    ret = _svm_reserve_va(reserve_mng, &alloc_va, size, flag);
    (void)pthread_rwlock_unlock(&reserve_mng->lock);

    if (ret == 0) {
        *start = alloc_va;
        svm_debug("Reserve va success. (va=0x%llx; size=0x%llx; flag=0x%x)\n", alloc_va, size, flag);
    }

    return ret;
}

int svm_release_va(u64 start)
{
    struct va_reserve_mng *reserve_mng = va_reserve_get_mng();
    int ret;

    (void)pthread_rwlock_wrlock(&reserve_mng->lock);
    ret = _svm_release_va(reserve_mng, start);
    (void)pthread_rwlock_unlock(&reserve_mng->lock);

    if (ret == 0) {
        svm_debug("Release va success. (va=0x%llx)\n", start);
    }

    return ret;
}

static int _svm_reserve_master_only_va(struct va_reserve_mng *reserve_mng, u64 va, u64 size)
{
    u32 flag;
    int ret;

    ret = va_reserve_master(va, size);
    if (ret != 0) {
        return ret;
    }

    flag = SVM_VA_RESERVE_FLAG_MASTER_ONLY;
    ret = va_reserve_add_node(reserve_mng, va, size, flag);
    if (ret != 0) {
        va_release_master(va, size);
        return ret;
    }

    return DRV_ERROR_NONE;
}

static int _svm_release_master_only_va(struct va_reserve_mng *reserve_mng, u64 start)
{
    struct va_reserve_node *node = NULL;
    u64 va, size;

    node = va_find_reserve_node(reserve_mng, start);
    if (node == 0) {
        svm_err("Invalid para. (va=0x%llx)\n", start);
        return DRV_ERROR_INVALID_VALUE;
    }

    va = node->va;
    size = node->size;

    va_release_del_node(reserve_mng, node);
    va_release_master(va, size);

    return DRV_ERROR_NONE;
}

int svm_reserve_master_only_va(u64 va, u64 size)
{
    struct va_reserve_mng *reserve_mng = va_reserve_get_mng();
    int ret;

    (void)pthread_rwlock_wrlock(&reserve_mng->lock);
    ret = _svm_reserve_master_only_va(reserve_mng, va, size);
    (void)pthread_rwlock_unlock(&reserve_mng->lock);

    return ret;
}

int svm_release_master_only_va(u64 start)
{
    struct va_reserve_mng *reserve_mng = va_reserve_get_mng();
    int ret;

    (void)pthread_rwlock_wrlock(&reserve_mng->lock);
    ret = _svm_release_master_only_va(reserve_mng, start);
    (void)pthread_rwlock_unlock(&reserve_mng->lock);

    return ret;
}

static int _svm_check_reserve_range(struct va_reserve_mng *reserve_mng, u64 va, u64 size, bool *is_reserved)
{
    struct rbtree_node *node = reserve_mng->root.rbtree_node;
    u64 check_start = va;
    u64 check_end = va + size - 1;

    while (node != NULL) {
        struct va_reserve_node *reserve_node = rb_entry(node, struct va_reserve_node, node);
        u64 start = reserve_node->va;
        u64 end = reserve_node->va + reserve_node->size - 1;

        if (check_end < start) {
            node = node->rbtree_left;
        } else if (check_start > end) {
            node = node->rbtree_right;
        } else if (check_start >= start && check_end <= end) {
            *is_reserved = true;
            return 0;
        } else {
            return DRV_ERROR_INVALID_VALUE;
        }
    }

    *is_reserved = false;
    return 0;
}

/* is range is mixed, will return DRV_ERROR_INVALID_VALUE */
int svm_check_reserve_range(u64 va, u64 size, bool *is_reserved)
{
    struct va_reserve_mng *reserve_mng = va_reserve_get_mng();
    u64 range_start, range_size, max_va;
    int ret;

    svm_get_va_range(&range_start, &range_size);
    max_va = range_start + range_size;

    if ((size > range_size) || (size == 0)) {
        return DRV_ERROR_INVALID_VALUE;
    }

    if (va >= max_va) {
        *is_reserved = false;
        return DRV_ERROR_NONE;
    }

    (void)pthread_rwlock_rdlock(&reserve_mng->lock);
    ret = _svm_check_reserve_range(reserve_mng, va, size, is_reserved);
    (void)pthread_rwlock_unlock(&reserve_mng->lock);

    return ret;
}

#ifdef CFG_FEATURE_SUPPORT_UB
static int svm_get_reserve_range(uint64_t va, uint64_t *range_start, uint64_t *range_size)
{
    struct va_reserve_mng *reserve_mng = va_reserve_get_mng();
    struct va_reserve_node *node = NULL;

    if ((range_start == NULL) || (range_size == NULL)) {
        return DRV_ERROR_INVALID_VALUE;
    }

    (void)pthread_rwlock_rdlock(&reserve_mng->lock);
    node = va_reserve_search_node(reserve_mng, va, 1);
    (void)pthread_rwlock_unlock(&reserve_mng->lock);
    if (node == NULL) {
        return DRV_ERROR_INVALID_VALUE;
    }

    *range_start = node->va;
    *range_size = node->size;
    return DRV_ERROR_NONE;
}

void *svm_acquire_token(void *seg_mng, uint64_t start, uint64_t size)
{
    uint64_t reserve_start = 0;
    uint64_t reserve_size = 0;
    struct ascend_urma_seg_info_ex info;

    (void)size;
    if (svm_get_reserve_range(start, &reserve_start, &reserve_size) != 0) {
        return NULL;
    }

    /* Check if the previous segment is within the reserve range. */
    if (ascend_urma_seg_get_prev(seg_mng, start, &info) == 0) {
        if ((info.start >= reserve_start) && (info.start < reserve_start + reserve_size)) {
            return (void *)info.token;
        }
    }

    /* Check if the next segment is within the reserve range. */
    if (ascend_urma_seg_get_next(seg_mng, start, &info) == 0) {
        if ((info.start >= reserve_start) && (info.start < reserve_start + reserve_size)) {
            return (void *)info.token;
        }
    }

    return NULL;
}
#endif

static int va_reserve_dev_single_range(u32 devid, u64 va, u64 size, u32 flag)
{
    int ret = 0;

    if (devid == svm_get_host_devid()) {
        if (va_reserve_has_master(flag)) {
            ret = va_reserve_master(va, size);
        }
    } else {
        if (va_reserve_has_agent(flag)) {
            ret = va_reserve_agent_client(devid, &va, size, (int)flag);
        }
    }

    return ret;
}

static void va_release_dev_single_range(u32 devid, u64 va, u64 size, u32 flag)
{
    if (devid == svm_get_host_devid()) {
        if (va_reserve_has_master(flag)) {
            va_release_master(va, size);
        }
    } else {
        if (va_reserve_has_agent(flag)) {
            va_release_agent_client(devid, va, size, flag);
        }
    }
}

static int _va_reserve_add_dev(u32 devid)
{
    struct va_reserve_mng *reserve_mng = va_reserve_get_mng();
    struct rbtree_node *node = NULL;
    u64 va = 0;
    int ret = 0;

    rbtree_node_for_each(node, &reserve_mng->root)
    {
        struct va_reserve_node *reserve_node = rb_entry(node, struct va_reserve_node, node);

        ret = va_reserve_dev_single_range(devid, reserve_node->va, reserve_node->size, reserve_node->flag);
        if (ret != 0) {
            va = reserve_node->va;
            svm_err("Add dev reserve va failed. (devid=%u; va=0x%llx; size=0x%llx)\n", devid, va, reserve_node->size);
            break;
        }
    }

    if (ret != 0) {
        rbtree_node_for_each(node, &reserve_mng->root)
        {
            struct va_reserve_node *reserve_node = rb_entry(node, struct va_reserve_node, node);
            if (va == reserve_node->va) {
                break;
            }

            va_release_dev_single_range(devid, reserve_node->va, reserve_node->size, reserve_node->flag);
        }
    }

    return ret;
}

static void _va_reserve_del_dev(u32 devid)
{
    struct va_reserve_mng *reserve_mng = va_reserve_get_mng();
    struct rbtree_node *node = NULL;

    rbtree_node_for_each(node, &reserve_mng->root)
    {
        struct va_reserve_node *reserve_node = rb_entry(node, struct va_reserve_node, node);
        va_release_dev_single_range(devid, reserve_node->va, reserve_node->size, reserve_node->flag);
    }
}

int va_reserve_add_dev(u32 devid)
{
    struct va_reserve_mng *reserve_mng = va_reserve_get_mng();
    int ret = 0;

    (void)pthread_rwlock_wrlock(&reserve_mng->lock);
    if (reserve_mng->dev_valid_flag[devid] == 0) {
        /* Before open dev, map have already vmm alloc va. */
        if ((devid == svm_get_host_devid()) && !svm_criu_is_device_reopen(devid)) {
            reserve_mng->dev_valid_flag[devid] = 1;
        } else {
            ret = _va_reserve_add_dev(devid);
            if (ret == 0) {
                reserve_mng->dev_valid_flag[devid] = 1;
            }
        }
    }
    (void)pthread_rwlock_unlock(&reserve_mng->lock);

    return ret;
}

void va_reserve_del_dev(u32 devid)
{
    struct va_reserve_mng *reserve_mng = va_reserve_get_mng();

    (void)pthread_rwlock_wrlock(&reserve_mng->lock);
    /* agent process has been exit, not need to unmap, do it for emu st */
    if (reserve_mng->dev_valid_flag[devid] == 1) {
        _va_reserve_del_dev(devid);
        reserve_mng->dev_valid_flag[devid] = 0;
    }
    (void)pthread_rwlock_unlock(&reserve_mng->lock);
}

static int _va_reserve_add_task(u32 devid, int task_type)
{
    struct va_reserve_mng *reserve_mng = va_reserve_get_mng();
    struct rbtree_node *node = NULL;

    if (reserve_mng->dev_valid_flag[devid] == 0) {
        return DRV_ERROR_INVALID_DEVICE;
    }

    rbtree_node_for_each(node, &reserve_mng->root)
    {
        struct va_reserve_node *reserve_node = rb_entry(node, struct va_reserve_node, node);

        if (((task_type == DEVDRV_PROCESS_CP2) && va_reserve_has_custom_cp(reserve_node->flag)) ||
            ((task_type == DEVDRV_PROCESS_HCCP) && va_reserve_has_hccp(reserve_node->flag))) {
            u64 va = reserve_node->va;
            u32 mmap_flag = va_reserve_is_private(reserve_node->flag) ? SVM_MMAP_FLAG_PRIVATE : 0;
            int ret = va_reserve_op_agent_task(devid, &va, reserve_node->size, mmap_flag, task_type, 1);
            if (ret != 0) {
                svm_err("Add dev reserve va failed. (devid=%u; va=0x%llx; size=0x%llx; task_type=%d)\n", devid, va,
                        reserve_node->size, task_type);
                return ret;
            }
        }
    }

    return 0;
}

int svm_va_reserve_add_task(u32 devid, int task_type)
{
    struct va_reserve_mng *reserve_mng = va_reserve_get_mng();
    int ret;

    (void)pthread_rwlock_wrlock(&reserve_mng->lock);
    ret = _va_reserve_add_task(devid, task_type);
    (void)pthread_rwlock_unlock(&reserve_mng->lock);

    return ret;
}

static void __attribute__((constructor)) va_reserve_init(void)
{
    struct va_reserve_mng *reserve_mng = va_reserve_get_mng();

    svm_get_va_range(&reserve_mng->start, &reserve_mng->size);
    reserve_mng->bitnum = SVM_VA_RESERVE_BIT_NUM;
    bitmap_clear((unsigned long *)reserve_mng->bitmap, 0, reserve_mng->bitnum);
    (void)pthread_rwlock_init(&reserve_mng->lock, NULL);
    rbtree_init(&reserve_mng->root);
}
