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

#include "uvm_async_task.h"
#include "devmm_mem_alloc_interface.h"
#include "svm_log.h"
#include "devmm_common.h"

#define UVM_TASK_HLIST_TABLE_BIT 13
#define UVM_TASK_HLIST_TABLE_MASK ((0x1UL << UVM_TASK_HLIST_TABLE_BIT) - 1)

static KA_DEFINE_HASHTABLE(prefetch_msg_id_hashtable, UVM_TASK_HLIST_TABLE_BIT);

ka_task_spinlock_t prefetch_msg_id_lock;
KA_TASK_DEFINE_SPINLOCK(prefetch_msg_id_lock);

void uvm_hnode_add_to_prefetch_msg_id_hashtable(struct msg_process_cnt *cnt)
{
#ifndef EMU_ST
    uint32_t key;
    key = ka_base_jhash(&cnt->id, sizeof(cnt->id), 0) & UVM_TASK_HLIST_TABLE_MASK;
    ka_task_spin_lock(&prefetch_msg_id_lock);
    ka_hash_add(prefetch_msg_id_hashtable, &cnt->hnode, key);
    ka_task_spin_unlock(&prefetch_msg_id_lock);
#endif
}

struct msg_process_cnt *uvm_hnode_find_from_prefetch_msg_id_hashtable(uint64_t prefetch_msg_id)
{
#ifndef EMU_ST
    struct msg_process_cnt *cnt = NULL;
    uint32_t key;

    key = ka_base_jhash(&prefetch_msg_id, sizeof(prefetch_msg_id), 0) & UVM_TASK_HLIST_TABLE_MASK;

    ka_task_spin_lock(&prefetch_msg_id_lock);
    ka_hash_for_each_possible(prefetch_msg_id_hashtable, cnt, hnode, key)
    {
        if (cnt->id == prefetch_msg_id) {
            ka_task_spin_unlock(&prefetch_msg_id_lock);
            return cnt;
        }
    }
    ka_task_spin_unlock(&prefetch_msg_id_lock);
#endif
    return NULL;
}

int uvm_hnode_find_and_add_to_prefetch_msg_id_hashtable(struct msg_process_cnt **cnt, uint64_t prefetch_msg_id)
{
    struct msg_process_cnt *tmp_cnt = NULL;
    uint32_t key;
    bool found = false;
    int ret = 0;

    key = ka_base_jhash(&prefetch_msg_id, sizeof(prefetch_msg_id), 0) & UVM_TASK_HLIST_TABLE_MASK;

    ka_task_spin_lock(&prefetch_msg_id_lock);
    ka_hash_for_each_possible(prefetch_msg_id_hashtable, tmp_cnt, hnode, key)
    {
        if (tmp_cnt->id == prefetch_msg_id) {
            found = true;
            ret = 1;
            *cnt = tmp_cnt;
            break;
        }
    }

    if (!found) {
        *cnt = devmm_kvalloc(sizeof(**cnt), KA_GFP_ATOMIC);
        if (!*cnt) {
            devmm_drv_err("kzalloc for cnt failed\n");
            ka_task_spin_unlock(&prefetch_msg_id_lock);
            return -EFAULT;
        }

        ka_base_atomic_set(&(*cnt)->cnt, 0);
        (*cnt)->id = prefetch_msg_id;
        ka_hash_add(prefetch_msg_id_hashtable, &(*cnt)->hnode, key);
        ret = 0;
    }
    ka_task_spin_unlock(&prefetch_msg_id_lock);
    return ret;
}

void uvm_hnode_del_from_prefetch_msg_id_hashtable(struct msg_process_cnt *cnt)
{
    ka_task_spin_lock(&prefetch_msg_id_lock);
    ka_hash_del(&cnt->hnode);
    ka_task_spin_unlock(&prefetch_msg_id_lock);
}

int add_to_prefetch_msg_id_hashtable(struct msg_process_cnt **cnt, uint64_t prefetch_msg_id)
{
#ifndef EMU_ST
    struct msg_process_cnt *tmp_cnt = NULL;
    struct msg_process_cnt *new_cnt = NULL;
    uint32_t key;

    new_cnt = devmm_kzalloc_ex(sizeof(*new_cnt), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (!new_cnt) {
        devmm_drv_err("kzalloc for cnt failed\n");
        return -EFAULT;
    }

    ka_base_atomic_set(&new_cnt->cnt, 0);
    new_cnt->id = prefetch_msg_id;
    key = ka_base_jhash(&prefetch_msg_id, sizeof(prefetch_msg_id), 0) & UVM_TASK_HLIST_TABLE_MASK;

    ka_task_spin_lock(&prefetch_msg_id_lock);
    ka_hash_for_each_possible(prefetch_msg_id_hashtable, tmp_cnt, hnode, key)
    {
        if (tmp_cnt->id == prefetch_msg_id) {
            *cnt = tmp_cnt;
            ka_task_spin_unlock(&prefetch_msg_id_lock);
            devmm_kfree_ex(new_cnt);
            return 0;
        }
    }
    ka_hash_add(prefetch_msg_id_hashtable, &new_cnt->hnode, key);
    *cnt = new_cnt;
    ka_task_spin_unlock(&prefetch_msg_id_lock);
#endif
    return 0;
}