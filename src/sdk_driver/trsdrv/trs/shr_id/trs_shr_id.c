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
#include "ka_base_pub.h"
#include "ka_task_pub.h"
#include "ka_list_pub.h"
#include "ka_memory_pub.h"
#include "ka_hashtable_pub.h"
#include "ka_system_pub.h"

#include "ascend_hal_define.h"
#include "securec.h"
#include "comm_kernel_interface.h"
#include "trs_core.h"

#include "trs_shr_id_node.h"
#include "trs_shr_id.h"
#include "trs_core_adapt.h"

#define TSDRV_WAKEUP_TIMEINTERVAL 500 /* 0.5s */

static KA_TASK_DEFINE_RWLOCK(proc_lock);

#define PROC_HASH_TABLE_BIT 4
#define PROC_HASH_TABLE_MASK ((1 << PROC_HASH_TABLE_BIT) - 1)
static KA_DECLARE_HASHTABLE(proc_htable, PROC_HASH_TABLE_BIT);

struct shr_id_proc_node {
    char name[SHR_ID_NSM_NAME_SIZE];
    int res_type;
    int type;
    ka_list_head_t list;
    struct trs_id_inst inst;
    int id;
    u32 ref;
    u32 flag; /* remote id */
    u32 opened_devid;
};

static int shr_id_type_trans[SHR_ID_TYPE_MAX] = {
    [SHR_ID_NOTIFY_TYPE] = TRS_NOTIFY,
    [SHR_ID_EVENT_TYPE] = TRS_EVENT,
};

static void shr_id_proc_num_inc(struct shr_id_proc_ctx *proc_ctx, int op, u32 num)
{
    (op == 0) ? (proc_ctx->create_node_num += num) : (proc_ctx->open_node_num += num);
}

static void shr_id_proc_num_dec(struct shr_id_proc_ctx *proc_ctx, int op, u32 num)
{
    (op == 0) ? (proc_ctx->create_node_num -= num) : (proc_ctx->open_node_num -= num);
}

static void _shr_id_proc_add(struct shr_id_proc_ctx *proc_ctx)
{
    int key = proc_ctx->pid & PROC_HASH_TABLE_MASK;

    ka_hash_add(proc_htable, &proc_ctx->link, key);
}

static void shr_id_hash_table_clean(struct shr_id_proc_ctx *proc_ctx)
{
    u32 bkt = 0;
    struct shr_id_hash_node *cur = NULL;
    ka_hlist_node_t *tmp = NULL;
    ka_hash_for_each_safe(proc_ctx->abnormal_ht.htable, bkt, tmp, cur, link)
    {
        ka_hash_del(&cur->link);
        ka_mm_kfree(cur);
    }
}

static void _shr_id_proc_del(struct shr_id_proc_ctx *proc_ctx)
{
    ka_task_write_lock(&proc_lock);
    ka_hash_del(&proc_ctx->link);
    ka_task_write_unlock(&proc_lock);
}

struct shr_id_proc_ctx *shr_id_proc_create(ka_pid_t pid)
{
    size_t ctx_size = sizeof(struct shr_id_proc_ctx);
    struct shr_id_proc_ctx *proc_ctx = NULL;

    proc_ctx = (struct shr_id_proc_ctx *)trs_kzalloc(ctx_size, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (proc_ctx == NULL) {
        trs_err("Kmalloc fail. (size=%ld)\n", ctx_size);
        return NULL;
    }

    proc_ctx->pid = ka_task_get_current_tgid();
    proc_ctx->start_time = ka_task_get_current_group_starttime();
    kref_safe_init(&proc_ctx->ref);
    KA_INIT_LIST_HEAD(&proc_ctx->create_list_head);
    KA_INIT_LIST_HEAD(&proc_ctx->open_list_head);
    ka_task_rwlock_init(&proc_ctx->lock);
    ka_task_mutex_init(&proc_ctx->mutex);
    ka_base_atomic_set(&proc_ctx->proc_in_release, 0);
    ka_hash_init(proc_ctx->abnormal_ht.htable);
    ka_task_rwlock_init(&proc_ctx->abnormal_ht.lock);
    return proc_ctx;
}

void shr_id_proc_destroy(struct shr_id_proc_ctx *proc_ctx)
{
    shr_id_hash_table_clean(proc_ctx);
    ka_task_mutex_destroy(&proc_ctx->mutex);
    trs_kfree(proc_ctx);
}

struct shr_id_proc_ctx *shr_id_find_proc(ka_pid_t pid)
{
    struct shr_id_proc_ctx *proc_ctx = NULL;
    int key = pid & PROC_HASH_TABLE_MASK;

    ka_hash_for_each_possible(proc_htable, proc_ctx, link, key)
    {
        if (proc_ctx->pid == pid) {
            return proc_ctx;
        }
    }
    return NULL;
}

static struct shr_id_proc_node *shr_id_creat_proc_node(const char *name, struct shr_id_node_op_attr *attr)
{
    struct shr_id_proc_node *proc_node = NULL;
    int i;

    proc_node = trs_kvzalloc(sizeof(struct shr_id_proc_node), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (proc_node == NULL) {
        return NULL;
    }

    proc_node->res_type = attr->res_type;
    proc_node->inst = attr->inst;
    proc_node->id = (int)attr->id;
    proc_node->type = attr->type;
    for (i = 0; i < SHR_ID_NSM_NAME_SIZE; i++) {
        proc_node->name[i] = name[i];
    }
    proc_node->ref = 1;

    return proc_node;
}

static void shr_id_destroy_proc_node(struct shr_id_proc_node *proc_node)
{
    if (proc_node != NULL) {
        trs_kvfree(proc_node);
    }
}

static void shr_id_recycle_opened(struct shr_id_proc_ctx *proc_ctx)
{
    struct shr_id_proc_node *proc_node = NULL, *n = NULL;
    int ret = 0;
    u32 i;

    trs_debug("Opened start recycle. (pid=%d; open_num=%u)\n", proc_ctx->pid, proc_ctx->open_node_num);

    ka_list_for_each_entry_safe(proc_node, n, &proc_ctx->open_list_head, list)
    {
        for (i = 0; i < proc_node->ref; i++) {
            ret |= shr_id_node_close(proc_node->name, proc_node->type, proc_ctx->pid);

            if (ka_task_need_resched()) {
                ka_task_cond_resched();
            }
        }

        if (ret == 0) {
            shr_id_proc_num_dec(proc_ctx, 1, proc_node->ref);
        } else {
            trs_warn("Opened recycle warn. (name=%s; type=%d; ret=%d)\n", proc_node->name, proc_node->type, ret);
        }
        ka_list_del(&proc_node->list);
        shr_id_destroy_proc_node(proc_node);
        trs_debug("Opened recycle. (pid=%d; open_num=%u)\n", proc_ctx->pid, proc_ctx->open_node_num);
    }
}

static void tsdrv_try_cond_resched_by_time(u32 *pre_stamp, u32 time)
{
    u32 timeinterval;

    timeinterval = ka_system_jiffies_to_msecs(ka_jiffies - *pre_stamp);
    if ((timeinterval > time) && !ka_base_in_atomic()) {
        ka_task_cond_resched();
        *pre_stamp = (u32)ka_jiffies;
    }
}

static void tsdrv_try_cond_resched(u32 *pre_stamp)
{
    tsdrv_try_cond_resched_by_time(pre_stamp, TSDRV_WAKEUP_TIMEINTERVAL);
}

static void shr_id_recycle_created(struct shr_id_proc_ctx *proc_ctx)
{
    struct shr_id_proc_node *proc_node = NULL, *n = NULL;
    u32 stamp = (u32)ka_jiffies;
    trs_debug("Create start recycle. (pid=%d; create_num=%u)\n", proc_ctx->pid, proc_ctx->create_node_num);

    ka_list_for_each_entry_safe(proc_node, n, &proc_ctx->create_list_head, list)
    {
#ifdef CFG_FEATURE_SUPPORT_XCOM
        int ret = shr_id_node_destroy(proc_node->name, proc_node->type, proc_ctx->pid, DEVDRV_S2S_SYNC_MODE);
#else
        int ret = shr_id_node_destroy(proc_node->name, proc_node->type, proc_ctx->pid, DEVDRV_S2S_ASYNC_MODE);
#endif
        if (ret == 0) {
            shr_id_proc_num_dec(proc_ctx, 0, proc_node->ref);
        } else {
            trs_warn("Create recycle warn. (name=%s; type=%d; ret=%d)\n", proc_node->name, proc_node->type, ret);
        }
        ka_list_del(&proc_node->list);
        shr_id_destroy_proc_node(proc_node);
        tsdrv_try_cond_resched(&stamp);
    }
}

static void shr_id_node_recycle(struct shr_id_proc_ctx *proc_ctx)
{
    shr_id_recycle_opened(proc_ctx);
    shr_id_recycle_created(proc_ctx);
}

static void shr_id_proc_release(struct kref_safe *kref)
{
    struct shr_id_proc_ctx *proc_ctx = ka_container_of(kref, struct shr_id_proc_ctx, ref);

    /*
     * After the recovery is complete, delete the process context from the hash table
     * This sequence cooperates with shr_id_wait_for_proc_exit to ensure that
     * only one process with the same pid exists at the same time
     */
    ka_base_atomic_set(&proc_ctx->proc_in_release, 1);
    shr_id_node_recycle(proc_ctx);
    _shr_id_proc_del(proc_ctx);
    shr_id_proc_destroy(proc_ctx);
}

static struct shr_id_proc_ctx *shr_id_proc_get(ka_pid_t pid)
{
    struct shr_id_proc_ctx *proc_ctx = NULL;

    ka_task_read_lock(&proc_lock);
    proc_ctx = shr_id_find_proc(pid);
    if (proc_ctx != NULL) {
        if (kref_safe_get_unless_zero(&proc_ctx->ref) == 0) {
            ka_task_read_unlock(&proc_lock);
            return NULL;
        }
    }
    ka_task_read_unlock(&proc_lock);
    return proc_ctx;
}

/*
 * When a process's reference count reaches zero, it triggers the release process,
 * making it impossible to get process.
 */
struct shr_id_proc_ctx *shr_id_proc_ctx_find(ka_pid_t pid)
{
    struct shr_id_proc_ctx *proc_ctx = NULL;

    ka_task_read_lock(&proc_lock);
    proc_ctx = shr_id_find_proc(pid);
    ka_task_read_unlock(&proc_lock);
    return proc_ctx;
}

static void shr_id_proc_put(struct shr_id_proc_ctx *proc_ctx)
{
    kref_safe_put(&proc_ctx->ref, shr_id_proc_release);
}

static int shr_id_name_generate(struct trs_id_inst *inst, int pid, int id_type, u32 shr_id, char *name)
{
    int offset = snprintf_s(name, SHR_ID_NSM_NAME_SIZE, SHR_ID_NSM_NAME_SIZE - 1, "%08x%08x%08x%08x%08x", pid,
                            inst->devid, inst->tsid, id_type, shr_id);
    if (offset < 0) {
        trs_err("Snprintf failed. (offset=%d)\n", offset);
        return -EINVAL;
    }
    name[offset] = '\0';

    return 0;
}

static ka_list_head_t *shr_id_get_list_head(struct shr_id_proc_ctx *proc_ctx, int op)
{
    return (op == 0) ? &proc_ctx->create_list_head : &proc_ctx->open_list_head;
}

static struct shr_id_proc_node *shr_id_find_proc_node(struct shr_id_proc_ctx *proc_ctx, const char *name, int op)
{
    struct shr_id_proc_node *proc_node = NULL, *n = NULL;
    ka_list_head_t *head;

    head = shr_id_get_list_head(proc_ctx, op);
    ka_task_read_lock(&proc_ctx->lock);
    ka_list_for_each_entry_safe(proc_node, n, head, list)
    {
        if (ka_base_strcmp(proc_node->name, name) == 0) {
            ka_task_read_unlock(&proc_ctx->lock);
            return proc_node;
        }
    }
    ka_task_read_unlock(&proc_ctx->lock);

    return NULL;
}

static int shr_id_add_to_proc(struct shr_id_proc_ctx *proc_ctx, struct shr_id_ioctl_info *ioctl_info,
                              struct shr_id_node_op_attr *attr, int op)
{
    struct shr_id_proc_node *proc_node = NULL;

    ka_task_mutex_lock(&proc_ctx->mutex);
    proc_node = shr_id_find_proc_node(proc_ctx, ioctl_info->name, op);
    if (proc_node != NULL) {
        if ((ioctl_info->flag & TSDRV_FLAG_REMOTE_ID) != 0) {
            ioctl_info->flag &= ~TSDRV_FLAG_REMOTE_ID;
        }
        if ((attr->flag & TSDRV_FLAG_SHR_ID_SHADOW) != 0) {
            ioctl_info->flag |= TSDRV_FLAG_SHR_ID_SHADOW;
        }
        proc_node->ref++;
        shr_id_proc_num_inc(proc_ctx, op, 1);
        ka_task_mutex_unlock(&proc_ctx->mutex);
        return 0;
    }

    proc_node = shr_id_creat_proc_node(ioctl_info->name, attr);
    if (proc_node == NULL) {
        ka_task_mutex_unlock(&proc_ctx->mutex);
        return -ENOMEM;
    }

    if ((ioctl_info->flag & TSDRV_FLAG_REMOTE_ID) != 0) {
        proc_node->opened_devid = ioctl_info->opened_devid;
        proc_node->flag |= TSDRV_FLAG_REMOTE_ID;
        ioctl_info->flag |= TSDRV_FLAG_REMOTE_ID;
    }

    if ((attr->flag & TSDRV_FLAG_SHR_ID_SHADOW) != 0) {
        proc_node->flag |= TSDRV_FLAG_SHR_ID_SHADOW;
        ioctl_info->flag |= TSDRV_FLAG_SHR_ID_SHADOW;
    }

    trs_debug("Add info. (devid=%u; tsid=%u; type=%d; id=%u; flag=%u; node_flag=0x%x; op=%d)\n", attr->inst.devid,
              attr->inst.tsid, attr->res_type, attr->id, ioctl_info->flag, proc_node->flag, op);

    ka_task_write_lock(&proc_ctx->lock);
    ka_list_add_tail(&proc_node->list, shr_id_get_list_head(proc_ctx, op));
    ka_task_write_unlock(&proc_ctx->lock);
    shr_id_proc_num_inc(proc_ctx, op, 1);
    ka_task_mutex_unlock(&proc_ctx->mutex);

    return 0;
}

static void shr_id_del_from_proc(struct shr_id_proc_ctx *proc_ctx, struct shr_id_ioctl_info *ioctl_info, int op)
{
    struct shr_id_proc_node *proc_node = NULL;

    ka_task_mutex_lock(&proc_ctx->mutex);
    proc_node = shr_id_find_proc_node(proc_ctx, ioctl_info->name, op);
    if (proc_node == NULL) {
        ka_task_mutex_unlock(&proc_ctx->mutex);
        return;
    }

    trs_debug("Del info. (devid=%u; tsid=%u; type=%d; id=%u; flag=%u; node_flag=%u; op=%d)\n", proc_node->inst.devid,
              proc_node->inst.devid, proc_node->type, proc_node->id, ioctl_info->flag, proc_node->flag, op);

    shr_id_proc_num_dec(proc_ctx, op, 1);
    proc_node->ref--;
    if (proc_node->ref == 0) {
        if ((proc_node->flag & TSDRV_FLAG_REMOTE_ID) != 0) {
            ioctl_info->opened_devid = proc_node->opened_devid;
            ioctl_info->flag |= TSDRV_FLAG_REMOTE_ID;
            ioctl_info->devid = proc_node->inst.devid;
            ioctl_info->tsid = proc_node->inst.tsid;
            ioctl_info->id_type = (u32)proc_node->type;
            ioctl_info->shr_id = (u32)proc_node->id;
            if ((proc_node->flag & TSDRV_FLAG_SHR_ID_SHADOW) != 0) {
                ioctl_info->flag |= TSDRV_FLAG_SHR_ID_SHADOW;
            }
        }
        /* shr id close don't need check shr id. */
        ka_task_write_lock(&proc_ctx->lock);
        ka_list_del(&proc_node->list);
        ka_task_write_unlock(&proc_ctx->lock);
        shr_id_destroy_proc_node(proc_node);
    }
    ka_task_mutex_unlock(&proc_ctx->mutex);
}

#define SHR_ID_MAX_WAIT_PROC_EXIT_CNT 3U
int shr_id_wait_for_proc_exit(ka_pid_t pid)
{
    int loop = 0;

    do {
        struct shr_id_proc_ctx *proc_ctx = NULL;

        ka_task_read_lock(&proc_lock);
        proc_ctx = shr_id_find_proc(pid);
        ka_task_read_unlock(&proc_lock);
        if (proc_ctx == NULL) {
            return 0;
        }
        ka_system_usleep_range(USEC_PER_MSEC - 100, USEC_PER_MSEC); /* 1ms, range 100 us */
    } while (++loop < SHR_ID_MAX_WAIT_PROC_EXIT_CNT);

    return -EBUSY;
}

int shr_id_proc_add(struct shr_id_proc_ctx *proc_ctx)
{
    ka_task_write_lock(&proc_lock);
    if (shr_id_find_proc(proc_ctx->pid) != NULL) {
        ka_task_write_unlock(&proc_lock);
        trs_err("Proc add fail. (pid=%d)\n", proc_ctx->pid);
        return -EINVAL;
    }
    _shr_id_proc_add(proc_ctx);
    ka_task_write_unlock(&proc_lock);
    return 0;
}

void shr_id_proc_del(struct shr_id_proc_ctx *proc_ctx)
{
    shr_id_proc_put(proc_ctx);
}

bool shr_id_is_belong_to_proc(struct trs_id_inst *inst, int pid, int res_type, u32 res_id)
{
    struct shr_id_proc_node *proc_node = NULL, *n = NULL;
    struct shr_id_proc_ctx *proc_ctx = NULL;
    bool is_belong = false;

    proc_ctx = shr_id_proc_get(pid);
    if (proc_ctx == NULL) {
        return false;
    }

    ka_task_read_lock(&proc_ctx->lock);
    ka_list_for_each_entry_safe(proc_node, n, &proc_ctx->open_list_head, list)
    {
        if ((proc_node->inst.devid == inst->devid) && (proc_node->inst.tsid == inst->tsid) &&
            (proc_node->res_type == res_type) && (proc_node->id == res_id)) {
            is_belong = true;
            break;
        }
    }

    /* for mc2 feature, shr id alloced by device cp, but host app need notify wait,
     * so host app need call halShridCreate.
     */
    ka_list_for_each_entry_safe(proc_node, n, &proc_ctx->create_list_head, list)
    {
        if ((proc_node->inst.devid == inst->devid) && (proc_node->inst.tsid == inst->tsid) &&
            (proc_node->res_type == res_type) && (proc_node->id == res_id)) {
            is_belong = true;
            break;
        }
    }

    ka_task_read_unlock(&proc_ctx->lock);
    shr_id_proc_put(proc_ctx);

    trs_debug("Id info. (devid=%u; tsid=%u; pid=%d; type=%d; shrid=%u; is=%u)\n", inst->devid, inst->tsid,
              ka_task_get_current_tgid(), res_type, res_id, is_belong);

    return is_belong;
}

static int shr_id_res_get(struct trs_id_inst *inst, int res_type, u32 res_id, u32 hpid, u32 seq)
{
    (void)hpid;
    (void)seq;
    return trs_res_id_get(inst, res_type, res_id);
}

static int shr_id_res_put(struct trs_id_inst *inst, int res_type, u32 res_id, u32 hpid, u32 seq)
{
    (void)hpid;
    (void)seq;
    return trs_res_id_put(inst, res_type, res_id);
}

static int shr_id_reomote_res_get(struct trs_id_inst *inst, int res_type, u32 res_id, u32 hpid, u32 seq)
{
    return trs_res_id_remote_get(inst, res_type, res_id, hpid, seq);
}

static int shr_id_reomote_res_put(struct trs_id_inst *inst, int res_type, u32 res_id, u32 hpid, u32 seq)
{
    return trs_res_id_remote_put(inst, res_type, res_id, hpid, seq);
}

static int shr_id_set_stars_die_id(u32 devid, u32 tsid, struct shr_id_node_op_attr *attr)
{
    int ret;

    ret = trs_core_get_stars_die_id(devid, tsid, &attr->stars_die_id);
    if (ret != 0) {
        trs_err("Set stars die id failed. (devid=%u; tsid=%u; ret=%d)\n", devid, tsid, ret);
        return -EINVAL;
    }
    trs_debug("Set stars die id. (devid=%u; tsid=%u; stars_die_id=%d)\n", devid, tsid, attr->stars_die_id);
    return 0;
}

static int shr_id_node_attr_pack(struct shr_id_node_op_attr *attr, ka_pid_t pid, u64 start_time,
                                 struct shr_id_ioctl_info *ioctl_info)
{
    u32 type = ioctl_info->id_type;
    int ret, i;

    attr->res_type = shr_id_type_trans[type];
    attr->type = (int)type;
    attr->id = ioctl_info->shr_id;
    attr->flag = ioctl_info->flag;
    attr->pid = pid;
    attr->start_time = start_time;

    if ((attr->flag & TSDRV_FLAG_REMOTE_ID) != 0) {
        attr->res_get = shr_id_reomote_res_get;
        attr->res_put = shr_id_reomote_res_put;
    } else {
        attr->res_get = shr_id_res_get;
        attr->res_put = shr_id_res_put;
    }
    ret = shr_id_set_stars_die_id(ioctl_info->devid, ioctl_info->tsid, attr);
    if (ret != 0) {
        return ret;
    }

    trs_id_inst_pack(&attr->inst, ioctl_info->devid, ioctl_info->tsid);
    ret = shr_id_name_generate(&attr->inst, pid, (int)type, ioctl_info->shr_id, attr->name);
    if (ret != 0) {
        return ret;
    }

    ret = shr_id_name_update(attr->inst.devid, attr->name);
    if (ret != 0) {
        return ret;
    }

    for (i = 0; i < SHR_ID_NSM_NAME_SIZE; i++) {
        ioctl_info->name[i] = attr->name[i];
    }
    return ret;
}

int shr_id_create(struct shr_id_proc_ctx *proc_ctx, unsigned long arg)
{
    struct shr_id_ioctl_info ioctl_info;
    struct shr_id_node_op_attr attr = {{0}};
    int ret;

    if (ka_base_copy_from_user(&ioctl_info, (void *)(uintptr_t)arg, sizeof(struct shr_id_ioctl_info)) != 0) {
        trs_err("Copy from user fail.\n");
        return -EFAULT;
    }

    if (uda_can_access_udevid(ioctl_info.devid) == false) {
        trs_err("Device is not in container. (devid=%u)\n", ioctl_info.devid);
        return -EINVAL;
    }

    if (ioctl_info.id_type >= SHR_ID_TYPE_MAX) {
        trs_err("Para invalid. (devid=%u; tsid=%u; idtype=%u)\n", ioctl_info.devid, ioctl_info.tsid,
                ioctl_info.id_type);
        return -EINVAL;
    }

    ret = shr_id_node_attr_pack(&attr, proc_ctx->pid, proc_ctx->start_time, &ioctl_info);
    if (ret != 0) {
        return ret;
    }

    ret = shr_id_node_create(&attr);
    if (ret != 0) {
        return ret;
    }

    ret = shr_id_add_to_proc(proc_ctx, &ioctl_info, &attr, 0);
    if (ret != 0) {
        /* res_id will be put when shr_id_node destroyed */
        (void)shr_id_node_destroy(ioctl_info.name, (int)ioctl_info.id_type, proc_ctx->pid, DEVDRV_S2S_SYNC_MODE);
        return ret;
    }

    if (ka_base_copy_to_user((void *)(uintptr_t)arg, &ioctl_info, sizeof(struct shr_id_ioctl_info)) != 0) {
        trs_err("Copy to user fail.\n");
        shr_id_del_from_proc(proc_ctx, &ioctl_info, 0);
        (void)shr_id_node_destroy(ioctl_info.name, (int)ioctl_info.id_type, proc_ctx->pid, DEVDRV_S2S_SYNC_MODE);
        return -EFAULT;
    }
    return ret;
}

int shr_id_destroy(struct shr_id_proc_ctx *proc_ctx, unsigned long arg)
{
    struct shr_id_ioctl_info ioctl_info;
    int ret, type;

    if (ka_base_copy_from_user(&ioctl_info, (void *)(uintptr_t)arg, sizeof(struct shr_id_ioctl_info)) != 0) {
        trs_err("Copy from user fail.\n");
        return -EFAULT;
    }

    ret = shr_id_get_type_by_name(ioctl_info.name, &type);
    if (ret != 0) {
        return ret;
    }

    ret = shr_id_node_destroy(ioctl_info.name, type, proc_ctx->pid, DEVDRV_S2S_SYNC_MODE);
    if (ret != 0) {
        trs_err("Destroy shr id node fail. (type=%d; ret=%d)\n", type, ret);
        return ret;
    }
    shr_id_del_from_proc(proc_ctx, &ioctl_info, 0);
    return 0;
}

int shr_id_set_attr(struct shr_id_proc_ctx *proc_ctx, unsigned long arg)
{
    struct shr_id_ioctl_info ioctl_info;
    int ret, type;

    ret = ka_base_copy_from_user(&ioctl_info, (void *)(uintptr_t)arg, sizeof(struct shr_id_ioctl_info));
    if (ret != 0) {
        trs_err("Copy from user fail.\n");
        return -EFAULT;
    }

    ret = shr_id_get_type_by_name(ioctl_info.name, &type);
    if (ret != 0) {
        return ret;
    }

    ret = shr_id_node_set_attr(ioctl_info.name, type, proc_ctx->pid);
    return ret;
}

int shr_id_get_attr(struct shr_id_proc_ctx *proc_ctx, unsigned long arg)
{
    struct shr_id_ioctl_info ioctl_info = {0};
    int ret, type;

    ret = ka_base_copy_from_user(&ioctl_info, (void *)(uintptr_t)arg, sizeof(struct shr_id_ioctl_info));
    if (ret != 0) {
        trs_err("Copy from user fail.\n");
        return -EFAULT;
    }

    ret = shr_id_get_type_by_name(ioctl_info.name, &type);
    if (ret != 0) {
        return ret;
    }

    ret = shr_id_node_get_attribute(ioctl_info.name, type, &ioctl_info);
    if (ret != 0) {
        trs_err("Get shr id info fail.\n");
        return ret;
    }

    ret = ka_base_copy_to_user((void *)(uintptr_t)arg, &ioctl_info, sizeof(struct shr_id_ioctl_info));
    if (ret != 0) {
        trs_err("Copy to user fail.\n");
        return -EFAULT;
    }
    return 0;
}

int shr_id_get_info(struct shr_id_proc_ctx *proc_ctx, unsigned long arg)
{
    struct shr_id_ioctl_info ioctl_info = {0};
    int ret, type;

    ret = ka_base_copy_from_user(&ioctl_info, (void *)(uintptr_t)arg, sizeof(struct shr_id_ioctl_info));
    if (ret != 0) {
        trs_err("Copy from user fail.\n");
        return -EFAULT;
    }

    ret = shr_id_get_type_by_name(ioctl_info.name, &type);
    if (ret != 0) {
        return ret;
    }

    ret = shr_id_node_get_info(ioctl_info.name, type, &ioctl_info);
    if (ret != 0) {
        trs_err("Get shr id info fail.\n");
        return ret;
    }

    ret = ka_base_copy_to_user((void *)(uintptr_t)arg, &ioctl_info, sizeof(struct shr_id_ioctl_info));
    if (ret != 0) {
        trs_err("Copy to user fail.\n");
        return -EFAULT;
    }
    return 0;
}

/* open func passed to the user the physical id */
int shr_id_open(struct shr_id_proc_ctx *proc_ctx, unsigned long arg)
{
    struct shr_id_ioctl_info ioctl_info;
    struct shr_id_node_op_attr attr;
    int ret, type;

    if (ka_base_copy_from_user(&ioctl_info, (void *)(uintptr_t)arg, sizeof(struct shr_id_ioctl_info)) != 0) {
        trs_err("Copy from user fail.\n");
        return -EFAULT;
    }

    ret = shr_id_get_type_by_name(ioctl_info.name, &type);
    if (ret != 0) {
        return ret;
    }

    attr.res_type = shr_id_type_trans[type];
    attr.type = type;
    ret = shr_id_node_open(ioctl_info.name, proc_ctx->pid, proc_ctx->start_time, &attr);
    if (ret != 0) {
        return ret;
    }

    ret = shr_id_add_to_proc(proc_ctx, &ioctl_info, &attr, 1);
    if (ret != 0) {
        (void)shr_id_node_close(ioctl_info.name, type, proc_ctx->pid);
        return ret;
    }

    ioctl_info.devid = attr.inst.devid;
    ioctl_info.tsid = attr.inst.tsid;
    ioctl_info.id_type = (u32)attr.type;
    ioctl_info.shr_id = (u32)attr.id;
    ioctl_info.rsv[0] = attr.stars_die_id;

    if (ka_base_copy_to_user((void *)(uintptr_t)arg, &ioctl_info, sizeof(struct shr_id_ioctl_info)) != 0) {
        trs_err("Copy to user fail.\n");
        shr_id_del_from_proc(proc_ctx, &ioctl_info, 1);
        (void)shr_id_node_close(ioctl_info.name, type, proc_ctx->pid);
        return -EFAULT;
    }
    return 0;
}

int shr_id_close(struct shr_id_proc_ctx *proc_ctx, unsigned long arg)
{
    struct shr_id_ioctl_info ioctl_info;
    int ret, type;

    if (ka_base_copy_from_user(&ioctl_info, (void *)(uintptr_t)arg, sizeof(struct shr_id_ioctl_info)) != 0) {
        trs_err("Copy from user fail.\n");
        return -EFAULT;
    }

    ret = shr_id_get_type_by_name(ioctl_info.name, &type);
    if (ret != 0) {
        return ret;
    }

    ret = shr_id_node_close(ioctl_info.name, type, proc_ctx->pid);
    if (ret != 0) {
        return ret;
    }
    shr_id_del_from_proc(proc_ctx, &ioctl_info, 1);

    if (ka_base_copy_to_user((void *)(uintptr_t)arg, &ioctl_info, sizeof(struct shr_id_ioctl_info)) != 0) {
        trs_err("Copy to user fail.\n");
        return -EFAULT;
    }
    return 0;
}

int shr_id_set_pid(struct shr_id_proc_ctx *proc_ctx, unsigned long arg)
{
    struct shr_id_ioctl_info ioctl_info;
    int ret, type;

    if (ka_base_copy_from_user(&ioctl_info, (void *)(uintptr_t)arg, sizeof(struct shr_id_ioctl_info)) != 0) {
        trs_err("Copy from user fail.\n");
        return -EFAULT;
    }

    ret = shr_id_get_type_by_name(ioctl_info.name, &type);
    if (ret != 0) {
        return ret;
    }
    return shr_id_node_set_pids(ioctl_info.name, type, proc_ctx->pid, ioctl_info.pid, SHR_ID_PID_MAX_NUM);
}

int shr_id_record(struct shr_id_proc_ctx *proc_ctx, unsigned long arg)
{
    struct shr_id_ioctl_info ioctl_info;
    int ret, type;

    if (ka_base_copy_from_user(&ioctl_info, (void *)(uintptr_t)arg, sizeof(struct shr_id_ioctl_info)) != 0) {
        trs_err("Copy from user fail.\n");
        return -EFAULT;
    }

    ret = shr_id_get_type_by_name(ioctl_info.name, &type);
    if (ret != 0) {
        return ret;
    }
    ret = shr_id_node_record(ioctl_info.name, type, proc_ctx->pid);
    if (ret != 0) {
        return ret;
    }
    return 0;
}
/*
 return val:
 true: shrid is destroyed by ioctl cmd: SHR_ID_DESTROY
 false: shrid is destroyed by shr_id_file_release
*/
bool shr_id_is_destroyed_in_process(int pid, unsigned long start_time)
{
    struct shr_id_proc_ctx *proc_ctx = NULL;
    bool in_process = false;
    proc_ctx = shr_id_proc_get(pid);
    if (proc_ctx == NULL) {
        return false;
    }

    if (ka_system_time_before_eq((unsigned long)proc_ctx->start_time, start_time)) {
        if (ka_base_atomic_read(&proc_ctx->proc_in_release) == 0) {
            in_process = true;
        }
    }
    shr_id_proc_put(proc_ctx);
    return in_process;
}