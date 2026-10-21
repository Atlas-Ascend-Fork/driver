/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
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
#endif

#define NDS_DEV_NAME "npu-nds"
#define NDS_DEV_PATH "/dev/" NDS_DEV_NAME

/* ioctl magic, mirror cuFile / nvidia-fs contract */
#define NDS_IOCTL_MAGIC 'N'

#define NDS_OPCODE_READ 0x1
#define NDS_OPCODE_WRITE 0x2

/*
 * NPUFS_IOCTL_MAP    -> NDS_IOCTL_MAP
 * Pin NPU device memory (hal_kernel_p2p_get_pages), allocate 4K shadow pages,
 * return a mgroup id and the mmap offset for the shadow buffer.
 */
struct npu_nds_map_args {
    __u64 npu_va; /* in:  NPU virtual address, page aligned */
    __u64 size;   /* in:  size in bytes, page aligned */
    __u32 flags;  /* in:  reserved, set 0 */
    __u32 reserved;
    /* out */
    __u64 mgroup_id;     /* out: opaque id of the shadow buffer group */
    __u64 mmap_offset;   /* out: offset to be used in mmap(/dev/npu-nds) */
    __u64 mmap_size;     /* out: total shadow buffer size */
    __u32 npu_page_size; /* out: NPU physical page size (bytes) */
    __u32 npu_page_num;  /* out: number of NPU physical pages */
};

struct npu_nds_unmap_args {
    __u64 mgroup_id; /* in: id returned by NDS_IOCTL_MAP */
};

/*
 * NPUFS_IOCTL_READ / NPUFS_IOCTL_WRITE -> NDS_IOCTL_READ / WRITE
 * Perform O_DIRECT IO between a file and the shadow buffer (which mirrors the
 * pinned NPU memory). fd must be opened with O_DIRECT | O_RDWR.
 */
struct npu_nds_io_args {
    __u32 fd;          /* in:  file descriptor opened with O_DIRECT */
    __u32 opcode;      /* in:  NDS_OPCODE_READ / NDS_OPCODE_WRITE */
    __u64 mgroup_id;   /* in:  mgroup id returned by NDS_IOCTL_MAP */
    __u64 file_offset; /* in:  offset in the file */
    __u64 size;        /* in:  bytes to transfer */
    /* out */
    __u64 done; /* out: bytes transferred */
    __s32 ret;  /* out: return code, 0 for success */
    __u32 reserved;
};

#define NDS_IOCTL_MAP _IOWR(NDS_IOCTL_MAGIC, 1, struct npu_nds_map_args)
#define NDS_IOCTL_READ _IOWR(NDS_IOCTL_MAGIC, 2, struct npu_nds_io_args)
#define NDS_IOCTL_WRITE _IOWR(NDS_IOCTL_MAGIC, 3, struct npu_nds_io_args)
#define NDS_IOCTL_UNMAP _IOW(NDS_IOCTL_MAGIC, 4, struct npu_nds_unmap_args)

#endif /* NPU_NDS_H */
