# NPU Direct Storage (NDS) demo

## Description

This sample is the Ascend NPU counterpart of NVIDIA's GPUDirect Storage (GDS)
`nvidia-fs.ko` driver. It implements the **NVMe passthrough + tracepoint probe**
zero-copy path (Path B) and ships both a **kernel-space** module and a
**user-space** demo:

- **Kernel module `npu_nds.ko`** creates the character device `/dev/npu-nds`
  and exposes an ioctl contract:
  - `NDS_IOCTL_MAP` —— pin NPU device memory (`hal_kernel_p2p_get_pages`, the
    Ascend counterpart of `nvidia_p2p_get_pages`) and build an mgroup.
  - `NDS_IOCTL_READ` / `NDS_IOCTL_WRITE` —— build NVMe passthrough commands
    (`RQF_NVME_PT`) whose SGL points straight at the NPU HBM P2P DMA bus
    address (mirrors `NPUFS_IOCTL_READ/WRITE`).
  - `NDS_IOCTL_UNMAP` —— call `hal_kernel_p2p_put_pages` (mirrors
    `NPUFS_IOCTL_REMOVE`).
- **User-space client library `libnpu_file.so`** mirrors `libcufile` but
  depends **only on libc and the NDS kernel ioctl contract** — no HAL or TSD
  symbols. It wraps the `/dev/npu-nds` ioctls and `FS_IOC_FIEMAP` queries into
  a small API — `npu_file_open` / `npu_file_buf_register` / `npu_file_read` /
  `npu_file_write` / `npu_file_buf_deregister` / `npu_file_close`.
- **User-space demo `npu_nds_demo`** links `libnpu_file.so` + the HAL
  user-space API (`drvMemDeviceOpen` / `halMemAlloc` / `drvMemsetD8` /
  `drvMemcpy`) and mirrors cufile-samples/cufile_rw.cc, exercising the full
  "open device -> alloc -> fill -> write -> clear -> read -> verify ->
  deregister -> close device" flow. Command-line args: `path` + `size`
  (aligned to 4096).

> The demo source lives in `docs/en/nds/demo/demo_main.c`, shared with the VFS
> path. Linking against the respective `libnpu_file.so` selects the zero-copy
> path.

### Zero-copy principle (NVMe passthrough)

At module load, `__symbol_get` resolves `nvme_alloc_request` and
`blk_abort_request`, and `for_each_kernel_tracepoint` locates the
`nvme_setup_cmd` tracepoint to register a probe.

During READ/WRITE:
1. The user-space library queries the file's on-disk layout via
   `FS_IOC_FIEMAP` (extents);
2. The kernel builds an NVMe passthrough request (`RQF_NVME_PT`) per extent,
   with the SGL pointing directly at the `dma_map_resource`-mapped NPU HBM BAR
   bus address;
3. The `nvme_setup_cmd` tracepoint probe rewrites the SGL type to
   `NVME_CMD_SGL_METABUF` (P2P-friendly);
4. The NVMe controller DMAs directly to/from NPU HBM — **zero-copy, no CPU
   bounce**.

If `nvme_alloc_request` is unavailable, READ/WRITE return `-ENOSYS` (fallback:
user space can use ordinary O_DIRECT + `drvMemcpy` bounce).

### Design mapping (nvidia-fs -> npu_nds)

| nvidia-fs design point (see `design.md` Appendix C) | npu_nds counterpart |
|---|---|
| `nvidia_p2p_get_pages_persistent` to pin GPU memory | `hal_kernel_p2p_get_pages` to pin NPU HBM |
| `nvidia_p2p_put_pages` | `hal_kernel_p2p_put_pages` |
| `nvidia_p2p_page_table_t` | `struct p2p_page_table` (with `pages_info[].pa`) |
| `nvidia_p2p_dma_map_pages` | `dma_map_resource` (Linux DMA API) |
| `nvidia_p2p_free_callback` | `nds_p2p_free_callback` state-machine termination |
| `NPUFS_IOCTL_MAP/READ/WRITE/REMOVE` | `NDS_IOCTL_MAP/READ/WRITE/UNMAP` |
| Vendor hooks (`npufs_dma_rw_ops`) | `nvme_alloc_request` + `nvme_setup_cmd` tracepoint probe |
| Shadow pages + mmap | **not needed** (SGL points directly at NPU HBM DMA address) |

## Directory layout

```
NVMe/
├── npu_nds.h               # ioctl contract shared by kernel and user (incl. FIEMAP extent)
├── CMakeLists.txt          # top-level CMake, builds lib + demo
├── kernel/
│   ├── npu_nds_hook.h      # NVMe PT defs: RQF_NVME_PT, SGL constants, symbol soft-lookup
│   ├── npu_nds_kern.c      # kernel module: P2P pin + NVMe PT submit + ioctl + state machine
│   └── Makefile            # Kbuild Makefile
├── lib/
│   ├── npu_file.h          # client library public API (mirrors cuFile API)
│   ├── npu_file.c          # client library impl (FIEMAP + ioctl, no HAL)
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
- The kernel must export `nvme_alloc_request` (mainline NVMe driver already
  does); otherwise READ/WRITE return `-ENOSYS`.
- Root privileges are required for `insmod` / `rmmod`.

## Build & run

```bash
cd ${git_clone_path}/docs/en/nds/NVMe
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
| `cuFileBufRegister` | `npu_file_buf_register` | ioctl MAP pin NPU memory, build mgroup |
| `cuFileRead` | `npu_file_read` | FIEMAP + NVMe PT: file -> NPU HBM |
| `cuFileWrite` | `npu_file_write` | FIEMAP + NVMe PT: NPU HBM -> file |
| `cuFileBufDeregister` | `npu_file_buf_deregister` | deregister mgroup |
| `cuFileDriverClose` | `npu_file_close` | close `/dev/npu-nds` |

> `npu_file_read/write` use the unified 6-argument signature (with
> `npu_offset`); the NVMe library runs `FS_IOC_FIEMAP` internally and passes
> the extents to the kernel. NPU device-context management
> (`drvMemDeviceOpen` / `TsdOpen`), memory allocation (`halMemAlloc`), memset
> (`drvMemsetD8`), and data transfer (`drvMemcpy`) are the caller's (demo's)
> responsibility and are not part of the client library.

## Known issues / limitations

- Zero-copy requires the kernel to export `nvme_alloc_request`; if missing,
  READ/WRITE return `-ENOSYS`.
- The `nvme_setup_cmd` tracepoint probe rewrites the SGL type to
  `NVME_CMD_SGL_METABUF` (=0x00); this value needs vendor confirmation.
- `nsid` is hardcoded to 1 (demo simplification).
- Each FIEMAP extent produces one NVMe command; adjacent extents are not merged.
- `nds_peer_device` uses PCI storage class to match NVMe controllers; a
  production driver should maintain a distance matrix.
- `nds_mgroup` uses a mutex-protected list instead of an RCU hash table for
  readability.
- Async batch submit + drain is implemented, but there is no eventfd-style
  async completion notification.
