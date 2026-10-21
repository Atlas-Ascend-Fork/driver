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
#include "ka_list_pub.h"
#include "ka_memory_pub.h"
#include "ka_task_pub.h"
#include "pbl/pbl_uda.h"

#include "apm_master.h"
#include "apm_slab.h"

enum apm_slave_op_type {
    SLAVE_OP_GET_SLAVE_TGID,
    SLAVE_OP_GET_SLAVE_TGID_ALL_STAGE,
    SLAVE_OP_SET_EXIT_STAGE,
    SLAVE_OP_GET_ALL_EXIT_STAGE,
    SLAVE_OP_SET_OOM_STATUS,
    SLAVE_OP_GET_OOM_STATUS,
    SLAVE_OP_SHOW_DETAILS,
    SLAVE_OP_GET_SLAVE_SSID,
    SLAVE_OP_SET_SLAVE_SSID,
    SLAVE_OP_MAX
};

struct slave_op_para {
    int op_type;
    u32 udevid;
    int proc_type;
    int slave_tgid;
    int val;
    int local_flag;
    void *priv;
};

int apm_master_tgid_to_pid(struct task_ctx_domain *domain, int master_tgid, int *master_pid)
{
    struct task_ctx *ctx = task_ctx_get(domain, master_tgid);
    if (ctx != NULL) {
        struct master_ctx *m_ctx = (struct master_ctx *)ctx->priv;
        *master_pid = m_ctx->master_pid;
        task_ctx_put(ctx);
        return 0;
    }

    return -EINVAL;
}

static void apm_master_ctx_release(struct task_ctx *ctx)
{
    struct master_ctx *m_ctx = (struct master_ctx *)ctx->priv;
    apm_proc_fs_del_task(m_ctx->entry);
    apm_vfree(ctx->priv);
    ctx->priv = NULL;
}

static int _apm_master_ctx_create(struct task_ctx_domain *domain, int master_tgid, int master_pid)
{
    struct master_ctx *m_ctx = NULL;
    unsigned long size = sizeof(*m_ctx) + uda_get_udev_max_num() * sizeof(struct master_dev_ctx);
    u32 i;
    int ret;

    m_ctx = apm_vzalloc(size);
    if (m_ctx == NULL) {
        apm_err("Vmalloc master ctx fail. (size=%lu)\n", size);
        return -ENOMEM;
    }

    m_ctx->master_pid = master_pid;
    KA_INIT_LIST_HEAD(&m_ctx->head);
    for (i = 0; i < uda_get_udev_max_num(); i++) {
        if (uda_can_access_udevid(i)) {
            m_ctx->dev_ctx[i].valid = 1;
        }
    }

    ret = task_ctx_create(domain, master_tgid, m_ctx, apm_master_ctx_release);
    if (ret != 0) {
        apm_err("Create fail. (ret=%d; master_tgid=%d)\n", ret, master_tgid);
        apm_vfree(m_ctx);
        return ret;
    }

    m_ctx->entry = apm_proc_fs_add_task(domain->name, master_tgid);

    return 0;
}

int apm_master_ctx_create(struct task_ctx_domain *domain, int master_tgid, int master_pid,
                          struct task_start_time *start_time)
{
    struct task_ctx *ctx = NULL;
    int ret = 0;

    ka_task_mutex_lock(&domain->mutex);
    ctx = task_ctx_get(domain, master_tgid);
    if (ctx == NULL) {
        ret = _apm_master_ctx_create(domain, master_tgid, master_pid);
        if (ret != 0) {
            ka_task_mutex_unlock(&domain->mutex);
            return ret;
        }
    } else {
        if ((start_time != NULL) && (!ka_system_timespec_equal(start_time->time, ctx->start_time.time))) {
            apm_debug("Master start time is different, please retry. (master_pid=%d)\n", master_pid);
            ret = -EAGAIN;
        }
        task_ctx_put(ctx);
    }
    ka_task_mutex_unlock(&domain->mutex);

    return ret;
}

void apm_master_ctx_destroy(struct task_ctx_domain *domain, int master_tgid)
{
    struct task_ctx *ctx = NULL;

    ka_task_mutex_lock(&domain->mutex);
    ctx = task_ctx_get(domain, master_tgid);
    if (ctx == NULL) {
        ka_task_mutex_unlock(&domain->mutex);
        return;
    }

    task_ctx_destroy(ctx);
    task_ctx_put(ctx);
    ka_task_mutex_unlock(&domain->mutex);
}

static int apm_master_ctx_add_slave(struct task_ctx *ctx, void *priv)
{
    struct master_ctx *m_ctx = (struct master_ctx *)ctx->priv;
    struct apm_task_group_cfg *cfg = (struct apm_task_group_cfg *)priv;
    struct apm_cmd_bind *para = &cfg->para;

    if (m_ctx->master_exit_stage != 0) {
#ifndef EMU_ST
        apm_debug("Master in exit stage, not supported. (master_pid=%d)\n", m_ctx->master_pid);
        return -EOPNOTSUPP;
#endif
    }

    if (m_ctx->master_pid != para->master_pid) {
        apm_err("Master pid mismatch. (master_pid=%d; add_master_pid=%d)\n", m_ctx->master_pid, para->master_pid);
        return -EINVAL;
    }

    if (para->proc_type == PROCESS_USER) {
        if (m_ctx->user_slave_num >= MAX_USER_SLAVE_NUM) {
            apm_err("Process user slave count exceed limit. (count=%u; max=%u; master_pid=%d; slave_pid=%d)\n",
                    m_ctx->user_slave_num, MAX_USER_SLAVE_NUM, para->master_pid, para->slave_pid);
            return -EINVAL;
        }
    }

    if (apm_is_surport_multi_slave(para->proc_type)) {
        struct master_slave_node *slave_node = apm_kzalloc(sizeof(struct master_slave_node),
                                                           KA_GFP_ATOMIC | __KA_GFP_ACCOUNT);
        if (slave_node == NULL) {
            apm_err("Alloc slave node failed. (proc_type=%d; master_pid=%d; slave_pid=%d)\n", para->proc_type,
                    para->master_pid, para->slave_pid);
            return -ENOMEM;
        }

        slave_node->udevid = para->devid;
        slave_node->proc_type = para->proc_type;
        slave_node->info.local_flag = cfg->local_flag;
        slave_node->info.slave_pid = para->slave_pid;
        slave_node->info.slave_tgid = cfg->slave_tgid;
        slave_node->info.ssid = -1;
        slave_node->info.try_unbind_flag = 0;
        ka_list_add_tail(&slave_node->node, &m_ctx->head);
        if (para->proc_type == PROCESS_USER) {
            m_ctx->user_slave_num++;
        }
    } else {
        struct master_slave_info *slave_info = NULL;

        if (para->devid >= uda_get_udev_max_num()) {
            apm_err("Invalid udevid. (proc_type=%d; master_pid=%d; slave_pid=%d; devid=%u)\n", para->proc_type,
                    para->master_pid, para->slave_pid, para->devid);
            return -EINVAL;
        }

        if (m_ctx->dev_ctx[para->devid].valid == 0) {
            apm_err("Bind invalid udevid. (proc_type=%d; master_pid=%d; slave_pid=%d; devid=%u)\n", para->proc_type,
                    para->master_pid, para->slave_pid, para->devid);
            return -EINVAL;
        }

        if (para->proc_type == PROCESS_CP2) {
            slave_info = &m_ctx->dev_ctx[para->devid].slave_info[PROCESS_CP1];
            if (slave_info->valid == 0) {
                apm_err("Bind custom cp, cp not bind. (proc_type=%d; master_pid=%d; slave_pid=%d)\n", para->proc_type,
                        para->master_pid, para->slave_pid);
                return -EINVAL;
            }
        }

        slave_info = &m_ctx->dev_ctx[para->devid].slave_info[para->proc_type];
        if ((slave_info->valid == 1) && (slave_info->exit_stage == 0)) { /* allow new after old exit */
            apm_err("Master already bind this proc_type. (proc_type=%d; master_pid=%d; slave_pid=%d)\n",
                    para->proc_type, para->master_pid, para->slave_pid);
            return -EINVAL;
        }

        slave_info->local_flag = cfg->local_flag;
        slave_info->slave_pid = para->slave_pid;
        slave_info->slave_tgid = cfg->slave_tgid;
        slave_info->valid = 1;
        slave_info->exit_stage = 0;
        slave_info->ssid = -1;
        slave_info->try_unbind_flag = 0;
        m_ctx->dev_ctx[para->devid].num++;
    }

    m_ctx->num++;

    return 0;
}

int apm_master_add_slave(struct task_ctx_domain *domain, int master_tgid, int slave_tgid, int local_flag,
                         struct apm_cmd_bind *para)
{
    struct apm_task_group_cfg cfg;

    apm_fill_task_group_cfg(&cfg, master_tgid, slave_tgid, para);
    cfg.local_flag = local_flag;
    return task_ctx_lock_call_func_non_block(domain, master_tgid, apm_master_ctx_add_slave, (void *)&cfg);
}

static inline bool apm_is_find_slave_node_in_all_stage(enum apm_slave_op_type op_type)
{
    return ((op_type != SLAVE_OP_GET_SLAVE_TGID) && (op_type != SLAVE_OP_GET_OOM_STATUS));
}

/* slave_tgid = 0, not match slave_tgid for query slave tgid */
/* local_flag = -1, match all local_flag */
static struct master_slave_node *apm_find_slave_node(struct master_ctx *m_ctx, u32 udevid, int proc_type,
                                                     int slave_tgid, bool find_in_all_stage, int local_flag)
{
    struct master_slave_node *slave_node = NULL;

    ka_list_for_each_entry(slave_node, &m_ctx->head, node)
    {
        if ((!find_in_all_stage) && (slave_node->info.exit_stage != 0)) {
            continue;
        }

        if ((local_flag != -1) && (slave_node->info.local_flag != local_flag)) {
            continue;
        }

        /* proc_type match */
        if ((slave_node->udevid == udevid) && (slave_node->proc_type == proc_type) &&
            ((slave_node->info.slave_tgid == slave_tgid) || (slave_tgid == 0))) {
            return slave_node;
        }

        /* slave_tgid match */
        if ((slave_node->udevid == udevid) && (slave_node->info.slave_tgid == slave_tgid) &&
            ((slave_node->proc_type == proc_type) || (proc_type == 0))) {
            return slave_node;
        }
    }

    return NULL;
}

static int apm_master_ctx_del_slave(struct task_ctx *ctx, void *priv)
{
    struct master_ctx *m_ctx = (struct master_ctx *)ctx->priv;
    struct apm_task_group_cfg *cfg = (struct apm_task_group_cfg *)priv;
    struct apm_cmd_bind *para = &cfg->para;

    if (apm_is_surport_multi_slave(para->proc_type)) {
        struct master_slave_node *slave_node = apm_find_slave_node(m_ctx, para->devid, para->proc_type, cfg->slave_tgid,
                                                                   true, cfg->local_flag);
        if (slave_node == NULL) {
            apm_warn("Slave node not find. (udevid=%u; proc_type=%d; master_pid=%d; slave_tgid=%d)\n", para->devid,
                     para->proc_type, para->master_pid, cfg->slave_tgid);
            return -EINVAL;
        }

        if ((slave_node->proc_type == PROCESS_USER) && (m_ctx->user_slave_num > 0)) {
            m_ctx->user_slave_num--;
        }
        ka_list_del(&slave_node->node);
        apm_kfree(slave_node);
    } else {
        struct master_slave_info *slave_info = NULL;
        if (para->devid >= uda_get_udev_max_num()) {
            apm_warn("Invalid udevid. (udevid=%u; proc_type=%d; master_pid=%d; slave_tgid=%d)\n", para->devid,
                     para->proc_type, para->master_pid, cfg->slave_tgid);
            return -EINVAL;
        }

        slave_info = &m_ctx->dev_ctx[para->devid].slave_info[para->proc_type];
        if ((m_ctx->dev_ctx[para->devid].valid == 0) || (slave_info->valid == 0)) {
            apm_warn("Slave not add. (udevid=%u; proc_type=%d; master_pid=%d; slave_tgid=%d)\n", para->devid,
                     para->proc_type, para->master_pid, cfg->slave_tgid);
        }

        m_ctx->dev_ctx[para->devid].num--;
        if (slave_info->slave_tgid == cfg->slave_tgid) { /* check para->slave is bind slave, which may be new slave */
            slave_info->valid = 0;
            slave_info->slave_pid = 0;
            slave_info->slave_tgid = 0;
        }
    }

    m_ctx->num--;

    return 0;
}

int apm_master_del_slave(struct task_ctx_domain *domain, int master_tgid, int slave_tgid, int local_flag,
                         struct apm_cmd_bind *para)
{
    struct apm_task_group_cfg cfg;

    apm_fill_task_group_cfg(&cfg, master_tgid, slave_tgid, para);
    cfg.local_flag = local_flag;
    return task_ctx_lock_call_func_non_block(domain, master_tgid, apm_master_ctx_del_slave, (void *)&cfg);
}

static void apm_fill_slave_op_para(struct slave_op_para *para, int op_type, u32 udevid, int slave_tgid, int val)
{
    para->op_type = op_type;
    para->udevid = udevid;
    para->proc_type = APM_PROC_TYPE_NUM;
    para->slave_tgid = slave_tgid;
    para->val = val;
    para->priv = NULL;
    para->local_flag = -1;
}

static void apm_master_ctx_signle_slave_show(struct master_slave_info *slave_info, struct slave_op_para *para)
{
    ka_seq_file_t *seq = (ka_seq_file_t *)para->priv;
    ka_fs_seq_printf(seq,
                     "    udevid %u type %d(%s) local %d slave pid %d tgid %d exit_stage %d oom_status %d oom_cnt %d\n",
                     para->udevid, para->proc_type, apm_proc_type_to_name(para->proc_type), slave_info->local_flag,
                     slave_info->slave_pid, slave_info->slave_tgid, slave_info->exit_stage, slave_info->oom_status,
                     slave_info->oom_cnt);
}

static int apm_master_ctx_signle_slave_op(struct master_slave_info *slave_info, struct slave_op_para *para)
{
    switch (para->op_type) {
        case SLAVE_OP_GET_SLAVE_TGID:
            if (slave_info->exit_stage != 0) {
                return -ESRCH;
            }
            para->slave_tgid = slave_info->slave_tgid;
            para->val = slave_info->slave_pid;
            break;
        case SLAVE_OP_GET_SLAVE_TGID_ALL_STAGE:
            para->slave_tgid = slave_info->slave_tgid;
            para->val = slave_info->slave_pid;
            break;
        case SLAVE_OP_SET_EXIT_STAGE:
            slave_info->exit_stage = para->val;
            break;
        case SLAVE_OP_GET_ALL_EXIT_STAGE:
            /* use the slowest exit task stage in the task group */
            if (slave_info->exit_stage < para->val) {
                para->val = slave_info->exit_stage;
            }
            break;
        case SLAVE_OP_SET_OOM_STATUS:
            slave_info->oom_status = para->val;
            slave_info->oom_cnt++;
            break;
        case SLAVE_OP_GET_OOM_STATUS:
            para->val = slave_info->oom_status;
            slave_info->oom_status = 0; /* status read clear */
            break;
        case SLAVE_OP_SHOW_DETAILS:
            apm_master_ctx_signle_slave_show(slave_info, para);
            break;
        case SLAVE_OP_GET_SLAVE_SSID:
            if ((para->proc_type == PROCESS_CP1) && (slave_info->try_unbind_flag != 0)) {
                return -EPERM;
            }
            if (slave_info->ssid < 0) {
                return -ENOSYS;
            }
            para->val = slave_info->ssid;
            break;
        case SLAVE_OP_SET_SLAVE_SSID:
            slave_info->ssid = para->val;
            break;
        default:
            break;
    }

    return 0;
}

static int apm_master_ctx_slave_op_all_node(struct task_ctx *ctx, void *priv)
{
    struct master_ctx *m_ctx = (struct master_ctx *)ctx->priv;
    struct slave_op_para *para = (struct slave_op_para *)priv;
    struct master_slave_node *slave_node = NULL;
    u32 udevid = 0;

    for (udevid = 0; udevid < uda_get_udev_max_num(); udevid++) {
        struct master_dev_ctx *dev_ctx = &m_ctx->dev_ctx[udevid];
        int proc_type;

        if ((dev_ctx->valid == 0) || (dev_ctx->num == 0)) {
            continue;
        }

        for (proc_type = 0; proc_type < APM_PROC_TYPE_NUM; proc_type++) {
            struct master_slave_info *slave_info = &dev_ctx->slave_info[proc_type];
            if ((slave_info->slave_tgid != 0)) {
                para->udevid = udevid;
                para->proc_type = proc_type;
                (void)apm_master_ctx_signle_slave_op(slave_info, para);
            }
        }
    }

    ka_list_for_each_entry(slave_node, &m_ctx->head, node)
    {
        para->udevid = slave_node->udevid;
        para->proc_type = slave_node->proc_type;
        (void)apm_master_ctx_signle_slave_op(&slave_node->info, para);
    }

    return 0;
}

static int apm_master_ctx_slave_op_by_tgid(struct task_ctx *ctx, void *priv)
{
    struct master_ctx *m_ctx = (struct master_ctx *)ctx->priv;
    struct slave_op_para *para = (struct slave_op_para *)priv;
    struct master_slave_node *slave_node = apm_find_slave_node(m_ctx, para->udevid, 0, para->slave_tgid, true,
                                                               para->local_flag);
    if (slave_node != NULL) {
        para->proc_type = slave_node->proc_type;
        (void)apm_master_ctx_signle_slave_op(&slave_node->info, para);
    }

    if (para->udevid < uda_get_udev_max_num()) {
        int proc_type;
        for (proc_type = 0; proc_type < APM_PROC_TYPE_NUM; proc_type++) {
            struct master_slave_info *slave_info = &m_ctx->dev_ctx[para->udevid].slave_info[proc_type];
            /* one tgid may have multi proc type */
            if ((slave_info->slave_tgid == para->slave_tgid) &&
                ((para->local_flag == -1) || (para->local_flag == slave_info->local_flag))) {
                para->proc_type = proc_type;
                (void)apm_master_ctx_signle_slave_op(slave_info, para);
            }
        }
    }

    return 0;
}

static int apm_master_ctx_slave_op_by_proc_type(struct task_ctx *ctx, void *priv)
{
    struct master_ctx *m_ctx = (struct master_ctx *)ctx->priv;
    struct slave_op_para *para = (struct slave_op_para *)priv;
    int ret = -EINVAL;

    if (apm_is_surport_multi_slave(para->proc_type)) {
        struct master_slave_node *slave_node = apm_find_slave_node(
            m_ctx, para->udevid, para->proc_type, 0, apm_is_find_slave_node_in_all_stage(para->op_type), -1);
        ret = (slave_node != NULL) ? apm_master_ctx_signle_slave_op(&slave_node->info, para) : -ESRCH;
    } else {
        struct master_slave_info *slave_info = &m_ctx->dev_ctx[para->udevid].slave_info[para->proc_type];
        if ((para->proc_type == PROCESS_CP2) && (slave_info->valid == 0)) {
            slave_info = &m_ctx->dev_ctx[para->udevid].slave_info[PROCESS_CP1];
        }

        ret = (slave_info->valid != 0) ? apm_master_ctx_signle_slave_op(slave_info, para) : -ESRCH;
    }

    return ret;
}

int apm_master_para_check(u32 udevid, int proc_type)
{
    if ((proc_type < 0) || (proc_type >= APM_PROC_TYPE_NUM) ||
        (!apm_is_surport_multi_slave(proc_type) && (udevid == UDA_INVALID_UDEVID))) {
        apm_err("Invalid para. (udevid=%d; proc_type=%d)\n", udevid, proc_type);
        return -ERANGE;
    }

    return 0;
}

int apm_master_query_slave_pid(struct task_ctx_domain *domain, int master_tgid, struct apm_cmd_query_slave_pid *para)
{
    struct slave_op_para op_para;
    int ret;

    ret = apm_master_para_check(para->devid, para->proc_type);
    if (ret != 0) {
        return ret;
    }

    apm_fill_slave_op_para(
        &op_para, (para->query_in_all_stage == 1) ? SLAVE_OP_GET_SLAVE_TGID_ALL_STAGE : SLAVE_OP_GET_SLAVE_TGID,
        para->devid, 0, 0);
    op_para.proc_type = para->proc_type;
    ret = task_ctx_lock_call_func_non_block(domain, master_tgid, apm_master_ctx_slave_op_by_proc_type,
                                            (void *)&op_para);
    if (ret == 0) {
        para->slave_tgid = op_para.slave_tgid;
        para->slave_pid = op_para.val;
    }

    return ret;
}

int apm_master_get_slave_ssid(struct task_ctx_domain *domain, int master_tgid, struct apm_cmd_slave_ssid *para)
{
    struct slave_op_para op_para;
    int ret;

    ret = apm_master_para_check(para->devid, para->proc_type);
    if (ret != 0) {
        return ret;
    }

    apm_fill_slave_op_para(&op_para, SLAVE_OP_GET_SLAVE_SSID, para->devid, 0, 0);
    op_para.proc_type = para->proc_type;
    ret = task_ctx_lock_call_func_non_block(domain, master_tgid, apm_master_ctx_slave_op_by_proc_type,
                                            (void *)&op_para);
    if (ret == 0) {
        para->ssid = op_para.val;
    }

    return ret;
}

int apm_master_set_slave_ssid(struct task_ctx_domain *domain, int master_tgid, struct apm_cmd_slave_ssid *para)
{
    struct slave_op_para op_para;
    int ret;

    ret = apm_master_para_check(para->devid, para->proc_type);
    if (ret != 0) {
        return ret;
    }

    apm_fill_slave_op_para(&op_para, SLAVE_OP_SET_SLAVE_SSID, para->devid, 0, 0);
    op_para.proc_type = para->proc_type;
    op_para.val = para->ssid;
    return task_ctx_lock_call_func_non_block(domain, master_tgid, apm_master_ctx_slave_op_by_proc_type,
                                             (void *)&op_para);
}

static int apm_master_ctx_set_exit_stage(struct task_ctx *ctx, void *priv)
{
    struct master_ctx *m_ctx = (struct master_ctx *)ctx->priv;
    int stage = (int)(uintptr_t)priv;

    if ((m_ctx->master_exit_stage == 0) && (m_ctx->num == 0)) { /* no exit-stage-control need when unbinded */
        return -EINVAL;
    }

    if ((m_ctx->master_exit_stage != 0) && (stage == APM_STAGE_PRE_EXIT)) {
        return 0;
    }

    m_ctx->master_exit_stage = stage;
    return 0;
}

int apm_master_set_exit_stage(struct task_ctx_domain *domain, int master_tgid, struct task_start_time *time, int stage)
{
    return task_ctx_time_check_lock_call_func_non_block(domain, master_tgid, time, apm_master_ctx_set_exit_stage,
                                                        (void *)(uintptr_t)stage);
}

static int apm_master_ctx_get_exit_stage(struct task_ctx *ctx, void *priv)
{
    struct master_ctx *m_ctx = (struct master_ctx *)ctx->priv;
    int *stage = (int *)priv;

    *stage = m_ctx->master_exit_stage;
    return 0;
}

int apm_master_get_exit_stage(struct task_ctx_domain *domain, int master_tgid, int *stage)
{
    return task_ctx_lock_call_func_non_block(domain, master_tgid, apm_master_ctx_get_exit_stage, (void *)stage);
}

int apm_master_set_slave_exit_stage(struct task_ctx_domain *domain, int master_tgid, u32 udevid, int slave_tgid,
                                    int stage, int local_flag)
{
    struct slave_op_para para;

    apm_fill_slave_op_para(&para, SLAVE_OP_SET_EXIT_STAGE, udevid, slave_tgid, stage);
    para.local_flag = local_flag;
    return task_ctx_lock_call_func_non_block(domain, master_tgid, apm_master_ctx_slave_op_by_tgid, (void *)&para);
}

int apm_master_get_all_slave_exit_stage(struct task_ctx_domain *domain, int master_tgid, int *stage)
{
    struct slave_op_para para;
    int ret;

    apm_fill_slave_op_para(&para, SLAVE_OP_GET_ALL_EXIT_STAGE, UDA_INVALID_UDEVID, 0, APM_STAGE_MAX);
    ret = task_ctx_lock_call_func_non_block(domain, master_tgid, apm_master_ctx_slave_op_all_node, (void *)&para);
    if (ret == 0) {
        *stage = para.val;
    }

    return ret;
}

int apm_master_set_slave_oom_status(struct task_ctx_domain *domain, int master_tgid, u32 udevid, int slave_tgid,
                                    int oom_status, int local_flag)
{
    struct slave_op_para para;

    apm_fill_slave_op_para(&para, SLAVE_OP_SET_OOM_STATUS, udevid, slave_tgid, oom_status);
    para.local_flag = local_flag;
    return task_ctx_lock_call_func_non_block(domain, master_tgid, apm_master_ctx_slave_op_by_tgid, (void *)&para);
}

int apm_master_get_slave_oom_status(struct task_ctx_domain *domain, int master_tgid, u32 udevid, int proc_type,
                                    int *oom_status)
{
    struct slave_op_para para;
    int ret;

    ret = apm_master_para_check(udevid, proc_type);
    if (ret != 0) {
        return ret;
    }

    apm_fill_slave_op_para(&para, SLAVE_OP_GET_OOM_STATUS, udevid, 0, 0);
    para.proc_type = proc_type;
    ret = task_ctx_lock_call_func_non_block(domain, master_tgid, apm_master_ctx_slave_op_by_proc_type, (void *)&para);
    if (ret != 0) {
        return ret;
    }

    if (para.val != 0) {
        para.val = 0;
        ka_system_msleep(120); /* ensure process remained in the OOM state for 120 ms */
        ret = task_ctx_lock_call_func_non_block(domain, master_tgid, apm_master_ctx_slave_op_by_proc_type,
                                                (void *)&para);
    }

    *oom_status = (ret == 0) ? para.val : *oom_status;

    return ret;
}

static int apm_master_ctx_show_details(struct task_ctx *ctx, void *priv)
{
    struct master_ctx *m_ctx = (struct master_ctx *)ctx->priv;
    ka_seq_file_t *seq = (ka_seq_file_t *)priv;
    struct slave_op_para para;

    ka_fs_seq_printf(seq, "domain: %s tgid %d(%s) master pid %d num %u master_exit_stage %d\n", ctx->domain->name,
                     ctx->tgid, ctx->name, m_ctx->master_pid, m_ctx->num, m_ctx->master_exit_stage);

    apm_fill_slave_op_para(&para, SLAVE_OP_SHOW_DETAILS, UDA_INVALID_UDEVID, 0, 0);
    para.priv = priv;
    apm_master_ctx_slave_op_all_node(ctx, &para);
    ka_fs_seq_printf(seq, "\n");

    return 0;
}

void apm_master_ctx_show(struct task_ctx_domain *domain, int master_tgid, ka_seq_file_t *seq)
{
    (void)task_ctx_lock_call_func_non_block(domain, master_tgid, apm_master_ctx_show_details, (void *)seq);
}

static int apm_master_ctx_pre_unbind(struct task_ctx *ctx, void *priv)
{
    struct master_ctx *m_ctx = (struct master_ctx *)ctx->priv;
    struct apm_cmd_bind *para = (struct apm_cmd_bind *)priv;

    if (apm_is_surport_multi_slave(para->proc_type)) {
        return 0;
    }

    /* master rebind other slave scene */
    if ((m_ctx->dev_ctx[para->devid].slave_info[para->proc_type].valid == 0) ||
        (m_ctx->dev_ctx[para->devid].slave_info[para->proc_type].slave_pid != para->slave_pid)) {
        return 0;
    }

    if (m_ctx->dev_ctx[para->devid].slave_info[para->proc_type].exit_stage != 0) {
        return 0;
    }

    m_ctx->dev_ctx[para->devid].slave_info[para->proc_type].try_unbind_flag = 1;
    return ((para->proc_type == PROCESS_CP1) && (ka_base_atomic_read(&m_ctx->dev_ctx[para->devid].refcnt) != 0)) ?
               -EBUSY :
               0;
}

int apm_master_pre_unbind(struct task_ctx_domain *domain, int master_tgid, struct apm_cmd_bind *para)
{
    return task_ctx_lock_call_func_non_block(domain, master_tgid, apm_master_ctx_pre_unbind, (void *)para);
}

static int apm_master_ctx_inc_ref(struct task_ctx *ctx, void *priv)
{
    struct master_ctx *m_ctx = (struct master_ctx *)ctx->priv;
    u32 udevid = (u32)(uintptr_t)priv;

    ka_base_atomic_inc(&m_ctx->dev_ctx[udevid].refcnt);
    return 0;
}

static int apm_master_ctx_dec_ref(struct task_ctx *ctx, void *priv)
{
    struct master_ctx *m_ctx = (struct master_ctx *)ctx->priv;
    u32 udevid = (u32)(uintptr_t)priv;

    ka_base_atomic_dec(&m_ctx->dev_ctx[udevid].refcnt);
    return 0;
}

int apm_master_inc_ref(struct task_ctx_domain *domain, int master_tgid, u32 udevid)
{
    return task_ctx_lock_call_func_non_block(domain, master_tgid, apm_master_ctx_inc_ref, (void *)(uintptr_t)udevid);
}

int apm_master_dec_ref(struct task_ctx_domain *domain, int master_tgid, u32 udevid)
{
    return task_ctx_lock_call_func_non_block(domain, master_tgid, apm_master_ctx_dec_ref, (void *)(uintptr_t)udevid);
}

static int apm_master_check_is_bind_slave(struct task_ctx *ctx, void *priv)
{
    struct master_ctx *m_ctx = (struct master_ctx *)ctx->priv;
    u32 udevid = (u32)(uintptr_t)priv;
    struct master_slave_node *slave_node = NULL;

    ka_task_spin_lock_bh(&ctx->lock);
    if (m_ctx->dev_ctx[udevid].num != 0) {
        ka_task_spin_unlock_bh(&ctx->lock);
        return -EEXIST;
    }

    ka_list_for_each_entry(slave_node, &m_ctx->head, node)
    {
        if ((slave_node->udevid == udevid) || (slave_node->udevid == UDA_INVALID_UDEVID)) {
            ka_task_spin_unlock_bh(&ctx->lock);
            return -EEXIST;
        }
    }
    ka_task_spin_unlock_bh(&ctx->lock);

    return 0;
}

int apm_master_check_all_master_is_bind_slave(struct task_ctx_domain *domain, u32 udevid)
{
    void *para = ((void *)(uintptr_t)udevid);

    return task_ctx_for_each_safe_ex(domain, para, apm_master_check_is_bind_slave, true);
}
