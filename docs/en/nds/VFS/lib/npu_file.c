/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

/*
 * NPU Direct Storage (NDS) user-space client library.
 *
 * Depends only on libc and the /dev/npu-nds ioctl contract.
 * No HAL, TSD, or other driver-user-space symbols are used.
 */

#define __STDC_WANT_LIB_EXT1__ 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "npu_file.h"

#define NPU_FILE_LOG(fmt, ...) fprintf(stderr, "[npu_file] " fmt, ##__VA_ARGS__)

int npu_file_open(npu_file_ctx_t *ctx)
{
    if (!ctx)
        return -EINVAL;

    (void)memset_s(ctx, sizeof(*ctx), 0, sizeof(*ctx));

    ctx->nds_fd = open(NDS_DEV_PATH, O_RDWR);
    if (ctx->nds_fd < 0) {
        char errbuf[64];
        (void)strerror_r(errno, errbuf, sizeof(errbuf));
        NPU_FILE_LOG("open %s failed: %s (is npu_nds.ko loaded?)\n", NDS_DEV_PATH, errbuf);
        return -errno;
    }

    ctx->initialized = 1;
    NPU_FILE_LOG("opened %s (fd=%d)\n", NDS_DEV_PATH, ctx->nds_fd);
    return 0;
}

void npu_file_close(npu_file_ctx_t *ctx)
{
    if (!ctx || !ctx->initialized)
        return;

    if (ctx->nds_fd >= 0)
        close(ctx->nds_fd);

    NPU_FILE_LOG("closed\n");
    (void)memset_s(ctx, sizeof(*ctx), 0, sizeof(*ctx));
}

int npu_file_buf_register(npu_file_ctx_t *ctx, uint64_t npu_va, uint64_t size, npu_file_handle_t *handle)
{
    struct npu_nds_map_args a;
    int ret;

    if (!ctx || !ctx->initialized || !handle)
        return -EINVAL;

    (void)memset_s(handle, sizeof(*handle), 0, sizeof(*handle));

    (void)memset_s(&a, sizeof(a), 0, sizeof(a));
    a.npu_va = npu_va;
    a.size = size;

    ret = ioctl(ctx->nds_fd, NDS_IOCTL_MAP, &a);
    if (ret) {
        char errbuf[64];
        (void)strerror_r(errno, errbuf, sizeof(errbuf));
        NPU_FILE_LOG("NDS_IOCTL_MAP failed: %d (%s)\n", ret, errbuf);
        return ret;
    }

    handle->mgroup_id = a.mgroup_id;
    handle->shadow_size = a.mmap_size;
    handle->buf_size = size;
    handle->npu_page_size = a.npu_page_size;
    handle->npu_page_num = a.npu_page_num;

    handle->shadow_va = mmap(NULL, a.mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, ctx->nds_fd, a.mmap_offset);
    if (handle->shadow_va == MAP_FAILED) {
        char errbuf[64];
        (void)strerror_r(errno, errbuf, sizeof(errbuf));
        NPU_FILE_LOG("mmap shadow failed: %s\n", errbuf);
        handle->shadow_va = NULL;
        {
            struct npu_nds_unmap_args ua = {.mgroup_id = a.mgroup_id};
            (void)ioctl(ctx->nds_fd, NDS_IOCTL_UNMAP, &ua);
        }
        return -errno;
    }

    handle->registered = 1;
    NPU_FILE_LOG("buf_register: mgid=%llu shadow=%p size=%llu npu_pg=%u/%u\n", (unsigned long long)handle->mgroup_id,
                 handle->shadow_va, (unsigned long long)handle->shadow_size, handle->npu_page_num,
                 handle->npu_page_size);
    return 0;
}

int npu_file_buf_deregister(npu_file_ctx_t *ctx, npu_file_handle_t *handle)
{
    struct npu_nds_unmap_args ua;
    int ret;

    if (!ctx || !ctx->initialized || !handle || !handle->registered)
        return -EINVAL;

    if (handle->shadow_va) {
        munmap(handle->shadow_va, handle->shadow_size);
        handle->shadow_va = NULL;
    }

    (void)memset_s(&ua, sizeof(ua), 0, sizeof(ua));
    ua.mgroup_id = handle->mgroup_id;
    ret = ioctl(ctx->nds_fd, NDS_IOCTL_UNMAP, &ua);
    if (ret) {
        char errbuf[64];
        (void)strerror_r(errno, errbuf, sizeof(errbuf));
        NPU_FILE_LOG("NDS_IOCTL_UNMAP failed: %d (%s)\n", ret, errbuf);
    } else {
        NPU_FILE_LOG("buf_deregister: mgid=%llu released\n", (unsigned long long)handle->mgroup_id);
    }

    handle->registered = 0;
    return ret;
}

ssize_t npu_file_read(npu_file_ctx_t *ctx, npu_file_handle_t *handle, int fd, uint64_t file_offset, uint64_t size,
                      uint64_t npu_offset)
{
    struct npu_nds_io_args a;
    int ret;

    (void)npu_offset;

    if (!ctx || !ctx->initialized || !handle || !handle->registered)
        return -EINVAL;

    (void)memset_s(&a, sizeof(a), 0, sizeof(a));
    a.opcode = NDS_OPCODE_READ;
    a.mgroup_id = handle->mgroup_id;
    a.fd = (uint32_t)fd;
    a.file_offset = file_offset;
    a.size = size;

    ret = ioctl(ctx->nds_fd, NDS_IOCTL_READ, &a);
    if (ret) {
        char errbuf[64];
        (void)strerror_r(errno, errbuf, sizeof(errbuf));
        NPU_FILE_LOG("NDS_IOCTL_READ failed: %d (%s)\n", ret, errbuf);
        return ret;
    }
    if (a.ret) {
        NPU_FILE_LOG("NDS_IOCTL_READ kernel ret=%d\n", a.ret);
        return a.ret;
    }
    NPU_FILE_LOG("read: done=%llu bytes\n", (unsigned long long)a.done);
    return (ssize_t)a.done;
}

ssize_t npu_file_write(npu_file_ctx_t *ctx, npu_file_handle_t *handle, int fd, uint64_t file_offset, uint64_t size,
                       uint64_t npu_offset)
{
    struct npu_nds_io_args a;
    int ret;

    (void)npu_offset;

    if (!ctx || !ctx->initialized || !handle || !handle->registered)
        return -EINVAL;

    (void)memset_s(&a, sizeof(a), 0, sizeof(a));
    a.opcode = NDS_OPCODE_WRITE;
    a.mgroup_id = handle->mgroup_id;
    a.fd = (uint32_t)fd;
    a.file_offset = file_offset;
    a.size = size;

    ret = ioctl(ctx->nds_fd, NDS_IOCTL_WRITE, &a);
    if (ret) {
        char errbuf[64];
        (void)strerror_r(errno, errbuf, sizeof(errbuf));
        NPU_FILE_LOG("NDS_IOCTL_WRITE failed: %d (%s)\n", ret, errbuf);
        return ret;
    }
    if (a.ret) {
        NPU_FILE_LOG("NDS_IOCTL_WRITE kernel ret=%d\n", a.ret);
        return a.ret;
    }
    NPU_FILE_LOG("write: done=%llu bytes\n", (unsigned long long)a.done);
    return (ssize_t)a.done;
}

void *npu_file_shadow_addr(const npu_file_handle_t *handle)
{
    if (!handle || !handle->registered)
        return NULL;
    return handle->shadow_va;
}
