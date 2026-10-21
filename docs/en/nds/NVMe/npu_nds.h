/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 and only version 2 as
 * published by the Free Software Foundation.
 *
 * NPU Direct Storage (NDS) — ioctl contract shared between the
 * `npu_nds.ko` kernel module and the `libnpu_file.so` user-space client.
 *
 * Design pivot (vs. the legacy VFS + npufs_dma_rw_ops hook path):
 *
 *   - The user space queries the file's on-disk physical layout with the
 *     standard `FS_IOC_FIEMAP` ioctl and hands the resulting extents to
 *     the kernel together with the registered NPU buffer handle.
 *   - The kernel module builds an NVMe passthrough request
 *     (RQF_NVME_PT, allocated via nvme_alloc_request) whose SGL points
 *     straight at the DMA-mapped NPU HBM pages (peer-to-peer), and submits
 *     it with `blk_execute_rq_nowait` to the NVMe driver — no VFS, no
 *     shadow buffer, no CPU bounce.
 *   - A tracepoint probe on `nvme_setup_cmd` rewrites the SGL type of any
 *     RQF_NVME_PT request to `NVME_CMD_SGL_METABUF` so that the controller
 *     treats the address as a metadata-buffer SGL (P2P friendly).
 */

#ifndef NPU_NDS_H
#define NPU_NDS_H

#include <linux/ioctl.h>
#include <linux/types.h>

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
typedef uint8_t __u8;
typedef uint16_t __u16;
typedef uint32_t __u32;
typedef uint64_t __u64;
typedef int32_t __s32;
#endif

#define NDS_DEV_NAME "npu-nds"
#define NDS_DEV_PATH "/dev/" NDS_DEV_NAME

/* ioctl magic, mirror cuFile / nvidia-fs contract */
#define NDS_IOCTL_MAGIC 'N'

#define NDS_OPCODE_READ 0x1
#define NDS_OPCODE_WRITE 0x2

#define NDS_BLOCK_SIZE 4096
/* Maximum FIEMAP extents carried per READ/WRITE ioctl (demo cap). */
#define NDS_MAX_EXTENTS 64

/*
 * NPUFS_IOCTL_MAP -> NDS_IOCTL_MAP
 * Pin NPU device memory (hal_kernel_p2p_get_pages) and create the mgroup.
 * The new design no longer allocates 4K shadow pages nor exposes an mmap
 * offset — zero-copy is achieved through NVMe passthrough, not through the
 * VFS bio path.  The mmap_* fields are retained for ABI stability and are
 * set to 0 by the kernel.
 */
struct npu_nds_map_args {
    __u64 npu_va; /* in:  NPU virtual address, page aligned */
    __u64 size;   /* in:  size in bytes, page aligned */
    __u32 flags;  /* in:  reserved, set 0 */
    __u32 reserved;
    /* out */
    __u64 mgroup_id;     /* out: opaque id of the pinned NPU buffer */
    __u64 mmap_offset;   /* out: 0 (legacy field, unused) */
    __u64 mmap_size;     /* out: 0 (legacy field, unused) */
    __u32 npu_page_size; /* out: NPU physical page size (bytes) */
    __u32 npu_page_num;  /* out: number of NPU physical pages */
};

struct npu_nds_unmap_args {
    __u64 mgroup_id; /* in: id returned by NDS_IOCTL_MAP */
};

/*
 * FIEMAP extent captured by the user space via FS_IOC_FIEMAP.
 *
 *   `physical` is the byte offset on the backing block device (LBA *
 *   sector_size) as reported by FIEMAP.  The kernel converts it to NVMe
 *   SLBA = physical / 512.
 *
 *   `npu_offset` is the offset within the registered NPU buffer that
 *   corresponds to this extent's logical position in the file, so the
 *   kernel can compute the NPU BAR DMA address for that range.
 */
struct npu_nds_fiemap_extent {
    __u64 logical;    /* logical offset in the file (bytes) */
    __u64 physical;   /* physical on-disk offset (bytes) */
    __u64 length;     /* extent length in bytes */
    __u64 npu_offset; /* offset within the registered NPU buffer */
    __u32 flags;      /* FIEMAP_EXTENT_* flags (passed through) */
    __u32 reserved;
};

/*
 * NPUFS_IOCTL_READ / NPUFS_IOCTL_WRITE -> NDS_IOCTL_READ / WRITE
 *
 * Submit NVMe passthrough (RQF_NVME_PT) requests that DMA directly between
 * the NVMe namespace LBA range (described by the FIEMAP extents) and the
 * registered NPU HBM buffer (whose pages are pinned and DMA-mapped against
 * the NVMe peer device).  No bounce buffer, no VFS, no shadow pages.
 */
struct npu_nds_io_args {
    __u32 fd;          /* in: file descriptor (used to find bdev) */
    __u32 opcode;      /* in:  NDS_OPCODE_READ / NDS_OPCODE_WRITE */
    __u64 mgroup_id;   /* in:  mgroup id returned by NDS_IOCTL_MAP */
    __u64 file_offset; /* in:  offset in the file */
    __u64 size;        /* in:  bytes to transfer */
    __u64 npu_offset;  /* in:  offset within the registered NPU buffer */
    __u32 extent_cnt;  /* in:  number of extents in @extents_ptr */
    __u32 reserved2;
    __u64 extents_ptr; /* in:  pointer to npu_nds_fiemap_extent[] */
    /* out */
    __u64 done; /* out: bytes transferred */
    __s32 ret;  /* out: return code, 0 for success */
    __u32 reserved3;
};

#define NDS_IOCTL_MAP _IOWR(NDS_IOCTL_MAGIC, 1, struct npu_nds_map_args)
#define NDS_IOCTL_READ _IOWR(NDS_IOCTL_MAGIC, 2, struct npu_nds_io_args)
#define NDS_IOCTL_WRITE _IOWR(NDS_IOCTL_MAGIC, 3, struct npu_nds_io_args)
#define NDS_IOCTL_UNMAP _IOW(NDS_IOCTL_MAGIC, 4, struct npu_nds_unmap_args)

#endif /* NPU_NDS_H */
