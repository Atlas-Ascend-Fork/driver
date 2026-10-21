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
#include "ka_common_pub.h"
#include "ka_fs_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_compiler_pub.h"

#include "pbl_uda.h"

#include "securec.h"
#include "chan_rxtx.h"

#define PROC_FS_NAME_LEN 32
#define PROC_FS_MODE 0444
#define BUFF_LEN 1000

static ka_proc_dir_entry_t *chan_top_entry = NULL;

ssize_t chan_trace_ops_write(ka_file_t *filp, const char __ka_user *ubuf, size_t count, loff_t *ppos);
int chan_trace_ops_open(ka_inode_t *inode, ka_file_t *file);
#ifdef CFG_BUILD_DEBUG
ssize_t chan_vaddr_ops_write(ka_file_t *filp, const char __ka_user *ubuf, size_t count, loff_t *ppos);
int chan_vaddr_ops_open(ka_inode_t *inode, ka_file_t *file);
#endif
int chan_sum_ops_open(ka_inode_t *inode, ka_file_t *file);
void chan_proc_fs_add_ts_inst(struct trs_chan_ts_inst *ts_inst);
void chan_proc_fs_del_ts_inst(struct trs_chan_ts_inst *ts_inst);
void chan_proc_fs_add_chan(struct trs_chan *chan);
void chan_proc_fs_del_chan(struct trs_chan *chan);
void chan_proc_fs_init(void);
void chan_proc_fs_uninit(void);

static ssize_t _chan_parse_bool_from_user(struct trs_chan_ts_inst *ts_inst, const char __ka_user *ubuf, size_t count,
                                          loff_t *ppos, long *val)
{
    char ch[2] = {0}; /* 2 bytes long */

    if ((ppos == NULL) || (*ppos != 0) || (count != sizeof(ch)) || (ubuf == NULL)) {
        return -EINVAL;
    }

    if (!uda_can_access_udevid(ts_inst->inst.devid)) {
        return -EACCES;
    }

    if (ka_base_copy_from_user(ch, ubuf, count)) {
        return -ENOMEM;
    }

    ch[count - 1] = '\0';
    if (ka_base_kstrtol(ch, 10, val)) {
        return -EFAULT;
    }

    return (ssize_t)count;
}

static int chan_trace_show(ka_seq_file_t *seq, void *offset)
{
    struct trs_chan_ts_inst *ts_inst = (struct trs_chan_ts_inst *)(uintptr_t)ka_fs_get_seq_file_private(seq);

    ka_fs_seq_printf(seq, "%d\n", ts_inst->trace_enable);
    return 0;
}

ssize_t chan_trace_ops_write(ka_file_t *filp, const char __ka_user *ubuf, size_t count, loff_t *ppos)
{
    struct trs_chan_ts_inst *ts_inst = (struct trs_chan_ts_inst *)(uintptr_t)ka_base_pde_data(ka_fs_file_inode(filp));
    long val;
    ssize_t ret = _chan_parse_bool_from_user(ts_inst, ubuf, count, ppos, &val);

    if (ret < 0) {
        return ret;
    }
    ts_inst->trace_enable = (val == 0) ? false : true;
    return ret;
}

int chan_trace_ops_open(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_single_open(file, chan_trace_show, ka_base_pde_data(inode));
}

// clang-format off
static const ka_procfs_ops_t chan_trace_ops = {
    ka_fs_init_pf_owner(KA_THIS_MODULE) \
    ka_fs_init_pf_open(chan_trace_ops_open) \
    ka_fs_init_pf_read(ka_fs_seq_read) \
    ka_fs_init_pf_write(chan_trace_ops_write) \
    ka_fs_init_pf_lseek(ka_fs_seq_lseek) \
    ka_fs_init_pf_release(ka_fs_single_release) \
};
// clang-format on

#ifdef CFG_BUILD_DEBUG
static int chan_vaddr_show(ka_seq_file_t *seq, void *offset)
{
    struct trs_chan_ts_inst *ts_inst = (struct trs_chan_ts_inst *)(uintptr_t)ka_fs_get_seq_file_private(seq);

    ka_fs_seq_printf(seq, "%d\n", ts_inst->vaddr_enable);
    return 0;
}

ssize_t chan_vaddr_ops_write(ka_file_t *filp, const char __ka_user *ubuf, size_t count, loff_t *ppos)
{
    struct trs_chan_ts_inst *ts_inst = (struct trs_chan_ts_inst *)(uintptr_t)ka_base_pde_data(ka_fs_file_inode(filp));
    long val;
    ssize_t ret = _chan_parse_bool_from_user(ts_inst, ubuf, count, ppos, &val);

    if (ret < 0) {
        return ret;
    }
    ts_inst->vaddr_enable = (val == 0) ? false : true;
    return ret;
}

int chan_vaddr_ops_open(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_single_open(file, chan_vaddr_show, ka_base_pde_data(inode));
}

// clang-format off
static const ka_procfs_ops_t chan_vaddr_ops = {
    ka_fs_init_pf_owner(KA_THIS_MODULE) \
    ka_fs_init_pf_open(chan_vaddr_ops_open) \
    ka_fs_init_pf_read(ka_fs_seq_read) \
    ka_fs_init_pf_write(chan_vaddr_ops_write) \
    ka_fs_init_pf_lseek(ka_fs_seq_lseek) \
    ka_fs_init_pf_release(ka_fs_single_release) \
};
// clang-format on
#endif

static int chan_sum_show(ka_seq_file_t *seq, void *offset)
{
    struct trs_chan *chan = (struct trs_chan *)ka_fs_get_seq_file_private(seq);
    char buff[BUFF_LEN];

    ka_fs_seq_printf(seq, "id %d ref %u flag 0x%x irq %u type %u subtype %u\n", chan->id, kref_safe_read(&chan->ref),
                     chan->flag, chan->irq, chan->types.type, chan->types.sub_type);

    if (_trs_chan_to_string(chan, buff, BUFF_LEN) > 0) {
        ka_fs_seq_printf(seq, "%s\n", buff);
    }
    return 0;
}

int chan_sum_ops_open(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_single_open(file, chan_sum_show, ka_base_pde_data(inode));
}

// clang-format off
static const ka_procfs_ops_t chan_sum_ops = {
    ka_fs_init_pf_owner(KA_THIS_MODULE) \
    ka_fs_init_pf_open(chan_sum_ops_open) \
    ka_fs_init_pf_read(ka_fs_seq_read) \
    ka_fs_init_pf_lseek(ka_fs_seq_lseek) \
    ka_fs_init_pf_release(ka_fs_single_release) \
};
// clang-format on

static void proc_fs_format_ts_inst_dir_name(struct trs_chan_ts_inst *ts_inst, char *name, int len)
{
    if (sprintf_s(name, len, "dev%u-ts%u", ts_inst->inst.devid, ts_inst->inst.tsid) <= 0) {
#ifndef EMU_ST
        trs_warn("Sprintf_s warn. (devid=%u; tsid=%u)\n", ts_inst->inst.devid, ts_inst->inst.tsid);
#endif
    }
}

static ka_proc_dir_entry_t *proc_fs_mk_ts_inst_dir(struct trs_chan_ts_inst *ts_inst, ka_proc_dir_entry_t *parent)
{
    char name[PROC_FS_NAME_LEN];

    proc_fs_format_ts_inst_dir_name(ts_inst, name, PROC_FS_NAME_LEN);
    return ka_fs_proc_mkdir((const char *)name, parent);
}

static void proc_fs_rm_ts_inst_dir(struct trs_chan_ts_inst *ts_inst, ka_proc_dir_entry_t *parent)
{
    char name[PROC_FS_NAME_LEN];

    proc_fs_format_ts_inst_dir_name(ts_inst, name, PROC_FS_NAME_LEN);
    (void)ka_fs_remove_proc_subtree((const char *)name, parent);
}

void chan_proc_fs_add_ts_inst(struct trs_chan_ts_inst *ts_inst)
{
    ts_inst->entry = proc_fs_mk_ts_inst_dir(ts_inst, chan_top_entry);
    if (ts_inst->entry == NULL) {
#ifndef EMU_ST
        trs_warn("Create entry warn. (devid=%u; tsid=%u)\n", ts_inst->inst.devid, ts_inst->inst.tsid);
#endif
        return;
    }

    (void)ka_fs_proc_create_data("trace_enable", PROC_FS_MODE, ts_inst->entry, &chan_trace_ops, ts_inst);
#ifdef CFG_BUILD_DEBUG
    (void)ka_fs_proc_create_data("vaddr_enable", PROC_FS_MODE, ts_inst->entry, &chan_vaddr_ops, ts_inst);
#endif
}

void chan_proc_fs_del_ts_inst(struct trs_chan_ts_inst *ts_inst)
{
    proc_fs_rm_ts_inst_dir(ts_inst, chan_top_entry);
}

static void proc_fs_format_chan_file_name(struct trs_chan *chan, char *name, int len)
{
    if (sprintf_s(name, len, "chan%d-%s", chan->id, trs_chan_type_to_name(&chan->types)) <= 0) {
#ifndef EMU_ST
        trs_warn("Sprintf_s warn. (id=%d)\n", chan->id);
#endif
    }
}

void chan_proc_fs_add_chan(struct trs_chan *chan)
{
    char name[PROC_FS_NAME_LEN];

    proc_fs_format_chan_file_name(chan, name, PROC_FS_NAME_LEN);
    chan->entry = ka_fs_proc_create_data(name, PROC_FS_MODE, chan->ts_inst->entry, &chan_sum_ops, chan);
}

void chan_proc_fs_del_chan(struct trs_chan *chan)
{
    ka_fs_proc_remove(chan->entry);
}

void chan_proc_fs_init(void)
{
    chan_top_entry = ka_fs_proc_mkdir("trs_chan", NULL);
    if (chan_top_entry == NULL) {
        trs_err("create top entry dir failed\n");
    }
}

void chan_proc_fs_uninit(void)
{
    (void)ka_fs_remove_proc_subtree("trs_chan", NULL);
}
