/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <stdint.h>
#include <stdlib.h>

#include "ascend_hal.h"

#include "svm_criu.h"
#include "svm_init_pri.h"
#include "svm_log.h"
#include "malloc_mng.h"
#include "madvise.h"
#include "madvise_client.h"

struct svm_mem_criu_node {
    uint64_t dev_va;
    uint64_t size;
    void *host_copy;
    struct svm_mem_criu_node *next;
    bool required;
};

struct svm_mem_criu_ctx {
    struct svm_mem_criu_node *head;
    uint8_t *host_copy;
};

struct svm_mem_criu_collect_ctx {
    u32 devid;
    bool include_optional;
};

static struct svm_mem_criu_ctx g_mem_criu_ctx[SVM_MAX_DEV_AGENT_NUM];

static void svm_mem_criu_cleanup_device(u32 devid)
{
    struct svm_mem_criu_node *node = NULL;
    struct svm_mem_criu_ctx *ctx = NULL;

    if (devid >= SVM_MAX_DEV_AGENT_NUM) {
        return;
    }

    ctx = &g_mem_criu_ctx[devid];
    node = ctx->head;
    free(ctx->host_copy);
    ctx->host_copy = NULL;
    while (node != NULL) {
        struct svm_mem_criu_node *next = node->next;
        free(node);
        node = next;
    }
    ctx->head = NULL;
}

static int svm_mem_criu_add(struct svm_mem_criu_ctx *ctx, uint64_t dev_va, uint64_t size, bool required)
{
    struct svm_mem_criu_node *node = NULL;

    if ((dev_va == 0) || (size == 0) || (size > (uint64_t)SIZE_MAX)) {
        svm_err("Invalid snapshot memory. (dev_va=0x%llx; size=%llu)\n", dev_va, size);
        return DRV_ERROR_INVALID_VALUE;
    }

    node = (struct svm_mem_criu_node *)calloc(1, sizeof(*node));
    if (node == NULL) {
        svm_err("Allocate snapshot node failed. (dev_va=0x%llx; size=%llu)\n", dev_va, size);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    node->dev_va = dev_va;
    node->size = size;
    node->required = required;
    node->next = ctx->head;
    ctx->head = node;
    return DRV_ERROR_NONE;
}

static int svm_mem_criu_backup_data(u32 devid)
{
    struct svm_mem_criu_ctx *ctx = &g_mem_criu_ctx[devid];
    struct svm_mem_criu_node *node = NULL;
    uint8_t *host_copy = NULL;
    size_t total_size = 0;
    int ret;

    for (node = ctx->head; node != NULL; node = node->next) {
        if (node->size > (uint64_t)(SIZE_MAX - total_size)) {
            svm_err("Snapshot host copy size overflow. (devid=%u; total_size=%zu; size=%llu)\n", devid, total_size,
                    node->size);
            return DRV_ERROR_OUT_OF_MEMORY;
        }
        total_size += (size_t)node->size;
    }

    if (total_size == 0) {
        return DRV_ERROR_NONE;
    }

    ctx->host_copy = (uint8_t *)malloc(total_size);
    if (ctx->host_copy == NULL) {
        svm_err("Allocate snapshot host copy failed. (devid=%u; total_size=%zu)\n", devid, total_size);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    host_copy = ctx->host_copy;
    for (node = ctx->head; node != NULL; node = node->next) {
        node->host_copy = host_copy;
        ret = drvMemcpy((DVdeviceptr)(uintptr_t)node->host_copy, (size_t)node->size, (DVdeviceptr)node->dev_va,
                        (size_t)node->size);
        if (ret != DRV_ERROR_NONE) {
            svm_err("Backup device memory failed. (ret=%d; devid=%u; dev_va=0x%llx; size=%llu)\n", ret, devid,
                    node->dev_va, node->size);
            return ret;
        }
        host_copy += (size_t)node->size;
    }
    return DRV_ERROR_NONE;
}

static int svm_mem_criu_restore_data(u32 devid, bool include_optional)
{
    struct svm_mem_criu_ctx *ctx = &g_mem_criu_ctx[devid];
    struct svm_mem_criu_node *node = NULL;
    struct svm_prop prop;
    u32 access_flag = SVM_MADVISE_ACCESS_READ;
    bool readonly;
    int ret, access_ret;

    for (node = ctx->head; node != NULL; node = node->next) {
        if (!include_optional && !node->required) {
            continue;
        }
        if (node->host_copy == NULL) {
            svm_err("Snapshot host copy is null. (devid=%u; dev_va=0x%llx; size=%llu)\n", devid, node->dev_va,
                    node->size);
            return DRV_ERROR_OPER_NOT_PERMITTED;
        }

        ret = svm_get_prop(node->dev_va, &prop);
        if (ret != DRV_ERROR_NONE) {
            svm_err("Get snapshot memory prop failed. (ret=%d; devid=%u; dev_va=0x%llx)\n", ret, devid, node->dev_va);
            return ret;
        }
        readonly = svm_flag_attr_is_pg_rdonly(prop.flag) && svm_flag_cap_is_support_madvise(prop.flag);
        if (readonly) {
            access_flag = svm_madvise_get_access(devid, node->dev_va, prop.aligned_size);
            /* Temporary access changes must not modify the saved advise records. */
            ret = svm_madvise_client(devid, node->dev_va, prop.aligned_size,
                                     SVM_MADVISE_ACCESS_READ | SVM_MADVISE_ACCESS_WRITE);
        }
        if (ret == DRV_ERROR_NONE) {
            ret = drvMemcpy((DVdeviceptr)node->dev_va, (size_t)node->size, (DVdeviceptr)(uintptr_t)node->host_copy,
                            (size_t)node->size);
        }
        if (readonly) {
            /* Restore the original access even when opening write access or copying failed. */
            access_ret = svm_madvise_client(devid, node->dev_va, prop.aligned_size, access_flag);
            if (access_ret != DRV_ERROR_NONE) {
                svm_err("Restore snapshot memory access failed. (ret=%d; devid=%u; dev_va=0x%llx; access=0x%x)\n",
                        access_ret, devid, node->dev_va, access_flag);
            }
            ret = (ret != DRV_ERROR_NONE) ? ret : access_ret;
        }
        if (ret != DRV_ERROR_NONE) {
            svm_err("Restore device memory failed. (ret=%d; devid=%u; dev_va=0x%llx; size=%llu)\n", ret, devid,
                    node->dev_va, node->size);
            return ret;
        }
    }
    return DRV_ERROR_NONE;
}

static int svm_mem_criu_collect_one(void *handle, u64 start, struct svm_prop *prop, void *priv)
{
    struct svm_mem_criu_collect_ctx *collect = (struct svm_mem_criu_collect_ctx *)priv;
    bool required = ((prop->flag & SVM_FLAG_SNAPSHOT_REQUIRED) != 0);
    bool optional = ((prop->flag & SVM_FLAG_SNAPSHOT_OPTIONAL) != 0);

    SVM_UNUSED(handle);
    if ((prop->devid != collect->devid) || (prop->size == 0)) {
        return DRV_ERROR_NONE;
    }
    if (!required && !(collect->include_optional && optional)) {
        return DRV_ERROR_NONE;
    }
    return svm_mem_criu_add(&g_mem_criu_ctx[collect->devid], start, prop->size, required);
}

static int svm_mem_criu_backup(u32 devid, void *data)
{
    const halProcResBackupInfo *info = (const halProcResBackupInfo *)data;
    struct svm_mem_criu_collect_ctx collect = {
        .devid = devid,
        .include_optional = (info != NULL) && ((info->reserve[0] & HAL_PROC_RES_BACKUP_INCLUDE_OPTIONAL) != 0),
    };
    int ret;

    if (devid >= SVM_MAX_DEV_AGENT_NUM) {
        return DRV_ERROR_NONE;
    }

    svm_mem_criu_cleanup_device(devid);
    ret = svm_for_each_valid_handle(svm_mem_criu_collect_one, &collect);
    if (ret == DRV_ERROR_NONE) {
        ret = svm_mem_criu_backup_data(devid);
    }
    if (ret != DRV_ERROR_NONE) {
        svm_mem_criu_cleanup_device(devid);
    }
    return ret;
}

static int svm_mem_criu_restore(u32 devid, void *data)
{
    const halProcResRestoreInfo *info = (const halProcResRestoreInfo *)data;
    bool include_optional = (info != NULL) && ((info->reserve[0] & HAL_PROC_RES_RESTORE_INCLUDE_OPTIONAL) != 0);
    int ret;

    if (devid >= SVM_MAX_DEV_AGENT_NUM) {
        return DRV_ERROR_NONE;
    }

    ret = svm_mem_criu_restore_data(devid, include_optional);
    if (ret == DRV_ERROR_NONE) {
        svm_mem_criu_cleanup_device(devid);
    }
    return ret;
}

static const struct svm_criu_ops g_mem_criu_ops = {
    .name = "mem_criu",
    .backup = svm_mem_criu_backup,
    .restore = svm_mem_criu_restore,
};

static void __attribute__((constructor(SVM_INIT_PRI_FINAL))) svm_mem_criu_init(void)
{
    int ret = svm_criu_register_ops(&g_mem_criu_ops);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Register memory snapshot CRIU ops failed. (ret=%d)\n", ret);
    }
}
