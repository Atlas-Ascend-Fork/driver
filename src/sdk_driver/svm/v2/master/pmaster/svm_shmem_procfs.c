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
#ifdef CONFIG_PROC_FS

#include "securec.h"
#include "svm_shmem_node.h"
#include "svm_shmem_procfs.h"
#include "ka_fs_pub.h"
#include "ka_task_pub.h"

#define PROC_FS_TOP_NAME "svm_shmem"
#define PROC_FS_MODE 0400

static ka_proc_dir_entry_t *g_svm_shmem_top_entry = NULL;

static int devmm_ipc_procfs_sum_show(ka_seq_file_t *seq, void *offset)
{
#ifndef EMU_ST
    struct devmm_ipc_node *node = (struct devmm_ipc_node *)ka_fs_get_seq_file_private(seq);
    struct ipc_node_wlist *wlist = NULL;
    u32 stamp = (u32)ka_jiffies;
    int i = 0;

    ka_fs_seq_printf(seq, "Ipc node detail:\n");
    ka_fs_seq_printf(seq,
                     "name=%s;sdid=%u;devid=%u;vfid=%u;pid=%d;vptr=0x%llx;len=%ld;page_size=%ld;is_huge=%d;"
                     "is_reserve=%d;need_set_wlist=%d;valid=%d;key=%u;ref=%d;mem_repair=%d\n",
                     node->attr.name, node->attr.sdid, node->attr.inst.devid, node->attr.inst.vfid, node->attr.pid,
                     node->attr.vptr, node->attr.len, node->attr.page_size, node->attr.is_huge,
                     node->attr.is_reserve_addr, node->attr.need_set_wlist, node->valid, node->key,
                     ka_base_kref_read(&node->ref), node->mem_repair_record);
    ka_fs_seq_printf(seq, "White List:\n");
    ka_task_mutex_lock(&node->mutex);
    ka_list_for_each_entry(wlist, &node->wlist_head, list)
    {
        ka_fs_seq_printf(seq, "   [%d]:sdid=%u;pid=%d;set_time=%lu;vptr=0x%llx\n", i++, wlist->sdid, wlist->pid,
                         wlist->set_time, wlist->vptr);
        devmm_try_cond_resched(&stamp);
    }
    ka_task_mutex_unlock(&node->mutex);
#endif
    return 0;
}

static int devmm_ipc_procfs_ops_open(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_single_open(file, devmm_ipc_procfs_sum_show, ka_base_pde_data(inode));
}

static const ka_procfs_ops_t devmm_ipc_procfs_ops = {
    ka_fs_init_pf_owner(KA_THIS_MODULE) ka_fs_init_pf_open(devmm_ipc_procfs_ops_open) ka_fs_init_pf_read(ka_fs_seq_read)
        ka_fs_init_pf_lseek(ka_fs_seq_lseek) ka_fs_init_pf_release(ka_fs_single_release)};

void devmm_ipc_procfs_add_node(struct devmm_ipc_node *node)
{
    if (g_svm_shmem_top_entry != NULL) {
        node->entry = ka_fs_proc_create_data(node->attr.name, PROC_FS_MODE, g_svm_shmem_top_entry,
                                             &devmm_ipc_procfs_ops, node);
    }
}

void devmm_ipc_procfs_del_node(struct devmm_ipc_node *node)
{
    if (node->entry != NULL) {
        ka_fs_proc_remove(node->entry);
        node->entry = NULL;
    }
}

void devmm_ipc_profs_init(void)
{
    g_svm_shmem_top_entry = ka_fs_proc_mkdir(PROC_FS_TOP_NAME, NULL);
    if (g_svm_shmem_top_entry == NULL) {
        devmm_drv_warn("Create top entry dir fail\n");
    }
}

void devmm_ipc_profs_uninit(void)
{
    if (g_svm_shmem_top_entry != NULL) {
        (void)ka_fs_remove_proc_subtree(PROC_FS_TOP_NAME, NULL);
        g_svm_shmem_top_entry = NULL;
    }
}

#endif /* CONFIG_PROC_FS */
