/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 and only version 2 as
 * published by the Free Software Foundation.
 *
 * NPU Direct Storage (NDS) kernel-side enabler demo.
 *
 * This module is the Ascend NPU counterpart of NVIDIA's nvidia-fs.ko
 * (GPUDirect Storage). It exposes /dev/npu-nds and a small ioctl contract so
 * that user space can:
 *   1. pin NPU device memory (hal_kernel_p2p_get_pages)
 *   2. allocate 4K "shadow pages" that stand in for the NPU pages on the
 *      block-layer O_DIRECT path (page->mapping == NULL is the GPU/NPU page
 *      marker)
 *   3. issue O_DIRECT read/write between a file and the shadow buffer
 *
 * Zero-copy P2P DMA path (vendor hooks):
 *   The module registers itself with the (patched) NVMe driver via the
 *   nvme_v1_register_nvfs_dma_ops symbol, providing a struct npufs_dma_rw_ops
 *   with 6 hook functions.  When the NVMe driver builds the DMA sglist for a
 *   request containing shadow pages, it calls our hooks:
 *     - npu_blk_rq_map_sg     builds sglist from NPU HBM physical pages
 *     - npu_dma_map_sg_attrs   dma_map_resource NPU PAs → BAR bus addresses
 *   The NVMe controller then DMAs directly to/from NPU HBM — zero-copy,
 *   no drvMemcpy in the data path.
 *
 * If the NVMe driver is not patched (hooks not found), the module falls back
 * to bounce-buffer mode: the NVMe controller DMAs to the shadow pages in
 * system RAM, and user space must copy to/from NPU HBM.
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
#include <linux/uio.h>
#include <linux/bio.h>
#include <linux/blkdev.h>
#include <linux/blk-mq.h>
#include <linux/kallsyms.h>

#include "ascend_kernel_hal.h"
#include "npu_nds.h"
#include "npu_nds_hook.h"

#define NDS_TAG "npu_nds"

#define nds_info(fmt, ...) pr_info(NDS_TAG ": " fmt, ##__VA_ARGS__)
#define nds_warn(fmt, ...) pr_warn(NDS_TAG ": " fmt, ##__VA_ARGS__)
#define nds_err(fmt, ...) pr_err(NDS_TAG ": " fmt, ##__VA_ARGS__)

#define NDS_IO_ERR (-EIO)
#define NDS_BAD_REQ (-ENODATA)

enum nds_mgroup_state {
    NDS_MG_FREE = 0,
    NDS_MG_ACTIVE,
    NDS_MG_TERMINATED,
};

/*
 * mgroup: one shadow buffer + its pinned NPU page table.
 * Mirrors npufs_io_mgroup (npufs-mmap.h). For the demo we keep a plain
 * mutex-protected list instead of the RCU hash table used by nvidia-fs.
 */
struct nds_mgroup {
    struct list_head list;
    u64 mgroup_id; /* global key, returned to user */
    u64 npu_va;
    u64 size; /* user requested size */
    u32 npu_page_size;
    u32 npu_page_num;
    struct p2p_page_table *page_table;

    /* shadow pages: 4K each, page->mapping == NULL marker (kept NULL) */
    struct page **shadow_pages;
    u32 nshadow_pages;
    u64 shadow_size;
    u64 mmap_offset; /* offset handed back to user for mmap */

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
static u64 nds_next_mmap_offset; /* protected by nds_mgroup_mutex */

static void nds_mgroup_release(struct kref *ref)
{
    struct nds_mgroup *mg = container_of(ref, struct nds_mgroup, refcount);
    u32 i;

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

    if (mg->shadow_pages) {
        for (i = 0; i < mg->nshadow_pages; i++) {
            if (mg->shadow_pages[i])
                put_page(mg->shadow_pages[i]);
        }
        kfree(mg->shadow_pages);
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
 * nds_mgroup_from_page — recover the mgroup from a shadow page.
 *
 * Mirrors nvidia-fs __npufs_mgroup_from_page (npufs-mmap.c:1102):
 *   1. page->mapping must be NULL (shadow/NPU page marker)
 *   2. extract base_index = page->index >> NDS_MAX_SHADOW_PAGES_ORDER
 *   3. base_index must be >= NDS_MIN_BASE_INDEX (1UL << 32)
 *   4. look up mgroup by base_index (== mgroup_id)
 *
 * Returns the mgroup with an elevated refcount, or NULL if the page is
 * not an NPU shadow page.
 */
static struct nds_mgroup *nds_mgroup_from_page(struct page *page)
{
    unsigned long base_index;

    if (!page || page->mapping != NULL)
        return NULL;

    base_index = (page->index >> NDS_MAX_SHADOW_PAGES_ORDER);
    if (base_index < NDS_MIN_BASE_INDEX)
        return NULL;

    return nds_mgroup_get((u64)base_index);
}

/*
 * nds_get_npu_pa — NPU HBM physical address for a shadow page.
 *
 * Shadow pages are 4K; NPU pages may be larger (e.g. 2 MB huge pages).
 * rel_index is the 4K-page offset within the mgroup.
 */
static u64 nds_get_npu_pa(struct nds_mgroup *mg, unsigned long rel_index)
{
    u64 offset = (u64)rel_index * PAGE_SIZE;
    u32 npu_page_idx;
    u64 page_offset;

    if (!mg->page_table || !mg->page_table->pages_info || !mg->npu_page_size)
        return 0;

    npu_page_idx = (u32)(offset / mg->npu_page_size);
    page_offset = offset % mg->npu_page_size;

    if (npu_page_idx >= mg->npu_page_num)
        return 0;

    return mg->page_table->pages_info[npu_page_idx].pa + page_offset;
}

/*
 * nds_ensure_dma_mapped — lazily create the peer DMA mapping cache.
 *
 * Maps each NPU page PA via dma_map_resource against the peer (NVMe)
 * device, caching the result in mg->sgt.  This is the NPU equivalent
 * of nvidia-fs npufs_get_p2p_dma_mapping (npufs-core.c:487).
 * Demo limitation: single peer only.
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
 * nds_get_dma_addr — cached P2P DMA bus address for a shadow page.
 *
 * Equivalent of nvidia-fs npufs_get_dma (npufs-core.c:561) which returns
 * dma_mapping->dma_addresses[gpu_page_index].
 * Returns the bus address the NVMe controller should DMA to.
 */
static dma_addr_t nds_get_dma_addr(struct nds_mgroup *mg, unsigned long rel_index)
{
    u64 offset = (u64)rel_index * PAGE_SIZE;
    u32 npu_page_idx = (u32)(offset / mg->npu_page_size);
    u64 page_offset = offset % mg->npu_page_size;
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

static int nds_alloc_shadow_pages(struct nds_mgroup *mg)
{
    u32 n = (u32)(mg->shadow_size / NDS_BLOCK_SIZE);
    u32 i;

    mg->shadow_pages = kcalloc(n, sizeof(struct page *), GFP_KERNEL);
    if (!mg->shadow_pages)
        return -ENOMEM;
    /* set count before the loop so release can free partial allocations
     * (kcalloc zeroed the array, so NULL entries are skipped) */
    mg->nshadow_pages = n;

    for (i = 0; i < n; i++) {
        struct page *p = alloc_page(GFP_USER | __GFP_ZERO);
        if (!p)
            return -ENOMEM;
        /*
         * page->mapping == NULL is the nvidia-fs GPU/NPU-page marker used by
         * the block-layer fast path; we keep it NULL on purpose.
         *
         * page->index encodes (base_index << NDS_MAX_SHADOW_PAGES_ORDER) | offset
         * so the hook can recover the mgroup (via base_index) and the
         * relative page index within it.
         */
        p->mapping = NULL;
        p->index = (mg->mgroup_id << NDS_MAX_SHADOW_PAGES_ORDER) + i;
        mg->shadow_pages[i] = p;
    }
    return 0;
}

static int nds_mgroup_pin_pages(struct nds_mgroup *mg)
{
    int ret;

    ret = hal_kernel_p2p_get_pages(mg->npu_va, mg->size, nds_p2p_free_callback, mg, &mg->page_table);
    if (ret) {
        nds_err("hal_kernel_p2p_get_pages failed: %d\n", ret);
        return ret;
    }
    if (!mg->page_table || !mg->page_table->pages_info || !mg->page_table->page_num) {
        nds_err("invalid page_table from hal_kernel_p2p_get_pages\n");
        return -ENODATA;
    }
    mg->npu_page_size = mg->page_table->page_size;
    mg->npu_page_num = mg->page_table->page_num;
    return 0;
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
    kref_get(&mg->refcount);
    mg->npu_va = args->npu_va;
    mg->size = args->size;
    mg->shadow_size = args->size;
    mg->mgroup_id = (u64)atomic64_inc_return(&nds_base_index);
    mg->state = NDS_MG_ACTIVE;

    ret = nds_mgroup_pin_pages(mg);
    if (ret) {
        nds_mgroup_put(mg);
        nds_mgroup_put(mg);
        return ret;
    }

    ret = nds_alloc_shadow_pages(mg);
    if (ret) {
        nds_mgroup_put(mg);
        nds_mgroup_put(mg);
        return ret;
    }

    mutex_lock(&nds_mgroup_mutex);
    mg->mmap_offset = nds_next_mmap_offset;
    nds_next_mmap_offset += mg->shadow_size;
    list_add_tail(&mg->list, &nds_mgroup_list);
    mutex_unlock(&nds_mgroup_mutex);

    args->mgroup_id = mg->mgroup_id;
    args->mmap_offset = mg->mmap_offset;
    args->mmap_size = mg->shadow_size;
    args->npu_page_size = mg->npu_page_size;
    args->npu_page_num = mg->npu_page_num;

    nds_info("map: npu_va=0x%llx size=%llu npu_pg=%u/%u shadow=%u -> mgid=%llu\n", mg->npu_va, mg->size,
             mg->npu_page_num, mg->npu_page_size, mg->nshadow_pages, mg->mgroup_id);
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

/*
 * Build a BVEC iov_iter over the shadow pages and drive O_DIRECT through
 * the file's read_iter / write_iter.
 *
 * This mirrors nvidia-fs npufs_direct_io: the shadow pages (page->mapping
 * == NULL) are placed directly into the bio.  When the NVMe driver builds
 * the DMA sglist it calls our registered hooks (npu_blk_rq_map_sg /
 * npu_dma_map_sg_attrs) which replace the shadow page PAs with NPU HBM P2P
 * DMA addresses — achieving zero-copy P2P.
 *
 * Using a BVEC iov_iter (instead of kmap + kernel_read) is essential:
 *   - kmap'd addresses are in the kernel map, not the user address space,
 *     so get_user_pages (needed by O_DIRECT) would fail on them.
 *   - A BVEC iterator hands the pages directly to the block layer without
 *     get_user_pages, so the shadow pages end up in the bio.
 */
static ssize_t nds_shadow_io_iter(struct nds_mgroup *mg, struct file *file, u64 off, u64 size, int write)
{
    struct bio_vec *bvec;
    u32 npages = mg->nshadow_pages;
    u32 i;
    u64 remain = min(size, mg->shadow_size);
    struct iov_iter iter;
    struct kiocb kiocb;
    ssize_t ret;

    bvec = kmalloc_array(npages, sizeof(*bvec), GFP_KERNEL);
    if (!bvec)
        return -ENOMEM;

    for (i = 0; i < npages; i++) {
        bvec[i].bv_page = mg->shadow_pages[i];
        bvec[i].bv_offset = 0;
        bvec[i].bv_len = PAGE_SIZE;
    }

    iov_iter_bvec(&iter, write ? WRITE : READ, bvec, npages, remain);
    init_sync_kiocb(&kiocb, file);
    kiocb.ki_pos = off;

    if (write) {
        if (!file->f_op->write_iter) {
            kfree(bvec);
            return -EINVAL;
        }
        ret = file->f_op->write_iter(&kiocb, &iter);
    } else {
        if (!file->f_op->read_iter) {
            kfree(bvec);
            return -EINVAL;
        }
        ret = file->f_op->read_iter(&kiocb, &iter);
    }

    kfree(bvec);
    if (ret < 0)
        nds_err("io: %s off=%llu len=%llu failed: %zd\n", write ? "write" : "read", (u64)off, remain, ret);
    return ret;
}

static int nds_do_io(struct npu_nds_io_args *args)
{
    struct nds_mgroup *mg;
    struct fd f;
    ssize_t n;
    int ret = 0;

    mg = nds_mgroup_get(args->mgroup_id);
    if (!mg) {
        nds_err("io: mgroup %llu not found/terminated\n", args->mgroup_id);
        return -ENOENT;
    }

    f = fdget(args->fd);
    if (!f.file) {
        ret = -EBADF;
        goto out_put;
    }
    if (args->opcode == NDS_OPCODE_WRITE && !(f.file->f_mode & FMODE_WRITE)) {
        ret = -EBADF;
        goto out_fd;
    }
    if (args->opcode == NDS_OPCODE_READ && !(f.file->f_mode & FMODE_READ)) {
        ret = -EBADF;
        goto out_fd;
    }

    mutex_lock(&mg->lock);
    if (mg->state != NDS_MG_ACTIVE) {
        mutex_unlock(&mg->lock);
        nds_err("io: mgroup %llu terminated\n", mg->mgroup_id);
        ret = -ENODEV;
        goto out_fd;
    }

    n = nds_shadow_io_iter(mg, f.file, args->file_offset, args->size, args->opcode == NDS_OPCODE_WRITE);
    mutex_unlock(&mg->lock);

    args->done = n > 0 ? (u64)n : 0;
    args->ret = n < 0 ? (int)n : 0;
    if (n < 0)
        ret = (int)n;

out_fd:
    fdput(f);
out_put:
    nds_mgroup_put(mg);
    return ret;
}

/*
 * Demonstrate the peer-specific P2P DMA mapping that nvidia-fs caches per
 * (mgroup, peer). We locate the NVMe pci_dev (PCI base class STORAGE = 0x01),
 * build an sglist from the pinned NPU physical pages and dma_map_resource each
 * page against that peer device.  The resulting IOVA is what the NVMe
 * controller would DMA to in zero-copy GDS.
 */
static struct device *nds_peer_device(u32 peer_devid)
{
    struct pci_dev *pdev = NULL;
    /*
     * Best-effort peer discovery: walk PCI devices looking for an NVMe
     * controller (storage class).  dma_map_resource must be issued against the
     * NVMe controller so the resulting bus address is valid for that
     * controller's DMA engine.  A production driver maintains the gpu2peer
     * distance matrix (npufs-pci.c).
     */
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

static void nds_vma_close(struct vm_area_struct *vma)
{
    struct nds_mgroup *mg = vma->vm_private_data;

    if (!mg)
        return;

    nds_info("vma_close: mgid=%llu vm_start=0x%lx vm_end=0x%lx\n", mg->mgroup_id, vma->vm_start, vma->vm_end);
    /*
     * Drop the "mmap lifetime" reference taken in nds_mmap.
     * Mirrors npufs_vma_close which calls npufs_mgroup_put(mgroup).
     * The mgroup itself is torn down by NDS_IOCTL_UNMAP (or module exit);
     * here we only release the mmap ref so the mgroup can eventually be freed.
     */
    nds_mgroup_put(mg);
    vma->vm_private_data = NULL;
}

static void nds_vma_open(struct vm_area_struct *vma)
{
    WARN_ON_ONCE(1);
}

static int nds_vma_split(struct vm_area_struct *vma, unsigned long addr)
{
    nds_warn("VMA split rejected\n");
    return -ENOMEM;
}

static int nds_vma_mremap(struct vm_area_struct *vma)
{
    nds_warn("VMA mremap rejected\n");
    return -ENOMEM;
}

static vm_fault_t nds_vma_fault(struct vm_fault *vmf)
{
    WARN_ON_ONCE(1);
    return 0;
}

static const struct vm_operations_struct nds_vm_ops = {
    .open = nds_vma_open,
    .close = nds_vma_close,
    .split = nds_vma_split,
    .mremap = nds_vma_mremap,
    .fault = nds_vma_fault,
};

static int nds_mmap(struct file *filp, struct vm_area_struct *vma)
{
    struct nds_mgroup *mg;
    u64 off = vma->vm_pgoff << PAGE_SHIFT;
    u64 vsize = vma->vm_end - vma->vm_start;
    u32 i;
    int ret;

    /*
     * VMA flag checks — mirror nvidia-fs npufs_mgroup_mmap_internal:
     *   VM_SHARED required (vm_insert_page needs it for CoW-free mapping)
     *   VM_EXEC forbidden (shadow buffer is data only)
     */
    if (!(vma->vm_flags & VM_SHARED)) {
        nds_err("mmap: VM_SHARED required\n");
        return -EINVAL;
    }
    if (vma->vm_flags & VM_EXEC) {
        nds_err("mmap: VM_EXEC not allowed\n");
        return -EINVAL;
    }

    mutex_lock(&nds_mgroup_mutex);
    list_for_each_entry(mg, &nds_mgroup_list, list)
    {
        if (mg->mmap_offset == off)
            goto found;
    }
    mutex_unlock(&nds_mgroup_mutex);
    return -EINVAL;

found:
    kref_get(&mg->refcount); /* lookup ref (dropped after VMA setup) */
    kref_get(&mg->refcount); /* mmap lifetime ref (dropped in vma_close) */
    mutex_unlock(&nds_mgroup_mutex);

    if (vsize > mg->shadow_size) {
        ret = -EINVAL;
        goto out_drop_mmap_ref;
    }

    /*
     * VM_MIXEDMAP is required by vm_insert_page (allows struct page backed
     * pages in the VMA alongside or instead of PFN mappings).
     * VM_DONTEXPAND/VM_DONTDUMP/VM_DONTCOPY match nvidia-fs to prevent
     * mremap expansion, core dump inclusion, and fork copy.
     */
    vma->vm_flags |= VM_MIXEDMAP | VM_DONTEXPAND | VM_DONTDUMP | VM_DONTCOPY;

    /*
     * Insert shadow pages one by one via vm_insert_page (not remap_pfn_range).
     * vm_insert_page is the correct API for struct page backed memory:
     *   - properly manages page reference counting (takes a ref per page)
     *   - page->mapping == NULL marker works correctly with vm_insert_page
     *   - mirrors nvidia-fs npufs_mgroup_mmap_internal which uses vm_insert_page
     * remap_pfn_range is for raw PFN/IOMEM without struct page backing.
     */
    for (i = 0; i < mg->nshadow_pages; i++) {
        ret = vm_insert_page(vma, vma->vm_start + i * PAGE_SIZE, mg->shadow_pages[i]);
        if (ret) {
            nds_err("mmap: vm_insert_page failed @ %u: %d\n", i, ret);
            /*
             * Already-inserted pages (0..i-1) are cleaned up by the kernel
             * when mmap returns error (VMA teardown unmaps them and drops
             * page refs).  Since vm_ops is not set yet, vma_close won't fire.
             */
            goto out_drop_mmap_ref;
        }
    }

    /* Set VMA ops AFTER all pages inserted so vma_close is only called on
     * a fully populated VMA. */
    vma->vm_ops = &nds_vm_ops;
    vma->vm_private_data = mg;

    nds_mgroup_put(mg); /* drop lookup ref; mmap ref stays for vma_close */
    return 0;

out_drop_mmap_ref:
    nds_mgroup_put(mg); /* drop mmap ref (vma_close won't be called) */
    nds_mgroup_put(mg); /* drop lookup ref */
    return ret;
}

static const struct file_operations nds_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = nds_ioctl,
    .open = nds_open,
    .release = nds_release,
    .mmap = nds_mmap,
};

static struct miscdevice nds_miscdev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = NDS_DEV_NAME,
    .fops = &nds_fops,
    .mode = 0666,
};

/* ============================================================================
 * NVMe vendor hooks — the zero-copy P2P DMA path.
 *
 * These functions are registered with the (patched) NVMe driver via the
 * nvme_v1_register_nvfs_dma_ops symbol.  When the NVMe driver builds the
 * DMA sglist for a request, it calls our hooks instead of the default
 * blk_rq_map_sg / dma_map_sg.  We identify shadow pages (page->mapping ==
 * NULL) and replace their CPU physical addresses with NPU HBM P2P DMA
 * addresses, so the NVMe controller DMAs directly to/from NPU HBM.
 *
 * Data flow (read path):
 *   file->f_op->read_iter(DIO)
 *     → block layer builds bio from shadow pages (page->mapping == NULL)
 *     → NVMe driver calls npu_blk_rq_map_sg         ← hook 1
 *         identify shadow pages, build sglist with shadow page as sg_page,
 *         coalesce contiguous NPU physical pages
 *     → NVMe driver calls npu_dma_map_sg_attrs       ← hook 2
 *         for each sg entry: find mgroup via sg_page,
 *         dma_map_resource the NPU PA against the NVMe device,
 *         set sg_dma_address = NPU BAR bus address
 *     → NVMe controller DMAs to NPU HBM             ← zero-copy!
 * ========================================================================== */

/*
 * Hook 1: build sglist from the request, using NPU physical addresses for
 * shadow pages.  Mirrors nvidia-fs npufs_nvme_blk_rq_map_sg (npufs-dma.c:531).
 *
 * Returns:
 *   >0  number of sg entries (request contains NPU pages)
 *   0   no NPU pages found (fall through to default blk_rq_map_sg)
 *  <0   error (mixed CPU/NPU pages, integrity, etc.)
 */
static int npu_blk_new_segment(struct scatterlist **sg, struct scatterlist *sglist, struct bio_vec *bvec, int *nsegs)
{
    (*nsegs)++;
    if (*nsegs == 1)
        *sg = sglist;
    else if (!sg_is_last(*sg))
        *sg = sg_next(*sg);
    else {
        nds_err("blk_rq_map_sg: sglist overflow (nsegs=%d)\n", *nsegs);
        return NDS_IO_ERR;
    }
    sg_set_page(*sg, bvec->bv_page, bvec->bv_len, bvec->bv_offset);
    return 0;
}

static int nds_blk_check_page(struct bio_vec *bvec, bool *found_cpu, bool *found_npu, struct nds_mgroup **mg_out)
{
    struct nds_mgroup *mg = nds_mgroup_from_page(bvec->bv_page);
    bool is_npu = (mg != NULL);

    if (is_npu) {
        *found_npu = true;
    } else {
        *found_cpu = true;
    }
    if (*found_npu && *found_cpu) {
        nds_err("blk_rq_map_sg: mixed CPU/NPU pages rejected\n");
        if (mg)
            nds_mgroup_put(mg);
        return NDS_IO_ERR;
    }

    if (*found_cpu) {
        if (mg)
            nds_mgroup_put(mg);
        return 1;
    }

    *mg_out = mg;
    return 0;
}

static int npu_blk_rq_map_sg(struct request_queue *q, struct request *req, struct scatterlist *sglist)
{
    int nsegs = 0, ret;
    bool found_cpu = false, found_npu = false;
    struct req_iterator iter;
    struct bio_vec bvec;
    struct scatterlist *sg = NULL;
    u64 prev_pa = 0, curr_pa = 0;

    if (!sglist)
        return NDS_IO_ERR;

    rq_for_each_segment(bvec, req, iter)
    {
        struct nds_mgroup *mg;
        unsigned long rel_index;

        ret = nds_blk_check_page(&bvec, &found_cpu, &found_npu, &mg);
        if (ret < 0)
            return NDS_IO_ERR;
        if (ret > 0)
            continue;

        rel_index = bvec.bv_page->index % NDS_MAX_SHADOW_PAGES;
        curr_pa = nds_get_npu_pa(mg, rel_index);
        nds_mgroup_put(mg);
        if (!curr_pa) {
            nds_err("blk_rq_map_sg: get_npu_pa failed for rel_index %lu\n", rel_index);
            return NDS_IO_ERR;
        }

        if (sg && prev_pa && (prev_pa + PAGE_SIZE == curr_pa)) {
            sg->length += bvec.bv_len;
            prev_pa = curr_pa;
            continue;
        }

        if (npu_blk_new_segment(&sg, sglist, &bvec, &nsegs))
            return NDS_IO_ERR;
        prev_pa = curr_pa;
    }

    if (found_npu) {
        sg_mark_end(sg);
        return nsegs;
    }
    return 0;
}

/*
 * Hook 2: DMA-map the sglist built by npu_blk_rq_map_sg.
 * Mirrors nvidia-fs npufs_dma_map_sg_attrs_internal (npufs-dma.c:1250).
 *
 * For each sg entry: find mgroup via sg_page, ensure the peer DMA mapping
 * is cached, and set sg_dma_address to the NPU BAR bus address.
 */
static int npu_dma_map_sg_attrs(struct device *device, struct scatterlist *sglist, int nents,
                                enum dma_data_direction dma_dir, unsigned long attrs)
{
    struct scatterlist *sg;
    int i, nr_npu = 0;

    if (nents == 0)
        return NDS_IO_ERR;

    for_each_sg(sglist, sg, nents, i)
    {
        struct nds_mgroup *mg;
        unsigned long rel_index;
        dma_addr_t dma_addr;
        int ret;

        mg = nds_mgroup_from_page(sg_page(sg));
        if (!mg) {
            nds_err("dma_map_sg: page not an NPU shadow page\n");
            return NDS_IO_ERR;
        }

        ret = nds_ensure_dma_mapped(mg, device);
        if (ret) {
            nds_err("dma_map_sg: ensure_dma_mapped failed: %d\n", ret);
            nds_mgroup_put(mg);
            return NDS_IO_ERR;
        }

        rel_index = sg_page(sg)->index % NDS_MAX_SHADOW_PAGES;
        dma_addr = nds_get_dma_addr(mg, rel_index);
        if (!dma_addr) {
            nds_err("dma_map_sg: get_dma_addr failed for rel_index %lu\n", rel_index);
            nds_mgroup_put(mg);
            return NDS_IO_ERR;
        }

        sg_dma_address(sg) = dma_addr + sg->offset;
        sg_dma_len(sg) = sg->length;
        nds_mgroup_put(mg);
        nr_npu++;
    }

    return nr_npu;
}

/*
 * Hook 3: unmap.  We keep the cached mapping, so this is a no-op.
 * Mirrors nvidia-fs npufs_dma_unmap_sg (npufs-dma.c) which calls
 * npufs_mgroup_put_dma.
 */
static int npu_dma_unmap_sg(struct device *device, struct scatterlist *sglist, int nents,
                            enum dma_data_direction dma_dir)
{
    (void)device;
    (void)sglist;
    (void)nents;
    (void)dma_dir;
    return 0;
}

/* Hook 4: is this page an NPU shadow page? */
static bool npu_is_gpu_page(struct page *page)
{
    struct nds_mgroup *mg = nds_mgroup_from_page(page);
    if (mg) {
        nds_mgroup_put(mg);
        return true;
    }
    return false;
}

/* Hook 5: NPU index (demo: single NPU, always 0) */
static unsigned int npu_gpu_index(struct page *page)
{
    (void)page;
    return 0;
}

/* Hook 6: device priority (demo: equal priority) */
static unsigned int npu_device_priority(struct device *dev, unsigned int gpu_index)
{
    (void)dev;
    (void)gpu_index;
    return 0;
}

/*
 * Ops table registered with the NVMe driver.
 * Layout must match struct npufs_dma_rw_ops (npufs-dma.h:74).
 */
static struct npufs_dma_rw_ops npu_dma_rw_ops = {
    .ft_bmap = NPU_NDS_FT_ALL,
    .npufs_blk_rq_map_sg = npu_blk_rq_map_sg,
    .npufs_dma_map_sg_attrs = npu_dma_map_sg_attrs,
    .npufs_dma_unmap_sg = npu_dma_unmap_sg,
    .npufs_is_gpu_page = npu_is_gpu_page,
    .npufs_gpu_index = npu_gpu_index,
    .npufs_device_priority = npu_device_priority,
};

/* Cached registration function pointers (via __symbol_get) */
static npufs_register_dma_ops_fn_t nds_reg_fn;
static npufs_unregister_dma_ops_fn_t nds_unreg_fn;
static bool nds_hooks_registered;
/* Symbols of the NVMe driver variant actually in use (set in init, consumed
 * by exit so __symbol_put matches the __symbol_get that succeeded). */
static const char *nds_reg_sym;
static const char *nds_unreg_sym;

static int __init nds_init(void)
{
    int ret;

    ret = misc_register(&nds_miscdev);
    if (ret) {
        nds_err("misc_register failed: %d\n", ret);
        return ret;
    }

    /*
     * Register our DMA ops with the (patched) NVMe driver.
     * Mirrors nvidia-fs probe_module_list (npufs-mod.c:40):
     *   1. __symbol_get the registration / unregistration symbols
     *   2. call reg_fn(ops) to install our hooks
     * If the NVMe driver isn't patched (symbols not found), we continue
     * without hooks — I/O falls back to bounce-buffer (shadow page DMA).
     * Try v1 symbols first, then v2 (design §4.2): different patched NVMe
     * drivers export only one of the two variants.
     */
    nds_reg_sym = NVME_V1_REGISTER_SVM;
    nds_unreg_sym = NVME_V1_UNREGISTER_SVM;
    nds_reg_fn = (npufs_register_dma_ops_fn_t)__symbol_get(NVME_V1_REGISTER_SVM);
    if (!nds_reg_fn) {
        /* v1 register symbol not present — try the v2 variant */
        nds_reg_sym = NVME_V2_REGISTER_SVM;
        nds_unreg_sym = NVME_V2_UNREGISTER_SVM;
        nds_reg_fn = (npufs_register_dma_ops_fn_t)__symbol_get(NVME_V2_REGISTER_SVM);
    }
    if (nds_reg_fn) {
        nds_unreg_fn = (npufs_unregister_dma_ops_fn_t)__symbol_get(nds_unreg_sym);
        if (!nds_unreg_fn) {
            __symbol_put(nds_reg_sym);
            nds_reg_fn = NULL;
            nds_warn("found %s but not %s — hooks disabled\n", nds_reg_sym, nds_unreg_sym);
        } else {
            ret = nds_reg_fn(&npu_dma_rw_ops);
            if (ret) {
                nds_err("NVMe hook registration failed: %d\n", ret);
                __symbol_put(nds_unreg_sym);
                __symbol_put(nds_reg_sym);
                nds_reg_fn = NULL;
                nds_unreg_fn = NULL;
                /* continue without hooks — bounce-buffer mode */
            } else {
                nds_hooks_registered = true;
                nds_info("NVMe vendor hooks registered via %s (zero-copy P2P)\n", nds_reg_sym);
            }
        }
    } else {
        nds_warn("NVMe driver not patched with npufs hooks — "
                 "bounce-buffer mode (no zero-copy)\n");
    }

    nds_info("NPU Direct Storage enabler loaded -> /dev/%s\n", NDS_DEV_NAME);
    nds_info("pins NPU memory via hal_kernel_p2p_get_pages\n");
    return 0;
}

static void __exit nds_exit(void)
{
    struct nds_mgroup *mg, *tmp;

    if (nds_hooks_registered) {
        nds_unreg_fn();
        __symbol_put(nds_unreg_sym);
        __symbol_put(nds_reg_sym);
        nds_hooks_registered = false;
        nds_info("NVMe vendor hooks unregistered\n");
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
MODULE_DESCRIPTION("NPU Direct Storage (NDS) enabler demo, Ascend counterpart of nvidia-fs");
