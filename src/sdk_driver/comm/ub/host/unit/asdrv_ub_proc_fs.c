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
#include "ka_compiler_pub.h"
#include "ka_kernel_def_pub.h"
#include "asdrv_ub_mem_decoder.h"
#include "asdrv_ub_admin_msg.h"
#include "asdrv_ub_proc_fs.h"
#include "asdrv_ub_main.h"

char *g_common_str[UBDRV_COMMON_CLIENT_CNT] = {UBDRV_PCIVNIC, UBDRV_SMMU,  UBDRV_DEVMM, UBDRV_VMNG,  UBDRV_PROFILE,
                                               UBDRV_DEVMAN,  UBDRV_TSDRV, UBDRV_HDC,   UBDRV_SYSFS, UBDRV_ESCHED,
                                               UBDRV_DP_PROC, UBDRV_TEST,  UBDRV_UDIS,  UBDRV_BBOX};
char *g_nontrans_str[UBDRV_NONTRANS_TYPE_CNT] = {UBDRV_PCIVNIC, UBDRV_PASID, UBDRV_DEVMM, UBDRV_COMMON,
                                                 UBDRV_DEVMAN,  UBDRV_TSDRV, UBDRV_HDC,   UBDRV_QUEUE,
                                                 UBDRV_S2S,     UBDRV_TABLE, UBDRV_ESCHED};
char *g_rao_str[DEVDRV_RAO_CLIENT_MAX] = {UBDRV_DEVMAN,  UBDRV_BBOXDDR, UBDRV_BBOXSRAM, UBDRV_BBOXKLOG,
                                          UBDRV_BBOXRUN, UBDRV_BBOXSEC, UBDRV_BBOXDEV,  UBDRV_TEST};

STATIC int ubdrv_procfs_mem_host_cfg_dfx(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_single_open(file, ubdrv_procfs_mem_host_cfg_dfx_show, inode->i_private);
}
STATIC_PROCFS_FILE_FUNC_OPS(mem_host_cfg, ubdrv_procfs_mem_host_cfg_dfx, NULL);

STATIC bool ubdrv_proc_check_data_valid(struct ubdrv_procfs_entry *entry)
{
    if (entry->dev_id == NULL) {
        ubdrv_err("Procfs entry dev_id is NULL.\n");
        return false;
    }
    if (entry->admin_stat == NULL) {
        ubdrv_err("Procfs entry admin_stat is NULL.\n");
        return false;
    }
    if (entry->nontrans_stat == NULL) {
        ubdrv_err("Procfs entry nontrans_stat is NULL.\n");
        return false;
    }
    if (entry->common_stat == NULL) {
        ubdrv_err("Procfs entry common_stat is NULL.\n");
        return false;
    }
    if (entry->rao_stat == NULL) {
        ubdrv_err("Procfs entry rao_stat is NULL.\n");
        return false;
    }
    if (entry->host_mem_cfg == NULL) {
        ubdrv_err("Procfs entry host_mem_cfg is NULL.\n");
        return false;
    }
    if (entry->prof == NULL) {
        ubdrv_err("Procfs entry prof is NULL.\n");
        return false;
    }
    return true;
}

STATIC int ubdrv_procfs_devid_show(ka_seq_file_t *seq, void *offset)
{
    struct ubdrv_user_input *user_input = NULL;

    user_input = get_global_user_data();
    ka_fs_seq_printf(seq, "[dev_id=%u]\n", user_input->dev_id);
    return 0;
}

STATIC int ubdrv_procfs_devid(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_single_open(file, ubdrv_procfs_devid_show, inode->i_private);
}

STATIC u32 ubdrv_procfs_parse_input(ka_file_t *filp, const char *ubuf, size_t count, loff_t *ppos, u32 *val)
{
    char ch[UBDRV_PROCFS_WRITE_LENGTH] = {0};

    if ((ppos == NULL) || (*ppos != 0) || (count == 0) || (count > sizeof(ch)) || (ubuf == NULL)) {
        ubdrv_err("Invalid dev_id param.\n");
        return -EINVAL;
    }

    if (ka_base_copy_from_user(ch, ubuf, count) != 0) {
        ubdrv_err("Copy from user error.\n");
        return -ENOMEM;
    }

    ch[count - 1] = '\0';
    if (ka_base_kstrtou32(ch, UBDRV_DECIMAL, val) != 0) {
        return -EFAULT;
    }
    return 0;
}

STATIC ssize_t ubdrv_procfs_devid_write(ka_file_t *filp, const char *ubuf, size_t count, loff_t *ppos)
{
    struct ubdrv_user_input *user_input = NULL;
    u32 temp_val;
    int ret;

    ret = ubdrv_procfs_parse_input(filp, ubuf, count, ppos, &temp_val);
    if (ret != 0) {
        return ret;
    }
    if (temp_val >= ASCEND_UB_DEV_MAX_NUM) {
        return -ERANGE;
    } else {
        user_input = get_global_user_data();
        KA_WRITE_ONCE(user_input->dev_id, temp_val);
    }

    return count;
}

STATIC int ubdrv_procfs_prof_show(ka_seq_file_t *seq, void *offset)
{
    if (ubdrv_procfs_get_prof_state() == true) {
        ka_fs_seq_printf(seq, "enable\n");
    } else {
        ka_fs_seq_printf(seq, "disable\n");
    }
    return 0;
}

STATIC int ubdrv_procfs_prof(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_single_open(file, ubdrv_procfs_prof_show, inode->i_private);
}

STATIC ssize_t ubdrv_procfs_prof_write(ka_file_t *filp, const char *ubuf, size_t count, loff_t *ppos)
{
    struct ubdrv_user_input *user_input = NULL;
    struct ubdrv_chan_dfx_cmd *cmd;
    u32 temp_val;
    u32 dev_id;
    int ret;

    ret = ubdrv_procfs_parse_input(filp, ubuf, count, ppos, &temp_val);
    if (ret != 0) {
        return ret;
    }
    user_input = get_global_user_data();
    dev_id = user_input->dev_id;
    cmd = ubdrv_kzalloc(sizeof(struct ubdrv_chan_dfx_cmd), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (cmd == NULL) {
        ubdrv_err("Alloc data buf fail. (dev_id=%u)\n", dev_id);
        return -ENOMEM;
    }
    if (temp_val != 0) {
        ubdrv_procfs_set_prof_state(true);
        (void)ubdrv_procfs_send_admin_msg(cmd, dev_id, UBDRV_PROCFS_ENABLE_PROF_CMD, 0);
    } else {
        ubdrv_procfs_set_prof_state(false);
        (void)ubdrv_procfs_send_admin_msg(cmd, dev_id, UBDRV_PROCFS_DISABLE_PROF_CMD, 0);
    }
    ubdrv_kfree(cmd);
    ubdrv_info("Set prof success. (dev_id=%u;val=%u;uid=%u)\n", dev_id, temp_val, ka_task_get_current_cred_uid());
    return count;
}

STATIC int ubdrv_procfs_admin_dfx_show(ka_seq_file_t *seq, void *offset)
{
    struct ubdrv_user_input *user_input = NULL;
    struct ascend_ub_admin_chan *msg_chan;
    struct ubdrv_chan_dfx_cmd *cmd;
    struct ascend_ub_msg_dev *msg_dev;
    u32 dev_id;
    int ret;

    user_input = get_global_user_data();
    dev_id = user_input->dev_id;
    ret = ubdrv_add_device_status_ref(dev_id);
    if (ret != 0) {
        return ret;
    }

    msg_dev = ubdrv_get_msg_dev_by_devid(dev_id);
    if (msg_dev == NULL) {
        ka_fs_seq_printf(seq, "Get msg_dev failed.(dev_id=%u)\n", dev_id);
        goto admin_sub_ref;
    }

    msg_chan = &msg_dev->admin_msg_chan;
    ka_fs_seq_printf(seq, "-------------------host info-------------------\n");
    ubdrv_procfs_show_chan_dfx_head(seq);
    // admin chan only have one id is zero
    ubdrv_procfs_show_chan_dfx(seq, offset, &msg_chan->chan_stat, 0, "admin");

    cmd = ubdrv_kzalloc(sizeof(struct ubdrv_chan_dfx_cmd), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (cmd == NULL) {
        ka_fs_seq_printf(seq, "Alloc data buf fail. (dev_id=%u)\n", dev_id);
        goto admin_sub_ref;
    }
    ret = ubdrv_procfs_send_admin_msg(cmd, dev_id, UBDRV_PROCFS_ADMIN_CHAN_CMD, 0);
    if (ret != 0) {
        ka_fs_seq_printf(seq, "Send admin msg fail. (ret=%d)\n", ret);
        goto free_admin_cmd;
    }

    ka_fs_seq_printf(seq, "-------------------dev  info-------------------\n");
    ubdrv_procfs_show_chan_dfx(seq, offset, &cmd->stat.chan, 0, "admin");

free_admin_cmd:
    ubdrv_kfree(cmd);
admin_sub_ref:
    ubdrv_sub_device_status_ref(dev_id);
    return 0;
}

STATIC int ubdrv_procfs_admin_dfx(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_single_open(file, ubdrv_procfs_admin_dfx_show, inode->i_private);
}

STATIC int ubdrv_procfs_non_trans_dfx_show(ka_seq_file_t *seq, void *offset)
{
    struct ubdrv_user_input *user_input = NULL;
    struct ubdrv_non_trans_chan *msg_chan;
    struct ubdrv_chan_dfx_cmd *cmd;
    struct ascend_ub_msg_dev *msg_dev;
    u32 dev_id;
    u32 i = 0;
    int ret;

    user_input = get_global_user_data();
    dev_id = user_input->dev_id;
    ret = ubdrv_add_device_status_ref(dev_id);
    if (ret != 0) {
        return ret;
    }
    msg_dev = ubdrv_get_msg_dev_by_devid(dev_id);
    if (msg_dev == NULL) {
        ka_fs_seq_printf(seq, "Get msg_dev failed.(dev_id = %d)\n", dev_id);
        goto non_trans_sub_ref;
    }

    ka_fs_seq_printf(seq, "-------------------------host info-------------------------\n");
    ubdrv_procfs_show_chan_dfx_head(seq);
    for (i = 0; i < msg_dev->chan_cnt; i++) {
        msg_chan = &msg_dev->non_trans_chan[i];
        if (msg_chan->status != UBDRV_CHAN_ENABLE) {
            continue;
        }
        ubdrv_procfs_show_chan_dfx(seq, offset, &msg_chan->chan_stat, i, g_nontrans_str[msg_chan->msg_type]);
    }

    cmd = ubdrv_kzalloc(sizeof(struct ubdrv_chan_dfx_cmd), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (cmd == NULL) {
        ka_fs_seq_printf(seq, "Alloc data buf fail. (dev_id=%u)\n", dev_id);
        goto non_trans_sub_ref;
    }

    ka_fs_seq_printf(seq, "-------------------------dev  info-------------------------\n");
    for (i = 0; i < msg_dev->chan_cnt; i++) {
        msg_chan = &msg_dev->non_trans_chan[i];
        if (msg_chan->status != UBDRV_CHAN_ENABLE) {
            continue;
        }
        ret = ubdrv_procfs_send_admin_msg(cmd, dev_id, UBDRV_PROCFS_NONTRANS_CHAN_CMD, i);
        if (ret != 0) {
            ka_fs_seq_printf(seq, "Send admin msg fail. (ret=%d;chan_id=%u)\n", ret, i);
            goto free_chan_cmd;
        }
        ubdrv_procfs_show_chan_dfx(seq, offset, &cmd->stat.chan, i, g_nontrans_str[msg_chan->msg_type]);
    }

free_chan_cmd:
    ubdrv_kfree(cmd);
non_trans_sub_ref:
    ubdrv_sub_device_status_ref(dev_id);
    return 0;
}

STATIC int ubdrv_procfs_non_trans_dfx(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_single_open(file, ubdrv_procfs_non_trans_dfx_show, inode->i_private);
}

STATIC int ubdrv_procfs_rao_chan_dfx_show(ka_seq_file_t *seq, void *offset)
{
    struct ubdrv_user_input *user_input = NULL;
    struct ubdrv_non_trans_chan *msg_chan;
    struct ascend_ub_msg_dev *msg_dev;
    u32 dev_id;
    int ret;
    u32 i;

    user_input = get_global_user_data();
    dev_id = user_input->dev_id;
    ret = ubdrv_add_device_status_ref(dev_id);
    if (ret != 0) {
        return ret;
    }
    msg_dev = ubdrv_get_msg_dev_by_devid(dev_id);
    if (msg_dev == NULL) {
        ka_fs_seq_printf(seq, "Get msg_dev failed.(dev_id = %d)\n", dev_id);
        goto rao_chan_sub_ref;
    }

    ka_fs_seq_printf(seq, "-------------------------host info-------------------------\n");
    ubdrv_procfs_show_chan_dfx_head(seq);
    for (i = 0; i < DEVDRV_RAO_CLIENT_MAX; i++) {
        msg_chan = &msg_dev->rao.rao_msg_chan[i];
        ubdrv_procfs_show_chan_dfx(seq, offset, &msg_chan->chan_stat, i, g_rao_str[i]);
    }

rao_chan_sub_ref:
    ubdrv_sub_device_status_ref(dev_id);
    return 0;
}

STATIC int ubdrv_procfs_rao_chan_dfx(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_single_open(file, ubdrv_procfs_rao_chan_dfx_show, inode->i_private);
}

STATIC int ubdrv_procfs_common_dfx_show(ka_seq_file_t *seq, void *offset)
{
    struct ubdrv_user_input *user_input = NULL;

    struct ubdrv_chan_dfx_cmd *cmd;
    struct ubdrv_common_msg_stat *stat;
    u32 dev_id;
    int ret;
    u32 i;

    user_input = get_global_user_data();
    dev_id = user_input->dev_id;
    ret = ubdrv_add_device_status_ref(dev_id);
    if (ret != 0) {
        return ret;
    }
    ka_fs_seq_printf(seq, "-------------------------host info-------------------------\n");
    ubdrv_procfs_show_common_chan_dfx_head(seq);
    for (i = 0; i < UBDRV_COMMON_CLIENT_CNT; i++) {
        stat = ubdrv_get_common_stat_dfx(dev_id, (u32)i);
        ubdrv_procfs_show_common_chan_dfx(seq, offset, stat, g_common_str[i]);
    }

    cmd = ubdrv_kzalloc(sizeof(struct ubdrv_chan_dfx_cmd), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (cmd == NULL) {
        ka_fs_seq_printf(seq, "Alloc data buf fail. (dev_id=%u)\n", dev_id);
        goto common_chan_sub_ref;
    }
    ret = ubdrv_procfs_send_admin_msg(cmd, dev_id, UBDRV_PROCFS_COMMON_CHAN_CMD, 0);
    if (ret != 0) {
        ka_fs_seq_printf(seq, "Send admin msg fail. (ret=%d)\n", ret);
        goto free_client_cmd;
    }

    ka_fs_seq_printf(seq, "-------------------------dev  info-------------------------\n");
    for (i = 0; i < UBDRV_COMMON_CLIENT_CNT; i++) {
        ubdrv_procfs_show_common_chan_dfx(seq, offset, &cmd->stat.client[i], g_common_str[i]);
    }

free_client_cmd:
    ubdrv_kfree(cmd);
common_chan_sub_ref:
    ubdrv_sub_device_status_ref(dev_id);
    return 0;
}

STATIC int ubdrv_procfs_common_dfx(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_single_open(file, ubdrv_procfs_common_dfx_show, inode->i_private);
}

STATIC_PROCFS_FILE_FUNC_OPS(dev_id, ubdrv_procfs_devid, ubdrv_procfs_devid_write);
STATIC_PROCFS_FILE_FUNC_OPS(admin_stat, ubdrv_procfs_admin_dfx, NULL);
STATIC_PROCFS_FILE_FUNC_OPS(nontrans_stat, ubdrv_procfs_non_trans_dfx, NULL);
STATIC_PROCFS_FILE_FUNC_OPS(rao_stat, ubdrv_procfs_rao_chan_dfx, NULL);
STATIC_PROCFS_FILE_FUNC_OPS(common_stat, ubdrv_procfs_common_dfx, NULL);
STATIC_PROCFS_FILE_FUNC_OPS(prof, ubdrv_procfs_prof, ubdrv_procfs_prof_write);

int ubdrv_proc_create_data_proc(ka_proc_dir_entry_t *top_entry, struct ubdrv_procfs_entry *procfs_entry)
{
    procfs_entry->dev_id = ka_fs_proc_create_data("dev_id", KA_S_IRUSR | KA_S_IWUSR, top_entry, &dev_id, NULL);
    procfs_entry->admin_stat = ka_fs_proc_create_data("admin", KA_S_IRUSR, top_entry, &admin_stat, NULL);
    procfs_entry->nontrans_stat = ka_fs_proc_create_data("nontrans", KA_S_IRUSR, top_entry, &nontrans_stat, NULL);
    procfs_entry->common_stat = ka_fs_proc_create_data("common", KA_S_IRUSR, top_entry, &common_stat, NULL);
    procfs_entry->rao_stat = ka_fs_proc_create_data("rao", KA_S_IRUSR, top_entry, &rao_stat, NULL);
    procfs_entry->host_mem_cfg = ka_fs_proc_create_data("host_decoder", KA_S_IRUSR, top_entry, &mem_host_cfg, NULL);
    procfs_entry->prof = ka_fs_proc_create_data("prof", KA_S_IRUSR | KA_S_IWUSR, top_entry, &prof, NULL);
    if (ubdrv_proc_check_data_valid(procfs_entry) == false) {
        ubdrv_err("Create chan dfx proc data error.\n");
        return -EBADR;
    }

    return 0;
}