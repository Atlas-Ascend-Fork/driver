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
#include "ka_common_pub.h"
#include "ka_hashtable_pub.h"
#include "ka_system_pub.h"
#include "ka_memory_pub.h"

#include "pbl_kref_safe.h"

#include "ascend_hal_define.h"
#include "securec.h"
#include "trs_core.h"
#include "trs_res_id_def.h"
#include "trs_shr_id_node.h"

#define TRS_SHR_ID_WLIST_FIRST_IDX 1

#define SHR_ID_NAME_COMPOSE_NUM 5
#define SHR_ID_HASH_TABLE_BIT 4
#define SHR_ID_HASH_BUCKET 16

#define TRS_SHR_ID_NODE_MAX_OPEN_NUM ((KA_S32_MAX - SHR_ID_PID_MAX_NUM) / SHR_ID_PID_MAX_NUM)

static ka_atomic_t g_shr_id_get_seq = KA_BASE_ATOMIC_INIT(0);

struct shr_id_node_htable {
    KA_DECLARE_HASHTABLE(htable, SHR_ID_HASH_TABLE_BIT);
    ka_rwlock_t lock;
};

static struct shr_id_node_htable htable[SHR_ID_TYPE_MAX];

static u32 get_elfhash_key(const char *name)
{
    struct shr_id_node_op_attr attr;
    int ret;

    ret = sscanf_s(name, "%08x%08x%08x%08x%08x", &attr.pid, &attr.inst.devid, &attr.inst.tsid, &attr.type, &attr.id);
    return (ret == SHR_ID_NAME_COMPOSE_NUM) ? attr.id % SHR_ID_HASH_BUCKET : 0;
}

struct shr_id_node_ops g_shrid_node_ops;
void shr_id_register_node_ops(struct shr_id_node_ops *ops)
{
    g_shrid_node_ops = *ops;
}

static struct shr_id_node_ops *shr_id_get_node_ops(void)
{
    return &g_shrid_node_ops;
}
struct shr_id_node_wlist {
    ka_pid_t pid;
    u64 set_time;
    u32 open_num;
};

struct shr_id_node {
    struct shr_id_node_op_attr attr;
    u32 key;

    ka_spinlock_t lock; /* lock whitelist */
    ka_hlist_node_t link;
    struct kref_safe ref;
    struct shr_id_node_wlist wlist[SHR_ID_PID_MAX_NUM];

    ka_mutex_t priv_mutex; /* lock priv(spod_info) */
    void *priv;            /* for expand */
    bool need_wlist;
};

static void _shr_id_node_init(struct shr_id_node *node, struct shr_id_node_op_attr *attr)
{
    node->attr = *attr;
    node->need_wlist = true;
    node->wlist[0].pid = attr->pid;
    node->wlist[0].set_time = ka_system_ktime_get_ns();
    node->key = get_elfhash_key(node->attr.name);

    ka_task_spin_lock_init(&node->lock);
    ka_task_mutex_init(&node->priv_mutex);
    kref_safe_init(&node->ref);
}

int shr_id_name_update(u32 devid, char *name)
{
    if (shr_id_get_node_ops()->name_update != NULL) {
        return shr_id_get_node_ops()->name_update(devid, name);
    }

    trs_debug("Not support name update. (devid=%u)\n", devid);
    return 0;
}

static struct shr_id_node *_shr_id_node_create(struct shr_id_node_op_attr *attr)
{
    struct shr_id_node *node = trs_kzalloc(sizeof(struct shr_id_node), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);

    if (node != NULL) {
        if (attr->res_get != NULL) {
            int ret = 0;
            attr->get_hpid = (u32)ka_task_get_current_tgid();
            attr->get_seq = ka_base_atomic_inc_return(&g_shr_id_get_seq);
            ret = attr->res_get(&attr->inst, attr->res_type, attr->id, attr->get_hpid, attr->get_seq);
            if (ret != 0) {
                trs_kfree(node);
                return NULL;
            }
        }
        _shr_id_node_init(node, attr);
    }
    return node;
}

static void _shr_id_node_destroy(struct shr_id_node *node)
{
    trs_debug("Destroy. (devid=%u; tsid=%u; name=%s; type=%d; id=%u; flag=%d)\n", node->attr.inst.devid,
              node->attr.inst.tsid, node->attr.name, node->attr.type, node->attr.id, node->attr.flag);

    if (node->attr.res_put != NULL) {
        node->attr.res_put(&node->attr.inst, node->attr.res_type, node->attr.id, node->attr.get_hpid,
                           node->attr.get_seq);
    }
    ka_task_mutex_destroy(&node->priv_mutex);
    trs_kfree(node);
}

static struct shr_id_node *_shr_id_node_find(const char *name, int type)
{
    u32 key = get_elfhash_key(name);
    struct shr_id_node *node = NULL;

    ka_hash_for_each_possible(htable[type].htable, node, link, key)
    {
        if (ka_base_strcmp(name, node->attr.name) == 0) {
            return node;
        }
    }
    return NULL;
}

static void _shr_id_node_add(struct shr_id_node *node)
{
    ka_hash_add(htable[node->attr.type].htable, &node->link, node->key);
}

static inline void shr_id_get_opened_wlist_info(struct shr_id_node *node, struct shr_id_node_wlist *wlist, int *len)
{
    int idx = 0, i = 0;
    for (i = 0; (i < SHR_ID_PID_MAX_NUM) && (idx < *len); i++) {
        if ((node->wlist[i].pid != 0) && node->wlist[i].open_num != 0) {
            wlist[idx] = node->wlist[i];
            idx++;
        }
    }

    *len = idx;
}

static int shr_id_node_add(struct shr_id_node *node)
{
    struct shr_id_node *tmp_node = NULL;

    ka_task_write_lock(&htable[node->attr.type].lock);
    tmp_node = _shr_id_node_find(node->attr.name, node->attr.type);
    if (tmp_node != NULL) {
        struct shr_id_node_wlist tmp_wlist = {0};
        int wlist_num = 1;
        shr_id_get_opened_wlist_info(tmp_node, &tmp_wlist, &wlist_num);
        ka_task_write_unlock(&htable[node->attr.type].lock);
        trs_err("Add shr id node fail. (devid=%u; tsid=%u; name=%s; type=%d; id=%u; procs_num=%d; opened_pid=%d)\n",
                node->attr.inst.devid, node->attr.inst.tsid, node->attr.name, node->attr.type, node->attr.id, wlist_num,
                tmp_wlist.pid);
        return -EACCES;
    }
    _shr_id_node_add(node);
    ka_task_write_unlock(&htable[node->attr.type].lock);

    return 0;
}

static void _shr_id_node_del(struct shr_id_node *node)
{
    ka_hash_del(&node->link);
}

static void shr_id_node_del(struct shr_id_node *node)
{
    ka_task_write_lock(&htable[node->attr.type].lock);
    _shr_id_node_del(node);
    ka_task_write_unlock(&htable[node->attr.type].lock);
}

int shr_id_get_type_by_name(const char *name, int *id_type)
{
    u32 tgid, devid, tsid;
    size_t name_len;
    int ret;

    name_len = ka_base_strnlen(name, SHR_ID_NSM_NAME_SIZE);
    if ((name_len == 0) || (name_len >= SHR_ID_NSM_NAME_SIZE)) {
        trs_err("Length out of range. (name_len=%lu; valid range=[1, %u))\n", name_len, (u32)SHR_ID_NSM_NAME_SIZE);
        return -EINVAL;
    }

    ret = sscanf_s(name, "%08x %08x %08x %08x", &tgid, &devid, &tsid, id_type);
    if (ret != 4) { /* 4 for parse nums */
        trs_err("sscanf failed. (ret=%d)\n", ret);
        return -EINVAL;
    }

    if ((*id_type >= SHR_ID_TYPE_MAX) || (*id_type < 0)) {
        trs_err("Para invalid. (id_type=%u)\n", *id_type);
        return -EINVAL;
    }
    return 0;
}

int shr_id_node_create(struct shr_id_node_op_attr *attr)
{
    struct shr_id_node *node = NULL;
    int ret;

    node = _shr_id_node_create(attr);
    if (node == NULL) {
        return -ENODEV;
    }

    ret = shr_id_node_add(node);
    if (ret != 0) {
        _shr_id_node_destroy(node);
    } else {
        trs_debug("Create success. (devid=%u; tsid=%u; name=%s; type=%d; id=%u; size=%ld; flag=%u)\n", attr->inst.devid,
                  attr->inst.tsid, node->attr.name, attr->type, attr->id, sizeof(*node), attr->flag);
    }
    return ret;
}

static void shr_id_node_release(struct kref_safe *kref)
{
    struct shr_id_node *node = ka_container_of(kref, struct shr_id_node, ref);

    trs_debug("Release success. (devid=%u; tsid=%u; name=%s; type=%d; id=%u)\n", node->attr.inst.devid,
              node->attr.inst.tsid, node->attr.name, node->attr.type, node->attr.id);
    shr_id_node_del(node);
    _shr_id_node_destroy(node);
}

static struct shr_id_node *_shr_id_node_get(const char *name, int type)
{
    struct shr_id_node *node = NULL;
    size_t name_len;

    name_len = ka_base_strnlen(name, SHR_ID_NSM_NAME_SIZE);
    if ((name_len == 0) || (name_len >= SHR_ID_NSM_NAME_SIZE)) {
        trs_err("Length out of range. (name_len=%lu; valid range=[1, %u); type=%d)\n", name_len,
                (u32)SHR_ID_NSM_NAME_SIZE, type);
        return NULL;
    }

    if (type >= SHR_ID_TYPE_MAX || type < 0) {
        trs_err("Unknown shr id type. (type=%d)\n", type);
        return NULL;
    }

    ka_task_read_lock(&htable[type].lock);
    node = _shr_id_node_find(name, type);
    if (node != NULL) {
        if (kref_safe_get_unless_zero(&node->ref) == 0) {
            ka_task_read_unlock(&htable[type].lock);
            return NULL;
        }
    }
    ka_task_read_unlock(&htable[type].lock);

    return node;
}

static void _shr_id_node_put(struct shr_id_node *node)
{
    kref_safe_put(&node->ref, shr_id_node_release);
}

int shr_id_node_get_attr(const char *name, struct shr_id_node_op_attr *attr)
{
    struct shr_id_node *node = NULL;
    int ret, type;

    ret = shr_id_get_type_by_name(name, &type);
    if (ret != 0) {
        return ret;
    }

    node = _shr_id_node_get(name, type);
    if (node != NULL) {
        *attr = node->attr;
        _shr_id_node_put(node);
        return 0;
    }

    return -ENODEV;
}

void *shr_id_node_get_priv(void *node)
{
    struct shr_id_node *tmp_node = (struct shr_id_node *)node;
    return tmp_node->priv;
}

void shr_id_node_set_priv(void *node, void *priv)
{
    struct shr_id_node *tmp_node = (struct shr_id_node *)node;
    tmp_node->priv = priv;
}

void shr_id_node_priv_mutex_lock(void *node)
{
    struct shr_id_node *tmp_node = (struct shr_id_node *)node;
    ka_task_mutex_lock(&tmp_node->priv_mutex);
}

void shr_id_node_priv_mutex_unlock(void *node)
{
    struct shr_id_node *tmp_node = (struct shr_id_node *)node;
    ka_task_mutex_unlock(&tmp_node->priv_mutex);
}

void shr_id_node_spin_lock(void *node)
{
    struct shr_id_node *tmp_node = (struct shr_id_node *)node;
    ka_task_spin_lock(&tmp_node->lock);
}

void shr_id_node_spin_unlock(void *node)
{
    struct shr_id_node *tmp_node = (struct shr_id_node *)node;
    ka_task_spin_unlock(&tmp_node->lock);
}

bool shr_id_node_need_wlist(void *node)
{
    struct shr_id_node *tmp_node = (struct shr_id_node *)node;
    return tmp_node->need_wlist;
}

void *shr_id_node_get(const char *name, int type)
{
    return (void *)_shr_id_node_get(name, type);
}

void shr_id_node_put(void *node)
{
    struct shr_id_node *tmp_node = (struct shr_id_node *)node;
    kref_safe_put(&tmp_node->ref, shr_id_node_release);
}

static int _shr_id_node_find_wlist_index(struct shr_id_node *node, ka_pid_t pid)
{
    int i;

    for (i = 0; i < SHR_ID_PID_MAX_NUM; i++) {
        if (node->wlist[i].pid == pid) {
            return i;
        }
    }

    return SHR_ID_PID_MAX_NUM;
}

static int _shr_id_node_get_idle_wlist_index(struct shr_id_node *node)
{
    int i;

    for (i = 0; i < SHR_ID_PID_MAX_NUM; i++) {
        if (node->wlist[i].pid == 0) {
            return i;
        }
    }
    return SHR_ID_PID_MAX_NUM;
}

static int _shr_id_node_set_pid(struct shr_id_node *node, ka_pid_t pid)
{
    int idx = _shr_id_node_find_wlist_index(node, pid);
    if (idx == SHR_ID_PID_MAX_NUM) {
        idx = _shr_id_node_get_idle_wlist_index(node);
        if (idx == SHR_ID_PID_MAX_NUM) {
            return -EBUSY;
        }
    }

    node->wlist[idx].pid = pid;
    node->wlist[idx].set_time = ka_system_ktime_get_ns();
    return 0;
}

static int _shr_id_node_set_attr(struct shr_id_node *node, int pid)
{
    if (((node->need_wlist) && (node->wlist[TRS_SHR_ID_WLIST_FIRST_IDX].pid != 0)) || (node->attr.pid != pid) ||
        (node->priv != NULL)) {
        return -EPERM;
    }

    node->need_wlist = false;
    return 0;
}

static int _shr_id_node_set_pids(struct shr_id_node *node, ka_pid_t create_pid, ka_pid_t pid[], u32 pid_num)
{
    u32 i;

    if (node->attr.pid != create_pid) {
        /* Only creator process have permission to add pid to wlist */
        return -EINVAL;
    }

    if (!node->need_wlist) {
        return -EPERM;
    }

    for (i = 0; i < pid_num; i++) {
        int ret;
        if ((pid[i] == 0) || (node->attr.pid == pid[i])) {
            /* empty pid and creator process do not add to wlist */
            continue;
        }
        ret = _shr_id_node_set_pid(node, pid[i]);
        if (ret != 0) {
            return ret;
        }
    }
    return 0;
}

int shr_id_node_set_pids(const char *name, int type, ka_pid_t create_pid, ka_pid_t pid[], u32 pid_num)
{
    struct shr_id_node *node = _shr_id_node_get(name, type);
    int ret;

    if (node == NULL) {
        return -ENODEV;
    }

    ka_task_spin_lock(&node->lock);
    ret = _shr_id_node_set_pids(node, create_pid, pid, pid_num);
    ka_task_spin_unlock(&node->lock);

    if (ret != 0) {
        trs_err("Shr node can't set pid.(need_wlist=%d; create_pid=%d; current_pid=%d; ret=%d)\n", node->need_wlist,
                node->attr.pid, create_pid, ret);
    }
    _shr_id_node_put(node);

    return ret;
}

static int shr_id_node_record_check(struct shr_id_node *node, ka_pid_t pid)
{
    int idx;

    idx = _shr_id_node_find_wlist_index(node, pid);
    if (idx == SHR_ID_PID_MAX_NUM) {
        return -EBUSY;
    }

    if (node->wlist[idx].open_num == 0) {
        return -EACCES;
    }

    return 0;
}

static inline int _shr_id_node_record(struct shr_id_node *node)
{
    /* if shr_id_node create success, the node info would not be changed. So it doesn't need lock to prorect. */
    return trs_res_id_ctrl(&node->attr.inst, node->attr.res_type, node->attr.id, TRS_RES_OP_RECORD);
}

int shr_id_node_record(const char *name, int type, ka_pid_t pid)
{
    struct shr_id_node *node = _shr_id_node_get(name, type);
    int ret = -ENODEV;

    if (node != NULL) {
        ka_task_spin_lock(&node->lock);
        ret = shr_id_node_record_check(node, pid);
        ka_task_spin_unlock(&node->lock);
        if (ret == 0) {
            /* Do notify/event record only when it passes. */
            ret = _shr_id_node_record(node);
        }
        _shr_id_node_put(node);
    }

    return ret;
}

static int _shr_id_node_uninit(struct shr_id_node *node, ka_pid_t pid)
{
    /* Only creator process have permission to destroy */
    if (node->attr.pid != pid) {
        return -EACCES;
    }
    /* Set invalid creator pid to prevent multiple destroy */
    node->attr.pid = 0;
    return 0;
}

static int shr_id_destroy_node_handle(struct shr_id_node *node, u32 mode)
{
    if (shr_id_get_node_ops()->destory_node != NULL) {
        return shr_id_get_node_ops()->destory_node(node, mode);
    }

    trs_debug("Not support destroy handle. (devid=%u; tsid=%u; name=%s; type=%d)\n", node->attr.inst.devid,
              node->attr.inst.tsid, node->attr.name, node->attr.type);
    return 0;
}

int shr_id_node_set_attr(const char *name, int type, ka_pid_t pid)
{
    struct shr_id_node *node = NULL;
    int ret;

    node = _shr_id_node_get(name, type);
    if (node == NULL) {
        return -EINVAL;
    }
    ka_task_spin_lock(&node->lock);
    ret = _shr_id_node_set_attr(node, pid);
    ka_task_spin_unlock(&node->lock);

    if (ret == 0) {
        trs_info("Set attribute success.(name=%s)\n", name);
    } else {
        trs_err("Shr node can't set attr.(need_wlist=%d; create_pid=%d; current_pid=%d; ret=%d)\n", node->need_wlist,
                node->attr.pid, pid, ret);
    }
    _shr_id_node_put(node);
    return ret;
}

int shr_id_node_get_info(const char *name, int type, struct shr_id_ioctl_info *info)
{
    struct shr_id_node *node = NULL;

    node = _shr_id_node_get(name, type);
    if (node == NULL) {
        return -EINVAL;
    }

    info->devid = node->attr.inst.devid;
    info->tsid = node->attr.inst.tsid;
    info->id_type = (u32)node->attr.type;
    info->shr_id = (u32)node->attr.id;
    _shr_id_node_put(node);
    return 0;
}

int shr_id_node_get_attribute(const char *name, int type, struct shr_id_ioctl_info *info)
{
    struct shr_id_node *node = NULL;

    node = _shr_id_node_get(name, type);
    if (node == NULL) {
        return -EINVAL;
    }

    info->enable_flag = node->need_wlist ? SHRID_WLIST_ENABLE : SHRID_NO_WLIST_ENABLE;
    _shr_id_node_put(node);
    return 0;
}

int shr_id_node_destroy(const char *name, int type, ka_pid_t pid, u32 mode)
{
    struct shr_id_node *node = NULL;
    int ret;

    node = _shr_id_node_get(name, type);
    if (node == NULL) {
        /* in mc2 feature, node has been destroy */
        return 0;
    }
    ka_task_spin_lock(&node->lock);
    ret = _shr_id_node_uninit(node, pid);
    ka_task_spin_unlock(&node->lock);
    if (ret == 0) {
        /* for expand node_put */
        ret = shr_id_destroy_node_handle(node, mode);
        if (ret == 0) {
            /* Decrease ref when shr_id_node created. */
            _shr_id_node_put(node);
            trs_debug("Destroy success. (devid=%u; tsid=%u; name=%s; type=%d; id=%u; ref=%u)\n", node->attr.inst.devid,
                      node->attr.inst.tsid, node->attr.name, node->attr.type, node->attr.id,
                      kref_safe_read(&node->ref));
        } else {
#ifndef EMU_ST
            ka_task_spin_lock(&node->lock);
            node->attr.pid = pid;
            ka_task_spin_unlock(&node->lock);
#endif
        }
    }
    _shr_id_node_put(node);
    return ret;
}

static int _shr_id_node_open(struct shr_id_node *node, ka_pid_t pid, unsigned long start_time,
                             struct shr_id_node_op_attr *attr)
{
    int idx;

    idx = _shr_id_node_find_wlist_index(node, pid);
    if (idx == SHR_ID_PID_MAX_NUM) {
        if (node->need_wlist) {
            return -EACCES;
        } else {
            idx = _shr_id_node_get_idle_wlist_index(node);
            if (idx == SHR_ID_PID_MAX_NUM) {
                return -EBUSY;
            }
            node->wlist[idx].pid = pid;
            node->wlist[idx].set_time = ka_system_ktime_get_ns();
        }
    }

    /*
     * Prevent scenario described below:
     * Exit the process that opened shr_id_node and pull up a new process with the same pid.
     * The new process can open shr_id_node without authorization.
     */
    if (ka_system_time_after(start_time, (unsigned long)node->wlist[idx].set_time)) {
        return -ENODEV;
    }

    /* Repeated opening of a shr_id_node is allowed, but cannot exceed the maximum number of times */
    if (node->wlist[idx].open_num >= TRS_SHR_ID_NODE_MAX_OPEN_NUM) {
        return -EFAULT;
    }
    node->wlist[idx].open_num++;
    *attr = node->attr;

    return 0;
}

int shr_id_node_open(const char *name, ka_pid_t pid, unsigned long start_time, struct shr_id_node_op_attr *attr)
{
    struct shr_id_node *node = NULL;
    int ret;

    node = _shr_id_node_get(name, attr->type);
    if (node == NULL) {
        trs_err("Get shr_id_node fail. (name=%s; type=%d)\n", name, attr->type);
        return -ENODEV;
    }
    ka_task_spin_lock(&node->lock);
    ret = _shr_id_node_open(node, pid, start_time, attr);
    ka_task_spin_unlock(&node->lock);
    if (ret != 0) {
        trs_err("Open fail. (name=%s; type=%d; pid=%d; ret=%d; attr=%d)\n", name, attr->type, pid, ret,
                node->need_wlist);
        _shr_id_node_put(node);
    } else {
        trs_debug("Open success. (devid=%u; tsid=%u; name=%s; type=%d; id=%u)\n", attr->inst.devid, attr->inst.tsid,
                  name, attr->type, attr->id);
    }

    return ret;
}

static int _shr_id_node_close(struct shr_id_node *node, ka_pid_t pid)
{
    int idx;

    idx = _shr_id_node_find_wlist_index(node, pid);
    if (idx == SHR_ID_PID_MAX_NUM) {
        return -EBUSY;
    }

    if (node->wlist[idx].open_num == 0) {
        return -EACCES;
    }

    node->wlist[idx].open_num--;

    return 0;
}

int shr_id_node_close(const char *name, int type, ka_pid_t pid)
{
    struct shr_id_node *node = NULL;
    int ret = -ENODEV;

    node = _shr_id_node_get(name, type);
    if (node != NULL) {
        ka_task_spin_lock(&node->lock);
        ret = _shr_id_node_close(node, pid);
        ka_task_spin_unlock(&node->lock);
        if (ret == 0) {
            /* Decrease ref when shr_id_node opened. */
            _shr_id_node_put(node);
            trs_debug("Close success. (devid=%u; tsid=%u; name=%s; type=%d; id=%u)\n", node->attr.inst.devid,
                      node->attr.inst.tsid, name, type, node->attr.id);
        }
        _shr_id_node_put(node);
    }
    return ret;
}

void shr_id_node_init(void)
{
    int type;

    for (type = 0; type < SHR_ID_TYPE_MAX; type++) {
        ka_hash_init(htable[type].htable);
        ka_task_rwlock_init(&htable[type].lock);
    }
}
