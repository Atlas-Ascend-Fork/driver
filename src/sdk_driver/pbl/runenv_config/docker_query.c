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
#include "ka_common_pub.h"
#include "ka_task_pub.h"
#include "ka_system_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_fs_pub.h"

#include "pbl/pbl_runenv_config.h"

#include "runenv_config_module.h"
#include "docker_query.h"

extern struct task_struct init_task;

static bool is_admin_task(struct task_struct *tsk)
{
    ka_kernel_cap_t privileged = ka_system_get_privileged_kernel_cap();
    const ka_cred_t *cred = NULL;
    ka_kernel_cap_t effective;
    unsigned int user_id;
    struct user_namespace *user_ns;

    ka_task_rcu_read_lock();
    cred = __ka_task_task_cred(tsk); // lint !e1058 !e64 !e666
    user_id = cred->uid.val;
    effective = cred->cap_effective;
#ifndef EMU_ST
    user_ns = cred->user_ns;
#endif
    ka_task_rcu_read_unlock();

#ifndef EMU_ST
    if (user_ns != &init_user_ns) {
        return false;
    }
#endif

    if (ka_system_kernel_cap_compare(effective, privileged) == false) {
        return false;
    }

    if (user_id == 0) {
        return true;
    } else {
        return false;
    }
}

STATIC bool cgroup_mount_is_ro(void)
{
    struct path kernel_path;
    int ret;
    bool is_ro = false;

    ret = ka_fs_kern_path("/sys/fs/cgroup/memory", 0, &kernel_path);
    if (ret != 0) {
        return false;
    }
    if (kernel_path.mnt == NULL) {
        recfg_err("Cgroup memory path mount is NULL.\n");
        ka_fs_path_put(&kernel_path);
        return false;
    }
    is_ro = __mnt_is_readonly(kernel_path.mnt);
    ka_fs_path_put(&kernel_path);
    return is_ro;
}

bool dbl_current_is_admin(void)
{
    /* In the process exit path (after exit_fs/exit_task_namespaces), current->fs and current->nsproxy may be NULL.
     * At this point, cgroup_mount_is_ro() → ka_fs_kern_path() will access the NULL pointer of current->fs,
     * so capability checks are used instead (RCU protects credentials, ensuring safety) */
    if (ka_task_get_current()->flags & PF_EXITING) {
        recfg_warn("Current task is exiting.\n");
        return is_admin_task(ka_task_get_current());
    }

    if (!is_admin_task(ka_task_get_current())) {
        return false;
    }
    /* for compatibility, only check ro as not admin, in other scenarios, admin is returned */
    if (cgroup_mount_is_ro()) {
        return false;
    }
    return true;
}
KA_EXPORT_SYMBOL(dbl_current_is_admin);

static int task_struct_check(struct task_struct *tsk)
{
    if (tsk == NULL) {
        recfg_err("The tsk is NULL\n");
        return -EINVAL;
    }

    if (tsk->nsproxy == NULL) {
        recfg_err("The tsk->nsproxy is NULL\n");
        return -ENODEV;
    }
    if (tsk->nsproxy->mnt_ns == NULL) {
        recfg_err("The tsk->nsproxy->mnt_ns is NULL\n");
        return -EFAULT;
    }
    return 0;
}

static ka_mnt_namespace_t *get_current_mnt_ns(void)
{
    int ret;

    ret = task_struct_check(ka_task_get_current());
    if (ret) {
        recfg_err("Current is invalid. (ret=%d)\n", ret);
        return NULL;
    }

    return ka_task_get_current_mnt_ns();
}

static ka_mnt_namespace_t *get_host_mnt_ns(void)
{
    return init_task.nsproxy->mnt_ns;
}

bool check_in_host_mach(void)
{
    ka_mnt_namespace_t *current_ns = NULL;
    ka_mnt_namespace_t *host_ns = NULL;

    current_ns = get_current_mnt_ns();
    if (current_ns == NULL) {
        return false;
    }

    host_ns = get_host_mnt_ns();
    if (host_ns == NULL) {
        return false;
    }

    if (current_ns == host_ns) {
        return true;
    }

    return false;
}

bool run_in_normal_docker(void)
{
    if ((!check_in_host_mach()) && (!dbl_current_is_admin())) {
        recfg_debug("In normal docker.\n");
        return true;
    }

    return false;
}
KA_EXPORT_SYMBOL(run_in_normal_docker);

bool run_in_admin_docker(void)
{
    if ((!check_in_host_mach()) && dbl_current_is_admin()) {
        recfg_debug("In admin docker.\n");
        return true;
    }

    return false;
}
KA_EXPORT_SYMBOL(run_in_admin_docker);

bool run_in_docker(void)
{
    if (!check_in_host_mach()) {
        recfg_debug("In docker.\n");
        return true;
    }

    return false;
}
KA_EXPORT_SYMBOL(run_in_docker);

int dbl_host_pid_to_container_pid(pid_t host_pid, pid_t *container_pid)
{
#if (!defined DMS_UT) && (!defined EMU_ST)
    ka_struct_pid_t *pgrp = NULL;
    ka_task_struct_t *tsk = NULL;
    ka_task_struct_t *g = NULL;

    if (!run_in_docker()) {
        *container_pid = host_pid;
        return 0;
    }

    ka_for_each_process_thread(g, tsk)
    {
        if (tsk != NULL) {
            ka_task_task_lock(tsk);
            if (tsk->pid == host_pid) {
                pgrp = ka_task_task_pid(tsk);
                if (pgrp == NULL) {
                    recfg_err("The process group parameter is NULL.\n");
                    ka_task_task_unlock(tsk);
                    return -EINVAL;
                }
                *container_pid = pgrp->numbers[pgrp->level].nr;
                ka_task_task_unlock(tsk);
                return 0;
            }
            ka_task_task_unlock(tsk);
        }
    }
    return -EINVAL;
#else
    return 0;
#endif
}
KA_EXPORT_SYMBOL(dbl_host_pid_to_container_pid);