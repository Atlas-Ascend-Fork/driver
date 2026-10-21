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
#ifndef SVM_PROC_FS_H
#define SVM_PROC_FS_H

#include "ka_base_pub.h"
#include "ka_fs_pub.h"

int svm_proc_fs_init(void);
void svm_proc_fs_uninit(void);

ka_proc_dir_entry_t *svm_proc_fs_add_dev(u32 udevid, u32 inst);
void svm_proc_fs_del_dev(ka_proc_dir_entry_t *dev_entry);
void svm_proc_dev_add_feature(u32 udevid, ka_proc_dir_entry_t *dev_entry, u32 feature_id, const char *feature_name);

ka_proc_dir_entry_t *svm_proc_fs_add_task(u32 udevid, ka_proc_dir_entry_t *dev_entry, int tgid, u32 task_id);
void svm_proc_fs_del_task(ka_proc_dir_entry_t *task_entry);
void svm_proc_task_add_feature(u32 udevid, int tgid, ka_proc_dir_entry_t *task_entry, u32 feature_id,
                               const char *feature_name);

int svm_task_feature_open(ka_inode_t *inode, ka_file_t *file);
int svm_dev_feature_open(ka_inode_t *inode, ka_file_t *file);

void svm_proc_fs_add_feature(const char *name, const ka_procfs_ops_t *ops, void *data);

#endif /* SVM_PROC_FS_H__ */