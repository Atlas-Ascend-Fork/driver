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
#include "ka_fs_pub.h"

#include "devmm_common.h"
#include "svm_proc_mng.h"
#include "svm_page_cnt_stats.h"
#include "svm_kapi_stats.h"
#include "svm_mem_stats.h"
#include "devmm_dev.h"
#include "svm_proc_fs.h"

#ifdef CONFIG_PROC_FS
ka_proc_dir_entry_t *devmm_task_entry = NULL;

static void devmm_proc_fs_format_task_dir_name(ka_pid_t pid, char *name, int len)
{
    if (sprintf_s(name, (unsigned long)len, "%d", pid) <= 0) { /* if fail just name is zero */
        devmm_drv_warn("Sprintf_s failed.\n");
    }
}

static ka_proc_dir_entry_t *devmm_proc_fs_mk_task_dir(ka_pid_t pid, ka_proc_dir_entry_t *parent)
{
    char name[DEVMM_PROC_FS_NAME_LEN] = {0};

    devmm_proc_fs_format_task_dir_name(pid, name, DEVMM_PROC_FS_NAME_LEN);
    return ka_fs_proc_mkdir((const char *)name, parent);
}

static void devmm_proc_fs_rm_task_dir(ka_pid_t pid, ka_proc_dir_entry_t *parent)
{
    char name[DEVMM_PROC_FS_NAME_LEN] = {0};

    devmm_proc_fs_format_task_dir_name(pid, name, DEVMM_PROC_FS_NAME_LEN);
    ka_fs_remove_proc_subtree((const char *)name, parent);
}

static void devmm_task_pg_cnt_stats_show(ka_seq_file_t *seq)
{
    struct devmm_svm_process *svm_proc = (struct devmm_svm_process *)ka_fs_get_seq_file_private(seq);
    u64 cgroup_used_page_cnt, cgroup_used_hpage_cnt, cgroup_used_gpage_cnt;
    u64 cdm_used_page_cnt, cdm_used_hpage_cnt, cdm_used_gpage_cnt;
    u64 peak_page_cnt, peak_hpage_cnt, peak_gpage_cnt;

    cdm_used_page_cnt = devmm_get_cdm_used_page_cnt(&svm_proc->pg_cnt_stats, DEVMM_NORMAL_PAGE_TYPE);
    cdm_used_hpage_cnt = devmm_get_cdm_used_page_cnt(&svm_proc->pg_cnt_stats, DEVMM_HUGE_PAGE_TYPE);
    cdm_used_gpage_cnt = devmm_get_cdm_used_page_cnt(&svm_proc->pg_cnt_stats, DEVMM_GIANT_PAGE_TYPE);

    cgroup_used_page_cnt = devmm_get_cgroup_used_page_cnt(&svm_proc->pg_cnt_stats, DEVMM_NORMAL_PAGE_TYPE);
    cgroup_used_hpage_cnt = devmm_get_cgroup_used_page_cnt(&svm_proc->pg_cnt_stats, DEVMM_HUGE_PAGE_TYPE);
    cgroup_used_gpage_cnt = devmm_get_cgroup_used_page_cnt(&svm_proc->pg_cnt_stats, DEVMM_GIANT_PAGE_TYPE);

    peak_page_cnt = devmm_get_peak_page_cnt(&svm_proc->pg_cnt_stats, DEVMM_NORMAL_PAGE_TYPE);
    peak_hpage_cnt = devmm_get_peak_page_cnt(&svm_proc->pg_cnt_stats, DEVMM_HUGE_PAGE_TYPE);
    peak_gpage_cnt = devmm_get_peak_page_cnt(&svm_proc->pg_cnt_stats, DEVMM_GIANT_PAGE_TYPE);

    ka_fs_seq_printf(seq,
                     "Svm page cnt stats:\nhostpid=%u; devpid=%u; devid=%u; vfid=%u; "
                     "cgroup_used_page_cnt=%llu; cgroup_used_hpage_cnt=%llu; cgroup_used_gpage_cnt=%llu; "
                     "cdm_used_page_cnt=%llu; cdm_used_hpage_cnt=%llu; cdm_used_gpage_cnt=%llu; "
                     "peak_page_cnt=%llu; peak_hpage_cnt=%llu; peak_gpage_cnt=%llu\n",
                     svm_proc->process_id.hostpid, svm_proc->devpid, svm_proc->process_id.devid,
                     svm_proc->process_id.vfid, cgroup_used_page_cnt, cgroup_used_hpage_cnt, cgroup_used_gpage_cnt,
                     cdm_used_page_cnt, cdm_used_hpage_cnt, cdm_used_gpage_cnt, peak_page_cnt, peak_hpage_cnt,
                     peak_gpage_cnt);
}

static void devmm_task_status_stats_show(ka_seq_file_t *seq)
{
    struct devmm_svm_process *svm_proc = (struct devmm_svm_process *)ka_fs_get_seq_file_private(seq);
    u32 i;

    ka_fs_seq_printf(seq, "\ntask status stats:\n");
    /* host or aicpu process status show */
    if (!ka_task_mutex_trylock(&svm_proc->proc_lock)) {
#ifndef EMU_ST
        devmm_drv_warn("Not get mutext lock, try next time. (pid=%u, devpid=%u)\n", svm_proc->process_id.hostpid,
                       svm_proc->devpid);
        return;
#endif
    }
    ka_fs_seq_printf(seq, "notifier_reg_flag=0x%x; status=%u; index=%u; msg_processing=%u; other_proc_occupying=%u\n",
                     svm_proc->notifier_reg_flag, svm_proc->proc_status, svm_proc->proc_idx, svm_proc->msg_processing,
                     svm_proc->other_proc_occupying);
    ka_task_mutex_unlock(&svm_proc->proc_lock);
    /* custom process status show */
    for (i = 0; i < DEVMM_CUSTOM_PROCESS_NUM; i++) {
        ka_task_mutex_lock(&svm_proc->custom[i].proc_lock);
        if (svm_proc->custom[i].status != DEVMM_CUSTOM_IDLE) {
            ka_fs_seq_printf(seq, "custom_pid=%u; index=%u; status=%u\n", svm_proc->custom[i].custom_pid,
                             svm_proc->custom[i].idx, svm_proc->custom[i].status);
        }
        ka_task_mutex_unlock(&svm_proc->custom[i].proc_lock);
    }
}

static int devmm_task_info_show(ka_seq_file_t *seq, void *offset)
{
/* docker path cannot run in emu st */
#ifndef EMU_ST
    struct devmm_svm_process *svm_proc = (struct devmm_svm_process *)ka_fs_get_seq_file_private(seq);
    /* cat information action just can run in the same docker or host */
    if (devmm_thread_is_run_in_docker() == true) {
        if (ka_task_get_current_mnt_ns() != devmm_get_proc_mnt_ns(svm_proc)) {
            return 0;
        }
    }
#endif
    devmm_task_pg_cnt_stats_show(seq);
    devmm_task_status_stats_show(seq);
    devmm_task_mem_stats_show(seq);
    return 0;
}

STATIC int devmm_task_sum_open(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_single_open(file, devmm_task_info_show, ka_base_pde_data(inode));
}

static const ka_procfs_ops_t devmm_task_sum_ops = {
    ka_fs_init_pf_owner(KA_THIS_MODULE) ka_fs_init_pf_open(devmm_task_sum_open) ka_fs_init_pf_read(ka_fs_seq_read)
        ka_fs_init_pf_lseek(ka_fs_seq_lseek) ka_fs_init_pf_release(ka_fs_single_release)};

void devmm_proc_fs_add_task(struct devmm_svm_process *svm_proc)
{
    ka_pid_t pid;

#ifndef EMU_ST /* emu_st all return true, cannot coverage */
    if ((devmm_task_entry == NULL) || (svm_proc->task_entry != NULL)) {
        return;
    }
#endif
    if (devmm_get_end_type() == DEVMM_END_DEVICE) {
        pid = svm_proc->devpid;
    } else {
        pid = svm_proc->process_id.hostpid;
    }
    svm_proc->task_entry = devmm_proc_fs_mk_task_dir(pid, devmm_task_entry);
    if (svm_proc->task_entry != NULL) {
        ka_fs_proc_create_data("summary", DEVMM_PROC_FS_MODE, svm_proc->task_entry, &devmm_task_sum_ops, svm_proc);
        return;
    }
}

void devmm_proc_fs_del_task(struct devmm_svm_process *svm_proc)
{
    ka_pid_t pid;

    if ((devmm_task_entry == NULL) || (svm_proc->task_entry == NULL)) {
        return;
    }
    if (devmm_get_end_type() == DEVMM_END_DEVICE) {
        pid = svm_proc->devpid;
    } else {
        pid = svm_proc->process_id.hostpid;
    }
    devmm_proc_fs_rm_task_dir(pid, devmm_task_entry);
    svm_proc->task_entry = NULL;
}

STATIC int devmm_sum_open(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_single_open(file, devmm_info_show, ka_base_pde_data(inode));
}

static const ka_procfs_ops_t devmm_sum_ops = {
    ka_fs_init_pf_owner(KA_THIS_MODULE) ka_fs_init_pf_open(devmm_sum_open) ka_fs_init_pf_read(ka_fs_seq_read)
        ka_fs_init_pf_lseek(ka_fs_seq_lseek) ka_fs_init_pf_release(ka_fs_single_release)};

STATIC int devmm_kapi_stats_open(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_single_open(file, devmm_kapi_stats_procfs_show, ka_base_pde_data(inode));
}

static const ka_procfs_ops_t devmm_kapi_stats_ops = {
    ka_fs_init_pf_owner(KA_THIS_MODULE) ka_fs_init_pf_open(devmm_kapi_stats_open) ka_fs_init_pf_read(ka_fs_seq_read)
        ka_fs_init_pf_lseek(ka_fs_seq_lseek) ka_fs_init_pf_release(ka_fs_single_release)};

static ka_proc_dir_entry_t *devmm_top_entry = NULL;
void devmm_proc_fs_init(struct devmm_svm_dev *svm_dev)
{
    devmm_top_entry = ka_fs_proc_mkdir("svm", NULL);
    if (devmm_top_entry != NULL) {
        devmm_task_entry = ka_fs_proc_mkdir("task", devmm_top_entry);
        ka_fs_proc_create_data("summary", DEVMM_PROC_FS_MODE, devmm_top_entry, &devmm_sum_ops, svm_dev);
        ka_fs_proc_create_data("kernel_api_stats", DEVMM_PROC_FS_MODE, devmm_top_entry, &devmm_kapi_stats_ops, NULL);
        devmm_dev_proc_fs_init();
    }
}

void devmm_proc_fs_uninit(void)
{
    if (devmm_top_entry != NULL) {
        devmm_dev_proc_fs_uninit();
        ka_fs_remove_proc_subtree("svm", NULL);
        devmm_top_entry = NULL;
        devmm_task_entry = NULL;
    }
}

ka_proc_dir_entry_t *devmm_get_top_entry(void)
{
    return devmm_top_entry;
}

#else
void devmm_proc_fs_add_task(struct devmm_svm_process *svm_proc)
{}

void devmm_proc_fs_del_task(struct devmm_svm_process *svm_proc)
{}

void devmm_proc_fs_init(struct devmm_svm_dev *svm_dev)
{
    return;
}

void devmm_proc_fs_uninit(void)
{}
#endif
