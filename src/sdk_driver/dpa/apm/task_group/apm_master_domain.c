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
#include "ka_base_pub.h"
#include "ka_common_pub.h"
#include "ka_compiler_pub.h"
#include "ka_dfx_pub.h"
#include "ka_ioctl_pub.h"
#include "ka_kernel_def_pub.h"

#include "apm_master_domain.h"
#include "apm_slave_domain.h"
#include "apm_task_exit.h"

#ifndef DRV_HOST
#include "drv_whitelist.h"
#endif

static struct task_ctx_domain *master_domain = NULL;
static struct apm_master_domain_ops *master_ops = NULL;
static struct apm_master_domain_cmd_ops *master_cmd_ops = NULL;
static const char *trusted_task_name = "tsdaemon";

static KA_DFX_BLOCKING_NOTIFIER_HEAD(master_exit_notifier);

struct apm_get_master_tgids_para {
    int *tgids;
    u32 tgids_array_len;
    u32 cnt;
};

static struct task_ctx_domain *apm_master_domain_query_domain_get(u32 udevid)
{
    return master_domain;
}

static void apm_master_domain_query_domain_put(struct task_ctx_domain *domain)
{}

static struct apm_master_query_domain_ops g_master_query_domain_ops = {
    .apm_master_query_domain_get = apm_master_domain_query_domain_get,
    .apm_master_query_domain_put = apm_master_domain_query_domain_put};

void apm_master_set_query_domain_ops(struct apm_master_query_domain_ops ops)
{
    g_master_query_domain_ops = ops;
}

static struct task_ctx_domain *apm_master_query_domain_get(u32 udevid)
{
    return g_master_query_domain_ops.apm_master_query_domain_get(udevid);
}

static void apm_master_query_domain_put(struct task_ctx_domain *domain)
{
    return g_master_query_domain_ops.apm_master_query_domain_put(domain);
}

bool apm_is_cur_task_trusted(void)
{
#if defined(DRV_HOST) && !defined(EMU_ST)
    (void)trusted_task_name;
    return false;
#else
#if defined(CFG_BUILD_DEBUG) && !defined(EMU_ST)
    (void)trusted_task_name;
    return true;
#else
    return (whitelist_process_handler(&trusted_task_name, 1) == 0);
#endif
#endif
}

void apm_master_domain_ops_register(struct apm_master_domain_ops *ops)
{
    master_ops = ops;
}

void apm_master_domain_cmd_ops_register(struct apm_master_domain_cmd_ops *ops)
{
    master_cmd_ops = ops;
}

static void apm_master_domain_ops_destroy(int tgid)
{
    if (master_ops != NULL) {
        struct task_ctx *ctx = task_ctx_get(master_domain, tgid);
        if (ctx != NULL) {
            master_ops->destroy((struct master_ctx *)ctx->priv, tgid);
            task_ctx_put(ctx);
        }
    }
}

static int apm_master_domain_ops_bind_unbind(int op, int master_tgid, int slave_tgid, struct apm_cmd_bind *para)
{
    struct task_ctx *ctx = NULL;
    int ret;

    if (master_ops == NULL) {
        return 0;
    }

    ctx = task_ctx_get(master_domain, master_tgid);
    if (ctx == NULL) {
        apm_err("Get ctx failed. (op=%d; tgid=%d; master_pid=%d)\n", op, master_tgid, para->master_pid);
        return -EFAULT;
    }

    if (op == APM_OP_BIND) {
        ret = master_ops->bind((struct master_ctx *)ctx->priv, master_tgid, slave_tgid, para);
    } else {
        ret = master_ops->unbind((struct master_ctx *)ctx->priv, master_tgid, slave_tgid, para);
    }

    task_ctx_put(ctx);

    return ret;
}

static int _apm_master_domain_add_slave(struct apm_cmd_bind *para, int master_tgid, int slave_tgid, int local_flag)
{
    int tgid = master_tgid, ret;

    ret = apm_master_add_slave(master_domain, tgid, slave_tgid, local_flag, para);
    if (ret != 0) {
        apm_err("Add failed. (ret=%d; devid=%u; proc_type=%d; mode=%d; slave_pid=%d; master_pid=%d)\n", ret,
                para->devid, para->proc_type, para->mode, para->slave_pid, para->master_pid);
        return ret;
    }

    if (local_flag == 1) {
        ret = apm_master_domain_ops_bind_unbind(APM_OP_BIND, tgid, slave_tgid, para);
        if (ret != 0) {
            (void)apm_master_del_slave(master_domain, tgid, slave_tgid, local_flag, para);
            apm_err("Ops failed. (ret=%d; devid=%u; proc_type=%d; mode=%d; slave_pid=%d; master_pid=%d)\n", ret,
                    para->devid, para->proc_type, para->mode, para->slave_pid, para->master_pid);
            return ret;
        }
    }

    return 0;
}

static int _apm_master_domain_del_slave(struct apm_cmd_bind *para, int master_tgid, int slave_tgid, int local_flag)
{
    int tgid = master_tgid, ret;

    ret = apm_master_del_slave(master_domain, tgid, slave_tgid, local_flag, para);
    if (ret != 0) {
        apm_warn("Del warn. (ret=%d; devid=%u; proc_type=%d; mode=%d; slave_pid=%d; master_pid=%d)\n", ret, para->devid,
                 para->proc_type, para->mode, para->slave_pid, para->master_pid);
    }

    if (local_flag == 1) {
        ret = apm_master_domain_ops_bind_unbind(APM_OP_UNBIND, tgid, slave_tgid, para);
        if (ret != 0) {
            apm_err("Ops failed. (ret=%d; devid=%u; proc_type=%d; mode=%d; slave_pid=%d; master_pid=%d)\n", ret,
                    para->devid, para->proc_type, para->mode, para->slave_pid, para->master_pid);
        }
    }

    return 0;
}

int apm_master_domain_add_slave(struct apm_cmd_bind *para, int master_tgid, int slave_tgid)
{
    return _apm_master_domain_add_slave(para, master_tgid, slave_tgid, 0);
}

int apm_master_domain_del_slave(struct apm_cmd_bind *para, int master_tgid, int slave_tgid)
{
    return _apm_master_domain_del_slave(para, master_tgid, slave_tgid, 0);
}

static int apm_master_domain_bind_slave(struct apm_cmd_bind *para, int master_tgid, int slave_tgid)
{
    return _apm_master_domain_add_slave(para, master_tgid, slave_tgid, 1);
}

static int apm_master_domain_unbind_slave(struct apm_cmd_bind *para, int master_tgid, int slave_tgid)
{
    int ret = apm_master_domain_pre_unbind(master_tgid, para);
    if (ret != 0) {
        return ret;
    }

    return _apm_master_domain_del_slave(para, master_tgid, slave_tgid, 1);
}

static int apm_master_domain_perm_check(struct apm_cmd_bind *para)
{
#ifdef CFG_SUPORT_GRANDCHILD_TASK
    struct apm_cmd_query_master_info m_info;
#endif
    int current_pid = ka_task_task_tgid_vnr(ka_task_get_current());

    if (!task_is_current_child_or_current(para->slave_pid)) {
#ifndef EMU_ST
        return -EOPNOTSUPP;
#endif
    }

    if (current_pid == para->master_pid) {
        /* The current process is the master process. */
        return 0;
    }

    /* The current process is in whitelist. */
    if (apm_is_cur_task_trusted()) {
        return 0;
    }

#ifdef CFG_SUPORT_GRANDCHILD_TASK
    /* the current process is all ready binded the master process */
    m_info.slave_pid = current_pid;
    if (apm_slave_domain_query_master(&m_info) == 0) {
        if (m_info.master_pid == para->master_pid) {
            return 0;
        }
    }
#endif

    return -EOPNOTSUPP;
}

static int apm_master_domain_get_master_tgid(int master_pid, int *master_tgid)
{
    return task_get_tgid_by_vpid(master_pid, master_tgid);
}

int apm_master_domain_set_slave_status(int master_tgid, u32 udevid, int slave_tgid, int type, int status,
                                       int local_flag)
{
    switch (type) {
        case SLAVE_STATUS_EXIT:
            return apm_master_set_slave_exit_stage(master_domain, master_tgid, udevid, slave_tgid, status, local_flag);
        case SLAVE_STATUS_OOM:
            return apm_master_set_slave_oom_status(master_domain, master_tgid, udevid, slave_tgid, status, local_flag);
        default:
            break;
    }

    return -EINVAL;
}

int apm_master_domain_get_slave_status(int master_tgid, u32 udevid, int proc_type, int type, int *status)
{
    switch (type) {
        case SLAVE_STATUS_OOM:
            return apm_master_get_slave_oom_status(master_domain, master_tgid, udevid, proc_type, status);
        default:
            break;
    }

    return -EINVAL;
}

int apm_master_query_domain_get_slave_ssid(int master_tgid, struct apm_cmd_slave_ssid *para)
{
    struct task_ctx_domain *query_master_domain = NULL;
    int ret;

    query_master_domain = apm_master_query_domain_get(para->devid);
    if (query_master_domain == NULL) {
        return -ENODEV;
    }

    ret = apm_master_get_slave_ssid(query_master_domain, master_tgid, para);
    apm_master_query_domain_put(query_master_domain);
    return ret;
}

int apm_master_query_domain_set_slave_ssid(int master_tgid, struct apm_cmd_slave_ssid *para)
{
    struct task_ctx_domain *query_master_domain = NULL;
    int ret;

    query_master_domain = apm_master_query_domain_get(para->devid);
    if (query_master_domain == NULL) {
        return -ENODEV;
    }

    ret = apm_master_set_slave_ssid(query_master_domain, master_tgid, para);
    apm_master_query_domain_put(query_master_domain);
    return ret;
}

static bool apm_master_domain_check_slave_in_task_group(int master_tgid, int slave_tgid, u32 udevid,
                                                        u32 proc_type_bitmap)
{
    struct apm_cmd_query_slave_pid para;
    int ret;
    u32 i;

    para.devid = udevid;
    para.query_in_all_stage = 1U;
    for (i = 0; i < (u32)APM_PROC_TYPE_NUM; i++) {
        if ((proc_type_bitmap & (1U << i)) == 0) {
            continue;
        }
        para.proc_type = (int)i;
        ret = apm_master_query_slave_pid(master_domain, master_tgid, &para);
        if ((ret == 0) && (para.slave_tgid == slave_tgid)) {
            break;
        }
    }

    if (i == (u32)APM_PROC_TYPE_NUM) {
        return false;
    }

    return true;
}

int apm_master_domain_get_tast_group_exit_stage(int master_tgid, int slave_tgid, u32 udevid, u32 proc_type_bitmap,
                                                int *exit_stage)
{
    int ret, master_stage, slave_stage;

    /* always true if slave not in master task group any more */
    if (!apm_master_domain_check_slave_in_task_group(master_tgid, slave_tgid, udevid, proc_type_bitmap)) {
        *exit_stage = APM_STAGE_MAX;
        return 0;
    }

    ret = apm_master_get_all_slave_exit_stage(master_domain, master_tgid, &slave_stage);
    ret |= apm_master_get_exit_stage(master_domain, master_tgid, &master_stage);
    if (ret == 0) {
        *exit_stage = (master_stage <= slave_stage) ? master_stage : slave_stage;
    }

    return ret;
}

int apm_master_domain_pre_unbind(int master_tgid, struct apm_cmd_bind *para)
{
    return apm_master_pre_unbind(master_domain, master_tgid, para);
}

int apm_master_use(int master_tgid, u32 udevid)
{
    if (udevid >= uda_get_udev_max_num()) {
        apm_err("Input invalid. (devid=%u; master_pid=%d)\n", udevid, master_tgid);
        return -EINVAL;
    }

    return apm_master_inc_ref(master_domain, master_tgid, udevid);
}
KA_EXPORT_SYMBOL_GPL(apm_master_use);

int apm_master_unuse(int master_tgid, u32 udevid)
{
    if (udevid >= uda_get_udev_max_num()) {
        apm_err("Input invalid. (devid=%u; master_pid=%d)\n", udevid, master_tgid);
        return -EINVAL;
    }

    return apm_master_dec_ref(master_domain, master_tgid, udevid);
}
KA_EXPORT_SYMBOL_GPL(apm_master_unuse);

static struct apm_slave_domain_ops m_slave_ops = {
    .perm_check = apm_master_domain_perm_check,
    .get_master_tgid = apm_master_domain_get_master_tgid,
    .bind = apm_master_domain_bind_slave,
    .unbind = apm_master_domain_unbind_slave,
    .set_status = apm_master_domain_set_slave_status,
    .get_tast_group_exit_stage = apm_master_domain_get_tast_group_exit_stage};

int apm_master_domain_tgid_to_pid(int tgid, int *master_pid)
{
    return apm_master_tgid_to_pid(master_domain, tgid, master_pid);
}

static int apm_fops_get_sign(u32 cmd, unsigned long arg)
{
    struct apm_cmd_get_sign *usr_arg = (struct apm_cmd_get_sign __ka_user *)(uintptr_t)arg;
    int tgid = ka_task_task_tgid_nr(ka_task_get_current());
    struct task_start_time start_time = {0};
    int ret;

    (void)task_get_start_time_by_tgid(tgid, &start_time);

    ret = apm_master_ctx_create(master_domain, tgid, (int)ka_task_task_tgid_vnr(ka_task_get_current()), &start_time);
    if (ret != 0) {
        if (ret != -EAGAIN) {
            apm_err("Create master ctx failed. (ret=%d; tgid=%d)\n", ret, tgid);
        }
        return ret;
    }

    apm_info("Get sign. (tgid=%d)\n", tgid);

    return (int)ka_base_put_user((u32)tgid, &usr_arg->cur_tgid);
}

static int apm_fops_query_slave_pid(u32 cmd, unsigned long arg)
{
    struct apm_cmd_query_slave_pid *usr_arg = (struct apm_cmd_query_slave_pid __ka_user *)(uintptr_t)arg;
    struct apm_cmd_query_slave_pid para;
    int tgid, ret;

    ret = (int)ka_base_copy_from_user(&para, usr_arg, sizeof(para));
    if (ret != 0) {
        apm_err("Copy from user failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = apm_devid_to_udevid(para.devid, &para.devid);
    if (ret != 0) {
        apm_err("Get udevid failed. (ret=%d; devid=%d)\n", ret, para.devid);
        return ret;
    }

    /* local domain query trans tgid */
    ret = task_get_tgid_by_vpid(para.master_pid, &tgid);
    if (ret != 0) {
#ifndef EMU_ST
        apm_warn("Get tgid warn. (master_pid=%d)\n", para.master_pid);
#endif
        return ret;
    }

    para.query_in_all_stage = 0;
    ret = apm_master_query_slave_pid(master_domain, tgid, &para);
    if (ret != 0) {
        apm_warn("Query slave pid warn. (ret=%d; master_pid=%d; tgid=%d; proc_type=%d)\n", ret, para.master_pid, tgid,
                 para.proc_type);
        return ret;
    }

    return (int)ka_base_copy_to_user(usr_arg, &para, sizeof(para));
}

static int apm_fops_query_slave_status(u32 cmd, unsigned long arg)
{
    struct apm_cmd_slave_status *usr_arg = (struct apm_cmd_slave_status __ka_user *)(uintptr_t)arg;
    struct apm_cmd_slave_status para;
    static int status_cmd_to_type[CMD_SLAVE_STATUS_TYPE_MAX] = {
        [CMD_SLAVE_STATUS_TYPE_OOM] = SLAVE_STATUS_OOM,
    };
    int ret;

    ret = (int)ka_base_copy_from_user(&para, usr_arg, sizeof(para));
    if (ret != 0) {
        apm_err("Copy from user failed. (ret=%d)\n", ret);
        return ret;
    }

    if ((para.type < 0) || (para.type >= CMD_SLAVE_STATUS_TYPE_MAX)) {
        apm_err("Invalid status type. (type=%d)\n", para.type);
        return -EINVAL;
    }

    ret = apm_devid_to_udevid(para.devid, &para.devid);
    if (ret != 0) {
        apm_err("Get udevid failed. (ret=%d; devid=%d)\n", ret, para.devid);
        return ret;
    }

    ret = apm_master_domain_get_slave_status(ka_task_get_current_tgid(), para.devid, para.proc_type,
                                             status_cmd_to_type[para.type], &para.status);
    if (ret != 0) {
#ifndef EMU_ST
        apm_err_ratelimited("Query status failed. (ret=%d; type=%d)\n", ret, para.type);
#endif
        return ret;
    }

    return (int)ka_base_copy_to_user(usr_arg, &para, sizeof(para));
}

static int apm_master_query_slave_meminfo(u32 udevid, int slave_tgid, processType_t process_type, processMemType_t type,
                                          u64 *size)
{
    if (master_cmd_ops != NULL) {
        return master_cmd_ops->query_meminfo(udevid, slave_tgid, process_type, type, size);
    }
    return 0;
}

static int apm_query_slave_meminfo_by_master(int master_tgid, unsigned int udevid, processType_t process_type,
                                             processMemType_t mem_type, unsigned long long *size)
{
    u32 slave_tgid;
    int ret;

    if ((size == NULL) || (udevid >= uda_get_udev_max_num())) {
        apm_err("Invalid size para or udevid. (size_is_null=%d; udevid=%u)\n", (size == NULL), udevid);
        return -EINVAL;
    }

    if ((mem_type >= PROC_MEM_MAX) || (process_type >= PROCESS_CPTYPE_MAX) || (process_type == PROCESS_USER)) {
        apm_debug("Unsupported type. (mem_type=%u; cp_type=%d)\n", mem_type, process_type);
        return -EOPNOTSUPP;
    }

    ret = hal_kernel_apm_query_slave_tgid_by_master(master_tgid, udevid, process_type, &slave_tgid);
    if (ret != 0) {
        apm_debug("Query slave pid failed. (ret=%d; master_tgid=%d; udevid=%u; proc_type=%d)\n", ret, master_tgid,
                  udevid, process_type);
        return ret;
    }

    if (process_type == PROCESS_CP2) {
        int cp1_tgid;
        ret = hal_kernel_apm_query_slave_tgid_by_master(master_tgid, udevid, PROCESS_CP1, &cp1_tgid);
        if (ret != 0) {
            return ret;
        }

        if (cp1_tgid == slave_tgid) {
            return -ESRCH;
        }
    }

    ret = apm_master_query_slave_meminfo(udevid, slave_tgid, process_type, mem_type, size);
    if (ret != 0) {
        apm_debug("Query slave meminfo failed. (ret=%d; udevid=%u; slave_tgid=%d; type=%u)\n", ret, udevid, slave_tgid,
                  mem_type);
        return ret;
    }

    return 0;
}

int apm_query_slave_all_meminfo_by_master(int master_tgid, unsigned int udevid, processType_t process_type,
                                          unsigned long long *size)
{
    return apm_query_slave_meminfo_by_master(master_tgid, udevid, process_type, PROC_MEM_TYPE_ALL, size);
}
KA_EXPORT_SYMBOL_GPL(apm_query_slave_all_meminfo_by_master);

static int apm_fops_query_slave_meminfo(u32 cmd, unsigned long arg)
{
    struct apm_cmd_slave_meminfo *usr_arg = (struct apm_cmd_slave_meminfo __ka_user *)(uintptr_t)arg;
    struct apm_cmd_slave_meminfo para;
    u32 udevid;
    int tgid, ret;

    ret = (int)ka_base_copy_from_user(&para, usr_arg, sizeof(para));
    if (ret != 0) {
        apm_err("Copy from user failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = apm_devid_to_udevid(para.devid, &udevid);
    if ((ret != 0) || (udevid == UDA_INVALID_UDEVID)) {
        apm_err("Get udevid failed. (ret=%d; devid=%d)\n", ret, para.devid);
        return -ENODEV;
    }

    ret = task_get_tgid_by_vpid(para.master_pid, &tgid);
    if (ret != 0) {
        apm_err("Get tgid failed. (master_pid=%d)\n", para.master_pid);
        return -ESRCH;
    }

    ret = apm_query_slave_meminfo_by_master(tgid, udevid, para.proc_type, para.type, &para.size);
    if (ret != 0) {
        apm_err_if(
            (para.proc_type == PROCESS_CP1) && (ret != -EOPNOTSUPP),
            "Query slave mem_info failed. (ret=%d; master_pid=%d; tgid=%d; udevid=%u; proc_type=%d; mem_type=%d)\n",
            ret, para.master_pid, tgid, udevid, para.proc_type, para.type);
        return ret;
    }

    return (int)ka_base_copy_to_user(usr_arg, &para, sizeof(para));
}

int hal_kernel_apm_query_slave_tgid_by_master(int master_tgid, u32 udevid, processType_t proc_type, int *slave_tgid)
{
    struct task_ctx_domain *query_master_domain = NULL;
    struct apm_cmd_query_slave_pid para;
    int ret;

    if (slave_tgid == NULL) {
        apm_err("Null ptr. (master_tgid=%d)\n", master_tgid);
        return -EINVAL;
    }

    if (udevid >= uda_get_udev_max_num()) {
        apm_err("Invalid para. (master_tgid=%d; udevid=%u)\n", master_tgid, udevid);
        return -EINVAL;
    }

    para.devid = udevid;
    para.proc_type = (int)proc_type;
    para.master_pid = master_tgid;
    para.query_in_all_stage = 0;

    query_master_domain = apm_master_query_domain_get(udevid);
    if (query_master_domain == NULL) {
        return -ENODEV;
    }

    ret = apm_master_query_slave_pid(query_master_domain, master_tgid, &para);
    if (ret != 0) {
        apm_debug_if((proc_type == PROCESS_CP1), "Query slave pid failed. (master_tgid=%d; udevid=%u)\n", master_tgid,
                     udevid);
        apm_master_query_domain_put(query_master_domain);
        return ret;
    }

    *slave_tgid = para.slave_tgid;
    apm_master_query_domain_put(query_master_domain);
    return 0;
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_apm_query_slave_tgid_by_master);

static void _apm_get_all_master_tgids(struct task_ctx *ctx, void *priv)
{
    struct apm_get_master_tgids_para *para = (struct apm_get_master_tgids_para *)priv;

    if (para->cnt < para->tgids_array_len) {
        para->tgids[para->cnt] = ctx->tgid;
        para->cnt++;
    }
}

int apm_get_all_master_tgids(int *tgids, unsigned int len, unsigned int *cnt)
{
    struct apm_get_master_tgids_para para = {0};

    if ((tgids == NULL) || (cnt == NULL) || (len == 0)) {
        apm_err("Tgids buffer or cnt is NULL or len is zero. (tgids_is_null=%d; cnt_is_null=%d; len=%u)\n",
                (tgids == NULL), (cnt == NULL), len);
        return -EINVAL;
    }

    para.tgids = tgids;
    para.tgids_array_len = len;
    task_ctx_for_each_safe(master_domain, (void *)&para, _apm_get_all_master_tgids);

    *cnt = para.cnt;
    return 0;
}
KA_EXPORT_SYMBOL_GPL(apm_get_all_master_tgids);

int apm_master_domain_check_all_master_is_bind_slave(u32 udevid)
{
    return apm_master_check_all_master_is_bind_slave(master_domain, udevid);
}

/* stub */
int devdrv_query_process_by_host_pid(unsigned int host_pid, unsigned int udevid, enum devdrv_process_type proc_type,
                                     unsigned int vfid, int *slave_pid)
{
    if (vfid != 0) {
        apm_err("Invalid para. (host_pid=%d; udevid=%u; vfid=%u)\n", host_pid, udevid, vfid);
        return -EINVAL;
    }

    return hal_kernel_apm_query_slave_tgid_by_master((int)host_pid, udevid, (processType_t)proc_type, slave_pid);
}
KA_EXPORT_SYMBOL_GPL(devdrv_query_process_by_host_pid);

int hal_kernel_devdrv_query_process_by_host_pid_kernel(unsigned int host_pid, /* esched use, delete later */
                                                       unsigned int chip_id, enum devdrv_process_type cp_type,
                                                       unsigned int vfid, int *pid)
{
    return devdrv_query_process_by_host_pid(host_pid, chip_id, cp_type, vfid, pid);
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_devdrv_query_process_by_host_pid_kernel);

int apm_master_exit_register(ka_notifier_block_t *n)
{
    return ka_dfx_blocking_notifier_chain_register(&master_exit_notifier, n);
}
KA_EXPORT_SYMBOL_GPL(apm_master_exit_register);

bool apm_master_domain_check_set_pre_exit(int tgid, struct task_start_time *time)
{
    return (apm_master_set_exit_stage(master_domain, tgid, time, APM_STAGE_PRE_EXIT) == 0);
}
KA_EXPORT_SYMBOL_GPL(apm_master_domain_check_set_pre_exit);

void apm_master_exit_unregister(ka_notifier_block_t *n)
{
    (void)ka_dfx_blocking_notifier_chain_unregister(&master_exit_notifier, n);
}
KA_EXPORT_SYMBOL_GPL(apm_master_exit_unregister);

static int apm_master_domain_task_exit_notifier(ka_notifier_block_t *self, unsigned long val, void *data)
{
    int tgid = apm_get_exit_tgid(val);
    int stage = apm_get_exit_stage(val);

    if (stage > (APM_STAGE_MAX - 1)) {
        goto exit;
    }

    (void)apm_master_set_exit_stage(master_domain, tgid, NULL, stage);

    if (stage == (APM_STAGE_MAX - 1)) {
        apm_master_domain_ops_destroy(tgid);
        apm_master_ctx_destroy(master_domain, tgid);
    }

exit:
    apm_info("Master exit notifier. (tgid=%d; stage=%d)\n", tgid, stage);
    return KA_NOTIFY_OK;
}

static ka_notifier_block_t apm_master_task_exit_nb = {
    .notifier_call = apm_master_domain_task_exit_notifier,
    .priority = APM_EXIT_NOTIFIY_PRI_APM_MASTER,
};

static bool apm_master_domain_is_exit_synchronized(int tgid, enum apm_exit_stage stage)
{
    int slave_stage;

    if (apm_master_get_all_slave_exit_stage(master_domain, tgid, &slave_stage) == 0) {
        /* master is later than slave, timeout will perform if slave not update exit stages when slave unbind */
        return (slave_stage >= (stage + 1));
    }

    return false;
}

void apm_master_domain_task_exit(u32 udevid, int tgid, struct task_start_time *start_time)
{
    if (apm_master_set_exit_stage(master_domain, tgid, start_time, APM_STAGE_PRE_EXIT) == 0) {
        apm_info("Master task exit. (tgid=%d)\n", tgid);
        apm_task_exit(tgid, &master_exit_notifier, apm_master_domain_is_exit_synchronized);
    } else {
        apm_info("Master task destroy. (tgid=%d)\n", tgid);
        apm_master_domain_ops_destroy(tgid);
        apm_master_ctx_destroy(master_domain, tgid);
    }
}
DECLAER_FEATURE_AUTO_UNINIT_TASK(apm_master_domain_task_exit, FEATURE_LOADER_STAGE_1);

void apm_master_domain_task_show(u32 udevid, int tgid, int feature_id, ka_seq_file_t *seq)
{
    if (tgid != 0) {
        apm_master_ctx_show(master_domain, tgid, seq);
    } else {
        task_ctx_domain_show(master_domain, seq);
    }
}
DECLAER_FEATURE_AUTO_SHOW_TASK(apm_master_domain_task_show, FEATURE_LOADER_STAGE_1);

int apm_master_domain_init(void)
{
    master_domain = task_ctx_domain_create("apm_master", MASTER_PROC_NUM);
    if (master_domain == NULL) {
        return -ENOMEM;
    }

    apm_register_ioctl_cmd_func(_KA_IOC_NR(APM_GET_SIGN), apm_fops_get_sign);
    apm_register_ioctl_cmd_func(_KA_IOC_NR(APM_QUERY_SLAVE_PID), apm_fops_query_slave_pid);
    apm_register_ioctl_cmd_func(_KA_IOC_NR(APM_QUERY_SLAVE_PID_BY_LOCAL_MASTER), apm_fops_query_slave_pid);
    apm_register_ioctl_cmd_func(_KA_IOC_NR(APM_QUERY_SLAVE_STATUS), apm_fops_query_slave_status);
    apm_register_ioctl_cmd_func(_KA_IOC_NR(APM_QUERY_SLAVE_MEMINFO), apm_fops_query_slave_meminfo);

    apm_slave_domain_ops_register(APM_ONLINE_MODE, &m_slave_ops);
    apm_slave_domain_ops_register(APM_OFFLINE_MODE, &m_slave_ops);

    (void)apm_master_exit_register(&apm_master_task_exit_nb);

    return 0;
}
DECLAER_FEATURE_AUTO_INIT(apm_master_domain_init, FEATURE_LOADER_STAGE_1);

void apm_master_domain_uninit(void)
{
    apm_master_exit_unregister(&apm_master_task_exit_nb);
    task_ctx_domain_destroy(master_domain);
    master_domain = NULL;
}
DECLAER_FEATURE_AUTO_UNINIT(apm_master_domain_uninit, FEATURE_LOADER_STAGE_1);
