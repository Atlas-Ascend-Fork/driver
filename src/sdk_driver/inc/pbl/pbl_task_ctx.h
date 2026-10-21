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

#ifndef PBL_TASK_CTX_H
#define PBL_TASK_CTX_H
#include "ka_task_pub.h"
#include "ka_hashtable_pub.h"
#include "ka_list_pub.h"
#include "ka_system_pub.h"
#include "ka_base_pub.h"
#include "ka_fs_pub.h"
#include "ka_memory_pub.h"

#include "securec.h"
#include "pbl_kref_safe.h"

/*
   usage : Process Context Management with reference counting
        1. call task_ctx_domain_create create a domain when module_init
        2. call task_ctx_create create a task context in the domain when process open a file
        3. call task_ctx_get and task_ctx_put in business
        4. call task_ctx_destroy when process exit or close a file
        5. call task_ctx_for_each_safe traverse a domain
        6. task_ctx_domain_show
        7. call task_ctx_domain_destroy destroy domain when module_exit
*/

#define TASK_CTX_HASH_BIT 6
#define TASK_CTX_HASH_MAX (1U << TASK_CTX_HASH_BIT)
#define TASK_CTX_DOMAIN_FLAG_ACTIVE (1ULL << 0ULL)

struct task_ctx_domain {
    struct kref_safe ref;
    KA_DECLARE_HASHTABLE(ctx_htable, TASK_CTX_HASH_BIT);
    ka_rwlock_t ctx_lock[TASK_CTX_HASH_MAX];
    ka_mutex_t mutex;
    const char *name;
    ka_atomic_t task_num;
    u32 task_max_num;
    u64 flags;
};

struct task_start_time {
    ka_system_timespec time;
};

struct task_id_entity {
    int tgid;
    struct task_start_time start_time;
};

struct task_ctx {
    struct kref_safe ref;
    struct task_start_time start_time;
    int tgid;
    char name[TASK_COMM_LEN];
    struct hlist_node link;
    ka_mutex_t mutex;
    spinlock_t lock;
    struct task_ctx_domain *domain;
    void *priv;
    void (*release)(struct task_ctx *ctx);
};

static inline bool task_ctx_domain_is_active(struct task_ctx_domain *domain)
{
    return ((domain->flags & TASK_CTX_DOMAIN_FLAG_ACTIVE) != 0);
}

/* interface */
static inline void task_ctx_domain_set_active(struct task_ctx_domain *domain)
{
    ka_task_mutex_lock(&domain->mutex);
    domain->flags |= TASK_CTX_DOMAIN_FLAG_ACTIVE;
    ka_task_mutex_unlock(&domain->mutex);
}

/* interface */
static inline int task_ctx_domain_set_inactive(struct task_ctx_domain *domain)
{
    ka_task_mutex_lock(&domain->mutex);
    if (ka_base_atomic_read(&domain->task_num) != 0) {
        ka_task_mutex_unlock(&domain->mutex);
        return -EBUSY;
    }

    domain->flags &= (~TASK_CTX_DOMAIN_FLAG_ACTIVE);
    ka_task_mutex_unlock(&domain->mutex);
    return 0;
}

/* task_max_num=0, Not restricted */
static inline void task_ctx_domain_init(struct task_ctx_domain *domain, const char *name, u32 task_max_num)
{
    u32 i;

    domain->name = name;
    domain->task_max_num = task_max_num;
    domain->flags = TASK_CTX_DOMAIN_FLAG_ACTIVE;
    ka_hash_init(domain->ctx_htable);
    ka_task_mutex_init(&domain->mutex);
    for (i = 0; i < TASK_CTX_HASH_MAX; i++) {
        ka_task_rwlock_init(&domain->ctx_lock[i]);
    }
}

/* interface */
static inline struct task_ctx_domain *task_ctx_domain_create(const char *name, u32 task_max_num)
{
    struct task_ctx_domain *domain = NULL;

    domain = ka_mm_vzalloc(sizeof(*domain));
    if (domain == NULL) {
        return NULL;
    }

    task_ctx_domain_init(domain, name, task_max_num);

    return domain;
}

/* interface */
static inline void task_ctx_domain_destroy(struct task_ctx_domain *domain)
{
    ka_task_mutex_destroy(&domain->mutex);
    ka_mm_vfree(domain);
}

static inline u32 task_get_bkt_from_tgid(int tgid)
{
    return ka_hash_min(tgid, TASK_CTX_HASH_BIT);
}

static inline void task_ctx_add(struct task_ctx_domain *domain, struct task_ctx *ctx)
{
    u32 bkt = task_get_bkt_from_tgid(ctx->tgid);
    ka_hlist_add_head(&ctx->link, &domain->ctx_htable[bkt]);
}

static inline void task_ctx_del(struct task_ctx *ctx)
{
    ka_hlist_del_init(&ctx->link);
}

static inline struct task_ctx *_task_ctx_find(struct task_ctx_domain *domain, int tgid)
{
    u32 bkt = task_get_bkt_from_tgid(tgid);
    struct task_ctx *ctx = NULL;

    ka_hlist_for_each_entry(ctx, &domain->ctx_htable[bkt], link)
    {
        if (ctx->tgid == tgid) {
            return ctx;
        }
    }
    return NULL;
}

static inline int task_ctx_add_to_domain(struct task_ctx_domain *domain, struct task_ctx *ctx)
{
    u32 bkt = task_get_bkt_from_tgid(ctx->tgid);

    ka_task_write_lock_bh(&domain->ctx_lock[bkt]);
    if (_task_ctx_find(domain, ctx->tgid) != NULL) {
        ka_task_write_unlock_bh(&domain->ctx_lock[bkt]);
        return -EEXIST;
    }
    task_ctx_add(domain, ctx);
    ka_task_write_unlock_bh(&domain->ctx_lock[bkt]);

    ka_base_atomic_inc(&domain->task_num);

    return 0;
}

static inline void _task_ctx_release(struct task_ctx_domain *domain, struct task_ctx *ctx)
{
    if (ctx->release != NULL) {
        ctx->release(ctx);
    }
    ka_task_mutex_destroy(&ctx->mutex);
    ka_mm_vfree(ctx);
}

static inline void task_ctx_release(struct kref_safe *ref)
{
    struct task_ctx *ctx = ka_container_of(ref, struct task_ctx, ref);
    _task_ctx_release(ctx->domain, ctx);
}

static inline struct task_ctx *_task_ctx_get_inner(struct task_ctx_domain *domain, int tgid,
                                                   struct task_start_time *time)
{
    struct task_ctx *ctx = _task_ctx_find(domain, tgid);
    if (ctx != NULL) {
        if ((time != NULL) && !(ka_system_timespec_equal(ctx->start_time.time, time->time))) {
            return NULL;
        }

        if (kref_safe_get_unless_zero(&ctx->ref) == 0) {
            return NULL;
        }
    }

    return ctx;
}

static inline struct task_ctx *_task_ctx_get(struct task_ctx_domain *domain, int tgid, struct task_start_time *time)
{
    u32 bkt = task_get_bkt_from_tgid(tgid);
    struct task_ctx *ctx = NULL;

    ka_task_read_lock_bh(&domain->ctx_lock[bkt]);
    ctx = _task_ctx_get_inner(domain, tgid, time);
    ka_task_read_unlock_bh(&domain->ctx_lock[bkt]);

    return ctx;
}

/* interface */
static inline struct task_ctx *task_ctx_get(struct task_ctx_domain *domain, int tgid)
{
    return _task_ctx_get(domain, tgid, NULL);
}

static inline struct task_ctx *task_ctx_time_check_get(struct task_ctx_domain *domain, int tgid,
                                                       struct task_start_time *time)
{
    return _task_ctx_get(domain, tgid, time);
}

/* interface */
static inline void task_ctx_put(struct task_ctx *ctx)
{
    kref_safe_put(&ctx->ref, task_ctx_release);
}

/* interface */
static inline void *task_ctx_priv(struct task_ctx *ctx)
{
    return ctx->priv;
}

static inline struct task_struct *_task_get_by_tgid(int tgid)
{
    ka_struct_pid_t *pid = NULL;
    struct task_struct *task = NULL;

    pid = ka_task_get_pid(ka_task_find_pid_ns(tgid, &init_pid_ns));
    if (pid == NULL) {
        return NULL;
    }

    task = ka_task_get_pid_task(pid, PIDTYPE_PID);
    ka_task_put_pid(pid);

    return task;
}

static inline struct task_struct *task_get_by_tgid(int tgid)
{
    struct task_struct *task = NULL;

    ka_task_rcu_read_lock();
    task = _task_get_by_tgid(tgid);
    ka_task_rcu_read_unlock();

    return task;
}

static inline int task_get_start_time_by_tgid(int tgid, struct task_start_time *start_time)
{
    struct task_struct *task = task_get_by_tgid(tgid);
    if (task != NULL) {
        start_time->time = task->group_leader->start_time;
        ka_task_put_task_struct(task);
        return 0;
    }

    return -ESRCH;
}

static inline int _task_ctx_init(struct task_ctx_domain *domain, struct task_ctx *ctx, int tgid, void *priv,
                                 void (*release)(struct task_ctx *ctx))
{
    (void)strncpy_s(ctx->name, TASK_COMM_LEN, ka_task_get_current_comm(), TASK_COMM_LEN);
    (void)task_get_start_time_by_tgid(tgid, &ctx->start_time); // caution: tgid may belong to a remote process
    ctx->tgid = tgid;
    ctx->priv = priv;
    ka_task_mutex_init(&ctx->mutex);
    ka_task_spin_lock_init(&ctx->lock);
    ctx->domain = domain;
    ctx->release = release;
    kref_safe_init(&ctx->ref);
    return task_ctx_add_to_domain(domain, ctx);
}

static inline int task_ctx_init(struct task_ctx_domain *domain, struct task_ctx *ctx, int tgid, void *priv,
                                void (*release)(struct task_ctx *ctx))
{
    if ((domain->task_max_num > 0) && (ka_base_atomic_read(&domain->task_num) >= (int)domain->task_max_num)) {
        return -EFAULT;
    }

    return _task_ctx_init(domain, ctx, tgid, priv, release);
}

/* interface */
static inline int task_ctx_create(struct task_ctx_domain *domain, int tgid, void *priv,
                                  void (*release)(struct task_ctx *ctx))
{
    struct task_ctx *ctx = NULL;
    int ret;

    if (task_ctx_domain_is_active(domain) == false) {
        return -EPERM;
    }

    ctx = ka_mm_vzalloc(sizeof(*ctx));
    if (ctx == NULL) {
        return -ENOMEM;
    }

    ret = task_ctx_init(domain, ctx, tgid, priv, release);
    if (ret != 0) {
        ka_mm_vfree(ctx);
    }

    return ret;
}

/* interface */
static inline int task_ctx_create_ex(struct task_ctx_domain *domain, int tgid, void *priv,
                                     void (*release)(struct task_ctx *ctx), struct task_ctx **ctx)
{
    int ret = -ENOMEM;

    if (task_ctx_domain_is_active(domain) == false) {
        return -EPERM;
    }

    *ctx = ka_mm_vzalloc(sizeof(**ctx));
    if (*ctx != NULL) {
        ret = task_ctx_init(domain, *ctx, tgid, priv, release);
        if (ret != 0) {
            ka_mm_vfree(*ctx);
            *ctx = NULL;
        }
    }

    return ret;
}

/* interface */
static inline void task_ctx_destroy(struct task_ctx *ctx)
{
    u32 bkt = task_get_bkt_from_tgid(ctx->tgid);

    ka_base_atomic_dec(&ctx->domain->task_num);

    ka_task_write_lock_bh(&ctx->domain->ctx_lock[bkt]);
    task_ctx_del(ctx);
    ka_task_write_unlock_bh(&ctx->domain->ctx_lock[bkt]);

    task_ctx_put(ctx);
}

static inline struct task_ctx *task_ctx_get_first(ka_hlist_head_t *head, ka_rwlock_t *lock)
{
    struct task_ctx *ctx = NULL;

    ka_task_read_lock_bh(lock);
    ctx = ka_hlist_entry_safe(head->first, struct task_ctx, link);
    if (ctx != NULL) {
        if (kref_safe_get_unless_zero(&ctx->ref) == 0) {
            ctx = NULL;
        }
    }
    ka_task_read_unlock_bh(lock);

    return ctx;
}

static inline struct task_ctx *task_ctx_get_next(struct task_ctx *ctx_from, ka_rwlock_t *lock)
{
    struct task_ctx *ctx = NULL;

    ka_task_read_lock_bh(lock);
    ctx = ka_hlist_entry_safe(ctx_from->link.next, struct task_ctx, link);
    if (ctx != NULL) {
        if (kref_safe_get_unless_zero(&ctx->ref) == 0) {
            ctx = NULL;
        }
    }
    ka_task_read_unlock_bh(lock);
    return ctx;
}

static inline void _task_ctx_for_each_safe(ka_hlist_head_t *head, ka_rwlock_t *lock, void *priv,
                                           void (*func)(struct task_ctx *ctx, void *priv))
{
    struct task_ctx *ctx = NULL;

    ctx = task_ctx_get_first(head, lock);

    while (ctx != NULL) {
        /* search and then release to ensure that the ctx is not deleted before the search */
        struct task_ctx *next_ctx = task_ctx_get_next(ctx, lock);
        func(ctx, priv);
        task_ctx_put(ctx);
        ctx = next_ctx;
    }
}

/* interface */
static inline void task_ctx_for_each_safe(struct task_ctx_domain *domain, void *priv,
                                          void (*func)(struct task_ctx *ctx, void *priv))
{
    u32 bkt;

    for (bkt = 0; bkt < TASK_CTX_HASH_MAX; bkt++) {
        _task_ctx_for_each_safe(&domain->ctx_htable[bkt], &domain->ctx_lock[bkt], priv, func);
    }
}

static inline void task_ctx_try_cond_resched_by_time(unsigned long *pre_stamp, unsigned long time)
{
    unsigned long timeinterval;

    if (ka_base_in_atomic()) {
        return;
    }

    timeinterval = ka_system_jiffies_to_msecs(ka_jiffies - *pre_stamp);
    if (timeinterval > time) {
        ka_task_cond_resched();
        *pre_stamp = ka_jiffies;
    }
}

#define TASK_CTX_COND_RESCHED_TIME 500
static inline int task_ctx_for_each_safe_ex(struct task_ctx_domain *domain, void *priv,
                                            int (*func)(struct task_ctx *ctx, void *priv), bool break_if_fail)
{
    unsigned long stamp = ka_jiffies;
    u32 bkt;

    for (bkt = 0; bkt < TASK_CTX_HASH_MAX; bkt++) {
        struct task_ctx *ctx = task_ctx_get_first(&domain->ctx_htable[bkt], &domain->ctx_lock[bkt]);
        int ret = 0;

        while (ctx != NULL) {
            /* search and then release to ensure that the ctx is not deleted before the search */
            struct task_ctx *next_ctx = task_ctx_get_next(ctx, &domain->ctx_lock[bkt]);
            ret = func(ctx, priv);
            task_ctx_put(ctx);

            if (!((ret != 0) && (break_if_fail == true))) {
                ctx = next_ctx;
                task_ctx_try_cond_resched_by_time(&stamp, TASK_CTX_COND_RESCHED_TIME);
                continue;
            }

            if (next_ctx != NULL) {
                task_ctx_put(next_ctx);
            }
            return ret;
        }
    }

    return 0;
}

static inline void task_ctx_item_show(struct task_ctx *ctx, void *priv)
{
    struct seq_file *seq = (struct seq_file *)priv;
    ka_fs_seq_printf(seq, "    name: %s tgid %d ref %d\n", ctx->name, ctx->tgid, kref_safe_read(&ctx->ref));
}

/* interface */
static inline void task_ctx_domain_show(struct task_ctx_domain *domain, struct seq_file *seq)
{
    ka_fs_seq_printf(seq, "domain: %s task_num %d task_max_num %u\n", domain->name,
                     ka_base_atomic_read(&domain->task_num), domain->task_max_num);

    task_ctx_for_each_safe(domain, seq, task_ctx_item_show);
    ka_fs_seq_printf(seq, "\n");
}

static inline int _task_ctx_lock_call_func(struct task_ctx_domain *domain, int tgid, struct task_start_time *time,
                                           int (*func)(struct task_ctx *ctx, void *para), void *para)
{
    int ret = -ESRCH;
    struct task_ctx *ctx = _task_ctx_get(domain, tgid, time);
    if (ctx != NULL) {
        ka_task_mutex_lock(&ctx->mutex);
        ret = func(ctx, para);
        ka_task_mutex_unlock(&ctx->mutex);
        task_ctx_put(ctx);
    }

    return ret;
}

/* interface */
static inline int task_ctx_lock_call_func(struct task_ctx_domain *domain, int tgid,
                                          int (*func)(struct task_ctx *ctx, void *para), void *para)
{
    return _task_ctx_lock_call_func(domain, tgid, NULL, func, para);
}

static inline int task_ctx_time_check_lock_call_func(struct task_ctx_domain *domain, int tgid,
                                                     struct task_start_time *time,
                                                     int (*func)(struct task_ctx *ctx, void *para), void *para)
{
    return _task_ctx_lock_call_func(domain, tgid, time, func, para);
}

static inline int _task_ctx_lock_call_func_non_block(struct task_ctx_domain *domain, int tgid,
                                                     struct task_start_time *time,
                                                     int (*func)(struct task_ctx *ctx, void *para), void *para)
{
    int ret = -ESRCH;
    struct task_ctx *ctx = _task_ctx_get(domain, tgid, time);
    if (ctx != NULL) {
        ka_task_spin_lock_bh(&ctx->lock);
        ret = func(ctx, para);
        ka_task_spin_unlock_bh(&ctx->lock);
        task_ctx_put(ctx);
    }

    return ret;
}

/* interface */
static inline int task_ctx_lock_call_func_non_block(struct task_ctx_domain *domain, int tgid,
                                                    int (*func)(struct task_ctx *ctx, void *para), void *para)
{
    return _task_ctx_lock_call_func_non_block(domain, tgid, NULL, func, para);
}

static inline int task_ctx_time_check_lock_call_func_non_block(struct task_ctx_domain *domain, int tgid,
                                                               struct task_start_time *time,
                                                               int (*func)(struct task_ctx *ctx, void *para),
                                                               void *para)
{
    return _task_ctx_lock_call_func_non_block(domain, tgid, time, func, para);
}

/* return false only when task is alive and not in exiting */
static inline bool task_is_exit(int tgid, struct task_start_time *start_time)
{
    bool is_exit = true;
    struct task_struct *task = task_get_by_tgid(tgid);
    if (task != NULL) {
        if ((ka_system_timespec_equal(task->group_leader->start_time, start_time->time)) &&
            !(task->flags & KA_TASK_PF_EXITING)) {
            is_exit = false;
        }
        ka_task_put_task_struct(task);
    }

    return is_exit;
}

static inline bool dbl_task_is_dead(int tgid, struct task_start_time *start_time)
{
    bool is_dead = true;
    struct task_struct *task = task_get_by_tgid(tgid);
    if (task != NULL) {
        is_dead = !(ka_system_timespec_equal(task->group_leader->start_time, start_time->time));
        ka_task_put_task_struct(task);
    }

    return is_dead;
}

static inline int task_get_tgid_by_vpid(int vpid, int *tgid)
{
    struct task_struct *task = NULL;

    ka_task_rcu_read_lock();
    task = ka_task_get_pid_task(ka_task_find_vpid(vpid), PIDTYPE_PID);
    ka_task_rcu_read_unlock();
    if (task == NULL) {
        return -ESRCH;
    }

    *tgid = (int)ka_task_task_tgid_nr(task);
    ka_task_put_task_struct(task);
    return 0;
}

static inline int task_get_parent_tgid_by_vpid(int vpid, int *tgid)
{
    struct task_struct *task = NULL;

    ka_task_rcu_read_lock();
    task = ka_task_get_pid_task(ka_task_find_vpid(vpid), PIDTYPE_PID);
    ka_task_rcu_read_unlock();
    if (task == NULL) {
        return -ESRCH;
    }

    *tgid = (int)task->real_parent->tgid;
    ka_task_put_task_struct(task);
    return 0;
}

static inline bool task_is_current_child_or_current(int vpid)
{
    int parent_tgid;

    if ((int)ka_task_task_tgid_vnr(ka_task_get_current()) == vpid) {
        return true;
    }

    if (task_get_parent_tgid_by_vpid(vpid, &parent_tgid) != 0) {
        return false;
    }

    return (ka_task_task_tgid_nr(ka_task_get_current()) == parent_tgid);
}

#endif
