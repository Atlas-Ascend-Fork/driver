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
#include "pbl/pbl_feature_loader.h"
#include "asdrv_ub_main.h"
#include "asdrv_ub_link_chan.h"
#include "asdrv_ub_admin_msg.h"
#include "asdrv_ub_mem_decoder.h"
#include "asdrv_ub_pair_dev_info.h"
#include "asdrv_ub_proc_fs.h"

ka_atomic_t fs_valid = KA_BASE_ATOMIC_INIT(0);
bool g_enable_prof = false;

STATIC struct ubdrv_procfs_entry g_procfs_entry = {
    .dev_id = NULL,
    .pair_info = NULL,
    .admin_stat = NULL,
    .nontrans_stat = NULL,
    .common_stat = NULL,
    .mem_decoder = NULL,
    .host_mem_cfg = NULL,
    .prof = NULL,
    .link_stat = NULL,
};

STATIC struct ubdrv_user_input g_user_data = {
    .dev_id = 0,
};

struct ubdrv_user_input *get_global_user_data(void)
{
    return &g_user_data;
}

STATIC int ubdrv_procfs_pair_info(ka_inode_t *inode, ka_file_t *file)
{
#ifdef CFG_FEATURE_SUPPORT_RUN_INSTALL
    return ka_fs_single_open(file, ubdrv_procfs_pair_info_show, pde_data(inode));
#else
    return -EOPNOTSUPP;
#endif
}

STATIC int ubdrv_procfs_get_device_admin_chan_dfx(struct ubdrv_msg_chan_stat *stat_buf, u32 chan_num,
                                                  struct ascend_ub_msg_dev *msg_dev, struct ascend_ub_msg_desc *desc)
{
    size_t len = sizeof(struct ubdrv_msg_chan_stat);
    (void)chan_num;
    int ret;

    ret = memcpy_s(stat_buf, len, &msg_dev->admin_msg_chan.chan_stat, len);
    if (ret != 0) {
        ubdrv_err("Memcpy admin stat dfx error. (dev_id=%u;ret=%d)\n", msg_dev->dev_id, ret);
        return ret;
    }

    return 0;
}

STATIC int ubdrv_procfs_get_device_nontrans_chan_dfx(struct ubdrv_msg_chan_stat *stat_buf, u32 chan_num, u32 chan_id,
                                                     struct ascend_ub_msg_dev *msg_dev, struct ascend_ub_msg_desc *desc)
{
    size_t len = sizeof(struct ubdrv_msg_chan_stat);
    int ret;

    if (chan_id >= chan_num) {
        ubdrv_err("Check chan_id invalid. (chan_id=%u;chan_num=%u)\n", chan_id, chan_num);
        return -EINVAL;
    }

    ret = memcpy_s(stat_buf, len, &msg_dev->non_trans_chan[chan_id].chan_stat, len);
    if (ret != 0) {
        ubdrv_err("Memcpy chan stat dfx error. (dev_id=%u;ret=%d)\n", msg_dev->dev_id, ret);
        return ret;
    }

    return 0;
}

STATIC int ubdrv_procfs_get_device_common_chan_dfx(struct ubdrv_common_msg_stat *stat_buf, u32 chan_num, u32 dev_id,
                                                   struct ascend_ub_msg_desc *desc)
{
    size_t len = sizeof(struct ubdrv_common_msg_stat);
    struct ubdrv_common_msg_stat *stat;
    int ret;
    u32 i;

    for (i = 0; i < chan_num; i++) {
        stat = ubdrv_get_common_stat_dfx(dev_id, i);
        ret = memcpy_s(stat_buf, len, stat, len);
        if (ret != 0) {
            ubdrv_err("Memcpy client stat dfx error. (dev_id=%d;ret=%u)\n", dev_id, ret);
            return ret;
        }
        stat_buf += 1;
    }

    return 0;
}

int ubdrv_procfs_device_dfx_info(struct ascend_ub_msg_dev *msg_dev, void *data)
{
    struct ascend_ub_msg_desc *desc = (struct ascend_ub_msg_desc *)data;
    struct ubdrv_chan_dfx_cmd *data_buf = (struct ubdrv_chan_dfx_cmd *)&desc->user_data;
    u32 len = sizeof(struct ubdrv_chan_dfx_cmd);
    u32 dev_id = msg_dev->dev_id;

    if (desc->in_data_len != len) {
        ubdrv_err("Recv data len invalid, when get dfx info. (dev_id=%u;len=%u;expect_len=%u)\n", dev_id,
                  desc->in_data_len, len);
        return -EINVAL;
    }

    desc->real_data_len = (u32)sizeof(struct ubdrv_chan_dfx_cmd);
    if (data_buf->opcode == UBDRV_PROCFS_ADMIN_CHAN_CMD) {
        // admin only have one chan
        return ubdrv_procfs_get_device_admin_chan_dfx(&data_buf->stat.chan, 1, msg_dev, desc);
    } else if (data_buf->opcode == UBDRV_PROCFS_NONTRANS_CHAN_CMD) {
        return ubdrv_procfs_get_device_nontrans_chan_dfx(&data_buf->stat.chan, msg_dev->chan_cnt, data_buf->chan_id,
                                                         msg_dev, desc);
    } else if (data_buf->opcode == UBDRV_PROCFS_COMMON_CHAN_CMD) {
        return ubdrv_procfs_get_device_common_chan_dfx(data_buf->stat.client, UBDRV_COMMON_CLIENT_CNT, dev_id, desc);
    } else if (data_buf->opcode == UBDRV_PROCFS_ENABLE_PROF_CMD) {
        ubdrv_procfs_set_prof_state(true);
    } else if (data_buf->opcode == UBDRV_PROCFS_DISABLE_PROF_CMD) {
        ubdrv_procfs_set_prof_state(false);
    } else {
        ubdrv_err("Recv data sub cmd invalid. (dev_id=%u;sub_cmd=%d)\n", dev_id, data_buf->opcode);
    }
    return 0;
}

void ubdrv_procfs_show_chan_dfx_head(ka_seq_file_t *seq)
{
    ka_fs_seq_printf(
        seq, "|chan_id |chan_type |tx_total |tx_err |tx_cqe |tx_cqe_timeout |tx_poll_cqe_err |"
             "tx_cqe_err |tx_rebuild |recv_cqe_timeout |recv_cqe |recv_poll_cqe_err |recv_cqe_err |recv_data_err |"
             "rx_total |rx_work |rx_poll_cqe_err |rx_cqe_err |rx_call_err |rx_null_call |rx_finish |rx_post_jfr_err |"
             "rx_tx_err |rx_tx_poll_cqe_err |rx_tx_cqe_timeout |rx_tx_cqe |rx_tx_cqe_err |rx_rebuild |"
             "rx_max_time |rx_work_max_time |"
             "jfae_err |tx_jfr |tx_jfr_jfc |tx_jfs |tx_jfs_jfc |rx_jfr |rx_jfr_jfc |rx_jfs |rx_jfs_jfc|\n");
    return;
}

void ubdrv_procfs_show_chan_dfx(ka_seq_file_t *seq, void *offset, struct ubdrv_msg_chan_stat *stat, u32 chan_id,
                                char *name)
{
    ka_fs_seq_printf(seq,
                     "|%-8u|%-10s|%-9llu|%-7llu|%-7llu|%-15llu|%-16llu"
                     "|%-11llu|%-11llu|%-17llu|%-9llu|%-18llu|%-13llu|%-14llu"
                     "|%-9llu|%-8llu|%-16llu|%-11llu|%-12llu|%-13llu|%-10llu|%-16llu"
                     "|%-10llu|%-19llu|%-18llu|%-10llu|%-14llu|%-11llu"
                     "|%-12u|%-17u"
                     "|%-9u|%-7u|%-11u|%-7u|%-11u|%-7u|%-11u|%-7u|%-10u|\n",
                     chan_id, name, stat->tx_total, stat->tx_post_send_err, stat->tx_cqe, stat->tx_cqe_timeout,
                     stat->tx_poll_cqe_err, stat->tx_cqe_status_err, stat->tx_rebuild, stat->tx_recv_cqe_timeout,
                     stat->tx_recv_cqe, stat->tx_recv_poll_cqe_err, stat->tx_recv_cqe_status_err,
                     stat->tx_recv_data_err, stat->rx_total, stat->rx_work, stat->rx_poll_cqe_err,
                     stat->rx_cqe_status_err, stat->rx_process_err, stat->rx_null_process, stat->rx_work_finish,
                     stat->rx_post_jfr_err, stat->rx_tx_post_err, stat->rx_tx_poll_cqe_err, stat->rx_tx_cqe_timeout,
                     stat->rx_tx_cqe, stat->rx_tx_cqe_status_err, stat->rx_rebuild, stat->rx_max_time,
                     stat->rx_work_max_time, stat->jfae_err, stat->tx_jfr_id, stat->tx_jfr_jfc_id, stat->tx_jfs_id,
                     stat->tx_jfs_jfc_id, stat->rx_jfr_id, stat->rx_jfr_jfc_id, stat->rx_jfs_id, stat->rx_jfs_jfc_id);
    return;
}

void ubdrv_procfs_show_common_chan_dfx_head(ka_seq_file_t *seq)
{
    ka_fs_seq_printf(seq, "| client_type | tx_total | tx_check_data_err | tx_chan_null_err | tx_enodev_err "
                          "| tx_chan_err | rx_total | rx_cb_process_err | rx_null_cb_err | rx_finish |\n");
    return;
}

void ubdrv_procfs_show_common_chan_dfx(ka_seq_file_t *seq, void *offset, struct ubdrv_common_msg_stat *stat, char *str)
{
    ka_fs_seq_printf(seq, "|%-13s|%-10llu|%-19llu|%-18llu|%-15llu|%-13llu|%-10llu|%-19llu|%-16llu|%-11llu|\n", str,
                     stat->tx_total, stat->tx_check_data_err, stat->tx_chan_null_err, stat->tx_enodev_err,
                     stat->tx_chan_err, stat->rx_total, stat->rx_cb_process_err, stat->rx_null_cb_err, stat->rx_finish);
    return;
}

int ubdrv_procfs_mem_decoder_dfx_show(ka_seq_file_t *seq, void *offset)
{
    struct ubmem_nonlinear_pa_info *nonlinear_info = NULL;
    struct ubmem_decoder_msg_data *decoder_info = NULL;
    u32 i;

    ka_task_down_read(&g_decoder_info.rw_sem);
    if (g_decoder_info.valid == ASCEND_UB_INVALID) {
        ka_fs_seq_printf(seq, "Ub mem decoder info is invalid.\n");
        ka_task_up_read(&g_decoder_info.rw_sem);
        return 0;
    }

    decoder_info = g_decoder_info.decoder_info;
    ka_fs_seq_printf(seq, "--------------- ub mem_decoder info ---------------.\n");
    ka_fs_seq_printf(seq, "version: %hu\n", decoder_info->version);
    ka_fs_seq_printf(seq, "super_pod_size: %hu\n", decoder_info->super_pod_size);
    ka_fs_seq_printf(seq, "mem_id: %hu\n", decoder_info->mem_id);
    ka_fs_seq_printf(seq, "tid: %u\n", decoder_info->tid);
    ka_fs_seq_printf(seq, "max_share_mem_size: 0x%llx\n", decoder_info->max_share_mem_size);
    ka_fs_seq_printf(seq, "uba: %pK\n", (void *)(uintptr_t)decoder_info->uba);
    ka_fs_seq_printf(seq, "linear_pa_base: %pK\n", (void *)(uintptr_t)decoder_info->linear_pa_base);
    ka_fs_seq_printf(seq, "linear_pa_size: 0x%llx\n", decoder_info->linear_pa_size);
    ka_fs_seq_printf(seq, "total_nonlinear_pa_num: %llu\n", (u64)decoder_info->total_nonlinear_pa_num);
    ka_fs_seq_printf(seq, "rsv: %llu\n", (u64)decoder_info->rsv);
    ka_fs_seq_printf(seq, "update_cnt: %d\n", ka_base_atomic_read(&g_decoder_info.query_stats.update_cnt));
    ka_fs_seq_printf(seq, "query: uda_trigger=%d ubcore_fail=%d other_fail=%d\n",
                     ka_base_atomic_read(&g_decoder_info.query_stats.uda_trigger_cnt),
                     ka_base_atomic_read(&g_decoder_info.query_stats.ubcore_fail_cnt),
                     ka_base_atomic_read(&g_decoder_info.query_stats.other_fail_cnt));
    ka_fs_seq_printf(seq, "trans req: default=%d onepath=%d multipath=%d\n",
                     ka_base_atomic_read(&g_decoder_info.trans_stats.req_cnt[UBDRV_TRANS_PATH_DEFAULT]),
                     ka_base_atomic_read(&g_decoder_info.trans_stats.req_cnt[UBDRV_TRANS_PATH_ONEPATH]),
                     ka_base_atomic_read(&g_decoder_info.trans_stats.req_cnt[UBDRV_TRANS_PATH_MULTIPATH]));
    ka_fs_seq_printf(seq, "trans result: onepath=%d multipath=%d linear=%d fail=%d\n",
                     ka_base_atomic_read(&g_decoder_info.trans_stats.result_cnt[UBDRV_TRANS_RESULT_ONEPATH]),
                     ka_base_atomic_read(&g_decoder_info.trans_stats.result_cnt[UBDRV_TRANS_RESULT_MULTIPATH]),
                     ka_base_atomic_read(&g_decoder_info.trans_stats.result_cnt[UBDRV_TRANS_RESULT_LINEAR]),
                     ka_base_atomic_read(&g_decoder_info.trans_stats.result_cnt[UBDRV_TRANS_RESULT_FAIL]));
    ka_fs_seq_printf(seq, "| remote_id | to_host | linear_val | onepath | rsv           "
                          "| pa_base            | pa_size           \n");
    for (i = 0; i < decoder_info->total_nonlinear_pa_num; i++) {
        nonlinear_info = &decoder_info->nonlinear_pa_info[i];
        ka_fs_seq_printf(seq, "| %-10llu| %-8llu| %-11llu| %-8llu| 0x%-12llx| %-18pK| 0x%llx\n",
                         (u64)nonlinear_info->remote_id, (u64)nonlinear_info->to_host,
                         (u64)nonlinear_info->linear_pa_valid, (u64)nonlinear_info->onepath, (u64)nonlinear_info->rsv,
                         (void *)(uintptr_t)nonlinear_info->pa_base, nonlinear_info->pa_size);
    }
    ka_task_up_read(&g_decoder_info.rw_sem);
    return 0;
}

STATIC int ubdrv_procfs_mem_decoder_dfx(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_single_open(file, ubdrv_procfs_mem_decoder_dfx_show, inode->i_private);
}
STATIC_PROCFS_FILE_FUNC_OPS(mem_decoder, ubdrv_procfs_mem_decoder_dfx, NULL);

int ubdrv_procfs_send_admin_msg(struct ubdrv_chan_dfx_cmd *cmd, u32 dev_id, u32 sub_cmd, u32 chan_id)
{
    struct ascend_ub_user_data user_desc = {0};

    cmd->opcode = sub_cmd;
    cmd->chan_id = chan_id;
    user_desc.opcode = UBDRV_SYNC_DFX_INFO;
    user_desc.size = (u32)sizeof(struct ubdrv_chan_dfx_cmd);
    user_desc.reply_size = (u32)sizeof(struct ubdrv_chan_dfx_cmd);
    user_desc.cmd = cmd;
    user_desc.reply = cmd;
    return ubdrv_admin_send_msg(dev_id, &user_desc);
}

void ubdrv_procfs_set_prof_state(bool state)
{
    g_enable_prof = state;
    ubdrv_info("Set prof state success. (val=%d;uid=%u)\n", state, ka_task_get_current_cred_uid());
}

bool ubdrv_procfs_get_prof_state(void)
{
    return g_enable_prof;
}

STATIC int ubdrv_procfs_link_dfx_show(ka_seq_file_t *seq, void *offset)
{
    struct ascend_ub_admin_chan *link_chan = NULL;
    char urma_name[UBCORE_MAX_DEV_NAME] = {0};
    struct ubdrv_msg_chan_stat stat = {0};
    struct ub_idev *idev = NULL;
    u32 idev_id = 0, dev_id = 0;

    ubdrv_procfs_show_chan_dfx_head(seq);
    for (idev_id = 0; idev_id < ASCEND_UDMA_DEV_MAX_NUM; idev_id++) {
        idev = ubdrv_get_idev(idev_id);
        link_chan = &idev->link_chan;
        ka_task_down_write(&idev->rw_sem);
        if ((link_chan->valid != UB_ADMIN_MSG_ENABLE) || (idev->valid != ASCEND_UB_VALID)) {
            ka_task_up_write(&idev->rw_sem);
            continue;
        }
        dev_id = idev->dev_id[0];
        (void)memcpy_s(urma_name, sizeof(urma_name), idev->ubc_dev->dev_name, sizeof(idev->ubc_dev->dev_name));
        (void)memcpy_s(&stat, sizeof(struct ubdrv_msg_chan_stat), &link_chan->chan_stat,
                       sizeof(struct ubdrv_msg_chan_stat));
        ka_task_up_write(&idev->rw_sem);
        ubdrv_procfs_show_chan_dfx(seq, offset, &stat, dev_id, urma_name);
    }
    return 0;
}

STATIC int ubdrv_procfs_link_dfx(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_single_open(file, ubdrv_procfs_link_dfx_show, inode->i_private);
}

STATIC_PROCFS_FILE_FUNC_OPS(pair_info, ubdrv_procfs_pair_info, NULL);
STATIC_PROCFS_FILE_FUNC_OPS(link_stat, ubdrv_procfs_link_dfx, NULL);

STATIC int ubdrv_proc_create_data(ka_proc_dir_entry_t *top_entry)
{
    g_procfs_entry.pair_info = ka_fs_proc_create_data("pair_info", KA_S_IRUSR, top_entry, &pair_info,
                                                      &g_user_data.dev_id);
    g_procfs_entry.link_stat = ka_fs_proc_create_data("link", KA_S_IRUSR, top_entry, &link_stat, NULL);
    if ((g_procfs_entry.pair_info == NULL) || (g_procfs_entry.link_stat == NULL)) {
        ubdrv_err("Create proc pair_info or link_stat data error.\n");
        return -EBADR;
    }

    g_procfs_entry.mem_decoder = ka_fs_proc_create_data("decoder", KA_S_IRUSR, top_entry, &mem_decoder, NULL);
    if (g_procfs_entry.mem_decoder == NULL) {
        ubdrv_err("Create proc decoder data error.\n");
        return -EBADR;
    }

    return ubdrv_proc_create_data_proc(top_entry, &g_procfs_entry);
}

int ubdrv_procfs_init(void)
{
    ka_proc_dir_entry_t *top_entry = NULL;
    int ret;

    top_entry = ka_fs_proc_mkdir("asdrv_ub", NULL);
    if (top_entry == NULL) {
        ubdrv_err("Create proc dir error.\n");
        return -EINVAL;
    }
    ret = ubdrv_proc_create_data(top_entry);
    if (ret != 0) {
        ubdrv_err("Create proc dir error.\n");
        (void)ka_fs_remove_proc_subtree("asdrv_ub", NULL);
        return ret;
    }
    ka_base_atomic_set(&fs_valid, UBDRV_PROCFS_VALID);

    return 0;
}
DECLAER_FEATURE_AUTO_INIT(ubdrv_procfs_init, FEATURE_LOADER_STAGE_5);

void ubdrv_procfs_uninit(void)
{
    if (ka_base_atomic_read(&fs_valid) == UBDRV_PROCFS_VALID) {
        (void)ka_fs_remove_proc_subtree("asdrv_ub", NULL);
    }
    return;
}
DECLAER_FEATURE_AUTO_UNINIT(ubdrv_procfs_uninit, FEATURE_LOADER_STAGE_5);
