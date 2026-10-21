/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "bbox_dump_lib.h"
#include "bbox_dump_exception.h"
#include "bbox_int.h"
#include "bbox_print.h"
#include "bbox_log_common.h"
#include "bbox_tool_log.h"
#include "bbox_tool_fs.h"
#include "bbox_system_api.h"
#include "bbox_feature.h"
#include "securec.h"

struct thread_args {
    s32 dev_id;
    struct BboxDumpOpt opt;
};

struct thread_info {
    bbox_thread_t tid;
    bbox_thread_block block;
};

STATIC struct thread_info g_bbox_dump_thread;
STATIC bbox_lock_t g_bbox_dump_lock = MM_MUTEX_INITIALIZER;

STATIC void *bbox_dump_handle(void *args)
{
    UNUSED(args);
    BBOX_INF("start bbox dump thread.");
    struct thread_args *thread_args = (struct thread_args *)args;
    enum BBOX_DUMP_MODE mode = BBOX_DUMP_MODE_NONE;
    char root_path[DIR_MAXLEN] = ROOT_DIR;
    mode = ((thread_args->opt.all == true) ? BBOX_DUMP_MODE_ALL : mode);
    mode = ((thread_args->opt.force == true) ? BBOX_DUMP_MODE_FORCE : mode);
    mode = ((thread_args->opt.vmcore == true) ? BBOX_DUMP_MODE_VMCORE : mode);
    mode = ((thread_args->opt.heart_beat_lost == true) ? BBOX_DUMP_MODE_HBL : mode);
    bbox_status ret = bbox_dump_exception(thread_args->dev_id, root_path, (u32)strlen(root_path), mode);
    if (ret != BBOX_SUCCESS) {
        BBOX_ERR("dump exception failed.");
    }
    bbox_free(thread_args);

    return NULL;
}

bbox_status BboxStartDump(s32 dev_id, const char *path, s32 p_size, const struct BboxDumpOpt *opt)
{
    struct thread_args *thread_args = NULL;

    BBOX_CHK_INVALID_PARAM((dev_id < BBOX_INVALID_DEVICE_ID) || (dev_id >= MAX_PHY_DEV_NUM), return BBOX_FAILURE,
                           "invalid dev_id. (dev_id=%d)", dev_id);
    BBOX_CHK_NULL_PTR(path, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(opt, return BBOX_FAILURE);
    BBOX_CHK_INVALID_PARAM(p_size <= 0, return BBOX_FAILURE, "%d", p_size);

    if (bbox_feature_init() != DRV_ERROR_NONE) {
        return BBOX_FAILURE;
    }

    if (strstr(path, "..") != NULL) {
        BBOX_ERR("Invalid path.");
        return BBOX_FAILURE;
    }

    if (bbox_lock_get(&g_bbox_dump_lock) != BBOX_SUCCESS) {
        BBOX_ERR("Failed to get bbox dump lock.");
        return BBOX_FAILURE;
    }

    bbox_dump_log_set_log_config(opt->print_mode, opt->log_level);
    bbox_status ret = bbox_chdir(path);
    BBOX_CHK_EXPR_CTRL(BBOX_PERROR, ret != BBOX_SUCCESS, goto dump_exit, "chdir", path);
    BBOX_INF("dir changed to: %s", path);

    ret = bbox_mkdir_recur(ROOT_DIR);
    BBOX_CHK_EXPR_ACTION(ret != BBOX_SUCCESS, goto dump_exit, "add sub-dir %s failed in %s", ROOT_DIR, path);

    if (g_bbox_dump_thread.tid == 0) {
        thread_args = (struct thread_args *)bbox_malloc(sizeof(struct thread_args));
        if (thread_args == NULL) {
            BBOX_ERR("Failed to malloc thread args.");
            goto dump_exit;
        }
        thread_args->dev_id = dev_id;
        s32 err = memcpy_s(&thread_args->opt, sizeof(struct BboxDumpOpt), opt, sizeof(struct BboxDumpOpt));
        BBOX_CHK_EXPR_CTRL(BBOX_ERR, err != EOK, goto dump_exit, "Failed to memcpy_s opt data.");
        bbox_thread_set_block(&g_bbox_dump_thread.block, bbox_dump_handle, thread_args);
        ret = bbox_thread_create(&g_bbox_dump_thread.tid, &g_bbox_dump_thread.block);
        if (ret != BBOX_SUCCESS) {
            g_bbox_dump_thread.tid = 0;
            BBOX_ERR_CTRL(THREAD_PERROR, goto dump_exit, "thread_create", "bbox_dump_handle", ret);
        }
        bbox_thread_set_name(&g_bbox_dump_thread.tid, BBOX_THREAD_HOST_DUMP);
    }
    (void)bbox_lock_release(&g_bbox_dump_lock);
    return BBOX_SUCCESS;

dump_exit:
    if (thread_args != NULL) {
        bbox_free(thread_args);
    }
    (void)bbox_lock_release(&g_bbox_dump_lock);
    return BBOX_FAILURE;
}

void BboxStopDump(void)
{
    if (bbox_lock_get(&g_bbox_dump_lock) != BBOX_SUCCESS) {
        BBOX_ERR("Failed to get bbox dump lock.");
        return;
    }

    if (g_bbox_dump_thread.tid != 0) {
        int ret = bbox_thread_join(&g_bbox_dump_thread.tid);
        if (ret != BBOX_SUCCESS) {
            BBOX_ERR_CTRL(THREAD_PERROR, (void)bbox_lock_release(&g_bbox_dump_lock), "thread_join", "bbox_dump_handle",
                          ret);
            return;
        }
        g_bbox_dump_thread.tid = 0;
    }
    (void)bbox_lock_release(&g_bbox_dump_lock);
    BBOX_INF("bbox dump thread stopped.");
}
