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

#include "securec.h"
#include "ka_base_pub.h"
#include "ka_common_pub.h"
#include "ka_system_pub.h"
#include "ka_task_pub.h"
#include "ka_compiler_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_list_pub.h"
#include "ka_fs_pub.h"
#include "pbl_mem_alloc_interface.h"
#include "udis_log.h"
#include "udis_dfx.h"

#define PROC_FS_R_MODE 0444
#define PROC_FS_RW_MODE 0644
#define UDIS_DFX_ROOT_DIR "udis_dfx"
#define UDIS_DFX_STAT_FILE "stat_info"
#define UDIS_DFX_CMD_BUF_LEN 16

STATIC ka_list_head_t g_dfx_stat_list;
STATIC ka_rw_semaphore_t g_dfx_stat_lock;
STATIC ka_proc_dir_entry_t *g_dfx_root_entry = NULL;
STATIC unsigned long long g_dfx_window_ms = (unsigned long long)UDIS_DFX_DEFAULT_WINDOW_SEC *
                                            UDIS_DFX_WINDOW_GRANULARITY_MS;

static const char *udis_dfx_module_type_to_str(UDIS_MODULE_TYPE type)
{
    static const char *module_str[] = {
        [UDIS_MODULE_DEVMNG] = "DEVMNG", [UDIS_MODULE_MEMORY] = "MEMORY",   [UDIS_MODULE_LP] = "LP",
        [UDIS_MODULE_SVM] = "SVM",       [UDIS_MODULE_NETWORK] = "NETWORK", [UDIS_MODULE_TS] = "TS",
        [UDIS_MODULE_APM] = "APM",
    };

    if (type >= UDIS_MODULE_MAX) {
        return "UNKNOWN";
    }
    return module_str[type];
}

static unsigned long long udis_dfx_get_cur_ms(void)
{
    return ka_system_ktime_get_raw_ns() / KA_NSEC_PER_MSEC;
}

static struct udis_dfx_stat_entry *udis_dfx_find_entry(UDIS_MODULE_TYPE module_type, const char *name)
{
    struct udis_dfx_stat_entry *entry = NULL;

    ka_list_for_each_entry(entry, &g_dfx_stat_list, list)
    {
        if ((entry->module_type == module_type) && (ka_base_strcmp(entry->name, name) == 0)) {
            return entry;
        }
    }
    return NULL;
}

static struct udis_dfx_stat_entry *udis_dfx_create_entry(UDIS_MODULE_TYPE module_type, const char *name,
                                                         unsigned int acc_ctrl)
{
    struct udis_dfx_stat_entry *entry = NULL;
    int ret;

    entry = (struct udis_dfx_stat_entry *)dbl_kzalloc(sizeof(struct udis_dfx_stat_entry),
                                                      KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (entry == NULL) {
        udis_err("Failed to alloc dfx stat entry. (module_type=%u; name=%s)\n", module_type, name);
        return NULL;
    }

    entry->module_type = module_type;
    entry->acc_ctrl = acc_ctrl;
    entry->window_start_ms = udis_dfx_get_cur_ms();

    ret = strcpy_s(entry->name, UDIS_MAX_NAME_LEN, name);
    if (ret != 0) {
        udis_err("strcpy_s failed. (name=%s; ret=%d)\n", name, ret);
        dbl_kfree(entry);
        return NULL;
    }

    ka_list_add(&entry->list, &g_dfx_stat_list);
    return entry;
}

static void udis_dfx_refresh_window(struct udis_dfx_stat_entry *entry)
{
    unsigned long long cur_ms = udis_dfx_get_cur_ms();

    if (cur_ms >= entry->window_start_ms && (cur_ms - entry->window_start_ms) >= g_dfx_window_ms) {
        entry->last_window_start_ms = entry->window_start_ms;
        entry->last_window_end_ms = cur_ms;
        entry->last_window_count = entry->set_count_window;
        entry->set_count_window = 0;
        entry->window_start_ms = cur_ms;
    }
}

void udis_dfx_record_register(UDIS_MODULE_TYPE module_type, const char *name, unsigned int acc_ctrl)
{
    struct udis_dfx_stat_entry *entry = NULL;

    if ((module_type >= UDIS_MODULE_MAX) || (name == NULL)) {
        return;
    }

    ka_task_down_write(&g_dfx_stat_lock);
    entry = udis_dfx_find_entry(module_type, name);
    if (entry == NULL) {
        entry = udis_dfx_create_entry(module_type, name, acc_ctrl);
        if (entry == NULL) {
            ka_task_up_write(&g_dfx_stat_lock);
            return;
        }
    } else {
        entry->acc_ctrl = acc_ctrl;
    }

    entry->register_count++;
    entry->last_op_time_ms = udis_dfx_get_cur_ms();
    ka_task_up_write(&g_dfx_stat_lock);
}

void udis_dfx_record_unregister(UDIS_MODULE_TYPE module_type, const char *name)
{
    struct udis_dfx_stat_entry *entry = NULL;

    if ((module_type >= UDIS_MODULE_MAX) || (name == NULL)) {
        return;
    }

    ka_task_down_write(&g_dfx_stat_lock);
    entry = udis_dfx_find_entry(module_type, name);
    if (entry != NULL) {
        entry->unregister_count++;
        entry->last_op_time_ms = udis_dfx_get_cur_ms();
    }
    ka_task_up_write(&g_dfx_stat_lock);
}

void udis_dfx_record_set(UDIS_MODULE_TYPE module_type, const char *name, unsigned int acc_ctrl)
{
    struct udis_dfx_stat_entry *entry = NULL;

    if ((module_type >= UDIS_MODULE_MAX) || (name == NULL)) {
        return;
    }

    ka_task_down_write(&g_dfx_stat_lock);
    entry = udis_dfx_find_entry(module_type, name);
    if (entry == NULL) {
        entry = udis_dfx_create_entry(module_type, name, acc_ctrl);
        if (entry == NULL) {
            ka_task_up_write(&g_dfx_stat_lock);
            return;
        }
    } else {
        entry->acc_ctrl = acc_ctrl;
    }

    udis_dfx_refresh_window(entry);
    entry->set_count_total++;
    entry->set_count_window++;
    entry->last_op_time_ms = udis_dfx_get_cur_ms();
    ka_task_up_write(&g_dfx_stat_lock);
}

void udis_dfx_reset_stat(void)
{
    struct udis_dfx_stat_entry *entry = NULL;
    struct udis_dfx_stat_entry *tmp = NULL;

    ka_task_down_write(&g_dfx_stat_lock);
    ka_list_for_each_entry_safe(entry, tmp, &g_dfx_stat_list, list)
    {
        entry->register_count = 0;
        entry->unregister_count = 0;
        entry->set_count_total = 0;
        entry->set_count_window = 0;
        entry->last_window_count = 0;
        entry->last_window_start_ms = 0;
        entry->last_window_end_ms = 0;
        entry->window_start_ms = udis_dfx_get_cur_ms();
        entry->last_op_time_ms = 0;
    }
    ka_task_up_write(&g_dfx_stat_lock);
}

void udis_dfx_set_window(unsigned int window_sec)
{
    struct udis_dfx_stat_entry *entry = NULL;
    struct udis_dfx_stat_entry *tmp = NULL;

    if ((window_sec == 0) || (window_sec > UDIS_DFX_MAX_WINDOW_SEC)) {
        udis_err("Invalid window. (window_sec=%u; min=1; max=%u)\n", window_sec, UDIS_DFX_MAX_WINDOW_SEC);
        return;
    }

    ka_task_down_write(&g_dfx_stat_lock);
    g_dfx_window_ms = (unsigned long long)window_sec * UDIS_DFX_WINDOW_GRANULARITY_MS;
    ka_list_for_each_entry_safe(entry, tmp, &g_dfx_stat_list, list)
    {
        entry->set_count_window = 0;
        entry->last_window_count = 0;
        entry->last_window_start_ms = 0;
        entry->last_window_end_ms = 0;
        entry->window_start_ms = udis_dfx_get_cur_ms();
    }
    ka_task_up_write(&g_dfx_stat_lock);

    udis_info("Udis dfx window set to %us.\n", window_sec);
}

static int udis_dfx_stat_show(ka_seq_file_t *seq, void *offset)
{
    struct udis_dfx_stat_entry *entry = NULL;
    unsigned int total_entries = 0;

    ka_fs_seq_printf(seq, "%-10s %-16s %-12s %-12s %-12s %-12s %-12s %-12s %-14s\n", "module", "info_name", "acc_ctrl",
                     "reg_cnt", "unreg_cnt", "set_total", "set_win", "last_win", "last_win_span");
    ka_fs_seq_printf(
        seq, "--------------------------------------------------------------------------------------------------------"
             "------------------------------\n");

    ka_task_down_read(&g_dfx_stat_lock);
    ka_list_for_each_entry(entry, &g_dfx_stat_list, list)
    {
        unsigned long long last_win_span_ms = 0;

        if (entry->set_count_total > 0) {
            udis_dfx_refresh_window(entry);
            if (entry->last_window_end_ms != 0) {
                last_win_span_ms = entry->last_window_end_ms - entry->last_window_start_ms;
            }
            ka_fs_seq_printf(seq, "%-10s %-16s 0x%-10x %-12llu %-12llu %-12llu %-12u %-12llu %-14llu\n",
                             udis_dfx_module_type_to_str(entry->module_type), entry->name, entry->acc_ctrl,
                             entry->register_count, entry->unregister_count, entry->set_count_total,
                             entry->set_count_window, entry->last_window_count, last_win_span_ms);
        } else {
            ka_fs_seq_printf(seq, "%-10s %-16s 0x%-10x %-12llu %-12llu %-12llu %-12u %-12llu %-14s\n",
                             udis_dfx_module_type_to_str(entry->module_type), entry->name, entry->acc_ctrl,
                             entry->register_count, entry->unregister_count, entry->set_count_total,
                             entry->set_count_window, entry->last_window_count, "-");
        }
        total_entries++;
    }
    ka_task_up_read(&g_dfx_stat_lock);

    ka_fs_seq_printf(
        seq, "--------------------------------------------------------------------------------------------------------"
             "------------------------------\n");
    ka_fs_seq_printf(seq, "total_entries: %u  window: %llus (max: %us)\n", total_entries,
                     g_dfx_window_ms / UDIS_DFX_WINDOW_GRANULARITY_MS, UDIS_DFX_MAX_WINDOW_SEC);
    return 0;
}

static int udis_dfx_stat_open(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_single_open(file, udis_dfx_stat_show, ka_fs_get_inode_i_private(inode));
}

static ssize_t udis_dfx_stat_write(ka_file_t *file, const char __ka_user *buffer, size_t count, loff_t *ppos)
{
    char cmd[UDIS_DFX_CMD_BUF_LEN] = {0};
    char *cmd_token = NULL;
    char *arg_token = NULL;

    if ((count == 0) || (count > sizeof(cmd))) {
        udis_err("Invalid input count. (count=%lu; max=%zu)\n", count, sizeof(cmd));
        return -EINVAL;
    }

    if (ka_base_copy_from_user(cmd, (void *)buffer, count) != 0) {
        udis_err("Copy from user failed.\n");
        return -EINVAL;
    }
    cmd[count - 1] = '\0';

    cmd_token = strtok_s(cmd, " ", &arg_token);
    if (cmd_token == NULL) {
        udis_err("Empty command.\n");
        return -EINVAL;
    }

    if (ka_base_strcmp(cmd_token, "reset") == 0) {
        udis_dfx_reset_stat();
        udis_info("Udis dfx statistics reset.\n");
    } else if (ka_base_strcmp(cmd_token, "window") == 0) {
        unsigned int window_sec = 0;
        if (arg_token == NULL) {
            udis_err("Missing window argument. (usage: window N)\n");
            return -EINVAL;
        }
        if (ka_base_kstrtouint(arg_token, 10, &window_sec) != 0) {
            udis_err("Invalid window argument. (arg=%s)\n", arg_token);
            return -EINVAL;
        }
        udis_dfx_set_window(window_sec);
    } else {
        udis_err("Unknown command. (cmd=%s; valid_cmds=reset|window N)\n", cmd_token);
        return -EINVAL;
    }

    return (ssize_t)count;
}

STATIC_PROCFS_FILE_FUNC_OPS_WRITE(udis_dfx_stat_ops, udis_dfx_stat_open, udis_dfx_stat_write);

int udis_dfx_init(void)
{
    KA_INIT_LIST_HEAD(&g_dfx_stat_list);
    ka_task_init_rwsem(&g_dfx_stat_lock);
    g_dfx_window_ms = (unsigned long long)UDIS_DFX_DEFAULT_WINDOW_SEC * UDIS_DFX_WINDOW_GRANULARITY_MS;

    g_dfx_root_entry = ka_fs_proc_mkdir(UDIS_DFX_ROOT_DIR, NULL);
    if (g_dfx_root_entry == NULL) {
        udis_err("Failed to create proc dir. (dir=%s)\n", UDIS_DFX_ROOT_DIR);
        return -ENOMEM;
    }

    if (ka_fs_proc_create_data(UDIS_DFX_STAT_FILE, PROC_FS_RW_MODE, g_dfx_root_entry, &udis_dfx_stat_ops, NULL) ==
        NULL) {
        udis_err("Failed to create proc file. (file=%s)\n", UDIS_DFX_STAT_FILE);
        ka_fs_proc_remove(g_dfx_root_entry);
        g_dfx_root_entry = NULL;
        return -ENOMEM;
    }

    udis_info("Udis dfx init success. (path=/proc/%s/%s; default_window=%llus)\n", UDIS_DFX_ROOT_DIR,
              UDIS_DFX_STAT_FILE, g_dfx_window_ms / UDIS_DFX_WINDOW_GRANULARITY_MS);
    return 0;
}

void udis_dfx_uninit(void)
{
    struct udis_dfx_stat_entry *entry = NULL;
    struct udis_dfx_stat_entry *tmp = NULL;

    if (g_dfx_root_entry != NULL) {
        ka_fs_proc_remove(g_dfx_root_entry);
        g_dfx_root_entry = NULL;
    }

    ka_list_for_each_entry_safe(entry, tmp, &g_dfx_stat_list, list)
    {
        ka_list_del(&entry->list);
        dbl_kfree(entry);
    }

    udis_info("Udis dfx uninit success.\n");
}
