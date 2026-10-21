/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
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
#include "ka_task_pub.h"
#include "ka_list_pub.h"
#include "ka_memory_pub.h"
#include "ka_common_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_hashtable_pub.h"
#include "ka_system_pub.h"

#include "securec.h"
#include "pbl_kref_safe.h"

#include "trs_msg.h"
#include "trs_id.h"

#define MAX_TRS_ID_BATCH_NUM 64U
static ka_mutex_t g_trs_id_mutex;

static ka_kmem_cache_t *trs_id_cache = NULL;

struct trs_id_rsv_node {
    u32 id;
    ka_pid_t pid;
    ka_list_head_t list; // rsv list node
};

#define TRS_ISOLATE_NUM_MAX 16
struct trs_id_node {
    u32 offset[TRS_ISOLATE_NUM_MAX];
    u32 id_start;
    u32 id_avail_num;
    u32 id_total_num;
    ka_pid_t pid;
    u32 is_in_idle; /* 0:default; 1: in idle list and idle hash table */
    ka_hlist_node_t link;
    ka_list_head_t list;
};

#define ID_POOL_DFX_MAX_TIMES 3
#define TRS_ID_HASH_TABLE_BIT 10
struct trs_id_pool {
    struct trs_id_inst inst;
    int type;

    struct trs_id_attr attr;
    u32 allocatable_num;      // Id numbers listed to free list (Allocatable id number)
    u32 alloc_num;            // Id numbers that already allocated
    u32 rsv_num;              // Id numbers that free with rsv flag
    ka_list_head_t idle_head; // the node is same as idle_htable to get one node fast
    KA_DECLARE_HASHTABLE(idle_htable, TRS_ID_HASH_TABLE_BIT);
    KA_DECLARE_HASHTABLE(allocated_htable, TRS_ID_HASH_TABLE_BIT);
    ka_list_head_t rsv_head; // rsv free id list head
    struct trs_id_ops ops;
    struct kref_safe ref;
    ka_mutex_t mutex;
    u32 dfx_times;
};

static struct trs_id_pool *g_trs_id_pool[TRS_TS_INST_MAX_NUM][TRS_ID_TYPE_MAX];

size_t trs_id_get_node_size(void);
size_t trs_id_get_pool_size(void);
int init_trs_id(void);
void exit_trs_id(void);

static u32 trs_id_make_hash_key(u32 isolate_num, u32 id)
{
    return (id / isolate_num * isolate_num);
}

size_t trs_id_get_node_size(void)
{
    return sizeof(struct trs_id_node);
}

size_t trs_id_get_pool_size(void)
{
    return sizeof(struct trs_id_pool);
}

static struct trs_id_node *trs_id_node_create(void)
{
    return (struct trs_id_node *)ka_mm_kmem_cache_alloc(trs_id_cache, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
}

static void trs_id_node_destroy(struct trs_id_node *node)
{
    if (node != NULL) {
        ka_mm_kmem_cache_free(trs_id_cache, node);
    }
}

static struct trs_id_rsv_node *trs_id_rsv_node_create(void)
{
    return (struct trs_id_rsv_node *)ka_mm_kmem_cache_alloc(trs_id_cache, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
}

static void trs_id_rsv_node_destroy(struct trs_id_rsv_node *node)
{
    if (node != NULL) {
        ka_mm_kmem_cache_free(trs_id_cache, node);
    }
}

static void trs_id_pool_del_rsv_node(struct trs_id_rsv_node *node)
{
    ka_list_del(&node->list);
    trs_id_rsv_node_destroy(node);
}

static struct trs_id_node *trs_id_pool_get_idle_node_by_id(struct trs_id_pool *id_pool, u32 id)
{
    struct trs_id_node *node = NULL;
    u32 key = trs_id_make_hash_key(id_pool->attr.isolate_num, id);

    ka_hash_for_each_possible(id_pool->idle_htable, node, link, key)
    {
        if ((id >= node->id_start) && (id < (node->id_start + node->id_total_num))) {
            return node;
        }
    }
    return NULL;
}

static struct trs_id_node *trs_id_pool_get_allocated_node_by_id(struct trs_id_pool *id_pool, u32 id)
{
    struct trs_id_node *node = NULL;
    u32 key = trs_id_make_hash_key(id_pool->attr.isolate_num, id);

    ka_hash_for_each_possible(id_pool->allocated_htable, node, link, key)
    {
        if ((id >= node->id_start) && (id < (node->id_start + node->id_total_num))) {
            return node;
        }
    }
    return NULL;
}

static int trs_id_pool_add_rsv_node(struct trs_id_pool *id_pool, u32 id)
{
    struct trs_id_rsv_node *node = trs_id_rsv_node_create();
    if (node == NULL) {
        trs_err("Alloc id node fail. (devid=%u; tsid=%u; type=%s; id=%u)\n", id_pool->inst.devid, id_pool->inst.tsid,
                trs_id_type_to_name(id_pool->type), id);
        return -ENOMEM;
    }

    node->id = id;
    node->pid = ka_task_get_current_tgid();
    id_pool->rsv_num++;
    ka_list_add(&node->list, &id_pool->rsv_head);
    id_pool->allocatable_num++;
    trs_debug("Add rsv node. (devid=%u; type=%s; allocatable_num=%u; id=%u; pid=%d)\n", id_pool->inst.devid,
              trs_id_type_to_name(id_pool->type), id_pool->allocatable_num, id, node->pid);
    return 0;
}

static int trs_id_pool_add_new_node(struct trs_id_pool *id_pool, u32 flag, u32 id)
{
    struct trs_id_node *node = trs_id_node_create();
    if (node == NULL) {
        trs_err("Alloc id node fail. (devid=%u; tsid=%u; type=%s; id=%u)\n", id_pool->inst.devid, id_pool->inst.tsid,
                trs_id_type_to_name(id_pool->type), id);
        return -ENOMEM;
    }

    node->id_start = trs_id_make_hash_key(id_pool->attr.isolate_num, id);
    node->id_total_num = id_pool->attr.isolate_num;
    node->pid = 0;
    node->is_in_idle = 1;
    node->id_avail_num = 1;
    (void)memset_s(node->offset, sizeof(u32) * TRS_ISOLATE_NUM_MAX, 0, sizeof(u32) * TRS_ISOLATE_NUM_MAX);
    id_pool->allocatable_num++;

    ka_hash_add(id_pool->idle_htable, &node->link, node->id_start);
    if (id < id_pool->attr.split) {
        ka_list_add(&node->list, &id_pool->idle_head);
    } else {
        ka_list_add_tail(&node->list, &id_pool->idle_head);
    }

    trs_debug("Add node. (devid=%u; type=%s; node_start=%u; node_total_num=%u; allocatable_num=%u; id=%u)\n",
              id_pool->inst.devid, trs_id_type_to_name(id_pool->type), node->id_start, node->id_total_num,
              id_pool->allocatable_num, id);
    return 0;
}

static int trs_id_pool_add_node(struct trs_id_pool *id_pool, u32 flag, u32 id)
{
    struct trs_id_node *node = trs_id_pool_get_allocated_node_by_id(id_pool, id);
    if (node == NULL) {
        node = trs_id_pool_get_idle_node_by_id(id_pool, id);
    }
    if (node != NULL) {
        node->id_avail_num++;
        id_pool->allocatable_num++;
        return 0;
    }

    return trs_id_pool_add_new_node(id_pool, flag, id);
}

#define TRS_NODE_ALLOCATED_TO_IDLE 0
#define TRS_NODE_IDLE_TO_ALLOCATED 1
static void trs_id_pool_move_node(struct trs_id_pool *id_pool, struct trs_id_node *node, u32 direction)
{
    ka_hash_del(&node->link);
    if (direction == TRS_NODE_ALLOCATED_TO_IDLE) {
        ka_hash_add(id_pool->idle_htable, &node->link, node->id_start);
        if (node->id_start < id_pool->attr.split) {
            ka_list_add(&node->list, &id_pool->idle_head);
        } else {
            ka_list_add_tail(&node->list, &id_pool->idle_head);
        }
        node->pid = 0;
        node->is_in_idle = 1;
        trs_debug("Add to idle list. (devid=%u; type=%s; id_start=%u; id_total_num=%u)\n", id_pool->inst.devid,
                  trs_id_type_to_name(id_pool->type), node->id_start, node->id_total_num);
    } else {
        ka_list_del(&node->list);
        ka_hash_add(id_pool->allocated_htable, &node->link, node->id_start);
        node->is_in_idle = 0;
        trs_debug("Add to allocated list. (devid=%u; type=%s; id_start=%u; id_total_num=%u)\n", id_pool->inst.devid,
                  trs_id_type_to_name(id_pool->type), node->id_start, node->id_total_num);
    }
}

static void trs_id_pool_del_node(struct trs_id_node *node)
{
    if (node->is_in_idle == 1) {
        ka_list_del(&node->list);
    }
    ka_hash_del(&node->link);
    trs_id_node_destroy(node);
}

static void trs_id_pool_del_node_all(struct trs_id_pool *id_pool)
{
    struct trs_id_node *node = NULL;
    struct trs_id_node *n = NULL;
    ka_hlist_node_t *tmp = NULL;
    u32 bkt = 0;

    ka_hash_for_each_safe(id_pool->allocated_htable, bkt, tmp, node, link)
    {
        trs_id_pool_del_node(node);
    }
    ka_list_for_each_entry_safe(node, n, &id_pool->idle_head, list)
    {
        trs_id_pool_del_node(node);
    }
    trs_debug("Id pool del node all. (devid=%u; type=%s; alloc_num=%u; allocatable_num=%u)\n", id_pool->inst.devid,
              trs_id_type_to_name(id_pool->type), id_pool->alloc_num, id_pool->allocatable_num);
}

static int trs_id_node_all(struct trs_id_pool *id_pool)
{
    int ret;
    u32 id;

    for (id = id_pool->attr.id_start; id < id_pool->attr.id_end; id++) {
        ret = trs_id_pool_add_new_node(id_pool, 0, id);
        if (ret != 0) {
            trs_id_pool_del_node_all(id_pool);
            return -EFAULT;
        }
    }
    return 0;
}

static struct trs_id_pool *trs_id_pool_create(struct trs_id_inst *inst, int type, struct trs_id_attr *attr,
                                              struct trs_id_ops *ops)
{
    struct trs_id_pool *id_pool = trs_kzalloc(trs_id_get_pool_size(), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);

    if (id_pool == NULL) {
        trs_err("Id pool no mem.\n");
        return NULL;
    }

    id_pool->inst = *inst;
    id_pool->type = type;
    id_pool->attr = *attr;
    id_pool->alloc_num = 0;
    id_pool->dfx_times = 0;
    ka_hash_init(id_pool->idle_htable);
    ka_hash_init(id_pool->allocated_htable);
    KA_INIT_LIST_HEAD(&id_pool->idle_head);
    KA_INIT_LIST_HEAD(&id_pool->rsv_head);
    kref_safe_init(&id_pool->ref);
    ka_task_mutex_init(&id_pool->mutex);

    if (ops == NULL) {
        int ret = trs_id_node_all(id_pool);
        if (ret != 0) {
            trs_err("Id node list init fail. (ret=%d)\n", ret);
            goto out;
        }
    } else {
        if ((attr->batch_num == 0) || (attr->batch_num > MAX_TRS_ID_BATCH_NUM)) {
            trs_err("Batch num invalid. (batch_num=%u)\n", attr->batch_num);
            goto out;
        }
        id_pool->ops = *ops;
    }
    return id_pool;
out:
    ka_task_mutex_destroy(&id_pool->mutex);
    trs_kfree(id_pool);
    return NULL;
}

static struct trs_id_node *trs_id_pool_get_available_node(struct trs_id_pool *id_pool)
{
    struct trs_id_node *node = NULL;
    ka_hlist_node_t *tmp = NULL;
    u32 bkt = 0;

    if (id_pool->attr.isolate_num > 1) {
        /* isolate_num > 1 means that there will be multiple ids in one node of allocated list */
        ka_hash_for_each_safe(id_pool->allocated_htable, bkt, tmp, node, link)
        {
            if ((node->id_avail_num > 0) && (node->pid == ka_task_get_current_tgid())) {
                return node;
            }
        }
    }

    if (ka_list_empty_careful(&id_pool->idle_head)) {
        return NULL;
    }
    node = ka_list_first_entry(&id_pool->idle_head, struct trs_id_node, list);
    return node;
}

static struct trs_id_node *trs_id_pool_get_idle_node(struct trs_id_pool *id_pool)
{
    if (!ka_list_empty_careful(&id_pool->idle_head)) {
        return ka_list_first_entry(&id_pool->idle_head, struct trs_id_node, list);
    }
    return NULL;
}

static inline void trs_id_pool_free_one_id_to_node(struct trs_id_pool *id_pool, struct trs_id_node *node, u32 id)
{
    node->offset[id - node->id_start] = 0;
    node->id_avail_num++;
    if (node->id_avail_num == node->id_total_num) {
        trs_id_pool_move_node(id_pool, node, TRS_NODE_ALLOCATED_TO_IDLE);
    }
}

static int trs_id_pool_alloc_one_id_in_node(struct trs_id_pool *id_pool, struct trs_id_node *node, u32 *id)
{
    int alloc_id = -1;
    u32 offset;

    for (offset = 0; offset < node->id_total_num; offset++) {
        if (node->offset[offset] == 0) {
            node->offset[offset] = 1;
            alloc_id = node->id_start + offset;
            break;
        }
    }
    if (alloc_id == -1) {
        trs_err("Node has no resource. (alloc_id=%d; start=%u; total_num=%u; avail_num=%u)\n", alloc_id, node->id_start,
                node->id_total_num, node->id_avail_num);
        return alloc_id;
    }
    if (node->pid == 0) {
        trs_id_pool_move_node(id_pool, node, TRS_NODE_IDLE_TO_ALLOCATED);
        node->pid = ka_task_get_current_tgid();
    }
    node->id_avail_num--;
    id_pool->allocatable_num--;
    *id = (u32)alloc_id;
    trs_debug("Alloc success. (type=%s; id=%d; start=%u; avail_num=%u; total_num=%u; isolate_num=%u; "
              "allocatable_num=%u)\n",
              trs_id_type_to_name(id_pool->type), alloc_id, node->id_start, node->id_avail_num, node->id_total_num,
              id_pool->attr.isolate_num, id_pool->allocatable_num);
    return 0;
}

static int trs_id_pool_alloc_one_available_id(struct trs_id_pool *id_pool, u32 *id)
{
    struct trs_id_node *node = trs_id_pool_get_available_node(id_pool);
    if (node == NULL) {
        return -ENOSPC;
    }
    return trs_id_pool_alloc_one_id_in_node(id_pool, node, id);
}

static int trs_id_pool_alloc_id_in_idle_node(struct trs_id_pool *id_pool, u32 batch_num, u32 *id, u32 *idx)
{
    int ret;
    struct trs_id_node *node = trs_id_pool_get_idle_node(id_pool);
    if (node == NULL) {
        return -ENOSPC;
    }

    for (; (*idx < batch_num) && (node->id_avail_num > 0); (*idx)++) {
        ret = trs_id_pool_alloc_one_id_in_node(id_pool, node, &id[*idx]);
        if (ret != 0) {
            return ret;
        }
    }
    if (node->id_avail_num == 0) {
        trs_debug("Del node. (type=%s; start=%u; total_num=%u; avail_num=%u; allocatable_num=%u; alloc=%u)\n",
                  trs_id_type_to_name(id_pool->type), node->id_start, node->id_total_num, node->id_avail_num,
                  id_pool->allocatable_num, id_pool->alloc_num);
        trs_id_pool_del_node(node);
    }
    return 0;
}

static int trs_id_pool_try_free_batch(struct trs_id_pool *id_pool)
{
    if (id_pool->ops.free_batch == NULL) {
        /* Local id do not need to free batch */
        return 0;
    }

    trs_debug("Free info. (type=%s; batch_num=%u; allocatable_num=%u; rsv_num=%u)\n",
              trs_id_type_to_name(id_pool->type), id_pool->attr.batch_num, id_pool->allocatable_num, id_pool->rsv_num);

    while (id_pool->allocatable_num > 0) {
        /* allocatable_num - rsv_num = not rsv num*/
        u32 batch_num = ka_base_min_t(u32, (id_pool->allocatable_num - id_pool->rsv_num), id_pool->attr.batch_num);
        u32 id[MAX_TRS_ID_BATCH_NUM];
        u32 idx = 0;
        int ret;

        while (idx < batch_num) {
            if (trs_id_pool_alloc_id_in_idle_node(id_pool, batch_num, id, &idx) != 0) {
                batch_num = idx;
                trs_debug("(batch_num=%u)\n", batch_num);
                break;
            }
        }

        if (batch_num == 0) {
#ifndef EMU_ST
            trs_warn("Free batch warn. (type=%s; batch_num=%u; allocatable_num=%u)\n",
                     trs_id_type_to_name(id_pool->type), batch_num, id_pool->allocatable_num);
#endif
            break;
        }

        ret = id_pool->ops.free_batch(&id_pool->inst, id_pool->type, id, batch_num);
        if (ret != 0) {
            trs_warn("Free batch warn. (type=%s; batch_num=%u; allocatable_num=%u; ret=%d)\n",
                     trs_id_type_to_name(id_pool->type), batch_num, id_pool->allocatable_num, ret);
            for (idx = 0; idx < batch_num; idx++) {
                (void)trs_id_pool_add_node(id_pool, 0, id[idx]);
            }
            return ret;
        }
    }

    return 0;
}

static void _trs_id_pool_destroy(struct trs_id_pool *id_pool)
{
    if (id_pool->ops.free_batch == NULL) {
        /* local id */
        trs_id_pool_del_node_all(id_pool);
    } else {
        (void)trs_id_pool_try_free_batch(id_pool);
        trs_id_pool_del_node_all(id_pool);
    }
}

static void trs_id_pool_destroy(struct trs_id_pool *id_pool)
{
    if (id_pool != NULL) {
        trs_info("id pool destroy. (type=%s)\n", trs_id_type_to_name(id_pool->type));
        _trs_id_pool_destroy(id_pool);
        ka_task_mutex_destroy(&id_pool->mutex);
        trs_kfree(id_pool);
    }
}

static int trs_id_pool_add(int type, struct trs_id_pool *id_pool)
{
    u32 ts_inst = trs_id_inst_to_ts_inst(&id_pool->inst);

    ka_task_mutex_lock(&g_trs_id_mutex);
    if (g_trs_id_pool[ts_inst][type] != NULL) {
        ka_task_mutex_unlock(&g_trs_id_mutex);
        trs_err("[%s] id pool exists. (devid=%u; tsid=%u)\n", trs_id_type_to_name(type), id_pool->inst.devid,
                id_pool->inst.tsid);
        return -ENODEV;
    }
    g_trs_id_pool[ts_inst][type] = id_pool;
    ka_task_mutex_unlock(&g_trs_id_mutex);
    return 0;
}

static void trs_id_pool_release(struct kref_safe *kref)
{
    struct trs_id_pool *id_pool = ka_container_of(kref, struct trs_id_pool, ref);

    trs_id_pool_destroy(id_pool);
}

static void trs_id_pool_del(struct trs_id_inst *inst, int type)
{
    u32 ts_inst = trs_id_inst_to_ts_inst(inst);
    struct trs_id_pool *id_pool = NULL;

    ka_task_mutex_lock(&g_trs_id_mutex);
    id_pool = g_trs_id_pool[ts_inst][type];
    g_trs_id_pool[ts_inst][type] = NULL;
    ka_task_mutex_unlock(&g_trs_id_mutex);

    if (id_pool != NULL) {
        kref_safe_put(&id_pool->ref, trs_id_pool_release);
    }
}

static int trs_id_type_check(int type)
{
    if ((type < TRS_STREAM_ID) || (type >= TRS_ID_TYPE_MAX)) {
        trs_err("Unknown Trs id type. (type=%d)\n", type);
        return -EINVAL;
    }
    return 0;
}

static int trs_id_param_check(struct trs_id_inst *inst, int type)
{
    int ret;

    ret = trs_id_inst_check(inst);
    if (ret != 0) {
        return ret;
    }

    ret = trs_id_type_check(type);
    if (ret != 0) {
        return ret;
    }

    return 0;
}

static struct trs_id_pool *trs_id_pool_get(struct trs_id_inst *inst, int type)
{
    struct trs_id_pool *id_pool = NULL;
    u32 ts_inst;
    int ret;

    ret = trs_id_param_check(inst, type);
    if (ret != 0) {
        return NULL;
    }

    ts_inst = trs_id_inst_to_ts_inst(inst);
    ka_task_mutex_lock(&g_trs_id_mutex);
    id_pool = g_trs_id_pool[ts_inst][type];
    if (id_pool != NULL) {
        /* When id pool is obtained, the module reference counting of ops must be added. */
        if (ka_system_try_module_get(id_pool->ops.owner)) {
            kref_safe_get(&id_pool->ref);
        } else {
            id_pool = NULL;
        }
    }
    ka_task_mutex_unlock(&g_trs_id_mutex);
    return id_pool;
}

static void trs_id_pool_put(struct trs_id_pool *id_pool)
{
    ka_system_module_put(id_pool->ops.owner);
    kref_safe_put(&id_pool->ref, trs_id_pool_release);
}

static int trs_id_pool_range_check(struct trs_id_pool *id_pool, u32 id)
{
    if ((id < id_pool->attr.id_start) || (id >= (id_pool->attr.id_end))) {
        trs_err("Trs id range check fail. (id_start=%u; id_end=%u; type=%d; id=%u)\n", id_pool->attr.id_start,
                id_pool->attr.id_end, id_pool->type, id);
        return -EBADR;
    }
    return 0;
}

static void trs_id_pool_alloc_batch(struct trs_id_pool *id_pool, u32 flag, u32 specified_id, u32 num)
{
    u32 id[MAX_TRS_ID_BATCH_NUM];
    u32 i, id_num, remain_num;
    int ret;

    remain_num = id_pool->attr.id_num - id_pool->alloc_num;
    if (remain_num == 0) {
        /* Max alloc num, don't try to sync id */
        return;
    }

    if (id_pool->ops.alloc_batch == NULL) {
        return;
    }

    if (id_pool->attr.batch_num <= 1) {
        /* non-cache cannot goto cache id allocator branch. */
        return;
    }

    if (trs_id_is_specified(flag)) {
        id_num = num;
        id[0] = specified_id;
    } else {
        id_num = (remain_num > id_pool->attr.batch_num) ? id_pool->attr.batch_num : remain_num;
    }

    id_num = ka_base_round_up(id_num, id_pool->attr.isolate_num);
    ret = id_pool->ops.alloc_batch(&id_pool->inst, id_pool->type, flag, id, &id_num);
    if ((ret != 0) || (id_num > id_pool->attr.batch_num)) {
        if (id_pool->dfx_times < ID_POOL_DFX_MAX_TIMES) {
            id_pool->dfx_times++;
            trs_warn(
                "Alloc batch fail. (devid=%u; tsid=%u; ret=%d; flag=0x%x; real_id_num=%u; batch_num=%u; type=%s)\n",
                id_pool->inst.devid, id_pool->inst.tsid, ret, flag, id_num, id_pool->attr.batch_num,
                trs_id_type_to_name(id_pool->type));
        }
        return;
    }

    for (i = 0; i < id_num; i++) {
        ret = trs_id_pool_range_check(id_pool, id[i]);
        if (ret == 0) {
            ret = trs_id_pool_add_node(id_pool, 0, id[i]);
            if (ret != 0) {
                (void)id_pool->ops.free_batch(&id_pool->inst, id_pool->type, &id[i], id_num - i);
                break;
            }
        }
    }

    trs_debug("Alloced batch num. (num=%u; type=%d)\n", id_num, id_pool->type);
}

static int trs_id_alloc_non_cache(struct trs_id_pool *id_pool, u32 flag, u32 *id)
{
    u32 id_num = 1;
    int ret;

    ret = id_pool->ops.alloc_batch(&id_pool->inst, id_pool->type, flag, id, &id_num);
    if ((ret != 0) || (id_num > id_pool->attr.batch_num) || (id_num == 0)) {
        trs_debug("Alloc batch fail. (devid=%u; tsid=%u; real_id_num=%u; batch_num=%u; type=%s; ret=%d)\n",
                  id_pool->inst.devid, id_pool->inst.tsid, id_num, id_pool->attr.batch_num,
                  trs_id_type_to_name(id_pool->type), ret);
        return -ENOSPC;
    }

    id_pool->alloc_num++;
    return ret;
}

static int trs_id_pool_alloc_specified_id_in_node(struct trs_id_pool *id_pool, struct trs_id_node *node, u32 start,
                                                  u32 end, u32 *id)
{
    int tmp_id = -1;
    u32 offset, offset_start, offset_end;

    offset_start = (start >= (int)node->id_start) ? start - node->id_start : 0;
    offset_end = (end <= (int)(node->id_start + node->id_total_num)) ? end - node->id_start : node->id_total_num;

    for (offset = offset_start; offset < offset_end; offset++) {
        if (node->offset[offset] == 0) {
            node->offset[offset] = 1;
            tmp_id = node->id_start + offset;
            break;
        }
    }
    if (tmp_id == -1) {
        trs_err("Node has no resource. (alloc_id=%d; start=%u; total_num=%u; avail_num=%u)\n", tmp_id, node->id_start,
                node->id_total_num, node->id_avail_num);
        return tmp_id;
    }

    if (node->pid == 0) {
        trs_id_pool_move_node(id_pool, node, TRS_NODE_IDLE_TO_ALLOCATED);
        node->pid = ka_task_get_current_tgid();
    }
    node->id_avail_num--;
    id_pool->allocatable_num--;
    *id = (u32)tmp_id;
    trs_debug("Alloc specified id. (devid=%u; type=%s; id=%u; id_start=%d; id_end=%d; node_pid=%d)\n",
              id_pool->inst.devid, trs_id_type_to_name(id_pool->type), *id, offset_start, offset_end, node->pid);
    return 0;
}

static bool trs_id_pool_find_specified_id(struct trs_id_pool *id_pool, u32 start, u32 end, u32 *id)
{
    struct trs_id_rsv_node *rsv_node = NULL;
    struct trs_id_rsv_node *rsv_n = NULL;
    struct trs_id_node *node = NULL;
    struct trs_id_node *n = NULL;
    ka_hlist_node_t *tmp = NULL;
    u32 bkt = 0;
    int ret;

    trs_debug("Find specified_id. (id=%u; start=%u; end=%u; pid=%d)\n", *id, start, end, ka_task_get_current_tgid());
    ka_list_for_each_entry_safe(rsv_node, rsv_n, &id_pool->rsv_head, list)
    {
        if ((rsv_node->id == *id) && (rsv_node->pid == ka_task_get_current_tgid())) {
            id_pool->rsv_num--;
            trs_id_pool_del_rsv_node(rsv_node);
            id_pool->allocatable_num--;
            return true;
        }
    }

    if (id_pool->attr.isolate_num > 1) {
        ka_hash_for_each_safe(id_pool->allocated_htable, bkt, tmp, node, link)
        {
            if ((node->pid != ka_task_get_current_tgid()) || (node->id_start >= end) ||
                (node->id_start + node->id_total_num <= start)) {
                continue;
            }
            ret = trs_id_pool_alloc_specified_id_in_node(id_pool, node, start, end, id);
            if (ret == 0) {
                return true;
            }
        }
    }

    ka_list_for_each_entry_safe(node, n, &id_pool->idle_head, list)
    {
        if ((node->id_start >= end) || (node->id_start + node->id_total_num <= start)) {
            continue;
        }
        ret = trs_id_pool_alloc_specified_id_in_node(id_pool, node, start, end, id);
        if (ret == 0) {
            return true;
        }
    }

    return false;
}

static bool trs_id_pool_has_res_for_alloc(struct trs_id_pool *id_pool)
{
    struct trs_id_node *node = NULL;
    ka_hlist_node_t *tmp = NULL;
    u32 bkt = 0;

    if (!ka_list_empty(&id_pool->idle_head)) {
        return true;
    }

    if (id_pool->attr.isolate_num > 1) {
        ka_hash_for_each_safe(id_pool->allocated_htable, bkt, tmp, node, link)
        {
            if ((node->id_avail_num > 0) && (node->pid == ka_task_get_current_tgid())) {
                return true;
            }
        }
    }
    return false;
}

static void trs_id_print_isolated_res_detail(struct trs_id_pool *id_pool)
{
    if (id_pool->type == TRS_NOTIFY_ID) {
        return;
    }

    if (id_pool->attr.isolate_num > 1) {
        struct trs_id_node *node = NULL;
        ka_hlist_node_t *tmp = NULL;
        u32 bkt = 0;

        trs_err("No res. (devid=%u; tsid=%u; type=%s; allocatable_num=%u; alloc_num=%u)\n", id_pool->inst.devid,
                id_pool->inst.tsid, trs_id_type_to_name(id_pool->type), id_pool->allocatable_num, id_pool->alloc_num);

        ka_hash_for_each_safe(id_pool->allocated_htable, bkt, tmp, node, link)
        {
            if (node->id_avail_num > 0) {
                trs_err("Other process node. (pid=%d; id_avail_num=%u)\n", node->pid, node->id_avail_num);
            }
        }
    }
}

static int trs_id_alloc_cache(struct trs_id_pool *id_pool, u32 flag, u32 *id, u32 num)
{
    bool find_specified_id = true;

    if (trs_id_is_specified(flag)) {
        find_specified_id = trs_id_pool_find_specified_id(id_pool, *id, (*id) + num, id);
        if (find_specified_id) {
            id_pool->alloc_num++;
            return 0;
        }
    }

    if (!trs_id_pool_has_res_for_alloc(id_pool) || (!find_specified_id)) {
        trs_id_pool_alloc_batch(id_pool, flag, *id, num);
        if (!trs_id_pool_has_res_for_alloc(id_pool)) {
            trs_id_print_isolated_res_detail(id_pool);
            return -ENOSPC;
        }
    }

    if (trs_id_is_specified(flag)) {
        if (!trs_id_pool_find_specified_id(id_pool, *id, (*id) + num, id)) {
            trs_err("Fail to alloc id in range. (devid=%u; type=%s; start=%u; end=%u)\n", id_pool->inst.devid,
                    trs_id_type_to_name(id_pool->type), *id, (*id) + num);
            return -ENOSPC;
        }
    } else {
        int ret;
        ret = trs_id_pool_alloc_one_available_id(id_pool, id);
        if (ret != 0) {
            trs_err("Alloc id failed. (ret=%d; devid=%u; type=%s; allocatable_num=%u; alloc_num=%u)\n", ret,
                    id_pool->inst.devid, trs_id_type_to_name(id_pool->type), id_pool->allocatable_num,
                    id_pool->alloc_num);
            return ret;
        }
    }

    id_pool->alloc_num++;
    trs_debug("Alloc id. (devid=%u; type=%s; id=%u; allocatable_num=%u; alloc_num=%u)\n", id_pool->inst.devid,
              trs_id_type_to_name(id_pool->type), *id, id_pool->allocatable_num, id_pool->alloc_num);

    return 0;
}

static int trs_id_pool_alloc(struct trs_id_pool *id_pool, u32 flag, u32 *id, u32 num)
{
    if ((id_pool->ops.is_non_cache_type != NULL) && id_pool->ops.is_non_cache_type(id_pool->type)) {
        return trs_id_alloc_non_cache(id_pool, flag, id);
    } else {
        return trs_id_alloc_cache(id_pool, flag, id, num);
    }
}

static int trs_id_free_non_cache(struct trs_id_pool *id_pool, u32 flag, u32 id)
{
    int ret;

    if (trs_id_is_reserved(flag)) {
        trs_warn("No support non cache type free with reserved flag. (flag=0x%x; id=%u)\n", flag, id);
    }

    ret = id_pool->ops.free_batch(&id_pool->inst, id_pool->type, &id, 1);
    if (ret != 0) {
        trs_warn("Free batch warn. (type=%s; batch_num=%u; allocatable_num=%u; ret=%d)\n",
                 trs_id_type_to_name(id_pool->type), id_pool->attr.batch_num, id_pool->allocatable_num, ret);
        return ret;
    }

    id_pool->alloc_num--;
    return ret;
}

static int trs_id_free_cache(struct trs_id_pool *id_pool, u32 flag, u32 id)
{
    struct trs_id_node *node = NULL;
    int ret = 0;

    node = trs_id_pool_get_allocated_node_by_id(id_pool, id);
    if (node == NULL) {
        trs_err("Invalid id. (devid=%u; tsid=%u; type=%s; id=%u)\n", id_pool->inst.devid, id_pool->inst.tsid,
                trs_id_type_to_name(id_pool->type), id);
        return -EINVAL;
    }

    if (!trs_id_is_reserved(flag)) {
        trs_id_pool_free_one_id_to_node(id_pool, node, id);
        id_pool->allocatable_num++;
    } else {
        ret = trs_id_pool_add_rsv_node(id_pool, id);
        if (ret != 0) {
            ret = trs_id_free_non_cache(id_pool, flag, id);
        }
    }

    id_pool->alloc_num--;
    trs_debug("Free id. (ret=%d; devid=%u; type=%s; id=%u; node_start=%u; avail_num=%u; total=%u; allocatable_num=%u; "
              "alloc_num=%u)\n",
              ret, id_pool->inst.devid, trs_id_type_to_name(id_pool->type), id, node->id_start, node->id_avail_num,
              node->id_total_num, id_pool->allocatable_num, id_pool->alloc_num);
    return ret;
}

static int trs_id_pool_free(struct trs_id_pool *id_pool, u32 flag, u32 id)
{
    int ret;

    ret = trs_id_pool_range_check(id_pool, id);
    if (ret != 0) {
        return ret;
    }

    if ((id_pool->ops.is_non_cache_type != NULL) && id_pool->ops.is_non_cache_type(id_pool->type)) {
        return trs_id_free_non_cache(id_pool, flag, id);
    } else {
        return trs_id_free_cache(id_pool, flag, id);
    }
}

static int trs_id_attr_check(struct trs_id_attr *attr)
{
    if (attr == NULL) {
        return -EINVAL;
    }

    if ((attr->id_num == 0) || ((attr->id_start + attr->id_num) < attr->id_start)) {
        trs_warn("Id para. (id_num=%u; id_start=%u; id_end=%u)\n", attr->id_num, attr->id_start, attr->id_end);
        return -EINVAL;
    }

    if (attr->batch_num > MAX_TRS_ID_BATCH_NUM) {
        trs_err("Invalid para. (batch_num=%u)\n", attr->batch_num);
        return -EINVAL;
    }

    return 0;
}

int trs_id_register(struct trs_id_inst *inst, int type, struct trs_id_attr *attr, struct trs_id_ops *ops)
{
    struct trs_id_pool *id_pool = NULL;
    int ret;

    if (trs_id_param_check(inst, type) != 0) {
        return -EINVAL;
    }

    if (trs_id_attr_check(attr) != 0) {
        return -EINVAL;
    }

    id_pool = trs_id_pool_create(inst, type, attr, ops);
    if (id_pool == NULL) {
        return -ENOMEM;
    }

    ret = trs_id_pool_add(type, id_pool);
    if (ret != 0) {
        trs_id_pool_destroy(id_pool);
        return ret;
    }
    trs_debug("Trs id init. (devid=%u; tsid=%u; type=%s; start=%u; end=%u; num=%u; split=%u; isolate_num=%u)\n",
              inst->devid, inst->tsid, trs_id_type_to_name(type), attr->id_start, attr->id_end, attr->id_num,
              attr->split, attr->isolate_num);
    return 0;
}
KA_EXPORT_SYMBOL_GPL(trs_id_register);

int trs_id_unregister(struct trs_id_inst *inst, int type)
{
    if (trs_id_param_check(inst, type) != 0) {
        return -EINVAL;
    }

    trs_id_pool_del(inst, type);
    return 0;
}
KA_EXPORT_SYMBOL_GPL(trs_id_unregister);

int trs_id_get_total_num(struct trs_id_inst *inst, int type, u32 *total_num)
{
    struct trs_id_pool *id_pool = trs_id_pool_get(inst, type);
    int ret = -EFAULT;

    if (id_pool != NULL) {
        ka_task_mutex_lock(&id_pool->mutex);
        if (total_num != NULL) {
            *total_num = id_pool->attr.id_num;
            ret = 0;
        }
        ka_task_mutex_unlock(&id_pool->mutex);
        trs_id_pool_put(id_pool);
    }
    return ret;
}
KA_EXPORT_SYMBOL_GPL(trs_id_get_total_num);

int trs_id_get_range(struct trs_id_inst *inst, int type, u32 *start, u32 *end)
{
    struct trs_id_pool *id_pool = trs_id_pool_get(inst, type);
    int ret = -EFAULT;

    if (id_pool != NULL) {
        ka_task_mutex_lock(&id_pool->mutex);
        if ((start != NULL) && (end != NULL)) {
            *start = id_pool->attr.id_start;
            *end = id_pool->attr.id_end;
            ret = 0;
        }
        ka_task_mutex_unlock(&id_pool->mutex);
        trs_id_pool_put(id_pool);
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(trs_id_get_range);

int trs_id_get_max_id(struct trs_id_inst *inst, int type, u32 *max_id)
{
    struct trs_id_pool *id_pool = trs_id_pool_get(inst, type);
    int ret = -ENODEV;

    if (id_pool != NULL) {
        ka_task_mutex_lock(&id_pool->mutex);
        if (max_id != NULL) {
            *max_id = id_pool->attr.id_end;
            ret = 0;
        }
        ka_task_mutex_unlock(&id_pool->mutex);

        trs_id_pool_put(id_pool);
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(trs_id_get_max_id);

int trs_id_get_split(struct trs_id_inst *inst, int type, u32 *split)
{
    struct trs_id_pool *id_pool = trs_id_pool_get(inst, type);
    int ret = -ENODEV;

    if (id_pool != NULL) {
        ka_task_mutex_lock(&id_pool->mutex);
        if (split != NULL) {
            *split = id_pool->attr.split;
            ret = 0;
        }
        ka_task_mutex_unlock(&id_pool->mutex);
        trs_id_pool_put(id_pool);
    }
    return ret;
}
KA_EXPORT_SYMBOL_GPL(trs_id_get_split);

int trs_id_get_avail_num(struct trs_id_inst *inst, int type, u32 *avail_num)
{
    struct trs_id_pool *id_pool = trs_id_pool_get(inst, type);
    int ret = -ENODEV;

    if (id_pool != NULL) {
        ka_task_mutex_lock(&id_pool->mutex);
        if (avail_num != NULL) {
            *avail_num = id_pool->attr.id_num - id_pool->alloc_num;
            ret = 0;
        }
        ka_task_mutex_unlock(&id_pool->mutex);
        trs_id_pool_put(id_pool);
    }
    return ret;
}
KA_EXPORT_SYMBOL_GPL(trs_id_get_avail_num);

int trs_id_get_avail_num_in_pool(struct trs_id_inst *inst, int type, u32 *avail_num)
{
    struct trs_id_pool *id_pool = trs_id_pool_get(inst, type);
    int ret = -ENODEV;

    if (id_pool != NULL) {
        ka_task_mutex_lock(&id_pool->mutex);
        if (id_pool->ops.avail_query != NULL) {
            ret = id_pool->ops.avail_query(inst, type, avail_num);
        }
        ka_task_mutex_unlock(&id_pool->mutex);
        trs_id_pool_put(id_pool);
    }
    return ret;
}
KA_EXPORT_SYMBOL_GPL(trs_id_get_avail_num_in_pool);

int trs_id_get_used_num(struct trs_id_inst *inst, int type, u32 *used_num)
{
    struct trs_id_pool *id_pool = trs_id_pool_get(inst, type);
    int ret = -EOPNOTSUPP;

    if (id_pool != NULL) {
        ka_task_mutex_lock(&id_pool->mutex);
        if (used_num != NULL) {
            *used_num = id_pool->alloc_num;
            ret = 0;
        }
        ka_task_mutex_unlock(&id_pool->mutex);
        trs_id_pool_put(id_pool);
    }
    return ret;
}
KA_EXPORT_SYMBOL_GPL(trs_id_get_used_num);

int trs_id_get_stat(struct trs_id_inst *inst, int type, struct trs_id_stat *stat)
{
    struct trs_id_pool *id_pool = trs_id_pool_get(inst, type);
    int ret = -ENODEV;

    if (id_pool != NULL) {
        ka_task_mutex_lock(&id_pool->mutex);
        stat->alloc = id_pool->alloc_num;
        stat->allocatable = id_pool->allocatable_num;
        stat->rsv_num = id_pool->rsv_num;
        ret = 0;
        ka_task_mutex_unlock(&id_pool->mutex);
        trs_id_pool_put(id_pool);
    }
    return ret;
}
KA_EXPORT_SYMBOL_GPL(trs_id_get_stat);

int trs_id_alloc_ex(struct trs_id_inst *inst, int type, u32 flag, u32 *id, u32 para)
{
    struct trs_id_pool *id_pool = trs_id_pool_get(inst, type);
    int ret = -EOPNOTSUPP;

    if (id_pool != NULL) {
        ka_task_mutex_lock(&id_pool->mutex);
        if (id != NULL) {
            ret = trs_id_pool_alloc(id_pool, flag, id, para);
        }
        ka_task_mutex_unlock(&id_pool->mutex);
        trs_id_pool_put(id_pool);
    }

    if ((ret == 0) && (id != NULL)) {
        trs_debug("Alloc success. (devid=%u; type=%s; flag=0x%x; para=%u; id=%u)\n", inst->devid,
                  trs_id_type_to_name(type), flag, para, *id);
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(trs_id_alloc_ex);

int trs_id_free_ex(struct trs_id_inst *inst, int type, u32 flag, u32 id)
{
    struct trs_id_pool *id_pool = trs_id_pool_get(inst, type);
    int ret = -ENODEV;

    trs_debug("Free id. (type=%s; flag=0x%x; id=%u)\n", trs_id_type_to_name(type), flag, id);

    if (id_pool != NULL) {
        ka_task_mutex_lock(&id_pool->mutex);
        ret = trs_id_pool_free(id_pool, flag, id);
        ka_task_mutex_unlock(&id_pool->mutex);
        trs_id_pool_put(id_pool);
    }
    return ret;
}
KA_EXPORT_SYMBOL_GPL(trs_id_free_ex);

int trs_id_free_batch_by_type(struct trs_id_inst *inst, int type)
{
    struct trs_id_pool *id_pool = trs_id_pool_get(inst, type);
    int ret;

    if (id_pool == NULL) {
        return 0;
    }

    ka_task_mutex_lock(&id_pool->mutex);
    ret = trs_id_pool_try_free_batch(id_pool);
    ka_task_mutex_unlock(&id_pool->mutex);
    trs_id_pool_put(id_pool);
    return ret;
}
KA_EXPORT_SYMBOL_GPL(trs_id_free_batch_by_type);

int trs_id_flush_to_pool(struct trs_id_inst *inst)
{
    int type, ret = 0;

    for (type = TRS_STREAM_ID; type < TRS_ID_TYPE_MAX; type++) {
        if (trs_id_free_batch_by_type(inst, type) != 0) {
            ret = -ENODEV;
            trs_err("Id flush to pool fail. (devid=%u; tsid=%u; type=%s)\n", inst->devid, inst->tsid,
                    trs_id_type_to_name(type));
        }
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(trs_id_flush_to_pool);

static struct trs_id_node *trs_id_pool_get_current_node_by_id(struct trs_id_pool *id_pool, u32 id)
{
    struct trs_id_node *node = NULL;
    u32 key = trs_id_make_hash_key(id_pool->attr.isolate_num, id);

    ka_hash_for_each_possible(id_pool->allocated_htable, node, link, key)
    {
        if ((id >= node->id_start) && (id < (node->id_start + node->id_total_num)) &&
            (node->pid == ka_task_get_current_tgid())) {
            return node;
        }
    }
    return NULL;
}

static void _trs_id_clear_reserved_flag(struct trs_id_pool *id_pool, ka_pid_t pid)
{
    struct trs_id_rsv_node *node = NULL;
    struct trs_id_rsv_node *n = NULL;
    struct trs_id_node *hash_node = NULL;
    u32 id;

    ka_list_for_each_entry_safe(node, n, &id_pool->rsv_head, list)
    {
        if (node->pid == pid) {
            id = node->id;
            trs_id_pool_del_rsv_node(node);
            id_pool->rsv_num--;

            hash_node = trs_id_pool_get_current_node_by_id(id_pool, id);
            if (hash_node == NULL) {
                trs_warn("Invalid id. (devid=%u; type=%s; id=%u)\n", id_pool->inst.devid,
                         trs_id_type_to_name(id_pool->type), id);
                continue;
            }

            trs_id_pool_free_one_id_to_node(id_pool, hash_node, id);
        }
    }
}

static void trs_id_clear_reserved_flag_by_type(struct trs_id_inst *inst, int type, ka_pid_t pid)
{
    struct trs_id_pool *id_pool = trs_id_pool_get(inst, type);

    if (id_pool == NULL) {
        return;
    }

    ka_task_mutex_lock(&id_pool->mutex);
    _trs_id_clear_reserved_flag(id_pool, pid);
    ka_task_mutex_unlock(&id_pool->mutex);
    trs_id_pool_put(id_pool);
}

void trs_id_clear_reserved_flag(struct trs_id_inst *inst, ka_pid_t pid)
{
    int type;

    for (type = TRS_STREAM_ID; type < TRS_ID_TYPE_MAX; type++) {
        trs_id_clear_reserved_flag_by_type(inst, type, pid);
    }
}
KA_EXPORT_SYMBOL_GPL(trs_id_clear_reserved_flag);

int trs_id_to_string(struct trs_id_inst *inst, int type, u32 id, char *msg, u32 msg_len)
{
    struct trs_id_pool *id_pool = trs_id_pool_get(inst, type);
    int ret = -ENODEV;

    if (id_pool != NULL) {
        ret = 0;
        if (id_pool->ops.trans != NULL) {
            u32 phy_id;
            ret = id_pool->ops.trans(inst, type, id, &phy_id);
            if ((ret == 0) && (id != phy_id)) {
                ret = sprintf_s(msg, msg_len, "type(%s),id(%u),phy id(%u).\n", trs_id_type_to_name(type), id, phy_id);
            }
        }

        trs_id_pool_put(id_pool);
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(trs_id_to_string);

int init_trs_id(void)
{
    int trs_inst, type;

    for (trs_inst = 0; trs_inst < TRS_TS_INST_MAX_NUM; trs_inst++) {
        for (type = TRS_STREAM_ID; type < TRS_ID_TYPE_MAX; type++) {
            g_trs_id_pool[trs_inst][type] = NULL;
        }
    }
    trs_id_cache = ka_mm_kmem_cache_create("trs_id_cache", sizeof(struct trs_id_node), 0, 0, NULL);
    if (trs_id_cache == NULL) {
        trs_err("kmem cache create fail\n");
        return -ENOMEM;
    }
    ka_task_mutex_init(&g_trs_id_mutex);
    return 0;
}

void exit_trs_id(void)
{
    int trs_inst, type;

    trs_info("Exit trs id\n");

    if (trs_id_cache != NULL) {
        ka_mm_kmem_cache_destroy(trs_id_cache);
        trs_id_cache = NULL;
        trs_debug("Trs id cache destroy\n");
    }

    for (trs_inst = 0; trs_inst < TRS_TS_INST_MAX_NUM; trs_inst++) {
        for (type = TRS_STREAM_ID; type < TRS_ID_TYPE_MAX; type++) {
            trs_id_pool_destroy(g_trs_id_pool[trs_inst][type]);
            g_trs_id_pool[trs_inst][type] = NULL;
        }
    }
    ka_task_mutex_destroy(&g_trs_id_mutex);
}