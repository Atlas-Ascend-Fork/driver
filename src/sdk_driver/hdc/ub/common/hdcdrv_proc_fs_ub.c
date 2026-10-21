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
#include "ka_fs_pub.h"
#include "ka_task_pub.h"
#include "ka_kernel_def_pub.h"
#include "pbl/pbl_feature_loader.h"
#include "hdcdrv_cmd.h"
#include "hdcdrv_cmd_ioctl.h"
#include "hdcdrv_cmd_msg.h"
#include "hdcdrv_core_ub.h"
#include "hdcdrv_core_com_ub.h"
#include "hdcdrv_ctrl_msg.h"
#include "hdcdrv_proc_fs_ub.h"

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

// file directory : /proc/asdrv_hdc
static ka_proc_dir_entry_t *ascend_hdc_entry = NULL;
static bool proc_fs_valid_flag = false;

static struct hdcdrv_user_input g_host_id = {
    .dev_id = 0,
};

static struct hdcdrv_procfs_entry g_procfs_entry = {
    .dev_id = NULL,
    .hdcdrv_dev_stat = NULL,
};

#define DEVID_LEN 5
#define KSTRTOL_BASE 10
STATIC ssize_t set_global_id(ka_file_t *filp, const char *buf, size_t count, loff_t *offp, u32 *output)
{
    char msg[DEVID_LEN] = {0};
    ssize_t ret = 0;
    u32 temp;

    if (count > DEVID_LEN) {
        hdcdrv_err("Set_global_id count size err!(count=%lu)\n", count);
        return -EINVAL;
    }

    if (ka_base_copy_from_user(msg, buf, count)) {
        hdcdrv_err("Set_global_id ka_base_copy_from_user err!\n");
        return -EFAULT;
    }

    msg[DEVID_LEN - 1] = '\0';
    ret = ka_base_kstrtou32(msg, 0, &temp);
    if (ret < 0) {
        hdcdrv_err("Set_global_id ka_base_kstrtol err!(ret=%ld)\n", ret);
        return -EINVAL;
    }

    KA_WRITE_ONCE(*output, (u32)temp);
    ret = (ssize_t)count;

    return ret;
}

STATIC ssize_t write_dev_id(ka_file_t *filp, const char *buf, size_t count, loff_t *offp)
{
    u32 dev_id = 0;
    int ret = 0;
    ret = set_global_id(filp, buf, count, offp, &dev_id);
    if (ret < 0) {
        return ret;
    }

    if (dev_id >= HDCDRV_SUPPORT_MAX_DEV) {
        ret = -ERANGE;
    } else {
        g_host_id.dev_id = dev_id;
    }

    return ret;
}

STATIC int read_dev_id_proc_show(ka_seq_file_t *m, void *v)
{
    ka_fs_seq_printf(m, "[ dev_id = %u ]\n", g_host_id.dev_id);
    return 0;
}

STATIC int read_dev_id_proc_open(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_single_open(file, read_dev_id_proc_show, NULL);
}

STATIC void hdcdrv_fill_session_info_list_and_show(ka_seq_file_t *m, struct hdcdrv_dev *hdc_dev,
                                                   struct hdcdrv_session_list_stat *s_brief)
{
    int i;
    int status;

    for (i = 0; i < HDCDRV_UB_SINGLE_DEV_MAX_SESSION; i++) {
        status = hdcdrv_get_session_status(&hdc_dev->sessions[i]);
        /* devices in the same OS share sessions. Count the number of idle sessions before check devid. */
        if (status == HDCDRV_SESSION_STATUS_CONN) {
            s_brief->active_list[s_brief->active_num++] = i;
        } else if (status == HDCDRV_SESSION_STATUS_IDLE) {
            s_brief->idle_list[s_brief->idle_num++] = i;
        }
    }

    ka_fs_seq_printf(m, "\n-------------------show session list info-------------------\n");
    ka_fs_seq_printf(m, "\nTotal session number: %d\n", HDCDRV_UB_SINGLE_DEV_MAX_SESSION);
    ka_fs_seq_printf(m,
                     "Total active session number: %d\n"
                     "active session list: ",
                     s_brief->active_num);
    for (i = 0; i < s_brief->active_num; i++) {
        ka_fs_seq_printf(m, "%d ", s_brief->active_list[i]);
    }

    ka_fs_seq_printf(m, "\nTotal idle session number: %d\n", s_brief->idle_num);
    for (i = 0; i < s_brief->idle_num; i++) {
        ka_fs_seq_printf(m, "%d ", s_brief->idle_list[i]);
    }
}

STATIC void hdcdrv_fill_msg_pool_stat_and_show(ka_seq_file_t *m, struct hdcdrv_dev *hdc_dev)
{
    struct hdcdrv_ctrl_msg_pool_stat pool_stat[HDCDRV_MSG_POOL_TYPE_MAX] = {0};
    int i;

    ka_fs_seq_printf(m, "\n-------------------show ctrl msg pool info-------------------\n");
    for (i = 0; i < HDCDRV_MSG_POOL_TYPE_MAX; i++) {
        hdcdrv_get_ctrl_msg_pool_stat(hdc_dev, i, &pool_stat[i]);
        ka_fs_seq_printf(m, "\nCtrl_msg_pool type: %d depth: %d head: %d tail: %d\n", pool_stat[i].pool_type,
                         pool_stat[i].depth, pool_stat[i].head, pool_stat[i].tail);
        ka_fs_seq_printf(m, "Connect_msg_num: %d\n", pool_stat[i].connect_msg_num);
        ka_fs_seq_printf(m, "Dfx_msg_num: %d\n", pool_stat[i].dfx_msg_num);
        ka_fs_seq_printf(m, "Close_msg_num: %d\n", pool_stat[i].close_msg_num);
    }
    return;
}

STATIC int hdcdrv_list_stat_init(struct hdcdrv_session_list_stat *s_brief)
{
    s_brief->active_list = (int *)hdcdrv_ub_kvmalloc(sizeof(int) * HDCDRV_UB_SINGLE_DEV_MAX_SESSION,
                                                     KA_SUB_MODULE_TYPE_4);
    if (s_brief->active_list == NULL) {
        hdcdrv_err("alloc active list err\n");
        return -ENOMEM;
    }

    s_brief->idle_list = (int *)hdcdrv_ub_kvmalloc(sizeof(int) * HDCDRV_UB_SINGLE_DEV_MAX_SESSION,
                                                   KA_SUB_MODULE_TYPE_4);
    if (s_brief->idle_list == NULL) {
        hdcdrv_err("alloc idle list err\n");
        hdcdrv_ub_kvfree((void **)&s_brief->active_list, KA_SUB_MODULE_TYPE_4);
        return -ENOMEM;
    }

    return HDCDRV_OK;
}

STATIC void hdcdrv_list_stat_uninit(struct hdcdrv_session_list_stat *s_brief)
{
    hdcdrv_ub_kvfree((void **)&s_brief->active_list, KA_SUB_MODULE_TYPE_4);
    hdcdrv_ub_kvfree((void **)&s_brief->idle_list, KA_SUB_MODULE_TYPE_4);

    return;
}

STATIC void hdcdrv_info_proc_show(ka_seq_file_t *m, void *v, struct hdcdrv_dev *hdc_dev)
{
    struct hdcdrv_session_list_stat s_brief = {0};
    int i, ret;

    ka_fs_seq_printf(m, "HDC device %u statistics:\n", hdc_dev->dev_id);
    ka_fs_seq_printf(m, "-------------------show mem pool info-------------------\n");
    ka_fs_seq_printf(m, "\nmem_pool \t\t status \t\t session_id \t\t pid\n");
    for (i = 0; i < HDCDRV_MEM_POOL_NUM; i++) {
        ka_fs_seq_printf(m, "mem_pool[%d]: \t\t %d \t\t %d \t\t %llu\n", i, hdc_dev->mem_pool_list[i].valid,
                         hdc_dev->mem_pool_list[i].session_id, hdc_dev->mem_pool_list[i].pool.pid);
    }

    hdcdrv_fill_msg_pool_stat_and_show(m, hdc_dev);
    ret = hdcdrv_list_stat_init(&s_brief);
    if (ret != HDCDRV_OK) {
        ka_fs_seq_printf(m, "\nCan not malloc mem for session list.\n");
        return;
    }
    hdcdrv_fill_session_info_list_and_show(m, hdc_dev, &s_brief);
    hdcdrv_list_stat_uninit(&s_brief);

    ka_fs_seq_printf(m, "\n------------------- END -------------------\n");

    return;
}

STATIC int hdcdrv_dev_stat_proc_show(ka_seq_file_t *m, void *v)
{
    u32 dev_id = g_host_id.dev_id;
    struct hdcdrv_dev *hdc_dev = NULL;

    hdc_dev = hdcdrv_ub_get_dev(dev_id);
    if ((hdc_dev == NULL) || (hdc_dev->valid != HDCDRV_VALID)) {
        ka_fs_seq_printf(m, "asdrv_hdc get hdc_dev failed, please input valid id.(dev_id = %d)\n", dev_id);
        return 0;
    }

    ka_task_mutex_lock(&g_hdc_ctrl.dev_lock[hdc_dev->dev_id]);
    hdcdrv_info_proc_show(m, v, hdc_dev);
    ka_task_mutex_unlock(&g_hdc_ctrl.dev_lock[hdc_dev->dev_id]);
    hdcdrv_put_dev(dev_id);
    return 0;
}

STATIC int hdcdrv_dev_stat_proc_open(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_single_open(file, hdcdrv_dev_stat_proc_show, NULL);
}

STATIC_PROCFS_FILE_FUNC_OPS(hdcdrv_dev_id, read_dev_id_proc_open, write_dev_id);
STATIC_PROCFS_FILE_FUNC_OPS(hdcdrv_dev_stat, hdcdrv_dev_stat_proc_open, NULL);

STATIC int hdcdrv_dev_id_proc_fs(void)
{
    g_procfs_entry.dev_id = ka_fs_proc_create_data("dev_id", KA_S_IRUSR | KA_S_IWUSR, ascend_hdc_entry, &hdcdrv_dev_id,
                                                   NULL);

    if (g_procfs_entry.dev_id == NULL) {
        hdcdrv_err("Create dev_id_entry dir failed.\n");
        return HDCDRV_ERR;
    }

    return HDCDRV_OK;
}

STATIC int hdcdrv_dev_stat_info_proc_fs(void)
{
    g_procfs_entry.hdcdrv_dev_stat = ka_fs_proc_create_data("dev_stat", KA_S_IRUSR, ascend_hdc_entry, &hdcdrv_dev_stat,
                                                            NULL);

    if (g_procfs_entry.hdcdrv_dev_stat == NULL) {
        hdcdrv_err("Create hdcdrv_dev_stat_entry dir failed.\n");
        return HDCDRV_ERR;
    }

    return HDCDRV_OK;
}

int hdcdrv_proc_fs_init(void)
{
    ascend_hdc_entry = ka_fs_proc_mkdir("asdrv_hdc", NULL);
    if (ascend_hdc_entry == NULL) {
        hdcdrv_err("Create asdrv_hdc entry dir failed\n");
        proc_fs_valid_flag = false;
        return HDCDRV_OK;
    }

    if ((hdcdrv_dev_id_proc_fs() != 0) || (hdcdrv_dev_stat_info_proc_fs() != 0)) {
        (void)ka_fs_remove_proc_subtree("asdrv_hdc", NULL);
        hdcdrv_err("asdrv_hdc proc fs init failed.\n");
        proc_fs_valid_flag = false;
        return HDCDRV_OK;
    }
    proc_fs_valid_flag = true;
    return HDCDRV_OK;
}
DECLAER_FEATURE_AUTO_INIT(hdcdrv_proc_fs_init, FEATURE_LOADER_STAGE_5);

void hdcdrv_proc_fs_uninit(void)
{
    if (proc_fs_valid_flag) {
        (void)ka_fs_remove_proc_subtree("asdrv_hdc", NULL);
    }
    return;
}
DECLAER_FEATURE_AUTO_UNINIT(hdcdrv_proc_fs_uninit, FEATURE_LOADER_STAGE_5);