# NPU Direct Storage (NDS) 设计文档

> 基于 `docs/zh/nds/VFS/` 与 `docs/zh/nds/NVMe/` 两套源码
> 对标: NVIDIA GPUDirect Storage (GDS) `nvidia-fs.ko` v2.29.4 (见附录 C)
> 内核模块: `npu_nds.ko` (`/dev/npu-nds`)
> 用户态: 客户端库 `libnpu_file.so` + 演示程序 `npu_nds_demo`

---

## 0. 文档说明

NDS 有两条零拷贝实现路径, 共用同一套 HAL P2P pin / mgroup 生命周期 / ioctl 框架,
仅在"如何把 NPU HBM 物理页送进 NVMe 控制器的 DMA 路径"上分叉:

| 路径 | 子目录 | 零拷贝机制 | 是否需要修改 NVMe 驱动 |
|---|---|---|---|
| **A: VFS 影子缓冲 + 厂商挂钩** | `docs/zh/nds/VFS/` | 4K 影子页 + `npufs_dma_rw_ops` 6-hook 拦截 DMA 映射 | **是**(借用 NVIDIA nvidia-fs 的 NVMe 驱动补丁, 导出 `nvme_v1_register_nvfs_dma_ops`/`nvme_v2_register_nvfs_dma_ops`) |
| **B: NVMe passthrough + tracepoint probe** | `docs/zh/nds/NVMe/` | `FS_IOC_FIEMAP` + `nvme_alloc_request` + `nvme_setup_cmd` 探针重写 SGL 类型 | **否**(只用主线已存在的稳定接口) |

两套 sample 的客户端 API (`npu_file_*`) 与 demo 程序对外契约一致; 差异只在内核数据
面与 `npu_nds_io_args` 的字段填充方式。本文档将两者的共享基础与路径特化部分合并叙述,
便于横向对照。各子目录另有 `README.md` 描述编译运行细节。

---

## 1. 概述

### 1.1 解决的问题

传统 NPU 与存储的数据通路为:

```
NVMe SSD ──DMA──> CPU bounce/page cache ──memcpy──> NPU HBM
```

该通路存在三重开销: CPU 内存带宽占用、CPU 缓存污染、双份数据拷贝。
NPU Direct Storage (NDS) 的目标是让 NVMe 控制器直接通过 PCIe P2P 将数据 DMA 到 NPU HBM:

```
NVMe SSD ──DMA(P2P)──> NPU BAR/HBM ──> NPU 显存
```

### 1.2 npu_nds 的角色

`npu_nds.ko` 是一个 GPL 内核模块, 充当 **NDS 的内核侧使能器**。它本身不实现块设备
或文件系统语义, 而是:

1. 通过 HAL P2P API (`hal_kernel_p2p_get_pages` / `hal_kernel_p2p_put_pages`) 与
   NPU 内核驱动协作, 完成对 NPU HBM 显存的 **pin(锁定)** 与 **DMA 映射**;
2. 把 NPU HBM 物理页"送进"NVMe 控制器的 DMA 路径——两条路径分别用不同手段(详见 §4、§5);
3. 向用户态暴露 `/dev/npu-nds` 字符设备与一组 ioctl, 由客户端库 `libnpu_file.so`
   封装为 `npu_file_buf_register` / `npu_file_read` / `npu_file_write` 等 API。

### 1.3 设计目标与约束

| 目标/约束 | 路径 A (VFS) | 路径 B (NVMe) |
|---|---|---|
| 架构验证 | 复刻 nvidia-fs "影子缓冲 + P2P pin + 厂商挂钩" 四位一体 | 用 FIEMAP + NVMe passthrough + tracepoint probe 三位一体 |
| 零拷贝 P2P | 注册 vendor hooks, 拦截 DMA 映射, 影子页 PA→NPU BAR 地址 | 内核直接构造 NVMe 命令, SGL 指向 NPU HBM DMA 总线地址 |
| 优雅降级 | NVMe 驱动未打补丁时退回 bounce-buffer | `nvme_alloc_request` 或 tracepoint 缺失时 READ/WRITE 返回 -ENOSYS |
| P2P DMA 映射 | 内核内部 `dma_map_resource` 缓存, 不暴露 IOVA | 内核内部 `dma_map_resource` 缓存, 不暴露 IOVA |
| 安全回收 | 状态机 + kref + `pinned_ref_dropped` 标志 | 同左 |
| 库/demo 分层 | lib 纯 libc + ioctl; demo 承担 HAL 设备上下文/显存分配 | 同左(lib 额外含 `FS_IOC_FIEMAP` 封装) |
| Demo 级实现 | 互斥链表代替 RCU 哈希; 同步 BVEC IO; 不合并 bio | 互斥链表; 异步 `blk_execute_rq_nowait` + 批量 drain (30s) + `kref` 生命周期; 不合并 extent |

---

## 2. 两条零拷贝路径对比

### 2.1 路径对比表

| 维度 | 路径 A: VFS 影子缓冲 | 路径 B: NVMe passthrough |
|---|---|---|
| **零拷贝实现** | 影子页 + `npufs_dma_rw_ops` 6-hook 拦截 `blk_rq_map_sg`/`dma_map_sg_attrs` | `nvme_alloc_request` + `RQF_NVME_PT` + `nvme_setup_cmd` tracepoint 探针重写 SGL 类型 |
| **文件布局来源** | VFS O_DIRECT 路径隐式获取 | 用户态 `FS_IOC_FIEMAP` 显式查询 |
| **内核 IO 路径** | 走 VFS `file->f_op->read_iter/write_iter` (BVEC iov_iter) | 绕开 VFS, 直接 `blk_execute_rq_nowait` |
| **CPU 内存占用** | 每 mgroup 一批 4K `alloc_page` 影子页 | 无影子页 |
| **mmap 给用户态** | 需要(`vm_insert_page` 影子页) | 不需要 |
| **NVMe 驱动修改** | 需导出 `nvme_v1_register_nvfs_dma_ops`/`nvme_v2_register_nvfs_dma_ops` 符号 | 无需(主线已导出 `nvme_alloc_request` + `nvme_setup_cmd` tracepoint) |
| **降级行为** | bounce-buffer(数据落影子页, `drvMemcpy` 推入 HBM) | READ/WRITE 返回 -ENOSYS, MAP 仍可用 |
| **IO 模型** | 同步 BVEC + `read_iter/write_iter` | 异步 submit + drain: `blk_execute_rq_nowait` + `wait_for_completion_timeout` (30s) + `blk_abort_request` (1s grace) + `kref` 批量生命周期 |
| **NPU 页面大小处理** | 影子页固定 4K, NPU 可能 2MB huge page, 需 `npu_page_size` 换算 | SGL 描述符按 FIEMAP extent 长度, 不涉及页面大小换算 |
| **nsid** | 由 VFS/块层隐式决定 | 硬编码为 1(demo 简化) |
| **extent 合并** | 块层 bio 合并 | 不合并, 每个 FIEMAP extent 一个 NVMe 命令 |

### 2.2 路径选择决策

- **选路径 A 的场景**: 已有打过 `npufs_dma_ops` 补丁的 NVMe 驱动; 需要复刻 nvidia-fs
  的精确语义; 需要在 VFS O_DIRECT 路径上拦截(如对 ext4/xfs 等文件系统有透明兼容需求)。
- **选路径 B 的场景**: 不能修改主线 NVMe 驱动; 追求真正零 CPU 内存占用; 接受用户态
  显式查询 FIEMAP; 用 `nvme_alloc_request` + tracepoint 这些主线稳定接口即可。

> 路径 B 是**推荐路径**: 无需修改 NVMe 驱动源码, 主线接口稳定, 无影子页副作用。
> 路径 A 作为对照保留, 便于与 nvidia-fs 源码逐行核对。

---

## 3. 共享设计基础

### 3.1 HAL P2P pin 接口(对应 NVIDIA P2P API)

nvidia-fs 依赖 NVIDIA 私有的 P2P API (`nvidia_p2p_get_pages` /
`nvidia_p2p_dma_map_pages`), 经 `nvidia.ko` 导出。npu_nds 对应使用 HAL 内核态接口:

| NVIDIA P2P API | HAL 内核态接口 | 说明 |
|---|---|---|
| `nvidia_p2p_get_pages_persistent` | `hal_kernel_p2p_get_pages` | 锁定 NPU 虚拟地址范围对应的 HBM 物理页 |
| `nvidia_p2p_put_pages_persistent` | `hal_kernel_p2p_put_pages` | 配对释放 |
| `nvidia_p2p_page_table_t` | `struct p2p_page_table` | 物理页描述, `pages_info[].pa` 为总线地址来源 |
| `nvidia_p2p_dma_map_pages` | `dma_map_resource` (Linux DMA API) | 针对某 peer PCI 设备建立 P2P DMA 映射 |
| `nvidia_p2p_free_callback` | `nds_p2p_free_callback` (HAL 回调) | NPU 显存即将释放时通知驱动 |

**HAL 契约关键约束**: `hal_kernel_p2p_put_pages` **不能**在 `free_callback` 上下文中调用
(死锁风险); 必须在回调返回后、从其他上下文(如 UNMAP ioctl)调用。这与 NVIDIA P2P API
的回调约束一致, 驱动用状态机 + `pinned_ref_dropped` 标志处理。

签名(`ascend_kernel_hal.h`):
```c
int hal_kernel_p2p_get_pages(u64 va, u64 len,
                             void (*free_callback)(void *data),
                             void *data,
                             struct p2p_page_table **page_table);
int hal_kernel_p2p_put_pages(struct p2p_page_table *page_table);
```

### 3.2 mgroup 数据结构与生命周期

两条路径共用 `struct nds_mgroup` 作为 pin 状态 + DMA 缓存 + 状态机载体:

```
struct nds_mgroup {
    list node               (nds_mgroup_list 互斥链表)
    kref                    (对象引用 + pin 引用)
    state                   (FREE / ACTIVE / TERMINATED)
    pinned_ref_dropped      (free_callback 是否已 drop pin ref)
    p2p_page_table *pt       (HAL 返回的 NPU 物理页描述)
    peer_device *dma_dev    (P2P 映射对端, 缓存)
    sg_table sgt            (DMA 映射缓存, per-mgroup 单一)
    dma_mapped              (缓存有效标志)
    lock                    (保护状态/缓存)
    id                      (atomic64_inc_return 键, >= 1<<32)
}
```

路径 A 额外字段: `shadow_pages[]` + `page_table` 编码;
路径 B 不需要影子页, mgroup 直接持有 `p2p_page_table`。

### 3.3 状态机与安全回收

NPU HBM 显存可在任意时刻被释放(如进程退出或显存回收), 此时可能仍有在途 IO。
驱动用状态机 + 引用计数协调:

```
mgroup 状态:
  NDS_MG_FREE ──MAP──> NDS_MG_ACTIVE ──UNMAP/free_callback──> NDS_MG_TERMINATED ──ref=0──> kfree
```

引用计数设计(kref):
- `kref_init` 时 ref=1(对象存在引用);
- `kref_get` 在 MAP 时再 +1(pinning lifetime 引用), 保证 free_callback 触发时 mgroup 不被释放;
- `nds_p2p_free_callback` 设置 `pinned_ref_dropped = true` 并 `kref_put` 一次(安全, 不会 double-drop);
- `NDS_IOCTL_UNMAP` 从链表删除后, 若 `pinned_ref_dropped == false` 则 put 两次(pin ref + object ref),
  否则只 put 一次(仅 object ref);
- ref 归零时 `nds_mgroup_release` 执行真正的资源回收: 注销 DMA 映射、(路径 A) 释放影子页、
  调用 `hal_kernel_p2p_put_pages`、kfree。

三条回收路径交汇于状态机:
1. **HAL free_callback**(`nds_p2p_free_callback`): NPU 驱动通知显存即将释放;
2. **UNMAP ioctl**(用户 `npu_file_buf_deregister`): 正常注销;
3. **module exit**(`nds_exit`): 模块卸载时强制清理残留 mgroup。

### 3.4 客户端库与 demo 分层

```
┌─────────────────────────────────────────────────────┐
│  demo (npu_nds_demo)                                │
│  ├─ HAL 用户态 API:                                 │
│  │   TsdOpen / drvMemDeviceOpen  (设备上下文)       │
│  │   halMemAlloc / halMemFree     (HBM 显存分配)     │
│  │   drvMemcpy                    (D2H 校验搬运)    │
│  └─ NDS 客户端库 API:                                │
│      npu_file_open / buf_register / read / write ... │
├─────────────────────────────────────────────────────┤
│  lib (libnpu_file.so)                               │
│  仅依赖 libc + Linux UAPI + NDS kernel ioctl 契约  │
│  ├─ open/close /dev/npu-nds                         │
│  ├─ ioctl(MAP/READ/WRITE/UNMAP)                    │
│  └─ 路径 B 额外: npu_file_fiemap_query (FS_IOC_FIEMAP)│
├─────────────────────────────────────────────────────┤
│  kernel (npu_nds.ko)                                │
│  ├─ HAL 内核态: hal_kernel_p2p_get_pages/put_pages  │
│  ├─ 路径 A: 影子页 + O_DIRECT BVEC + 6 NVMe hooks   │
│  ├─ 路径 B: nvme_alloc_request + tracepoint probe   │
│  ├─ DMA 映射缓存: dma_map_resource + sglist         │
│  └─ 状态机 + kref + free_callback                   │
└─────────────────────────────────────────────────────┘
```

客户端库刻意不依赖 HAL/TSD 等 API, 使其可独立复用——任何需要 `/dev/npu-nds` ioctl
封装的场景都能链接 `libnpu_file.so` 而不引入 HAL 的传递依赖。

---

## 4. 路径 A: VFS 影子缓冲 + 厂商挂钩

> 源码位于 `docs/zh/nds/VFS/`。本节描述其路径特化部分; 共享基础见 §3。

### 4.1 影子缓冲区间接层

**核心矛盾**: 块层与文件系统 O_DIRECT 路径假设 bio 携带的 `struct page` 是 CPU 可访问的
常规内存页; 而 NPU HBM 显存通过 BAR 暴露, 并非 `struct page` 体系内可寻址的对象。

**解决思路**: 驱动构造一个"影子缓冲区"作为中介:

- 当用户态 `npu_file_buf_register` 调用 `NDS_IOCTL_MAP` 时, 驱动调用
  `hal_kernel_p2p_get_pages` 把 NPU HBM 页锁定, 得到 `struct p2p_page_table`
  (含 `pages_info[].pa` 物理地址数组);
- 驱动用 `alloc_page(GFP_USER | __GFP_ZERO)` 分配一批 **4K 影子页**
  (每 NPU 页对应 `npu_page_size / 4096` 个影子页);
- 每个影子页的 `page->index` 被编码为 `(mgroup_id << NDS_MAX_SHADOW_PAGES_ORDER) + i`,
  其中 `mgroup_id` 由 `atomic64_inc_return(&nds_base_index)` 生成(初值 `NDS_MIN_BASE_INDEX = 1 << 32`),
  确保 base_index `>= NDS_MIN_BASE_INDEX`, 与普通页缓存 index 空间不重叠;
- 关键标记: 影子页的 `page->mapping == NULL`, 驱动据此在块层快速路径中识别
  "这是 NPU 影子页"而非普通页缓存页——与 nvidia-fs 完全一致;
- 用户态对 `/dev/npu-nds` 做 `mmap`, 通过 `vm_insert_page` 把影子页映射进用户地址空间。

```
用户态 shadow VA ─mmap─> 影子页(struct page, alloc_page)
                              │ page->index = (mgid << 12) + off
                              │ page->mapping = NULL  ◄── NPU 页识别标记
                              ▼
                      nds_mgroup_from_page() ─mutex─> mgroup
                              │ base_index = page->index >> 12
                              │ (== mgroup_id, >= NDS_MIN_BASE_INDEX)
                              ▼
               mgroup->page_table (p2p_page_table)
                              │ pages_info[].pa = NPU 物理地址
                              ▼
               dma_map_resource() ──> NPU BAR DMA 地址 (IOVA)
```

### 4.2 NVMe Vendor Hooks 注册机制

> **符号来源**: `nvme_v1_register_nvfs_dma_ops` / `nvme_v2_register_nvfs_dma_ops`
> 借用自 NVIDIA nvidia-fs 的 NVMe 驱动补丁。该补丁在主线 NVMe 驱动
> (`drivers/nvme/host/pci.c`) 中新增 `EXPORT_SYMBOL_GPL` 导出注册/注销符号,
> 供外部模块注册 DMA ops 钩子; v1/v2 对应不同内核版本接口差异, npu_nds
> 沿用此补丁以复用成熟的 hook 注册框架。

```
npu_nds.ko 模块加载 (nds_init)
  │
  ├─ __symbol_get("nvme_v1_register_nvfs_dma_ops")   (失败则尝试 v2)
  │    └─ 或 __symbol_get("nvme_v2_register_nvfs_dma_ops")
  │    └─ 成功: NVMe 驱动已打补丁, 导出了注册符号
  ├─ __symbol_get("nvme_v1_unregister_nvfs_dma_ops") (失败则尝试 v2)
  │    └─ 或 __symbol_get("nvme_v2_unregister_nvfs_dma_ops")
  │    └─ 成功: 获取注销符号
  ├─ reg_fn(&npu_dma_rw_ops)    ← 注册 ops 表
  │    └─ 失败: 记录错误, 继续无 hook 模式 (bounce-buffer 降级)
  └─ 成功: nds_hooks_registered = true
```

```
npu_nds.ko 模块卸载 (nds_exit)
  │
  ├─ unreg_fn()                  ← 注销 ops 表
  └─ __symbol_put × 2            ← 释放符号引用
```

`__symbol_get` / `__symbol_put` 是内核 GPL 导出的符号查找 API, 不会在模块卸载时
残留引用。若 NVMe 驱动未打补丁(符号不存在), `__symbol_get` 返回 NULL, 模块继续加载
但进入 bounce-buffer 模式(无零拷贝)。

### 4.3 ops 表结构

`struct npufs_dma_rw_ops` 布局**必须**与 nvidia-fs 一致(npufs-dma.h:74), 因为
NVMe 驱动将注册的指针 cast 为该类型:

| 字段 | 类型 | NPU 实现 | 作用 |
|---|---|---|---|
| `ft_bmap` | u64 | `NPU_NDS_FT_ALL` | 特性位图: 声明支持哪些 hook |
| `npufs_blk_rq_map_sg` | fn | `npu_blk_rq_map_sg` | 从 request 构建 sglist, 用 NPU PA |
| `npufs_dma_map_sg_attrs` | fn | `npu_dma_map_sg_attrs` | DMA 映射 sglist, 设 NPU BAR 地址 |
| `npufs_dma_unmap_sg` | fn | `npu_dma_unmap_sg` | no-op(缓存映射不在此释放) |
| `npufs_is_gpu_page` | fn | `npu_is_gpu_page` | 判断页面是否为 NPU 影子页 |
| `npufs_gpu_index` | fn | `npu_gpu_index` | 返回 0(demo: 单 NPU) |
| `npufs_device_priority` | fn | `npu_device_priority` | 返回 0(demo: 等同优先级) |

### 4.4 零拷贝数据路径(读为例)

```
file->f_op->read_iter(DIO, BVEC over shadow pages)
  │  块层构建 bio, 影子页 page->mapping == NULL
  ▼
NVMe 驱动: npu_blk_rq_map_sg(q, req, sglist)          ← hook 1
  │  rq_for_each_segment 遍历 bio segments
  │  对每个 bvec.bv_page:
  │    nds_mgroup_from_page(page)  →  mgroup (ref +1)
  │    rel_index = page->index % NDS_MAX_SHADOW_PAGES
  │    curr_pa = nds_get_npu_pa(mg, rel_index)  → NPU HBM PA
  │    若与前一段 NPU PA 连续: 合并 sg->length
  │    否则: sg_set_page(sg, page, len, off), nsegs++
  │    nds_mgroup_put(mg)  (ref -1)
  │  sg_mark_end(sg); return nsegs (>0 表示含 NPU 页)
  ▼
NVMe 驱动: npu_dma_map_sg_attrs(dev, sglist, nents, dir, attrs)  ← hook 2
  │  for_each_sg:
  │    nds_mgroup_from_page(sg_page(sg))  →  mgroup
  │    nds_ensure_dma_mapped(mg, dev)
  │      └─ dma_map_resource(dev, npu_pa, npu_page_size, BIDIR, 0)
  │         缓存到 mg->sgt, mg->dma_dev, mg->dma_mapped = true
  │         get_device(peer)  ← 持有 peer 引用(release 时 put_device)
  │    nds_get_dma_addr(mg, rel_index)  → BAR 总线地址
  │    sg_dma_address(sg) = dma_addr + sg->offset
  │    sg_dma_len(sg) = sg->length
  │    nds_mgroup_put(mg)
  │  return nr_npu
  ▼
NVMe 控制器: DMA 直达 NPU HBM  ← 零拷贝! 不经 CPU 暂存
```

### 4.5 NPU 页面大小处理

NPU HBM 可能使用 2MB huge page(`MEM_PAGE_HUGE`), 而影子页固定为 4K。`nds_get_npu_pa`
和 `nds_get_dma_addr` 均处理 `npu_page_size > PAGE_SIZE` 的情况:

```
offset = rel_index * PAGE_SIZE            (4K 影子页偏移)
npu_page_idx = offset / npu_page_size     (落在第几个 NPU 页)
page_offset  = offset % npu_page_size     (页内偏移)

NPU PA  = page_table->pages_info[npu_page_idx].pa + page_offset
DMA addr = sg_dma_address(sgt.sgl[npu_page_idx]) + page_offset
```

### 4.6 引用计数

| 操作 | 引用变化 | 说明 |
|---|---|---|
| `npu_blk_rq_map_sg` | mgroup get→put (每段) | 获取 NPU PA 后立即释放, 不持有 |
| `npu_dma_map_sg_attrs` | mgroup get→put (每 sg) | 确保 DMA 映射后立即释放 |
| `nds_ensure_dma_mapped` (首次) | `get_device(peer)` | 缓存映射持有 peer 引用 |
| `nds_mgroup_release` | `put_device(dma_dev)` | 释放缓存映射的 peer 引用 |

### 4.7 源码级对应点验证(对照 nvidia-fs)

下表列出经 nvidia-fs v2.29.4 源码逐行核验的 **精确对应点**:
相同常量、相同 API、相同 VMA flag, 证明路径 A 的 mmap/影子页路径是对 nvidia-fs
`npufs-mmap.c` 的忠实移植:

| 对应点 | nvidia-fs 源码 | npu_nds 实现 | 验证结论 |
|---|---|---|---|
| 影子页 GFP | `alloc_page(GFP_USER\|__GFP_ZERO)` | 同 | **完全一致** |
| 页插入 API | `vm_insert_page(vma, addr, page)` | 同 | **完全一致** |
| VMA flag | `VM_MIXEDMAP\|VM_DONTEXPAND\|VM_DONTDUMP\|VM_DONTCOPY` | 同 | **完全一致** |
| 影子页标记 | `page->mapping == NULL` | 同 | **完全一致** |
| `page->index` 编码 | `base_index + off` | `(mgroup_id << 12) + off` | **机制一致** |
| mgroup 查找 | RCU 哈希表 | 互斥链表 | **有意简化** |
| 故障注入 | `npufs-fault.c` 验证 `vm_insert_page` | 无 | **反向印证** |
| 影子页释放 | `put_page` | `put_page` | **完全一致** |
| VMA 关闭回调 | vma_ops `.close` | `nds_vma_close` | **机制一致** |

> 路径 A 的 `nds_mmap` / 影子页 / VMA ops 是 nvidia-fs
> `npufs_mgroup_mmap_internal` 的忠实移植。差异仅在:
> (1) 查找结构 RCU→mutex; (2) 无故障注入/统计/procfs;
> (3) P2P 页来源由 NVIDIA P2P API 换为 HAL `hal_kernel_p2p_get_pages`。

### 4.8 数据通路全景(路径 A)

```
┌─────────────────────────────────────────────────────────────────────┐
│ 用户态:  npu_file_read/Write ──> libnpu_file ──ioctl──> /dev/npu-nds │
│           │                                                          │
│           └─ mmap 影子缓冲 (vm_insert_page)                         │
└─────────────────────────────────────────────────────────────────────┘
                                     │ NDS_IOCTL_READ/WRITE
                                     ▼
┌────────────────────────── 内核: npu_nds.ko ──────────────────────────┐
│  nds_ioctl → nds_do_io                                              │
│   │  mgroup_get(mgroup_id), 状态校验 ACTIVE                          │
│   ▼                                                                  │
│  nds_shadow_io_iter:                                                 │
│   │  构建 BVEC iov_iter (影子页直接作为 bio_vec)                     │
│   │  调用 file->f_op->read_iter/write_iter (DIO)                     │
│   ▼                                                                  │
│  块层构建 bio (影子页, page->mapping == NULL)                        │
│  ──(零拷贝路径, vendor hooks 已注册)──                                │
│  NVMe 驱动调用 npu_blk_rq_map_sg    ← hook 1                         │
│   │  识别影子页, nds_get_npu_pa() 取 NPU HBM 物理地址                  │
│   │  合并连续 NPU PA, 生成 sglist                                    │
│   ▼                                                                  │
│  NVMe 驱动调用 npu_dma_map_sg_attrs ← hook 2                         │
│   │  nds_ensure_dma_mapped() 缓存 P2P DMA 映射                      │
│   │  nds_get_dma_addr() 取 NPU BAR 总线地址                          │
│   ▼                                                                  │
│  NVMe 控制器 DMA 直达 NPU HBM ← 零拷贝!                              │
│                                                                      │
│  ──(bounce 路径, NVMe 未打补丁时降级)──                              │
│  数据落在影子页(CPU 内存), 用户态 drvMemcpy 推入 NPU HBM              │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 5. 路径 B: NVMe passthrough + tracepoint probe

> 源码位于 `docs/zh/nds/NVMe/`。本节描述其路径特化部分; 共享基础见 §3。

### 5.1 取消影子缓冲与厂商挂钩的理由

旧版 npu_nds (即路径 A) 复刻 nvidia-fs 的 "影子页 + `npufs_dma_rw_ops` 厂商挂钩" 路径,
但有两个根本性问题:

1. **强耦合 NVMe 驱动**: 需要给 NVMe 驱动打补丁, 导出
   `nvme_v1_register_nvfs_dma_ops`/`nvme_v2_register_nvfs_dma_ops` 符号, 与上游 NVMe 驱动不兼容;
2. **影子页副作用**: 4K `alloc_page` 影子页占用 CPU 内存, `vm_insert_page` 路径
   复杂且 ARM64 上对 `kmap` 有限制; 同时需要 mmap 给用户态, 增加一层间接。

新设计直接绕开 VFS 与块层 O_DIRECT 路径, 用 NVMe passthrough 把 NPU HBM 作为
SGL 源, 实现真正的零拷贝且无需修改 NVMe 驱动 — `nvme_alloc_request` 与
`nvme_setup_cmd` tracepoint 都是主线已存在的稳定接口。

### 5.2 NVMe passthrough 提交路径

```
nds_do_io
  │  mgroup_get(mgroup_id), 状态校验 ACTIVE
  │  copy_from_user(extents[], extents_ptr, extent_cnt * sizeof(extent))
  │  batch = kzalloc; kref_init(ref=1, owner ref)
  ▼
Phase 1: for each extent in extents[]:
  │  atomic_inc(&batch->pending); kref_get(&batch->ref)  ← 先记账
  │  nds_pt_submit_one(mg, queue, ext, opcode, ctx)
  │   │  构造 struct nvme_command cmd:
  │   │    cmd->common.opcode = (READ? nvme_cmd_read : nvme_cmd_write)
  │   │    cmd->common.nsid  = 1 (demo 硬编码)
  │   │    cmd->common.slba = ext.physical / 512
  │   │    cmd->common.length = (ext.length / 512 - 1)  (零基)
  │   │    cmd->common.dptr.sgl.addr = nds_get_dma_addr(mg, ext.npu_offset)
  │   │    cmd->common.dptr.sgl.length = ext.length
  │   │    cmd->common.dptr.sgl.type = NVME_CMD_SGL_DATA_BLOCK
  │   │  req = nvme_alloc_request(queue, &cmd, 0)
  │   │  req->rq_flags |= RQF_NVME_PT
  │   │  req->end_io_data = ctx; ctx->req = req
  │   │  blk_execute_rq_nowait(req, false, nds_pt_end_io)
  │   │  ✗ 失败: atomic_dec + kref_put (撤销记账)
  ▼
Phase 2: wait_for_completion_timeout(&batch->cmp, 30s)
  │  ✗ 超时 → Phase 3
  ▼
Phase 3: for each !completed ctx: blk_abort_request(ctx->req)
  │  Phase 3b: wait_for_completion_timeout(&batch->cmp, 1s grace)
  │  ✗ grace 仍超时且 pending > 0 → Phase 4
  ▼
Phase 4: batch->abandoned = true; kref_put(owner ref)
  │  若 pending=0: 本次 kref_put 释放 batch
  │  若 pending>0: 最后一个 end_io 的 kref_put 释放 batch
  │  batch = NULL; ret = -ETIMEDOUT; goto out_peer
  ▼
Phase 5: sum bytes from ctxs[] (completed && status==OK)
  │
  args->done = 累计字节数; args->ret = 错误码
  out_batch: kref_put(owner ref) → ref=0 → nds_pt_batch_release → kfree
```

关键点:
- `nvme_alloc_request` 通过 `__symbol_get` 软查找, 失败则 READ/WRITE 返回 -ENOSYS;
- `blk_abort_request` 同样通过 `__symbol_get` 查找, 可选 — 缺失时仅记日志, 依赖块层自身超时;
- `nsid` 暂硬编码为 1(demo);
- `length` 字段为 `(NLB - 1)` 零基编码, 按 512 字节逻辑块计算;
- 每个 FIEMAP extent 对应一个独立的 NVMe 命令(简化实现, 不合并相邻 extent);
- **异步批量提交**: Phase 1 一次性提交全部 extent, 不逐条等待;
- **kref 批量生命周期**: owner ref (nds_do_io) + 1 ref per request; end_io 和 nds_do_io 各 kref_put 一次, 最后一次 put 释放 batch;
- **先记账后提交**: `atomic_inc` + `kref_get` 在 `blk_execute_rq_nowait` 之前, 避免 end_io 先于记账返回的竞态;
- **abandoned 标志纯咨询**: 只控制 end_io 是否调用 `complete()`; 竞态读无害 — 无人等待的 complete() 是空操作;
- 完成 IRQ 触发 `nds_pt_end_io`: `atomic_dec_and_test` → 若 pending=0 且 !abandoned → `complete()`; 然后 `kref_put`。

### 5.3 tracepoint 探针: SGL 类型重写

NVMe 控制器对 P2P 友好的 SGL 类型是 `NVME_CMD_SGL_METABUF`, 但构造命令时
该字段写什么由 NVMe 驱动决定, 上游 NVMe 驱动在 `nvme_setup_cmd` tracepoint 触发后
可能覆写。为此路径 B 在 `nvme_setup_cmd` tracepoint 上挂探针:

```c
static void nds_nvme_setup_cmd_probe(void *data, struct request *req,
                                       struct nvme_command *cmd)
{
    if (req->rq_flags & RQF_NVME_PT)
        cmd->common.dptr.sgl.type = NVME_CMD_SGL_METABUF;
}
```

注册:
```c
for_each_kernel_tracepoint("nvme_setup_cmd", nds_register_tracepoint, &tp);
if (tp) {
    ret = tracepoint_probe_register(tp, nds_nvme_setup_cmd_probe, NULL);
    nds_tp = tp;
}
```

注销(模块卸载时):
```c
if (nds_tp)
    tracepoint_probe_unregister(nds_tp, nds_nvme_setup_cmd_probe, NULL);
```

这样在不修改 NVMe 驱动源码的前提下, 任何 `RQF_NVME_PT` 请求的 SGL 类型都会被强制
重写为 `NVME_CMD_SGL_METABUF`, 适配 P2P。

### 5.4 数据通路全景(路径 B)

```
┌─────────────────────────────────────────────────────────────────────┐
│ 用户态:  npu_file_read/Write ──> libnpu_file ──ioctl──> /dev/npu-nds │
│           │                                                            │
│           └─ npu_file_fiemap_query(FS_IOC_FIEMAP on file_fd)         │
│              得到 extents[] (logical/physical/length)                  │
└─────────────────────────────────────────────────────────────────────┘
                                     │ NDS_IOCTL_READ/WRITE
                                     ▼
┌────────────────────────── 内核: npu_nds.ko ──────────────────────────┐
│  nds_ioctl → nds_do_io                                              │
│   │  mgroup_get(mgroup_id), 状态校验 ACTIVE                          │
│   │  copy_from_user(extents[], extents_ptr)                         │
│   ▼                                                                  │
│  batch = kzalloc + kref_init(ref=1)                                 │
│  for each extent:                                                   │
│   │  atomic_inc(pending) + kref_get(ref)  ← 先记账                  │
│   │  nds_pt_submit_one: 构造 NVMe 命令:                              │
│   │    slba = ext.physical / 512                                     │
│   │    length = ext.length / 512 - 1                                 │
│   │    dptr.sgl.addr = nds_get_dma_addr(mg, ext.npu_offset)        │
│   │                     (NPU BAR 总线地址, 经 dma_map_resource)      │
│   │    dptr.sgl.type  = NVME_CMD_SGL_DATA_BLOCK                      │
│   │  req = nvme_alloc_request(q, &cmd, 0)                           │
│   │  req->rq_flags |= RQF_NVME_PT                                   │
│   │  blk_execute_rq_nowait(req, nds_pt_end_io)──▶│ NVMe 提交        │
│   ▼                                                                  │
│  wait_for_completion_timeout(batch.cmp, 30s)                        │
│   │  ✗超时 → blk_abort_request ×N → grace 1s → abandon+kref_put    │
│   ▼                                                                  │
│  NVMe 驱动 nvme_setup_cmd → tracepoint probe 重写 SGL = METABUF     │
│  NVMe 控制器 DMA 直达 NPU HBM ← 零拷贝!                              │
│  nds_pt_end_io: atomic_dec(pending) → complete? + kref_put          │
│  out_batch: kref_put(owner ref) → ref=0 → kfree(batch)              │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 6. ioctl 契约

### 6.1 共享 ioctl

| ioctl | 方向 | 作用 | 路径 A 行为 | 路径 B 行为 |
|---|---|---|---|---|
| `NDS_IOCTL_MAP` | 用户→内核 | pin NPU HBM, 建立 mgroup | 额外分配影子页 + 返回 mmap offset | 不分配影子页, mmap offset/size 留 0(ABI 兼容) |
| `NDS_IOCTL_UNMAP` | — | unpin, 释放 mgroup | 额外释放影子页 | 直接 put_pages + kfree |
| `NDS_IOCTL_READ` | 双向 | 文件→NPU HBM | `npu_nds_io_args` 用 fd/offset/size, 内核走 BVEC + VFS DIO | `npu_nds_io_args` 额外用 extent_cnt/extents_ptr, 内核走 NVMe PT |
| `NDS_IOCTL_WRITE` | 双向 | NPU HBM→文件 | 同 READ 反向 | 同 READ 反向 |

### 6.2 路径 A 的 READ/WRITE 参数

`npu_nds_io_args` (路径 A):
- `fd`: 文件描述符(内核经 `fget` 取 `struct file`, 调其 `read_iter/write_iter`);
- `mgroup_id`: NPU HBM 句柄;
- `file_offset` / `size`: 范围描述;
- `done` / `ret`: 内核回填完成字节数与返回码。

用户态通过 `mmap` 影子缓冲获得 `shadow_va`, 但 IO 时内核直接用 BVEC 影子页, 不引用
`shadow_va`。

### 6.3 路径 B 的 READ/WRITE 参数

`npu_nds_io_args` (路径 B):
- `fd`: 文件描述符(内核用 `file_inode(fd_file(fd))->i_sb->s_bdev` 找回 NVMe 块设备);
- `mgroup_id`: NPU HBM 句柄;
- `file_offset` / `size` / `npu_offset`: 范围描述;
- `extent_cnt` / `extents_ptr`: 指向 `npu_nds_fiemap_extent[]` 数组(用户态填充);
- `done` / `ret`: 内核回填完成字节数与返回码。

每个 `npu_nds_fiemap_extent` 含 `logical` / `physical` / `length` / `npu_offset`,
内核据此计算 SLBA = `physical / 512`, NLB = `length / 512 - 1`, NPU BAR 地址由
`npu_offset` 经 `nds_get_dma_addr(mg, npu_offset)` 取得。

### 6.4 ioctl 速查

| ioctl | magic | nr | 结构 | 方向 |
|---|---|---|---|---|
| `NDS_IOCTL_MAP` | `'N'` | 1 | `npu_nds_map_args` | `_IOWR` |
| `NDS_IOCTL_READ` | `'N'` | 2 | `npu_nds_io_args` (路径 B 含 FIEMAP extents) | `_IOWR` |
| `NDS_IOCTL_WRITE` | `'N'` | 3 | `npu_nds_io_args` (路径 B 含 FIEMAP extents) | `_IOWR` |
| `NDS_IOCTL_UNMAP` | `'N'` | 4 | `npu_nds_unmap_args` | `_IOW` |

---

## 7. 与 GPU 对应的设计点(Design Mapping)

### 7.1 架构映射表

| nvidia-fs 设计要点 | 路径 A (VFS) | 路径 B (NVMe) | 说明 |
|---|---|---|---|
| `nvidia_p2p_get_pages_persistent` pin GPU 显存 | `hal_kernel_p2p_get_pages` | 同 | HAL 内核态接口, 导出为 `KA_EXPORT_SYMBOL_GPL` |
| `nvidia_p2p_put_pages` | `hal_kernel_p2p_put_pages` | 同 | 配对释放, 不能在 free_callback 上下文调用 |
| `nvidia_p2p_page_table_t` | `struct p2p_page_table` | 同 | 含 `pages_info[].pa` 物理地址数组 |
| `nvidia_p2p_dma_map_pages` | `dma_map_resource` | 同 | 对 NPU 物理页做 peer-specific 映射 |
| `nvidia_p2p_free_callback` | `nds_p2p_free_callback` | 同 | NPU 驱动通知显存即将释放 |
| 4K 影子页 `alloc_page`, `page->mapping=NULL` | 同样 | **不再需要** | 路径 B 无影子页 |
| `page->index = base_index + off` | `page->index = mgroup_id + off` | 不适用 | 路径 A mgroup 全局唯一键编入 page->index |
| `npufs_io_mgroup` RCU 哈希表 | `struct nds_mgroup` 互斥链表 | 同(无影子页) | demo 简化 |
| `NPUFS_IOCTL_MAP/READ/WRITE/REMOVE` | `NDS_IOCTL_*` | 同(READ/WRITE 扩展 FIEMAP) | ioctl 契约一一对应 |
| 厂商驱动挂钩 | **已实现**: `__symbol_get` + `npufs_dma_rw_ops` | **替代方案**: `nvme_alloc_request` + `tracepoint_probe_register` | 路径 B 无需修改 NVMe 驱动 |
| `NPUFS_IOCTL_BATCH_IO` (≤256) | 未实现 | 未实现 | demo 仅同步 |
| `NPUFS_IOCTL_SET/GET/CLEAR_RDMA_REG_INFO` | 未实现 | 未实现 | RDMA 不在 demo 范围 |
| `configure` + `config-host.h` | 不需要 | 不需要 | 单一内核目标 |
| `npufs-pci.c` 距离矩阵 | `nds_peer_device` best-effort | 同 | demo 用 PCI class 粗匹配 |
| `npufs-stat.c` procfs | 未实现 | 未实现 | demo 仅 `pr_info` |
| `npufs-fault.c` debugfs | 未实现 | 未实现 | — |
| `libcufile.so` (闭源) | `libnpu_file.so` (开源) | 同(+FIEMAP) | 仅依赖 libc + UAPI + ioctl |
| cuFile API | npu_file API | 同(READ/WRITE 多 npu_offset) | open/register/read/write/deregister/close |

### 7.2 API 映射表

| cuFile API | npu_file API | 路径 A | 路径 B | 说明 |
|---|---|---|---|---|
| `cuFileDriverOpen` | `npu_file_open` | ✓ | ✓ | 打开 `/dev/npu-nds` |
| `cuFileBufRegister` | `npu_file_buf_register` | MAP + mmap 影子缓冲 | ioctl MAP | pin NPU HBM, 建立 mgroup |
| `cuFileRead` | `npu_file_read` | READ: O_DIRECT → 影子缓冲 | READ: FIEMAP + RQF_NVME_PT | 文件→NPU HBM |
| `cuFileWrite` | `npu_file_write` | WRITE: 影子缓冲 → O_DIRECT | WRITE: FIEMAP + RQF_NVME_PT | NPU HBM→文件 |
| — | `npu_file_fiemap_query` | — | ✓ | 路径 B 独有: `FS_IOC_FIEMAP` 封装 |
| `cuFileBufDeregister` | `npu_file_buf_deregister` | munmap + UNMAP | ioctl UNMAP | 释放 mgroup + unpin |
| `cuFileDriverClose` | `npu_file_close` | ✓ | ✓ | 关闭 `/dev/npu-nds` |

> NPU 设备上下文管理(`drvMemDeviceOpen`/`TsdOpen`)、显存分配(`halMemAlloc`)、
> 数据搬运(`drvMemcpy`)由调用方(demo)自行完成, 不属于客户端库职责。

### 7.3 关键差异

| 维度 | nvidia-fs | 路径 A (VFS) | 路径 B (NVMe) | 差异原因 |
|---|---|---|---|---|
| 零拷贝实现 | 厂商驱动挂钩 | 同样注册 `npufs_dma_rw_ops` | NVMe PT + tracepoint probe | 路径 B 避免修改 NVMe 驱动 |
| 文件布局来源 | VFS DIO 隐式 | 同 | 用户态 `FS_IOC_FIEMAP` 显式 | 路径 B 内核不走 VFS |
| mgroup 查找 | RCU 哈希表 O(1) | 互斥链表 O(n) | 同 | demo 可读性优先 |
| IO 模型 | 同步+异步+批量 | 同步 BVEC + `read_iter` | 异步 submit + drain + `kref` 批量 + `blk_abort_request` | demo 验证架构 |
| 拓扑亲和 | BDF+距离矩阵+ACS | PCI class 粗匹配 | 同 | 生产驱动需补全 |
| DMA 缓存 | per-(mgroup,peer) RCU | per-mgroup 单一 | 同 | demo 只演示一个 peer |
| 可观测性 | procfs+stats+debugfs | `pr_info` | 同 | 生产驱动需补全 |
| RDMA 直达 | `NPUFS_IOCTL_SET_RDMA_REG_INFO` | 未实现 | 未实现 | — |
| 降级策略 | 无 | `__symbol_get` 失败→bounce | 失败→-ENOSYS | 优雅降级 |
| 回调约束 | 可在回调中注销 DMA | 不能在回调中 put_pages | 同 | HAL 契约差异 |
| extent 合并 | 块层 bio 合并 | 同 | 不合并 | 路径 B demo 简化 |
| nsid | — | VFS/块层隐式 | 硬编码为 1 | 路径 B demo 简化 |

---

## 8. 4+1 视图

### 8.1 逻辑视图(Logical View)

```
                        ┌──────────────────────────────────┐
                        │  npu_nds_demo (用户态)           │
                        │  ├─ HAL API: TsdOpen/drvMemDeviceOpen
                        │  │           halMemAlloc/drvMemcpy
                        │  └─ lib API: npu_file_*         │
                        └───────────────┬──────────────────┘
                                        │
                ┌───────────────────────┼───────────────────┐
                │                       │                   │
        ┌───────▼────────┐    ┌────────▼─────────┐         │
        │ libnpu_file│    │ HAL 用户态       │         │
        │ .so             │    │ libascend_hal.so │         │
        │ (libc + ioctl)  │    │ libc_sec.so      │         │
        │ (+FIEMAP, 路径B)│    │ -ldl             │         │
        └───────┬────────┘    └────────┬─────────┘         │
                │ ioctl                │ HAL syscall         │
                ▼                      ▼                     │
┌─────────────────────────── npu_nds.ko ──────────────────────┐
│                                                              │
│  ┌─────── 控制面 (npu_nds_kern.c) ──────────┐               │
│  │ ioctl 分发    nds_ioctl                    │               │
│  │ MAP           nds_do_map                   │               │
│  │ UNMAP         nds_do_unmap                 │               │
│  │ READ/WRITE    nds_do_io                    │               │
│  │   路径 A: → nds_shadow_io_iter            │               │
│  │   路径 B: → nds_pt_submit_one             │               │
│  │ mgroup 生命周期  nds_mgroup_get/put/release│               │
│  │ 路径 A: NVMe hook 注册  __symbol_get + npu_dma_rw_ops     │
│  │ 路径 B: 符号查找  __symbol_get("nvme_alloc_request")       │
│  │ 路径 B: tracepoint probe 注册 for_each_kernel_tracepoint │
│  └───────────────────┬────────────────────────┘               │
│                       │                                       │
│  ┌─────── 内存面 ──────┴──────────────────┐                   │
│  │ struct nds_mgroup:                      │                   │
│  │   [路径 A] shadow_pages[] + page_table │                   │
│  │   [路径 B] page_table (p2p_page_table)  │                   │
│  │   sgt + dma_dev + dma_mapped (P2P 缓存) │                   │
│  │   状态机 + kref + pinned_ref_dropped    │                   │
│  └───────────────────┬────────────────────┘                   │
│                       │                                       │
│  ┌─────── 数据面 ──────┴──────────────────┐                   │
│  │ 共享:                                  │                   │
│  │   nds_peer_device: PCI 对端枚举           │                   │
│  │   nds_get_npu_pa: NPU offset → HBM PA    │                   │
│  │   nds_ensure_dma_mapped: P2P 映射缓存    │                   │
│  │   nds_get_dma_addr: NPU offset → BAR 地址│                   │
│  │ 路径 A 专属:                            │                   │
│  │   nds_mgroup_from_page: 页面→mgroup       │                   │
│  │   nds_shadow_io_iter: BVEC iov_iter     │                   │
│  │   NVMe hooks: npu_dma_rw_ops (6 hooks)  │                   │
│  │ 路径 B 专属:                            │                   │
│  │   nds_file_to_queue: fd → NVMe 请求队列  │                   │
│  │   nds_pt_submit_one: 构造 NVMe 命令 + 提交│                   │
│  │   nds_pt_end_io: 完成 IRQ 回调          │                   │
│  │   nds_pt_batch_release: kref 释放 batch  │                   │
│  │   nds_nvme_setup_cmd_probe: SGL 重写     │                   │
│  └──────────────────────────────────────────┘                  │
│                                                                │
│  ┌─ 辅助 ──────────────────────────────────┐                  │
│  │ nds_p2p_free_callback  HAL 回调 → 状态机 │                  │
│  │ [路径 A] nds_mmap/nds_vma_close  mmap   │                  │
│  │ nds_init/exit          模块入口/清理      │                  │
│  └──────────────────────────────────────────┘                  │
└────────────────────────────────────────────────────────────────┘
        │ hal_kernel_p2p_get_pages/put_pages
        ▼
┌───────────────────┐
│ NPU 内核驱动       │
│ (HAL, 导出 P2P    │
│  符号)             │
└───────────────────┘
```

关键逻辑数据结构:

| 结构 | 文件 | 职责 |
|---|---|---|
| `struct nds_mgroup` | npu_nds_kern.c | [A] shadow_pages + page_table / [B] page_table + 状态机 + kref + DMA 缓存 |
| `struct nds_pt_ctx` | npu_nds_kern.c | [B] per-request: batch backref, status, nr_bytes, req back-pointer, completed |
| `struct nds_pt_batch` | npu_nds_kern.c | [B] async batch: kref ref, completion, atomic pending, ctxs[], abandoned flag |
| `struct p2p_page_table` | ascend_kernel_hal.h | NPU 物理页描述, `pages_info[].pa` |
| `struct npu_nds_map_args` | npu_nds.h | MAP: npu_va, size → mgroup_id, [A] mmap_offset |
| `struct npu_nds_io_args` | npu_nds.h | READ/WRITE: fd, offset, size → done, ret; [B] 额外 extent_cnt/extents_ptr |
| `struct npu_nds_fiemap_extent` | npu_nds.h | [B] FIEMAP extent: logical/physical/length/npu_offset |
| `npu_file_ctx_t` | npu_file.h | 用户态上下文: nds_fd |
| `npu_file_handle_t` | npu_file.h | [A] mgroup_id + shadow_va / [B] mgroup_id + buf_size |

### 8.2 进程视图(Process View)

#### 8.2.1 路径 A 同步读/写主流程

```
用户线程              内核(npu_nds.ko)           块层/文件系统         NPU 驱动/HAL
    │                     │                          │                     │
    │ TsdOpen             │                          │                     │
    │ drvMemDeviceOpen    │                          │                     │
    │ halMemAlloc(HBM)    │                          │                     │
    │ npu_file_open       │                          │                     │
    │──ioctl(MAP)──────▶│ nds_do_map               │                     │
    │                     │  hal_kernel_p2p_get_pages────────────────────▶│ pin NPU HBM
    │                     │  ←─ page_table ───────────────────────────────│ pages_info[].pa
    │                     │  alloc_page(影子页)       │                     │
    │                     │  page->mapping=NULL     │                     │
    │                     │  page->index=(mgid<<12)+i                    │
    │                     │  list_add(mgroup)        │                     │
    │←──mgroup_id+offset──│                          │                     │
    │ mmap(影子缓冲)     │                          │                     │
    │                     │  vm_insert_page          │                     │
    │ npu_file_write       │                          │                     │
    │──ioctl(WRITE)─────▶│ nds_do_io                │                     │
    │                     │  mgroup_get, 状态校验    │                     │
    │                     │  nds_shadow_io_iter:     │                     │
    │                     │  ──BVEC read_iter──────▶│ O_DIRECT (影子页入bio)│
    │                     │  ←─────────────────────│ NVMe hooks 拦截:    │
    │                     │                          │  npu_blk_rq_map_sg  │
    │                     │                          │  npu_dma_map_sg     │
    │                     │                          │  DMA→NPU HBM (零拷贝)│
    │←──done=ssize_t─────│                          │                     │
    │ (零拷贝: 无需 drvMemcpy, 数据直达 NPU HBM)                         │
    │ (bounce: drvMemcpy(shadow→NPU) 推入 HBM)                           │
    │ npu_file_read        │                          │                     │
    │──ioctl(READ)──────▶│ (同上, 反向)             │                     │
    │ verify(D2H)        │                          │                     │
    │ npu_file_buf_deregister                        │                     │
    │──ioctl(UNMAP)────▶│ nds_do_unmap             │                     │
    │                     │  hal_kernel_p2p_put_pages─────────────────────▶│ unpin
    │                     │  free shadow pages      │                     │
    │ npu_file_close      │                          │                     │
    │ halMemFree / drvMemDeviceClose / TsdClose    │                     │
```

#### 8.2.2 路径 B 同步读/写主流程

```
用户线程              内核(npu_nds.ko)           NVMe 驱动            NPU 驱动/HAL
    │ TsdOpen / drvMemDeviceOpen / halMemAlloc(HBM) │                     │
    │ open(O_DIRECT) / fallocate(size)             │                     │
    │ npu_file_open                                  │                     │
    │──ioctl(MAP)──────▶│ nds_do_map               │                     │
    │                     │  hal_kernel_p2p_get_pages────────────────────▶│ pin NPU HBM
    │                     │  ←─ page_table ───────────────────────────────│
    │                     │  list_add(mgroup)        │                     │
    │←──mgroup_id────────│                          │                     │
    │ npu_file_write (lib 内 FIEMAP)                │                     │
    │──ioctl(WRITE, ext)─▶│ nds_do_io                │                     │
    │                     │  mgroup_get, copy_from_user(extents)        │                     │
    │                     │  nds_file_to_queue(fd) → req_queue           │                     │
    │                     │  batch = kzalloc + kref_init(ref=1)        │                     │
    │                     │  for each ext:                             │                     │
    │                     │    atomic_inc(pending) + kref_get(ref)     │                     │
    │                     │    dma_addr = nds_get_dma_addr(mg, ext.npu_offset)
    │                     │    req = nvme_alloc_request(queue, &cmd, 0)│                     │
    │                     │    req->rq_flags |= RQF_NVME_PT            │                     │
    │                     │    blk_execute_rq_nowait(req, nds_pt_end_io)──▶│ NVMe 提交
    │                     │                          │ nvme_setup_cmd ←probe─│
    │                     │                          │  (重写 SGL = METABUF)
    │                     │                          │ NVMe DMA→NPU HBM (零拷贝)
    │                     │  wait_for_completion_timeout(batch.cmp, 30s)
    │                     │  ✗超时 → blk_abort_request ×N → grace 1s → abandon
    │                     │  Phase 5: sum bytes from ctxs               │                     │
    │←──done=ssize_t─────│  out_batch: kref_put(owner) → kfree        │                     │
    │ (零拷贝: 数据直达 NPU HBM)                                          │
    │ (降级: nvme_alloc_request 未找到 → -ENOSYS)                        │
    │ npu_file_read (同上, READ 命令)                                     │
    │ verify(D2H) / npu_file_buf_deregister / npu_file_close              │
    │──ioctl(UNMAP)────▶│ hal_kernel_p2p_put_pages─────────────────────▶│ unpin
    │ halMemFree / drvMemDeviceClose / TsdClose                           │
```

#### 8.2.3 free_callback 与在途 IO 的竞态(两条路径共用)

```
路径 A:  ioctl线程 ──BVEC──> 文件系统 ──> 完成 ──> mgroup_put
路径 B:  ioctl线程 ──async submit N──> NVMe ──> end_io ×N ──> kref_put(→0) ──> kfree(batch)
路径 C (回收):  HAL free_callback ──> state=TERMINATED ──> pinned_ref_dropped=true ──> mgroup_put
路径 D (模块):  nds_exit ──> 遍历链表 ──> 强制 TERMINATED ──> mgroup_put ×2
```

A/B 与 C 经 `pinned_ref_dropped` 标志 + kref 串行化:
- 若 C 先到, A/B(UNMAP) 只 put 一次(object ref);
- 若 A/B 先到, C 检测到已设, 不再 put;
- 最终 ref=0 → `nds_mgroup_release` → `hal_kernel_p2p_put_pages` + (路径 A) 释放影子页 + kfree。

#### 8.2.4 并发关键点

- **mgroup 锁**: `mg->lock` 保护 IO 路径与回收路径对 mgroup 状态的读写;
- **全局链表锁**: `nds_mgroup_mutex` 保护 `nds_mgroup_list` 的增删查;
- **路径 A mmap 引用**: `nds_mmap` 在 `vm_insert_page` 前 `kref_get` ×2(lookup + mmap lifetime),
  成功后 drop lookup ref; `nds_vma_close`(munmap) drop mmap lifetime ref;
- **DMA 映射单飞**: `nds_ensure_dma_mapped` 用 `mg->lock` + `dma_mapped` 标志保证同一 mgroup
  只映射一次;
- **路径 B tracepoint 探针**: 模块加载/卸载时一次性 register/unregister, 探针本身无锁;
- **路径 B batch 生命周期**: `nds_pt_batch` 用 `kref` 管理 — owner ref (nds_do_io) + 1 ref per request;
  end_io 和 nds_do_io 各 kref_put, 最后一次 put 释放 batch;
  `abandoned` 标志仅控制是否调用 `complete()` (竞态读无害); 无 `smp_mb` 需求。


### 8.3 开发视图(Development View)

#### 8.3.1 目录结构(路径 A — VFS)

```
docs/zh/nds/VFS/
├── DESIGN_NPU.md          # 设计文档(legacy, 供对照)
├── README.md
├── README_en.md
├── run.sh                 # 驱动/构建/运行/卸载
├── kernel/
│   ├── Makefile           # Kbuild
│   ├── npu_nds_hook.h     # 厂商挂钩: npufs_dma_rw_ops, 6 hook, feature bitmap
│   └── npu_nds_kern.c     # 控制面 + 数据面 + 内存面
├── include/
│   └── ascend_kernel_hal.h  # HAL UAPI
├── npu_nds.h              # ioctl UAPI
├── lib/
│   ├── CMakeLists.txt
│   ├── npu_file.h         # 用户态 API + npu_file_handle_t
│   └── npu_file.c         # open/register/read/write/deregister/close + mmap
└── demo/
    ├── CMakeLists.txt          # 引用共享 demo_main.c
    └── demo_main.c             # 已移至共享目录, 见下方
```

> 两条路径的 demo 共用同一份源码 `demo/demo_main.c`, 通过链接各自
> `libnpu_file.so` 选择零拷贝路径。详见下文"3. 共享 demo 目录"。

#### 8.3.2 目录结构(路径 B — NVMe)

```
docs/zh/nds/NVMe/
├── DESIGN_NPU.md          # 设计文档(NVMe passthrough)
├── README.md
├── README_en.md
├── run.sh
├── kernel/
│   ├── Makefile
│   ├── npu_nds_hook.h     # 符号/tracepoint 定义: RQF_NVME_PT, SGL 常量, 软查找
│   └── npu_nds_kern.c     # 控制面 + 数据面 + 内存面(无影子页/mmap)
├── include/
│   └── ascend_kernel_hal.h
├── npu_nds.h              # ioctl UAPI(+FIEMAP extent)
├── lib/
│   ├── CMakeLists.txt
│   ├── npu_file.h         # 用户态 API(+FIEMAP 查询)
│   └── npu_file.c         # open/register/read/write/deregister/close(+FIEMAP)
└── demo/
    └── CMakeLists.txt          # 引用共享 demo_main.c
```

#### 8.3.3 共享 demo 目录

```
docs/zh/nds/demo/
└── demo_main.c            # 共享 demo: H2D/D2H 验证, 支持 fallocate 预分配
```

两条路径的 `libnpu_file.so` 对外 API 签名完全一致, 差异在 lib 内部屏蔽:
路径 A 的 lib 在 `npu_file_buf_register` 内 `mmap` 影子缓冲, 路径 B 的 lib
在 `npu_file_read/write` 内执行 `FS_IOC_FIEMAP` 查询。demo 无需感知差异,
统一调用带 `npu_offset` 的 6 参数版本 (传 0 即可)。

#### 8.3.4 构建系统

**内核模块** (路径 A/B 相同 Makefile 模式):

```makefile
obj-m := npu_nds.o
npu_nds-objs := npu_nds_kern.o
ccflags-y := -I$(src)/../include -I$(src)/.. -Wall
```

- 路径 A 额外 `__symbol_get("nvme_v1_register_nvfs_dma_ops")` (失败回退 `nvme_v2_register_nvfs_dma_ops`);
- 路径 B 额外 `__symbol_get("nvme_alloc_request")` + `for_each_kernel_tracepoint("nvme_setup_cmd", ...)`。

**用户态库** (CMake):

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

#### 8.3.5 依赖关系

```
npu_nds_demo
  ├── libnpu_file.so   (npu_file.c)
  │     ├── libc        (open, ioctl, mmap, munmap, fallocate, FS_IOC_FIEMAP)
  │     └── npu_nds.h   (UAPI, ioctl 结构)
  ├── libdl             (libascend_hal.so 动态查找)
  ├── librt             (clock_gettime)
  └── libascend_hal.so  (HAL: TsdOpen, drvMemDeviceOpen, halMemAlloc, hal_kernel_p2p_get_pages, ...)
        └── NPU 内核驱动 (导出 P2P 符号)

npu_nds.ko
  ├── npu_nds.h         (UAPI)
  ├── ascend_kernel_hal.h  (HAL 内核态类型)
  ├── [A] __symbol_get("nvme_v1_register_nvfs_dma_ops") (或 v2)  → NVMe 驱动
  ├── [B] __symbol_get("nvme_alloc_request")                → NVMe 驱动
  ├── [B] for_each_kernel_tracepoint("nvme_setup_cmd")     → tracepoint
  ├── hal_kernel_p2p_get_pages / put_pages               → HAL/NPU 驱动
  ├── dma_map_resource / dma_unmap_resource               → DMA API
  └── blk_execute_rq_nowait / blk_mq_rq_to_blk            → 块层(路径 B)
```

### 8.4 物理视图(Physical View)

```
┌─────────────────────────────────────────────────────────────┐
│                    用户态进程 (npu_nds_demo)                │
│  ┌─────────────┐  ┌──────────────┐  ┌────────────────────┐ │
│  │ HAL handle  │  │ npu_file_ctx │  │ FIEMAP extents buf  │ │
│  │ (dev fd,    │  │ (nds_fd,     │  │ (stack/heap)        │ │
│  │  mem handle)│  │  mgroup_id)  │  │                     │ │
│  └──────┬──────┘  └──────┬───────┘  └────────────────────┘ │
│         │ halMemAlloc       │ mmap (路径A) / 无 (路径B)       │
│         ▼                   │                               │
│  ┌──────────────┐           │                               │
│  │ NPU HBM      │◄──────────┘ (影子缓冲映射, 路径A)          │
│  │ (用户态VA)   │             (无映射, 路径B)                │
│  └──────────────┘                                           │
└─────────────────────────────┬───────────────────────────────┘
                              │ syscall (ioctl/mmap)
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                     内核态 (npu_nds.ko)                     │
│  ┌──────────────────────────────────────────────────────┐   │
│  │  nds_mgroup_list  ───[mgroup]───[mgroup]─── ...      │   │
│  │     (mutex)         │ state, kref                    │   │
│  │                     │                                 │   │
│  │  [A] shadow_pages[]──┤  [B] (无影子页)                 │   │
│  │  page_table ────────┤  (p2p_page_table, NPU HBM PA)   │   │
│  │  sgt, dma_dev ──────┤  (P2P DMA 映射缓存)             │   │
│  └─────────────────────┼────────────────────────────────┘   │
│                         │                                     │
│  [A] npufs_dma_rw_ops ──┘  (6 hooks, 注册到 NVMe 驱动)       │
│  [B] tracepoint probe ────┘  (nvme_setup_cmd, SGL 重写)      │
│  [B] nvme_alloc_request ───┘  (软查找)                       │
└─────────────────────────────┬───────────────────────────────┘
                              │
              ┌───────────────┼───────────────┐
              ▼               ▼               ▼
┌─────────────────┐  ┌──────────────┐  ┌──────────────────┐
│ NVMe 控制器     │  │ NPU 设备     │  │ 系统内存        │
│ (bdev, queue)   │  │ (HBM, BAR)   │  │ (影子页, 路径A) │
│                 │  │              │  │                  │
│ DMA R/W ────────┼──┤ HBM (P2P)   │  │ alloc_page       │
│ (SGL 指向 NPU)  │  │              │  │ (4K, page->mapping=NULL)
│                 │  │              │  │                  │
│ [A] hooks 拦截  │  │              │  │ [A] vm_insert_page
│ [B] PT 直接提交 │  │              │  │   → 用户 VA      │
└─────────────────┘  └──────────────┘  └──────────────────┘
```

物理拓扑约束:
- NPU HBM 不在系统内存地址空间, 需经 `dma_map_resource` 映射到 DMA 地址空间;
- NVMe 控制器需支持 P2P (ACS 配置, NTB 距离, 或同 RC);
- 路径 A 影子页占用系统内存 (每 4K NPU HBM 对应 4K 影子页);
- 路径 B 无系统内存开销 (SGL 直接指向 NPU HBM 的 DMA 地址)。

### 8.5 场景视图(Scenario View)

#### 8.5.1 正常写入流程(路径 A)

1. 用户态 `drvMemDeviceOpen` + `halMemAlloc` 分配 NPU HBM;
2. `npu_file_open` 打开 `/dev/npu-nds`;
3. `npu_file_buf_register` → ioctl MAP → `hal_kernel_p2p_get_pages` pin NPU HBM;
4. `alloc_page` 影子页, `page->mapping=NULL`, `page->index` 编入 mgroup_id;
5. `npu_file_buf_register` 内 `mmap` → `vm_insert_page` 影子页到用户 VA;
6. `npu_file_write(fd, shadow_va, offset, size)` → ioctl WRITE;
7. 内核 `nds_shadow_io_iter`: 影子页封装为 BVEC, 调 `file->f_op->write_iter`;
8. VFS DIO 构造 bio, NVMe hooks 拦截 `npu_blk_rq_map_sg` + `npu_dma_map_sg`;
9. hooks 将 SGL 指向 NPU HBM 物理地址 (零拷贝) 或影子页 (bounce);
10. NVMe DMA 直接写入 NPU HBM (零拷贝) 或影子页 (bounce);
11. 用户态 `drvMemcpy(HBM, shadow_va)` 推入 NPU (仅 bounce);
12. `npu_file_buf_deregister` → ioctl UNMAP → `hal_kernel_p2p_put_pages` + 释放影子页;
13. `npu_file_close`, `halMemFree`, `drvMemDeviceClose`, `TsdClose`。

#### 8.5.2 正常写入流程(路径 B)

1. 用户态 `drvMemDeviceOpen` + `halMemAlloc` 分配 NPU HBM;
2. `open(O_DIRECT)` + `fallocate` 预分配文件;
3. `npu_file_open` 打开 `/dev/npu-nds`;
4. `npu_file_buf_register` → ioctl MAP → `hal_kernel_p2p_get_pages` pin NPU HBM;
5. `npu_file_fiemap_query(fd)` → `FS_IOC_FIEMAP` 得到 extents[];
6. `npu_file_write(fd, npu_offset=0, offset, size)` → ioctl WRITE(extents);
7. 内核 `nds_do_io` → `nds_file_to_queue(fd)` 获取 NVMe 请求队列;
8. `batch = kzalloc` + `kref_init(ref=1)`;
9. Phase 1: for each extent: `atomic_inc(pending)` + `kref_get(ref)`, `nds_pt_submit_one` 构造 NVMe 命令, `blk_execute_rq_nowait`;
10. NVMe 驱动 `nvme_setup_cmd` → tracepoint probe 重写 SGL = `NVME_CMD_SGL_METABUF`;
11. NVMe DMA 直接写入 NPU HBM (零拷贝);
12. `nds_pt_end_io` ×N: `atomic_dec(pending)` → 若=0 且 !abandoned → `complete()`, 然后 `kref_put`;
13. Phase 2: `wait_for_completion_timeout(batch.cmp, 30s)` — 超时则 Phase 3 abort + 1s grace + Phase 4 abandon;
14. Phase 5: 累加 completed ctx 字节数; `out_batch: kref_put(owner ref)` → ref=0 → `kfree(batch)`;
15. `npu_file_buf_deregister` → ioctl UNMAP → `hal_kernel_p2p_put_pages`;
16. `npu_file_close`, `halMemFree`, `drvMemDeviceClose`, `TsdClose`。

#### 8.5.3 HAL free_callback 回收

1. NPU 驱动通知 HBM 即将释放, 调 `nds_p2p_free_callback`;
2. `nds_p2p_free_callback` 取 `mgroup` (经 page->index 或 pgmap), 设 `state=TERMINATED`;
3. 若 `pinned_ref_dropped=false`: 设 true, `kref_put(&mg->ref)` 释放 pinned ref;
4. 后续 ioctl 到来时发现 `state != ACTIVE`, 返回 `-EIO`;
5. 最后一个 IO 完成(路径 A/B) 或 UNMAP 到来时, `kref_put` 释放 object ref;
6. ref=0 → `nds_mgroup_release` → `hal_kernel_p2p_put_pages` + (路径 A) 释放影子页 + kfree;
7. 注意: `free_callback` 中**不能**调 `put_pages`, 延迟到 `release`。

#### 8.5.4 模块卸载

1. `nds_exit` 加锁 `nds_mgroup_mutex`, 遍历 `nds_mgroup_list`;
2. 对每个 mgroup: 设 `state=TERMINATED`, 若 `pinned_ref_dropped` 未设则设 + put;
3. kref_put 释放 object ref (若 pinned ref 已 put, 则 ref=0);
4. `nds_mgroup_release` 释放每个 mgroup;
5. [A] `__symbol_put` 释放 NVMe 驱动引用;
6. [B] `tracepoint_probe_unregister` 卸载探针;
7. [B] `__symbol_put` 释放 NVMe 驱动引用;
8. `unregister_chrdev_region`, `class_destroy`, exit 完成。

#### 8.5.5 降级场景

| 场景 | 路径 A | 路径 B |
|---|---|---|
| NVMe 驱动符号未找到 | `npufs_dma_rw_ops` 未注册, READ/WRITE bounce (drvMemcpy) | `nvme_alloc_request` 未找到, READ/WRITE 返回 -ENOSYS |
| tracepoint 未找到 | 不适用 | probe 不注册, SGL 不重写(可能 P2P 失败) |
| NPU P2P 不支持 | `hal_kernel_p2p_get_pages` 失败, MAP 返回 -ENOMEM | 同 |
| 文件系统不支持 FIEMAP | 不适用(走 VFS DIO) | `FS_IOC_FIEMAP` 失败, READ/WRITE 返回 -ENOTTY |
| 非 NVMe 块设备 | VFS DIO 正常工作(hooks 不拦截) | `I_BDEV` 找不到 NVMe queue, 返回 -ENODEV |

#### 8.5.6 可观测性

- `pr_info`: mgroup 创建/释放, IO 提交/完成, free_callback, 模块加载/卸载;
- `dmesg | grep npu_nds`: 查看模块日志;
- `cat /proc/kallsyms | grep nvme_alloc_request`: 验证符号是否存在;
- `cat /sys/kernel/debug/tracing/events/nvme/nvme_setup_cmd/enable`: 验证 tracepoint 是否存在;
- 路径 A: `cat /proc/kallsyms | grep npufs_dma_rw_ops`: 验证 hooks 是否注册成功。


---

## 9. 关键不变量与设计权衡

### 9.1 不变量(Invariants)

| # | 不变量 | 保证机制 | 违反后果 |
|---|---|---|---|
| INV-1 | mgroup ref > 0 期间 NPU HBM 保持 pinned | kref + hal_kernel_p2p_get_pages/put_pages 配对 | HBM 被释放, DMA 飞鸿 |
| INV-2 | 同一 mgroup 的 P2P DMA 映射只做一次 | mg->lock + dma_mapped 标志 | 重复映射, 资源泄漏 |
| INV-3 | free_callback 不调 put_pages | 延迟到 nds_mgroup_release | 递归锁或 use-after-free |
| INV-4 | IO 路径检查 state == ACTIVE | mg->lock 保护读写 | IO 到已回收 HBM |
| INV-5 | 影子页 page->mapping == NULL (路径 A) | alloc_page 后立即置空 | 内核 reclaim 误回收 |
| INV-6 | 影子页 page->index 编入 mgroup_id (路径 A) | 分配时写入 | free_callback 找不到 mgroup |
| INV-7 | RQF_NVME_PT 仅用于 npu_nds 请求 (路径 B) | 构造命令时设置 | 误改其他 NVMe 请求 SGL |
| INV-8 | tracepoint probe 只改 RQF_NVME_PT 请求 (路径 B) | probe 内检查 rq_flags | 影响所有 NVMe IO |
| INV-9 | mgroup 生命周期内 peer_device 不变 | 首次 IO 时缓存 | DMA 地址失效 |
| INV-10 | 模块卸载前所有 mgroup 被释放 | nds_exit 强制 TERMINATED + put | use-after-free |

### 9.2 设计权衡(Design Trade-offs)

#### 9.2.1 影子缓冲 vs NVMe passthrough

| 维度 | 路径 A (影子缓冲) | 路径 B (NVMe PT) | 权衡 |
|---|---|---|---|
| 零拷贝 | 需 hooks 拦截, 否则 bounce | 天然零拷贝 | B 更简洁 |
| 内存开销 | 每 4K HBM 需 4K 影子页 | 无 | B 更优 |
| 驱动修改 | 需 NVMe 驱动导出符号 | 无需修改 | B 更优 |
| 文件布局 | VFS DIO 隐式处理 | 需用户态 FIEMAP | A 更简洁 |
| 文件系统兼容 | 任何支持 DIO 的 FS | 仅 NVMe 块设备 | A 更通用 |
| SGL 控制 | hooks 完全控制 SGL | 需 tracepoint probe 重写 | A 更直接 |
| 稳定性 | hooks 接口可能变化 | tracepoint 接口更稳定 | B 更稳定 |
| 性能 | VFS DIO 路径开销 | 直接 NVMe 提交 | B 更快 |
| 复杂度 | 影子页 + mmap + BVEC | FIEMAP + PT + probe | 相当 |

#### 9.2.2 mgroup 查找: RCU 哈希表 vs 互斥链表

| 维度 | RCU 哈希表 (nvidia-fs) | 互斥链表 (demo) | 权衡 |
|---|---|---|---|
| 查找复杂度 | O(1) | O(n) | 生产用 RCU |
| 并发性 | RCU 读无锁 | 互斥锁 | RCU 更优 |
| 实现复杂度 | 哈希函数 + RCU | list_for_each | demo 更简 |
| 扩展性 | 哈希桶扩容 | 链表线性 | RCU 更优 |

#### 9.2.3 IO 模型: 同步 vs 异步 vs 批量

| 维度 | 同步 (demo) | 异步 (nvidia-fs) | 批量 (nvidia-fs) | 权衡 |
|---|---|---|---|---|
| 实现复杂度 | 最低 | 中 | 高 | demo 验证架构 |
| 延迟 | 每次 syscall 切换 | 多 IO 并行 | 批量减少 syscall | 批量最优 |
| 语义 | 简单清晰 | 需完成回调 | 需批量结构 | demo 最简 |
| 适用场景 | 验证 | 生产 | 高吞吐 | — |

#### 9.2.4 extent 合并策略

| 维度 | 不合并 (demo) | 合并相邻 extent | 块层 bio 合并 (路径 A) | 权衡 |
|---|---|---|---|---|
| 实现复杂度 | 最低 | 中 | VFS 自带 | demo 最简 |
| NVMe 命令数 | 每 extent 一条 | 减少 | 块层优化 | 合并更优 |
| 适用场景 | 验证 | 生产 | 路径 A 天然 | — |

#### 9.2.5 nsid 获取策略

| 维度 | 硬编码 1 (demo) | ioctl 查询 | 文件系统推断 | 权衡 |
|---|---|---|---|---|
| 实现复杂度 | 最低 | 中 | 低 | demo 最简 |
| 正确性 | 仅单 namespace | 正确 | 依赖 FS | 生产需 ioctl |
| 适用场景 | 验证 | 生产 | 路径 A | — |

### 9.3 已知限制(Known Limitations)

1. **单 peer 假设**: DMA 映射缓存只有一个 peer (NVMe 控制器), 多 peer 场景未处理;
2. **无 RDMA 直达**: 未实现 `NPUFS_IOCTL_SET_RDMA_REG_INFO` 等 RDMA API;
3. **无批量 IO**: 未实现 `NPUFS_IOCTL_BATCH_IO`, 每次 IO 独立 syscall;
4. **无异步完成通知**: 路径 B 已实现异步批量提交 + drain, 但无 eventfd/回调式的异步完成通知;
5. **无统计 procfs**: 未实现 `npufs-stat.c` 风格的 procfs 接口;
6. **无调试 debugfs**: 未实现 `npufs-fault.c` 风格的 debugfs 接口;
7. **无拓扑距离矩阵**: `nds_peer_device` 仅用 PCI class 粗匹配, 无 BDF 距离计算;
8. **nsid 硬编码**: 路径 B 的 namespace ID 硬编码为 1;
9. **逻辑块假设 512B**: `NDS_NVME_LB_SIZE = 512`, 未支持 4K 原生设备;
10. **extent 上限 64**: `NDS_MAX_EXTENTS = 64`, 大文件可能不够;
11. **SGL 类型值待确认**: `NVME_CMD_SGL_METABUF = 0x00` 是猜测值, 需厂商确认;
12. **无 extent 合并**: 路径 B 每个 extent 一条 NVMe 命令, 不合并相邻 extent;
13. **单进程假设**: mgroup 链表全局, 多进程并发 IO 未充分测试;
14. **无 ACS 检查**: 未检查 ACS 配置, P2P 可能被 ACS 阻止;
15. **路径 B 仅 NVMe**: 非 NVMe 块设备 (如 virtio-blk) 不支持。

### 9.4 生产化路线(Roadmap to Production)

```
demo (当前)
  │
  ├─ mgroup RCU 哈希表 (替换链表)          ── 性能
  ├─ 异步完成通知 (completion eventfd)     ── 吞吐 (async submit+drain 已实现)
  ├─ 批量 IO (BATCH_IO ioctl)             ── 吞吐
  ├─ extent 合并                            ── NVMe 命令数
  ├─ nsid ioctl 查询                        ── 正确性
  ├─ 4K 原生块支持                          ── 兼容性
  ├─ 多 peer DMA 缓存                       ── 功能
  ├─ PCI 拓扑距离矩阵                       ── 性能
  ├─ ACS 检查                               ── 安全
  ├─ RDMA 直达 (SET_RDMA_REG_INFO)         ── 功能
  ├─ procfs 统计                            ── 可观测性
  ├─ debugfs 调试                           ── 可观测性
  ├─ 路径 A: hooks 接口版本协商            ── 兼容性
  └─ 路径 B: SGL 类型厂商确认              ── 正确性
  │
  ▼
生产驱动
```

---

## 附录 A: 源码索引

### A.1 路径 A (VFS)

| 文件 | 行数(约) | 关键函数/结构 |
|---|---|---|
| `VFS/kernel/npu_nds_kern.c` | ~800 | `nds_do_map`, `nds_do_unmap`, `nds_do_io`, `nds_shadow_io_iter`, `nds_mgroup_get/put/release`, `nds_p2p_free_callback`, `nds_mmap`, `nds_vma_close`, `nds_init`, `nds_exit` |
| `VFS/kernel/npu_nds_hook.h` | ~150 | `struct npufs_dma_rw_ops`, `NPUFS_DMA_RW_OPS_VER`, `NPUFS_FEATURE_*`, hook 注册/注销宏 |
| `VFS/npu_nds.h` | ~85 | `struct npu_nds_map_args`, `struct npu_nds_io_args`, ioctl 编号 |
| `VFS/lib/npu_file.h` | ~70 | `npu_file_handle_t` (含 shadow_va), `npu_file_open/register/read/write/deregister/close` |
| `VFS/lib/npu_file.c` | ~250 | mmap 影子缓冲, ioctl 封装 |
| `docs/zh/nds/demo/demo_main.c` | ~265 | 共享 demo: H2D/D2H 验证流程 (fallocate 预分配) |
| `VFS/DESIGN_NPU.md` | ~830 | 设计文档 (legacy, 供对照) |

### A.2 路径 B (NVMe)

| 文件 | 行数(约) | 关键函数/结构 |
|---|---|---|
| `NVMe/kernel/npu_nds_kern.c` | ~1020 | `nds_do_map`, `nds_do_unmap`, `nds_do_io` (5-phase async drain), `nds_pt_batch_release`, `nds_pt_submit_one`, `nds_pt_end_io`, `nds_file_to_queue`, `nds_nvme_setup_cmd_probe`, `nds_register_tracepoint`/`nds_unregister_tracepoint`, `nds_mgroup_get/put/release`, `nds_p2p_free_callback`, `nds_init`, `nds_exit` |
| `NVMe/kernel/npu_nds_hook.h` | ~107 | `RQF_NVME_PT`, `NVME_CMD_SGL_METABUF`, `NVME_CMD_SGL_DATA_BLOCK`, `nvme_alloc_request_fn_t`, `blk_abort_request_fn_t`, `NDS_NVME_ALLOC_REQUEST_SYM`, `NDS_BLK_ABORT_REQUEST_SYM`, `NDS_NVME_SETUP_CMD_TP_NAME` |
| `NVMe/npu_nds.h` | ~140 | `struct npu_nds_fiemap_extent`, `NDS_MAX_EXTENTS`, `struct npu_nds_io_args` (含 extent_cnt/extents_ptr), 其余同路径 A |
| `NVMe/lib/npu_file.h` | ~80 | `npu_file_handle_t` (无 shadow_va), `npu_file_fiemap_query`, read/write 多 `npu_offset` 参数 |
| `NVMe/lib/npu_file.c` | ~250 | FIEMAP 封装, ioctl 封装 (无 mmap) |
| `docs/zh/nds/demo/demo_main.c` | ~265 | 共享 demo: H2D/D2H 验证流程 (fallocate 预分配) |
| `NVMe/DESIGN_NPU.md` | ~770 | 设计文档 (NVMe passthrough) |

### A.3 共享文件

| 文件 | 说明 |
|---|---|
| `include/ascend_kernel_hal.h` | HAL UAPI: `struct p2p_page_table`, `hal_kernel_p2p_get_pages`/`put_pages` 函数指针原型 |
| `design.md` | 本文档 (合并设计文档) |

---

## 附录 B: ioctl 速查

### B.1 共享 ioctl 编号

```c
#define NDS_IOCTL_MAP      _IOWR('N', 1, struct npu_nds_map_args)
#define NDS_IOCTL_READ     _IOWR('N', 2, struct npu_nds_io_args)
#define NDS_IOCTL_WRITE    _IOWR('N', 3, struct npu_nds_io_args)
#define NDS_IOCTL_UNMAP    _IOW('N',  4, struct npu_nds_unmap_args)
```

### B.2 结构体速查

```c
/* 路径 A/B 共用 */
struct npu_nds_map_args {
    uint64_t npu_va;        /* i: NPU HBM 虚拟地址 */
    uint64_t size;          /* i: 大小 */
    uint32_t flags;         /* i: 保留 */
    uint32_t reserved;
    /* out */
    uint64_t mgroup_id;    /* o: mgroup 句柄 */
    uint64_t mmap_offset;  /* o: [A] 影子缓冲 mmap offset; [B] 0 */
    uint64_t mmap_size;    /* o: [A] 影子缓冲 mmap size; [B] 0 */
    uint32_t npu_page_size;/* o: NPU 物理页大小 */
    uint32_t npu_page_num; /* o: NPU 物理页个数 */
};

struct npu_nds_unmap_args {
    uint64_t mgroup_id;    /* i: mgroup 句柄 */
};

/* 路径 A 的 io_args */
struct npu_nds_io_args {       /* 路径 A */
    uint32_t fd;               /* i: 文件描述符 */
    uint32_t opcode;           /* i: NDS_OPCODE_READ / WRITE */
    uint64_t mgroup_id;        /* i: mgroup 句柄 */
    uint64_t file_offset;      /* i: 文件偏移 */
    uint64_t size;             /* i: IO 大小 */
    /* out */
    uint64_t done;             /* o: 完成字节数 */
    int32_t  ret;              /* o: 返回码 */
    uint32_t reserved;
};

/* 路径 B 的 io_args (扩展) */
struct npu_nds_io_args {       /* 路径 B */
    uint32_t fd;               /* i: 文件描述符 */
    uint32_t opcode;           /* i: NDS_OPCODE_READ / WRITE */
    uint64_t mgroup_id;        /* i: mgroup 句柄 */
    uint64_t file_offset;      /* i: 文件偏移 */
    uint64_t size;             /* i: IO 大小 */
    uint64_t npu_offset;       /* i: NPU HBM 偏移 (路径 B 新增) */
    uint32_t extent_cnt;       /* i: FIEMAP extent 数 (路径 B 新增) */
    uint32_t reserved2;
    uint64_t extents_ptr;      /* i: 指向 npu_nds_fiemap_extent[] (路径 B 新增) */
    /* out */
    uint64_t done;             /* o: 完成字节数 */
    int32_t  ret;              /* o: 返回码 */
    uint32_t reserved3;
};

struct npu_nds_fiemap_extent { /* 路径 B 独有 */
    uint64_t logical;     /* i: 文件逻辑偏移 */
    uint64_t physical;    /* i: 磁盘物理偏移 (→ SLBA) */
    uint64_t length;      /* i: 长度 */
    uint64_t npu_offset;  /* i: NPU HBM 偏移 */
};
```

### B.3 错误码

| 错误码 | 含义 | 触发场景 |
|---|---|---|
| `-ENOMEM` | 内存不足 | pin HBM / alloc_page 失败 |
| `-EINVAL` | 参数无效 | mgroup_id 不存在 / size 不对齐 |
| `-EIO` | IO 错误 | mgroup 状态非 ACTIVE / NVMe 命令失败 |
| `-ENOSYS` | 功能未实现 | 路径 B: `nvme_alloc_request` 未找到 |
| `-ENODEV` | 设备不存在 | 路径 B: 非 NVMe 块设备 |
| `-ENOTTY` | 不支持 ioctl | 路径 B: `FS_IOC_FIEMAP` 不支持 |
| `-EFAULT` | 用户态指针无效 | copy_from_user 失败 |
| `-ETIMEDOUT` | 超时 | 路径 B: `wait_for_completion_timeout` 30s drain 超时 + 1s abort grace 超时, batch 被 abandon |

---

*本文档合并自 `docs/zh/nds/VFS/DESIGN_NPU.md` 与 `docs/zh/nds/NVMe/DESIGN_NPU.md`，
描述 npu_nds 零拷贝驱动框架的完整设计，包含两条独立的零拷贝路径及其共享基础设施。*


## 附录 C: NVIDIA GPUDirect Storage (nvidia-fs) 设计参考

> 本附录为对 NVIDIA GDS `nvidia-fs.ko` v2.29.4 源码的独立分析，作为 npu_nds 设计的对标参考。完整对应关系见正文 §7。

---

> 基于 `NVIDIA/gds-nvidia-fs` 仓库 v2.29.4 源码分析  
> 适用对象: GPUDirect Storage (GDS) 内核侧 `nvidia-fs.ko` 驱动  
> 用户态契约: CUDA cuFile API (`libcufile.so`, `cufile.h`)

---

### 1. 概述

#### 1.1 解决的问题

传统 GPU 与存储的数据通路为:

```
NVMe SSD ──DMA──> CPU bounce/page cache ──memcpy──> GPU BAR ──> GPU 显存
```

该通路存在三重开销: CPU 内存带宽占用、CPU 缓存污染、双份数据拷贝。GPUDirect Storage (GDS) 的目标是消除中间的 CPU 暂存环节, 让 NVMe 控制器(或 RDMA 网卡)直接通过 PCIe P2P 将数据 DMA 到 GPU 显存:

```
NVMe SSD ──DMA(P2P)──> GPU BAR ──> GPU 显存
```

#### 1.2 nvidia-fs 的角色

`nvidia-fs` 是一个 GPL v2 Linux 内核模块, 充当 **GDS 的内核侧使能器**。它本身不实现具体的块设备或文件系统语义, 而是:

1. 通过 NVIDIA P2P API (`nv-p2p.h`) 与 NVIDIA 内核驱动 (`nvidia.ko`) 协作, 完成对 GPU 显存的 **pin(锁定)** 与 **DMA 映射**;
2. 在块设备驱动 (`nvme.ko`, `nvme-rdma` 等) 的 DMA 路径上 **挂钩回调函数**, 当 bio 的目标页是 GPU 页时, 将普通的 CPU 物理地址替换为 GPU BAR 的 P2P DMA 总线地址;
3. 向用户态 `libcufile` 暴露 `/dev/nvidia-fs` 字符设备与一组 ioctl, 由 libcufile 在 cuFile API 内部完成 `cuFileRead`/`cuFileWrite` 等调用。

#### 1.3 设计目标与约束

| 目标/约束 | 说明 |
|---|---|
| 零拷贝直达 | NVMe/RDMA 直接 DMA 到 GPU, 不经 CPU 暂存 |
| 透明兼容 | 既支持 GPU 页(P2P 路径), 又能无缝回退到普通 CPU 页(传统 DIO 路径), 同一请求内可混用 |
| 异步与批量 | 支持 `O_DIRECT` 同步/异步、批量 IO(最多 256 个条目) |
| 安全回收 | 显存被 `cudaFree` 或进程退出时, 在途 IO 必须安全终止并回收映射 |
| 多存储后端 | NVMe 本地、NVMe-oF (RDMA/TCP)、ScaleFlux、Excelero、Lustre、BeeGFS、WekaFS、IBM Spectrum Scale(GPFS)、NFS over RDMA |
| 可移植 | 单一源码跨内核 3.x ~ 6.17+ 编译, 依赖 `configure` 特性探测 |
| 不侵入主线 | 不打补丁修改内核源码, 仅通过各厂商驱动对外导出的"注册/注销"符号挂钩 |

---

### 2. 实现思路(Implementation Approach)

nvidia-fs 的实现思路可以概括为 **"影子页 + 厂商挂钩 + ioctl 契约"** 三位一体。

#### 2.1 影子缓冲区(Shadow Buffer)间接层

**核心矛盾**: 块层与文件系统 O_DIRECT 路径假设 bio 携带的 `struct page` 是 CPU 可访问的常规内存页; 而 GPU 显存通过 BAR 暴露, 并非 `struct page` 体系内可寻址的对象, 无法直接塞进 bio。

**解决思路**: 驱动构造一个"影子缓冲区"作为中介:

- 当用户态 `cuFileBufRegister` 调用 `NVFS_IOCTL_MAP` 时, 驱动调用 `nvidia_p2p_get_pages[_persistent]` 把 GPU 64K 页锁定, 得到 GPU BAR 物理页表;
- 驱动用 `alloc_page(GFP_USER | __GFP_ZERO)` 分配一批 **4K 影子页**(每 GPU 64K 页对应 16 个 4K 影子页), 组成 mgroup 的 `nvfs_ppages[]`;
- 用户态对 `/dev/nvidia-fs` 做 `mmap`, 把这些影子页映射进用户地址空间(即 `cuFileBufRegister` 返回的 `devPtr` 对应的 CPU 虚拟地址);
- 每个影子页的 `page->index` 被编码为 `base_index + 相对偏移`, 其中 `base_index` 是一个随机 32 位值(加 `NVFS_MIN_BASE_INDEX`), 作为该 mgroup 的全局唯一键, 存入 RCU 哈希表 `nvfs_io_mgroup_hash`;
- 关键标记: 影子页的 `page->mapping == NULL`, 驱动据此在块层快速路径中识别"这是 GPU 影子页"而非普通页缓存页。

如此一来, O_DIRECT 的 bio 携带的是合法的 CPU 页(影子页), 文件系统/块层全程可正常推进; 仅在 **DMA 映射**这一步, 驱动挂钩把影子页"翻译"回它对应的 GPU BAR 总线地址。

```
用户态 devPtr(VA) ─mmap─> 影子页(struct page, alloc_page)
                              │ page->index = base_index + off
                              │ page->mapping = NULL  ◄── GPU 页识别标记
                              ▼
                     nvfs_mgroup_from_page() ─RCU─> mgroup
                              │
                              ▼
              mgroup->gpu_info->page_table (nvidia_p2p 页表)
                              │
                              ▼
              nvidia_p2p_dma_map_pages() ──> GPU BAR DMA 地址
```

#### 2.2 厂商驱动挂钩(Vendor Hook)而非内核打补丁

为实现"不修改内核源码", nvidia-fs 选择在各 **存储驱动** 暴露的注册符号上挂钩:

- `nvfs-dma.c` 维护 `modules_list[]`, 每项含 `(模块名, 注册符号名, 注销符号名, 优先级, ops 指针)`;
- 支持的模块键(`nvfs-dma.h`): `nvme` / `nvme_rdma` / `scsi_mod` / `sfxvdriver` / `nvmeib_common` / `lnet` / `beegfs` / `mmfslinux` / `rpcrdma` / `wekafsio` / `scatefs`;
- 模块加载时 `nvfs_blk_register_dma_ops` → `probe_module_list()`: 对每个条目用 `symbol_get(注册符号)` 拿到函数指针并调用之, 把 ops 表交给该驱动; 该驱动将其存入全局变量(如 nvme 的 `nvme_pci_dma_set_ops`), 在自身 DMA 路径上调用;
- 模块卸载时反向 `symbol_put` 并调用注销符号。

ops 回调表(`nvfs_dma_rw_ops` / `nvfs_dma_rw_blk_iter_ops`)是整个 GDS 数据通路的"插桩点":

| 钩子 | 作用 |
|---|---|
| `nvfs_blk_rq_map_sg` | 由块层调用, 用 bio 里的页构造 scatterlist。若全是 GPU 影子页则合并相邻 GPU 物理页并返回段数; 若混入 CPU 页或非法则返回错误; 若全是普通 CPU 页则返回 0, 让厂商驱动走默认路径(透明回退) |
| `nvfs_dma_map_sg_attrs` | 把 sglist 的每段映射成 DMA 总线地址, 内部调用 `nvfs_get_dma` 查 mgroup 的缓存映射 |
| `nvfs_dma_unmap_sg` | 释放 DMA 映射引用 |
| `nvfs_is_gpu_page` / `nvfs_gpu_index` | 判断页是否为 GPU 影子页、取 GPU 页索引 |
| `nvfs_device_priority` | 返回 GPU 与存储设备的 P2P 亲和优先级, 供多路径选择 |
| V2 迭代器 (`*_iter_start/next`, `nvfs_dma_unmap_page`) | 内核 6.17+ 的 `blk_rq_dma_map_iter` 模型, 一次返回一段 |

这样, 块层/NVMe 驱动的主干代码无需改动, 只在"构造 sg"和"map sg"两处多一次"是不是 GPU 页?"的分流判断。

#### 2.3 ioctl 契约(P2P 用户态接口)

用户态 `libcufile` 与内核的唯一交互通道是 `/dev/nvidia-fs` 的 ioctl(`nvfs-core.c: nvfs_ioctl`):

| ioctl | 方向 | 作用 |
|---|---|---|
| `NVFS_IOCTL_MAP` | 用户→内核 | 锁定 GPU 显存、建立影子缓冲与 mgroup, 返回影子 CPU 虚拟地址供 `cuFileBufRegister` 使用 |
| `NVFS_IOCTL_READ` / `NVFS_IOCTL_WRITE` | 双向 | 单次 O_DIRECT 读/写, 同步或异步(`-EIOCBQUEUED`) |
| `NVFS_IOCTL_BATCH_IO` | 双向 | 批量 IO, 最多 `NVFS_MAX_BATCH_ENTRIES=256` 条 |
| `NVFS_IOCTL_SET/GET/CLEAR_RDMA_REG_INFO` | 双向 | 为 mgroup 设置/查询/清除 RDMA 注册信息(rkey/远程地址/QP/gid), 供 RDMA 类文件系统(GPFS)直接对 GPU 显存做 RDMA |
| `NVFS_IOCTL_REMOVE` | — | 预留, 当前为 no-op |

参数经 `copy_from_user` 装入联合 `nvfs_ioctl_param_union`(含 `map_args` / `ioargs` / `io_entries[]` 等变体, `nvfs-core.h`)。异步完成结果通过一个额外的 **end_fence 页**回传: 驱动在完成时把状态/结果/稀疏空洞信息写入该用户页, 用户态轮询。

#### 2.4 配置式可移植(Configure-driven Portability)

`configure` 脚本在构建期探测内核特性, 生成 `config-host.h` 的一组 `HAVE_*` 宏, 例如:

- `HAVE_PIN_USER_PAGES_FAST` / `HAVE_GET_USER_PAGES_FAST_*`
- `HAVE_PROC_OPS_STRUCT`(新内核用 `proc_ops`, 旧内核用 `file_operations`)
- `HAVE_CLASS_CREATE_WITHOUT_MODULE_ARGS`
- `HAVE_CALL_READ_WRITE_ITER` / `HAVE_FILEMAP_RANGE_HAS_PAGE`
- `HAVE_BLK_RQ_DMA_MAP_ITER_START`(6.17+ 迭代器 API)
- `HAVE_KIOCBCOMPLETE_3_ARG` / `HAVE_ACCESS_OK_3_ARG` / `HAVE_VM_FLAGS`

源码以 `#ifdef HAVE_*` 适配不同内核形态, 实现"一套源码跨多版本内核编译"。`nvfs-kernel-interface.c` 进一步把 `access_ok`、`sg 标记` 等小差异封装为内联包装。

#### 2.5 思路小结(数据通路全景)

```
┌─────────────────────────────────────────────────────────────────────┐
│ 用户态:  cuFileRead/Write  ──>  libcufile  ──ioctl──>  /dev/nvidia-fs │
└─────────────────────────────────────────────────────────────────────┘
                                    │ NVFS_IOCTL_READ/WRITE
                                    ▼
┌────────────────────────── 内核: nvidia-fs.ko ─────────────────────────┐
│  nvfs_io_init (校验 fd/inode/generation) → nvfs_io_start_op           │
│   │  切分 shadow_buf_size 块, 置 IO_QUEUED                              │
│   ▼                                                                     │
│  nvfs_direct_io: 建 iovec(影子VA) → f_op->read_iter/write_iter (DIO)  │
│   │  文件系统/块层构造 bio, bio 页 = 影子页(page->mapping=NULL)        │
│   ▼                                                                     │
│  NVMe 驱动: nvfs_blk_rq_map_sg  ──> 识别 GPU 页, 合并, 生成 sglist      │
│   ▼                                                                     │
│  nvfs_dma_map_sg_attrs: nvfs_get_dma(影子页→mgroup→P2P DMA 缓存)        │
│   │  nvidia_p2p_dma_map_pages 给出 GPU BAR 总线地址                     │
│   ▼                                                                     │
│  NVMe 控制器 DMA: 直接 P2P 到 GPU BAR  (零拷贝!)                        │
│   ▼                                                                     │
│  nvfs_io_complete (IRQ/softirq) → 更新统计 → 写 end_fence 页 → 放 mgroup │
└────────────────────────────────────────────────────────────────────────┘
```

---

### 3. 实现原理(Implementation Principles)

#### 3.1 P2P DMA 寻址原理

GPU 显存并非挂在系统 DRAM 总线上, 而是经 GPU 的 PCIe BAR 窗口暴露。要让 NVMe 控制器能 DMA 到它, 必须得到一个 **对 NVMe 控制器可见的总线地址**。NVIDIA P2P API 提供了这一能力:

- `nvidia_p2p_get_pages_persistent(...)` 把指定 GPU 虚拟地址范围对应的显存 **pin 住**(防止被换出/迁移), 返回 `nvidia_p2p_page_table_t`, 内含 GPU BAR 物理页数组(每页 64K);
- `nvidia_p2p_dma_map_pages(page_table, peer_pdev, ...)` 针对某一 PCIe 对端设备(GPU↔NVMe)建立 P2P DMA 映射, 返回 `nvidia_p2p_dma_mapping_t`, 内含该对端可见的总线地址数组。映射与对端相关——同一 GPU 显存对 GPU0、对 NVMe0、对 IB 网卡会得到不同总线地址;
- 因此驱动 **按 (mgroup, peer PCI device) 缓存** DMA 映射, 存入 `gpu_info->buckets` 哈希表(键为对端 PCI devid), RCU 保护, 并用 `dma_mapping_in_progress` 标志 + 等待队列实现"单飞"(同一对端只并发映射一次)。

P2P 路由本身在 PCIe 硬件层完成: 当 GPU 与 NVMe 共享同一 PCIe Switch 时为 switch-local P2P; 否则经根复合体回环。这是 3.6 节拓扑计算的依据。

#### 3.2 影子页编码与 mgroup 寻址原理

- `base_index` = `get_random_u32() + NVFS_MIN_BASE_INDEX`, 作为 mgroup 全局键, 防止用户猜测并探测他人 mgroup;
- 影子页 `page->index = base_index + block_offset_in_mgroup`;
- `nvfs_mgroup_from_page(page)`: 解码 `base_index`, 在 RCU 哈希表中 `hash_for_each_possible_rcu` 查找, 校验 `magic`、`base_index` 与页在 `[0, npages)` 范围内;
- 命中后取引用(`nvfs_mgroup_get`), 块层快速路径全程在 `rcu_read_lock()` 下完成。

这套机制让"任意一个 bio 影子页 → 对应的 mgroup → GPU 物理页 → DMA 映射"在 O(1) + RCU 下完成, 是 GDS 热路径的基石。

#### 3.3 状态机原理(安全回收)

GPU 显存可在任意时刻被 `cudaFree` 释放, 或进程突然退出; 此时可能仍有在途 DMA。驱动用两层原子状态机协调:

**块级(每个 4K block)状态** —— `nvfs_metadata[]`:
```
NVFS_IO_ALLOC ──> IO_FREE ──> IO_INIT ──> IO_QUEUED ──> IO_DMA_START ──> IO_DONE
                                          (失败时) ──> IO_DMA_ERROR
```

**IO 级(整次 IO)状态** —— `gpu_info.io_state`:
```
IO_FREE ──> IO_INIT ──> IO_READY ──> IO_IN_PROGRESS ──> IO_DONE
                                    │              └─> IO_TERMINATED
                                    └─> IO_CALLBACK_IN_PROGRESS
                                       └─> IO_UNPIN_PAGES_ALREADY_INVOKED
```

回收路径三处触发点交汇于此状态机:
1. **NVIDIA P2P free callback**(`nvfs_get_pages_free_callback`): GPU 驱动通知显存即将释放。驱动注销所有 DMA 映射、释放页表、把 end_fence 标为 dead、推进状态到终止分支;
2. **vma_close**(用户 munmap): 影子缓冲被解除映射, 同样进入回收;
3. **同步 IO 完成 / 异步 ki_complete**: 推进到 `IO_DONE`。

只有当"所有在途 IO 引用归零"且"映射已注销"时, 才真正释放 mgroup。引用计数 + `synchronize_rcu` 保证块层快速路径不会拿到已释放的 mgroup。

#### 3.4 RCU + 引用计数原理

mgroup 在全局 RCU 哈希表中, 块层热路径(NVMe 中断/DMA map)在 `rcu_read_lock` 下查表并 `nvfs_mgroup_get` 取引用。回收侧从哈希表删除后, 用 `synchronize_rcu`/`synchronize_rcu_expedited` 等待所有读侧退出, 再释放。这种"无锁读 + 延迟释放"是高性能数据通路与安全回收并存的关键。

#### 3.5 O_DIRECT 协同原理(写分配与稀疏读)

GDS 走 O_DIRECT, 必须避免文件系统把 DIO 回退到 buffered IO(那会经 page cache, 失去零拷贝):

- **写**: `nvfs_io_start_op` 对 WRITE 先 `fallocate`(保证块已分配, 避免触发 DIO→buffered 回退), 再 `flush_dirty_pages` + 失效页缓存, 保证 DIO 直达;
- **稀疏读**: 读到未分配块(空洞)时, NVMe 不会返回数据, 对应 4K block 状态不会推进到 `IO_DMA_START`。驱动据此识别空洞, 在 end_fence 页里记录最多 `NVFS_MAX_HOLE_REGIONS` 个空洞区间, 用户态据此对相应区域做零填充。

#### 3.6 PCIe 拓扑亲和原理

P2P 的性能/可达性取决于 GPU 与存储在 PCIe 树上的相对位置:

- `nvfs-pci.c` 在 init 时枚举所有 GPU(display 3D/VGA)、NVMe、IB、以太网设备;
- 自底向上构造每设备的 BDF 路径(经哪些 Bridge, 深度上限 `MAX_PCI_DEPTH`);
- 计算 `gpu2peer` 距离矩阵, 排序键为 `(MAX_PCIE_BW_INDEX - 带宽) | (pci_dist << 16)`——优先 PCI 物理邻近, 其次带宽;
- 处理 ACS(Access Control Services)重定向: 某些拓扑下 ACS 会阻止 P2P, 需识别;
- NUMA 距离作为兜底;
- 结果经 `nvfs_device_priority` 暴露给多路径存储, 并经 procfs `/proc/driver/nvidia-fs/peer_affinity`、`peer_distance` 可观测。

#### 3.7 RDMA 直达原理

对 GPFS 等 RDMA 文件系统, 不仅能让 NVMe P2P, 还能让 RNIC 直接 RDMA 读写 GPU 显存:

- `NVFS_IOCTL_SET_RDMA_REG_INFO` 把一次 RDMA 注册得到的(rkey、远程虚拟地址、大小、QP 号、GID、LID、DC key)存入 mgroup(`nvfs-rdma.c: nvfs_set_rdma_reg_info_to_mgroup`);
- `nvfs_get_gpu_sglist_rdma_info`(`nvfs-dma.c`) 在 sglist 上迭代, 由页查 mgroup, 校验 `shadow_buf_size`, 组装出 RDMA 描述符, 供文件系统直接对 GPU 显存发起 RDMA READ/WRITE。

#### 3.8 可观测性与故障注入原理

- **procfs** (`nvfs-proc.c`): `/proc/driver/nvidia-fs/{version, modules, bridges, devcount, stats, peer_affinity, peer_distance}`, 其中 `modules` 列出 `modules_list[]` 在 `nvfs_module_mutex` 下各模块挂钩状态;
- **stats** (`nvfs-stat.c`): 原子计数器集合——读/写吞吐与延迟、批量提交延迟、map/unmap、mmap/munmap、稀疏读、各类错误(`nvfs_n_err_*`)、per-GPU(`nvfs_gpu_stat`: max_bar_memory_pinned、active_bar_memory_pinned、active_bounce_buffer_memory 等)。带 `NVFS_STAT_VERSION` 版本号, 构成用户态解析契约;
- **debugfs fault injection** (`nvfs-fault.c`, `CONFIG_FAULT_INJECTION`): `/sys/kernel/debug/nvfs_inject_fault/` 下挂多个 `fault_attr`(`nvfs_dma_error`、`nvfs_rw_verify_area_error`、`nvfs_end_fence_get_user_pages_fast_error`、`nvfs_invalid_p2p_get_page`、`nvfs_io_transit_state_fail`、`nvfs_pin_shadow_pages_error`、`nvfs_vm_insert_page_error`), 通过 `should_fail()` 在关键点可控地注入错误, 用于在开发期演练异常回收路径。

---

### 4. 4+1 视图

#### 4.1 逻辑视图(Logical View)

逻辑视图刻画系统的功能组件及其关系, 不涉及部署与运行时。

```
                       ┌──────────────────────────────┐
                       │   libcufile.so (用户态, 闭源) │
                       │   cuFile API 契约 (cufile.h)  │
                       └───────────────┬──────────────┘
                                       │ ioctl(/dev/nvidia-fs)
                                       ▼
┌──────────────────────────── nvidia-fs.ko ─────────────────────────────┐
│                                                                       │
│  ┌─────────────── 控制面 (nvfs-core.c) ───────────────┐               │
│  │ ioctl 分发  nvfs_ioctl                              │               │
│  │ IO 初始化   nvfs_io_init / nvfs_io_start_op         │               │
│  │ DIO 主循环  nvfs_direct_io                          │               │
│  │ 完成/回收   nvfs_io_complete / nvfs_io_free         │               │
│  │ P2P 映射    nvfs_pin_gpu_pages / nvfs_get_p2p_dma_* │               │
│  │ mgroup 生命周期  nvfs_get_mgroup_from_vaddr/get/put│               │
│  └───────────────────────────┬────────────────────────┘               │
│                              │                                        │
│  ┌────────── 内存面 (nvfs-mmap.c) ─────┴───────────┐                   │
│  │ mgroup  : 影子页 + GPU 页表 + metadata 状态机    │                   │
│  │ 影子缓冲: alloc_page, page->index 编码, mmap      │                   │
│  │ 区间树  : INTERVAL_TREE_GENERIC (页范围索引)      │                   │
│  │ 寻址    : nvfs_mgroup_from_page / from_vaddr      │                   │
│  └───────────────────────────┬────────────────────┘                   │
│                              │                                        │
│  ┌────────── 数据面 (nvfs-dma.c) ──────────────────┐                  │
│  │ modules_list[]: 厂商驱动挂钩表 (优先级+符号名)   │                  │
│  │ nvfs_blk_register_dma_ops / probe_module_list    │                  │
│  │ ops: rq_map_sg / dma_map_sg / is_gpu_page ...    │                  │
│  │ nvfs_get_dma: 影子页→mgroup→P2P DMA 缓存         │                  │
│  │ RDMA 信息提取: nvfs_get_gpu_sglist_rdma_info     │                  │
│  └───────────────────────────┬────────────────────┘                   │
│                              │                                        │
│  ┌────────── 拓扑面 (nvfs-pci.c) ──────────────────┐                  │
│  │ 设备枚举 / BDF 路径 / gpu2peer 距离矩阵          │                  │
│  │ ACS 检查 / 亲和优先级 nvfs_device_priority       │                  │
│  └──────────────────────────────────────────────────┘                  │
│                                                                        │
│  ┌─ 辅助 ───────────────────────────────────────────────────────────┐  │
│  │ nvfs-batch.c      批量 IO (≤256 条目)                            │  │
│  │ nvfs-rdma.c       RDMA 注册信息存取                              │  │
│  │ nvfs-stat.c       原子计数器统计                                 │  │
│  │ nvfs-proc.c       procfs 可观测接口                              │  │
│  │ nvfs-fault.c      debugfs 故障注入                               │  │
│  │ nvfs-kernel-interface.c  内核版本差异封装                        │  │
│  │ nvfs-mod.c        模块入口 / 参数                                │  │
│  └──────────────────────────────────────────────────────────────────┘  │
└────────────────────────────────────────────────────────────────────────┘
        │ nvidia_p2p_* 符号                    │ 挂钩回调注册
        ▼                                       ▼
┌───────────────────┐               ┌────────────────────────────────┐
│ nvidia.ko /        │               │ 块/RDMA 驱动                   │
│ nvidia-uvm.ko /    │  P2P API      │ nvme, nvme_rdma, scsi_mod,     │
│ nvidia-peermem.ko  │ ◄──────────── │ sfxvdriver, nvmeib_common,     │
│ (NVIDIA 私有)      │               │ lnet, beegfs, mmfslinux,       │
└───────────────────┘               │ rpcrdma, wekafsio, scatefs     │
                                     └────────────────────────────────┘
```

关键逻辑数据结构:

| 结构 | 文件 | 职责 |
|---|---|---|
| `nvfs_ioctl_param_union` | nvfs-core.h | ioctl 入参联合(map_args / ioargs / io_entries[]) |
| `nvfs_ioctl_map_t` | nvfs-core.h | MAP 参数: GPU vaddr、size、flags、peer GPU info |
| `nvfs_ioctl_ioargs_t` | nvfs-core.h | 单次 R/W 参数: 文件 fd、offset、size、devPtr、opcode、cookie、end_fence 等 |
| `nvfs_io_mgroup` | nvfs-mmap.h | 影子缓冲 + GPU 页表 + metadata 状态 + RDMA info + DMA buckets |
| `gpu_info` (nvfs_gpu_args) | nvfs-core.h | GPU 显存范围、base_index、io_state、page_table、dma buckets |
| `nvfs_io` | nvfs-core.h | 一次 IO: mgroup、文件、offset、size、状态、end_fence |
| `nvidia_p2p_page_table_t` | nv-p2p.h | GPU BAR 物理页数组(64K/页) |
| `nvidia_p2p_dma_mapping_t` | nv-p2p.h | 对某 peer 的 P2P DMA 总线地址数组 |
| `modules_list[]` / `nvfs_module_entry` | nvfs-dma.h | 厂商驱动挂钩表 |
| `nvfs_dma_rw_ops` / `nvfs_dma_rw_blk_iter_ops` | nvfs-dma.h | 挂钩回调表 |
| `nvfs_gpu_stat` | nvfs-stat.h | per-GPU 统计 |

#### 4.2 进程视图(Process View)

进程视图描述运行时的并发、通信与同步。

##### 4.2.1 同步读/写主流程

```
用户线程                内核(nvidia-fs)              块层/NVMe            GPU/NVIDIA驱动
   │                       │                          │                       │
   │ cuFileRead            │                          │                       │
   │──ioctl(READ)────────▶│ nvfs_ioctl               │                       │
   │                       │ nvfs_io_init             │                       │
   │                       │  fdget, inode 校验,      │                       │
   │                       │  generation 匹配         │                       │
   │                       │  mgroup←from_vaddr      │                       │
   │                       │  state READY→IN_PROGRESS│                       │
   │                       │ nvfs_io_start_op         │                       │
   │                       │  WRITE: fallocate+flush  │                       │
   │                       │  循环切分 shadow_buf_size│                       │
   │                       │──read_iter/write_iter──▶│ 构造 bio(影子页)       │
   │                       │                          │ nvfs_blk_rq_map_sg ◀──┤
   │                       │                          │  识别 GPU 页, 合并 sg   │
   │                       │                          │ nvfs_dma_map_sg ◀─────┤
   │                       │   nvfs_get_dma           │  影子页→mgroup→DMA缓存  │
   │                       │   ◀── nvidia_p2p_dma_map_pages ──────────────────┤ GPU BAR 地址
   │                       │                          │──DMA(P2P)────────────▶│ 直达显存
   │                       │                          │◀─完成中断─────────────┤
   │                       │  nvfs_io_complete        │                       │
   │                       │   更新 stats, 写 end_fence│                       │
   │                       │   nvfs_io_free, put mgroup│                      │
   │◀──ioctl 返回/ssize_t──│                          │                       │
```

##### 4.2.2 异步完成与并发的关键点

- **异步**: 同步 `read_iter` 返回字节数即完成; 若返回 `-EIOCBQUEUED`, 驱动把 `kiocb->ki_complete = nvfs_io_complete`, 用户线程立即从 ioctl 返回, 完成在 **中断/softirq 上下文** 异步发生, 结果写入 end_fence 页供用户轮询;
- **per-cpu ops 计数**(`nvfs_n_ops`): 模块卸载前需等所有在途 IO 归零, 避免卸载时仍有回调进入已卸载模块;
- **DMA 映射单飞**: 同一 (mgroup, peer) 的 `nvidia_p2p_dma_map_pages` 用 `dma_mapping_in_progress` 标志 + waitqueue 保证只并发执行一次, 其余等待者复用结果;
- **回收与在途 IO 的竞态**: P2P free callback / vma_close / IO 完成三路并发, 由状态机 + 引用计数 + `synchronize_rcu` 串行化, 见 3.3。

##### 4.2.3 三条关键并发路径

```
路径A(数据):  ioctl线程 ─DIO─> 块层 ─rq_map_sg/dma_map_sg─> NVMe DMA ─完成中断─> stats/end_fence
路径B(回收):  NVIDIA free_cb 或 vma_close ─> 注销DMA ─> 状态机终止 ─> 释放mgroup
路径C(拓扑):  模块init ─> 枚举PCI ─> 距离矩阵 ─> 暴露给 nvfs_device_priority (读多写极少)
```

A 与 B 经 RCU + 引用计数解耦; C 在 init 期完成, 运行期只读。

#### 4.3 开发视图(Development View)

开发视图关注源码组织、构建与依赖。

##### 4.3.1 源码模块(27 个文件)

```
gds-nvidia-fs/src/
├── nvfs-mod.c                模块入口/参数/许可证 (≈90L)
├── nvfs-core.c               ★ 控制面: ioctl/io_init/io_complete/mgroup生命周期 (2329L)
├── nvfs-core.h               ioctl 参数联合、io 状态、io 接口
├── nvfs-dma.c                ★ 数据面: 厂商挂钩、sg构造/DMA map/RDMA信息 (1529L)
├── nvfs-dma.h                modules_list、ops 表、NVME_MAX_SEGS=127
├── nvfs-mmap.c               ★ 内存面: 影子页/区间树/mgroup寻址 (1258L)
├── nvfs-mmap.h               mgroup、metadata、NVFS_BLOCK_SIZE=4096
├── nvfs-pci.c                拓扑: BDF路径/距离矩阵/ACS/亲和 (783L)
├── nvfs-pci.h
├── nvfs-batch.c              批量IO (162L, NVFS_BATCH_SUPPORT)
├── nvfs-batch.h              NVFS_MAX_BATCH_ENTRIES=256
├── nvfs-rdma.c               RDMA注册信息 (214L, NVFS_ENABLE_KERN_RDMA_SUPPORT)
├── nvfs-rdma.h
├── nvfs-stat.c               原子统计 (651L)
├── nvfs-stat.h               统计计数器、NVFS_STAT_VERSION、NVFS_MAX_GPU=16
├── nvfs-proc.c               procfs (266L)
├── nvfs-fault.c              debugfs 故障注入 (64L, CONFIG_FAULT_INJECTION)
├── nvfs-fault.h
├── nvfs-kernel-interface.c   内核版本差异封装 (65L)
├── nvfs-kernel-interface.h
├── nvfs-vers.h               版本字符串
├── nvfs-p2p.h                NVIDIA P2P API 重声明(便于不依赖 nvidia-uvm 头)
├── config-host.h             configure 生成: HAVE_* 特性宏
├── Makefile                  构建
└── dkms.conf                 DKMS 注册
```

##### 4.3.2 构建与特性探测

```
configure (shell)  ──探测内核──▶  config-host.h  (HAVE_* 宏)
        │
        ▼
   Makefile ──KBUILD──▶  nvidia-fs.ko
        │
        ▼
   dkms.conf ──DKMS──▶  自动随内核升级重编
```

`configure` 探测项示例: `pin_user_pages_fast`、`struct proc_ops`、`class_create` 形参、`call_read_write_iter`、`filemap_range_has_page`、`blk_rq_dma_map_iter`、`ki_complete` 形参、`access_ok` 形参、`vm_flags`。源码用 `#ifdef HAVE_*` 在各差异点分叉, 实现"一套源码跨内核 3.x~6.17+"。

##### 4.3.3 外部依赖(符号)

- **NVIDIA P2P API**: `nvidia_p2p_get_pages` / `nvidia_p2p_get_pages_persistent` / `nvidia_p2p_put_pages_persistent` / `nvidia_p2p_dma_map_pages` / `nvidia_p2p_dma_unmap_pages` / `nvidia_p2p_free_callback`(由 `nvidia.ko`/`nvidia-uvm.ko` 导出)。运行期读 `/sys/module/nvidia/version` 判断 ≥555 用 persistent API;
- **块/RDMA 厂商驱动**: 各自导出的"注册/注销 ops"符号(经 `symbol_get/put`);
- **Linux 内核**: misc device、misc class、bio、blk-mq、scatterlist、interval tree、jhash、procfs、debugfs、fault-inject 框架。

##### 4.3.4 层次分层

```
表现层:   nvfs-mod.c | nvfs-proc.c | nvfs-fault.c | nvfs-core.c(ioctl)
控制层:   nvfs-core.c(io_init/start_op/direct_io/complete) | nvfs-batch.c
数据层:   nvfs-dma.c(挂钩+sg+map) | nvfs-rdma.c | nvfs-pci.c
内存层:   nvfs-mmap.c(影子页/mgroup/区间树)
适配层:   nvfs-kernel-interface.c | config-host.h (HAVE_*)
支撑层:   nvfs-stat.c | nvfs-vers.h | nvfs-p2p.h
```

#### 4.4 物理视图(Physical View)

物理视图描述部署后的硬件/软件拓扑与资源。

##### 4.4.1 硬件拓扑

```
                          CPU + DRAM (NUMA node)
                                │ PCIe Root Complex
                ┌───────────────┴────────────────┐
        ┌───────Switch A────────┐        ┌───────Switch B────────┐
        │   GPU0   GPU1   NVMe0 │        │   GPU2   NVMe1  RNIC │
        │  (BAR)  (BAR)         │        │  (BAR)              │
        └───────────────────────┘        └─────────────────────┘
   P2P 优化区(GPU↔NVMe 同 switch)        跨区 P2P 需经 RC, 性能差
```

nvidia-fs 的 `nvfs-pci.c` 在启动期测绘此树, 计算 `gpu2peer` 距离, 为多路径存储选最近 GPU 的 NVMe。

##### 4.4.2 软件部署与节点

| 节点 | 内容 |
|---|---|
| GPU 内存 | 被锁定的 64K BAR 物理页(`nvidia_p2p_page_table_t`) |
| CPU 内存(影子) | 每 mgroup 一批 4K `alloc_page` 影子页, `NVFS_BLOCK_SIZE=4096`, 经 mmap 给用户 |
| CPU 内存(回退) | 非 GPU 页时使用普通 bounce/page cache(传统 DIO 路径) |
| 字符设备 | `/dev/nvidia-fs0..N`(`MAX_NVFS_DEVICES`, 默认按可用 GPU 数) |
| procfs | `/proc/driver/nvidia-fs/{version,modules,bridges,devcount,stats,peer_affinity,peer_distance}` |
| debugfs | `/sys/kernel/debug/nvfs_inject_fault/*` |
| sysfs 读取 | `/sys/module/nvidia/version`(判 persistent API) |
| 内核模块 | `nvidia-fs.ko`(本驱动) + `nvidia.ko`/`nvidia-uvm.ko`(P2P API) + 各存储驱动 |

#### 4.5 场景视图(Scenarios / Use Cases)

##### 场景 1: 同步 cuFileRead 从 NVMe 读到 GPU

1. 用户 `cuFileBufRegister(devPtr)`: libcufile 发 `NVFS_IOCTL_MAP`, 驱动 pin GPU 显存、建影子缓冲、返回影子 CPU vaddr;
2. 用户 `cuFileRead(devPtr, fh, offset, size)`: libcufile 发 `NVFS_IOCTL_READ`, 驱动 `nvfs_io_init` 校验文件 fd/inode/generation, 取 mgroup, 转 IN_PROGRESS;
3. `nvfs_io_start_op` 切分块、置 IO_QUEUED, `nvfs_direct_io` 调 `f_op->read_iter`;
4. 块层用影子页构造 bio; NVMe 驱动调 `nvfs_blk_rq_map_sg` 合并 GPU 页、`nvfs_dma_map_sg_attrs` 取 P2P DMA 地址;
5. NVMe 控制器 DMA 直达 GPU BAR(零拷贝);
6. 完成回调 `nvfs_io_complete` 更新 stats, `nvfs_io_free` 放 mgroup, ioctl 返回 ssize_t。

##### 场景 2: 异步 cuFileWrite(轮询 end_fence)

1. 同 1 注册;
2. `NVFS_IOCTL_WRITE`, 驱动先 `fallocate`+`flush_dirty_pages`+失效页缓存(防 DIO 回退);
3. `read_iter/write_iter` 返回 `-EIOCBQUEUED`, 用户线程立即返回, `ki_complete=nvfs_io_complete`;
4. 中断上下文完成 → 写 end_fence 页(状态/结果/偏移);
5. 用户轮询 end_fence 得知完成。

##### 场景 3: 批量 IO(cuFileBatch)

1. `NVFS_IOCTL_BATCH_IO`: `nvfs_io_batch_init` 对每个 entry `copy_from_user` + `nvfs_io_init`;
2. `nvfs_io_batch_submit` 逐个 `nvfs_io_start_op`, 记 `start_io`(ktime) 测批量提交延迟;
3. 各 IO 独立完成, 状态写回对应 end_fence 槽。

##### 场景 4: RDMA 文件系统(GPFS)直达 GPU

1. GPFS 用户态注册 GPU 缓冲, 经 `NVFS_IOCTL_SET_RDMA_REG_INFO` 把一次 RDMA 注册的 rkey/远地址/QP/gid 存入 mgroup;
2. GPFS 内核驱动调 `nvfs_get_gpu_sglist_rdma_info` 取得 GPU 显存的 RDMA 描述符;
3. RNIC 直接对 GPU BAR 做 RDMA READ/WRITE, 绕过 CPU bounce。

##### 场景 5: 在途 IO 时 cudaFree

1. 用户对已注册显存调用 `cudaFree`;
2. NVIDIA 驱动触发 `nvfs_get_pages_free_callback`;
3. 驱动注销所有 DMA 映射、释放页表、end_fence 标 dead、状态机进终止分支;
4. 在途 IO 完成后引用归零, `synchronize_rcu` 后真正释放 mgroup;
5. 后续若用户再读 devPtr, end_fence 标 dead, 用户态得到错误并重新注册。

##### 场景 6: 混合 CPU/GPU 页的同一请求(透明回退)

1. bio 同时含 CPU 页与 GPU 影子页: `nvfs_blk_rq_map_sg` 检测混用 → 计 `nvfs_n_err_mix_cpu_gpu` 并返回错误;
2. 全 CPU 页: `nvfs_blk_rq_map_sg` 返回 0, 厂商驱动走默认 DMA 路径(传统 DIO);
3. 全 GPU 页: 走 P2P 路径。

---

### 5. 关键不变量与设计权衡

| 不变量 | 保证手段 |
|---|---|
| 影子页必能映射回唯一 mgroup | `page->index` = `base_index+off`, 哈希查找 + magic/范围校验 |
| P2P DMA 地址对某 peer 稳定 | 按 (mgroup, peer devid) 缓存 + 单飞 + RCU 读 |
| 在途 IO 期间显存不被释放 | pin(P2P API) + 引用计数 + 状态机 |
| 不破坏普通 IO(非 GPU 页) | `is_gpu_page` 判别, 全 CPU 时返回 0 让驱动走默认路径 |
| 跨内核可编译 | `configure`→`config-host.h` 的 `HAVE_*` 适配 |

**主要权衡**:

- **影子页 4K vs GPU 页 64K**: 影子页用 4K 以贴合块层 PAGE_SIZE; 每 GPU 64K 页拆 16 个影子页, 增加页表项但换得与块层原生兼容;
- **DMA 映射缓存 vs 实时映射**: 缓存以减少 `nvidia_p2p_dma_map_pages` 开销(该调用昂贵), 代价是回收时需遍历 buckets 注销;
- **RCU 读 vs 写侧延迟**: 读侧无锁快, 但回收侧 `synchronize_rcu` 可能慢; 用 `_expedited` 加速关键路径;
- **厂商挂钩 vs 内核打补丁**: 挂钩保持驱动独立、可随内核升级, 但依赖各厂商驱动持续导出注册符号, 受限于它们的演进。

---

### 6. 可观测性与运维接口速查

| 接口 | 路径 | 内容 |
|---|---|---|
| 版本 | `/proc/driver/nvidia-fs/version` | 驱动版本、NVIDIA 驱动版本、P2P 模式(persistent/legacy) |
| 模块 | `/proc/driver/nvidia-fs/modules` | 各厂商驱动挂钩状态 |
| 统计 | `/proc/driver/nvidia-fs/stats` | 读/写吞吐与延迟、批量、map、稀疏读、错误等(`NVFS_STAT_VERSION` 版本化) |
| 亲和 | `/proc/driver/nvidia-fs/peer_affinity` | GPU↔peer 优先级 |
| 距离 | `/proc/driver/nvidia-fs/peer_distance` | GPU↔peer 距离矩阵 |
| 故障注入 | `/sys/kernel/debug/nvfs_inject_fault/*` | 各关键点 `fault_attr` |
| 模块参数 | `modprobe nvidia-fs ...` | `max_devices`、`dbg_enabled`、`info_enabled`、`peer_stats_enabled`、`rw_stats_enabled`、`use_legacy_p2p_allocation` |

---

### 附录 A: 源码索引(关键函数定位)

| 功能 | 函数 | 文件:行(概) |
|---|---|---|
| ioctl 分发 | `nvfs_ioctl` | nvfs-core.c |
| IO 初始化校验 | `nvfs_io_init` | nvfs-core.c |
| IO 启动主循环 | `nvfs_io_start_op` | nvfs-core.c |
| DIO 发起 | `nvfs_direct_io` | nvfs-core.c |
| 异步完成 | `nvfs_io_complete` | nvfs-core.c |
| 释放回收 | `nvfs_io_free` | nvfs-core.c |
| GPU 页 pin | `nvfs_pin_gpu_pages` | nvfs-core.c |
| P2P DMA 映射 | `nvfs_get_p2p_dma_mapping` | nvfs-core.c |
| DMA 快路径 | `nvfs_get_dma` | nvfs-core.c |
| P2P 释放回调 | `nvfs_get_pages_free_callback` | nvfs-core.c |
| mgroup 寻址(页) | `nvfs_mgroup_from_page` | nvfs-mmap.c |
| mgroup 寻址(vaddr) | `nvfs_get_mgroup_from_vaddr` | nvfs-mmap.c |
| 厂商挂钩注册 | `nvfs_blk_register_dma_ops`/`probe_module_list` | nvfs-dma.c |
| sg 构造 | `nvfs_blk_rq_map_sg` | nvfs-dma.c |
| DMA map sg | `nvfs_dma_map_sg`/`nvfs_dma_map_sg_attrs` | nvfs-dma.c |
| RDMA 信息提取 | `nvfs_get_gpu_sglist_rdma_info` | nvfs-dma.c |
| PCI 拓扑 | `nvfs_bdf2pdevinfo` | nvfs-pci.c |
| 亲和优先级 | `nvfs_device_priority` | nvfs-pci.c |
| 批量 IO | `nvfs_io_batch_init`/`submit` | nvfs-batch.c |
| RDMA 注册 | `nvfs_set_rdma_reg_info_to_mgroup` | nvfs-rdma.c |
| 统计 | 计数器定义/proc show | nvfs-stat.c |
| procfs | `nvfs_modules_show` 等 | nvfs-proc.c |
| 故障注入 | `nvfs_init_debugfs` | nvfs-fault.c |
| 模块入口 | `nvfs_init`/`nvfs_exit` | nvfs-mod.c / nvfs-core.c |

---

*文档基于 `gds-nvidia-fs` v2.29.4 全部源码分析生成。函数行号为概略, 精确定位以源码为准。*
