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
 * Depends only on libc, the standard Linux UAPI headers and the
 * /dev/npu-nds ioctl contract.  No HAL, TSD, or other driver-user-space
 * symbols are used.
 *
 * Zero-copy is achieved by handing FS_IOC_FIEMAP extents + the NPU
 * mgroup id to the kernel; the kernel builds RQF_NVME_PT requests whose
 * SGL points straight at the DMA-mapped NPU HBM pages.
 */

#define __STDC_WANT_LIB_EXT1__ 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <linux/fiemap.h>

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
    handle->shadow_va = NULL;
    handle->shadow_size = 0;
    handle->buf_size = size;
    handle->npu_page_size = a.npu_page_size;
    handle->npu_page_num = a.npu_page_num;
    handle->registered = 1;

    NPU_FILE_LOG("buf_register: mgid=%llu size=%llu npu_pg=%u/%u\n", (unsigned long long)handle->mgroup_id,
                 (unsigned long long)handle->buf_size, handle->npu_page_num, handle->npu_page_size);
    return 0;
}

int npu_file_buf_deregister(npu_file_ctx_t *ctx, npu_file_handle_t *handle)
{
    struct npu_nds_unmap_args ua;
    int ret;

    if (!ctx || !ctx->initialized || !handle || !handle->registered)
        return -EINVAL;

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

static int fiemap_process_batch(struct fiemap *fm, uint64_t file_offset, uint64_t size, uint64_t *cursor,
                                uint64_t *covered, struct npu_nds_fiemap_extent *extents, uint32_t cap,
                                uint32_t *out_cnt)
{
    uint32_t i;
    struct fiemap_extent *fe;

    if (fm->fm_mapped_extents == 0) {
        NPU_FILE_LOG("FIEMAP returned 0 extents at off=0x%llx\n", (unsigned long long)*cursor);
        return 1;
    }

    for (i = 0; i < fm->fm_mapped_extents; i++) {
        uint64_t ext_start, ext_end, len, phys;
        uint64_t clipped_start, clipped_end, clipped_len;

        fe = &fm->fm_extents[i];
        ext_start = fe->fe_logical;
        ext_end = fe->fe_logical + fe->fe_length;
        phys = fe->fe_physical;

        if (ext_start >= file_offset + size)
            return 1;

        clipped_start = ext_start < file_offset ? file_offset : ext_start;
        clipped_end = ext_end > file_offset + size ? file_offset + size : ext_end;
        clipped_len = clipped_end - clipped_start;
        if (clipped_len == 0)
            continue;

        if (*out_cnt >= cap) {
            NPU_FILE_LOG("FIEMAP cap=%u too small (need >= %u)\n", cap, *out_cnt + 1);
            return -ENOSPC;
        }

        extents[*out_cnt].logical = clipped_start;
        extents[*out_cnt].physical = phys + (clipped_start - ext_start);
        extents[*out_cnt].length = clipped_len;
        extents[*out_cnt].npu_offset = clipped_start - file_offset;
        extents[*out_cnt].flags = fe->fe_flags;
        extents[*out_cnt].reserved = 0;

        (*out_cnt)++;
        *covered += clipped_len;
        *cursor = clipped_end;

        if (fe->fe_flags & FIEMAP_EXTENT_LAST)
            return 1;
    }

    return 0;
}

/*
 * Walk FIEMAP extents covering [file_offset, file_offset + size) and emit
 * npu_nds_fiemap_extent entries (one per underlying fiemap_extent, split
 * across the requested range boundaries as needed).  Each entry's
 * npu_offset is computed relative to @p file_offset so that the caller
 * can place the data at any base offset inside a registered NPU buffer.
 *
 * FIEMAP can return FE_LAST per extent; we keep fetching until we have
 * covered the requested range or hit the end of the file.
 */
int npu_file_fiemap_query(int file_fd, uint64_t file_offset, uint64_t size, struct npu_nds_fiemap_extent *extents,
                          uint32_t cap, uint32_t *total)
{
    uint8_t *buf = NULL;
    struct fiemap *fm;
    uint64_t cursor, covered = 0;
    uint32_t out_cnt = 0, batch_cap, fm_buf_sz;
    int ret = 0;

    if (file_fd < 0 || !extents || cap == 0 || !total)
        return -EINVAL;

    batch_cap = cap < 64 ? cap : 64;
    fm_buf_sz = sizeof(struct fiemap) + batch_cap * sizeof(struct fiemap_extent);
    buf = calloc(1, fm_buf_sz);
    if (!buf)
        return -ENOMEM;

    cursor = file_offset;
    *total = 0;

    while (covered < size) {
        (void)memset_s(buf, fm_buf_sz, 0, fm_buf_sz);
        fm = (struct fiemap *)buf;
        fm->fm_start = cursor;
        fm->fm_length = size - covered;
        fm->fm_extent_count = batch_cap;

        if (ioctl(file_fd, FS_IOC_FIEMAP, fm)) {
            char errbuf[64];
            (void)strerror_r(errno, errbuf, sizeof(errbuf));
            ret = -errno;
            NPU_FILE_LOG("FS_IOC_FIEMAP failed: %s\n", errbuf);
            goto out;
        }

        ret = fiemap_process_batch(fm, file_offset, size, &cursor, &covered, extents, cap, &out_cnt);
        if (ret < 0)
            goto out;
        if (ret > 0)
            break;
    }

    ret = 0;
    *total = out_cnt;
    if (covered < size)
        NPU_FILE_LOG("FIEMAP covered %llu of %llu bytes (file shorter?)\n", (unsigned long long)covered,
                     (unsigned long long)size);
    NPU_FILE_LOG("FIEMAP: %u extents covering %llu bytes\n", out_cnt, (unsigned long long)covered);

out:
    free(buf);
    return ret;
}

/*
 * Helper: query FIEMAP for the requested range, then submit the kernel
 * ioctl with the extents array pinned on the caller's stack/heap.
 * Used by both npu_file_read and npu_file_write.
 */
static void npu_file_build_io_args(struct npu_nds_io_args *a, npu_file_handle_t *handle, int fd, uint64_t file_offset,
                                   uint64_t size, uint64_t npu_offset, uint32_t opcode, uint32_t extent_cnt,
                                   struct npu_nds_fiemap_extent *extents)
{
    (void)memset_s(a, sizeof(*a), 0, sizeof(*a));
    a->opcode = opcode;
    a->mgroup_id = handle->mgroup_id;
    a->fd = (uint32_t)fd;
    a->file_offset = file_offset;
    a->size = size;
    a->npu_offset = npu_offset;
    a->extent_cnt = extent_cnt;
    a->extents_ptr = (uint64_t)(uintptr_t)extents;
}

static ssize_t npu_file_exec_ioctl(npu_file_ctx_t *ctx, npu_file_handle_t *handle, int fd, uint64_t file_offset,
                                   uint64_t size, uint64_t npu_offset, uint32_t opcode,
                                   struct npu_nds_fiemap_extent *extents, uint32_t total)
{
    struct npu_nds_io_args a;
    int ret;

    npu_file_build_io_args(&a, handle, fd, file_offset, size, npu_offset, opcode, total, extents);

    ret = ioctl(ctx->nds_fd, opcode == NDS_OPCODE_READ ? NDS_IOCTL_READ : NDS_IOCTL_WRITE, &a);
    if (ret) {
        char errbuf[64];
        (void)strerror_r(errno, errbuf, sizeof(errbuf));
        NPU_FILE_LOG("NDS_IOCTL_%s failed: %d (%s)\n", opcode == NDS_OPCODE_READ ? "READ" : "WRITE", ret, errbuf);
        return ret;
    }
    if (a.ret) {
        NPU_FILE_LOG("NDS_IOCTL_%s kernel ret=%d\n", opcode == NDS_OPCODE_READ ? "READ" : "WRITE", a.ret);
        return a.ret;
    }
    NPU_FILE_LOG("%s: done=%llu bytes\n", opcode == NDS_OPCODE_READ ? "read" : "write", (unsigned long long)a.done);
    return (ssize_t)a.done;
}

static ssize_t npu_file_io_submit(npu_file_ctx_t *ctx, npu_file_handle_t *handle, int fd, uint64_t file_offset,
                                  uint64_t size, uint64_t npu_offset, uint32_t opcode)
{
    /* Heap-allocated: NDS_MAX_EXTENTS * sizeof(npu_nds_fiemap_extent) ~= 2.5K,
     * exceeds the 2K user-space stack budget from the coding standard. */
    struct npu_nds_fiemap_extent *extents = NULL;
    uint32_t total = 0;
    ssize_t ret;

    if (!ctx || !ctx->initialized || !handle || !handle->registered)
        return -EINVAL;

    extents = calloc(NDS_MAX_EXTENTS, sizeof(*extents));
    if (!extents)
        return -ENOMEM;

    ret = npu_file_fiemap_query(fd, file_offset, size, extents, NDS_MAX_EXTENTS, &total);
    if (ret) {
        NPU_FILE_LOG("fiemap_query failed: %d\n", ret);
        goto out;
    }
    if (total == 0) {
        NPU_FILE_LOG("no extents for [off=0x%llx len=%llu)\n", (unsigned long long)file_offset,
                     (unsigned long long)size);
        ret = -ENXIO;
        goto out;
    }

    if (npu_offset) {
        uint32_t i;
        for (i = 0; i < total; i++)
            extents[i].npu_offset += npu_offset;
    }

    ret = npu_file_exec_ioctl(ctx, handle, fd, file_offset, size, npu_offset, opcode, extents, total);
out:
    free(extents);
    return ret;
}

ssize_t npu_file_read(npu_file_ctx_t *ctx, npu_file_handle_t *handle, int fd, uint64_t file_offset, uint64_t size,
                      uint64_t npu_offset)
{
    return npu_file_io_submit(ctx, handle, fd, file_offset, size, npu_offset, NDS_OPCODE_READ);
}

ssize_t npu_file_write(npu_file_ctx_t *ctx, npu_file_handle_t *handle, int fd, uint64_t file_offset, uint64_t size,
                       uint64_t npu_offset)
{
    return npu_file_io_submit(ctx, handle, fd, file_offset, size, npu_offset, NDS_OPCODE_WRITE);
}
