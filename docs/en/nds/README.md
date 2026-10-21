# NPU Direct Storage (NDS) Design Document

> Based on the two source trees in `docs/zh/nds/VFS/` and `docs/zh/nds/NVMe/`
> Reference: NVIDIA GPUDirect Storage (GDS) `nvidia-fs.ko` v2.29.4 (see Appendix C)
> Kernel module: `npu_nds.ko` (`/dev/npu-nds`)
> Userspace: client library `libnpu_file.so` + demo program `npu_nds_demo`
> **Code demos are located in `docs/zh/nds/`**

---

## 0. Document Overview

NDS has two zero-copy implementation paths that share the same HAL P2P pin / mgroup lifecycle / ioctl framework, diverging only on "how to deliver NPU HBM physical pages into the NVMe controller's DMA path":

| Path | Subdirectory | Zero-copy mechanism | Requires modifying NVMe driver |
|---|---|---|---|
| **A: VFS shadow buffer + vendor hooks** | `docs/zh/nds/VFS/` | 4K shadow pages + `npufs_dma_rw_ops` 6-hook interception of DMA mapping | **Yes** (borrows NVIDIA nvidia-fs NVMe driver patch, exporting `nvme_v1_register_nvfs_dma_ops`/`nvme_v2_register_nvfs_dma_ops`) |
| **B: NVMe passthrough + tracepoint probe** | `docs/zh/nds/NVMe/` | `FS_IOC_FIEMAP` + `nvme_alloc_request` + `nvme_setup_cmd` probe rewriting SGL type | **No** (uses only stable interfaces already in mainline) |

The client API (`npu_file_*`) and demo program contract are identical across both samples; the difference lies only in the kernel data path and the field-filling of `npu_nds_io_args`. This document merges the shared foundation and path-specific parts for side-by-side comparison. Each subdirectory has its own `README.md` describing build and run details.

---

## 1. Overview

### 1.1 The Problem

The traditional data path between NPU and storage is:

```
NVMe SSD ──DMA──> CPU bounce/page cache ──memcpy──> NPU HBM
```

This path incurs triple overhead: CPU memory bandwidth consumption, CPU cache pollution, and double-copy of data. NPU Direct Storage (NDS) aims to let the NVMe controller DMA data directly to NPU HBM via PCIe P2P:

```
NVMe SSD ──DMA(P2P)──> NPU BAR/HBM ──> NPU memory
```

### 1.2 Role of npu_nds

`npu_nds.ko` is a GPL kernel module acting as the **kernel-side enabler of NDS**. It does not implement block device or filesystem semantics itself, but rather:

1. Works with the NPU kernel driver via the HAL P2P API (`hal_kernel_p2p_get_pages` / `hal_kernel_p2p_put_pages`) to **pin (lock)** and **DMA-map** NPU HBM memory;
2. Delivers NPU HBM physical pages "into" the NVMe controller's DMA path — the two paths use different means (see §4, §5);
3. Exposes a `/dev/npu-nds` character device and a set of ioctls to userspace, wrapped by the client library `libnpu_file.so` as `npu_file_buf_register` / `npu_file_read` / `npu_file_write` etc.

### 1.3 Design Goals and Constraints

| Goal/Constraint | Path A (VFS) | Path B (NVMe) |
|---|---|---|
| Architecture validation | Replicates nvidia-fs "shadow buffer + P2P pin + vendor hooks" four-in-one | Uses FIEMAP + NVMe passthrough + tracepoint probe three-in-one |
| Zero-copy P2P | Registers vendor hooks, intercepts DMA mapping, shadow page PA → NPU BAR address | Kernel directly builds NVMe command, SGL points to NPU HBM DMA bus address |
| Graceful degradation | Falls back to bounce-buffer when NVMe driver not patched | READ/WRITE returns -ENOSYS when `nvme_alloc_request` or tracepoint missing |
| P2P DMA mapping | In-kernel `dma_map_resource` cache, no IOVA exposed | In-kernel `dma_map_resource` cache, no IOVA exposed |
| Safe reclaim | State machine + kref + `pinned_ref_dropped` flag | Same as left |
| Lib/demo layering | lib pure libc + ioctl; demo handles HAL device context/memory alloc | Same as left (lib additionally contains `FS_IOC_FIEMAP` wrapper) |
| Demo-level implementation | Mutex list instead of RCU hash; synchronous BVEC IO; no bio merging | Mutex list; async `blk_execute_rq_nowait` + batch drain (30s) + `kref` lifecycle; no extent merging |

---

## 2. Comparison of Two Zero-Copy Paths

### 2.1 Path Comparison Table

| Dimension | Path A: VFS shadow buffer | Path B: NVMe passthrough |
|---|---|---|
| **Zero-copy implementation** | Shadow pages + `npufs_dma_rw_ops` 6-hook intercepting `blk_rq_map_sg`/`dma_map_sg_attrs` | `nvme_alloc_request` + `RQF_NVME_PT` + `nvme_setup_cmd` tracepoint probe rewriting SGL type |
| **File layout source** | Implicitly obtained via VFS O_DIRECT path | Explicitly queried by userspace `FS_IOC_FIEMAP` |
| **Kernel IO path** | Goes through VFS `file->f_op->read_iter/write_iter` (BVEC iov_iter) | Bypasses VFS, directly `blk_execute_rq_nowait` |
| **CPU memory usage** | Batch of 4K `alloc_page` shadow pages per mgroup | No shadow pages |
| **mmap to userspace** | Required (`vm_insert_page` shadow pages) | Not required |
| **NVMe driver modification** | Needs to export `nvme_v1_register_nvfs_dma_ops`/`nvme_v2_register_nvfs_dma_ops` symbols | Not needed (mainline already exports `nvme_alloc_request` + `nvme_setup_cmd` tracepoint) |
| **Degradation behavior** | bounce-buffer (data lands in shadow page, `drvMemcpy` pushes to HBM) | READ/WRITE returns -ENOSYS, MAP still usable |
| **IO model** | Synchronous BVEC + `read_iter/write_iter` | Async submit + drain: `blk_execute_rq_nowait` + `wait_for_completion_timeout` (30s) + `blk_abort_request` (1s grace) + `kref` batch lifecycle |
| **NPU page size handling** | Shadow page fixed 4K, NPU may be 2MB huge page, needs `npu_page_size` conversion | SGL descriptor by FIEMAP extent length, no page size conversion |
| **nsid** | Implicitly determined by VFS/block layer | Hardcoded to 1 (demo simplification) |
| **Extent merging** | Block layer bio merging | No merging, one NVMe command per FIEMAP extent |

### 2.2 Path Selection Decision

- **Choose Path A when**: the NVMe driver is already patched with `npufs_dma_ops`; you need to replicate the exact semantics of nvidia-fs; you need to intercept on the VFS O_DIRECT path (e.g., transparent compatibility with ext4/xfs etc.).
- **Choose Path B when**: you cannot modify the mainline NVMe driver; you want truly zero CPU memory usage; you accept userspace explicitly querying FIEMAP; the mainline stable interfaces `nvme_alloc_request` + tracepoint suffice.

> Path B is the **recommended path**: no need to modify NVMe driver source, stable mainline interfaces, no shadow-page side effects.
> Path A is retained for comparison, facilitating line-by-line cross-checking against nvidia-fs source.

---

## 3. Shared Design Foundation

### 3.1 HAL P2P Pin Interface (corresponds to NVIDIA P2P API)

nvidia-fs relies on NVIDIA's proprietary P2P API (`nvidia_p2p_get_pages` /
`nvidia_p2p_dma_map_pages`), exported via `nvidia.ko`. npu_nds uses the HAL kernel-mode interface correspondingly:

| NVIDIA P2P API | HAL kernel interface | Description |
|---|---|---|
| `nvidia_p2p_get_pages_persistent` | `hal_kernel_p2p_get_pages` | Pins HBM physical pages corresponding to an NPU virtual address range |
| `nvidia_p2p_put_pages_persistent` | `hal_kernel_p2p_put_pages` | Paired release |
| `nvidia_p2p_page_table_t` | `struct p2p_page_table` | Physical page descriptor; `pages_info[].pa` is the bus address source |
| `nvidia_p2p_dma_map_pages` | `dma_map_resource` (Linux DMA API) | Establishes P2P DMA mapping for a peer PCI device |
| `nvidia_p2p_free_callback` | `nds_p2p_free_callback` (HAL callback) | Notifies driver that NPU memory is about to be freed |

**Key HAL contract constraint**: `hal_kernel_p2p_put_pages` **must not** be called from the `free_callback` context (deadlock risk); it must be called after the callback returns, from another context (e.g., UNMAP ioctl). This is consistent with the NVIDIA P2P API callback constraint; the driver handles it via state machine + `pinned_ref_dropped` flag.

Signature (`ascend_kernel_hal.h`):
```c
int hal_kernel_p2p_get_pages(u64 va, u64 len,
                             void (*free_callback)(void *data),
                             void *data,
                             struct p2p_page_table **page_table);
int hal_kernel_p2p_put_pages(struct p2p_page_table *page_table);
```

### 3.2 mgroup Data Structure and Lifecycle

Both paths share `struct nds_mgroup` as the carrier for pin state + DMA cache + state machine:

```
struct nds_mgroup {
    list node               (nds_mgroup_list mutex list)
    kref                    (object ref + pin ref)
    state                   (FREE / ACTIVE / TERMINATED)
    pinned_ref_dropped      (whether free_callback has dropped pin ref)
    p2p_page_table *pt       (NPU physical page descriptor returned by HAL)
    peer_device *dma_dev    (P2P mapping peer, cached)
    sg_table sgt            (DMA mapping cache, single per-mgroup)
    dma_mapped              (cache valid flag)
    lock                    (protects state/cache)
    id                      (atomic64_inc_return key, >= 1<<32)
}
```

Path A additional fields: `shadow_pages[]` + `page_table` encoding;
Path B needs no shadow pages; mgroup directly holds `p2p_page_table`.

### 3.3 State Machine and Safe Reclaim

NPU HBM memory can be freed at any time (e.g., process exit or memory reclaim), at which point in-flight IO may still exist. The driver coordinates via state machine + reference counting:

```
mgroup states:
  NDS_MG_FREE ──MAP──> NDS_MG_ACTIVE ──UNMAP/free_callback──> NDS_MG_TERMINATED ──ref=0──> kfree
```

Reference counting design (kref):
- On `kref_init`, ref=1 (object existence ref);
- `kref_get` adds +1 at MAP (pinning lifetime ref), ensuring mgroup is not freed when free_callback fires;
- `nds_p2p_free_callback` sets `pinned_ref_dropped = true` and `kref_put` once (safe, no double-drop);
- `NDS_IOCTL_UNMAP` removes from list, then if `pinned_ref_dropped == false` put twice (pin ref + object ref), otherwise put once (object ref only);
- When ref reaches zero, `nds_mgroup_release` performs real resource reclaim: deregister DMA mapping, (Path A) free shadow pages, call `hal_kernel_p2p_put_pages`, kfree.

Three reclaim paths converge on the state machine:
1. **HAL free_callback** (`nds_p2p_free_callback`): NPU driver notifies memory is about to be freed;
2. **UNMAP ioctl** (user `npu_file_buf_deregister`): normal deregistration;
3. **module exit** (`nds_exit`): force cleanup of residual mgroups on module unload.

### 3.4 Client Library and Demo Layering

```
┌─────────────────────────────────────────────────────┐
│  demo (npu_nds_demo)                                │
│  ├─ HAL userspace API:                              │
│  │   TsdOpen / drvMemDeviceOpen  (device context)   │
│  │   halMemAlloc / halMemFree     (HBM memory alloc)│
│  │   drvMemcpy                    (D2H verify copy) │
│  └─ NDS client library API:                          │
│      npu_file_open / buf_register / read / write ... │
├─────────────────────────────────────────────────────┤
│  lib (libnpu_file.so)                               │
│  depends only on libc + Linux UAPI + NDS kernel ioctl│
│  ├─ open/close /dev/npu-nds                         │
│  ├─ ioctl(MAP/READ/WRITE/UNMAP)                    │
│  └─ Path B extra: npu_file_fiemap_query (FS_IOC_FIEMAP)│
├─────────────────────────────────────────────────────┤
│  kernel (npu_nds.ko)                                │
│  ├─ HAL kernel: hal_kernel_p2p_get_pages/put_pages  │
│  ├─ Path A: shadow page + O_DIRECT BVEC + 6 NVMe hooks│
│  ├─ Path B: nvme_alloc_request + tracepoint probe   │
│  ├─ DMA mapping cache: dma_map_resource + sglist    │
│  └─ state machine + kref + free_callback            │
└─────────────────────────────────────────────────────┘
```

The client library deliberately avoids depending on HAL/TSD APIs, making it independently reusable — any scenario needing `/dev/npu-nds` ioctl wrapping can link `libnpu_file.so` without pulling in HAL's transitive dependencies.

---

## 4. Path A: VFS Shadow Buffer + Vendor Hooks

> Source code is in `docs/zh/nds/VFS/`. This section describes its path-specific parts; shared foundation see §3.

### 4.1 Shadow Buffer Indirection Layer

**Core contradiction**: the block layer and filesystem O_DIRECT path assume that the `struct page` carried by a bio is a CPU-accessible regular memory page; whereas NPU HBM memory is exposed via BAR and is not an addressable object within the `struct page` system.

**Solution approach**: the driver constructs a "shadow buffer" as an intermediary:

- When userspace `npu_file_buf_register` calls `NDS_IOCTL_MAP`, the driver calls `hal_kernel_p2p_get_pages` to pin NPU HBM pages, obtaining `struct p2p_page_table` (containing the `pages_info[].pa` physical address array);
- The driver allocates a batch of **4K shadow pages** via `alloc_page(GFP_USER | __GFP_ZERO)` (`npu_page_size / 4096` shadow pages per NPU page);
- Each shadow page's `page->index` is encoded as `(mgroup_id << NDS_MAX_SHADOW_PAGES_ORDER) + i`, where `mgroup_id` is generated by `atomic64_inc_return(&nds_base_index)` (initial value `NDS_MIN_BASE_INDEX = 1 << 32`), ensuring base_index `>= NDS_MIN_BASE_INDEX`, not overlapping with the regular page-cache index space;
- Key marker: the shadow page's `page->mapping == NULL`; the driver uses this to identify "this is an NPU shadow page" rather than a regular page-cache page in the block layer fast path — identical to nvidia-fs;
- Userspace `mmap`s `/dev/npu-nds`, mapping shadow pages into the user address space via `vm_insert_page`.

```
userspace shadow VA ─mmap─> shadow page (struct page, alloc_page)
                              │ page->index = (mgid << 12) + off
                              │ page->mapping = NULL  ◄── NPU page identification marker
                              ▼
                      nds_mgroup_from_page() ─mutex─> mgroup
                              │ base_index = page->index >> 12
                              │ (== mgroup_id, >= NDS_MIN_BASE_INDEX)
                              ▼
               mgroup->page_table (p2p_page_table)
                              │ pages_info[].pa = NPU physical address
                              ▼
               dma_map_resource() ──> NPU BAR DMA address (IOVA)
```

### 4.2 NVMe Vendor Hooks Registration Mechanism

> **Symbol source**: `nvme_v1_register_nvfs_dma_ops` / `nvme_v2_register_nvfs_dma_ops`
> are borrowed from NVIDIA nvidia-fs's NVMe driver patch. This patch adds `EXPORT_SYMBOL_GPL` to export registration/deregistration symbols in the mainline NVMe driver
> (`drivers/nvme/host/pci.c`), allowing external modules to register DMA ops hooks;
> v1/v2 correspond to interface differences across kernel versions; npu_nds reuses this patch to leverage the mature hook registration framework.

```
npu_nds.ko module load (nds_init)
  │
  ├─ __symbol_get("nvme_v1_register_nvfs_dma_ops")   (fall back to v2 on failure)
  │    └─ or __symbol_get("nvme_v2_register_nvfs_dma_ops")
  │    └─ success: NVMe driver patched, registration symbol exported
  ├─ __symbol_get("nvme_v1_unregister_nvfs_dma_ops") (fall back to v2 on failure)
  │    └─ or __symbol_get("nvme_v2_unregister_nvfs_dma_ops")
  │    └─ success: deregistration symbol obtained
  ├─ reg_fn(&npu_dma_rw_ops)    ← register ops table
  │    └─ failure: log error, continue in no-hook mode (bounce-buffer degradation)
  └─ success: nds_hooks_registered = true
```

```
npu_nds.ko module unload (nds_exit)
  │
  ├─ unreg_fn()                  ← deregister ops table
  └─ __symbol_put × 2            ← release symbol references
```

`__symbol_get` / `__symbol_put` are the kernel GPL-exported symbol lookup API, leaving no residual references at module unload. If the NVMe driver is not patched (symbol absent), `__symbol_get` returns NULL; the module continues loading but enters bounce-buffer mode (no zero-copy).

### 4.3 ops Table Structure

The layout of `struct npufs_dma_rw_ops` **must** match nvidia-fs (npufs-dma.h:74), because the NVMe driver casts the registered pointer to this type:

| Field | Type | NPU implementation | Purpose |
|---|---|---|---|
| `ft_bmap` | u64 | `NPU_NDS_FT_ALL` | Feature bitmap: declares which hooks are supported |
| `npufs_blk_rq_map_sg` | fn | `npu_blk_rq_map_sg` | Builds sglist from request, using NPU PA |
| `npufs_dma_map_sg_attrs` | fn | `npu_dma_map_sg_attrs` | DMA-maps sglist, sets NPU BAR address |
| `npufs_dma_unmap_sg` | fn | `npu_dma_unmap_sg` | no-op (cached mapping not released here) |
| `npufs_is_gpu_page` | fn | `npu_is_gpu_page` | Determines whether a page is an NPU shadow page |
| `npufs_gpu_index` | fn | `npu_gpu_index` | Returns 0 (demo: single NPU) |
| `npufs_device_priority` | fn | `npu_device_priority` | Returns 0 (demo: equal priority) |

### 4.4 Zero-Copy Data Path (read example)

```
file->f_op->read_iter(DIO, BVEC over shadow pages)
  │  block layer builds bio, shadow page page->mapping == NULL
  ▼
NVMe driver: npu_blk_rq_map_sg(q, req, sglist)          ← hook 1
  │  rq_for_each_segment iterates bio segments
  │  for each bvec.bv_page:
  │    nds_mgroup_from_page(page)  →  mgroup (ref +1)
  │    rel_index = page->index % NDS_MAX_SHADOW_PAGES
  │    curr_pa = nds_get_npu_pa(mg, rel_index)  → NPU HBM PA
  │    if contiguous with previous NPU PA: merge sg->length
  │    else: sg_set_page(sg, page, len, off), nsegs++
  │    nds_mgroup_put(mg)  (ref -1)
  │  sg_mark_end(sg); return nsegs (>0 means contains NPU pages)
  ▼
NVMe driver: npu_dma_map_sg_attrs(dev, sglist, nents, dir, attrs)  ← hook 2
  │  for_each_sg:
  │    nds_mgroup_from_page(sg_page(sg))  →  mgroup
  │    nds_ensure_dma_mapped(mg, dev)
  │      └─ dma_map_resource(dev, npu_pa, npu_page_size, BIDIR, 0)
  │         cached to mg->sgt, mg->dma_dev, mg->dma_mapped = true
  │         get_device(peer)  ← holds peer reference (put_device on release)
  │    nds_get_dma_addr(mg, rel_index)  → BAR bus address
  │    sg_dma_address(sg) = dma_addr + sg->offset
  │    sg_dma_len(sg) = sg->length
  │    nds_mgroup_put(mg)
  │  return nr_npu
  ▼
NVMe controller: DMA directly to NPU HBM  ← zero-copy! no CPU staging
```

### 4.5 NPU Page Size Handling

NPU HBM may use 2MB huge pages (`MEM_PAGE_HUGE`), while shadow pages are fixed at 4K. `nds_get_npu_pa` and `nds_get_dma_addr` both handle the `npu_page_size > PAGE_SIZE` case:

```
offset = rel_index * PAGE_SIZE            (4K shadow page offset)
npu_page_idx = offset / npu_page_size     (which NPU page it falls in)
page_offset  = offset % npu_page_size     (intra-page offset)

NPU PA  = page_table->pages_info[npu_page_idx].pa + page_offset
DMA addr = sg_dma_address(sgt.sgl[npu_page_idx]) + page_offset
```

### 4.6 Reference Counting

| Operation | Ref change | Description |
|---|---|---|
| `npu_blk_rq_map_sg` | mgroup get→put (per segment) | Releases immediately after obtaining NPU PA, does not hold |
| `npu_dma_map_sg_attrs` | mgroup get→put (per sg) | Releases immediately after ensuring DMA mapping |
| `nds_ensure_dma_mapped` (first time) | `get_device(peer)` | Cached mapping holds peer reference |
| `nds_mgroup_release` | `put_device(dma_dev)` | Releases peer reference of cached mapping |

### 4.7 Source-Level Correspondence Verification (vs. nvidia-fs)

The table below lists **exact correspondence points** verified line-by-line against nvidia-fs v2.29.4 source: identical constants, identical APIs, identical VMA flags, proving that Path A's mmap/shadow-page path is a faithful port of nvidia-fs `npufs-mmap.c`:

| Correspondence | nvidia-fs source | npu_nds implementation | Verification conclusion |
|---|---|---|---|
| Shadow page GFP | `alloc_page(GFP_USER\|__GFP_ZERO)` | same | **Identical** |
| Page insert API | `vm_insert_page(vma, addr, page)` | same | **Identical** |
| VMA flags | `VM_MIXEDMAP\|VM_DONTEXPAND\|VM_DONTDUMP\|VM_DONTCOPY` | same | **Identical** |
| Shadow page marker | `page->mapping == NULL` | same | **Identical** |
| `page->index` encoding | `base_index + off` | `(mgroup_id << 12) + off` | **Mechanism identical** |
| mgroup lookup | RCU hash table | mutex list | **Intentional simplification** |
| Fault injection | `npufs-fault.c` verifies `vm_insert_page` | none | **Reverse corroboration** |
| Shadow page release | `put_page` | `put_page` | **Identical** |
| VMA close callback | vma_ops `.close` | `nds_vma_close` | **Mechanism identical** |

> Path A's `nds_mmap` / shadow page / VMA ops is a faithful port of nvidia-fs
> `npufs_mgroup_mmap_internal`. Differences are only in:
> (1) lookup structure RCU→mutex; (2) no fault injection/stats/procfs;
> (3) P2P page source changed from NVIDIA P2P API to HAL `hal_kernel_p2p_get_pages`.

### 4.8 Data Path Panorama (Path A)

```
┌─────────────────────────────────────────────────────────────────────┐
│ userspace: npu_file_read/Write ─> libnpu_file ─ioctl─> /dev/npu-nds │
│           │                                                          │
│           └─ mmap shadow buffer (vm_insert_page)                    │
└─────────────────────────────────────────────────────────────────────┘
                                     │ NDS_IOCTL_READ/WRITE
                                     ▼
┌────────────────────────── kernel: npu_nds.ko ───────────────────────┐
│  nds_ioctl → nds_do_io                                              │
│   │  mgroup_get(mgroup_id), state check ACTIVE                      │
│   ▼                                                                  │
│  nds_shadow_io_iter:                                                 │
│   │  build BVEC iov_iter (shadow pages directly as bio_vec)         │
│   │  call file->f_op->read_iter/write_iter (DIO)                     │
│   ▼                                                                  │
│  block layer builds bio (shadow page, page->mapping == NULL)        │
│  ──(zero-copy path, vendor hooks registered)──                      │
│  NVMe driver calls npu_blk_rq_map_sg    ← hook 1                    │
│   │  identify shadow page, nds_get_npu_pa() gets NPU HBM physical addr│
│   │  merge contiguous NPU PA, build sglist                          │
│   ▼                                                                  │
│  NVMe driver calls npu_dma_map_sg_attrs ← hook 2                    │
│   │  nds_ensure_dma_mapped() caches P2P DMA mapping                │
│   │  nds_get_dma_addr() gets NPU BAR bus address                    │
│   ▼                                                                  │
│  NVMe controller DMA directly to NPU HBM ← zero-copy!               │
│                                                                      │
│  ──(bounce path, degrades when NVMe not patched)──                  │
│  data lands in shadow page (CPU memory), userspace drvMemcpy pushes to NPU HBM│
└──────────────────────────────────────────────────────────────────────┘
```

---

## 5. Path B: NVMe passthrough + tracepoint probe

> Source code is in `docs/zh/nds/NVMe/`. This section describes its path-specific parts; shared foundation see §3.

### 5.1 Rationale for Dropping Shadow Buffer and Vendor Hooks

The old npu_nds (i.e., Path A) replicates nvidia-fs's "shadow page + `npufs_dma_rw_ops` vendor hooks" path, but has two fundamental problems:

1. **Strong coupling to NVMe driver**: requires patching the NVMe driver to export
   `nvme_v1_register_nvfs_dma_ops`/`nvme_v2_register_nvfs_dma_ops` symbols, incompatible with upstream NVMe driver;
2. **Shadow page side effects**: 4K `alloc_page` shadow pages consume CPU memory, the `vm_insert_page` path is complex and has `kmap` limitations on ARM64; meanwhile mmap to userspace is needed, adding an extra indirection layer.

The new design directly bypasses VFS and the block layer O_DIRECT path, using NVMe passthrough to use NPU HBM as the SGL source, achieving true zero-copy without modifying the NVMe driver — `nvme_alloc_request` and `nvme_setup_cmd` tracepoint are both stable interfaces already present in mainline.

### 5.2 NVMe passthrough Submit Path

```
nds_do_io
  │  mgroup_get(mgroup_id), state check ACTIVE
  │  copy_from_user(extents[], extents_ptr, extent_cnt * sizeof(extent))
  │  batch = kzalloc; kref_init(ref=1, owner ref)
  ▼
Phase 1: for each extent in extents[]:
  │  atomic_inc(&batch->pending); kref_get(&batch->ref)  ← account first
  │  nds_pt_submit_one(mg, queue, ext, opcode, ctx)
  │   │  build struct nvme_command cmd:
  │   │    cmd->common.opcode = (READ? nvme_cmd_read : nvme_cmd_write)
  │   │    cmd->common.nsid  = 1 (demo hardcoded)
  │   │    cmd->common.slba = ext.physical / 512
  │   │    cmd->common.length = (ext.length / 512 - 1)  (zero-based)
  │   │    cmd->common.dptr.sgl.addr = nds_get_dma_addr(mg, ext.npu_offset)
  │   │    cmd->common.dptr.sgl.length = ext.length
  │   │    cmd->common.dptr.sgl.type = NVME_CMD_SGL_DATA_BLOCK
  │   │  req = nvme_alloc_request(queue, &cmd, 0)
  │   │  req->rq_flags |= RQF_NVME_PT
  │   │  req->end_io_data = ctx; ctx->req = req
  │   │  blk_execute_rq_nowait(req, false, nds_pt_end_io)
  │   │  ✗ failure: atomic_dec + kref_put (undo accounting)
  ▼
Phase 2: wait_for_completion_timeout(&batch->cmp, 30s)
  │  ✗ timeout → Phase 3
  ▼
Phase 3: for each !completed ctx: blk_abort_request(ctx->req)
  │  Phase 3b: wait_for_completion_timeout(&batch->cmp, 1s grace)
  │  ✗ grace still timeout and pending > 0 → Phase 4
  ▼
Phase 4: batch->abandoned = true; kref_put(owner ref)
  │  if pending=0: this kref_put frees batch
  │  if pending>0: last end_io's kref_put frees batch
  │  batch = NULL; ret = -ETIMEDOUT; goto out_peer
  ▼
Phase 5: sum bytes from ctxs[] (completed && status==OK)
  │
  args->done = total bytes; args->ret = error code
  out_batch: kref_put(owner ref) → ref=0 → nds_pt_batch_release → kfree
```

Key points:
- `nvme_alloc_request` is soft-looked-up via `__symbol_get`; on failure READ/WRITE returns -ENOSYS;
- `blk_abort_request` is likewise looked up via `__symbol_get`, optional — if absent only a log is emitted, relying on the block layer's own timeout;
- `nsid` is temporarily hardcoded to 1 (demo);
- the `length` field is `(NLB - 1)` zero-based encoding, counted in 512-byte logical blocks;
- each FIEMAP extent corresponds to an independent NVMe command (simplified, no adjacent extent merging);
- **async batch submit**: Phase 1 submits all extents at once, not waiting per-command;
- **kref batch lifecycle**: owner ref (nds_do_io) + 1 ref per request; end_io and nds_do_io each kref_put once, the last put frees batch;
- **account before submit**: `atomic_inc` + `kref_get` before `blk_execute_rq_nowait`, avoiding the race where end_io returns before accounting;
- **abandoned flag is purely advisory**: only controls whether end_io calls `complete()`; a racy read is harmless — a complete() that nobody waits on is a no-op;
- completion IRQ triggers `nds_pt_end_io`: `atomic_dec_and_test` → if pending=0 and !abandoned → `complete()`; then `kref_put`.

### 5.3 tracepoint Probe: SGL Type Rewrite

The P2P-friendly SGL type for the NVMe controller is `NVME_CMD_SGL_METABUF`, but what to write in this field when constructing the command is decided by the NVMe driver; the upstream NVMe driver may overwrite it after the `nvme_setup_cmd` tracepoint fires. To address this, Path B attaches a probe on the `nvme_setup_cmd` tracepoint:

```c
static void nds_nvme_setup_cmd_probe(void *data, struct request *req,
                                       struct nvme_command *cmd)
{
    if (req->rq_flags & RQF_NVME_PT)
        cmd->common.dptr.sgl.type = NVME_CMD_SGL_METABUF;
}
```

Registration:
```c
for_each_kernel_tracepoint("nvme_setup_cmd", nds_register_tracepoint, &tp);
if (tp) {
    ret = tracepoint_probe_register(tp, nds_nvme_setup_cmd_probe, NULL);
    nds_tp = tp;
}
```

Deregistration (at module unload):
```c
if (nds_tp)
    tracepoint_probe_unregister(nds_tp, nds_nvme_setup_cmd_probe, NULL);
```

This way, without modifying the NVMe driver source, the SGL type of any `RQF_NVME_PT` request is forcibly rewritten to `NVME_CMD_SGL_METABUF`, adapting for P2P.

### 5.4 Data Path Panorama (Path B)

```
┌─────────────────────────────────────────────────────────────────────┐
│ userspace: npu_file_read/Write ─> libnpu_file ─ioctl─> /dev/npu-nds │
│           │                                                          │
│           └─ npu_file_fiemap_query(FS_IOC_FIEMAP on file_fd)        │
│              yields extents[] (logical/physical/length)              │
└─────────────────────────────────────────────────────────────────────┘
                                     │ NDS_IOCTL_READ/WRITE
                                     ▼
┌────────────────────────── kernel: npu_nds.ko ───────────────────────┐
│  nds_ioctl → nds_do_io                                              │
│   │  mgroup_get(mgroup_id), state check ACTIVE                      │
│   │  copy_from_user(extents[], extents_ptr)                         │
│   ▼                                                                  │
│  batch = kzalloc + kref_init(ref=1)                                 │
│  for each extent:                                                   │
│   │  atomic_inc(pending) + kref_get(ref)  ← account first          │
│   │  nds_pt_submit_one: build NVMe command:                         │
│   │    slba = ext.physical / 512                                     │
│   │    length = ext.length / 512 - 1                                 │
│   │    dptr.sgl.addr = nds_get_dma_addr(mg, ext.npu_offset)        │
│   │                     (NPU BAR bus addr, via dma_map_resource)    │
│   │    dptr.sgl.type  = NVME_CMD_SGL_DATA_BLOCK                      │
│   │  req = nvme_alloc_request(q, &cmd, 0)                           │
│   │  req->rq_flags |= RQF_NVME_PT                                   │
│   │  blk_execute_rq_nowait(req, nds_pt_end_io)──▶ NVMe submit       │
│   ▼                                                                  │
│  wait_for_completion_timeout(batch.cmp, 30s)                        │
│   │  ✗timeout → blk_abort_request ×N → grace 1s → abandon+kref_put  │
│   ▼                                                                  │
│  NVMe driver nvme_setup_cmd → tracepoint probe rewrites SGL = METABUF│
│  NVMe controller DMA directly to NPU HBM ← zero-copy!               │
│  nds_pt_end_io: atomic_dec(pending) → complete? + kref_put          │
│  out_batch: kref_put(owner ref) → ref=0 → kfree(batch)              │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 6. ioctl Contract

### 6.1 Shared ioctl

| ioctl | Direction | Purpose | Path A behavior | Path B behavior |
|---|---|---|---|---|
| `NDS_IOCTL_MAP` | user→kernel | pin NPU HBM, establish mgroup | additionally allocates shadow pages + returns mmap offset | no shadow pages, mmap offset/size left 0 (ABI compat) |
| `NDS_IOCTL_UNMAP` | — | unpin, release mgroup | additionally frees shadow pages | directly put_pages + kfree |
| `NDS_IOCTL_READ` | bidirectional | file→NPU HBM | `npu_nds_io_args` uses fd/offset/size, kernel goes BVEC + VFS DIO | `npu_nds_io_args` additionally uses extent_cnt/extents_ptr, kernel goes NVMe PT |
| `NDS_IOCTL_WRITE` | bidirectional | NPU HBM→file | same as READ reversed | same as READ reversed |

### 6.2 Path A READ/WRITE Arguments

`npu_nds_io_args` (Path A):
- `fd`: file descriptor (kernel obtains `struct file` via `fget`, calls its `read_iter/write_iter`);
- `mgroup_id`: NPU HBM handle;
- `file_offset` / `size`: range description;
- `done` / `ret`: kernel fills in completed bytes and return code.

Userspace obtains `shadow_va` via `mmap` on the shadow buffer, but during IO the kernel directly uses BVEC shadow pages, not referencing `shadow_va`.

### 6.3 Path B READ/WRITE Arguments

`npu_nds_io_args` (Path B):
- `fd`: file descriptor (kernel uses `file_inode(fd_file(fd))->i_sb->s_bdev` to find the NVMe block device);
- `mgroup_id`: NPU HBM handle;
- `file_offset` / `size` / `npu_offset`: range description;
- `extent_cnt` / `extents_ptr`: pointer to `npu_nds_fiemap_extent[]` array (filled by userspace);
- `done` / `ret`: kernel fills in completed bytes and return code.

Each `npu_nds_fiemap_extent` contains `logical` / `physical` / `length` / `npu_offset`; the kernel computes SLBA = `physical / 512`, NLB = `length / 512 - 1`, and the NPU BAR address is obtained from `npu_offset` via `nds_get_dma_addr(mg, npu_offset)`.

### 6.4 ioctl Quick Reference

| ioctl | magic | nr | struct | direction |
|---|---|---|---|---|
| `NDS_IOCTL_MAP` | `'N'` | 1 | `npu_nds_map_args` | `_IOWR` |
| `NDS_IOCTL_READ` | `'N'` | 2 | `npu_nds_io_args` (Path B includes FIEMAP extents) | `_IOWR` |
| `NDS_IOCTL_WRITE` | `'N'` | 3 | `npu_nds_io_args` (Path B includes FIEMAP extents) | `_IOWR` |
| `NDS_IOCTL_UNMAP` | `'N'` | 4 | `npu_nds_unmap_args` | `_IOW` |

---

## 7. Design Mapping (vs. GPU)

### 7.1 Architecture Mapping Table

| nvidia-fs design point | Path A (VFS) | Path B (NVMe) | Notes |
|---|---|---|---|
| `nvidia_p2p_get_pages_persistent` pin GPU memory | `hal_kernel_p2p_get_pages` | same | HAL kernel interface, exported as `KA_EXPORT_SYMBOL_GPL` |
| `nvidia_p2p_put_pages` | `hal_kernel_p2p_put_pages` | same | paired release, must not be called from free_callback context |
| `nvidia_p2p_page_table_t` | `struct p2p_page_table` | same | contains `pages_info[].pa` physical address array |
| `nvidia_p2p_dma_map_pages` | `dma_map_resource` | same | peer-specific mapping for NPU physical pages |
| `nvidia_p2p_free_callback` | `nds_p2p_free_callback` | same | NPU driver notifies memory about to be freed |
| 4K shadow page `alloc_page`, `page->mapping=NULL` | same | **no longer needed** | Path B has no shadow pages |
| `page->index = base_index + off` | `page->index = mgroup_id + off` | N/A | Path A mgroup global unique key encoded into page->index |
| `npufs_io_mgroup` RCU hash table | `struct nds_mgroup` mutex list | same (no shadow pages) | demo simplification |
| `NPUFS_IOCTL_MAP/READ/WRITE/REMOVE` | `NDS_IOCTL_*` | same (READ/WRITE extends FIEMAP) | ioctl contract one-to-one |
| Vendor driver hooks | **implemented**: `__symbol_get` + `npufs_dma_rw_ops` | **alternative**: `nvme_alloc_request` + `tracepoint_probe_register` | Path B needs no NVMe driver modification |
| `NPUFS_IOCTL_BATCH_IO` (≤256) | not implemented | not implemented | demo synchronous only |
| `NPUFS_IOCTL_SET/GET/CLEAR_RDMA_REG_INFO` | not implemented | not implemented | RDMA out of demo scope |
| `configure` + `config-host.h` | not needed | not needed | single kernel target |
| `npufs-pci.c` distance matrix | `nds_peer_device` best-effort | same | demo uses PCI class coarse match |
| `npufs-stat.c` procfs | not implemented | not implemented | demo only `pr_info` |
| `npufs-fault.c` debugfs | not implemented | not implemented | — |
| `libcufile.so` (closed source) | `libnpu_file.so` (open source) | same (+FIEMAP) | depends only on libc + UAPI + ioctl |
| cuFile API | npu_file API | same (READ/WRITE adds npu_offset) | open/register/read/write/deregister/close |

### 7.2 API Mapping Table

| cuFile API | npu_file API | Path A | Path B | Notes |
|---|---|---|---|---|
| `cuFileDriverOpen` | `npu_file_open` | ✓ | ✓ | open `/dev/npu-nds` |
| `cuFileBufRegister` | `npu_file_buf_register` | MAP + mmap shadow buffer | ioctl MAP | pin NPU HBM, establish mgroup |
| `cuFileRead` | `npu_file_read` | READ: O_DIRECT → shadow buffer | READ: FIEMAP + RQF_NVME_PT | file→NPU HBM |
| `cuFileWrite` | `npu_file_write` | WRITE: shadow buffer → O_DIRECT | WRITE: FIEMAP + RQF_NVME_PT | NPU HBM→file |
| — | `npu_file_fiemap_query` | — | ✓ | Path B only: `FS_IOC_FIEMAP` wrapper |
| `cuFileBufDeregister` | `npu_file_buf_deregister` | munmap + UNMAP | ioctl UNMAP | release mgroup + unpin |
| `cuFileDriverClose` | `npu_file_close` | ✓ | ✓ | close `/dev/npu-nds` |

> NPU device context management (`drvMemDeviceOpen`/`TsdOpen`), memory allocation (`halMemAlloc`),
> data copy (`drvMemcpy`) are done by the caller (demo) itself, not part of client library responsibilities.

### 7.3 Key Differences

| Dimension | nvidia-fs | Path A (VFS) | Path B (NVMe) | Reason for difference |
|---|---|---|---|---|
| Zero-copy implementation | vendor driver hooks | same, registers `npufs_dma_rw_ops` | NVMe PT + tracepoint probe | Path B avoids modifying NVMe driver |
| File layout source | VFS DIO implicit | same | userspace `FS_IOC_FIEMAP` explicit | Path B kernel does not go through VFS |
| mgroup lookup | RCU hash table O(1) | mutex list O(n) | same | demo readability first |
| IO model | sync+async+batch | sync BVEC + `read_iter` | async submit + drain + `kref` batch + `blk_abort_request` | demo validates architecture |
| Topology affinity | BDF+distance matrix+ACS | PCI class coarse match | same | production driver needs completion |
| DMA cache | per-(mgroup,peer) RCU | per-mgroup single | same | demo only shows one peer |
| Observability | procfs+stats+debugfs | `pr_info` | same | production driver needs completion |
| RDMA direct | `NPUFS_IOCTL_SET_RDMA_REG_INFO` | not implemented | not implemented | — |
| Degradation strategy | none | `__symbol_get` fail→bounce | fail→-ENOSYS | graceful degradation |
| Callback constraint | can deregister DMA in callback | cannot put_pages in callback | same | HAL contract difference |
| Extent merging | block layer bio merging | same | no merging | Path B demo simplification |
| nsid | — | VFS/block layer implicit | hardcoded to 1 | Path B demo simplification |

---

## 8. 4+1 Views

### 8.1 Logical View

```
                        ┌──────────────────────────────────┐
                        │  npu_nds_demo (userspace)        │
                        │  ├─ HAL API: TsdOpen/drvMemDeviceOpen
                        │  │           halMemAlloc/drvMemcpy
                        │  └─ lib API: npu_file_*          │
                        └───────────────┬──────────────────┘
                                        │
                ┌───────────────────────┼───────────────────┐
                │                       │                   │
        ┌───────▼────────┐    ┌────────▼─────────┐         │
        │ libnpu_file    │    │ HAL userspace    │         │
        │ .so            │    │ libascend_hal.so │         │
        │ (libc + ioctl) │    │ libc_sec.so      │         │
        │ (+FIEMAP, B)   │    │ -ldl             │         │
        └───────┬────────┘    └────────┬─────────┘         │
                │ ioctl                │ HAL syscall       │
                ▼                      ▼                   │
┌─────────────────────────── npu_nds.ko ──────────────────────┐
│                                                              │
│  ┌─────── control plane (npu_nds_kern.c) ───┐               │
│  │ ioctl dispatch  nds_ioctl                │               │
│  │ MAP             nds_do_map               │               │
│  │ UNMAP           nds_do_unmap             │               │
│  │ READ/WRITE      nds_do_io                │               │
│  │   Path A: → nds_shadow_io_iter          │               │
│  │   Path B: → nds_pt_submit_one           │               │
│  │ mgroup lifecycle  nds_mgroup_get/put/release│           │
│  │ Path A: NVMe hook reg  __symbol_get + npu_dma_rw_ops   │
│  │ Path B: symbol lookup  __symbol_get("nvme_alloc_request")│
│  │ Path B: tracepoint probe reg for_each_kernel_tracepoint│
│  └───────────────────┬────────────────────────┘               │
│                       │                                       │
│  ┌─────── memory plane ─┴──────────────────┐                │
│  │ struct nds_mgroup:                      │                │
│  │   [Path A] shadow_pages[] + page_table │                │
│  │   [Path B] page_table (p2p_page_table)  │                │
│  │   sgt + dma_dev + dma_mapped (P2P cache)│                │
│  │   state machine + kref + pinned_ref_dropped│            │
│  └───────────────────┬────────────────────┘                │
│                       │                                       │
│  ┌─────── data plane ──┴──────────────────┐                │
│  │ shared:                                 │                │
│  │   nds_peer_device: PCI peer enum        │                │
│  │   nds_get_npu_pa: NPU offset → HBM PA   │                │
│  │   nds_ensure_dma_mapped: P2P map cache   │                │
│  │   nds_get_dma_addr: NPU offset → BAR addr│               │
│  │ Path A specific:                        │                │
│  │   nds_mgroup_from_page: page→mgroup     │                │
│  │   nds_shadow_io_iter: BVEC iov_iter     │                │
│  │   NVMe hooks: npu_dma_rw_ops (6 hooks)  │                │
│  │ Path B specific:                        │                │
│  │   nds_file_to_queue: fd → NVMe req queue│               │
│  │   nds_pt_submit_one: build NVMe cmd + submit│           │
│  │   nds_pt_end_io: completion IRQ callback│              │
│  │   nds_pt_batch_release: kref frees batch │              │
│  │   nds_nvme_setup_cmd_probe: SGL rewrite │              │
│  └──────────────────────────────────────────┘              │
│                                                            │
│  ┌─ helpers ───────────────────────────────┐              │
│  │ nds_p2p_free_callback  HAL cb → state machine│        │
│  │ [Path A] nds_mmap/nds_vma_close  mmap  │              │
│  │ nds_init/exit          module entry/cleanup│          │
│  └──────────────────────────────────────────┘              │
└────────────────────────────────────────────────────────────────┘
        │ hal_kernel_p2p_get_pages/put_pages
        ▼
┌───────────────────┐
│ NPU kernel driver │
│ (HAL, exports P2P│
│  symbols)         │
└───────────────────┘
```

Key logical data structures:

| Structure | File | Responsibility |
|---|---|---|
| `struct nds_mgroup` | npu_nds_kern.c | [A] shadow_pages + page_table / [B] page_table + state machine + kref + DMA cache |
| `struct nds_pt_ctx` | npu_nds_kern.c | [B] per-request: batch backref, status, nr_bytes, req back-pointer, completed |
| `struct nds_pt_batch` | npu_nds_kern.c | [B] async batch: kref ref, completion, atomic pending, ctxs[], abandoned flag |
| `struct p2p_page_table` | ascend_kernel_hal.h | NPU physical page descriptor, `pages_info[].pa` |
| `struct npu_nds_map_args` | npu_nds.h | MAP: npu_va, size → mgroup_id, [A] mmap_offset |
| `struct npu_nds_io_args` | npu_nds.h | READ/WRITE: fd, offset, size → done, ret; [B] additionally extent_cnt/extents_ptr |
| `struct npu_nds_fiemap_extent` | npu_nds.h | [B] FIEMAP extent: logical/physical/length/npu_offset |
| `npu_file_ctx_t` | npu_file.h | userspace context: nds_fd |
| `npu_file_handle_t` | npu_file.h | [A] mgroup_id + shadow_va / [B] mgroup_id + buf_size |

### 8.2 Process View

#### 8.2.1 Path A Synchronous Read/Write Main Flow

```
user thread           kernel(npu_nds.ko)        block/fs          NPU driver/HAL
    │                     │                        │                     │
    │ TsdOpen             │                        │                     │
    │ drvMemDeviceOpen    │                        │                     │
    │ halMemAlloc(HBM)    │                        │                     │
    │ npu_file_open       │                        │                     │
    │──ioctl(MAP)──────▶│ nds_do_map             │                     │
    │                     │  hal_kernel_p2p_get_pages────────────────────▶│ pin NPU HBM
    │                     │  ←─ page_table ───────────────────────────────│ pages_info[].pa
    │                     │  alloc_page(shadow)    │                     │
    │                     │  page->mapping=NULL    │                     │
    │                     │  page->index=(mgid<<12)+i                    │
    │                     │  list_add(mgroup)      │                     │
    │←──mgroup_id+offset──│                        │                     │
    │ mmap(shadow buf)    │                        │                     │
    │                     │  vm_insert_page        │                     │
    │ npu_file_write      │                        │                     │
    │──ioctl(WRITE)─────▶│ nds_do_io              │                     │
    │                     │  mgroup_get, state check│                    │
    │                     │  nds_shadow_io_iter:   │                     │
    │                     │  ──BVEC read_iter──────▶│ O_DIRECT (shadow in bio)│
    │                     │  ←─────────────────────│ NVMe hooks intercept:│
    │                     │                        │  npu_blk_rq_map_sg  │
    │                     │                        │  npu_dma_map_sg     │
    │                     │                        │  DMA→NPU HBM (zero-copy)│
    │←──done=ssize_t─────│                        │                     │
    │ (zero-copy: no drvMemcpy needed, data direct to NPU HBM)           │
    │ (bounce: drvMemcpy(shadow→NPU) pushes to HBM)                      │
    │ npu_file_read       │                        │                     │
    │──ioctl(READ)──────▶│ (same as above, reversed)│                    │
    │ verify(D2H)         │                        │                     │
    │ npu_file_buf_deregister                      │                     │
    │──ioctl(UNMAP)────▶│ nds_do_unmap           │                     │
    │                     │  hal_kernel_p2p_put_pages─────────────────────▶│ unpin
    │                     │  free shadow pages     │                     │
    │ npu_file_close      │                        │                     │
    │ halMemFree / drvMemDeviceClose / TsdClose   │                     │
```

#### 8.2.2 Path B Synchronous Read/Write Main Flow

```
user thread           kernel(npu_nds.ko)        NVMe driver        NPU driver/HAL
    │ TsdOpen / drvMemDeviceOpen / halMemAlloc(HBM)│                     │
    │ open(O_DIRECT) / fallocate(size)             │                     │
    │ npu_file_open                                 │                     │
    │──ioctl(MAP)──────▶│ nds_do_map              │                     │
    │                     │  hal_kernel_p2p_get_pages────────────────────▶│ pin NPU HBM
    │                     │  ←─ page_table ───────────────────────────────│
    │                     │  list_add(mgroup)      │                     │
    │←──mgroup_id────────│                         │                     │
    │ npu_file_write (lib-internal FIEMAP)          │                     │
    │──ioctl(WRITE, ext)─▶│ nds_do_io              │                     │
    │                     │  mgroup_get, copy_from_user(extents)         │
    │                     │  nds_file_to_queue(fd) → req_queue           │
    │                     │  batch = kzalloc + kref_init(ref=1)          │
    │                     │  for each ext:                               │
    │                     │    atomic_inc(pending) + kref_get(ref)       │
    │                     │    dma_addr = nds_get_dma_addr(mg, ext.npu_offset)
    │                     │    req = nvme_alloc_request(queue, &cmd, 0)  │
    │                     │    req->rq_flags |= RQF_NVME_PT               │
    │                     │    blk_execute_rq_nowait(req, nds_pt_end_io)──▶│ NVMe submit
    │                     │                        │ nvme_setup_cmd ←probe─│
    │                     │                        │  (rewrite SGL = METABUF)
    │                     │                        │ NVMe DMA→NPU HBM (zero-copy)
    │                     │  wait_for_completion_timeout(batch.cmp, 30s)
    │                     │  ✗timeout → blk_abort_request ×N → grace 1s → abandon
    │                     │  Phase 5: sum bytes from ctxs                │
    │←──done=ssize_t─────│  out_batch: kref_put(owner) → kfree          │
    │ (zero-copy: data direct to NPU HBM)                                │
    │ (degrade: nvme_alloc_request not found → -ENOSYS)                  │
    │ npu_file_read (same, READ command)                                 │
    │ verify(D2H) / npu_file_buf_deregister / npu_file_close             │
    │──ioctl(UNMAP)────▶│ hal_kernel_p2p_put_pages─────────────────────▶│ unpin
    │ halMemFree / drvMemDeviceClose / TsdClose                           │
```

#### 8.2.3 free_callback vs. In-Flight IO Race (shared by both paths)

```
Path A:  ioctl thread ──BVEC──> filesystem ──> completion ──> mgroup_put
Path B:  ioctl thread ──async submit N──> NVMe ──> end_io ×N ──> kref_put(→0) ──> kfree(batch)
Path C (reclaim):  HAL free_callback ──> state=TERMINATED ──> pinned_ref_dropped=true ──> mgroup_put
Path D (module):  nds_exit ──> walk list ──> force TERMINATED ──> mgroup_put ×2
```

A/B and C are serialized via the `pinned_ref_dropped` flag + kref:
- If C arrives first, A/B (UNMAP) only put once (object ref);
- If A/B arrive first, C detects it's set, no longer puts;
- Eventually ref=0 → `nds_mgroup_release` → `hal_kernel_p2p_put_pages` + (Path A) free shadow pages + kfree.

#### 8.2.4 Concurrency Key Points

- **mgroup lock**: `mg->lock` protects reads/writes of mgroup state on the IO path and reclaim path;
- **global list lock**: `nds_mgroup_mutex` protects add/del/lookup of `nds_mgroup_list`;
- **Path A mmap references**: `nds_mmap` does `kref_get` ×2 before `vm_insert_page` (lookup + mmap lifetime), drops lookup ref on success; `nds_vma_close` (munmap) drops mmap lifetime ref;
- **DMA mapping single-flight**: `nds_ensure_dma_mapped` uses `mg->lock` + `dma_mapped` flag to guarantee one mgroup is mapped only once;
- **Path B tracepoint probe**: register/unregister once at module load/unload, the probe itself is lockless;
- **Path B batch lifecycle**: `nds_pt_batch` is managed via `kref` — owner ref (nds_do_io) + 1 ref per request;
  end_io and nds_do_io each kref_put, the last put frees batch;
  the `abandoned` flag only controls whether `complete()` is called (racy read harmless); no `smp_mb` needed.


### 8.3 Development View

#### 8.3.1 Directory Structure (Path A — VFS)

```
docs/zh/nds/VFS/
├── DESIGN_NPU.md          # design doc (legacy, for reference)
├── README.md
├── README_en.md
├── run.sh                 # driver/build/run/unload
├── kernel/
│   ├── Makefile           # Kbuild
│   ├── npu_nds_hook.h     # vendor hooks: npufs_dma_rw_ops, 6 hooks, feature bitmap
│   └── npu_nds_kern.c     # control plane + data plane + memory plane
├── include/
│   └── ascend_kernel_hal.h  # HAL UAPI
├── npu_nds.h              # ioctl UAPI
├── lib/
│   ├── CMakeLists.txt
│   ├── npu_file.h         # userspace API + npu_file_handle_t
│   └── npu_file.c         # open/register/read/write/deregister/close + mmap
└── demo/
    ├── CMakeLists.txt          # references shared demo_main.c
    └── demo_main.c             # moved to shared directory, see below
```

> Both paths share the same `demo/demo_main.c` source; linking against the
> respective `libnpu_file.so` selects the zero-copy path.  See "3. Shared demo"
> below.

#### 8.3.2 Directory Structure (Path B — NVMe)

```
docs/zh/nds/NVMe/
├── DESIGN_NPU.md          # design doc (NVMe passthrough)
├── README.md
├── README_en.md
├── run.sh
├── kernel/
│   ├── Makefile
│   ├── npu_nds_hook.h     # symbol/tracepoint defs: RQF_NVME_PT, SGL constants, soft lookup
│   └── npu_nds_kern.c     # control plane + data plane + memory plane (no shadow/mmap)
├── include/
│   └── ascend_kernel_hal.h
├── npu_nds.h              # ioctl UAPI (+FIEMAP extent)
├── lib/
│   ├── CMakeLists.txt
│   ├── npu_file.h         # userspace API (+FIEMAP query)
│   └── npu_file.c         # open/register/read/write/deregister/close (+FIEMAP)
└── demo/
    └── CMakeLists.txt          # references shared demo_main.c
```

#### 8.3.3 Shared Demo Directory

```
docs/zh/nds/demo/
└── demo_main.c            # shared demo: H2D/D2H verification, fallocate pre-alloc
```

The `libnpu_file.so` API signatures are identical across both paths;
differences are hidden inside the library:
- Path A's lib `mmap`s the shadow buffer inside `npu_file_buf_register`.
- Path B's lib runs `FS_IOC_FIEMAP` inside `npu_file_read/write`.

The demo calls the 6-argument API (with `npu_offset`, passing 0) and
works with either library.

#### 8.3.4 Build System

**Kernel module** (Path A/B same Makefile pattern):

```makefile
obj-m := npu_nds.o
npu_nds-objs := npu_nds_kern.o
ccflags-y := -I$(src)/../include -I$(src)/.. -Wall
```

- Path A additionally `__symbol_get("nvme_v1_register_nvfs_dma_ops")` (fall back to `nvme_v2_register_nvfs_dma_ops`);
- Path B additionally `__symbol_get("nvme_alloc_request")` + `for_each_kernel_tracepoint("nvme_setup_cmd", ...)`.

**Userspace library** (CMake):

```cmake
add_library(npu_file SHARED npu_file.c)
target_include_directories(npu_file PUBLIC ../ ..)
set_target_properties(npu_file PROPERTIES
    C_STANDARD 99
    POSITION_INDEPENDENT_CODE ON)
```

**Demo** (CMake):

```cmake
add_executable(npu_nds_demo ../demo/demo_main.c)
target_link_libraries(npu_nds_demo PRIVATE npu_file dl rt)
```

#### 8.3.4 Dependencies

```
npu_nds_demo
  ├── libnpu_file.so   (npu_file.c)
  │     ├── libc        (open, ioctl, mmap, munmap, fallocate, FS_IOC_FIEMAP)
  │     └── npu_nds.h   (UAPI, ioctl structs)
  ├── libdl             (dynamic lookup of libascend_hal.so)
  ├── librt             (clock_gettime)
  └── libascend_hal.so  (HAL: TsdOpen, drvMemDeviceOpen, halMemAlloc, hal_kernel_p2p_get_pages, ...)
        └── NPU kernel driver (exports P2P symbols)

npu_nds.ko
  ├── npu_nds.h         (UAPI)
  ├── ascend_kernel_hal.h  (HAL kernel types)
  ├── [A] __symbol_get("nvme_v1_register_nvfs_dma_ops") (or v2)  → NVMe driver
  ├── [B] __symbol_get("nvme_alloc_request")                → NVMe driver
  ├── [B] for_each_kernel_tracepoint("nvme_setup_cmd")     → tracepoint
  ├── hal_kernel_p2p_get_pages / put_pages               → HAL/NPU driver
  ├── dma_map_resource / dma_unmap_resource               → DMA API
  └── blk_execute_rq_nowait / blk_mq_rq_to_blk            → block layer (Path B)
```

### 8.4 Physical View

```
┌─────────────────────────────────────────────────────────────┐
│                  userspace process (npu_nds_demo)           │
│  ┌─────────────┐  ┌──────────────┐  ┌────────────────────┐ │
│  │ HAL handle   │  │ npu_file_ctx │  │ FIEMAP extents buf │ │
│  │ (dev fd,     │  │ (nds_fd,     │  │ (stack/heap)       │ │
│  │  mem handle) │  │  mgroup_id)  │  │                    │ │
│  └──────┬──────┘  └──────┬───────┘  └────────────────────┘ │
│         │ halMemAlloc     │ mmap (Path A) / none (Path B)    │
│         ▼                 │                                  │
│  ┌──────────────┐         │                                  │
│  │ NPU HBM      │◄────────┘ (shadow buffer map, Path A)     │
│  │ (user VA)    │           (no map, Path B)                 │
│  └──────────────┘                                           │
└─────────────────────────────┬───────────────────────────────┘
                              │ syscall (ioctl/mmap)
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    kernel space (npu_nds.ko)                │
│  ┌──────────────────────────────────────────────────────┐   │
│  │  nds_mgroup_list  ───[mgroup]───[mgroup]─── ...      │   │
│  │     (mutex)         │ state, kref                    │   │
│  │                     │                                 │   │
│  │  [A] shadow_pages[]──┤  [B] (no shadow pages)         │   │
│  │  page_table ────────┤  (p2p_page_table, NPU HBM PA)   │   │
│  │  sgt, dma_dev ──────┤  (P2P DMA mapping cache)        │   │
│  └─────────────────────┼────────────────────────────────┘   │
│                         │                                     │
│  [A] npufs_dma_rw_ops ──┘  (6 hooks, registered to NVMe driver)│
│  [B] tracepoint probe ────┘  (nvme_setup_cmd, SGL rewrite)   │
│  [B] nvme_alloc_request ───┘  (soft lookup)                  │
└─────────────────────────────┬───────────────────────────────┘
                              │
              ┌───────────────┼───────────────┐
              ▼               ▼               ▼
┌─────────────────┐  ┌──────────────┐  ┌──────────────────┐
│ NVMe controller │  │ NPU device   │  │ system memory    │
│ (bdev, queue)   │  │ (HBM, BAR)   │  │ (shadow, Path A) │
│                 │  │              │  │                  │
│ DMA R/W ────────┼──┤ HBM (P2P)    │  │ alloc_page       │
│ (SGL → NPU)     │  │              │  │ (4K, page->mapping=NULL)
│                 │  │              │  │                  │
│ [A] hooks intercept│ │             │  │ [A] vm_insert_page
│ [B] PT direct submit│ │            │  │   → user VA      │
└─────────────────┘  └──────────────┘  └──────────────────┘
```

Physical topology constraints:
- NPU HBM is not in the system memory address space; it must be mapped into the DMA address space via `dma_map_resource`;
- the NVMe controller must support P2P (ACS config, NTB distance, or same RC);
- Path A shadow pages consume system memory (4K shadow page per 4K NPU HBM);
- Path B has no system memory overhead (SGL directly points to NPU HBM DMA address).

### 8.5 Scenario View

#### 8.5.1 Normal Write Flow (Path A)

1. Userspace `drvMemDeviceOpen` + `halMemAlloc` allocates NPU HBM;
2. `npu_file_open` opens `/dev/npu-nds`;
3. `npu_file_buf_register` → ioctl MAP → `hal_kernel_p2p_get_pages` pin NPU HBM;
4. `alloc_page` shadow pages, `page->mapping=NULL`, `page->index` encodes mgroup_id;
5. `mmap` inside `npu_file_buf_register` → `vm_insert_page` shadow pages to user VA;
6. `npu_file_write(fd, shadow_va, offset, size)` → ioctl WRITE;
7. Kernel `nds_shadow_io_iter`: shadow pages wrapped as BVEC, calls `file->f_op->write_iter`;
8. VFS DIO builds bio, NVMe hooks intercept `npu_blk_rq_map_sg` + `npu_dma_map_sg`;
9. hooks point SGL to NPU HBM physical address (zero-copy) or shadow page (bounce);
10. NVMe DMA writes directly to NPU HBM (zero-copy) or shadow page (bounce);
11. Userspace `drvMemcpy(HBM, shadow_va)` pushes to NPU (bounce only);
12. `npu_file_buf_deregister` → ioctl UNMAP → `hal_kernel_p2p_put_pages` + free shadow pages;
13. `npu_file_close`, `halMemFree`, `drvMemDeviceClose`, `TsdClose`.

#### 8.5.2 Normal Write Flow (Path B)

1. Userspace `drvMemDeviceOpen` + `halMemAlloc` allocates NPU HBM;
2. `open(O_DIRECT)` + `fallocate` pre-allocates file;
3. `npu_file_open` opens `/dev/npu-nds`;
4. `npu_file_buf_register` → ioctl MAP → `hal_kernel_p2p_get_pages` pin NPU HBM;
5. `npu_file_fiemap_query(fd)` → `FS_IOC_FIEMAP` yields extents[];
6. `npu_file_write(fd, npu_offset=0, offset, size)` → ioctl WRITE(extents);
7. Kernel `nds_do_io` → `nds_file_to_queue(fd)` gets NVMe request queue;
8. `batch = kzalloc` + `kref_init(ref=1)`;
9. Phase 1: for each extent: `atomic_inc(pending)` + `kref_get(ref)`, `nds_pt_submit_one` builds NVMe command, `blk_execute_rq_nowait`;
10. NVMe driver `nvme_setup_cmd` → tracepoint probe rewrites SGL = `NVME_CMD_SGL_METABUF`;
11. NVMe DMA writes directly to NPU HBM (zero-copy);
12. `nds_pt_end_io` ×N: `atomic_dec(pending)` → if =0 and !abandoned → `complete()`, then `kref_put`;
13. Phase 2: `wait_for_completion_timeout(batch.cmp, 30s)` — timeout → Phase 3 abort + 1s grace + Phase 4 abandon;
14. Phase 5: sum completed ctx bytes; `out_batch: kref_put(owner ref)` → ref=0 → `kfree(batch)`;
15. `npu_file_buf_deregister` → ioctl UNMAP → `hal_kernel_p2p_put_pages`;
16. `npu_file_close`, `halMemFree`, `drvMemDeviceClose`, `TsdClose`.

#### 8.5.3 HAL free_callback Reclaim

1. NPU driver notifies HBM is about to be freed, calls `nds_p2p_free_callback`;
2. `nds_p2p_free_callback` gets `mgroup` (via page->index or pgmap), sets `state=TERMINATED`;
3. If `pinned_ref_dropped=false`: set true, `kref_put(&mg->ref)` releases pinned ref;
4. Subsequent ioctls find `state != ACTIVE`, return `-EIO`;
5. When last IO completes (Path A/B) or UNMAP arrives, `kref_put` releases object ref;
6. ref=0 → `nds_mgroup_release` → `hal_kernel_p2p_put_pages` + (Path A) free shadow pages + kfree;
7. Note: **must not** call `put_pages` in `free_callback`; defer to `release`.

#### 8.5.4 Module Unload

1. `nds_exit` locks `nds_mgroup_mutex`, walks `nds_mgroup_list`;
2. For each mgroup: set `state=TERMINATED`, if `pinned_ref_dropped` not set then set + put;
3. kref_put releases object ref (if pinned ref already put, then ref=0);
4. `nds_mgroup_release` frees each mgroup;
5. [A] `__symbol_put` releases NVMe driver references;
6. [B] `tracepoint_probe_unregister` unloads the probe;
7. [B] `__symbol_put` releases NVMe driver references;
8. `unregister_chrdev_region`, `class_destroy`, exit done.

#### 8.5.5 Degradation Scenarios

| Scenario | Path A | Path B |
|---|---|---|
| NVMe driver symbol not found | `npufs_dma_rw_ops` not registered, READ/WRITE bounce (drvMemcpy) | `nvme_alloc_request` not found, READ/WRITE returns -ENOSYS |
| tracepoint not found | N/A | probe not registered, SGL not rewritten (P2P may fail) |
| NPU P2P not supported | `hal_kernel_p2p_get_pages` fails, MAP returns -ENOMEM | same |
| Filesystem doesn't support FIEMAP | N/A (via VFS DIO) | `FS_IOC_FIEMAP` fails, READ/WRITE returns -ENOTTY |
| Non-NVMe block device | VFS DIO works (hooks don't intercept) | `I_BDEV` can't find NVMe queue, returns -ENODEV |

#### 8.5.6 Observability

- `pr_info`: mgroup create/release, IO submit/complete, free_callback, module load/unload;
- `dmesg | grep npu_nds`: view module logs;
- `cat /proc/kallsyms | grep nvme_alloc_request`: verify symbol exists;
- `cat /sys/kernel/debug/tracing/events/nvme/nvme_setup_cmd/enable`: verify tracepoint exists;
- Path A: `cat /proc/kallsyms | grep npufs_dma_rw_ops`: verify hooks registered successfully.


---

## 9. Key Invariants and Design Trade-offs

### 9.1 Invariants

| # | Invariant | Guarantee mechanism | Violation consequence |
|---|---|---|---|
| INV-1 | NPU HBM stays pinned while mgroup ref > 0 | kref + hal_kernel_p2p_get_pages/put_pages paired | HBM freed, DMA crash |
| INV-2 | P2P DMA mapping for one mgroup done only once | mg->lock + dma_mapped flag | duplicate mapping, resource leak |
| INV-3 | free_callback does not call put_pages | deferred to nds_mgroup_release | recursive lock or use-after-free |
| INV-4 | IO path checks state == ACTIVE | mg->lock protects read/write | IO to reclaimed HBM |
| INV-5 | shadow page page->mapping == NULL (Path A) | set NULL immediately after alloc_page | kernel reclaim mis-reclaims |
| INV-6 | shadow page page->index encodes mgroup_id (Path A) | written at allocation | free_callback can't find mgroup |
| INV-7 | RQF_NVME_PT only for npu_nds requests (Path B) | set when building command | mistakenly rewrites other NVMe request SGL |
| INV-8 | tracepoint probe only modifies RQF_NVME_PT requests (Path B) | probe checks rq_flags | affects all NVMe IO |
| INV-9 | peer_device unchanged during mgroup lifetime | cached at first IO | DMA address invalid |
| INV-10 | all mgroups released before module unload | nds_exit force TERMINATED + put | use-after-free |

### 9.2 Design Trade-offs

#### 9.2.1 Shadow Buffer vs NVMe passthrough

| Dimension | Path A (shadow buffer) | Path B (NVMe PT) | Trade-off |
|---|---|---|---|
| Zero-copy | needs hooks to intercept, else bounce | natural zero-copy | B simpler |
| Memory overhead | 4K shadow page per 4K HBM | none | B better |
| Driver modification | needs NVMe driver to export symbol | no modification | B better |
| File layout | VFS DIO implicit handling | needs userspace FIEMAP | A simpler |
| FS compatibility | any FS supporting DIO | NVMe block device only | A more general |
| SGL control | hooks fully control SGL | needs tracepoint probe rewrite | A more direct |
| Stability | hooks interface may change | tracepoint interface more stable | B more stable |
| Performance | VFS DIO path overhead | direct NVMe submit | B faster |
| Complexity | shadow page + mmap + BVEC | FIEMAP + PT + probe | comparable |

#### 9.2.2 mgroup lookup: RCU hash table vs mutex list

| Dimension | RCU hash table (nvidia-fs) | mutex list (demo) | Trade-off |
|---|---|---|---|
| Lookup complexity | O(1) | O(n) | production uses RCU |
| Concurrency | RCU read lockless | mutex lock | RCU better |
| Implementation complexity | hash function + RCU | list_for_each | demo simpler |
| Scalability | hash bucket resize | list linear | RCU better |

#### 9.2.3 IO model: sync vs async vs batch

| Dimension | sync (demo) | async (nvidia-fs) | batch (nvidia-fs) | Trade-off |
|---|---|---|---|---|
| Implementation complexity | lowest | medium | high | demo validates architecture |
| Latency | syscall switch each time | parallel multi-IO | batch reduces syscall | batch optimal |
| Semantics | simple clear | needs completion callback | needs batch structure | demo simplest |
| Applicable scenario | validation | production | high throughput | — |

#### 9.2.4 extent merging strategy

| Dimension | no merge (demo) | merge adjacent extents | block layer bio merge (Path A) | Trade-off |
|---|---|---|---|---|
| Implementation complexity | lowest | medium | VFS built-in | demo simplest |
| NVMe command count | one per extent | reduced | block layer optimized | merge better |
| Applicable scenario | validation | production | Path A natural | — |

#### 9.2.5 nsid acquisition strategy

| Dimension | hardcoded 1 (demo) | ioctl query | FS inference | Trade-off |
|---|---|---|---|---|
| Implementation complexity | lowest | medium | low | demo simplest |
| Correctness | single namespace only | correct | depends on FS | production needs ioctl |
| Applicable scenario | validation | production | Path A | — |

### 9.3 Known Limitations

1. **Single peer assumption**: DMA mapping cache has only one peer (NVMe controller), multi-peer scenarios not handled;
2. **No RDMA direct**: `NPUFS_IOCTL_SET_RDMA_REG_INFO` etc. RDMA API not implemented;
3. **No batch IO**: `NPUFS_IOCTL_BATCH_IO` not implemented, each IO is an independent syscall;
4. **No async completion notification**: Path B has implemented async batch submit + drain, but no eventfd/callback-style async completion notification;
5. **No stats procfs**: `npufs-stat.c`-style procfs interface not implemented;
6. **No debug debugfs**: `npufs-fault.c`-style debugfs interface not implemented;
7. **No topology distance matrix**: `nds_peer_device` only uses PCI class coarse match, no BDF distance computation;
8. **nsid hardcoded**: Path B namespace ID hardcoded to 1;
9. **Logical block assumes 512B**: `NDS_NVME_LB_SIZE = 512`, 4K native devices not supported;
10. **extent limit 64**: `NDS_MAX_EXTENTS = 64`, may be insufficient for large files;
11. **SGL type value to confirm**: `NVME_CMD_SGL_METABUF = 0x00` is a guessed value, needs vendor confirmation;
12. **No extent merging**: Path B one NVMe command per extent, no adjacent extent merging;
13. **Single process assumption**: mgroup list is global, multi-process concurrent IO not fully tested;
14. **No ACS check**: ACS config not checked, P2P may be blocked by ACS;
15. **Path B NVMe only**: non-NVMe block devices (e.g., virtio-blk) not supported.

### 9.4 Roadmap to Production

```
demo (current)
  │
  ├─ mgroup RCU hash table (replace list)      ── performance
  ├─ async completion notification (eventfd)   ── throughput (async submit+drain already done)
  ├─ batch IO (BATCH_IO ioctl)                 ── throughput
  ├─ extent merging                            ── NVMe command count
  ├─ nsid ioctl query                          ── correctness
  ├─ 4K native block support                   ── compatibility
  ├─ multi-peer DMA cache                      ── functionality
  ├─ PCI topology distance matrix              ── performance
  ├─ ACS check                                 ── safety
  ├─ RDMA direct (SET_RDMA_REG_INFO)           ── functionality
  ├─ procfs stats                              ── observability
  ├─ debugfs debugging                         ── observability
  ├─ Path A: hooks interface version negotiation ── compatibility
  └─ Path B: SGL type vendor confirmation       ── correctness
  │
  ▼
production driver
```

---

## Appendix A: Source Index

### A.1 Path A (VFS)

| File | Lines (approx) | Key functions/structures |
|---|---|---|
| `VFS/kernel/npu_nds_kern.c` | ~800 | `nds_do_map`, `nds_do_unmap`, `nds_do_io`, `nds_shadow_io_iter`, `nds_mgroup_get/put/release`, `nds_p2p_free_callback`, `nds_mmap`, `nds_vma_close`, `nds_init`, `nds_exit` |
| `VFS/kernel/npu_nds_hook.h` | ~150 | `struct npufs_dma_rw_ops`, `NPUFS_DMA_RW_OPS_VER`, `NPUFS_FEATURE_*`, hook register/unregister macros |
| `VFS/npu_nds.h` | ~85 | `struct npu_nds_map_args`, `struct npu_nds_io_args`, ioctl numbers |
| `VFS/lib/npu_file.h` | ~70 | `npu_file_handle_t` (incl. shadow_va), `npu_file_open/register/read/write/deregister/close` |
| `VFS/lib/npu_file.c` | ~250 | mmap shadow buffer, ioctl wrappers |
| `docs/zh/nds/demo/demo_main.c` | ~265 | Shared demo: H2D/D2H verification flow (fallocate pre-alloc) |
| `VFS/DESIGN_NPU.md` | ~830 | design doc (legacy, for reference) |

### A.2 Path B (NVMe)

| File | Lines (approx) | Key functions/structures |
|---|---|---|
| `NVMe/kernel/npu_nds_kern.c` | ~1020 | `nds_do_map`, `nds_do_unmap`, `nds_do_io` (5-phase async drain), `nds_pt_batch_release`, `nds_pt_submit_one`, `nds_pt_end_io`, `nds_file_to_queue`, `nds_nvme_setup_cmd_probe`, `nds_register_tracepoint`/`nds_unregister_tracepoint`, `nds_mgroup_get/put/release`, `nds_p2p_free_callback`, `nds_init`, `nds_exit` |
| `NVMe/kernel/npu_nds_hook.h` | ~107 | `RQF_NVME_PT`, `NVME_CMD_SGL_METABUF`, `NVME_CMD_SGL_DATA_BLOCK`, `nvme_alloc_request_fn_t`, `blk_abort_request_fn_t`, `NDS_NVME_ALLOC_REQUEST_SYM`, `NDS_BLK_ABORT_REQUEST_SYM`, `NDS_NVME_SETUP_CMD_TP_NAME` |
| `NVMe/npu_nds.h` | ~140 | `struct npu_nds_fiemap_extent`, `NDS_MAX_EXTENTS`, `struct npu_nds_io_args` (incl. extent_cnt/extents_ptr), rest same as Path A |
| `NVMe/lib/npu_file.h` | ~80 | `npu_file_handle_t` (no shadow_va), `npu_file_fiemap_query`, read/write adds `npu_offset` parameter |
| `NVMe/lib/npu_file.c` | ~250 | FIEMAP wrapper, ioctl wrappers (no mmap) |
| `docs/zh/nds/demo/demo_main.c` | ~265 | Shared demo: H2D/D2H verification flow (fallocate pre-alloc) |
| `NVMe/DESIGN_NPU.md` | ~770 | design doc (NVMe passthrough) |

### A.3 Shared Files

| File | Description |
|---|---|
| `include/ascend_kernel_hal.h` | HAL UAPI: `struct p2p_page_table`, `hal_kernel_p2p_get_pages`/`put_pages` function pointer prototypes |
| `design.md` | this document (merged design doc) |

---

## Appendix B: ioctl Quick Reference

### B.1 Shared ioctl Numbers

```c
#define NDS_IOCTL_MAP      _IOWR('N', 1, struct npu_nds_map_args)
#define NDS_IOCTL_READ     _IOWR('N', 2, struct npu_nds_io_args)
#define NDS_IOCTL_WRITE    _IOWR('N', 3, struct npu_nds_io_args)
#define NDS_IOCTL_UNMAP    _IOW('N',  4, struct npu_nds_unmap_args)
```

### B.2 Structure Quick Reference

```c
/* shared by Path A/B */
struct npu_nds_map_args {
    uint64_t npu_va;        /* i: NPU HBM virtual address */
    uint64_t size;          /* i: size */
    uint32_t flags;         /* i: reserved */
    uint32_t reserved;
    /* out */
    uint64_t mgroup_id;    /* o: mgroup handle */
    uint64_t mmap_offset;  /* o: [A] shadow buffer mmap offset; [B] 0 */
    uint64_t mmap_size;    /* o: [A] shadow buffer mmap size; [B] 0 */
    uint32_t npu_page_size;/* o: NPU physical page size (bytes) */
    uint32_t npu_page_num; /* o: number of NPU physical pages */
};

struct npu_nds_unmap_args {
    uint64_t mgroup_id;    /* i: mgroup handle */
};

/* Path A io_args */
struct npu_nds_io_args {       /* Path A */
    uint32_t fd;               /* i: file descriptor */
    uint32_t opcode;           /* i: NDS_OPCODE_READ / WRITE */
    uint64_t mgroup_id;        /* i: mgroup handle */
    uint64_t file_offset;      /* i: file offset */
    uint64_t size;             /* i: IO size */
    /* out */
    uint64_t done;             /* o: completed bytes */
    int32_t  ret;              /* o: return code */
    uint32_t reserved;
};

/* Path B io_args (extended) */
struct npu_nds_io_args {       /* Path B */
    uint32_t fd;               /* i: file descriptor */
    uint32_t opcode;           /* i: NDS_OPCODE_READ / WRITE */
    uint64_t mgroup_id;        /* i: mgroup handle */
    uint64_t file_offset;      /* i: file offset */
    uint64_t size;             /* i: IO size */
    uint64_t npu_offset;       /* i: NPU HBM offset (Path B added) */
    uint32_t extent_cnt;       /* i: FIEMAP extent count (Path B added) */
    uint32_t reserved2;
    uint64_t extents_ptr;      /* i: pointer to npu_nds_fiemap_extent[] (Path B added) */
    /* out */
    uint64_t done;             /* o: completed bytes */
    int32_t  ret;              /* o: return code */
    uint32_t reserved3;
};

struct npu_nds_fiemap_extent { /* Path B only */
    uint64_t logical;     /* i: file logical offset */
    uint64_t physical;    /* i: disk physical offset (→ SLBA) */
    uint64_t length;      /* i: length */
    uint64_t npu_offset;  /* i: NPU HBM offset */
};
```

### B.3 Error Codes

| Error code | Meaning | Trigger scenario |
|---|---|---|
| `-ENOMEM` | out of memory | pin HBM / alloc_page failure |
| `-EINVAL` | invalid parameter | mgroup_id nonexistent / size misaligned |
| `-EIO` | IO error | mgroup state not ACTIVE / NVMe command failure |
| `-ENOSYS` | function not implemented | Path B: `nvme_alloc_request` not found |
| `-ENODEV` | device not present | Path B: non-NVMe block device |
| `-ENOTTY` | ioctl not supported | Path B: `FS_IOC_FIEMAP` not supported |
| `-EFAULT` | invalid userspace pointer | copy_from_user failure |
| `-ETIMEDOUT` | timeout | Path B: `wait_for_completion_timeout` 30s drain timeout + 1s abort grace timeout, batch abandoned |

---

*This document is merged from `docs/zh/nds/VFS/DESIGN_NPU.md` and `docs/zh/nds/NVMe/DESIGN_NPU.md`, describing the complete design of the npu_nds zero-copy driver framework, including two independent zero-copy paths and their shared infrastructure.*

---

## Appendix C: NVIDIA GPUDirect Storage (nvidia-fs) Design Reference

> This appendix is an independent analysis of the NVIDIA GDS `nvidia-fs.ko` v2.29.4 source code, serving as a benchmark reference for the npu_nds design. The full correspondence is in §7 of the main text.

---

> Based on the `NVIDIA/gds-nvidia-fs` repository v2.29.4 source analysis  
> Target: GPUDirect Storage (GDS) kernel-side `nvidia-fs.ko` driver  
> Userspace contract: CUDA cuFile API (`libcufile.so`, `cufile.h`)

---

### 1. Overview

#### 1.1 The Problem

The traditional data path between GPU and storage is:

```
NVMe SSD ──DMA──> CPU bounce/page cache ──memcpy──> GPU BAR ──> GPU memory
```

This path incurs triple overhead: CPU memory bandwidth consumption, CPU cache pollution, and double data copies. The goal of GPUDirect Storage (GDS) is to eliminate the intermediate CPU staging step, letting the NVMe controller (or RDMA NIC) directly DMA data into GPU memory via PCIe P2P:

```
NVMe SSD ──DMA(P2P)──> GPU BAR ──> GPU memory
```

#### 1.2 Role of nvidia-fs

`nvidia-fs` is a GPL v2 Linux kernel module that serves as the **kernel-side enabler for GDS**. It does not itself implement block device or filesystem semantics; rather, it:

1. Works with the NVIDIA kernel driver (`nvidia.ko`) via the NVIDIA P2P API (`nv-p2p.h`) to **pin (lock)** GPU memory and perform **DMA mapping**;
2. **Hooks callback functions** into the DMA paths of block device drivers (`nvme.ko`, `nvme-rdma`, etc.), so that when a bio's target page is a GPU page, the ordinary CPU physical address is replaced with the GPU BAR's P2P DMA bus address;
3. Exposes the `/dev/nvidia-fs` character device and a set of ioctls to userspace `libcufile`, which internally completes `cuFileRead`/`cuFileWrite` and similar calls through the cuFile API.

#### 1.3 Design Goals and Constraints

| Goal/Constraint | Description |
|---|---|
| Zero-copy direct access | NVMe/RDMA directly DMAs to GPU, bypassing CPU staging |
| Transparent compatibility | Supports both GPU pages (P2P path) and seamless fallback to ordinary CPU pages (traditional DIO path); can be mixed within the same request |
| Async and batch | Supports `O_DIRECT` sync/async, batch IO (up to 256 entries) |
| Safe reclamation | When GPU memory is freed via `cudaFree` or process exits, in-flight IO must safely terminate and reclaim mappings |
| Multiple storage backends | NVMe local, NVMe-oF (RDMA/TCP), ScaleFlux, Excelero, Lustre, BeeGFS, WekaFS, IBM Spectrum Scale (GPFS), NFS over RDMA |
| Portability | Single source compiles across kernels 3.x ~ 6.17+, relying on `configure` feature detection |
| Non-invasive to mainline | Does not patch kernel source; hooks only through "register/unregister" symbols exported by vendor drivers |

---

### 2. Implementation Approach

The nvidia-fs implementation approach can be summarized as a triad of **"shadow pages + vendor hooks + ioctl contract"**.

#### 2.1 Shadow Buffer Indirection Layer

**Core contradiction**: The block layer and filesystem O_DIRECT path assume that the `struct page` carried by a bio is a CPU-accessible regular memory page; however, GPU memory is exposed through the BAR and is not an addressable object within the `struct page` system, so it cannot be directly placed into a bio.

**Solution**: The driver constructs a "shadow buffer" as an intermediary:

- When userspace `cuFileBufRegister` calls `NVFS_IOCTL_MAP`, the driver calls `nvidia_p2p_get_pages[_persistent]` to pin GPU 64K pages, obtaining the GPU BAR physical page table;
- The driver uses `alloc_page(GFP_USER | __GFP_ZERO)` to allocate a batch of **4K shadow pages** (each 64K GPU page corresponds to 16 4K shadow pages), forming the mgroup's `nvfs_ppages[]`;
- Userspace performs `mmap` on `/dev/nvidia-fs` to map these shadow pages into the user address space (i.e., the CPU virtual address corresponding to the `devPtr` returned by `cuFileBufRegister`);
- Each shadow page's `page->index` is encoded as `base_index + relative_offset`, where `base_index` is a random 32-bit value (plus `NVFS_MIN_BASE_INDEX`), serving as the mgroup's globally unique key, stored in the RCU hash table `nvfs_io_mgroup_hash`;
- Key marker: the shadow page's `page->mapping == NULL`; the driver uses this to quickly identify "this is a GPU shadow page" rather than a regular page cache page in the block layer fast path.

This way, the O_DIRECT bio carries legal CPU pages (shadow pages), and the filesystem/block layer can proceed normally throughout; only at the **DMA mapping** step does the driver hook "translate" the shadow page back to its corresponding GPU BAR bus address.

```
Userspace devPtr (VA) ─mmap─> shadow page (struct page, alloc_page)
                              │ page->index = base_index + off
                              │ page->mapping = NULL  ◄── GPU page identification marker
                              ▼
                     nvfs_mgroup_from_page() ─RCU─> mgroup
                              │
                              ▼
              mgroup->gpu_info->page_table (nvidia_p2p page table)
                              │
                              ▼
              nvidia_p2p_dma_map_pages() ──> GPU BAR DMA address
```

#### 2.2 Vendor Driver Hooks (Not Kernel Patching)

To achieve "no kernel source modification," nvidia-fs chooses to hook on the registration symbols exposed by each **storage driver**:

- `nvfs-dma.c` maintains `modules_list[]`, each entry containing `(module_name, register_symbol_name, unregister_symbol_name, priority, ops pointer)`;
- Supported module keys (`nvfs-dma.h`): `nvme` / `nvme_rdma` / `scsi_mod` / `sfxvdriver` / `nvmeib_common` / `lnet` / `beegfs` / `mmfslinux` / `rpcrdma` / `wekafsio` / `scatefs`;
- On module load, `nvfs_blk_register_dma_ops` → `probe_module_list()`: for each entry, `symbol_get(register_symbol)` obtains the function pointer and calls it, handing the ops table to that driver; the driver stores it in a global variable (e.g., nvme's `nvme_pci_dma_set_ops`) and calls it on its own DMA path;
- On module unload, the reverse `symbol_put` and calls the unregister symbol.

The ops callback table (`nvfs_dma_rw_ops` / `nvfs_dma_rw_blk_iter_ops`) is the "instrumentation point" of the entire GDS data path:

| Hook | Purpose |
|---|---|
| `nvfs_blk_rq_map_sg` | Called by the block layer to build a scatterlist from bio pages. If all are GPU shadow pages, merges adjacent GPU physical pages and returns the segment count; if CPU pages or invalid pages are mixed, returns an error; if all are ordinary CPU pages, returns 0 to let the vendor driver take the default path (transparent fallback) |
| `nvfs_dma_map_sg_attrs` | Maps each segment of the sglist to a DMA bus address, internally calling `nvfs_get_dma` to look up the mgroup's cached mapping |
| `nvfs_dma_unmap_sg` | Releases DMA mapping references |
| `nvfs_is_gpu_page` / `nvfs_gpu_index` | Determines whether a page is a GPU shadow page, gets the GPU page index |
| `nvfs_device_priority` | Returns the P2P affinity priority between a GPU and a storage device, for multipath selection |
| V2 iterators (`*_iter_start/next`, `nvfs_dma_unmap_page`) | Kernel 6.17+ `blk_rq_dma_map_iter` model, returns one segment at a time |

This way, the main code of the block layer/NVMe driver needs no changes; only at the two points of "build sg" and "map sg" is there one extra "is this a GPU page?" branch decision.

#### 2.3 ioctl Contract (P2P Userspace Interface)

The sole interaction channel between userspace `libcufile` and the kernel is the ioctl on `/dev/nvidia-fs` (`nvfs-core.c: nvfs_ioctl`):

| ioctl | Direction | Purpose |
|---|---|---|
| `NVFS_IOCTL_MAP` | user→kernel | Pins GPU memory, builds shadow buffer and mgroup, returns the shadow CPU virtual address for `cuFileBufRegister` |
| `NVFS_IOCTL_READ` / `NVFS_IOCTL_WRITE` | bidirectional | Single O_DIRECT read/write, sync or async (`-EIOCBQUEUED`) |
| `NVFS_IOCTL_BATCH_IO` | bidirectional | Batch IO, up to `NVFS_MAX_BATCH_ENTRIES=256` entries |
| `NVFS_IOCTL_SET/GET/CLEAR_RDMA_REG_INFO` | bidirectional | Sets/queries/clears RDMA registration info (rkey/remote address/QP/gid) for an mgroup, enabling RDMA-based filesystems (GPFS) to directly perform RDMA on GPU memory |
| `NVFS_IOCTL_REMOVE` | — | Reserved, currently a no-op |

Parameters are loaded via `copy_from_user` into the union `nvfs_ioctl_param_union` (containing `map_args` / `ioargs` / `io_entries[]` and other variants, `nvfs-core.h`). Async completion results are returned through an additional **end_fence page**: on completion, the driver writes status/result/sparse hole info into this user page, and userspace polls it.

#### 2.4 Configure-driven Portability

The `configure` script detects kernel features at build time, generating a set of `HAVE_*` macros in `config-host.h`, for example:

- `HAVE_PIN_USER_PAGES_FAST` / `HAVE_GET_USER_PAGES_FAST_*`
- `HAVE_PROC_OPS_STRUCT` (newer kernels use `proc_ops`, older use `file_operations`)
- `HAVE_CLASS_CREATE_WITHOUT_MODULE_ARGS`
- `HAVE_CALL_READ_WRITE_ITER` / `HAVE_FILEMAP_RANGE_HAS_PAGE`
- `HAVE_BLK_RQ_DMA_MAP_ITER_START` (6.17+ iterator API)
- `HAVE_KIOCBCOMPLETE_3_ARG` / `HAVE_ACCESS_OK_3_ARG` / `HAVE_VM_FLAGS`

The source uses `#ifdef HAVE_*` to adapt to different kernel variants, achieving "one source, multi-version kernel compilation." `nvfs-kernel-interface.c` further wraps small differences like `access_ok` and `sg flags` into inline wrappers.

#### 2.5 Approach Summary (Data Path Panorama)

```
┌─────────────────────────────────────────────────────────────────────┐
│ Userspace:  cuFileRead/Write  ──>  libcufile  ──ioctl──>  /dev/nvidia-fs │
└─────────────────────────────────────────────────────────────────────┘
                                    │ NVFS_IOCTL_READ/WRITE
                                    ▼
┌────────────────────────── Kernel: nvidia-fs.ko ──────────────────────┐
│  nvfs_io_init (validate fd/inode/generation) → nvfs_io_start_op      │
│   │  slice shadow_buf_size blocks, set IO_QUEUED                       │
│   ▼                                                                   │
│  nvfs_direct_io: build iovec(shadow VA) → f_op->read_iter/write_iter (DIO) │
│   │  filesystem/block layer builds bio, bio pages = shadow pages (page->mapping=NULL) │
│   ▼                                                                   │
│  NVMe driver: nvfs_blk_rq_map_sg  ──> identify GPU pages, merge, generate sglist │
│   ▼                                                                   │
│  nvfs_dma_map_sg_attrs: nvfs_get_dma(shadow page→mgroup→P2P DMA cache) │
│   │  nvidia_p2p_dma_map_pages provides GPU BAR bus address             │
│   ▼                                                                   │
│  NVMe controller DMA: direct P2P to GPU BAR  (zero-copy!)             │
│   ▼                                                                   │
│  nvfs_io_complete (IRQ/softirq) → update stats → write end_fence page → release mgroup │
└────────────────────────────────────────────────────────────────────────┘
```

---

### 3. Implementation Principles

#### 3.1 P2P DMA Addressing Principle

GPU memory is not on the system DRAM bus; it is exposed through the GPU's PCIe BAR window. For the NVMe controller to DMA to it, a **bus address visible to the NVMe controller** must be obtained. The NVIDIA P2P API provides this capability:

- `nvidia_p2p_get_pages_persistent(...)` **pins** the GPU memory corresponding to a specified GPU virtual address range (preventing it from being swapped out/migrated), returning `nvidia_p2p_page_table_t` containing an array of GPU BAR physical pages (64K each);
- `nvidia_p2p_dma_map_pages(page_table, peer_pdev, ...)` establishes a P2P DMA mapping for a specific PCIe peer device (GPU↔NVMe), returning `nvidia_p2p_dma_mapping_t` containing the bus address array visible to that peer. The mapping is peer-dependent—the same GPU memory yields different bus addresses for GPU0, NVMe0, and an IB NIC;
- Therefore, the driver **caches DMA mappings by (mgroup, peer PCI device)**, stored in `gpu_info->buckets` hash table (keyed by peer PCI devid), RCU-protected, with a `dma_mapping_in_progress` flag + wait queue implementing "single-flight" (only one concurrent mapping per peer).

P2P routing itself is done at the PCIe hardware layer: when GPU and NVMe share the same PCIe Switch, it is switch-local P2P; otherwise, it loops through the root complex. This is the basis for the topology calculation in §3.6.

#### 3.2 Shadow Page Encoding and mgroup Addressing Principle

- `base_index` = `get_random_u32() + NVFS_MIN_BASE_INDEX`, serving as the mgroup's global key, preventing users from guessing and probing others' mgroups;
- Shadow page `page->index = base_index + block_offset_in_mgroup`;
- `nvfs_mgroup_from_page(page)`: decodes `base_index`, looks up in the RCU hash table via `hash_for_each_possible_rcu`, validates `magic`, `base_index`, and that the page is within `[0, npages)` range;
- On hit, takes a reference (`nvfs_mgroup_get`); the entire block layer fast path completes under `rcu_read_lock()`.

This mechanism enables "any bio shadow page → corresponding mgroup → GPU physical page → DMA mapping" to complete in O(1) + RCU, forming the cornerstone of the GDS hot path.

#### 3.3 State Machine Principle (Safe Reclamation)

GPU memory can be freed via `cudaFree` at any time, or a process may exit suddenly; at that point, in-flight DMAs may still exist. The driver coordinates with a two-layer atomic state machine:

**Block-level (per 4K block) state** — `nvfs_metadata[]`:
```
NVFS_IO_ALLOC ──> IO_FREE ──> IO_INIT ──> IO_QUEUED ──> IO_DMA_START ──> IO_DONE
                                          (on failure) ──> IO_DMA_ERROR
```

**IO-level (per IO) state** — `gpu_info.io_state`:
```
IO_FREE ──> IO_INIT ──> IO_READY ──> IO_IN_PROGRESS ──> IO_DONE
                                    │              └─> IO_TERMINATED
                                    └─> IO_CALLBACK_IN_PROGRESS
                                       └─> IO_UNPIN_PAGES_ALREADY_INVOKED
```

Three reclamation path trigger points converge at this state machine:
1. **NVIDIA P2P free callback** (`nvfs_get_pages_free_callback`): the GPU driver notifies that memory is about to be freed. The driver unregisters all DMA mappings, releases the page table, marks end_fence as dead, and advances the state to the termination branch;
2. **vma_close** (user munmap): shadow buffer is unmapped, also enters reclamation;
3. **Sync IO completion / async ki_complete**: advances to `IO_DONE`.

The mgroup is truly freed only when "all in-flight IO references reach zero" and "mappings are unregistered." Reference counting + `synchronize_rcu` ensures the block layer fast path never obtains a freed mgroup.

#### 3.4 RCU + Reference Counting Principle

mgroups are in a global RCU hash table; the block layer hot path (NVMe interrupt/DMA map) looks up the table and takes a reference via `nvfs_mgroup_get` under `rcu_read_lock`. The reclamation side removes from the hash table, then uses `synchronize_rcu`/`synchronize_rcu_expedited` to wait for all read-side critical sections to exit before freeing. This "lock-free read + deferred free" is the key to coexistence of high-performance data paths and safe reclamation.

#### 3.5 O_DIRECT Coordination Principle (Write Allocation and Sparse Read)

GDS uses O_DIRECT and must prevent the filesystem from falling back from DIO to buffered IO (which would go through page cache, losing zero-copy):

- **Write**: `nvfs_io_start_op` for WRITE first does `fallocate` (ensures blocks are allocated, avoiding DIO→buffered fallback), then `flush_dirty_pages` + invalidates page cache, ensuring DIO direct access;
- **Sparse read**: when reading an unallocated block (hole), NVMe does not return data, and the corresponding 4K block state does not advance to `IO_DMA_START`. The driver uses this to identify holes, recording up to `NVFS_MAX_HOLE_REGIONS` hole ranges in the end_fence page, so userspace can zero-fill the corresponding regions.

#### 3.6 PCIe Topology Affinity Principle

P2P performance/reachability depends on the relative position of GPU and storage in the PCIe tree:

- `nvfs-pci.c` enumerates all GPUs (display 3D/VGA), NVMe, IB, and Ethernet devices at init;
- Builds each device's BDF path bottom-up (through which Bridges, depth limit `MAX_PCI_DEPTH`);
- Computes the `gpu2peer` distance matrix, with sort key `(MAX_PCIE_BW_INDEX - bandwidth) | (pci_dist << 16)`—prioritizing PCI physical proximity, then bandwidth;
- Handles ACS (Access Control Services) redirection: certain topologies have ACS blocking P2P, which must be identified;
- NUMA distance as a fallback;
- Results are exposed to multipath storage via `nvfs_device_priority`, and observable via procfs `/proc/driver/nvidia-fs/peer_affinity` and `peer_distance`.

#### 3.7 RDMA Direct Access Principle

For RDMA-based filesystems like GPFS, not only NVMe P2P is possible, but also letting the RNIC directly RDMA read/write GPU memory:

- `NVFS_IOCTL_SET_RDMA_REG_INFO` stores the (rkey, remote virtual address, size, QP number, GID, LID, DC key) obtained from a one-time RDMA registration into the mgroup (`nvfs-rdma.c: nvfs_set_rdma_reg_info_to_mgroup`);
- `nvfs_get_gpu_sglist_rdma_info` (`nvfs-dma.c`) iterates over the sglist, looks up mgroup from pages, validates `shadow_buf_size`, assembles RDMA descriptors for the filesystem to directly initiate RDMA READ/WRITE on GPU memory.

#### 3.8 Observability and Fault Injection Principle

- **procfs** (`nvfs-proc.c`): `/proc/driver/nvidia-fs/{version, modules, bridges, devcount, stats, peer_affinity, peer_distance}`, where `modules` lists the hook status of each module in `modules_list[]` under `nvfs_module_mutex`;
- **stats** (`nvfs-stat.c`): a set of atomic counters—read/write throughput and latency, batch submission latency, map/unmap, mmap/munmap, sparse reads, various errors (`nvfs_n_err_*`), per-GPU (`nvfs_gpu_stat`: max_bar_memory_pinned, active_bar_memory_pinned, active_bounce_buffer_memory, etc.). Versioned with `NVFS_STAT_VERSION`, forming a userspace parsing contract;
- **debugfs fault injection** (`nvfs-fault.c`, `CONFIG_FAULT_INJECTION`): `/sys/kernel/debug/nvfs_inject_fault/` mounts multiple `fault_attr` objects (`nvfs_dma_error`, `nvfs_rw_verify_area_error`, `nvfs_end_fence_get_user_pages_fast_error`, `nvfs_invalid_p2p_get_page`, `nvfs_io_transit_state_fail`, `nvfs_pin_shadow_pages_error`, `nvfs_vm_insert_page_error`), using `should_fail()` to controllably inject errors at critical points, for exercising exception reclamation paths during development.

---

### 4. 4+1 Views

#### 4.1 Logical View

The logical view depicts the system's functional components and their relationships, without involving deployment or runtime.

```
                       ┌──────────────────────────────┐
                       │   libcufile.so (userspace, closed-source) │
                       │   cuFile API contract (cufile.h)  │
                       └───────────────┬──────────────┘
                                       │ ioctl(/dev/nvidia-fs)
                                       ▼
┌──────────────────────────── nvidia-fs.ko ─────────────────────────────┐
│                                                                       │
│  ┌─────────────── Control plane (nvfs-core.c) ───────────────┐        │
│  │ ioctl dispatch  nvfs_ioctl                              │        │
│  │ IO init         nvfs_io_init / nvfs_io_start_op          │        │
│  │ DIO main loop   nvfs_direct_io                           │        │
│  │ Complete/reclaim  nvfs_io_complete / nvfs_io_free        │        │
│  │ P2P mapping      nvfs_pin_gpu_pages / nvfs_get_p2p_dma_* │        │
│  │ mgroup lifecycle  nvfs_get_mgroup_from_vaddr/get/put     │        │
│  └───────────────────────────┬──────────────────────────────┘        │
│                              │                                        │
│  ┌────────── Memory plane (nvfs-mmap.c) ─────┴───────────┐           │
│  │ mgroup  : shadow pages + GPU page table + metadata state machine │ │
│  │ Shadow buffer: alloc_page, page->index encoding, mmap           │ │
│  │ Interval tree  : INTERVAL_TREE_GENERIC (page range indexing)    │ │
│  │ Addressing     : nvfs_mgroup_from_page / from_vaddr              │ │
│  └───────────────────────────┬──────────────────────────────┘        │
│                              │                                        │
│  ┌────────── Data plane (nvfs-dma.c) ──────────────────┐           │
│  │ modules_list[]: vendor driver hook table (priority + symbol names) │ │
│  │ nvfs_blk_register_dma_ops / probe_module_list           │        │
│  │ ops: rq_map_sg / dma_map_sg / is_gpu_page ...            │        │
│  │ nvfs_get_dma: shadow page→mgroup→P2P DMA cache           │        │
│  │ RDMA info extraction: nvfs_get_gpu_sglist_rdma_info      │        │
│  └───────────────────────────┬──────────────────────────────┘        │
│                              │                                        │
│  ┌────────── Topology plane (nvfs-pci.c) ──────────────────┐        │
│  │ Device enumeration / BDF path / gpu2peer distance matrix │        │
│  │ ACS check / affinity priority nvfs_device_priority       │        │
│  └──────────────────────────────────────────────────────────┘        │
│                                                                        │
│  ┌─ Auxiliary ──────────────────────────────────────────────────────┐ │
│  │ nvfs-batch.c      Batch IO (≤256 entries)                        │ │
│  │ nvfs-rdma.c       RDMA registration info access                  │ │
│  │ nvfs-stat.c       Atomic counter statistics                      │ │
│  │ nvfs-proc.c       procfs observability interface                 │ │
│  │ nvfs-fault.c      debugfs fault injection                        │ │
│  │ nvfs-kernel-interface.c  Kernel version difference wrappers     │ │
│  │ nvfs-mod.c        Module entry / parameters                      │ │
│  └──────────────────────────────────────────────────────────────────┘ │
│                                                                        │
└────────────────────────────────────────────────────────────────────────┘
        │ nvidia_p2p_* symbols                    │ hook callback registration
        ▼                                       ▼
┌───────────────────┐               ┌────────────────────────────────┐
│ nvidia.ko /        │               │ Block/RDMA drivers             │
│ nvidia-uvm.ko /    │  P2P API      │ nvme, nvme_rdma, scsi_mod,     │
│ nvidia-peermem.ko  │ ◄──────────── │ sfxvdriver, nvmeib_common,     │
│ (NVIDIA proprietary)│               │ lnet, beegfs, mmfslinux,       │
└───────────────────┘               │ rpcrdma, wekafsio, scatefs     │
                                      └────────────────────────────────┘
```

Key logical data structures:

| Structure | File | Responsibility |
|---|---|---|
| `nvfs_ioctl_param_union` | nvfs-core.h | ioctl input parameter union (map_args / ioargs / io_entries[]) |
| `nvfs_ioctl_map_t` | nvfs-core.h | MAP parameters: GPU vaddr, size, flags, peer GPU info |
| `nvfs_ioctl_ioargs_t` | nvfs-core.h | Single R/W parameters: file fd, offset, size, devPtr, opcode, cookie, end_fence, etc. |
| `nvfs_io_mgroup` | nvfs-mmap.h | Shadow buffer + GPU page table + metadata state + RDMA info + DMA buckets |
| `gpu_info` (nvfs_gpu_args) | nvfs-core.h | GPU memory range, base_index, io_state, page_table, dma buckets |
| `nvfs_io` | nvfs-core.h | One IO: mgroup, file, offset, size, state, end_fence |
| `nvidia_p2p_page_table_t` | nv-p2p.h | GPU BAR physical page array (64K/page) |
| `nvidia_p2p_dma_mapping_t` | nv-p2p.h | P2P DMA bus address array for a peer |
| `modules_list[]` / `nvfs_module_entry` | nvfs-dma.h | Vendor driver hook table |
| `nvfs_dma_rw_ops` / `nvfs_dma_rw_blk_iter_ops` | nvfs-dma.h | Hook callback table |
| `nvfs_gpu_stat` | nvfs-stat.h | per-GPU statistics |

#### 4.2 Process View

The process view describes runtime concurrency, communication, and synchronization.

##### 4.2.1 Synchronous Read/Write Main Flow

```
User thread            Kernel (nvidia-fs)          Block layer/NVMe        GPU/NVIDIA driver
   │                       │                          │                       │
   │ cuFileRead            │                          │                       │
   │──ioctl(READ)────────▶│ nvfs_ioctl               │                       │
   │                       │ nvfs_io_init             │                       │
   │                       │  fdget, inode validate,  │                       │
   │                       │  generation match        │                       │
   │                       │  mgroup←from_vaddr       │                       │
   │                       │  state READY→IN_PROGRESS  │                       │
   │                       │ nvfs_io_start_op         │                       │
   │                       │  WRITE: fallocate+flush   │                       │
   │                       │  loop slicing shadow_buf_size │                   │
   │                       │──read_iter/write_iter──▶│ build bio (shadow pages) │
   │                       │                          │ nvfs_blk_rq_map_sg ◀──┤
   │                       │                          │  identify GPU pages, merge sg │
   │                       │                          │ nvfs_dma_map_sg ◀─────┤
   │                       │   nvfs_get_dma           │  shadow page→mgroup→DMA cache │
   │                       │   ◀── nvidia_p2p_dma_map_pages ──────────────────┤ GPU BAR address
   │                       │                          │──DMA(P2P)────────────▶│ direct to memory
   │                       │                          │◀─completion interrupt─────┤
   │                       │  nvfs_io_complete        │                       │
   │                       │   update stats, write end_fence │               │
   │                       │   nvfs_io_free, put mgroup│                      │
   │◀──ioctl return/ssize_t─│                          │                       │
```

##### 4.2.2 Async Completion and Concurrency Key Points

- **Async**: Synchronous `read_iter` returns byte count upon completion; if it returns `-EIOCBQUEUED`, the driver sets `kiocb->ki_complete = nvfs_io_complete`, the user thread immediately returns from ioctl, completion occurs asynchronously in **interrupt/softirq context**, and the result is written to the end_fence page for user polling;
- **per-cpu ops counter** (`nvfs_n_ops`): before module unload, all in-flight IO must reach zero, preventing callbacks from entering an already-unloaded module;
- **DMA mapping single-flight**: for the same (mgroup, peer), `nvidia_p2p_dma_map_pages` uses the `dma_mapping_in_progress` flag + waitqueue to ensure only one concurrent execution; other waiters reuse the result;
- **Reclamation vs in-flight IO race**: P2P free callback / vma_close / IO completion three-way concurrency, serialized by the state machine + reference counting + `synchronize_rcu`, see §3.3.

##### 4.2.3 Three Critical Concurrency Paths

```
Path A (data):   ioctl thread ─DIO─> block layer ─rq_map_sg/dma_map_sg─> NVMe DMA ─completion IRQ─> stats/end_fence
Path B (reclaim): NVIDIA free_cb or vma_close ─> unregister DMA ─> state machine termination ─> free mgroup
Path C (topology): module init ─> enumerate PCI ─> distance matrix ─> expose to nvfs_device_priority (read-mostly)
```

A and B are decoupled via RCU + reference counting; C completes during init, read-only at runtime.

#### 4.3 Development View

The development view focuses on source organization, build, and dependencies.

##### 4.3.1 Source Modules (27 files)

```
gds-nvidia-fs/src/
├── nvfs-mod.c                Module entry/parameters/license (≈90L)
├── nvfs-core.c               ★ Control plane: ioctl/io_init/io_complete/mgroup lifecycle (2329L)
├── nvfs-core.h               ioctl parameter union, io state, io interface
├── nvfs-dma.c                ★ Data plane: vendor hooks, sg build/DMA map/RDMA info (1529L)
├── nvfs-dma.h                modules_list, ops table, NVME_MAX_SEGS=127
├── nvfs-mmap.c               ★ Memory plane: shadow pages/interval tree/mgroup addressing (1258L)
├── nvfs-mmap.h               mgroup, metadata, NVFS_BLOCK_SIZE=4096
├── nvfs-pci.c                Topology: BDF path/distance matrix/ACS/affinity (783L)
├── nvfs-pci.h
├── nvfs-batch.c              Batch IO (162L, NVFS_BATCH_SUPPORT)
├── nvfs-batch.h              NVFS_MAX_BATCH_ENTRIES=256
├── nvfs-rdma.c               RDMA registration info (214L, NVFS_ENABLE_KERN_RDMA_SUPPORT)
├── nvfs-rdma.h
├── nvfs-stat.c               Atomic statistics (651L)
├── nvfs-stat.h               Statistics counters, NVFS_STAT_VERSION, NVFS_MAX_GPU=16
├── nvfs-proc.c               procfs (266L)
├── nvfs-fault.c              debugfs fault injection (64L, CONFIG_FAULT_INJECTION)
├── nvfs-fault.h
├── nvfs-kernel-interface.c   Kernel version difference wrappers (65L)
├── nvfs-kernel-interface.h
├── nvfs-vers.h               Version string
├── nvfs-p2p.h                NVIDIA P2P API re-declaration (avoids depending on nvidia-uvm header)
├── config-host.h             Generated by configure: HAVE_* feature macros
├── Makefile                  Build
└── dkms.conf                 DKMS registration
```

##### 4.3.2 Build and Feature Detection

```
configure (shell)  ──detect kernel──▶  config-host.h  (HAVE_* macros)
        │
        ▼
   Makefile ──KBUILD──▶  nvidia-fs.ko
        │
        ▼
   dkms.conf ──DKMS──▶  auto-rebuild on kernel upgrade
```

Example `configure` detection items: `pin_user_pages_fast`, `struct proc_ops`, `class_create` parameter form, `call_read_write_iter`, `filemap_range_has_page`, `blk_rq_dma_map_iter`, `ki_complete` parameter form, `access_ok` parameter form, `vm_flags`. The source uses `#ifdef HAVE_*` to branch at each difference point, achieving "one source across kernels 3.x~6.17+".

##### 4.3.3 External Dependencies (Symbols)

- **NVIDIA P2P API**: `nvidia_p2p_get_pages` / `nvidia_p2p_get_pages_persistent` / `nvidia_p2p_put_pages_persistent` / `nvidia_p2p_dma_map_pages` / `nvidia_p2p_dma_unmap_pages` / `nvidia_p2p_free_callback` (exported by `nvidia.ko`/`nvidia-uvm.ko`). At runtime, reads `/sys/module/nvidia/version` to determine ≥555 for persistent API;
- **Block/RDMA vendor drivers**: their exported "register/unregister ops" symbols (via `symbol_get/put`);
- **Linux kernel**: misc device, misc class, bio, blk-mq, scatterlist, interval tree, jhash, procfs, debugfs, fault-inject framework.

##### 4.3.4 Layered Architecture

```
Presentation layer:  nvfs-mod.c | nvfs-proc.c | nvfs-fault.c | nvfs-core.c (ioctl)
Control layer:       nvfs-core.c (io_init/start_op/direct_io/complete) | nvfs-batch.c
Data layer:          nvfs-dma.c (hooks+sg+map) | nvfs-rdma.c | nvfs-pci.c
Memory layer:        nvfs-mmap.c (shadow pages/mgroup/interval tree)
Adaptation layer:    nvfs-kernel-interface.c | config-host.h (HAVE_*)
Support layer:       nvfs-stat.c | nvfs-vers.h | nvfs-p2p.h
```

#### 4.4 Physical View

The physical view describes the deployed hardware/software topology and resources.

##### 4.4.1 Hardware Topology

```
                          CPU + DRAM (NUMA node)
                                │ PCIe Root Complex
                ┌───────────────┴────────────────┐
        ┌───────Switch A────────┐        ┌───────Switch B────────┐
        │   GPU0   GPU1   NVMe0 │        │   GPU2   NVMe1  RNIC │
        │  (BAR)  (BAR)         │        │  (BAR)              │
        └───────────────────────┘        └─────────────────────┘
   P2P optimal zone (GPU↔NVMe same switch)   Cross-zone P2P via RC, worse performance
```

nvidia-fs's `nvfs-pci.c` maps this tree at startup, computes `gpu2peer` distance, and selects the nearest GPU's NVMe for multipath storage.

##### 4.4.2 Software Deployment and Nodes

| Node | Contents |
|---|---|
| GPU memory | Pinned 64K BAR physical pages (`nvidia_p2p_page_table_t`) |
| CPU memory (shadow) | Per-mgroup batch of 4K `alloc_page` shadow pages, `NVFS_BLOCK_SIZE=4096`, mapped to user via mmap |
| CPU memory (fallback) | Ordinary bounce/page cache used for non-GPU pages (traditional DIO path) |
| Character device | `/dev/nvidia-fs0..N` (`MAX_NVFS_DEVICES`, default by available GPU count) |
| procfs | `/proc/driver/nvidia-fs/{version,modules,bridges,devcount,stats,peer_affinity,peer_distance}` |
| debugfs | `/sys/kernel/debug/nvfs_inject_fault/*` |
| sysfs read | `/sys/module/nvidia/version` (determine persistent API) |
| Kernel modules | `nvidia-fs.ko` (this driver) + `nvidia.ko`/`nvidia-uvm.ko` (P2P API) + various storage drivers |

#### 4.5 Scenario View (Scenarios / Use Cases)

##### Scenario 1: Synchronous cuFileRead from NVMe to GPU

1. User `cuFileBufRegister(devPtr)`: libcufile sends `NVFS_IOCTL_MAP`, driver pins GPU memory, builds shadow buffer, returns shadow CPU vaddr;
2. User `cuFileRead(devPtr, fh, offset, size)`: libcufile sends `NVFS_IOCTL_READ`, driver `nvfs_io_init` validates file fd/inode/generation, gets mgroup, transitions to IN_PROGRESS;
3. `nvfs_io_start_op` slices blocks, sets IO_QUEUED, `nvfs_direct_io` calls `f_op->read_iter`;
4. Block layer builds bio with shadow pages; NVMe driver calls `nvfs_blk_rq_map_sg` to merge GPU pages, `nvfs_dma_map_sg_attrs` to get P2P DMA addresses;
5. NVMe controller DMA directly to GPU BAR (zero-copy);
6. Completion callback `nvfs_io_complete` updates stats, `nvfs_io_free` releases mgroup, ioctl returns ssize_t.

##### Scenario 2: Async cuFileWrite (polling end_fence)

1. Same registration as 1;
2. `NVFS_IOCTL_WRITE`, driver first does `fallocate` + `flush_dirty_pages` + invalidate page cache (prevent DIO fallback);
3. `read_iter/write_iter` returns `-EIOCBQUEUED`, user thread immediately returns, `ki_complete=nvfs_io_complete`;
4. Completion in interrupt context → write end_fence page (status/result/offset);
5. User polls end_fence to learn of completion.

##### Scenario 3: Batch IO (cuFileBatch)

1. `NVFS_IOCTL_BATCH_IO`: `nvfs_io_batch_init` does `copy_from_user` + `nvfs_io_init` for each entry;
2. `nvfs_io_batch_submit` calls `nvfs_io_start_op` for each, records `start_io` (ktime) to measure batch submission latency;
3. Each IO completes independently, state written to the corresponding end_fence slot.

##### Scenario 4: RDMA Filesystem (GPFS) Direct to GPU

1. GPFS userspace registers GPU buffer, sends `NVFS_IOCTL_SET_RDMA_REG_INFO` to store the rkey/remote address/QP/gid from one RDMA registration into mgroup;
2. GPFS kernel driver calls `nvfs_get_gpu_sglist_rdma_info` to get RDMA descriptors for GPU memory;
3. RNIC directly performs RDMA READ/WRITE on GPU BAR, bypassing CPU bounce.

##### Scenario 5: cudaFree During In-Flight IO

1. User calls `cudaFree` on registered memory;
2. NVIDIA driver triggers `nvfs_get_pages_free_callback`;
3. Driver unregisters all DMA mappings, releases page table, marks end_fence dead, state machine enters termination branch;
4. After in-flight IO completes and references reach zero, `synchronize_rcu` then truly frees mgroup;
5. If user later reads devPtr again, end_fence is marked dead, userspace gets error and re-registers.

##### Scenario 6: Mixed CPU/GPU Pages in Same Request (Transparent Fallback)

1. bio contains both CPU pages and GPU shadow pages: `nvfs_blk_rq_map_sg` detects mixing → increments `nvfs_n_err_mix_cpu_gpu` and returns error;
2. All CPU pages: `nvfs_blk_rq_map_sg` returns 0, vendor driver takes default DMA path (traditional DIO);
3. All GPU pages: takes P2P path.

---

### 5. Key Invariants and Design Trade-offs

| Invariant | Guarantee mechanism |
|---|---|
| Shadow page always maps back to unique mgroup | `page->index` = `base_index+off`, hash lookup + magic/range validation |
| P2P DMA address stable for a peer | Cached by (mgroup, peer devid) + single-flight + RCU read |
| GPU memory not freed during in-flight IO | pin (P2P API) + reference counting + state machine |
| Does not break ordinary IO (non-GPU pages) | `is_gpu_page` check, returns 0 for all-CPU to let driver take default path |
| Cross-kernel compilable | `configure`→`config-host.h` `HAVE_*` adaptation |

**Main trade-offs**:

- **Shadow page 4K vs GPU page 64K**: Shadow pages use 4K to fit block layer PAGE_SIZE; each 64K GPU page is split into 16 shadow pages, increasing page table entries but gaining native block layer compatibility;
- **DMA mapping cache vs real-time mapping**: Caching reduces `nvidia_p2p_dma_map_pages` overhead (expensive call), at the cost of needing to traverse buckets to unregister on reclamation;
- **RCU read vs write-side latency**: Read side is lock-free and fast, but reclamation side `synchronize_rcu` can be slow; `_expedited` is used to accelerate critical paths;
- **Vendor hooks vs kernel patching**: Hooks keep the driver independent and upgradeable with the kernel, but depend on vendor drivers continuing to export registration symbols, limited by their evolution.

---

### 6. Observability and Ops Interface Quick Reference

| Interface | Path | Contents |
|---|---|---|
| Version | `/proc/driver/nvidia-fs/version` | Driver version, NVIDIA driver version, P2P mode (persistent/legacy) |
| Modules | `/proc/driver/nvidia-fs/modules` | Vendor driver hook status |
| Stats | `/proc/driver/nvidia-fs/stats` | Read/write throughput and latency, batch, map, sparse read, errors, etc. (versioned with `NVFS_STAT_VERSION`) |
| Affinity | `/proc/driver/nvidia-fs/peer_affinity` | GPU↔peer priority |
| Distance | `/proc/driver/nvidia-fs/peer_distance` | GPU↔peer distance matrix |
| Fault injection | `/sys/kernel/debug/nvfs_inject_fault/*` | `fault_attr` at each critical point |
| Module parameters | `modprobe nvidia-fs ...` | `max_devices`, `dbg_enabled`, `info_enabled`, `peer_stats_enabled`, `rw_stats_enabled`, `use_legacy_p2p_allocation` |

---

### Appendix A: Source Index (Key Function Locator)

| Function | Function | File:line (approx.) |
|---|---|---|
| ioctl dispatch | `nvfs_ioctl` | nvfs-core.c |
| IO init validation | `nvfs_io_init` | nvfs-core.c |
| IO start main loop | `nvfs_io_start_op` | nvfs-core.c |
| DIO initiation | `nvfs_direct_io` | nvfs-core.c |
| Async completion | `nvfs_io_complete` | nvfs-core.c |
| Free/reclaim | `nvfs_io_free` | nvfs-core.c |
| GPU page pin | `nvfs_pin_gpu_pages` | nvfs-core.c |
| P2P DMA mapping | `nvfs_get_p2p_dma_mapping` | nvfs-core.c |
| DMA fast path | `nvfs_get_dma` | nvfs-core.c |
| P2P free callback | `nvfs_get_pages_free_callback` | nvfs-core.c |
| mgroup addressing (page) | `nvfs_mgroup_from_page` | nvfs-mmap.c |
| mgroup addressing (vaddr) | `nvfs_get_mgroup_from_vaddr` | nvfs-mmap.c |
| Vendor hook registration | `nvfs_blk_register_dma_ops`/`probe_module_list` | nvfs-dma.c |
| sg construction | `nvfs_blk_rq_map_sg` | nvfs-dma.c |
| DMA map sg | `nvfs_dma_map_sg`/`nvfs_dma_map_sg_attrs` | nvfs-dma.c |
| RDMA info extraction | `nvfs_get_gpu_sglist_rdma_info` | nvfs-dma.c |
| PCI topology | `nvfs_bdf2pdevinfo` | nvfs-pci.c |
| Affinity priority | `nvfs_device_priority` | nvfs-pci.c |
| Batch IO | `nvfs_io_batch_init`/`submit` | nvfs-batch.c |
| RDMA registration | `nvfs_set_rdma_reg_info_to_mgroup` | nvfs-rdma.c |
| Statistics | counter definitions/proc show | nvfs-stat.c |
| procfs | `nvfs_modules_show` etc. | nvfs-proc.c |
| Fault injection | `nvfs_init_debugfs` | nvfs-fault.c |
| Module entry | `nvfs_init`/`nvfs_exit` | nvfs-mod.c / nvfs-core.c |

---

*This document is based on analysis of the full `gds-nvidia-fs` v2.29.4 source code. Function line numbers are approximate; refer to the source for precise locations.*
