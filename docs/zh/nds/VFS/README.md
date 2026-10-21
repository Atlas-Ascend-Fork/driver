# NPU Direct Storage (NDS) demo

## 描述

本样例是 NVIDIA GPUDirect Storage (GDS) `nvidia-fs.ko` 在昇腾 NPU 侧的对应实现 demo,
采用 **VFS 影子缓冲 + 厂商挂钩** 零拷贝路径(路径 A),同时包含**内核态**和**用户态**两部分:

- **内核态模块 `npu_nds.ko`**:创建字符设备 `/dev/npu-nds`,通过 ioctl 契约对外提供:
  - `NDS_IOCTL_MAP` —— 锁定 NPU 显存(`hal_kernel_p2p_get_pages`,对应 NVIDIA 的 `nvidia_p2p_get_pages`)、分配 4K 影子页、建立 mgroup。
  - `NDS_IOCTL_READ` / `NDS_IOCTL_WRITE` —— 以 O_DIRECT 在文件与影子页之间搬数据(对应 `NPUFS_IOCTL_READ/WRITE`)。
  - `NDS_IOCTL_UNMAP` —— 释放影子页、调用 `hal_kernel_p2p_put_pages`(对应 `NPUFS_IOCTL_REMOVE`)。
- **用户态客户端库 `libnpu_file.so`**:对应 `libcufile`,仅依赖 libc 与 NDS kernel ioctl 契约(不依赖 HAL/TSD 等 API),封装 `/dev/npu-nds` 的 ioctl 与影子缓冲 mmap,提供 `npu_file_open` / `npu_file_buf_register` / `npu_file_read` / `npu_file_write` / `npu_file_buf_deregister` / `npu_file_close` API。
- **用户态 demo `npu_nds_demo`**:链接 `libnpu_file.so` + HAL 用户态 API(`drvMemDeviceOpen`/`halMemAlloc`/`drvMemsetD8`/`drvMemcpy`),对标 cufile-samples/cufile_rw.cc,完成"打开设备→分配→填充→写→清零→读→校验→注销→关闭设备"全流程演示。命令行参数 `path` + `size`(对齐到 4096)。

> demo 源码位于 `docs/zh/nds/demo/demo_main.c`,与 NVMe 路径共享同一份源码,通过链接不同的 `libnpu_file.so` 选择零拷贝路径。

### 零拷贝原理(VFS 影子缓冲 + vendor hooks)

模块加载时通过 `__symbol_get` 查找 NVMe 驱动导出的
`nvme_v1_register_nvfs_dma_ops` 符号(v1 失败时回退 v2),
注册 `struct npufs_dma_rw_ops` ops 表(6 个 hook 函数)。
NVMe 驱动构建 DMA sglist 时调用 `npu_blk_rq_map_sg` / `npu_dma_map_sg_attrs`,
将影子页 CPU 物理地址替换为 NPU HBM P2P DMA 总线地址, NVMe 控制器直接 DMA 到 NPU HBM。
若 NVMe 驱动未打补丁(`__symbol_get` 返回 NULL), 模块优雅降级为 bounce-buffer 模式。

### 设计映射(nvidia-fs → npu_nds)

| nvidia-fs 设计要点(见 `design.md` 附录 C) | npu_nds 对应 |
|---|---|
| `nvidia_p2p_get_pages_persistent` pin GPU 显存 | `hal_kernel_p2p_get_pages` pin NPU HBM |
| `nvidia_p2p_put_pages` | `hal_kernel_p2p_put_pages` |
| `nvidia_p2p_page_table_t` | `struct p2p_page_table`(含 `pages_info[].pa`) |
| 4K 影子页 `alloc_page`、`page->mapping=NULL` 标记 | 同样分配 4K 影子页、`page->mapping=NULL`、`page->index=(mgid<<12)+off` |
| `npufs_io_mgroup` 哈希表 + 基址随机键 | `struct nds_mgroup` 链表 + `atomic64_inc_return` 键 |
| `npufs_get_pages_free_callback` 回收 | `nds_p2p_free_callback` 状态机终止分支 |
| `NPUFS_IOCTL_MAP/READ/WRITE/REMOVE` | `NDS_IOCTL_MAP/READ/WRITE/UNMAP` |
| 厂商驱动挂钩(`npufs_dma_rw_ops`)零拷贝直达 | **已实现**: `__symbol_get` 注册 `npu_dma_rw_ops`(6 个 hook 函数) |

## 目录结构

```
VFS/
├── npu_nds.h               # 内核与用户共享的 ioctl 契约
├── CMakeLists.txt          # 顶层 CMake,构建 lib + demo
├── kernel/
│   ├── npu_nds_hook.h      # vendor hook 定义: npufs_dma_rw_ops 结构体, 特性位, v1/v2 符号
│   ├── npu_nds_kern.c      # 内核态模块:影子缓冲 + P2P pin + ioctl + vendor hooks + 状态机
│   └── Makefile            # Kbuild Makefile
├── lib/
│   ├── npu_file.h          # 客户端库公共 API(对应 cuFile API)
│   ├── npu_file.c           # 客户端库实现(mmap 影子缓冲 + ioctl,不依赖 HAL)
│   └── CMakeLists.txt      # 构建 libnpu_file.so(无外部链接依赖)
├── demo/
│   └── CMakeLists.txt      # 构建 npu_nds_demo(引用共享 ../../demo/demo_main.c)
├── run.sh                  # 编译加载内核模块 + 编译运行用户态 demo
└── README.md
```

> 共享 demo 源码 `demo_main.c` 位于 `docs/zh/nds/demo/`,两条路径共用。

## 支持的产品型号

- Atlas A2 训练系列产品/Atlas A2 推理系列产品
- Atlas A3 训练系列产品/Atlas A3 推理系列产品
- 昇腾950PR处理器/昇腾950DT处理器

> `hal_kernel_p2p_get_pages` 的支持矩阵以其头文件 `ascend_kernel_hal.h` 中声明为准;
> 计算能力分组(ascend310P/ascend910)场景不保证支持。

## 环境准备

- 已安装 NPU 驱动固件与 CANN 软件包,参见《CANN软件安装指南》"安装NPU驱动固件"章节。
- 运行主机内核版本需与驱动编译所用内核树一致,否则 `npu_nds.ko` 无法 `insmod`。
- 零拷贝 P2P 需 NVMe 驱动已打 npufs hook 补丁(导出 `nvme_v1_register_nvfs_dma_ops` 或 `nvme_v2_register_nvfs_dma_ops` 符号)。
- 需 root 权限执行 `insmod`/`rmmod`。

## 编译运行

```bash
cd ${git_clone_path}/docs/zh/nds/VFS
sudo bash run.sh
```

`run.sh` 依次:
1. 读取 `/etc/ascend_install.info`,source `setenv.bash` 拿到 `DRIVER_HAL_INC_DIR` 等内核头路径;
2. `make -C /lib/modules/$(uname -r)/build M=kernel modules` 编出 `npu_nds.ko`;
3. `insmod npu_nds.ko` 生成 `/dev/npu-nds`(退出时 `rmmod`);
4. `cmake` 编出 `libnpu_file.so` + `npu_nds_demo`;
5. 运行 `npu_nds_demo`,完成 "打开设备→注册→读→校验→写→校验→注销→关闭设备" 全流程,结果写入 `output_msg.txt`。

## CANN Driver API

内核态:
- `hal_kernel_p2p_get_pages` —— 锁定 NPU 虚拟地址对应的物理页,返回 `p2p_page_table`(对应 `nvidia_p2p_get_pages`)。
- `hal_kernel_p2p_put_pages` —— 与上者配对释放。
- `struct p2p_page_table` / `struct p2p_page_info` —— NPU 物理页描述,`pages_info[].pa` 即 P2P 总线地址来源。

用户态(demo 使用,不属于客户端库):
- `halMemAlloc` —— 申请 NPU HBM(`MEM_DEV | devid | MEM_TYPE_HBM | MEM_PAGE_HUGE`)。
- `halMemFree` —— 释放。
- `drvMemsetD8` —— 填充(0xAB pattern) + 清零 NPU HBM(≈ `cudaMemset`);**不在零拷贝数据路径中**。
- `drvMemcpy` —— 仅用于 D2H 校验(≈ `cudaMemcpy` D2H);**不在零拷贝数据路径中**。
- `drvMemDeviceOpen` / `drvMemDeviceClose` —— 设备上下文打开/关闭。

客户端库 API(`libnpu_file.so`,对应 `libcufile`,仅依赖 libc + NDS ioctl):

| cuFile API | npu_file API | 说明 |
|---|---|---|
| `cuFileDriverOpen` | `npu_file_open` | 打开 `/dev/npu-nds`(不管理 NPU 设备上下文) |
| `cuFileBufRegister` | `npu_file_buf_register` | ioctl MAP 锁定 NPU 显存 + 分配影子缓冲 + mmap |
| `cuFileRead` | `npu_file_read` | 文件 O_DIRECT → 影子缓冲 |
| `cuFileWrite` | `npu_file_write` | 影子缓冲 → 文件 O_DIRECT |
| `cuFileBufDeregister` | `npu_file_buf_deregister` | munmap + 注销 mgroup |
| `cuFileDriverClose` | `npu_file_close` | 关闭 `/dev/npu-nds` |

> `npu_file_read/write` 统一为 6 参数版本(含 `npu_offset`),VFS 路径忽略该参数
> (偏移由影子缓冲 mmap 决定)。NPU 设备上下文管理(`drvMemDeviceOpen`/`TsdOpen`)、
> 显存分配(`halMemAlloc`)、memset(`drvMemsetD8`)、数据搬运(`drvMemcpy`)由调用方(demo)
> 自行完成,不属于客户端库职责。

## 已知 issue / 限制

- 零拷贝 P2P 需 NVMe 驱动已打 npufs hook 补丁(导出 `nvme_v1_register_nvfs_dma_ops` 或
  `nvme_v2_register_nvfs_dma_ops` 符号);未打补丁时模块自动降级为 bounce-buffer 模式(`drvMemcpy` 搬运)。
- `nds_peer_device` 用 PCI storage class 粗匹配 NVMe 控制器,生产驱动应维护距离矩阵。
- `nds_mgroup` 用互斥链表代替 RCU 哈希表,仅为 demo 可读性;热路径回收请参考 `npufs-mmap.c` 的 RCU + 引用计数实现。
- 暂未实现异步 IO 与批量 IO(`NPUFS_IOCTL_BATCH_IO`),同步路径仅作演示。
