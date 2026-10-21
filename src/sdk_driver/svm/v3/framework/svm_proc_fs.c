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
#include "ka_fs_pub.h"
#include "ka_kernel_def_pub.h"

#include "pbl_feature_loader.h"
#include "pbl/pbl_runenv_config.h"

#include "securec.h"
#include "svm_task.h"
#include "svm_proc_fs.h"

#define SVM_PROC_TOP_NAME "svm"
#define SVM_PROC_FS_MODE 0444
#define SVM_PROC_NAME_LEN 64

static ka_proc_dir_entry_t *svm_top_entry = NULL;
static int svm_task_feature_show(ka_seq_file_t *seq, void *offset)
{
    u64 priv = (u64)(uintptr_t)(ka_fs_get_seq_file_private(seq));
    u32 udevid = (u32)(priv >> 32) & 0xffff; /* 0xffff udevid bit 32-47 */
    u32 feature_id = (u32)(priv >> 48);      /* feature_id bit 48-63 */
    int tgid = (int)priv;

    if (run_in_normal_docker()) {
        return 0;
    }

    module_feature_auto_show_task(udevid, tgid, feature_id, seq);
    return 0;
}

int svm_task_feature_open(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_single_open(file, svm_task_feature_show, ka_base_pde_data(inode));
}

static int svm_dev_feature_show(ka_seq_file_t *seq, void *offset)
{
    u64 priv = (u64)(uintptr_t)(ka_fs_get_seq_file_private(seq));
    u32 udevid = (u32)(priv >> 32); /* udevid high 32 bit */
    u32 feature_id = (u32)priv;

    if (run_in_normal_docker()) {
        return 0;
    }

    module_feature_auto_show_dev(udevid, feature_id, seq);
    return 0;
}

int svm_dev_feature_open(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_single_open(file, svm_dev_feature_show, ka_base_pde_data(inode));
}

static const ka_procfs_ops_t svm_task_feature = {
    ka_fs_init_pf_owner(KA_THIS_MODULE) ka_fs_init_pf_open(svm_task_feature_open) ka_fs_init_pf_read(ka_fs_seq_read)
        ka_fs_init_pf_lseek(ka_fs_seq_lseek) ka_fs_init_pf_release(ka_fs_single_release)};

static const ka_procfs_ops_t svm_dev_feature = {
    ka_fs_init_pf_owner(KA_THIS_MODULE) ka_fs_init_pf_open(svm_dev_feature_open) ka_fs_init_pf_read(ka_fs_seq_read)
        ka_fs_init_pf_lseek(ka_fs_seq_lseek) ka_fs_init_pf_release(ka_fs_single_release)};

ka_proc_dir_entry_t *svm_proc_fs_add_task(u32 udevid, ka_proc_dir_entry_t *dev_entry, int tgid, u32 task_id)
{
    char name[SVM_PROC_NAME_LEN];

    (void)sprintf_s(name, SVM_PROC_NAME_LEN, "task-%d_id-%u", tgid, task_id);
    return ka_fs_proc_mkdir(name, dev_entry);
}

void svm_proc_fs_del_task(ka_proc_dir_entry_t *task_entry)
{
    if (task_entry != NULL) {
        ka_fs_proc_remove(task_entry);
    }
}

void svm_proc_task_add_feature(u32 udevid, int tgid, ka_proc_dir_entry_t *task_entry, u32 feature_id,
                               const char *feature_name)
{
    u64 priv = (u32)tgid | ((u64)(udevid) << 32) | ((u64)(feature_id) << 48); /* udevid 32-47, feature_id 48-63 */
    (void)ka_fs_proc_create_data(feature_name, SVM_PROC_FS_MODE, task_entry, &svm_task_feature,
                                 (void *)(uintptr_t)priv);
}

ka_proc_dir_entry_t *svm_proc_fs_add_dev(u32 udevid, u32 inst)
{
    char name[SVM_PROC_NAME_LEN];

    if (svm_top_entry == NULL) {
        return NULL;
    }

    (void)sprintf_s(name, SVM_PROC_NAME_LEN, "dev-%u_inst-%u", udevid, inst);
    return ka_fs_proc_mkdir(name, svm_top_entry);
}

void svm_proc_fs_del_dev(ka_proc_dir_entry_t *dev_entry)
{
    if (dev_entry != NULL) {
        ka_fs_proc_remove(dev_entry);
    }
}

void svm_proc_dev_add_feature(u32 udevid, ka_proc_dir_entry_t *dev_entry, u32 feature_id, const char *feature_name)
{
    u64 priv = (u32)feature_id | ((u64)(udevid) << 32); /* udevid high 32 bit */
    (void)ka_fs_proc_create_data(feature_name, SVM_PROC_FS_MODE, dev_entry, &svm_dev_feature, (void *)(uintptr_t)priv);
}

void svm_proc_fs_add_feature(const char *name, const ka_procfs_ops_t *ops, void *data)
{
    if (svm_top_entry == NULL) {
        return;
    }
    (void)ka_fs_proc_create_data(name, SVM_PROC_FS_MODE, svm_top_entry, ops, data);
}

int svm_proc_fs_init(void)
{
    svm_top_entry = ka_fs_proc_mkdir(SVM_PROC_TOP_NAME, NULL);
    return 0;
}

void svm_proc_fs_uninit(void)
{
    if (svm_top_entry != NULL) {
        ka_fs_remove_proc_subtree(SVM_PROC_TOP_NAME, NULL);
        svm_top_entry = NULL;
    }
}
