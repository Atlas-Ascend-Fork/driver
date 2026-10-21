/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

/**
 * @file npu_file.h
 * @brief NPU Direct Storage (NDS) user-space client library.
 *
 * Thin wrapper around the /dev/npu-nds ioctl contract.  No shadow buffer:
 * the kernel module submits NVMe passthrough requests (RQF_NVME_PT) whose
 * SGL points straight at the DMA-mapped NPU HBM pages.  This library is
 * responsible for querying the file's on-disk layout (FS_IOC_FIEMAP) and
 * handing the resulting extents to the kernel along with the NPU buffer
 * handle.
 *
 * Depends only on libc + the NDS kernel interface and the standard Linux
 * UAPI headers (<linux/fiemap.h>, <linux/fs.h>).  No HAL or TSD symbols.
 *
 * The application is responsible for NPU device-context management
 * (drvMemDeviceOpen / TsdOpen) and NPU memory allocation (halMemAlloc)
 * before calling npu_file_buf_register.
 *
 * API mirrors cuFile usage:
 *   cuFileDriverOpen        -> npu_file_open
 *   cuFileBufRegister       -> npu_file_buf_register
 *   cuFileRead              -> npu_file_read
 *   cuFileWrite             -> npu_file_write
 *   cuFileBufDeregister     -> npu_file_buf_deregister
 *   cuFileDriverClose       -> npu_file_close
 */

#ifndef NPU_FILE_H
#define NPU_FILE_H

#include <stdint.h>
#include <sys/types.h>
#include "npu_nds.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * NDS context (one per process).  Holds only the /dev/npu-nds fd.
 */
typedef struct npu_file_ctx {
    int nds_fd;
    int initialized;
} npu_file_ctx_t;

/**
 * Handle for a registered NPU buffer (returned by npu_file_buf_register).
 * Mirrors cufile_bufhandle_t.  The kernel pins the NPU HBM pages and
 * records the mgroup id; no shadow buffer is allocated in the new design.
 */
typedef struct npu_file_handle {
    uint64_t mgroup_id;     /* kernel mgroup id              */
    void *shadow_va;        /* NULL (NVMe path has no shadow buffer) */
    uint64_t shadow_size;   /* 0   (NVMe path has no shadow buffer) */
    uint64_t buf_size;      /* registered buffer size (bytes) */
    uint32_t npu_page_size; /* NPU physical page size        */
    uint32_t npu_page_num;  /* number of NPU physical pages  */
    int registered;
} npu_file_handle_t;

/**
 * @brief Open the NDS driver (/dev/npu-nds).
 *
 * Must be called before any other API.  The caller is responsible for
 * NPU device-context management (drvMemDeviceOpen / TsdOpen) separately.
 *
 * @param [out] ctx  Pointer to a caller-allocated context struct.
 * @return 0 on success, negative errno on failure.
 */
int npu_file_open(npu_file_ctx_t *ctx);

/**
 * @brief Close the NDS driver.
 *
 * All buffers must be deregistered first.
 *
 * @param [in] ctx  Context previously initialised by npu_file_open.
 */
void npu_file_close(npu_file_ctx_t *ctx);

/**
 * @brief Pin NPU device memory and create the kernel mgroup (cuFileBufRegister).
 *
 * No shadow buffer is allocated — zero-copy is achieved through NVMe
 * passthrough in the kernel.
 *
 * @param [in]  ctx     NDS context.
 * @param [in]  npu_va  NPU virtual address (page-aligned), from halMemAlloc.
 * @param [in]  size    size in bytes (page-aligned).
 * @param [out] handle  Caller-allocated handle, filled on success.
 * @return 0 on success, negative errno on failure.
 */
int npu_file_buf_register(npu_file_ctx_t *ctx, uint64_t npu_va, uint64_t size, npu_file_handle_t *handle);

/**
 * @brief Unpin NPU memory and release the mgroup (cuFileBufDeregister).
 *
 * Safe to call on a handle whose register failed; the function is a no-op
 * if @p handle is not registered.
 *
 * @param [in] ctx     NDS context.
 * @param [in] handle  Handle previously filled by npu_file_buf_register.
 * @return 0 on success, negative errno on failure.
 */
int npu_file_buf_deregister(npu_file_ctx_t *ctx, npu_file_handle_t *handle);

/**
 * @brief Query the file's on-disk layout via FS_IOC_FIEMAP.
 *
 * Walks FIEMAP extents covering [@p file_offset, @p file_offset + @p size)
 * and writes them into @p extents (at most @p cap entries).  Each extent's
 * `npu_offset` is filled in relative to @p file_offset, so that the
 * caller (or npu_file_read/npu_file_write) can place the data at the
 * right offset inside a registered NPU buffer.
 *
 * @p total receives the real number of extents covering the range; if it
 * exceeds @p cap, the call fails with -ENOSPC.
 *
 * @param [in]  file_fd     File descriptor opened with O_DIRECT.
 * @param [in]  file_offset Byte offset inside the file to start querying.
 * @param [in]  size        Length in bytes of the range to map.
 * @param [out] extents     Caller-allocated array (cap entries), filled on
 *                          success.
 * @param [in]  cap         Capacity of @p extents (use NDS_MAX_EXTENTS).
 * @param [out] total       Real number of extents covering the range.
 * @return 0 on success, negative errno on failure.
 */
int npu_file_fiemap_query(int file_fd, uint64_t file_offset, uint64_t size, struct npu_nds_fiemap_extent *extents,
                          uint32_t cap, uint32_t *total);

/**
 * @brief Submit a zero-copy read (file -> NPU HBM) via NVMe passthrough.
 *
 * Internally queries FS_IOC_FIEMAP for the file range and hands the extents
 * to the kernel along with the registered NPU buffer handle.  The kernel
 * builds RQF_NVME_PT requests whose SGL points at the DMA-mapped NPU HBM
 * pages.
 *
 * @param [in] ctx          NDS context.
 * @param [in] handle       Registered buffer handle.
 * @param [in] fd           File descriptor opened with O_DIRECT.
 * @param [in] file_offset  Offset in the file to read from.
 * @param [in] size         Number of bytes to read.
 * @param [in] npu_offset   Offset within the registered NPU buffer.
 * @return Number of bytes read (>0), or negative errno on failure.
 */
ssize_t npu_file_read(npu_file_ctx_t *ctx, npu_file_handle_t *handle, int fd, uint64_t file_offset, uint64_t size,
                      uint64_t npu_offset);

/**
 * @brief Submit a zero-copy write (NPU HBM -> file) via NVMe passthrough.
 *
 * @param [in] ctx          NDS context.
 * @param [in] handle       Registered buffer handle.
 * @param [in] fd           File descriptor opened with O_DIRECT | O_WRONLY.
 * @param [in] file_offset  Offset in the file to write at.
 * @param [in] size         Number of bytes to write.
 * @param [in] npu_offset   Offset within the registered NPU buffer.
 * @return Number of bytes written (>0), or negative errno on failure.
 */
ssize_t npu_file_write(npu_file_ctx_t *ctx, npu_file_handle_t *handle, int fd, uint64_t file_offset, uint64_t size,
                       uint64_t npu_offset);

#ifdef __cplusplus
}
#endif

#endif /* NPU_FILE_H */
