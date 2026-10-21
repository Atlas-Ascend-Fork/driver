# NPU Direct Storage (NDS) demo

## Description

This sample is the Ascend NPU counterpart of NVIDIA's GPUDirect Storage (GDS)
`nvidia-fs.ko` driver. It implements the **VFS shadow buffer + vendor hooks**
zero-copy path (Path A) and ships both a **kernel-space** module and a
**user-space** demo:

- **Kernel module `npu_nds.ko`** creates the character device `/dev/npu-nds`
  and exposes an ioctl contract:
  - `NDS_IOCTL_MAP` —— pin NPU device memory (`hal_kernel_p2p_get_pages`, the
    Ascend counterpart of `nvidia_p2p_get_pages`), allocate 4K shadow pages,
    build an mgroup.
  - `NDS_IOCTL_READ` / `NDS_IOCTL_WRITE` —— O_DIRECT transfer between a file
    and the shadow pages (mirrors `NPUFS_IOCTL_READ/WRITE`).
  - `NDS_IOCTL_UNMAP` —— free shadow pages, call `hal_kernel_p2p_put_pages`
    (mirrors `NPUFS_IOCTL_REMOVE`).
- **User-space client library `libnpu_file.so`** mirrors `libcufile` but
  depends **only on libc and the NDS kernel ioctl contract** — no HAL or TSD
  symbols. It wraps the `/dev/npu-nds` ioctls and the shadow-buffer mmap into
  a small API — `npu_file_open` / `npu_file_buf_register` / `npu_file_read` /
  `npu_file_write` / `npu_file_buf_deregister` / `npu_file_close`.
- **User-space demo `npu_nds_demo`** links `libnpu_file.so` + the HAL
  user-space API (`drvMemDeviceOpen` / `halMemAlloc` / `drvMemsetD8` /
  `drvMemcpy`) and mirrors cufile-samples/cufile_rw.cc, exercising the full
  "open device -> alloc -> fill -> write -> clear -> read -> verify ->
  deregister -> close device" flow. Command-line args: `path` + `size`
  (aligned to 4096).

> The demo source lives in `docs/en/nds/demo/demo_main.c`, shared with the
> NVMe path. Linking against the respective `libnpu_file.so` selects the
> zero-copy path.

### Zero-copy principle (VFS shadow buffer + vendor hooks)

At module load, `__symbol_get` looks up the
`nvme_v1_register_nvfs_dma_ops` symbol exported by the (patched) NVMe driver
(falls back to `nvme_v2_register_nvfs_dma_ops` if v1 is absent) and registers
a `struct npufs_dma_rw_ops` ops table (6 hook functions). When the NVMe driver
builds the DMA sglist it calls `npu_blk_rq_map_sg` / `npu_dma_map_sg_attrs`,
which replace shadow-page CPU PAs with NPU HBM P2P DMA bus addresses — the
NVMe controller DMAs directly to/from NPU HBM. If the NVMe driver is not
patched (`__symbol_get` returns NULL), the module gracefully degrades to
bounce-buffer mode.

### Design mapping (nvidia-fs -> npu_nds)

| nvidia-fs design point (see `design.md` Appendix C) | npu_nds counterpart |
|---|---|
| `nvidia_p2p_get_pages_persistent` to pin GPU memory | `hal_kernel_p2p_get_pages` to pin NPU HBM |
| `nvidia_p2p_put_pages` | `hal_kernel_p2p_put_pages` |
| `nvidia_p2p_page_table_t` | `struct p2p_page_table` (with `pages_info[].pa`) |
| 4K shadow pages via `alloc_page`, `page->mapping=NULL` marker | same 4K shadow pages, `page->mapping=NULL`, `page->index=(mgid<<12)+off` |
| `npufs_io_mgroup` hash table + random base index | `struct nds_mgroup` list + `atomic64_inc_return` key |
| `npufs_get_pages_free_callback` teardown | `nds_p2p_free_callback` state-machine termination |
| `NPUFS_IOCTL_MAP/READ/WRITE/REMOVE` | `NDS_IOCTL_MAP/READ/WRITE/UNMAP` |
| Vendor hooks (`npufs_dma_rw_ops`) for zero copy | **implemented**: `__symbol_get` registers `npu_dma_rw_ops` (6 hook functions) |

## Directory layout

```
VFS/
├── npu_nds.h               # ioctl contract shared by kernel and user
├── CMakeLists.txt          # top-level CMake, builds lib + demo
├── kernel/
│   ├── npu_nds_hook.h      # vendor hook definitions: npufs_dma_rw_ops struct, feature bits, v1/v2 symbols
│   ├── npu_nds_kern.c      # kernel module: shadow buffer + P2P pin + ioctl + vendor hooks + state machine
│   └── Makefile            # Kbuild Makefile
├── lib/
│   ├── npu_file.h          # client library public API (mirrors cuFile API)
│   ├── npu_file.c          # client library impl (mmap shadow buffer + ioctl, no HAL)
│   └── CMakeLists.txt      # builds libnpu_file.so (no external link deps)
├── demo/
│   └── CMakeLists.txt      # builds npu_nds_demo (references shared ../../demo/demo_main.c)
├── run.sh                  # build+load kernel module, build+run user demo
└── README.md
```

> The shared demo source `demo_main.c` is in `docs/en/nds/demo/`, common to
> both paths.

## Supported products

- Atlas A2 training / Atlas A2 inference
- Atlas A3 training / Atlas A3 inference
- Ascend 950PR / Ascend 950DT

> The support matrix of `hal_kernel_p2p_get_pages` is governed by its
> declaration in `ascend_kernel_hal.h`; computing-power-grouped
> (ascend310P/ascend910) scenarios are not guaranteed.

## Environment

- NPU driver/firmware and CANN packages installed (see the CANN Software
  Installation Guide, "Installing NPU Driver Firmware").
- The running host kernel must match the kernel tree the driver was built
  against, otherwise `npu_nds.ko` cannot be `insmod`-ed.
- Zero-copy P2P requires the NVMe driver to be patched with npufs hooks
  (exporting `nvme_v1_register_nvfs_dma_ops` or `nvme_v2_register_nvfs_dma_ops`).
- Root privileges are required for `insmod` / `rmmod`.

## Build & run

```bash
cd ${git_clone_path}/docs/en/nds/VFS
sudo bash run.sh
```

`run.sh` will:
1. read `/etc/ascend_install.info`, source `setenv.bash` to obtain kernel
   header paths such as `DRIVER_HAL_INC_DIR`;
2. `make -C /lib/modules/$(uname -r)/build M=kernel modules` to produce
   `npu_nds.ko`;
3. `insmod npu_nds.ko` which creates `/dev/npu-nds` (and `rmmod` on exit);
4. `cmake` to produce `libnpu_file.so` + `npu_nds_demo`;
5. run `npu_nds_demo` to exercise the full "open device -> register -> read ->
   verify -> write -> verify -> deregister -> close device" flow, logging to
   `output_msg.txt`.

## CANN Driver API

Kernel-space:
- `hal_kernel_p2p_get_pages` —— pin NPU virtual-address-backed physical pages,
  returns `p2p_page_table` (counterpart of `nvidia_p2p_get_pages`).
- `hal_kernel_p2p_put_pages` —— paired release.
- `struct p2p_page_table` / `struct p2p_page_info` —— NPU physical page
  descriptors; `pages_info[].pa` is the source of the P2P bus addresses.

User-space (used by demo, not by client library):
- `halMemAlloc` —— allocate NPU HBM
  (`MEM_DEV | devid | MEM_TYPE_HBM | MEM_PAGE_HUGE`).
- `halMemFree` —— free.
- `drvMemsetD8` —— fill (0xAB pattern) and clear NPU HBM (≈ `cudaMemset`);
  **not in the zero-copy data path**.
- `drvMemcpy` —— used only for D2H verify (≈ `cudaMemcpy` D2H);
  **not in the zero-copy data path**.
- `drvMemDeviceOpen` / `drvMemDeviceClose` —— open/close device context.

Client library API (`libnpu_file.so`, counterpart of `libcufile`,
depends only on libc + NDS ioctl):

| cuFile API | npu_file API | Description |
|---|---|---|
| `cuFileDriverOpen` | `npu_file_open` | open `/dev/npu-nds` (does not manage NPU device context) |
| `cuFileBufRegister` | `npu_file_buf_register` | ioctl MAP pin NPU memory + alloc shadow buffer + mmap |
| `cuFileRead` | `npu_file_read` | file O_DIRECT -> shadow buffer |
| `cuFileWrite` | `npu_file_write` | shadow buffer -> file O_DIRECT |
| `cuFileBufDeregister` | `npu_file_buf_deregister` | munmap + deregister mgroup |
| `cuFileDriverClose` | `npu_file_close` | close `/dev/npu-nds` |

> `npu_file_read/write` use the unified 6-argument signature (with
> `npu_offset`); the VFS library ignores this parameter (the offset is
> determined by the shadow-buffer mmap). NPU device-context management
> (`drvMemDeviceOpen` / `TsdOpen`), memory allocation (`halMemAlloc`), memset
> (`drvMemsetD8`), and data transfer (`drvMemcpy`) are the caller's (demo's)
> responsibility and are not part of the client library.

## Known issues / limitations

- Zero-copy P2P requires the NVMe driver to be patched with npufs hooks
  (exporting `nvme_v1_register_nvfs_dma_ops` or
  `nvme_v2_register_nvfs_dma_ops`); without the patch, the module
  automatically degrades to bounce-buffer mode (`drvMemcpy` data transfer).
- `nds_peer_device` uses PCI storage class to match NVMe controllers; a
  production driver should maintain a distance matrix.
- `nds_mgroup` uses a mutex-protected list instead of an RCU hash table for
  readability; the hot-path teardown should follow the RCU + refcount model of
  `npufs-mmap.c`.
- Async IO and batch IO (`NPUFS_IOCTL_BATCH_IO`) are not implemented; only the
  synchronous path is demonstrated.
