/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 and only version 2 as
 * published by the Free Software Foundation.
 *
 * NPU Direct Storage (NDS) kernel-side enabler demo — NVMe passthrough path.
 *
 * This module is the Ascend NPU counterpart of NVIDIA's nvidia-fs.ko
 * (GPUDirect Storage). It exposes /dev/npu-nds and a small ioctl contract
 * so that user space can:
 *   1. pin NPU device memory (hal_kernel_p2p_get_pages)
 *   2. issue zero-copy NVMe read/write between a file and the pinned NPU
 *      HBM buffer without going through the VFS bio path.
 *
 * Zero-copy P2P DMA path (NVMe passthrough):
 *   The module resolves `nvme_alloc_request` via __symbol_get and the
 *   `nvme_setup_cmd` tracepoint via for_each_kernel_tracepoint. For each
 *   FIEMAP extent supplied by the user it:
 *     1. allocates an NVMe passthrough request (RQF_NVME_PT)
 *     2. builds a read/write NVMe command whose SGL points straight at
 *        the peer-DMA-mapped NPU HBM BAR address
 *     3. submits it via blk_execute_rq_nowait to the NVMe driver
 *   A tracepoint probe on `nvme_setup_cmd` rewrites the SGL type of any
 *   RQF_NVME_PT request to NVME_CMD_SGL_METABUF before the controller
 *   sees it, which is the form used for peer-to-peer metadata-buffer SGL.
 *
 * If `nvme_alloc_request` or `nvme_setup_cmd` is unavailable, the module
 * still loads but READ/WRITE return -ENOSYS (user space can fall back to
 * ordinary O_DIRECT read/write + drvMemcpy bounce).
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/atomic.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/scatterlist.h>
#include <linux/dma-mapping.h>
#include <linux/sched.h>
#include <linux/device.h>
#include <linux/pci.h>
#include <linux/blkdev.h>
#include <linux/blk-mq.h>
#include <linux/nvme.h>
#include <linux/tracepoint.h>
#include <linux/kallsyms.h>
#include <linux/completion.h>
#include <linux/kref.h>

#include "ascend_kernel_hal.h"
#include "npu_nds.h"
#include "npu_nds_hook.h"

#define NDS_TAG "npu_nds"

#define nds_info(fmt, ...) pr_info(NDS_TAG ": " fmt, ##__VA_ARGS__)
#define nds_warn(fmt, ...) pr_warn(NDS_TAG ": " fmt, ##__VA_ARGS__)
#define nds_err(fmt, ...) pr_err(NDS_TAG ": " fmt, ##__VA_ARGS__)

#define NDS_IO_ERR (-EIO)
#define NDS_BAD_REQ (-ENODATA)
/* Timeout for the entire async batch drain (jiffies). */
#define NDS_PT_TIMEOUT_JIFFIES (30 * HZ)
/* Grace period for blk_abort_request to land after drain timeout. */
#define NDS_PT_ABORT_GRACE_JIFFIES (1 * HZ)
/* NVMe logical block size assumed by the demo (matches FIEMAP granularity). */
#define NDS_NVME_LB_SIZE 512

/* NVMe passthrough function pointers — resolved at module load via __symbol_get.
 * Declared up here (before first use in nds_pt_submit_one / nds_pt_drain)
 * because file-scope static variables must be visible at their use site. */
static nvme_alloc_request_fn_t nds_nvme_alloc_request_fn;
static blk_abort_request_fn_t nds_blk_abort_request_fn;
static bool nds_pt_enabled;

enum nds_mgroup_state {
    NDS_MG_FREE = 0,
    NDS_MG_ACTIVE,
    NDS_MG_TERMINATED,
};

/*
 * mgroup: pinned NPU buffer + its peer DMA mapping cache.
 *
 * In the legacy design this struct also held the 4K shadow pages (and the
 * mmap offset / VMA lifetime refs) because the VFS O_DIRECT path needed a
 * struct page proxy per NPU page.  With the NVMe passthrough pivot, the
 * shadow pages are gone — the SGL in the passthrough command points
 * directly at the peer-DMA-mapped NPU HBM BAR addresses.
 */
struct nds_mgroup {
    struct list_head list;
    u64 mgroup_id; /* global key, returned to user */
    u64 npu_va;
    u64 size; /* user requested size */
    u32 npu_page_size;
    u32 npu_page_num;
    struct p2p_page_table *page_table;

    /* P2P DMA mapping cache against a peer device (nvidia-fs buckets) */
    struct sg_table sgt;
    struct device *dma_dev;
    bool dma_mapped;

    enum nds_mgroup_state state;
    bool pinned_ref_dropped; /* set when the p2p free callback dropped the pin ref */
    struct kref refcount;
    struct mutex lock;
};

static LIST_HEAD(nds_mgroup_list);
static DEFINE_MUTEX(nds_mgroup_mutex);
static atomic64_t nds_base_index = ATOMIC64_INIT(NDS_MIN_BASE_INDEX);

static void nds_mgroup_release(struct kref *ref)
{
    struct nds_mgroup *mg = container_of(ref, struct nds_mgroup, refcount);

    if (mg->dma_mapped && mg->dma_dev) {
        struct scatterlist *sg;
        int j;

        for_each_sg(mg->sgt.sgl, sg, mg->sgt.nents, j)
        {
            dma_addr_t a = sg_dma_address(sg);
            if (a)
                dma_unmap_resource(mg->dma_dev, a, sg_dma_len(sg), DMA_BIDIRECTIONAL, 0);
        }
        sg_free_table(&mg->sgt);
        mg->dma_mapped = false;
        /* release the peer pci_dev reference acquired by nds_peer_device */
        put_device(mg->dma_dev);
        mg->dma_dev = NULL;
    }

    if (mg->page_table) {
        if (hal_kernel_p2p_put_pages(mg->page_table))
            nds_err("hal_kernel_p2p_put_pages failed\n");
    }

    kfree(mg);
}

static struct nds_mgroup *nds_mgroup_get(u64 id)
{
    struct nds_mgroup *mg;

    mutex_lock(&nds_mgroup_mutex);
    list_for_each_entry(mg, &nds_mgroup_list, list)
    {
        if (mg->mgroup_id == id) {
            if (mg->state == NDS_MG_TERMINATED)
                break;
            kref_get(&mg->refcount);
            mutex_unlock(&nds_mgroup_mutex);
            return mg;
        }
    }
    mutex_unlock(&nds_mgroup_mutex);
    return NULL;
}

static void nds_mgroup_put(struct nds_mgroup *mg)
{
    kref_put(&mg->refcount, nds_mgroup_release);
}

/*
 * nds_get_npu_pa — NPU HBM physical address for a byte offset within the
 * registered NPU buffer.  Handles 4K shadow pages as well as NPU huge
 * pages (npu_page_size > PAGE_SIZE).
 */
static u64 nds_get_npu_pa(struct nds_mgroup *mg, u64 npu_offset)
{
    u32 npu_page_idx;
    u64 page_offset;

    if (!mg->page_table || !mg->page_table->pages_info || !mg->npu_page_size)
        return 0;

    npu_page_idx = (u32)(npu_offset / mg->npu_page_size);
    page_offset = npu_offset % mg->npu_page_size;

    if (npu_page_idx >= mg->npu_page_num)
        return 0;

    return mg->page_table->pages_info[npu_page_idx].pa + page_offset;
}

/*
 * nds_ensure_dma_mapped — lazily create the peer DMA mapping cache.
 *
 * Maps each NPU page PA via dma_map_resource against the peer (NVMe)
 * device, caching the result in mg->sgt.  Demo limitation: single peer.
 */
static int nds_ensure_dma_mapped(struct nds_mgroup *mg, struct device *peer)
{
    struct p2p_page_table *pt = mg->page_table;
    struct scatterlist *sg;
    u32 i;
    int ret;
    int j;

    if (mg->dma_mapped) {
        if (mg->dma_dev != peer)
            return -EBUSY;
        return 0;
    }
    if (!peer || !pt || !pt->pages_info || pt->page_num == 0)
        return -ENODEV;

    ret = sg_alloc_table(&mg->sgt, pt->page_num, GFP_KERNEL);
    if (ret)
        return ret;

    for_each_sg(mg->sgt.sgl, sg, pt->page_num, i)
    {
        dma_addr_t a = dma_map_resource(peer, pt->pages_info[i].pa, mg->npu_page_size, DMA_BIDIRECTIONAL, 0);
        if (dma_mapping_error(peer, a)) {
            nds_err("dma_map_resource failed @ %u\n", i);
            /* Roll back every mapping established so far so that a later
             * nds_get_dma_addr can never hand a half-mapped (zero) address
             * to the NVMe controller. */
            for_each_sg(mg->sgt.sgl, sg, i, j)
            {
                dma_addr_t prev = sg_dma_address(sg);
                if (prev)
                    dma_unmap_resource(peer, prev, sg_dma_len(sg), DMA_BIDIRECTIONAL, 0);
            }
            sg_free_table(&mg->sgt);
            return -EIO;
        }
        sg_dma_address(sg) = a;
        sg_dma_len(sg) = mg->npu_page_size;
    }
    mg->dma_dev = peer;
    mg->dma_mapped = true;
    get_device(peer);
    nds_info("dma mapped %u NPU pages against peer %s\n", pt->page_num, dev_name(peer));
    return 0;
}

/*
 * nds_get_dma_addr — cached P2P DMA bus address for a byte offset within
 * the registered NPU buffer.  This is the IOVA the NVMe controller should
 * DMA to/from.
 */
static dma_addr_t nds_get_dma_addr(struct nds_mgroup *mg, u64 npu_offset)
{
    u32 npu_page_idx = (u32)(npu_offset / mg->npu_page_size);
    u64 page_offset = npu_offset % mg->npu_page_size;
    struct scatterlist *sg;
    int i;

    if (!mg->dma_mapped)
        return 0;

    for_each_sg(mg->sgt.sgl, sg, mg->sgt.nents, i)
    {
        if (i == (int)npu_page_idx)
            return sg_dma_address(sg) + page_offset;
    }
    return 0;
}

static void nds_p2p_free_callback(void *data)
{
    struct nds_mgroup *mg = data;

    if (!mg)
        return;
    nds_warn("mgroup %llu: NPU memory free callback, terminating\n", mg->mgroup_id);
    mutex_lock(&mg->lock);
    mg->state = NDS_MG_TERMINATED;
    mutex_unlock(&mg->lock);
    /*
     * Drop the reference that MAP held for the lifetime of the pinning.
     * Guard with pinned_ref_dropped so that UNMAP does not double-drop if it
     * runs after the callback.  We do NOT call hal_kernel_p2p_put_pages here
     * (deadlock risk per HAL contract); put_pages runs later from the
     * mgroup_release context triggered by UNMAP.
     */
    mutex_lock(&nds_mgroup_mutex);
    if (!mg->pinned_ref_dropped) {
        mg->pinned_ref_dropped = true;
        mutex_unlock(&nds_mgroup_mutex);
        nds_mgroup_put(mg);
    } else {
        mutex_unlock(&nds_mgroup_mutex);
    }
}

static int nds_do_map(struct npu_nds_map_args *args)
{
    struct nds_mgroup *mg;
    int ret;

    if (!args->npu_va || !args->size || (args->size & (NDS_BLOCK_SIZE - 1)) || (args->npu_va & (NDS_BLOCK_SIZE - 1))) {
        nds_err("map: va/size must be %d-aligned\n", NDS_BLOCK_SIZE);
        return -EINVAL;
    }

    mg = kzalloc(sizeof(*mg), GFP_KERNEL);
    if (!mg)
        return -ENOMEM;

    mutex_init(&mg->lock);
    kref_init(&mg->refcount);
    /*
     * Take the "pinning lifetime" reference BEFORE calling
     * hal_kernel_p2p_get_pages: the HAL registers our free_callback during that
     * call, and the callback may fire at any point afterwards (including
     * before we finish setting up).  Holding this extra ref guarantees the
     * mgroup stays alive across the callback's nds_mgroup_put.
     */
    kref_get(&mg->refcount); /* ref=2: object + pinning lifetime */
    mg->npu_va = args->npu_va;
    mg->size = args->size;
    mg->mgroup_id = (u64)atomic64_inc_return(&nds_base_index);
    mg->state = NDS_MG_ACTIVE;

    /* pin NPU pages, this is the NPU equivalent of nvidia_p2p_get_pages */
    ret = hal_kernel_p2p_get_pages(mg->npu_va, mg->size, nds_p2p_free_callback, mg, &mg->page_table);
    if (ret) {
        nds_err("hal_kernel_p2p_get_pages failed: %d\n", ret);
        /* callback was never registered; drop both refs to free mg */
        nds_mgroup_put(mg);
        nds_mgroup_put(mg);
        return ret;
    }

    if (!mg->page_table || !mg->page_table->pages_info || !mg->page_table->page_num) {
        nds_err("invalid page_table from hal_kernel_p2p_get_pages\n");
        /* callback IS registered; release will call hal_kernel_p2p_put_pages */
        nds_mgroup_put(mg);
        nds_mgroup_put(mg);
        return -ENODATA;
    }
    mg->npu_page_size = mg->page_table->page_size;
    mg->npu_page_num = mg->page_table->page_num;

    mutex_lock(&nds_mgroup_mutex);
    list_add_tail(&mg->list, &nds_mgroup_list);
    mutex_unlock(&nds_mgroup_mutex);

    args->mgroup_id = mg->mgroup_id;
    args->mmap_offset = 0;
    args->mmap_size = 0;
    args->npu_page_size = mg->npu_page_size;
    args->npu_page_num = mg->npu_page_num;

    nds_info("map: npu_va=0x%llx size=%llu npu_pg=%u/%u -> mgid=%llu\n", mg->npu_va, mg->size, mg->npu_page_num,
             mg->npu_page_size, mg->mgroup_id);
    return 0;
}

static int nds_do_unmap(struct npu_nds_unmap_args *args)
{
    struct nds_mgroup *mg;

    mutex_lock(&nds_mgroup_mutex);
    list_for_each_entry(mg, &nds_mgroup_list, list)
    {
        if (mg->mgroup_id == args->mgroup_id) {
            list_del(&mg->list);
            mg->state = NDS_MG_TERMINATED;
            /*
             * Drop the MAP lifetime "pinning" reference only if the p2p free
             * callback has not already done so.  Then always drop the "object
             * exists" reference (from kref_init) to trigger teardown.
             */
            if (!mg->pinned_ref_dropped) {
                mg->pinned_ref_dropped = true;
                mutex_unlock(&nds_mgroup_mutex);
                nds_info("unmap: mgid=%llu (dropping pin ref)\n", mg->mgroup_id);
                nds_mgroup_put(mg);
                nds_mgroup_put(mg);
            } else {
                mutex_unlock(&nds_mgroup_mutex);
                nds_info("unmap: mgid=%llu (pin ref already dropped by callback)\n", mg->mgroup_id);
                nds_mgroup_put(mg);
            }
            return 0;
        }
    }
    mutex_unlock(&nds_mgroup_mutex);
    nds_err("unmap: mgroup %llu not found\n", args->mgroup_id);
    return -ENOENT;
}

/* ============================================================================
 *  NVMe passthrough helpers
 * ========================================================================== */

/*
 * Look up the NVMe peer device backing the file's block device.
 *
 * Best-effort demo: walk PCI devices looking for an NVMe controller (PCI base
 * class STORAGE = 0x01, programming interface 0x02 for NVMe).  dma_map_resource
 * must be issued against the NVMe controller so the resulting bus address is
 * valid for that controller's DMA engine.  A production driver maintains a
 * gpu2peer distance matrix (npufs-pci.c).
 */
static struct device *nds_peer_device(u32 peer_devid)
{
    struct pci_dev *pdev = NULL;

    while ((pdev = pci_get_device(PCI_ANY_ID, PCI_ANY_ID, pdev)) != NULL) {
        u32 cls = pdev->class >> 16;
        if (cls == PCI_BASE_CLASS_STORAGE) {
            /*
             * pci_get_device already acquired a reference; keep it for the
             * lifetime of the cached DMA mapping (released in mgroup_release).
             */
            return &pdev->dev;
        }
    }
    (void)peer_devid;
    return NULL;
}

/*
 * Resolve the request_queue of the NVMe namespace backing @file.
 * For filesystem files this is file_inode(file)->i_sb->s_bdev; for raw
 * block device files it is I_BDEV(file_inode(file)).
 */
static struct request_queue *nds_file_to_queue(struct file *file, struct block_device **out_bdev)
{
    struct inode *inode = file ? file_inode(file) : NULL;
    struct block_device *bdev = NULL;

    if (!inode)
        return NULL;
    if (S_ISBLK(inode->i_mode))
        bdev = I_BDEV(inode);
    else if (inode->i_sb && inode->i_sb->s_bdev)
        bdev = inode->i_sb->s_bdev;

    if (!bdev)
        return NULL;
    if (out_bdev)
        *out_bdev = bdev;
    return bdev_get_queue(bdev);
}

/* Per-request passthrough completion context (heap-allocated, part of batch). */
struct nds_pt_ctx {
    struct nds_pt_batch *batch; /* parent batch */
    blk_status_t status;
    u32 nr_bytes;
    struct request *req; /* back-pointer for abort */
    bool completed;
};

/*
 * Batch context for async submit + drain.  Lifetime is managed by a kref
 * so that nds_do_io and the last end_io can safely race on teardown:
 *
 *   - nds_do_io holds 1 ref (the "owner" ref from kref_init)
 *   - each submitted request holds 1 ref (kref_get before submit)
 *   - every end_io does kref_put (drops the request's ref)
 *   - nds_do_io does kref_put at cleanup (drops the owner ref)
 *
 * The last kref_put (ref → 0) frees batch + ctxs via nds_pt_batch_release.
 * This eliminates the smp_mb handoff race of the previous abandoned-flag
 * design: both sides simply kref_put and the kernel atomics guarantee
 * exactly one side frees.
 *
 * The `abandoned` flag is now purely advisory — it tells end_io to skip
 * the complete() call when nobody is waiting (nds_do_io already returned).
 * A racy read of `abandoned` is harmless: a spurious complete() on a
 * completion nobody waits on is a no-op.
 */
struct nds_pt_batch {
    struct kref ref;
    struct completion cmp;
    atomic_t pending; /* in-flight count */
    u32 nr_extents;
    struct nds_pt_ctx *ctxs; /* array[nr_extents] */
    bool abandoned;          /* nds_do_io gave up; skip complete() */
};

static void nds_pt_batch_release(struct kref *ref)
{
    struct nds_pt_batch *batch = container_of(ref, struct nds_pt_batch, ref);

    kfree(batch->ctxs);
    kfree(batch);
}

static void nds_pt_end_io(struct request *req, blk_status_t err, unsigned int nr_bytes)
{
    struct nds_pt_ctx *ctx = req->end_io_data;
    struct nds_pt_batch *batch;

    if (!ctx)
        return;

    batch = ctx->batch;
    ctx->status = err;
    ctx->nr_bytes = nr_bytes;
    ctx->completed = true;

    if (atomic_dec_and_test(&batch->pending) && !batch->abandoned)
        complete(&batch->cmp);

    kref_put(&batch->ref, nds_pt_batch_release);
}

/* ============================================================================
 *  Tracepoint probe on nvme_setup_cmd
 *
 *  Registered via tracepoint_probe_register once the tracepoint is located
 *  with for_each_kernel_tracepoint.  For every request that carries the
 *  RQF_NVME_PT flag we stamp the SGL type to NVME_CMD_SGL_METABUF so the
 *  controller treats the SGL address as a peer-to-peer metadata buffer
 *  SGL — the descriptor form that lets the controller DMA straight into
 *  the NPU HBM BAR aperture.
 * ========================================================================== */

static struct tracepoint *nds_nvme_setup_cmd_tp;

static void nds_tp_lookup_cb(struct tracepoint *tp, void *priv)
{
    if (!tp || !tp->name)
        return;
    if (!strcmp(tp->name, NDS_NVME_SETUP_CMD_TP_NAME))
        *((struct tracepoint **)priv) = tp;
}

static void nds_nvme_setup_cmd_probe(void *data, struct request *req, struct nvme_command *cmd)
{
    if (!req || !cmd)
        return;
    if (!(req->rq_flags & RQF_NVME_PT))
        return;
    /*
     * Re-stamp the SGL descriptor type to NVME_CMD_SGL_METABUF so the
     * controller accepts the bus address in dptr.sgl.addr as a
     * metadata-buffer SGL (peer-to-peer DMA into NPU BAR).
     */
    cmd->common.dptr.sgl.type = NVME_CMD_SGL_METABUF;
    (void)data;
}

static int nds_register_tracepoint(void)
{
    int ret;

    nds_nvme_setup_cmd_tp = NULL;
    for_each_kernel_tracepoint(nds_tp_lookup_cb, &nds_nvme_setup_cmd_tp);
    if (!nds_nvme_setup_cmd_tp) {
        nds_warn("tracepoint %s not found — passthrough SGL retype disabled\n", NDS_NVME_SETUP_CMD_TP_NAME);
        return -ENOSYS;
    }
    ret = tracepoint_probe_register(nds_nvme_setup_cmd_tp, (void *)nds_nvme_setup_cmd_probe, NULL);
    if (ret) {
        nds_err("tracepoint_probe_register failed: %d\n", ret);
        nds_nvme_setup_cmd_tp = NULL;
        return ret;
    }
    nds_info("registered probe on tracepoint %s\n", NDS_NVME_SETUP_CMD_TP_NAME);
    return 0;
}

static void nds_unregister_tracepoint(void)
{
    if (nds_nvme_setup_cmd_tp) {
        tracepoint_probe_unregister(nds_nvme_setup_cmd_tp, (void *)nds_nvme_setup_cmd_probe, NULL);
        nds_nvme_setup_cmd_tp = NULL;
    }
}

/* ============================================================================
 *  READ / WRITE — async submit + drain
 *
 *  Phase 1: build and submit all NVMe passthrough requests via
 *           blk_execute_rq_nowait (no per-request wait).  kref_get + pending
 *           increment BEFORE each submit so end_io can't race ahead.
 *  Phase 2: single wait_for_completion_timeout on the batch (30s budget).
 *  Phase 3: on timeout — blk_abort_request each pending request, wait 1s
 *           grace period for aborts to land.
 *  Phase 4: if still in flight after grace — mark batch abandoned (skip
 *           complete), kref_put owner ref.  Last end_io's kref_put frees.
 *
 *  Batch lifetime is kref-managed: owner ref (nds_do_io) + 1 ref per
 *  submitted request.  Both nds_do_io and every end_io kref_put; the
 *  last put (ref→0) frees batch+ctxs via nds_pt_batch_release.
 * ========================================================================== */

/*
 * Build and submit one NVMe passthrough request.  Does NOT wait — the caller
 * drains the batch after all submits are done.
 *
 * Returns 0 on success (ctx->req is set, end_io_data wired), negative errno
 * on failure (ctx is untouched).
 */
static int nds_pt_submit_one(struct request_queue *q, u32 nsid, struct device *peer_dev, struct nds_mgroup *mg,
                             const struct npu_nds_fiemap_extent *ext, int write, struct nds_pt_ctx *ctx)
{
    struct nvme_command cmd = {0};
    struct request *req;
    int ret;
    dma_addr_t npu_bar;
    unsigned int lb = NDS_NVME_LB_SIZE;
    u64 slba;
    u16 nlb;

    if (!ext->length || (ext->length & (lb - 1)) || (ext->physical & (lb - 1))) {
        nds_err("pt: extent not %u-aligned (phys=0x%llx len=%llu)\n", lb, ext->physical, ext->length);
        return -EINVAL;
    }

    slba = ext->physical / lb;
    nlb = (u16)(ext->length / lb - 1);

    ret = nds_ensure_dma_mapped(mg, peer_dev);
    if (ret) {
        nds_err("pt: ensure_dma_mapped failed: %d\n", ret);
        return ret;
    }
    npu_bar = nds_get_dma_addr(mg, ext->npu_offset);
    if (!npu_bar) {
        nds_err("pt: get_dma_addr failed for npu_offset=0x%llx\n", ext->npu_offset);
        return -EIO;
    }

    cmd.rw.opcode = write ? nvme_cmd_write : nvme_cmd_read;
    cmd.rw.flags = (1u << 6); /* PSDT=01b: SGL in dptr */
    cmd.rw.nsid = cpu_to_le32(nsid);
    cmd.rw.slba = cpu_to_le64(slba);
    cmd.rw.length = cpu_to_le16(nlb);
    cmd.rw.dptr.sgl.addr = cpu_to_le64(npu_bar);
    cmd.rw.dptr.sgl.length = cpu_to_le32((u32)ext->length);
    cmd.rw.dptr.sgl.type = NVME_CMD_SGL_DATA_BLOCK;

    if (!nds_nvme_alloc_request_fn) {
        nds_err("pt: nvme_alloc_request unavailable\n");
        return -ENOSYS;
    }
    req = nds_nvme_alloc_request_fn(q, &cmd, 0);
    if (IS_ERR_OR_NULL(req)) {
        nds_err("pt: nvme_alloc_request failed: %ld\n", PTR_ERR(req));
        return IS_ERR(req) ? PTR_ERR(req) : -ENOMEM;
    }
    req->rq_flags |= RQF_NVME_PT;

    ctx->req = req;
    req->end_io_data = ctx;

    blk_execute_rq_nowait(req, false, nds_pt_end_io);
    return 0;
}

static struct nds_pt_batch *nds_pt_batch_alloc(u32 nr_extents)
{
    struct nds_pt_batch *batch;

    batch = kzalloc(sizeof(*batch), GFP_KERNEL);
    if (!batch)
        return NULL;
    batch->ctxs = kcalloc(nr_extents, sizeof(*batch->ctxs), GFP_KERNEL);
    if (!batch->ctxs) {
        kfree(batch);
        return NULL;
    }
    batch->nr_extents = nr_extents;
    atomic_set(&batch->pending, 0);
    init_completion(&batch->cmp);
    batch->abandoned = false;
    kref_init(&batch->ref);
    return batch;
}

static int nds_pt_submit_all(struct nds_mgroup *mg, struct request_queue *q, u32 nsid, struct device *peer,
                             const struct npu_nds_fiemap_extent *extents, u32 nr_extents, int write,
                             struct nds_pt_batch *batch, u32 *submitted)
{
    u32 i;
    int ret = 0;

    mutex_lock(&mg->lock);
    if (mg->state != NDS_MG_ACTIVE) {
        mutex_unlock(&mg->lock);
        nds_err("io: mgroup %llu terminated\n", mg->mgroup_id);
        return -ENODEV;
    }

    for (i = 0; i < nr_extents; i++) {
        struct nds_pt_ctx *ctx = &batch->ctxs[i];
        ctx->batch = batch;
        ctx->completed = false;
        ctx->status = BLK_STS_IOERR;
        ctx->nr_bytes = 0;
        ctx->req = NULL;

        atomic_inc(&batch->pending);
        kref_get(&batch->ref);

        ret = nds_pt_submit_one(q, nsid, peer, mg, &extents[i], write, ctx);
        if (ret) {
            atomic_dec(&batch->pending);
            kref_put(&batch->ref, nds_pt_batch_release);
            nds_err("io: submit[%u] failed: %d\n", i, ret);
            break;
        }
        (*submitted)++;
    }
    mutex_unlock(&mg->lock);
    return ret;
}

static int nds_pt_drain(struct nds_pt_batch *batch, u32 submitted, bool *abandoned)
{
    unsigned long timeout;
    u32 i;

    *abandoned = false;
    if (submitted == 0)
        return 0;

    timeout = wait_for_completion_timeout(&batch->cmp, NDS_PT_TIMEOUT_JIFFIES);
    if (timeout)
        return 0;

    nds_err("io: drain timed out, %d/%u still in flight\n", atomic_read(&batch->pending), submitted);

    for (i = 0; i < submitted; i++) {
        if (!batch->ctxs[i].completed && batch->ctxs[i].req) {
            if (nds_blk_abort_request_fn)
                nds_blk_abort_request_fn(batch->ctxs[i].req);
            else
                nds_err("io: blk_abort_request unavailable; "
                        "req[%u] left in flight\n",
                        i);
        }
    }

    timeout = wait_for_completion_timeout(&batch->cmp, NDS_PT_ABORT_GRACE_JIFFIES);
    if (!timeout && atomic_read(&batch->pending) > 0) {
        nds_err("io: %d reqs still in flight after abort grace; "
                "abandoning batch\n",
                atomic_read(&batch->pending));
        batch->abandoned = true;
        *abandoned = true;
    }
    return -ETIMEDOUT;
}

static ssize_t nds_pt_collect(struct nds_pt_batch *batch, const struct npu_nds_fiemap_extent *extents, u32 submitted)
{
    ssize_t total = 0;
    u32 i;

    for (i = 0; i < submitted; i++) {
        struct nds_pt_ctx *ctx = &batch->ctxs[i];

        if (ctx->completed && ctx->status == BLK_STS_OK) {
            u32 bytes = ctx->nr_bytes;
            if (!bytes)
                bytes = extents[i].length;
            total += bytes;
        } else if (!ctx->completed) {
            nds_err("io: ctx[%u] not completed after drain\n", i);
        }
    }
    return total;
}

static int nds_pt_setup_io(struct npu_nds_io_args *args, struct nds_mgroup *mg, struct npu_nds_fiemap_extent **extents,
                           struct fd *f, struct request_queue **q, struct device **peer, int write)
{
    struct block_device *bdev = NULL;
    int ret;

    *extents = kcalloc(args->extent_cnt, sizeof(**extents), GFP_KERNEL);
    if (!*extents)
        return -ENOMEM;
    if (copy_from_user(*extents, (void __user *)(uintptr_t)args->extents_ptr, sizeof(**extents) * args->extent_cnt)) {
        ret = -EFAULT;
        goto out_free;
    }

    *f = fdget(args->fd);
    if (!f->file) {
        ret = -EBADF;
        goto out_free;
    }
    if (write && !(f->file->f_mode & FMODE_WRITE)) {
        ret = -EBADF;
        goto out_fd;
    }
    if (!write && !(f->file->f_mode & FMODE_READ)) {
        ret = -EBADF;
        goto out_fd;
    }

    *q = nds_file_to_queue(f->file, &bdev);
    if (!*q) {
        nds_err("io: file %d has no backing block device\n", args->fd);
        ret = -ENODEV;
        goto out_fd;
    }

    *peer = nds_peer_device(0);
    if (!*peer) {
        nds_err("io: no NVMe peer device found\n");
        ret = -ENODEV;
        goto out_fd;
    }
    return 0;

out_fd:
    fdput(*f);
out_free:
    kfree(*extents);
    *extents = NULL;
    return ret;
}

static int nds_io_validate_args(struct npu_nds_io_args *args, struct nds_mgroup **mg_out)
{
    struct nds_mgroup *mg;

    if (args->extent_cnt == 0 || args->extent_cnt > NDS_MAX_EXTENTS) {
        nds_err("io: invalid extent_cnt=%u\n", args->extent_cnt);
        return -EINVAL;
    }

    mg = nds_mgroup_get(args->mgroup_id);
    if (!mg) {
        nds_err("io: mgroup %llu not found/terminated\n", args->mgroup_id);
        return -ENOENT;
    }
    *mg_out = mg;
    return 0;
}

static int nds_do_io(struct npu_nds_io_args *args)
{
    struct nds_mgroup *mg;
    struct fd f = {0};
    int ret = 0;
    struct npu_nds_fiemap_extent *extents = NULL;
    struct request_queue *q = NULL;
    struct device *peer = NULL;
    struct nds_pt_batch *batch = NULL;
    int write = (args->opcode == NDS_OPCODE_WRITE);
    u32 submitted = 0;
    bool abandoned = false;

    ret = nds_io_validate_args(args, &mg);
    if (ret)
        return ret;

    ret = nds_pt_setup_io(args, mg, &extents, &f, &q, &peer, write);
    if (ret)
        goto out_put;

    batch = nds_pt_batch_alloc(args->extent_cnt);
    if (!batch) {
        ret = -ENOMEM;
        goto out_peer;
    }

    ret = nds_pt_submit_all(mg, q, 1, peer, extents, args->extent_cnt, write, batch, &submitted);
    if (ret)
        goto out_batch;

    ret = nds_pt_drain(batch, submitted, &abandoned);
    if (abandoned) {
        kref_put(&batch->ref, nds_pt_batch_release);
        batch = NULL;
        args->done = 0;
        args->ret = -ETIMEDOUT;
        goto out_peer;
    }

    args->done = (u64)nds_pt_collect(batch, extents, submitted);
    args->ret = ret;

out_batch:
    if (batch)
        kref_put(&batch->ref, nds_pt_batch_release);
out_peer:
    put_device(peer);
    fdput(f);
    kfree(extents);
out_put:
    nds_mgroup_put(mg);
    return ret;
}

static long nds_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    void __user *uarg = (void __user *)arg;

    switch (cmd) {
        case NDS_IOCTL_MAP: {
            struct npu_nds_map_args a;
            int r;

            if (copy_from_user(&a, uarg, sizeof(a)))
                return -EFAULT;
            r = nds_do_map(&a);
            if (r)
                return r;
            if (copy_to_user(uarg, &a, sizeof(a)))
                return -EFAULT;
            return 0;
        }
        case NDS_IOCTL_UNMAP: {
            struct npu_nds_unmap_args a;
            if (copy_from_user(&a, uarg, sizeof(a)))
                return -EFAULT;
            return nds_do_unmap(&a);
        }
        case NDS_IOCTL_READ:
        case NDS_IOCTL_WRITE: {
            struct npu_nds_io_args a;
            int r;
            if (copy_from_user(&a, uarg, sizeof(a)))
                return -EFAULT;
            a.opcode = (cmd == NDS_IOCTL_READ) ? NDS_OPCODE_READ : NDS_OPCODE_WRITE;
            a.done = 0;
            a.ret = 0;
            r = nds_do_io(&a);
            if (copy_to_user(uarg, &a, sizeof(a)))
                return -EFAULT;
            return r;
        }
        default:
            return -ENOTTY;
    }
}

static int nds_open(struct inode *inode, struct file *filp)
{
    return 0;
}

static int nds_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static const struct file_operations nds_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = nds_ioctl,
    .open = nds_open,
    .release = nds_release,
};

static struct miscdevice nds_miscdev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = NDS_DEV_NAME,
    .fops = &nds_fops,
    .mode = 0666,
};

/* ============================================================================
 *  Module init/exit
 * ========================================================================== */

static int __init nds_init(void)
{
    int ret;

    ret = misc_register(&nds_miscdev);
    if (ret) {
        nds_err("misc_register failed: %d\n", ret);
        return ret;
    }

    /*
     * Look up nvme_alloc_request via __symbol_get.  The symbol is provided
     * by the NVMe core driver; if absent we still load but READ/WRITE
     * return -ENOSYS.
     */
    nds_nvme_alloc_request_fn = (nvme_alloc_request_fn_t)__symbol_get(NDS_NVME_ALLOC_REQUEST_SYM);
    if (!nds_nvme_alloc_request_fn) {
        nds_warn("symbol %s not found — passthrough ioctls disabled\n", NDS_NVME_ALLOC_REQUEST_SYM);
    }

    /*
     * Look up blk_abort_request for the drain-timeout cleanup path.
     * Optional — if absent we log but still function (abandoned batches
     * are eventually freed by the block layer's own timeout).
     */
    nds_blk_abort_request_fn = (blk_abort_request_fn_t)__symbol_get(NDS_BLK_ABORT_REQUEST_SYM);
    if (!nds_blk_abort_request_fn) {
        nds_warn("symbol %s not found — timeout abort disabled\n", NDS_BLK_ABORT_REQUEST_SYM);
    }

    /*
     * Register the tracepoint probe on nvme_setup_cmd so we can retype
     * the SGL of every RQF_NVME_PT request to NVME_CMD_SGL_METABUF.
     */
    ret = nds_register_tracepoint();
    if (ret == 0)
        nds_pt_enabled = true;

    nds_info("NPU Direct Storage enabler loaded -> /dev/%s (pt=%s)\n", NDS_DEV_NAME, nds_pt_enabled ? "on" : "off");
    nds_info("pins NPU memory via hal_kernel_p2p_get_pages\n");
    return 0;
}

static void __exit nds_exit(void)
{
    struct nds_mgroup *mg, *tmp;

    if (nds_pt_enabled) {
        nds_unregister_tracepoint();
        nds_pt_enabled = false;
    }

    if (nds_nvme_alloc_request_fn) {
        __symbol_put(NDS_NVME_ALLOC_REQUEST_SYM);
        nds_nvme_alloc_request_fn = NULL;
    }

    if (nds_blk_abort_request_fn) {
        __symbol_put(NDS_BLK_ABORT_REQUEST_SYM);
        nds_blk_abort_request_fn = NULL;
    }

    misc_deregister(&nds_miscdev);

    mutex_lock(&nds_mgroup_mutex);
    list_for_each_entry_safe(mg, tmp, &nds_mgroup_list, list)
    {
        list_del(&mg->list);
        mg->state = NDS_MG_TERMINATED;
        if (!mg->pinned_ref_dropped) {
            mg->pinned_ref_dropped = true;
            /* drop pin ref + exists ref */
            nds_mgroup_put(mg);
            nds_mgroup_put(mg);
        } else {
            /* pin ref already dropped by callback; drop exists ref only */
            nds_mgroup_put(mg);
        }
    }
    mutex_unlock(&nds_mgroup_mutex);

    nds_info("unloaded\n");
}

module_init(nds_init);
module_exit(nds_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Huawei Technologies Co., Ltd.");
MODULE_DESCRIPTION("NPU Direct Storage (NDS) enabler demo, NVMe passthrough path");
