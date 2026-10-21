/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 and only version 2 as
 * published by the Free Software Foundation.
 *
 * NPU Direct Storage (NDS) vendor hook definitions.
 *
 * This header defines the NVMe DMA ops struct and page-index encoding that
 * the NPU NDS module uses to register itself with the NVMe driver, mirroring
 * the nvidia-fs (GPUDirect Storage) hook mechanism.
 *
 * The NVMe driver (patched with npufs hooks) exports registration symbols:
 *   nvme_v1_register_nvfs_dma_ops / nvme_v1_unregister_nvfs_dma_ops
 * The NPU module calls these via __symbol_get/__symbol_put to register its
 * own ops table, which intercepts the NVMe driver's DMA mapping path:
 *
 *   1. npu_blk_rq_map_sg     - builds sglist from NPU HBM physical pages
 *   2. npu_dma_map_sg_attrs   - dma_map_resource NPU PAs → BAR bus addresses
 *
 * The shadow pages (page->mapping == NULL) serve as markers: the hook
 * identifies them, looks up the mgroup, and replaces the CPU physical
 * address with the NPU HBM P2P DMA address.  The NVMe controller then
 * DMAs directly to/from NPU HBM — zero-copy, no drvMemcpy.
 */
#ifndef NPU_NDS_HOOK_H
#define NPU_NDS_HOOK_H

#include <linux/types.h>
#include <linux/blkdev.h>
#include <linux/blk-mq.h>
#include <linux/scatterlist.h>
#include <linux/dma-direction.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/page-flags.h>

/* Feature bitmap — must match nvidia-fs enum ft_bits (npufs-dma.h:104-112) */
#define NPUFS_FT_PREP_SGLIST (1ULL << 0)
#define NPUFS_FT_MAP_SGLIST (1ULL << 1)
#define NPUFS_FT_IS_GPU_PAGE (1ULL << 2)
#define NPUFS_FT_DEVICE_PRIORITY (1ULL << 3)
#define NPUFS_FT_GET_GPU_SGLIST_RDMA (1ULL << 4)

#define NPU_NDS_FT_ALL (NPUFS_FT_PREP_SGLIST | NPUFS_FT_MAP_SGLIST | NPUFS_FT_IS_GPU_PAGE | NPUFS_FT_DEVICE_PRIORITY)

/* Page-index encoding — must match nvidia-fs (npufs-mmap.h:33-50) */
#define NDS_BLOCK_SIZE 4096
#define NDS_PAGE_TO_BLOCK_ORDER 0 /* PAGE_SIZE == NDS_BLOCK_SIZE */
#define NDS_MAX_SHADOW_PAGES_ORDER (12 - NDS_PAGE_TO_BLOCK_ORDER)
#define NDS_MAX_SHADOW_PAGES (1 << NDS_MAX_SHADOW_PAGES_ORDER)
#define NDS_MIN_BASE_INDEX ((unsigned long)1L << 32)

/* Forward declaration — not used by NPU, kept for struct compatibility */
struct npufs_rdma_info;

/*
 * struct npufs_dma_rw_ops — vendor hook ops table.
 * Layout MUST match nvidia-fs struct npufs_dma_rw_ops (npufs-dma.h:74-101)
 * because the NVMe driver casts the registered pointer to this type.
 */
struct npufs_dma_rw_ops {
    unsigned long long ft_bmap;

    int (*npufs_blk_rq_map_sg)(struct request_queue *q, struct request *req, struct scatterlist *sglist);

    int (*npufs_dma_map_sg_attrs)(struct device *device, struct scatterlist *sglist, int nents,
                                  enum dma_data_direction dma_dir, unsigned long attrs);

    int (*npufs_dma_unmap_sg)(struct device *device, struct scatterlist *sglist, int nents,
                              enum dma_data_direction dma_dir);

    bool (*npufs_is_gpu_page)(struct page *page);

    unsigned int (*npufs_gpu_index)(struct page *page);

    unsigned int (*npufs_device_priority)(struct device *dev, unsigned int gpu_index);

    int (*npufs_get_gpu_sglist_rdma_info)(struct scatterlist *sglist, int nents, struct npufs_rdma_info *rdma_infop);
};

/* Registration function typedefs */
typedef int (*npufs_register_dma_ops_fn_t)(struct npufs_dma_rw_ops *ops);
typedef void (*npufs_unregister_dma_ops_fn_t)(void);

/*
 * Symbols exported by the patched NVMe driver.
 * nvidia-fs uses the same mechanism (npufs-mod.c:56-64).
 * v1 is the legacy registration symbol set; v2 is the newer variant exported
 * by newer patched NVMe drivers. The module tries v1 first, then falls back
 * to v2 — per design §4.2.
 */
#define NVME_V1_REGISTER_SVM "nvme_v1_register_nvfs_dma_ops"
#define NVME_V1_UNREGISTER_SVM "nvme_v1_unregister_nvfs_dma_ops"
#define NVME_V2_REGISTER_SVM "nvme_v2_register_nvfs_dma_ops"
#define NVME_V2_UNREGISTER_SVM "nvme_v2_unregister_nvfs_dma_ops"

#endif /* NPU_NDS_HOOK_H */
