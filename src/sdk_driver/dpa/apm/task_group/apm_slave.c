/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
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
#include "ka_common_pub.h"
#include "ka_fs_pub.h"
#include "ka_task_pub.h"

#include "apm_slave.h"
#include "apm_slab.h"

static void apm_slave_ctx_release(struct task_ctx *ctx)
{
    struct slave_ctx *s_ctx = (struct slave_ctx *)ctx->priv;
    apm_proc_fs_del_task(s_ctx->entry);
    apm_vfree(ctx->priv);
    ctx->priv = NULL;
}

static int _apm_slave_ctx_create(struct task_ctx_domain *domain, int slave_tgid, int slave_pid)
{
    struct slave_ctx *s_ctx = NULL;
    int ret;

    s_ctx = apm_vzalloc(sizeof(*s_ctx));
    if (s_ctx == NULL) {
        apm_err("Vmalloc slave ctx fail. (size=%ld)\n", sizeof(*s_ctx));
        return -ENOMEM;
    }

    s_ctx->slave_pid = slave_pid;
    s_ctx->ssid = -1;

    ret = task_ctx_create(domain, slave_tgid, s_ctx, apm_slave_ctx_release);
    if (ret != 0) {
        apm_err("Create fail. (ret=%d; slave_tgid=%d)\n", ret, slave_tgid);
        apm_vfree(s_ctx);
        return ret;
    }

    s_ctx->entry = apm_proc_fs_add_task(domain->name, slave_tgid);

    return 0;
}

int apm_slave_ctx_create(struct task_ctx_domain *domain, int slave_tgid, int slave_pid,
                         struct task_start_time *start_time)
{
    struct task_ctx *ctx = NULL;
    int ret = 0;

    ka_task_mutex_lock(&domain->mutex);
    ctx = task_ctx_get(domain, slave_tgid);
    if (ctx == NULL) {
        ret = _apm_slave_ctx_create(domain, slave_tgid, slave_pid);
        if (ret != 0) {
            ka_task_mutex_unlock(&domain->mutex);
            apm_err("Create slave ctx failed. (slave_tgid=%d;)\n", slave_tgid);
            return ret;
        }
    } else {
        if ((start_time != NULL) && (!ka_system_timespec_equal(start_time->time, ctx->start_time.time))) {
            struct slave_ctx *s_ctx = (struct slave_ctx *)ctx->priv;
            s_ctx->need_fast_exit = 1; /* accelerate the process exit */
            ret = -EAGAIN;
            apm_debug("Slave start time is different, please retry. (slave_tgid=%d)\n", slave_tgid);
        }
        task_ctx_put(ctx);
    }
    ka_task_mutex_unlock(&domain->mutex);

    return ret;
}

int apm_slave_ctx_need_fast_exit(struct task_ctx_domain *domain, int slave_tgid)
{
    struct task_ctx *ctx = NULL;
    int need_fast_exit = 0;

    ka_task_mutex_lock(&domain->mutex);
    ctx = task_ctx_get(domain, slave_tgid);
    if (ctx != NULL) {
        struct slave_ctx *s_ctx = (struct slave_ctx *)ctx->priv;
        need_fast_exit = s_ctx->need_fast_exit;
        task_ctx_put(ctx);
    }
    ka_task_mutex_unlock(&domain->mutex);

    return need_fast_exit;
}

void apm_slave_ctx_destroy(struct task_ctx_domain *domain, int slave_tgid)
{
    struct task_ctx *ctx = NULL;

    ka_task_mutex_lock(&domain->mutex);
    ctx = task_ctx_get(domain, slave_tgid);
    if (ctx == NULL) {
        ka_task_mutex_unlock(&domain->mutex);
        return;
    }

    task_ctx_destroy(ctx);
    task_ctx_put(ctx);
    ka_task_mutex_unlock(&domain->mutex);
}

static int apm_slave_para_check(struct apm_cmd_bind *para)
{
    if ((para->proc_type < 0) || (para->proc_type >= APM_PROC_TYPE_NUM)) {
        return -ERANGE;
    }

    if (!apm_is_surport_multi_slave(para->proc_type) && (para->devid == UDA_INVALID_UDEVID)) {
        return -ERANGE;
    }

    if ((para->mode != APM_ONLINE_MODE) && (para->mode != APM_OFFLINE_MODE)) {
        return -EINVAL;
    }

    return 0;
}

static int apm_slave_ctx_add_master(struct task_ctx *ctx, void *priv)
{
    struct slave_ctx *s_ctx = (struct slave_ctx *)ctx->priv;
    struct apm_task_group_cfg *cfg = (struct apm_task_group_cfg *)priv;
    struct apm_cmd_bind *para = &cfg->para;

    if (s_ctx->exit_check_src_flag != 0) {
        apm_err("Slave in exit stage, not supported. (slave_pid=%d)\n", s_ctx->slave_pid);
        return -EOPNOTSUPP;
    }

    if (s_ctx->slave_pid != para->slave_pid) {
        apm_err("Slave pid mismatch. (slave_pid=%d; add_slave_pid=%d)\n", s_ctx->slave_pid, para->slave_pid);
        return -EINVAL;
    }

    if ((s_ctx->num > 0) &&
        ((s_ctx->master_pid != para->master_pid) || (s_ctx->udevid != para->devid) || (s_ctx->mode != para->mode))) {
        apm_err("Should bind same master. (master_pid=%d; slave_pid=%d; bound_master_pid=%d; devid=%u; mode=%d)\n",
                para->master_pid, para->slave_pid, s_ctx->master_pid, para->devid, para->mode);
        return -EINVAL;
    }

    if (s_ctx->valid[para->proc_type] != 0) {
        return -EEXIST;
    }

    s_ctx->valid[para->proc_type] = 1;
    s_ctx->num++;
    if (s_ctx->num == 1) {
        s_ctx->master_pid = para->master_pid;
        s_ctx->master_tgid = cfg->master_tgid;
        s_ctx->mode = para->mode;
        s_ctx->udevid = para->devid;
    }

    return 0;
}

int apm_slave_add_master(struct task_ctx_domain *domain, int slave_tgid, int master_tgid, struct apm_cmd_bind *para)
{
    struct apm_task_group_cfg cfg;
    int ret;

    ret = apm_slave_para_check(para);
    if (ret != 0) {
        apm_err("Invalid para. (proc_type=%d; mode=%d; udevid=%d)\n", para->proc_type, para->mode, para->devid);
        return ret;
    }

    apm_fill_task_group_cfg(&cfg, master_tgid, slave_tgid, para);
    return task_ctx_lock_call_func_non_block(domain, slave_tgid, apm_slave_ctx_add_master, (void *)&cfg);
}

static int apm_slave_ctx_del_master(struct task_ctx *ctx, void *priv)
{
    struct slave_ctx *s_ctx = (struct slave_ctx *)ctx->priv;
    struct apm_cmd_bind *para = (struct apm_cmd_bind *)priv;

    if (s_ctx->valid[para->proc_type] == 0) {
        apm_warn("Not bind proc type. (master_pid=%d; slave_pid=%d; proc_type=%d)\n", para->master_pid, para->slave_pid,
                 para->proc_type);
        return -ENXIO;
    }

    if (s_ctx->master_pid != para->master_pid) {
        apm_err("Master pid not match. (master_pid=%d; slave_pid=%d)\n", para->master_pid, para->slave_pid);
        return -ENXIO;
    }

    if (s_ctx->udevid != para->devid) {
        apm_err("Devid not match. (master_pid=%d; slave_pid=%d)\n", para->master_pid, para->slave_pid);
        return -ENXIO;
    }

    s_ctx->valid[para->proc_type] = 0;
    s_ctx->num--; /* not clear master_pid when num=0, because master task exit will match to destroy slave ctx */

    return 0;
}

int apm_slave_del_master(struct task_ctx_domain *domain, int slave_tgid, struct apm_cmd_bind *para)
{
    int ret;

    ret = apm_slave_para_check(para);
    if (ret != 0) {
        apm_err("Invalid para. (proc_type=%d; mode=%d; udevid=%d)\n", para->proc_type, para->mode, para->devid);
        return ret;
    }

    return task_ctx_lock_call_func_non_block(domain, slave_tgid, apm_slave_ctx_del_master, (void *)para);
}

static int _apm_slave_ctx_query_master(struct task_ctx *ctx, void *priv, bool check_is_self_exit)
{
    struct slave_ctx *s_ctx = (struct slave_ctx *)ctx->priv;
    struct apm_cmd_query_master_info *para = (struct apm_cmd_query_master_info *)priv;
    int proc_type;

    if (check_is_self_exit == true) {
        if (s_ctx->exit_check_src_flag != APM_SLAVE_CHECK_EXIT_NONE) {
            return -ESRCH;
        }
    }

    para->master_pid = (unsigned int)s_ctx->master_pid;
    para->master_tgid = (unsigned int)s_ctx->master_tgid;
    para->slave_pid = (unsigned int)s_ctx->slave_pid;
    para->udevid = s_ctx->udevid;
    para->mode = s_ctx->mode;
    para->proc_type_bitmap = 0;
    for (proc_type = 0; proc_type < APM_PROC_TYPE_NUM; proc_type++) {
        if (s_ctx->valid[proc_type] == 1) {
            para->proc_type_bitmap |= (0x1 << proc_type);
        }
    }

    return (para->proc_type_bitmap > 0) ? 0 : -ESRCH;
}

static int apm_slave_ctx_query_master(struct task_ctx *ctx, void *priv)
{
    return _apm_slave_ctx_query_master(ctx, priv, false);
}

static int apm_slave_ctx_query_master_self_exit_check(struct task_ctx *ctx, void *priv)
{
    return _apm_slave_ctx_query_master(ctx, priv, true);
}

int apm_slave_query_master(struct task_ctx_domain *domain, int slave_tgid, struct apm_cmd_query_master_info *para)
{
    return task_ctx_lock_call_func_non_block(domain, slave_tgid, apm_slave_ctx_query_master, (void *)para);
}

int apm_slave_query_master_self_exit_check(struct task_ctx_domain *domain, int slave_tgid,
                                           struct apm_cmd_query_master_info *para)
{
    return task_ctx_lock_call_func_non_block(domain, slave_tgid, apm_slave_ctx_query_master_self_exit_check,
                                             (void *)para);
}

int apm_slave_tgid_to_pid(struct task_ctx_domain *domain, int slave_tgid, int *slave_pid)
{
    struct task_ctx *ctx = task_ctx_get(domain, slave_tgid);
    if (ctx != NULL) {
        struct slave_ctx *s_ctx = (struct slave_ctx *)ctx->priv;
        *slave_pid = s_ctx->slave_pid;
        task_ctx_put(ctx);
        return 0;
    }

    return -ESRCH;
}

static int apm_slave_ctx_get_ssid(struct task_ctx *ctx, void *priv)
{
    struct slave_ctx *s_ctx = (struct slave_ctx *)ctx->priv;
    struct apm_cmd_slave_ssid *para = (struct apm_cmd_slave_ssid *)priv;

    if (s_ctx->ssid < 0) {
        return -ENOSYS;
    }

    para->ssid = s_ctx->ssid;
    return 0;
}

int apm_slave_get_ssid(struct task_ctx_domain *domain, int slave_tgid, struct apm_cmd_slave_ssid *para)
{
    return task_ctx_lock_call_func_non_block(domain, slave_tgid, apm_slave_ctx_get_ssid, (void *)para);
}

static int apm_slave_ctx_set_ssid(struct task_ctx *ctx, void *priv)
{
    struct slave_ctx *s_ctx = (struct slave_ctx *)ctx->priv;
    struct apm_cmd_slave_ssid *para = (struct apm_cmd_slave_ssid *)priv;

    s_ctx->ssid = para->ssid;
    return 0;
}

int apm_slave_set_ssid(struct task_ctx_domain *domain, int slave_tgid, struct apm_cmd_slave_ssid *para)
{
    return task_ctx_lock_call_func_non_block(domain, slave_tgid, apm_slave_ctx_set_ssid, (void *)para);
}

static void apm_slave_ctx_try_destroy(struct task_ctx *ctx, void *priv)
{
    struct slave_ctx *s_ctx = (struct slave_ctx *)(uintptr_t)ctx->priv;
    int master_tgid = (int)(uintptr_t)priv;

    if (s_ctx->master_tgid == master_tgid) {
        apm_info("Master exit destroy slave. (slave_pid=%d; master_tgid=%d; slave_tgid=%d)\n", s_ctx->slave_pid,
                 master_tgid, ctx->tgid);
        apm_slave_ctx_destroy(ctx->domain, ctx->tgid);
    }
}

void apm_slave_ctx_destroy_by_master_tgid(struct task_ctx_domain *domain, int master_tgid)
{
    task_ctx_for_each_safe(domain, (void *)(uintptr_t)master_tgid, apm_slave_ctx_try_destroy);
}

static int apm_slave_ctx_show_details(struct task_ctx *ctx, void *priv)
{
    struct slave_ctx *s_ctx = (struct slave_ctx *)ctx->priv;
    ka_seq_file_t *seq = (ka_seq_file_t *)priv;
    int proc_type;

    ka_fs_seq_printf(seq, "domain: %s slave_tgid %d(%s) slave pid %d master pid %d tgid %d mode %d udevid %u num %u\n",
                     ctx->domain->name, ctx->tgid, ctx->name, s_ctx->slave_pid, s_ctx->master_pid, s_ctx->master_tgid,
                     s_ctx->mode, s_ctx->udevid, s_ctx->num);
    for (proc_type = 0; proc_type < APM_PROC_TYPE_NUM; proc_type++) {
        if (s_ctx->valid[proc_type] == 1) {
            ka_fs_seq_printf(seq, "    %d(%s)", proc_type, apm_proc_type_to_name(proc_type));
        }
    }
    ka_fs_seq_printf(seq, "\n");
    return 0;
}

void apm_slave_ctx_show(struct task_ctx_domain *domain, int slave_tgid, ka_seq_file_t *seq)
{
    (void)task_ctx_lock_call_func_non_block(domain, slave_tgid, apm_slave_ctx_show_details, (void *)seq);
}

static int apm_slave_ctx_check_set_exit_stage(struct task_ctx *ctx, void *priv)
{
    u32 *check_src = (u32 *)priv;
    struct slave_ctx *s_ctx = (struct slave_ctx *)ctx->priv;

    if (*check_src == APM_SLAVE_CHECK_EXIT_FROM_APM) {
        if (s_ctx->exit_check_src_flag & (1U << APM_SLAVE_CHECK_EXIT_FROM_APM)) {
            return -EEXIST;
        }
    }

    if ((s_ctx->num != 0) || (s_ctx->exit_check_src_flag != 0)) {
        s_ctx->exit_check_src_flag |= (1U << *check_src);
        return 0;
    }

    return -EINVAL;
}

int apm_slave_check_set_exit_status(struct task_ctx_domain *domain, int slave_tgid, struct task_start_time *time,
                                    u32 check_src)
{
    u32 src = check_src;
    return task_ctx_time_check_lock_call_func_non_block(domain, slave_tgid, time, apm_slave_ctx_check_set_exit_stage,
                                                        &src);
}
