# adp_mlx - mlx5 driver adapter loaded by ibv_extend at runtime

## 概述

`adp_mlx` 是面向 Mellanox ConnectX (CX) 系列网卡的驱动适配层组件，作为 `ibv_extend` 扩展库的运行时依赖存在。

```
┌─────────────────────────────────┐
│         应用层 (Application)      │
└───────────────┬─────────────────┘
                │
┌───────────────▼─────────────────┐
│     ibv_extend 扩展库 (.so)       │
└───────────────┬─────────────────┘
                │
┌───────────────▼─────────────────┐
│     adp_mlx 驱动适配层 (.so)      │
└───────────────┬─────────────────┘
                │
┌───────────────▼─────────────────┐
│       mlx5 标准驱动               │
└─────────────────────────────────┘
```

## 目录结构

```
src/custom/nda/
├── ibv_extend/              # ibv_extend 扩展库（提供 ibv_extend.h 头文件）
└── adp_mlx/                 # 本组件
    ├── adp_mlx.c            # 驱动实现源文件
    ├── CMakeLists.txt       # CMake 构建配置
    ├── build.sh             # 编译脚本
    ├── README.md            # 本文档
    └── README_en.md         # 英文文档
```

## 依赖

| 依赖项 | 用途 |
|--------|------|
| libibverbs (rdma-core) | 标准 verbs 头文件和接口 |
| libmlx5 | mlx5 vendor direct verbs 接口 |
| ibv_extend.h | 扩展接口定义（同级目录 `ibv_extend/`） |

## 编译方法

### 前置条件

- cmake、gcc、make
- rdma-core（含 libibverbs 和 libmlx5）

### 使用 build.sh

```bash
# 使用系统已安装的 libibverbs / libmlx5
bash build.sh

# 指定 rdma-core 构建目录（含 include/ 和 lib/ 子目录）
bash build.sh -b=/path/to/rdma-core/build

# 指定 ibv_extend 构建目录（链接 libibv_extend.so）
bash build.sh -b=/path/to/rdma-core/build -e=/path/to/ibv_extend/build
```

### 编译产物

```
output/lib/libadp_mlx.so
```

## 使用方法

### 安装驱动库

将编译产物复制到 `ibv_extend` 的驱动搜索路径（通常为 `/usr/local/lib64` 或 `/usr/lib64`）：

```bash
cp output/lib/libadp_mlx.so /usr/local/lib64/
ldconfig
```

### 配置 ibv_extend 驱动加载列表

**方式一：环境变量**

```bash
export IBV_EXTEND_DRIVERS=adp_mlx
```

**方式二：配置文件**

```bash
# 创建 /etc/libibverbs_extend.d/adp_mlx.conf，内容：
driver adp_mlx
```
