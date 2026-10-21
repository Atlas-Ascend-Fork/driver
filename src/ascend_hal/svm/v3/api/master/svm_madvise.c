/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <pthread.h>

#include "ascend_hal.h"

#include "drv_user_common.h"

#include "svm_pub.h"
#include "svm_user_adapt.h"
#include "svm_log.h"
#include "svm_init_pri.h"
#include "svm_criu.h"
#include "svm_pagesize.h"
#include "svm_dbi.h"
#include "malloc_mng.h"
#include "svm_vmm.h"
#include "madvise.h"
#include "madvise_client.h"

struct svm_madvise_attr {
    u64 access_flag;
    u64 reserved[3];
};

struct svm_madvise_record {
    struct list_head node;
    u32 devid;
    u64 va;
    u64 size;
    struct svm_madvise_attr attr;
};

static struct list_head g_madvise_record_head = LIST_HEAD_INIT(g_madvise_record_head);
static pthread_rwlock_t g_madvise_record_rwlock = PTHREAD_RWLOCK_INITIALIZER;

static u32 svm_madvise_attr_get_access_flag(const struct svm_madvise_attr *attr)
{
    return (u32)attr->access_flag;
}

u32 svm_madvise_get_access(u32 devid, u64 va, u64 size)
{
    struct list_head *node = NULL;
    struct list_head *tmp = NULL;
    u32 access_flag = SVM_MADVISE_ACCESS_READ;

    /* HOST_RW_DEV_RO is read-only unless a READWRITE advise record exists. */
    (void)pthread_rwlock_rdlock(&g_madvise_record_rwlock);
    list_for_each_safe(node, tmp, &g_madvise_record_head)
    {
        struct svm_madvise_record *record = list_entry(node, struct svm_madvise_record, node);
        if ((record->devid == devid) && (record->va == va) && (record->size == size)) {
            access_flag = svm_madvise_attr_get_access_flag(&record->attr);
            break;
        }
    }
    (void)pthread_rwlock_unlock(&g_madvise_record_rwlock);
    return access_flag;
}

static bool svm_madvise_record_is_overlap(const struct svm_madvise_record *record, u32 devid, u64 va, u64 size)
{
    u64 record_end;
    u64 end;

    if ((record->devid != devid) || (record->size == 0) || (size == 0) || (record->va > (UINT64_MAX - record->size)) ||
        (va > (UINT64_MAX - size))) {
        return false;
    }

    record_end = record->va + record->size;
    end = va + size;
    return (va < record_end) && (record->va < end);
}

static int svm_madvise_record_add(u32 devid, u64 va, u64 size, const struct svm_madvise_attr *attr)
{
    struct svm_madvise_record *record = (struct svm_madvise_record *)svm_ua_calloc(1, sizeof(*record));

    (void)pthread_rwlock_wrlock(&g_madvise_record_rwlock);
    if (record == NULL) {
        (void)pthread_rwlock_unlock(&g_madvise_record_rwlock);
        svm_err("Calloc madvise record failed. (devid=%u; va=0x%llx; size=%llu)\n", devid, va, size);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    record->devid = devid;
    record->va = va;
    record->size = size;
    record->attr = *attr;
    drv_user_list_add_tail(&record->node, &g_madvise_record_head);
    (void)pthread_rwlock_unlock(&g_madvise_record_rwlock);

    return DRV_ERROR_NONE;
}

static void _svm_madvise_record_del_range(u32 devid, u64 va, u64 size)
{
    struct list_head *node = NULL;
    struct list_head *tmp = NULL;

    list_for_each_safe(node, tmp, &g_madvise_record_head)
    {
        struct svm_madvise_record *record = list_entry(node, struct svm_madvise_record, node);
        if (svm_madvise_record_is_overlap(record, devid, va, size)) {
            drv_user_list_del(&record->node);
            svm_ua_free(record);
        }
    }
}

static void _svm_madvise_recycle(u32 devid)
{
    struct list_head *node = NULL;
    struct list_head *tmp = NULL;

    list_for_each_safe(node, tmp, &g_madvise_record_head)
    {
        struct svm_madvise_record *record = list_entry(node, struct svm_madvise_record, node);
        if (record->devid == devid) {
            drv_user_list_del(&record->node);
            svm_ua_free(record);
        }
    }
}

static void svm_madvise_record_del_range(u32 devid, u64 va, u64 size)
{
    (void)pthread_rwlock_wrlock(&g_madvise_record_rwlock);
    _svm_madvise_record_del_range(devid, va, size);
    (void)pthread_rwlock_unlock(&g_madvise_record_rwlock);
}

void svm_madvise_recycle(u32 devid)
{
    (void)pthread_rwlock_wrlock(&g_madvise_record_rwlock);
    _svm_madvise_recycle(devid);
    (void)pthread_rwlock_unlock(&g_madvise_record_rwlock);
}

static void svm_madvise_attr_init(u32 type, struct svm_madvise_attr *attr)
{
    attr->access_flag |= (type == ADVISE_ACCESS_READONLY) ? SVM_MADVISE_ACCESS_READ : 0;
    attr->access_flag |= (type == ADVISE_ACCESS_READWRITE) ? (SVM_MADVISE_ACCESS_READ | SVM_MADVISE_ACCESS_WRITE) : 0;
}

static int mem_advise_check(u32 devid, u64 va, u64 size, u64 *aligned_size)
{
    struct svm_prop prop;
    int ret;

    ret = svm_get_prop(va, &prop);
    if (ret != 0) {
        svm_err("Get prop failed. (va=0x%llx)\n", va);
        return ret;
    }

    if (devid != prop.devid) {
        svm_err("Devid is invalid. (va=0x%llx; size=%llu; devid=%u; prop start=0x%llx; size=%llu; devid=%u)\n", va,
                size, devid, prop.start, prop.size, prop.devid);
        return DRV_ERROR_INVALID_DEVICE;
    }

    ret = svm_get_aligned_size(devid, prop.flag, size, aligned_size);
    if (ret != 0) {
        svm_err("Get aligned size fail. (va=0x%llx; size=%llu; flag=0x%llx)\n", va, size, prop.flag);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((va != prop.start) || !svm_is_page_align(devid, prop.flag, va) || (*aligned_size != prop.aligned_size)) {
        svm_err("Va or size is invalid. (va=0x%llx; size=%llu; aligned_size=%llu; prop start=0x%llx; "
                "aligned_size=%llu)\n",
                va, size, *aligned_size, prop.start, prop.aligned_size);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (!svm_flag_cap_is_support_madvise(prop.flag)) {
        svm_run_info("Addr cap does not support madvise. (va=0x%llx; flag=0x%llx)\n", va, prop.flag);
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (svm_get_host_devid() == prop.devid) {
        svm_run_info("Not support host va. (va=0x%llx; size=%llu; devid=%u)\n", va, size, prop.devid);
        return DRV_ERROR_NOT_SUPPORT;
    }
    return DRV_ERROR_NONE;
}

static int mem_advise(u32 devid, u64 va, u64 size, u32 type)
{
    struct svm_madvise_attr attr = {0};
    u64 aligned_size;
    int ret;

    ret = mem_advise_check(devid, va, size, &aligned_size);
    if (ret != 0) {
        return ret;
    }
    svm_madvise_attr_init(type, &attr);
    ret = svm_madvise_client(devid, va, aligned_size, svm_madvise_attr_get_access_flag(&attr));
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    /* Only HOST_RW_DEV_RO support advise access, which default access is READ_ONLY. */
    if (type == ADVISE_ACCESS_READWRITE) {
        svm_madvise_record_del_range(devid, va, aligned_size);
        return svm_madvise_record_add(devid, va, aligned_size, &attr);
    } else {
        svm_madvise_record_del_range(devid, va, aligned_size);
        return 0;
    }
}

static int snapshot_mem_mark_required(u32 devid, u64 va, u64 size, u32 type)
{
    struct svm_prop prop;
    void *va_handle = NULL;
    u64 offset;
    int ret;

    SVM_UNUSED(type);
    if (size == 0) {
        return DRV_ERROR_INVALID_VALUE;
    }

    va_handle = svm_handle_get(va);
    if (va_handle == NULL) {
        return DRV_ERROR_NOT_EXIST;
    }
    if (vmm_get_svmm(va_handle) != NULL) {
        svm_handle_put(va_handle);
        svm_run_info("Vmm memory does not support snapshot advise. (va=0x%llx)\n", va);
        return DRV_ERROR_NOT_SUPPORT;
    }

    ret = svm_get_prop(va, &prop);
    if (ret != DRV_ERROR_NONE) {
        svm_handle_put(va_handle);
        return ret;
    }

    if ((prop.devid != devid) || (va < prop.start)) {
        svm_handle_put(va_handle);
        return DRV_ERROR_INVALID_VALUE;
    }

    offset = va - prop.start;
    if ((offset > prop.aligned_size) || (size > (prop.aligned_size - offset))) {
        svm_handle_put(va_handle);
        return DRV_ERROR_INVALID_VALUE;
    }

    svm_mod_prop_flag(va_handle, (prop.flag & ~SVM_FLAG_SNAPSHOT_OPTIONAL) | SVM_FLAG_SNAPSHOT_REQUIRED);
    svm_handle_put(va_handle);
    return DRV_ERROR_NONE;
}

typedef int (*svm_advise_policy)(u32 devid, u64 va, u64 size, u32 type);
drvError_t halMemAdvise(DVdeviceptr ptr, size_t count, unsigned int type, DVdevice device)
{
    static const svm_advise_policy advise_policy[ADVISE_TYPE_MAX] = {
        [ADVISE_ACCESS_READONLY] = mem_advise,
        [ADVISE_ACCESS_READWRITE] = mem_advise,
        [ADVISE_SNAPSHOT_REQUIRED] = snapshot_mem_mark_required,
    };

    if ((type >= ADVISE_TYPE_MAX) || (advise_policy[type] == NULL)) {
        svm_run_info("Advise not support this type. (type=%u)\n", type);
        return DRV_ERROR_NOT_SUPPORT;
    }
    svm_debug("halMemAdvise. (ptr=0x%llx; count=%lu; type=%u; devid=%u)\n", ptr, count, type, device);
    return advise_policy[type](device, ptr, count, type);
}

static void svm_madvise_pre_free(void *handle, u64 start, struct svm_prop *prop)
{
    SVM_UNUSED(handle);

    if (svm_flag_cap_is_support_madvise(prop->flag)) {
        svm_madvise_record_del_range(prop->devid, start, prop->aligned_size);
    }
}

static struct svm_mng_ops g_madvise_mng_ops = {
    .pre_free = svm_madvise_pre_free,
};

static int svm_madvise_pre_unmap(u32 devid, u64 start, u64 svm_flag, struct svm_global_va *src_info)
{
    if (svm_flag_cap_is_support_madvise(svm_flag)) {
        svm_madvise_record_del_range(devid, start, src_info->size);
    }
    return DRV_ERROR_NONE;
}

static struct svm_vmm_ops g_madvise_vmm_ops = {
    .pre_unmap = svm_madvise_pre_unmap,
};

static int svm_madvise_criu_restore(u32 devid, void *data)
{
    struct list_head *node = NULL;
    struct list_head *tmp = NULL;
    int ret = DRV_ERROR_NONE;

    SVM_UNUSED(data);

    (void)pthread_rwlock_rdlock(&g_madvise_record_rwlock);
    list_for_each_safe(node, tmp, &g_madvise_record_head)
    {
        struct svm_madvise_record *record = list_entry(node, struct svm_madvise_record, node);
        if (devid != record->devid) {
            continue;
        }

        ret = svm_madvise_client(record->devid, record->va, record->size,
                                 svm_madvise_attr_get_access_flag(&record->attr));
        if (ret != DRV_ERROR_NONE) {
            svm_err("Restore madvise failed. (ret=%d; devid=%u; va=0x%llx; size=%llu; flag=0x%x)\n", ret, record->devid,
                    record->va, record->size, svm_madvise_attr_get_access_flag(&record->attr));
            break;
        }
    }
    (void)pthread_rwlock_unlock(&g_madvise_record_rwlock);

    return ret;
}

static const struct svm_criu_ops g_madvise_criu_ops = {
    .name = "madvise",
    .restore = svm_madvise_criu_restore,
};

static void __attribute__((constructor)) svm_madvise_init(void)
{
    int ret;

    svm_mng_set_ops(&g_madvise_mng_ops);
    svm_vmm_set_ops(&g_madvise_vmm_ops);

    ret = svm_criu_register_ops(&g_madvise_criu_ops);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Register CRIU ops failed.\n");
    }
}
