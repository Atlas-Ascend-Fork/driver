/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 and only version 2 as
 * published by the Free Software Foundation.
 *
 * NPU Direct Storage (NDS) — NVMe passthrough glue.
 *
 * The legacy nvidia-fs-style approach used by this module was to register
 * a `struct npufs_dma_rw_ops` hook table with a patched NVMe driver so that
 * the NVMe block layer would invoke our hooks while building the DMA SGL.
 * That required both the VFS O_DIRECT path (and 4K "shadow pages") and a
 * patched NVMe driver — heavy and brittle.
 *
 * The new design drops the VFS path entirely:
 *
 *   1. User space resolves the file's on-disk layout with FS_IOC_FIEMAP.
 *   2. For each FIEMAP extent the kernel allocates an NVMe passthrough
 *      request via `nvme_alloc_request`, flags it `RQF_NVME_PT`, fills a
 *      `struct nvme_command` whose SGL points at the peer-DMA-mapped NPU
 *      HBM address, and submits it via `blk_execute_rq_nowait`.
 *   3. A tracepoint probe on `nvme_setup_cmd` rewrites the SGL type of any
 *      RQF_NVME_PT request to `NVME_CMD_SGL_METABUF` so the controller
 *      treats the supplied address as a metadata-buffer SGL (which is
 *      the form used for peer-to-peer DMA to NPU BAR memory).
 *
 * `nvme_alloc_request` and `nvme_setup_cmd` are not exported to modules on
 * every kernel, so they are looked up at module load with `__symbol_get`
 * (for the function symbol) and `for_each_kernel_tracepoint` (for the
 * tracepoint by name).  If either is missing, the module still loads but
 * READ/WRITE ioctls return -ENOSYS.
 */
#ifndef NPU_NDS_HOOK_H
#define NPU_NDS_HOOK_H

#include <linux/types.h>
#include <linux/blkdev.h>
#include <linux/blk-mq.h>

/*
 * Page-index encoding retained for the mgroup_id allocator so that the
 * pinned NPU buffer still has a unique 64-bit key (just like the legacy
 * path, but the key is no longer encoded into any struct page).
 */
#define NDS_BLOCK_SIZE 4096
#define NDS_PAGE_TO_BLOCK_ORDER 0
#define NDS_MAX_SHADOW_PAGES_ORDER (12 - NDS_PAGE_TO_BLOCK_ORDER)
#define NDS_MAX_SHADOW_PAGES (1 << NDS_MAX_SHADOW_PAGES_ORDER)
#define NDS_MIN_BASE_INDEX ((unsigned long)1L << 32)

/*
 * RQF_NVME_PT — NVMe passthrough request flag (matches the value used in
 * include/linux/blk_types.h).  Redefined here so the module builds against
 * older kernels that pre-date the symbol; at runtime we prefer the kernel
 * header definition when present.
 */
#ifndef RQF_NVME_PT
#define RQF_NVME_PT (1ULL << 12)
#endif

/*
 * NVMe SGL descriptor type values.  The mainline kernel enum in
 * include/linux/nvme.h uses `NVME_SGL_DATA_BLOCK = 0x00`, but does not
 * define a "metadata-buffer" SGL type.  The METABUF constant below is a
 * vendor-tree convention (mirrors the name used by nvidia-fs); it is
 * stamped onto every RQF_NVME_PT request by the nvme_setup_cmd
 * tracepoint probe so that the controller treats the supplied bus
 * address as a P2P-friendly metadata-buffer descriptor.  The actual
 * value must be cross-checked against the target NVMe controller /
 * nvidia-fs reference — keep it identical to `NVME_SGL_DATA_BLOCK` if
 * the controller already accepts P2P addresses under the standard
 * data-block type.
 */
#ifndef NVME_CMD_SGL_METABUF
#define NVME_CMD_SGL_METABUF 0x00
#endif
#ifndef NVME_CMD_SGL_DATA_BLOCK
#define NVME_CMD_SGL_DATA_BLOCK 0x00
#endif

/* Forward declaration of struct nvme_command (real one lives in <linux/nvme.h>). */
struct nvme_command;

/*
 * Function pointer type for `nvme_alloc_request(struct request_queue *q,
 * struct nvme_command *cmd, blk_mq_req_flags_t flags)` — looked up at
 * module load via __symbol_get.
 */
typedef struct request *(*nvme_alloc_request_fn_t)(struct request_queue *q, struct nvme_command *cmd,
                                                   blk_mq_req_flags_t flags);

/* Kernel symbol name to look up for nvme_alloc_request. */
#define NDS_NVME_ALLOC_REQUEST_SYM "nvme_alloc_request"

/*
 * blk_abort_request — used to cancel in-flight NVMe PT requests when the
 * drain timeout fires.  Looked up at module load via __symbol_get so the
 * module builds on kernels that don't export it to modules.
 */
typedef void (*blk_abort_request_fn_t)(struct request *req);
#define NDS_BLK_ABORT_REQUEST_SYM "blk_abort_request"

/* Name of the nvme_setup_cmd tracepoint (resolved via for_each_kernel_tracepoint). */
#define NDS_NVME_SETUP_CMD_TP_NAME "nvme_setup_cmd"

#endif /* NPU_NDS_HOOK_H */
