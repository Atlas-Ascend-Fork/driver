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

#ifndef ASDRV_UB_PROC_FS_H
#define ASDRV_UB_PROC_FS_H

#include "asdrv_ub_jetty.h"
#include "asdrv_ub_common_msg.h"
#include "asdrv_ub_main_adapt.h"

#define UBDRV_PROCFS_VALID 1
#define UBDRV_PROCFS_WRITE_LENGTH 5
#define UBDRV_START_HALF_PROBE_NEW 2

#define UBDRV_PCIVNIC "   vnic"
#define UBDRV_PASID "  pasid"
#define UBDRV_SMMU "   smmu"
#define UBDRV_DEVMM "  devmm"
#define UBDRV_VMNG "   vmng"
#define UBDRV_PROFILE "profile"
#define UBDRV_DEVMAN " devman"
#define UBDRV_TSDRV "  tsdrv"
#define UBDRV_HDC "    hdc"
#define UBDRV_SYSFS "  sysfs"
#define UBDRV_ESCHED " esched"
#define UBDRV_DP_PROC "dp_proc"
#define UBDRV_TEST "   test"
#define UBDRV_UDIS "   udis"
#define UBDRV_COMMON " common"
#define UBDRV_QUEUE "  queue"
#define UBDRV_S2S "    s2s"
#define UBDRV_TABLE "  table"
#define UBDRV_BBOX "   bbox"
#define UBDRV_BBOXDDR "bboxddr"
#define UBDRV_BBOXKLOG "   klog"
#define UBDRV_BBOXSRAM "   sram"
#define UBDRV_BBOXRUN "bboxrun"
#define UBDRV_BBOXSEC "bboxsec"
#define UBDRV_BBOXDEV "bboxdev"

// clang-format off
#define STATIC_PROCFS_FILE_FUNC_OPS(ops, open_func, write_func)     \
    static const ka_procfs_ops_t ops = {                            \
        ka_fs_init_pf_owner(KA_THIS_MODULE)                         \
        ka_fs_init_pf_open(open_func)                               \
        ka_fs_init_pf_read(ka_fs_seq_read)                          \
        ka_fs_init_pf_lseek(ka_fs_seq_lseek)                        \
        ka_fs_init_pf_release(ka_fs_single_release)                 \
        ka_fs_init_pf_write(write_func)                             \
    }
// clang-format on

struct ubdrv_procfs_entry {
    ka_proc_dir_entry_t *dev_id;
    ka_proc_dir_entry_t *pair_info;
    ka_proc_dir_entry_t *admin_stat;
    ka_proc_dir_entry_t *nontrans_stat;
    ka_proc_dir_entry_t *common_stat;
    ka_proc_dir_entry_t *rao_stat;
    ka_proc_dir_entry_t *mem_decoder;
    ka_proc_dir_entry_t *host_mem_cfg;
    ka_proc_dir_entry_t *prof;
    ka_proc_dir_entry_t *link_stat;
};

struct ubdrv_user_input {
    u32 dev_id;
};

int ubdrv_procfs_device_dfx_info(struct ascend_ub_msg_dev *msg_dev, void *data);
int ubdrv_proc_create_data_proc(ka_proc_dir_entry_t *top_entry, struct ubdrv_procfs_entry *procfs_entry);
struct ubdrv_user_input *get_global_user_data(void);
int ubdrv_procfs_send_admin_msg(struct ubdrv_chan_dfx_cmd *cmd, u32 dev_id, u32 sub_cmd, u32 chan_id);
void ubdrv_procfs_show_common_chan_dfx_head(ka_seq_file_t *seq);
void ubdrv_procfs_show_common_chan_dfx(ka_seq_file_t *seq, void *offset, struct ubdrv_common_msg_stat *stat, char *str);
void ubdrv_procfs_show_chan_dfx(ka_seq_file_t *seq, void *offset, struct ubdrv_msg_chan_stat *stat, u32 chan_id,
                                char *name);
void ubdrv_procfs_show_chan_dfx_head(ka_seq_file_t *seq);
void ubdrv_procfs_set_prof_state(bool state);
bool ubdrv_procfs_get_prof_state(void);
#endif