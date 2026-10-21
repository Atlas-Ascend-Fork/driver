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
#include "ka_task_pub.h"

#include "apm_master_domain.h"
#include "apm_slave_meminfo.h"
#include "apm_task_group_def.h"
#include "apm_slave_domain.h"
#include "apm_task_exit.h"

static void *slave_domain = NULL;
static struct apm_slave_domain_ops *slave_ops[APM_MODE_NUM] = {
    NULL,
};
static struct apm_slave_mem_stats_ops *mem_stats_ops[APM_MODE_NUM] = {
    NULL,
};
static KA_DFX_BLOCKING_NOTIFIER_HEAD(slave_exit_notifier);
static ka_rw_semaphore_t apm_bind_query_sem;

void apm_slave_domain_ops_register(int mode, struct apm_slave_domain_ops *ops)
{
    slave_ops[mode] = ops;
}

void apm_slave_mem_stats_ops_register(int mode, struct apm_slave_mem_stats_ops *ops)
{
    mem_stats_ops[mode] = ops;
}

static int apm_bind(int tgid, int master_tgid, struct apm_cmd_bind *para)
{
    struct task_start_time start_time = {0};
    int ret;

    (void)task_get_start_time_by_tgid(tgid, &start_time);

    ret = apm_slave_ctx_create(slave_domain, tgid, para->slave_pid, &start_time);
    if (ret != 0) {
        if (ret != -EAGAIN) {
            apm_err("Create slave ctx failed. (tgid=%d; slave_pid=%d; ret=%d)\n", tgid, para->slave_pid, ret);
        }
        return ret;
    }

    ret = apm_slave_add_master(slave_domain, tgid, master_tgid, para);
    if (ret != 0) {           /* support multi bind, may not destroy slave_ctx */
        if (ret == -EEXIST) { /* repeat bind */
            return 0;
        }
        apm_err("Add master failed. (tgid=%d; slave_pid=%d; master_pid=%d)\n", tgid, para->slave_pid, para->master_pid);
        return ret;
    }

    /* notice master */
    ret = slave_ops[para->mode]->bind(para, master_tgid, tgid);
    if (ret != 0) {
        (void)apm_slave_del_master(slave_domain, tgid, para);
        apm_err("Bind failed. (tgid=%d; slave_pid=%d; master_pid=%d)\n", tgid, para->slave_pid, para->master_pid);
        return ret;
    }

    if ((mem_stats_ops[para->mode] != NULL) && (mem_stats_ops[para->mode]->bind != NULL)) {
        mem_stats_ops[para->mode]->bind(para->devid, para->proc_type, master_tgid, tgid);
    }

    return 0;
}

static int apm_unbind(int tgid, int master_tgid, struct apm_cmd_bind *para)
{
    int ret;

    ret = apm_slave_del_master(slave_domain, tgid, para);
    if (ret != 0) {
        apm_warn("Del master warn. (tgid=%d; slave_pid=%d; master_pid=%d)\n", tgid, para->slave_pid, para->master_pid);
        return ret;
    }

    /* notice master */
    ret = slave_ops[para->mode]->unbind(para, master_tgid, tgid);
    if (ret != 0) {
        if (ret == -EBUSY) {
            (void)apm_slave_add_master(slave_domain, tgid, master_tgid, para);
            return ret;
        }
        apm_warn("Unbind failed. (tgid=%d; slave_pid=%d; master_pid=%d)\n", tgid, para->slave_pid, para->master_pid);
    }

    if ((mem_stats_ops[para->mode] != NULL) && (mem_stats_ops[para->mode]->unbind != NULL)) {
        mem_stats_ops[para->mode]->unbind(para->devid, para->proc_type, tgid);
    }

    return 0;
}

static int _apm_fops_bind_unbind(u32 cmd, struct apm_cmd_bind *para)
{
    int tgid, master_tgid, ret;

    if ((para->mode != APM_ONLINE_MODE) && (para->mode != APM_OFFLINE_MODE)) {
        apm_err("Invalid mode. (cmd=%d; mode=%d)\n", _KA_IOC_NR(cmd), para->mode);
        return -EINVAL;
    }

    ret = apm_devid_to_udevid(para->devid, &para->devid);
    if (ret != 0) {
        apm_err("Get udevid failed. (cmd=%d; ret=%d; devid=%d)\n", _KA_IOC_NR(cmd), ret, para->devid);
        return ret;
    }

    ret = task_get_tgid_by_vpid(para->slave_pid, &tgid);
    if (ret != 0) {
        apm_err("Get tgid failed. (cmd=%d; ret=%d; slave_pid=%d)\n", _KA_IOC_NR(cmd), ret, para->slave_pid);
        return ret;
    }

    ret = slave_ops[para->mode]->get_master_tgid(para->master_pid, &master_tgid);
    if (ret != 0) {
        apm_err("Get master tgid failed. (cmd=%d; ret=%d; master_pid=%d)\n", _KA_IOC_NR(cmd), ret, para->master_pid);
        return ret;
    }

#ifdef DRV_HOST
    if (para->proc_type != PROCESS_USER) {
        apm_warn("Not supported proc_type. (cmd=%d; proc_type=%d)\n", _KA_IOC_NR(cmd), para->proc_type);
        return -EOPNOTSUPP;
    }
#endif

    /* Security Verification */
    ret = slave_ops[para->mode]->perm_check(para);
    if (ret != 0) {
        apm_err("No bind or unbind permission. (cmd=%d; master_pid=%d; slave_pid=%d)\n", _KA_IOC_NR(cmd),
                para->master_pid, para->slave_pid);
        return ret;
    }

    if (cmd == APM_BIND) {
        ret = apm_bind(tgid, master_tgid, para);
    } else {
        ret = apm_unbind(tgid, master_tgid, para);
    }

    return ret;
}

static int apm_fops_bind_unbind(u32 cmd, unsigned long arg)
{
    struct apm_cmd_bind *usr_arg = (struct apm_cmd_bind __ka_user *)(uintptr_t)arg;
    struct apm_cmd_bind para;
    int ret;

    ret = (int)ka_base_copy_from_user(&para, usr_arg, sizeof(para));
    if (ret != 0) {
        apm_err("Copy from user failed. (cmd=%d; ret=%d)\n", _KA_IOC_NR(cmd), ret);
        return ret;
    }

    ka_task_down_write(&apm_bind_query_sem);
    ret = _apm_fops_bind_unbind(cmd, &para);
    if (ret != 0) {
        ka_task_up_write(&apm_bind_query_sem);
        apm_err_if(
            ((ret != -EBUSY) && (ret != -EOPNOTSUPP) &&
             (ret != -EAGAIN)), /* -EAGAIN:will retry bind when slave_pid is still exiting */
            "Bind or unbind failed. (cmd=%d; ret=%d; devid=%u; proc_type=%d; mode=%d; slave_pid=%d; master_pid=%d)\n",
            _KA_IOC_NR(cmd), ret, para.devid, para.proc_type, para.mode, para.slave_pid, para.master_pid);
        return ret;
    }

    ka_task_up_write(&apm_bind_query_sem);
    apm_info("success. (cmd=%d; devid=%u; proc_type=%d; mode=%d; slave_pid=%d; master_pid=%d)\n", _KA_IOC_NR(cmd),
             para.devid, para.proc_type, para.mode, para.slave_pid, para.master_pid);

    return 0;
}

int apm_slave_domain_query_master(struct apm_cmd_query_master_info *para)
{
    int tgid, ret;

    ret = task_get_tgid_by_vpid(para->slave_pid, &tgid);
    if (ret != 0) {
        apm_err("Get tgid failed. (slave_pid=%d)\n", para->slave_pid);
        return ret;
    }

    return apm_slave_query_master(slave_domain, tgid, para);
}

static int apm_fops_query_master_pid(u32 cmd, unsigned long arg)
{
    struct apm_cmd_query_master_info *usr_arg = (struct apm_cmd_query_master_info __ka_user *)(uintptr_t)arg;
    struct apm_cmd_query_master_info para;
    int ret;

    ret = (int)ka_base_copy_from_user(&para, usr_arg, sizeof(para));
    if (ret != 0) {
        apm_err("Copy from user failed. (ret=%d)\n", ret);
        return ret;
    }

    ka_task_down_read(&apm_bind_query_sem);
    ret = apm_slave_domain_query_master(&para);
    if (ret != 0) {
        ka_task_up_read(&apm_bind_query_sem);
        apm_debug("Query warning. (ret=%d; slave_pid=%d)\n", ret, para.slave_pid);
        return ret;
    }

    ka_task_up_read(&apm_bind_query_sem);
    return (int)ka_base_copy_to_user(usr_arg, &para, sizeof(para));
}

int apm_slave_domain_tgid_to_pid(int tgid, int *slave_pid)
{
    return apm_slave_tgid_to_pid(slave_domain, tgid, slave_pid);
}

int apm_slave_domain_get_ssid(int slave_tgid, struct apm_cmd_slave_ssid *para)
{
    return apm_slave_get_ssid(slave_domain, slave_tgid, para);
}

int apm_slave_domain_set_ssid(int slave_tgid, struct apm_cmd_slave_ssid *para)
{
    return apm_slave_set_ssid(slave_domain, slave_tgid, para);
}

int apm_query_master_info_by_slave(int slave_tgid, int *master_tgid, u32 *udevid, int *mode, u32 *proc_type_bitmap)
{
    struct apm_cmd_query_master_info para;
    int ret, tgid = slave_tgid;

    if ((master_tgid == NULL) || (udevid == NULL) || (proc_type_bitmap == NULL)) {
        apm_err("Null ptr. (tgid=%d)\n", tgid);
        return -EINVAL;
    }

    para.slave_pid = (unsigned int)slave_tgid;
    ret = apm_slave_query_master(slave_domain, tgid, &para);
    if (ret != 0) {
        return ret;
    }

    *master_tgid = para.master_tgid;
    *udevid = para.udevid;
    *mode = para.mode;
    *proc_type_bitmap = para.proc_type_bitmap;

    return 0;
}
KA_EXPORT_SYMBOL_GPL(apm_query_master_info_by_slave);

int hal_kernel_devdrv_query_process_host_pid(int slave_pid, unsigned int *udevid, unsigned int *vfid,
                                             unsigned int *host_pid, enum devdrv_process_type *proc_type)
{
    u32 proc_type_bitmap;
    int ret, mode;

    if (vfid == NULL) {
        apm_err("Null ptr. (tgid=%d)\n", slave_pid);
        return -EINVAL;
    }

    ret = apm_query_master_info_by_slave(slave_pid, (int *)host_pid, udevid, &mode, &proc_type_bitmap);
    if (ret != 0) {
        return ret;
    }

    *vfid = 0;
    *proc_type = (enum devdrv_process_type)apm_trans_proc_type_from_bitmap(proc_type_bitmap);

    return 0;
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_devdrv_query_process_host_pid);

int devdrv_query_process_host_pids_by_pid(int slave_pid, devdrv_host_pids_info_t *host_pids_info)
{
    u32 udevid, proc_type_bitmap;
    int ret, master_tgid, mode, proc_type;

    if (host_pids_info == NULL) {
        apm_err("Null ptr. (tgid=%d)\n", slave_pid);
        return -EINVAL;
    }

    ret = apm_query_master_info_by_slave(slave_pid, &master_tgid, &udevid, &mode, &proc_type_bitmap);
    if (ret != 0) {
        return ret;
    }

    host_pids_info->vaild_num = 0;
    for (proc_type = 0; proc_type < APM_PROC_TYPE_NUM; proc_type++) {
        if ((proc_type_bitmap & (0x1 << proc_type)) != 0) {
            host_pids_info->chip_id[host_pids_info->vaild_num] = udevid;
            host_pids_info->vfid[host_pids_info->vaild_num] = 0;
            host_pids_info->host_pids[host_pids_info->vaild_num] = (ka_pid_t)master_tgid;
            host_pids_info->cp_type[host_pids_info->vaild_num] = (u32)proc_type;
            host_pids_info->vaild_num++;
        }
    }

    return 0;
}
KA_EXPORT_SYMBOL_GPL(devdrv_query_process_host_pids_by_pid);

static void apm_try_unbind_all(int tgid)
{
    struct apm_cmd_query_master_info para;
    struct apm_cmd_bind unbind;
    int ret, proc_type;

    ret = apm_slave_query_master(slave_domain, tgid, &para);
    if (ret != 0) {
        return;
    }

    unbind.devid = para.udevid;
    unbind.mode = para.mode;
    unbind.slave_pid = (int)para.slave_pid;
    unbind.master_pid = (int)para.master_pid;

    for (proc_type = 0; proc_type < APM_PROC_TYPE_NUM; proc_type++) {
        if ((para.proc_type_bitmap & (0x1 << proc_type)) != 0) {
            unbind.proc_type = proc_type;
            ret = apm_unbind(tgid, para.master_tgid, &unbind);
            apm_info("Recycle. (ret=%d; tgid=%d; devid=%u; proc_type=%d; mode=%d; slave_pid=%d; master_pid=%d)\n", ret,
                     tgid, unbind.devid, unbind.proc_type, unbind.mode, unbind.slave_pid, unbind.master_pid);
        }
    }
}

static void apm_mem_stats_unbind_all(int tgid)
{
    struct apm_cmd_query_master_info para;
    int ret, proc_type;

    ret = apm_slave_query_master(slave_domain, tgid, &para);
    if (ret != 0) {
        return;
    }

    for (proc_type = 0; proc_type < APM_PROC_TYPE_NUM; proc_type++) {
        if ((para.proc_type_bitmap & (0x1 << proc_type)) != 0) {
            if ((mem_stats_ops[para.mode] != NULL) && (mem_stats_ops[para.mode]->unbind != NULL)) {
                mem_stats_ops[para.mode]->unbind(para.udevid, proc_type, tgid);
            }
        }
    }
}

static inline bool apm_is_slave_task(int tgid)
{
    int master_tgid;
    return (apm_query_master_tgid_by_slave(tgid, &master_tgid) == 0);
}

static int apm_slave_domain_task_set_status(int tgid, int type, int status)
{
    int master_tgid, mode, ret;
    u32 udevid, proc_type_bitmap;

    ret = apm_query_master_info_by_slave(tgid, &master_tgid, &udevid, &mode, &proc_type_bitmap);
    if (ret != 0) {
        return ret;
    }

    return slave_ops[mode]->set_status(master_tgid, udevid, tgid, type, status, 1);
}

void apm_try_to_set_slave_oom(int tgid, int oom_status)
{
    if (apm_is_slave_task(tgid)) {
        if (apm_slave_domain_task_set_status(tgid, SLAVE_STATUS_OOM, oom_status) != 0) {
            apm_warn("Notice master failed. (tgid=%d; oom_status=%d)\n", tgid, oom_status);
        }
    }
}

int apm_slave_exit_register(ka_notifier_block_t *n)
{
    return ka_dfx_blocking_notifier_chain_register(&slave_exit_notifier, n);
}
KA_EXPORT_SYMBOL_GPL(apm_slave_exit_register);

void apm_slave_exit_unregister(ka_notifier_block_t *n)
{
    (void)ka_dfx_blocking_notifier_chain_unregister(&slave_exit_notifier, n);
}
KA_EXPORT_SYMBOL_GPL(apm_slave_exit_unregister);

static int apm_slave_domain_task_exit_notifier(ka_notifier_block_t *self, unsigned long val, void *data)
{
    int tgid = apm_get_exit_tgid(val);
    int stage = apm_get_exit_stage(val);

    if (stage > (APM_STAGE_MAX - 1)) {
        goto exit;
    }

    if (apm_slave_domain_task_set_status(tgid, SLAVE_STATUS_EXIT, stage) != 0) {
        apm_warn("Notice master warn. (tgid=%d; stage=%d)\n", tgid, stage);
    }

    if (stage == (APM_STAGE_MAX - 1)) {
        apm_try_unbind_all(tgid);
        apm_slave_ctx_destroy(slave_domain, tgid);
    }

exit:
    apm_info("Slave exit notifier. (tgid=%d; stage=%d)\n", tgid, stage);
    return KA_NOTIFY_OK;
}

static ka_notifier_block_t apm_slave_task_exit_nb = {
    .notifier_call = apm_slave_domain_task_exit_notifier,
    .priority = APM_EXIT_NOTIFIY_PRI_APM_SLAVE,
};

static bool apm_slave_domain_is_exit_synchronized(int tgid, enum apm_exit_stage stage)
{
    int master_tgid, mode, ret;
    u32 udevid, proc_type_bitmap;
    int task_group_exit_stage;

    ret = apm_query_master_info_by_slave(tgid, &master_tgid, &udevid, &mode, &proc_type_bitmap);
    if (ret != 0) {
        return true; /* no sync needed if none bind relation exist */
    }
    ret = slave_ops[mode]->get_tast_group_exit_stage(master_tgid, tgid, udevid, proc_type_bitmap,
                                                     &task_group_exit_stage);
    if (ret != 0) {
        return false;
    }

    return (task_group_exit_stage >= stage) ||
           (apm_slave_ctx_need_fast_exit(slave_domain, tgid) && (task_group_exit_stage == 0));
}

bool apm_slave_domain_check_set_pre_exit(int tgid, struct task_start_time *time)
{
    return (apm_slave_check_set_exit_status(slave_domain, tgid, time, APM_SLAVE_CHECK_EXIT_FROM_EXTERNAL) == 0);
}
KA_EXPORT_SYMBOL_GPL(apm_slave_domain_check_set_pre_exit);

void apm_slave_domain_task_exit(u32 udevid, int tgid, struct task_start_time *start_time)
{
    int ret = 0;

    ret = apm_slave_check_set_exit_status(slave_domain, tgid, start_time, APM_SLAVE_CHECK_EXIT_FROM_APM);
    if ((ret == -EEXIST) || ret == -ESRCH) {
        return;
    }
    apm_mem_stats_unbind_all(tgid);

    if (ret == 0) {
        apm_info("Slave task exit. (slave_tgid=%d)\n", tgid);
        apm_task_exit(tgid, &slave_exit_notifier, apm_slave_domain_is_exit_synchronized);
    } else {
        /* slave ctx may be created, but not bind host. try destroy ctx */
        apm_info("Slave ctx destroy. (slave_tgid=%d)\n", tgid);
        apm_slave_ctx_destroy(slave_domain, tgid);
    }
}
DECLAER_FEATURE_AUTO_UNINIT_TASK(apm_slave_domain_task_exit, FEATURE_LOADER_STAGE_2);

static void apm_slave_task_exit_check(struct task_ctx *ctx, void *priv)
{
    if (dbl_task_is_dead(ctx->tgid, &ctx->start_time)) {
        apm_slave_domain_task_exit(0, ctx->tgid, &ctx->start_time);
    }
}

int apm_slave_domain_task_exit_check(void *priv)
{
    task_ctx_for_each_safe(slave_domain, NULL, apm_slave_task_exit_check);
    return 0;
}
DECLAER_FEATURE_AUTO_INIT_BY_SCOPE(EXIT_CHECK_SCOPE, apm_slave_domain_task_exit_check, FEATURE_LOADER_STAGE_1);

void apm_slave_domain_task_show(u32 udevid, int tgid, int feature_id, ka_seq_file_t *seq)
{
    if (tgid != 0) {
        apm_slave_ctx_show(slave_domain, tgid, seq);
    } else {
        task_ctx_domain_show(slave_domain, seq);
    }
}

static int apm_slave_domain_get_meminfo(u32 udevid, int slave_tgid, processType_t process_type, processMemType_t type,
                                        u64 *size)
{
    (void)process_type;
    return apm_get_slave_meminfo(slave_tgid, type, size);
}

static struct apm_master_domain_cmd_ops master_domain_cmd_ops = {
    .query_meminfo = apm_slave_domain_get_meminfo,
};

DECLAER_FEATURE_AUTO_SHOW_TASK(apm_slave_domain_task_show, FEATURE_LOADER_STAGE_2);

int apm_slave_domain_init(void)
{
    slave_domain = task_ctx_domain_create("apm_slave", 0);
    if (slave_domain == NULL) {
        return -ENOMEM;
    }

    ka_task_init_rwsem(&apm_bind_query_sem);

    apm_register_ioctl_cmd_func(_KA_IOC_NR(APM_BIND), apm_fops_bind_unbind);
    apm_register_ioctl_cmd_func(_KA_IOC_NR(APM_UNBIND), apm_fops_bind_unbind);
    apm_register_ioctl_cmd_func(_KA_IOC_NR(APM_QUERY_MASTER_INFO), apm_fops_query_master_pid);

    (void)apm_slave_exit_register(&apm_slave_task_exit_nb);
    apm_master_domain_cmd_ops_register(&master_domain_cmd_ops);

    return 0;
}
DECLAER_FEATURE_AUTO_INIT(apm_slave_domain_init, FEATURE_LOADER_STAGE_2);

void apm_slave_domain_uninit(void)
{
    apm_slave_exit_unregister(&apm_slave_task_exit_nb);
    task_ctx_domain_destroy(slave_domain);
    slave_domain = NULL;
}
DECLAER_FEATURE_AUTO_UNINIT(apm_slave_domain_uninit, FEATURE_LOADER_STAGE_2);
