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

#ifndef CFG_EDGE_HOST
#include <linux/profile.h>
#endif

#include "ka_memory_pub.h"
#include "ka_fs_pub.h"
#include "ka_base_pub.h"
#include "ka_errno_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_dfx_pub.h"
#include "ka_common_pub.h"
#include "devdrv_user_common.h"
#include "pbl_mem_alloc_interface.h"
#include "securec.h"
#include "dms_define.h"
#include "urd_feature.h"
#include "dms_common.h"
#include "dms_probe.h"
#include "dms_template.h"
#include "dms_basic_info.h"
#include "dms_timer.h"
#include "dms_event_adapt.h"
#include "dms_product.h"
#include "devmng_dms_adapt.h"
#include "pbl/pbl_davinci_api.h"
#include "dms_sysfs.h"

#include "pbl/pbl_feature_loader.h"
#include "davinci_interface.h"
#include "dms_init.h"
#define MAX_EVENT_CONFIG_SIZE (2 * 1024 * 1024)

STATIC int dms_event_config_verify(u32 event_code, u32 severity, int config_cnt)
{
    int i;

    if (severity > (u32)DMS_EVENT_CRITICAL) {
        dms_err("severity invalid. (event_code=0x%x; severity=%u)\n", event_code, severity);
        return -EINVAL;
    }

    for (i = 0; i < config_cnt; i++) {
        if ((event_code == g_event_configs.event_configs[i].event_code) &&
            (severity != g_event_configs.event_configs[i].severity)) {
            dms_err("event_code repeated. (event_code=0x%x; severity=%u)\n", event_code, severity);
            return -EINVAL;
        }
    }

    return 0;
}

STATIC int dms_parse_event_config(const char *line_buf, int size, struct dms_event_config *conf, int *cnt)
{
    int ret;
    u32 event_code, severity;

    if (line_buf == NULL || size == 0) {
        dms_err("line_buf is NULL or size is 0\n");
        return -EINVAL;
    }

    ret = sscanf_s(line_buf, "0x%x %u", &event_code, &severity);
    if (ret != 2) { /* 2: sscanf_s element number */
        dms_err("sscanf_s fail. (ret=%d; linecnt=%d)\n", ret, *cnt);
        return -ENOMEM;
    }

    ret = dms_event_config_verify(event_code, severity, *cnt);
    if (ret != 0) {
        dms_err("dms event_config verify failed. (ret=%d)\n", ret);
        return ret;
    }

    conf->event_code = event_code;
    conf->severity = severity;

    (*cnt)++;
    if (*cnt >= EVENT_INFO_ARRAY_MAX) {
        dms_err("The number of input event config is greater than the max array size. (event_config_num=%d; "
                "max_array_size=%d)\n",
                *cnt, EVENT_INFO_ARRAY_MAX);
        return -EFBIG;
    }

    return 0;
}
#define TMP_BUF_MAX 128
STATIC int get_eventinfo_from_buf(char *config_buf, size_t lBufLens)
{
    int ret;
    int i;
    char *curr = config_buf;
    int cnt = 0;
    int tpcnt = 0;
    char *line_buf = NULL;

    line_buf = dbl_kzalloc(TMP_BUF_MAX, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (line_buf == NULL) {
        dms_err("kzalloc line_buf failed, (size = %d)\n", TMP_BUF_MAX);
        return -ENOMEM;
    }

    for (i = 0; i < (int)lBufLens; i++) {
        if (*curr == '#') { /* handle '#' line */
            while (*(++curr) != '\n') {
                i++;
            }
            continue;
        }

        if ((*curr != '\n') && (*curr != '\r') && (tpcnt < TMP_BUF_MAX)) {
            line_buf[tpcnt++] = *curr++;
            continue;
        }

        if (tpcnt >= TMP_BUF_MAX) {
            dms_err("line length %d exceeds buf size %d.\n", tpcnt, TMP_BUF_MAX);
            goto free_tmp_buf;
        }

        if (tpcnt == 0) { /* handle empty line */
            curr++;
            continue;
        }

        line_buf[tpcnt] = '\0';
        ret = dms_parse_event_config(line_buf, tpcnt, &g_event_configs.event_configs[cnt], &cnt);
        if (ret != 0) {
            dms_err("dms_parse_event_config fail, (ret = %d, linecnt = %d)\n", ret, cnt);
            goto free_tmp_buf;
        }

        ret = memset_s(line_buf, TMP_BUF_MAX, 0, TMP_BUF_MAX);
        if (ret != 0) {
            dms_err("memset_s fail, (ret = %d)\n", ret);
            goto free_tmp_buf;
        }
        tpcnt = 0;
        curr++;
    }

    g_event_configs.config_cnt = cnt;
    dbl_kfree(line_buf);
    return 0;

free_tmp_buf:
    dbl_kfree(line_buf);
    return -1;
}

STATIC int cmp(const void *a, const void *b)
{
    return (*(struct dms_event_config *)a).event_code - (*(struct dms_event_config *)b).event_code;
}

STATIC int get_file_size(size_t *buf_size)
{
    int ret;
    ka_kstat_t src_stat;

    ret = ka_fs_get_file_kstat(&src_stat, EVENT_INFO_CONFIG_PATH);
    if (ret != 0) {
        dms_err("vfs_getattr failed. (file: %s, src_stat.size = %lld, ret = %d)\n", EVENT_INFO_CONFIG_PATH,
                src_stat.size, ret);
        if (ret == -ENOENT) {
            return -ENOENT;
        }
        return ret;
    }
    *buf_size = (size_t)src_stat.size;
    return 0;
}

STATIC int read_file_to_buf(size_t file_size, char *config_buf)
{
    size_t read_size;
    int ret = -EIO;
    ka_file_t *src_filp = NULL;
    loff_t offset = 0;

    src_filp = ka_fs_filp_open(EVENT_INFO_CONFIG_PATH, KA_O_RDONLY, KA_S_IRUSR);
    if (KA_IS_ERR(src_filp)) {
        dms_err("unable to open (file: %s, errno = %ld)\n", EVENT_INFO_CONFIG_PATH, KA_PTR_ERR(src_filp));
        goto _end;
    }

    read_size = ka_fs_kernel_read(src_filp, config_buf, file_size, &offset);
    if (read_size != file_size) {
        dms_err("read file not success. (read bytes=%lu, file_size=%lu)\n", read_size, file_size);
        goto _end;
    }
    ret = 0;

_end:
    if (!KA_IS_ERR(src_filp)) {
        (void)ka_fs_filp_close(src_filp, NULL);
    }

    return ret;
}

int get_eventinfo_from_config(void)
{
    int ret;
    size_t file_size = 0;
    char *config_buf = NULL;
    (void)memset_s(&g_event_configs, sizeof(g_event_configs), 0, sizeof(g_event_configs));

    ret = get_file_size(&file_size);
    if (ret == -ENOENT) {
        dms_err("event config file does not exist.\n");
        return 0;
    }

    if (ret != 0) {
        dms_err("get_file_size fail! (ret = %d)\n", ret);
        return ret;
    }

    if (file_size >= (size_t)MAX_EVENT_CONFIG_SIZE) {
        dms_err("The file size(%lu bytes) exceeds the upper limit(%d bytes).\n", file_size, MAX_EVENT_CONFIG_SIZE);
        return -EFBIG;
    }

    config_buf = dbl_kzalloc(file_size + 1UL, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (config_buf == NULL) {
        dms_err("kzalloc config_buf failed. (size = %lu)\n", file_size);
        return -ENOMEM;
    }
    ret = read_file_to_buf(file_size, config_buf);
    if (ret != 0) {
        dms_err("read file to buf failed, (ret = %d)\n", ret);
        goto undo_acBuf_alloc;
    }
    config_buf[file_size] = '\n';

    ret = get_eventinfo_from_buf(config_buf, file_size + 1);
    if (ret != 0) {
        dms_err("get_event_info from buf failed! (ret=%d)\n", ret);
        goto undo_acBuf_alloc;
    }

    dbl_kfree(config_buf);
    config_buf = NULL;
    ka_base_sort(g_event_configs.event_configs, g_event_configs.config_cnt, sizeof(g_event_configs.event_configs[0]),
                 cmp, NULL);

    return 0;

undo_acBuf_alloc:
    dbl_kfree(config_buf);
    config_buf = NULL;
    return ret;
}
KA_EXPORT_SYMBOL(get_eventinfo_from_config);

STATIC int dms_release_prepare(ka_notifier_block_t *self, unsigned long val, void *data)
{
    ka_task_struct_t *task = (ka_task_struct_t *)data;
    (void)self;

    if ((task != NULL) && ka_task_judge_mm(task) && (task->tgid != 0)) {
        dms_event_release_proc(task->tgid, task->pid);
    }
    return 0;
}

static ka_notifier_block_t dms_exit_notifier = {
    .notifier_call = dms_release_prepare,
};

static struct sub_module_ops g_sub_table[] = {
    {dms_timer_init, dms_timer_uninit},
    {devdrv_manager_init, devdrv_manager_exit},
    {dms_product_init, dms_product_exit},
    {module_feature_auto_init, module_feature_auto_uninit},
};

STATIC int dms_init_submodule(void)
{
    int index, ret;
    int table_size = sizeof(g_sub_table) / sizeof(struct sub_module_ops);

    for (index = 0; index < table_size; index++) {
        ret = g_sub_table[index].init();
        if (ret != 0) {
            goto out;
        }
    }
    return 0;
out:
    for (; index > 0; index--) {
        g_sub_table[index - 1].uninit();
    }
    return ret;
}

STATIC void dms_exit_submodule(void)
{
    int index;
    int table_size = sizeof(g_sub_table) / sizeof(struct sub_module_ops);

    for (index = table_size; index > 0; index--) {
        g_sub_table[index - 1].uninit();
    }
}

int dms_init(void)
{
    int ret;

    dms_debug("dms_init start.\n");

    ret = ka_dfx_profile_event_register(KA_PROFILE_TASK_EXIT, &dms_exit_notifier);
    if (ret != 0) {
        dms_err("Register notify fail. (ret=%d)\n", ret);
        return ret;
    }

    ret = get_eventinfo_from_config();
    if (ret != 0) {
        dms_err("get_eventinfo_from_config failed. (ret=%d)\n", ret);
        goto register_notify_fail;
    }

    ret = dms_init_submodule();
    if (ret != 0) {
        dms_err("init sub module failed. (ret=%d)\n", ret);
        goto register_notify_fail;
    }
    dms_event_adapt_init();
#ifndef DEVDRV_MANAGER_HOST_UT_TEST
#ifdef CFG_FEATURE_GET_CURRENT_EVENTINFO
    ret = dms_remote_event_save_in_local_init();
    if (ret != 0) {
        dms_err("dms_remote_event_save_in_local_init failed. (ret=%d)\n", ret);
        dms_exit_submodule();
        goto register_notify_fail;
    }
#endif
#endif
    /* Initialize sensor global resources */
    CALL_INIT_MODULE(DMS_MODULE_BASIC_INFO);
    dms_sysfs_init();
    dms_info("Dms driver init success.\n");
    return 0;

register_notify_fail:
    (void)ka_dfx_profile_event_unregister(KA_PROFILE_TASK_EXIT, &dms_exit_notifier);
    return ret;
}

void dms_exit(void)
{
    dms_sysfs_uninit();
    dms_info("dms_exit start.\n");
    CALL_EXIT_MODULE(DMS_MODULE_BASIC_INFO);
#ifndef DEVDRV_MANAGER_HOST_UT_TEST
#ifdef CFG_FEATURE_GET_CURRENT_EVENTINFO
    dms_remote_event_save_in_local_exit();
#endif
#endif
    dms_event_adapt_exit();
    dms_exit_submodule();
    (void)drv_ascend_unregister_notify(DAVINCI_INTF_MODULE_URD);
    (void)ka_dfx_profile_event_unregister(KA_PROFILE_TASK_EXIT, &dms_exit_notifier);
    dms_info("Dms driver exit success.\n");
    return;
}
